#pragma once
#include <chrono>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <tuple>

namespace fqpv
{
    class speedometer
    {
        using clock = std::chrono::high_resolution_clock;
        using time_point = decltype(clock::now());

        time_point start;
        time_point start_interval;
        time_point last;
        uint_fast64_t bytes;
        uint_fast64_t bytes_interval;
        double interval_time;
        std::string remarks;

    public:
        speedometer() {
            interval_time = 1.0;
            reset();
        }

        ~speedometer() {
            finish();
        }

        void reset() {
            start = clock::now();
            start_interval = start;
            last = start;

            bytes = 0;
            bytes_interval = 0;
        }

        void measure(uint_fast64_t increase) {
            last = clock::now();
            bytes += increase;
            bytes_interval += increase;

            auto sec_interval = count<sec_double>(last - start_interval);
            if (sec_interval >= interval_time) {
                print(bytes_interval / sec_interval);
                bytes_interval = 0;
                start_interval = clock::now();
            }
        }

        void finish() const {
            if (start == last) {
                print(0);
            } else {
                auto sec_total = count<sec_double>(last - start);
                print(bytes / sec_total);
            }
            fprintf(stderr, "\n");
            fflush(stderr);
        }

        void set_interval_time(double sec) {
            interval_time = sec;
        }

        void set_remarks(const std::string& s) {
            remarks = s;
        }

    private:
        using sec_double = std::chrono::duration<double>;
        using msec_int64 = std::chrono::milliseconds;

        template <class Period, class Duration>
        [[nodiscard]]
        static Period::rep count(const Duration& d) {
            return std::chrono::duration_cast<Period>(d).count();
        }

        void print(double bps) const {
            auto [scaled_bytes, prefix_bytes] = binary_prefix(bytes);
            auto [scaled_bps, prefix_bps] = binary_prefix(bps);

            std::chrono::hh_mm_ss hms(last - start);
            fprintf(stderr,
                    "%6.2f %3s %2ld:%02ld:%02ld.%03ld [%6.2f %3s/s] %-15s\r",
                    scaled_bytes, prefix_bytes.c_str(),
                    hms.hours().count(),
                    hms.minutes().count(),
                    hms.seconds().count(),
                    count<msec_int64>(hms.subseconds()),
                    scaled_bps, prefix_bps.c_str(),
                    remarks.c_str());
        }

        [[nodiscard]]
        static std::tuple<double, std::string> binary_prefix(double bytes) {
            auto scaled_bytes = bytes;

            for (auto prefix : prefixes_without_last) {
                if (scaled_bytes < 1000)
                    return {scaled_bytes, prefix};

                scaled_bytes /= 1024;
            }

            return {scaled_bytes, prefix_last};
        }

        static constexpr const char* prefixes_array[] = {
            "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB",
            "RiB", "QiB",
        };
        static constexpr auto prefixes = std::span(prefixes_array);
        static constexpr auto prefixes_without_last = prefixes.first(prefixes.size() - 1);
        static constexpr auto prefix_last = prefixes.back();
    };
}
