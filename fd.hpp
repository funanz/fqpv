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

    class ownership_fd {
        int fd_;
        bool owned_;

        ownership_fd(const ownership_fd&) = delete;
        ownership_fd& operator=(const ownership_fd&) = delete;

    public:
        ownership_fd() noexcept : fd_(-1), owned_(false) {}
        ownership_fd(int fd, bool owned) noexcept : fd_(fd), owned_(owned) {}

        ~ownership_fd() {
            close();
        }

        ownership_fd(ownership_fd&& r) noexcept : fd_(-1), owned_(false) {
            swap(*this, r);
        }

        ownership_fd& operator=(ownership_fd&& r) {
            if (this != &r) {
                close();
                swap(*this, r);
            }
            return *this;
        }

        friend void swap(ownership_fd& a, ownership_fd& b) noexcept {
            std::swap(a.fd_, b.fd_);
            std::swap(a.owned_, b.owned_);
        }

        [[nodiscard]]
        int get() const noexcept {
            return fd_;
        }

        void close() {
            if (owned_ && fd_ != -1)
                ownership_fd::close(fd_);

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
            auto bytes = std::as_writable_bytes(span);
            std::span<std::byte> buf = bytes;
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
            std::span<T> buf = bytes;
            while (!buf.empty()) {
                auto ret = ::write(fd_, buf.data(), buf.size());
                if (ret == -1) {
                    if (errno == EAGAIN || errno == EINTR)
                        continue;
                    else if (errno == EPIPE)
                        throw pipe_error(errno);
                    else
                        throw io_error(errno);
                }
                buf = buf.subspan(ret);
            }
        }

        template <class T, size_t E>
        requires (sizeof(T) > 1)
        void write(std::span<T, E> span) const {
            write(std::as_bytes(span));
        }

        [[nodiscard]]
        bool can_splice(const ownership_fd& out) const {
            return is_pipe() || out.is_pipe();
        }

        ssize_t splice(off_t* off_in, const ownership_fd& out, off_t* off_out, size_t len, unsigned int flags) const {
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
        static ownership_fd stdin() {
            return {STDIN_FILENO, false};
        }

        [[nodiscard]]
        static ownership_fd stdout() {
            return {STDOUT_FILENO, false};
        }

        [[nodiscard]]
        static ownership_fd stderr() {
            return {STDERR_FILENO, false};
        }

        template <typename... Args>
        [[nodiscard]]
        static ownership_fd open(const std::string& file, int flags, Args&&... args) {
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
        static std::tuple<ownership_fd, ownership_fd> pipe2(int flags) {
            int fds[2];
            auto ret = ::pipe2(fds, flags);
            if (ret == -1)
                throw io_error(errno);

            return {
                ownership_fd{fds[0], true},
                ownership_fd{fds[1], true},
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
    };

    using fd = ownership_fd;
}
