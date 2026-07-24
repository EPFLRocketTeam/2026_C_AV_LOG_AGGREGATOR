
#include "log_aggregator/reassembler.hpp"

namespace log_aggregator {

void LineReassembler::feed (const uint8_t chunk[8], void* ctx, LineReadyFn line_ready) noexcept {
    for (uint32_t i = 0; i < 8; i++) {
        uint8_t byte = chunk[i];

        if (byte == '\0') {
            // Sender's padding sentinel: nothing more of value in this chunk.
            break;
        }

        if (byte == '\n') {
            line_ready(ctx, reinterpret_cast<const char*>(buffer_), used_);
            used_ = 0;
            continue;
        }

        if (used_ >= kLineBufferSize) {
            // Overlong line with no \n seen yet: flush what we have as a
            // truncated line rather than dropping bytes or overflowing.
            line_ready(ctx, reinterpret_cast<const char*>(buffer_), used_);
            used_ = 0;
        }

        buffer_[used_++] = byte;
    }
}

}
