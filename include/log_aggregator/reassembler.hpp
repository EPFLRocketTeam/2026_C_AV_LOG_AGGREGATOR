
#pragma once

#include <cstdint>

/**
 * Receiver-side half of the log aggregator: reassembles the 8-byte
 * chunks chunker.hpp produced back into complete lines. Transport-
 * agnostic, same as chunker.hpp -- the caller is responsible for getting
 * chunks here (e.g. from prc_intranet's on_log_chunk_dpr_eth callback).
 *
 * One LineReassembler instance per source stream. A board that relays
 * logs from N other boards needs N instances, one 128-byte buffer each
 * -- there is no shared/multiplexed buffer, interleaved chunks from
 * different sources must never be fed into the same instance or their
 * lines will corrupt into each other.
 */
namespace log_aggregator {

    constexpr uint32_t kLineBufferSize = 128;

    // Called once per complete line, with the accumulated text (NOT
    // null-terminated, NOT including the trailing \n) and its length.
    // The pointer is only valid for the duration of the call.
    using LineReadyFn = void (*)(void* ctx, const char* line, uint32_t length);

    class LineReassembler {
    public:
        // Feeds one 8-byte chunk (a full CAN frame's payload). Appending
        // stops at the first \0 in chunk (the sender's chunk_and_send()
        // padding sentinel). Every \n found flushes everything
        // accumulated so far (excluding the \n itself) via line_ready,
        // then continues with whatever's left in the same chunk. If the
        // buffer fills without a \n ever showing up, it force-flushes a
        // truncated line rather than growing unbounded or silently
        // dropping bytes.
        void feed (const uint8_t chunk[8], void* ctx, LineReadyFn line_ready) noexcept;

    private:
        uint8_t  buffer_[kLineBufferSize] = {};
        uint32_t used_ = 0;
    };

};
