#pragma once
#include <cstdint>
#include <functional>
#include "fd.hpp"

namespace fqpv
{
    class splice_transfer_engine
    {
        static constexpr size_t PipeSize = 1024 * 1024;
        using measure_fn = std::function<void(uint_fast64_t)>;

    public:
        void transfer(const fd& in, const fd& out, measure_fn measure) {
            constexpr auto flags = SPLICE_F_MOVE | SPLICE_F_MORE;// | SPLICE_F_NONBLOCK;

            for (;;) {
                auto size = in.splice(nullptr, out, nullptr, PipeSize, flags);
                if (size == 0) return;

                measure(size);
            }
        }
    };
}
