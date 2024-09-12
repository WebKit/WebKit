/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if (_LIBCPP_VERSION >= 14000) && !defined(_LIBCPP_HAS_NO_CXX20_COROUTINES)
#import <coroutine>
#else
// FIXME: Remove this once all supported toolchains have non-experimental coroutine support.
#import <experimental/coroutine>
namespace std {
using std::experimental::coroutine_handle;
using std::experimental::suspend_never;
using std::experimental::suspend_always;
}
#endif

namespace TestWebKitAPI {

template<typename PromiseType>
struct CoroutineHandle {
    CoroutineHandle(std::coroutine_handle<PromiseType>&& handle)
        : handle(WTFMove(handle)) { }
    CoroutineHandle(const CoroutineHandle&) = delete;
    CoroutineHandle(CoroutineHandle&& other)
        : handle(std::exchange(other.handle, nullptr)) { }
    ~CoroutineHandle()
    {
        if (handle)
            handle.destroy();
    }
    std::coroutine_handle<PromiseType> handle;
};

struct Task {
    struct promise_type {
        Task get_return_object() { return { std::coroutine_handle<promise_type>::from_promise(*this) }; }
        std::suspend_never initial_suspend() { return { }; }
        std::suspend_always final_suspend() noexcept { return { }; }
        void unhandled_exception() { }
        void return_void() { }
    };
    CoroutineHandle<promise_type> handle;
};

}
