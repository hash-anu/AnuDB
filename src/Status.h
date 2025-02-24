#ifndef STATUS_H
#define STATUS_H

#include <string>

namespace anudb {
    // Error handling class
    class Status {
    public:
        enum Code {
            OKAY = 0,
            NOT_FOUND = 1,
            CORRUPTION = 2,
            NOT_SUPPORTED = 3,
            INVALID_ARGUMENT = 4,
            IO_ERROR = 5,
            INTERNAL_ERROR = 6
        };

        Status() : code_(OKAY) {}
        Status(Code code, const std::string& msg) : code_(code), msg_(msg) {}

        bool ok() const { return code_ == OKAY; }
        bool isNotFound() const { return code_ == NOT_FOUND; }
        Code code() const { return code_; }
        std::string message() const { return msg_; }

        static Status OK() { return Status(); }
        static Status NotFound(const std::string& msg) { return Status(NOT_FOUND, msg); }
        static Status Corruption(const std::string& msg) { return Status(CORRUPTION, msg); }
        static Status NotSupported(const std::string& msg) { return Status(NOT_SUPPORTED, msg); }
        static Status InvalidArgument(const std::string& msg) { return Status(INVALID_ARGUMENT, msg); }
        static Status IOError(const std::string& msg) { return Status(IO_ERROR, msg); }
        static Status InternalError(const std::string& msg) { return Status(INTERNAL_ERROR, msg); }

    private:
        Code code_;
        std::string msg_;
    };
}

#endif // STATUS_H
