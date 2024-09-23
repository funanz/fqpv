#pragma once
#include <cstdint>
#include <functional>
#include "fd.hpp"

namespace fqpv
{
    class splice_transfer_engine
    {
        using measure_fn = std::function<void(uint_fast64_t)>;

        size_t transfer_size;

    public:
        splice_transfer_engine(size_t size) : transfer_size(size) {}

        void transfer(const fd& in, const fd& out, measure_fn measure) {
            constexpr auto flags = SPLICE_F_MOVE | SPLICE_F_MORE;// | SPLICE_F_NONBLOCK;

            for (;;) {
                auto size = in.splice(nullptr, out, nullptr, transfer_size, flags);
                if (size == 0) return;

                measure(size);
            }
        }
    };
}
