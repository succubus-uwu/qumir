#pragma once

#include <coroutine>
#include <expected>
#include <optional>
#include <exception>
#include <utility>
#include <cassert>

namespace NQumir {

template<typename T> struct TFinalAwaiter;

template<typename T> struct TFuture;

template<typename T>
struct TPromiseBase {
    std::suspend_never initial_suspend() { return {}; }
    TFinalAwaiter<T> final_suspend() noexcept;
    /// Handle to the caller coroutine (initialized to a no-operation coroutine).
    std::coroutine_handle<> Caller = std::noop_coroutine();
};

template<typename T>
struct TPromise: public TPromiseBase<T> {
    TFuture<T> get_return_object();

    void return_value(const T& t) {
        ErrorOr = t;
    }

    void return_value(T&& t) {
        ErrorOr = std::move(t);
    }

    void unhandled_exception() {
        ErrorOr = std::unexpected(std::current_exception());
    }

    /// Optional container that holds either the result or an exception.
    std::optional<std::expected<T, std::exception_ptr>> ErrorOr;
};

template<typename T>
struct TFutureBase: public std::coroutine_handle<TPromise<T>> {
    TFutureBase() = default;
    TFutureBase(TPromise<T>& promise)
        : std::coroutine_handle<TPromise<T>>(std::coroutine_handle<TPromise<T>>::from_promise(promise))
    { }
    TFutureBase(TFutureBase&& other)
        : std::coroutine_handle<TPromise<T>>(other)
    {
        other.std::coroutine_handle<TPromise<T>>::operator=(nullptr);
    }
    TFutureBase(const TFutureBase&) = delete;
    TFutureBase& operator=(const TFutureBase&) = delete;
    TFutureBase& operator=(TFutureBase&& other) = delete;

    ~TFutureBase() {
        if (*this) {
            this->destroy();
        }
    }

    bool await_ready() const {
        return this->promise().ErrorOr.has_value();
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) {
        this->promise().Caller = caller;
        return *this;
    }

    using promise_type = TPromise<T>;
};

template<> struct TFuture<void>;

template<typename T>
struct TFuture : public TFutureBase<T> {
    T await_resume() {
        auto& errorOr = *this->promise().ErrorOr;
        if (errorOr.has_value()) {
            return std::move(errorOr.value());
        } else {
            std::rethrow_exception(errorOr.error());
        }
    }
};

template<>
struct TPromise<void>: public TPromiseBase<void> {
    TFuture<void> get_return_object();

    void return_void() {
        ErrorOr = nullptr;
    }

    void unhandled_exception() {
        ErrorOr = std::current_exception();
    }

    std::optional<std::exception_ptr> ErrorOr;
};


template<>
struct TFuture<void> : public TFutureBase<void> {
    void await_resume() {
        auto& errorOr = *this->promise().ErrorOr;
        if (errorOr) {
            std::rethrow_exception(errorOr);
        }
    }

    template<typename Func>
    auto Accept(Func func) -> TFuture<void> {
        auto prev = std::move(*this);
        co_await prev;
        co_return func();
    }
};

template<typename T>
struct TFinalAwaiter {
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise<T>> h) noexcept {
        return h.promise().Caller;
    }
    void await_resume() noexcept { }
};

inline TFuture<void> TPromise<void>::get_return_object() { return { TFuture<void>{*this} }; }
template<typename T>
TFuture<T> TPromise<T>::get_return_object() { return { TFuture<T>{*this} }; }


template<typename T>
TFinalAwaiter<T> TPromiseBase<T>::final_suspend() noexcept { return {}; }


struct ITypeLessFuture {
    virtual ~ITypeLessFuture() = default;

    virtual bool done() = 0;
    virtual void resume() = 0;
    virtual void destroy() = 0;

    virtual bool await_ready() = 0;
    virtual void await_suspend(std::coroutine_handle<> caller) = 0;
    virtual void await_resume(void* result) = 0;
    virtual std::coroutine_handle<> continuation() = 0;
};

template<typename T>
struct TWrappedFuture : public ITypeLessFuture {
    TWrappedFuture(TFuture<T>&& future)
        : Future(std::move(future))
    { }

    bool done() override {
        if (Future) {
            return Future->done();
        }
        return true;
    }

    void resume() override {
        if (Future && !Future->done()) {
            Future->resume();
        }
    }

    void destroy() override {
        Future.reset();
    }

    bool await_ready() override {
        assert(Future);
        return Future->await_ready();
    }

    void await_suspend(std::coroutine_handle<> caller) override {
        assert(Future);
        Future->await_suspend(caller);
    }

    void await_resume(void* result) override {
        assert(Future);
        if (std::is_same_v<T, void>) {
            Future->await_resume();
        } else {
            new (result) T(Future->await_resume());
        }
    }

private:
    std::optional<TFuture<T>> Future;
};

extern "C" {

// Assume module exports specific ITypeLessFuture contructor for each TFuture<T> specialization
void __qumir_future_destroy(ITypeLessFuture* future);
bool __qumir_future_done(ITypeLessFuture* future);
void __qumir_future_resume(ITypeLessFuture* future);
bool __qumir_future_await_ready(ITypeLessFuture* future);
void __qumir_future_await_suspend(ITypeLessFuture* future, void* caller);
void __qumir_future_await_resume(ITypeLessFuture* future, void* result);

};

} // namespace NQumir
