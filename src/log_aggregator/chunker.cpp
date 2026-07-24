
#include "log_aggregator/chunker.hpp"

#include <cstring>

namespace log_aggregator {

void chunk_and_send (const uint8_t* data, uint32_t length, void* ctx, SendChunkFn send_chunk) noexcept {
    uint32_t offset = 0;

    while (offset < length) {
        uint8_t piece[8] = {0};
        uint32_t n = (length - offset < 8) ? (length - offset) : 8;

        memcpy(piece, data + offset, n);
        send_chunk(ctx, piece);

        offset += n;
    }
}

}
