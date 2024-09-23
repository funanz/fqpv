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
        using measure_fn = std::function<void(uint_fast64_t)>;

        std::unique_ptr<std::byte[]> buf_ptr;
        std::span<std::byte> buf;

    public:
        buffered_transfer_engine(size_t size) {
            buf_ptr = std::make_unique<std::byte[]>(size);
            buf = std::span(buf_ptr.get(), size);
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
