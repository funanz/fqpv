#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include "fd.hpp"

namespace fqpv
{
    class buffered_transfer_engine
    {
        static constexpr size_t BufSize = 1024 * 1024;
        using measure_fn = std::function<void(uint_fast64_t)>;

        std::unique_ptr<std::byte[]> buf_ptr;
        std::span<std::byte> buf;

    public:
        buffered_transfer_engine() {
            buf_ptr = std::make_unique<std::byte[]>(BufSize);
            buf = std::span(buf_ptr.get(), BufSize);
        }

        void transfer(const fd& in, const fd& out, measure_fn measure) {
            for (;;) {
                auto read = in.read(buf);
                if (read.empty()) return;

                out.write(read);
                measure(read.size());
            }
        }
    };
}
