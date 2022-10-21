/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <tuple>
#include <utility>
#include <wtf/Function.h>
#include <wtf/MainThread.h>

namespace WTF {

template<typename> class CompletionHandler;
class CompletionHandlerCallThread {
public:
    static inline constexpr auto ConstructionThread = currentThreadLike;
    static inline constexpr auto MainThread = mainThreadLike;
    static inline constexpr auto AnyThread = anyThreadLike;
};

// Wraps a Function to make sure it is always called once and only once.
template <typename Out, typename... In>
class CompletionHandler<Out(In...)> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using OutType = Out;
    using InTypes = std::tuple<In...>;

    CompletionHandler() = default;

    template<typename CallableType, class = typename std::enable_if<std::is_rvalue_reference<CallableType&&>::value>::type>
    CompletionHandler(CallableType&& callable, ThreadLikeAssertion callThread = CompletionHandlerCallThread::ConstructionThread)
        : m_function(std::forward<CallableType>(callable))
        , m_callThread(WTFMove(callThread))
    {
    }

    CompletionHandler(CompletionHandler&&) = default;
    CompletionHandler& operator=(CompletionHandler&&) = default;

    ~CompletionHandler()
    {
        ASSERT_WITH_MESSAGE(!m_function, "Completion handler should always be called");
        m_callThread = anyThreadLike;
    }

    explicit operator bool() const { return !!m_function; }

    Out operator()(In... in)
    {
        assertIsCurrent(m_callThread);
        ASSERT_WITH_MESSAGE(m_function, "Completion handler should not be called more than once");
        return std::exchange(m_function, nullptr)(std::forward<In>(in)...);
    }

private:
    Function<Out(In...)> m_function;
    NO_UNIQUE_ADDRESS ThreadLikeAssertion m_callThread;
};

// Wraps a Function to make sure it is called at most once.
// If the CompletionHandlerWithFinalizer is destroyed and the function hasn't yet been called,
// the finalizer is invoked with the function as its argument.
template<typename> class CompletionHandlerWithFinalizer;
template <typename Out, typename... In>
class CompletionHandlerWithFinalizer<Out(In...)> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using OutType = Out;
    using InTypes = std::tuple<In...>;

    template<typename CallableType, class = typename std::enable_if<std::is_rvalue_reference<CallableType&&>::value>::type>
    CompletionHandlerWithFinalizer(CallableType&& callable, Function<void(Function<Out(In...)>&)>&& finalizer)
        : m_function(std::forward<CallableType>(callable))
        , m_finalizer(WTFMove(finalizer))
    {
    }

    CompletionHandlerWithFinalizer(CompletionHandlerWithFinalizer&&) = default;
    CompletionHandlerWithFinalizer& operator=(CompletionHandlerWithFinalizer&&) = default;

    ~CompletionHandlerWithFinalizer()
    {
        if (!m_function)
            return;
        assertIsCurrent(m_callThread);
        m_finalizer(m_function);
    }

    explicit operator bool() const { return !!m_function; }

    Out operator()(In... in)
    {
        assertIsCurrent(m_callThread);
        ASSERT_WITH_MESSAGE(m_function, "Completion handler should not be called more than once");
        return std::exchange(m_function, nullptr)(std::forward<In>(in)...);
    }

private:
    Function<Out(In...)> m_function;
    Function<void(Function<Out(In...)>&)> m_finalizer;
    NO_UNIQUE_ADDRESS ThreadLikeAssertion m_callThread;
};

namespace Detail {

template<typename Out, typename... In>
class CallableWrapper<CompletionHandler<Out(In...)>, Out, In...> : public CallableWrapperBase<Out, In...> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CallableWrapper(CompletionHandler<Out(In...)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    {
        RELEASE_ASSERT(m_completionHandler);
    }
    Out call(In... in) final { return m_completionHandler(std::forward<In>(in)...); }
private:
    CompletionHandler<Out(In...)> m_completionHandler;
};

} // namespace Detail

class CompletionHandlerCallingScope final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CompletionHandlerCallingScope() = default;

    CompletionHandlerCallingScope(CompletionHandler<void()>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    { }

    ~CompletionHandlerCallingScope()
    {
        if (m_completionHandler)
            m_completionHandler();
    }

    CompletionHandlerCallingScope(CompletionHandlerCallingScope&&) = default;
    CompletionHandlerCallingScope& operator=(CompletionHandlerCallingScope&&) = default;

    CompletionHandler<void()> release() { return WTFMove(m_completionHandler); }

private:
    CompletionHandler<void()> m_completionHandler;
};

} // namespace WTF

using WTF::CompletionHandler;
using WTF::CompletionHandlerCallThread;
using WTF::CompletionHandlerCallingScope;
using WTF::CompletionHandlerWithFinalizer;
