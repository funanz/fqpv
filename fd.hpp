#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fqpv
{
    class runtime_error : public std::runtime_error {
    public:
        explicit runtime_error(const std::string& s) : std::runtime_error(s) {}
        explicit runtime_error(const char* s) : std::runtime_error(s) {}
    };

    class io_error : public runtime_error {
        int errno_;
    public:
        io_error(int arg_errno, const std::string& s)
            : runtime_error(s), errno_(arg_errno) {}
        io_error(int arg_errno, const char* s)
            : runtime_error(s), errno_(arg_errno) {}
        explicit io_error(int arg_errno)
            : runtime_error(strerror(arg_errno)), errno_(arg_errno) {}

        int get_errno() const { return errno_; }
    };

    class file_error : public io_error {
        std::string file_;
    public:
        file_error(const std::string& file, int arg_errno)
            : io_error(arg_errno, file + ": " + strerror(arg_errno)), file_(file) {}

        const std::string& get_file() const { return file_; }
    };

    class pipe_error : public io_error {
    public:
        explicit pipe_error(int arg_errno) : io_error(arg_errno) {}
    };

    class splice_error : public io_error {
    public:
        explicit splice_error(int arg_errno) : io_error(arg_errno) {}
    };

    class fd {
        int fd_;
        bool owned_;

        fd(const fd&) = delete;
        fd& operator=(const fd&) = delete;

    public:
        fd() noexcept : fd_(-1), owned_(false) {}
        fd(int fd, bool owned) noexcept : fd_(fd), owned_(owned) {}

        ~fd() {
            close();
        }

        fd(fd&& r) noexcept : fd_(-1), owned_(false) {
            swap(*this, r);
        }

        fd& operator=(fd&& r) {
            if (this != &r) {
                close();
                swap(*this, r);
            }
            return *this;
        }

        friend void swap(fd& a, fd& b) noexcept {
            std::swap(a.fd_, b.fd_);
            std::swap(a.owned_, b.owned_);
        }

        [[nodiscard]]
        int get() const noexcept {
            return fd_;
        }

        void close() {
            if (owned_ && fd_ != -1)
                fd::close(fd_);

            fd_ = -1;
            owned_ = false;
        }

        void set_nonblock(bool enable) const {
            auto flags = fcntl(F_GETFL);

            if (enable)
                flags |= O_NONBLOCK;
            else
                flags &= ~O_NONBLOCK;

            fcntl(F_SETFL, flags);
        }

        [[nodiscard]]
        int get_pipe_size() const {
            return fcntl(F_GETPIPE_SZ);
        }

        int set_pipe_size(int size) const {
            return fcntl(F_SETPIPE_SZ, size);
        }

        [[nodiscard]]
        bool is_pipe() const {
            return get_stat_mode() == S_IFIFO;
        }

        int try_extend_pipe_size(int max_size) const {
            int current_size;
            try {
                if (is_pipe())
                    current_size = get_pipe_size();
                else
                    return -1;
            }
            catch (io_error&) {
                return -1;
            }

            auto size = max_size;
            while (size > current_size) {
                try {
                    return set_pipe_size(size);
                }
                catch (io_error&) {
                    size /= 2;
                }
            }
            return -1;
        }

        template <class T, size_t E>
        requires (sizeof(T) == 1)
        [[nodiscard]]
        std::span<T> read(std::span<T, E> bytes) const {
            for (;;) {
                auto ret = ::read(fd_, bytes.data(), bytes.size());
                if (ret == -1) {
                    if (errno == EAGAIN || errno == EINTR)
                        continue;
                    else
                        throw io_error(errno);
                }
                return bytes.first(ret);
            }
        }

        template <class T, size_t E>
        requires (sizeof(T) > 1)
        [[nodiscard]]
        std::span<T> read(std::span<T, E> span) const {
            auto bytes = to_byte_span(span);
            auto buf = bytes;
            for (;;) {
                auto buf_read = read(buf);
                buf = buf.subspan(buf_read.size());

                auto total_read = bytes.size() - buf.size();
                if (total_read % sizeof(T)) {
                    if (buf_read.empty())
                        throw io_error(0, "unexpected end of stream");
                } else {
                    return span.first(total_read / sizeof(T));
                }
            }
        }

        template <class T, size_t E>
        requires (sizeof(T) == 1)
        void write(std::span<T, E> bytes) const {
            while (!bytes.empty()) {
                auto ret = ::write(fd_, bytes.data(), bytes.size());
                if (ret == -1) {
                    if (errno == EAGAIN || errno == EINTR)
                        continue;
                    else if (errno == EPIPE)
                        throw pipe_error(errno);
                    else
                        throw io_error(errno);
                }
                bytes = bytes.subspan(ret);
            }
        }

        template <class T, size_t E>
        requires (sizeof(T) > 1)
        void write(std::span<T, E> span) const {
            write(to_byte_span(span));
        }

        [[nodiscard]]
        bool can_splice(const fd& out) const {
            auto sm_in = get_stat_mode();
            auto sm_out = out.get_stat_mode();

            return (sm_in == S_IFIFO) || (sm_out == S_IFIFO);
        }

        ssize_t splice(off_t* off_in, const fd& out, off_t* off_out, size_t len, unsigned int flags) const {
            for (;;) {
                auto ret = ::splice(fd_, off_in, out.fd_, off_out, len, flags);
                if (ret == -1) {
                    if (errno == EAGAIN || errno == EINTR)
                        continue;
                    else if (errno == EPIPE)
                        throw pipe_error(errno);
                    else if (errno == EINVAL)
                        throw splice_error(errno);
                    else
                        throw io_error(errno);
                }
                return ret;
            }
        }

        template <class... Args>
        int fcntl(int cmd, Args&&... args) const {
            for (;;) {
                auto ret = ::fcntl(fd_, cmd, std::forward<Args>(args)...);
                if (ret == -1) {
                    if (errno == EAGAIN || errno == EINTR)
                        continue;
                    else
                        throw io_error(errno);
                }
                return ret;
            }
        }

        [[nodiscard]]
        static fd stdin() {
            return {STDIN_FILENO, false};
        }

        [[nodiscard]]
        static fd stdout() {
            return {STDOUT_FILENO, false};
        }

        [[nodiscard]]
        static fd stderr() {
            return {STDERR_FILENO, false};
        }

        template <typename... Args>
        [[nodiscard]]
        static fd open(const std::string& file, int flags, Args&&... args) {
            for (;;) {
                auto fd = ::open(file.c_str(), flags, std::forward<Args>(args)...);
                if (fd == -1) {
                    if (errno == EINTR)
                        continue;
                    else
                        throw file_error(file, errno);
                }
                return {fd, true};
            }
        }

        [[nodiscard]]
        static std::tuple<fd, fd> pipe2(int flags) {
            int fds[2];
            auto ret = ::pipe2(fds, flags);
            if (ret == -1)
                throw io_error(errno);

            return {
                fd{fds[0], true},
                fd{fds[1], true},
            };
        }

    private:
        static void close(int fd) {
            if (::close(fd) == -1)
                throw io_error(errno);
        }

        [[nodiscard]]
        mode_t get_stat_mode() const {
            struct stat sb;
            if (fstat(fd_, &sb) == -1)
                throw io_error(errno);

            return sb.st_mode & S_IFMT;
        }

        template <class T, size_t E>
        [[nodiscard]]
        static std::span<std::byte> to_byte_span(std::span<T, E> span) {
            auto data = reinterpret_cast<std::byte*>(span.data());
            auto size = span.size() * sizeof(T);

            return {data, size};
        }
    };
}
