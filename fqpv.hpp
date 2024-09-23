#pragma once
#include <span>
#include <string>
#include <signal.h>
#include "fd.hpp"
#include "buffered_engine.hpp"
#include "splice_engine.hpp"
#include "speedometer.hpp"

namespace fqpv
{
    class fqpv
    {
        static constexpr int PipeSize = 1024 * 1024;

        fd stdin;
        fd stdout;
        buffered_transfer_engine buffered {PipeSize};
        splice_transfer_engine splice {PipeSize};
        speedometer speed;

    public:
        fqpv() {
            stdin = fd::stdin();
            stdout = fd::stdout();
        }

        int main(std::span<const char*> args) {
            try {
                trap_sigpipe();
                stdin.try_extend_pipe_size(PipeSize);
                stdout.try_extend_pipe_size(PipeSize);

                run(args);
            }
            catch (pipe_error&) {
                return 0;
            }
            catch (runtime_error& e) {
                print_error(e);
                return 1;
            }
            return 0;
        }

    private:
        void trap_sigpipe() {
            if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
                throw runtime_error(strerror(errno));
        }

        void run(std::span<const char*> args) {
            if (args.size() > 1) {
                for (std::string arg : args.subspan(1)) {
                    if (arg == "-")
                        transfer(stdin, stdout);
                    else
                        transfer(arg, stdout);
                }
            } else {
                transfer(stdin, stdout);
            }
        }

        void transfer(const fd& in, const fd& out) {
            if (in.can_splice(out)) {
                try {
                    speed.set_remarks("<splice>");
                    splice.transfer(in, out, [&](auto n){ speed.measure(n); });
                    return;
                }
                catch (splice_error&) {
                    // Fallback to buffered transfer. e.g., for /dev/zero.
                }
            }

            speed.set_remarks("<buffered>");
            buffered.transfer(in, out, [&](auto n){ speed.measure(n); });
        }

        void transfer(const std::string& file, const fd& out) {
            try {
                auto in = fd::open(file, O_RDONLY);
                transfer(in, out);
            }
            catch (file_error& e) {
                print_error(e);
            }
        }

        static void print_error(runtime_error& e) {
            fprintf(stderr, "fqpv: %s\n", e.what());
        }
    };
}
