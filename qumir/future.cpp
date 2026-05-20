#include "future.h"

namespace NQumir {

extern "C" {

void __qumir_future_destroy(ITypeLessFuture* future) {
    future->destroy();
}

bool __qumir_future_done(ITypeLessFuture* future) {
    return future->done();
}

void __qumir_future_resume(ITypeLessFuture* future) {
    future->resume();
}

bool __qumir_future_await_ready(ITypeLessFuture* future) {
    return future->await_ready();
}

void __qumir_future_await_suspend(ITypeLessFuture* future, void* caller) {
    future->await_suspend(std::coroutine_handle<>::from_address(caller));
}

void __qumir_future_await_resume(ITypeLessFuture* future, void* result) {
    future->await_resume(result);
}

} // extern "C"
} // namespace NQumir
