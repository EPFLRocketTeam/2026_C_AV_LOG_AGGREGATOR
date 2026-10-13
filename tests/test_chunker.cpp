#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "log_aggregator/chunker.hpp"

namespace {

using Chunk = std::vector<uint8_t>;

void collect (void* ctx, const uint8_t chunk[8]) {
    static_cast<std::vector<Chunk>*>(ctx)->emplace_back(chunk, chunk + 8);
}

std::vector<Chunk> run (const std::string& text) {
    std::vector<Chunk> out;
    log_aggregator::chunk_and_send(reinterpret_cast<const uint8_t*>(text.data()),
                                   static_cast<uint32_t>(text.size()), &out, collect);
    return out;
}

Chunk chunk_of (const char* s) {
    Chunk c(8, 0);
    memcpy(c.data(), s, strlen(s));
    return c;
}

}

TEST(Chunker, Length0SendsNothing) {
    std::vector<Chunk> out;
    const uint8_t dummy = 'a';
    log_aggregator::chunk_and_send(&dummy, 0, &out, collect);
    EXPECT_TRUE(out.empty());
}

TEST(Chunker, Length0AcceptsNullData) {
    std::vector<Chunk> out;
    log_aggregator::chunk_and_send(nullptr, 0, &out, collect);
    EXPECT_TRUE(out.empty());
}

TEST(Chunker, Length1IsOnePaddedChunk) {
    auto out = run("A");
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], (Chunk{'A', 0, 0, 0, 0, 0, 0, 0}));
}

TEST(Chunker, Length7IsOnePaddedChunk) {
    auto out = run("ABCDEFG");
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], (Chunk{'A', 'B', 'C', 'D', 'E', 'F', 'G', 0}));
}

TEST(Chunker, Length8IsOneFullChunkNoPadding) {
    auto out = run("ABCDEFGH");
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], chunk_of("ABCDEFGH"));
}

TEST(Chunker, Length9IsFullChunkThenPaddedTail) {
    auto out = run("ABCDEFGHI");
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], chunk_of("ABCDEFGH"));
    EXPECT_EQ(out[1], (Chunk{'I', 0, 0, 0, 0, 0, 0, 0}));
}

TEST(Chunker, Length16IsTwoFullChunks) {
    auto out = run("ABCDEFGHIJKLMNOP");
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], chunk_of("ABCDEFGH"));
    EXPECT_EQ(out[1], chunk_of("IJKLMNOP"));
}

TEST(Chunker, Length17IsTwoFullChunksThenPaddedTail) {
    auto out = run("ABCDEFGHIJKLMNOPQ");
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], chunk_of("ABCDEFGH"));
    EXPECT_EQ(out[1], chunk_of("IJKLMNOP"));
    EXPECT_EQ(out[2], (Chunk{'Q', 0, 0, 0, 0, 0, 0, 0}));
}

TEST(Chunker, DataWithoutZeroBytesContainsZeroOnlyInPadding) {
    std::string text(13, 'x');
    auto out = run(text);
    ASSERT_EQ(out.size(), 2u);
    for (uint8_t b : out[0]) EXPECT_EQ(b, 'x');
    for (int i = 0; i < 5; i++) EXPECT_EQ(out[1][i], 'x');
    for (int i = 5; i < 8; i++) EXPECT_EQ(out[1][i], 0);
}
