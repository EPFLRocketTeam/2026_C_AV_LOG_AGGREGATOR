
#pragma once

#include <cstdint>

/**
 * Sender-side half of the log aggregator: splits a raw byte buffer (a
 * printf write, typically) into consecutive 8-byte chunks suitable for
 * one CAN frame each. Transport-agnostic -- this file knows nothing
 * about CAN; the caller's send_chunk callback is what actually puts a
 * chunk on the wire (e.g. via prc_intranet's send_log_chunk_dpr_eth()).
 */
namespace log_aggregator {

    // Called once per 8-byte chunk, in order. The last chunk of a buffer
    // whose length isn't a multiple of 8 is zero-padded; do not go
    // looking for a length parameter here, there isn't one -- the
    // receiver infers the meaningful length by scanning for the first
    // \0 in each chunk. This means chunked data must never contain a
    // literal \0 byte -- true for ordinary printf-style C-string text,
    // not true in general for arbitrary binary data.
    using SendChunkFn = void (*)(void* ctx, const uint8_t chunk[8]);

    // Splits data[0..length) into 8-byte pieces and calls send_chunk once
    // per piece. No-op if length is 0.
    void chunk_and_send (const uint8_t* data, uint32_t length, void* ctx, SendChunkFn send_chunk) noexcept;

};
