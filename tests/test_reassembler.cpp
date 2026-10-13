#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "log_aggregator/chunker.hpp"
#include "log_aggregator/reassembler.hpp"

namespace {

using log_aggregator::LineReassembler;
using log_aggregator::kLineBufferSize;

struct Sink {
    std::vector<std::string> lines;
};

void on_line (void* ctx, const char* line, uint32_t length) {
    static_cast<Sink*>(ctx)->lines.emplace_back(line, length);
}

// bytes8 must point at 8 readable bytes, padding with \0 included.
void feed_chunk (LineReassembler& r, Sink& s, const char* bytes8) {
    uint8_t c[8];
    memcpy(c, bytes8, 8);
    r.feed(c, &s, on_line);
}

struct Pair {
    LineReassembler* r;
    Sink* s;
};

// Chunks text with the real chunker and feeds every chunk to r.
void feed_text (LineReassembler& r, Sink& s, const std::string& text) {
    Pair p{&r, &s};
    log_aggregator::chunk_and_send(
        reinterpret_cast<const uint8_t*>(text.data()), static_cast<uint32_t>(text.size()), &p,
        [](void* ctx, const uint8_t chunk[8]) {
            auto* pp = static_cast<Pair*>(ctx);
            pp->r->feed(chunk, pp->s, on_line);
        });
}

std::string alphabet_text (size_t n) {
    std::string t(n, '\0');
    for (size_t i = 0; i < n; i++) t[i] = static_cast<char>('a' + i % 26);
    return t;
}

}

TEST(Reassembler, OneLineInOneChunk) {
    LineReassembler r; Sink s;
    feed_chunk(r, s, "hello!\n\0");
    ASSERT_EQ(s.lines.size(), 1u);
    EXPECT_EQ(s.lines[0], "hello!");
}

TEST(Reassembler, LineWithoutNewlineIsHeldBack) {
    LineReassembler r; Sink s;
    feed_chunk(r, s, "abc\0\0\0\0\0");
    EXPECT_TRUE(s.lines.empty());
}

TEST(Reassembler, LineSpanningChunks) {
    LineReassembler r; Sink s;
    feed_chunk(r, s, "ABCDEFGH");
    feed_chunk(r, s, "IJKLMNOP");
    EXPECT_TRUE(s.lines.empty());
    feed_chunk(r, s, "Q\n\0\0\0\0\0\0");
    ASSERT_EQ(s.lines.size(), 1u);
    EXPECT_EQ(s.lines[0], "ABCDEFGHIJKLMNOPQ");
}

TEST(Reassembler, TwoLinesInOneChunk) {
    LineReassembler r; Sink s;
    feed_chunk(r, s, "ab\ncd\ne\0");
    ASSERT_EQ(s.lines.size(), 2u);
    EXPECT_EQ(s.lines[0], "ab");
    EXPECT_EQ(s.lines[1], "cd");
    // "e" is still pending and joins the next chunk.
    feed_chunk(r, s, "f\n\0\0\0\0\0\0");
    ASSERT_EQ(s.lines.size(), 3u);
    EXPECT_EQ(s.lines[2], "ef");
}

TEST(Reassembler, AllPaddingChunkChangesNothing) {
    LineReassembler r; Sink s;
    feed_chunk(r, s, "\0\0\0\0\0\0\0\0");
    EXPECT_TRUE(s.lines.empty());
    feed_chunk(r, s, "ab\0\0\0\0\0\0");
    feed_chunk(r, s, "\0\0\0\0\0\0\0\0");
    feed_chunk(r, s, "c\n\0\0\0\0\0\0");
    ASSERT_EQ(s.lines.size(), 1u);
    EXPECT_EQ(s.lines[0], "abc");
}

// Documented: appending stops at the first \0, later bytes in that chunk are ignored.
TEST(Reassembler, ZeroEndsTheChunkEvenWithBytesAfterIt) {
    LineReassembler r; Sink s;
    feed_chunk(r, s, "ab\0cd\n\0\0");
    EXPECT_TRUE(s.lines.empty());
    feed_chunk(r, s, "\n\0\0\0\0\0\0\0");
    ASSERT_EQ(s.lines.size(), 1u);
    EXPECT_EQ(s.lines[0], "ab");
}

TEST(Reassembler, EmptyLineIsReportedWithLengthZero) {
    LineReassembler r; Sink s;
    feed_chunk(r, s, "\n\0\0\0\0\0\0\0");
    ASSERT_EQ(s.lines.size(), 1u);
    EXPECT_EQ(s.lines[0], "");
}

TEST(Reassembler, Line128BytesIsNotTruncated) {
    LineReassembler r; Sink s;
    std::string line = alphabet_text(kLineBufferSize);
    feed_text(r, s, line + "\n");
    ASSERT_EQ(s.lines.size(), 1u);
    EXPECT_EQ(s.lines[0], line);
}

// Current behaviour: the 129th byte with no \n seen force-flushes the first
// 128 bytes, and the overflow byte starts the next line.
TEST(Reassembler, Line129BytesSplitsAs128Plus1) {
    LineReassembler r; Sink s;
    std::string line = alphabet_text(129);
    feed_text(r, s, line + "\n");
    ASSERT_EQ(s.lines.size(), 2u);
    EXPECT_EQ(s.lines[0], line.substr(0, 128));
    EXPECT_EQ(s.lines[1], line.substr(128));
}

TEST(Reassembler, Line300BytesSplitsAs128Plus128Plus44) {
    LineReassembler r; Sink s;
    std::string line = alphabet_text(300);
    feed_text(r, s, line + "\n");
    ASSERT_EQ(s.lines.size(), 3u);
    EXPECT_EQ(s.lines[0], line.substr(0, 128));
    EXPECT_EQ(s.lines[1], line.substr(128, 128));
    EXPECT_EQ(s.lines[2], line.substr(256));
}

// Current behaviour: the forced flush happens when byte 129 arrives, not
// when the buffer reaches 128.
TEST(Reassembler, Exactly128BytesWithoutNewlineIsHeldUntilMoreArrives) {
    LineReassembler r; Sink s;
    feed_text(r, s, std::string(kLineBufferSize, 'q'));
    EXPECT_TRUE(s.lines.empty());
}

TEST(Reassembler, RoundTripMultiLineText) {
    LineReassembler r; Sink s;
    const std::string text =
        "boot ok\n"
        "\n"
        "altitude=1234 m, speed=56 m/s\n"
        "x\n"
        "a longer final line that crosses several chunks\n";
    feed_text(r, s, text);
    ASSERT_EQ(s.lines.size(), 5u);
    EXPECT_EQ(s.lines[0], "boot ok");
    EXPECT_EQ(s.lines[1], "");
    EXPECT_EQ(s.lines[2], "altitude=1234 m, speed=56 m/s");
    EXPECT_EQ(s.lines[3], "x");
    EXPECT_EQ(s.lines[4], "a longer final line that crosses several chunks");
}

TEST(Reassembler, InstancesAreIndependent) {
    LineReassembler a, b; Sink sa, sb;
    feed_chunk(a, sa, "AAAA\0\0\0\0");
    feed_chunk(b, sb, "BBBB\n\0\0\0");
    feed_chunk(a, sa, "\n\0\0\0\0\0\0\0");
    ASSERT_EQ(sa.lines.size(), 1u);
    ASSERT_EQ(sb.lines.size(), 1u);
    EXPECT_EQ(sa.lines[0], "AAAA");
    EXPECT_EQ(sb.lines[0], "BBBB");
}
