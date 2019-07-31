/* Copyright (c) 2018 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License,
 * attached with Common Clause Condition 1.0, found in the LICENSES directory.
 */

#pragma once

#include <cstdarg>
#include <string>
#include <ostream>
#include <memory>


/**
 * Status is modeled on the one from levelDB, beyond that,
 * this one adds support on move semantics and formatted error messages.
 *
 * Status is as cheap as raw pointers in the successful case,
 * without any heap memory allocations.
 */

namespace nebula {

class Status final {
public:
    Status() = default;

    ~Status() = default;

    Status(const Status &rhs) {
        state_ = rhs.state_ == nullptr ? nullptr : copyState(rhs.state_.get());
    }

    Status& operator=(const Status &rhs) {
        // `state_ == rhs.state_' means either `this == &rhs',
        // or both `*this' and `rhs' are OK
        if (state_ != rhs.state_) {
            state_ = rhs.state_ == nullptr ? nullptr : copyState(rhs.state_.get());
        }
        return *this;
    }

    Status(Status &&rhs) noexcept {
        state_ = std::move(rhs.state_);
    }

    Status& operator=(Status &&rhs) noexcept {
        // `state_ == rhs.state_' means either `this == &rhs',
        // or both `*this' and `rhs' are OK
        if (state_ != rhs.state_) {
            state_ = std::move(rhs.state_);
        }
        return *this;
    }

    bool operator==(const Status &rhs) const {
        // `state_ == rhs.state_' means either `this == &rhs',
        // or both `*this' and `rhs' are OK
        if (state_ == rhs.state_) {
            return true;
        }
        return code() == rhs.code();
    }

    bool ok() const {
        return state_ == nullptr;
    }

    static Status OK() {
        return Status();
    }

#define STATUS_GENERATOR(ERROR)                         \
    static Status ERROR() {                             \
        return Status(k##ERROR, "");                    \
    }                                                   \
                                                        \
                                                        \
    static Status ERROR(const char *fmt, ...) {         \
        va_list args;                                   \
        va_start(args, fmt);                            \
        auto msg = format(fmt, args);                   \
        va_end(args);                                   \
        return Status(k##ERROR, msg);                   \
    }                                                   \
                                                        \
    bool is##ERROR() const {                            \
        return code() == k##ERROR;                      \
    }

    // General errors
    STATUS_GENERATOR(Error);
    STATUS_GENERATOR(NoSuchFile);


#undef STATUS_GENERATOR

    std::string toString() const;

    friend std::ostream& operator<<(std::ostream &os, const Status &status);

    // If some kind of error really needs to be distinguished with others using a specific
    // code, other than a general code and specific msg, you could add a new code below,
    // e.g. kSomeError, and add the cooresponding STATUS_GENERATOR(SomeError)
    enum Code : uint16_t {
        // OK
        kOk                     = 0,
        // 1xx, for general errors
        kError                  = 101,
        kNoSuchFile             = 102,
    };

    Code code() const {
        if (state_ == nullptr) {
            return kOk;
        }
        return reinterpret_cast<const Header*>(state_.get())->code_;
    }

private:
    // REQUIRES: stat_ != nullptr
    uint16_t size() const {
        return reinterpret_cast<const Header*>(state_.get())->size_;
    }

    Status(Code code, const std::string &msg);

    static std::unique_ptr<const char[]> copyState(const char *state);

    static std::string format(const char *fmt, va_list args);

private:
    struct Header {
        uint16_t                    size_;
        Code                        code_;
    };
    static constexpr auto kHeaderSize = sizeof(Header);
    // state_ == nullptr indicates OK
    // otherwise, the buffer layout is:
    // state_[0..1]                 length of the error msg, i.e. size() - kHeaderSize
    // state_[2..3]                 code
    // state_[4...]                 verbose error message
    std::unique_ptr<const char[]>   state_;
};


inline std::ostream& operator<<(std::ostream &os, const Status &status) {
    return os << status.toString();
}

}   // namespace nebula