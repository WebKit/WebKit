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
#include <wtf/ThreadAssertions.h>

namespace WTF {

// Token returned by CompletionHandler::operator() to prove the handler was called.
// The constructor is private; only CompletionHandler itself can manufacture
// one, so the only way to obtain a token is to actually call the handler.
class CompletionHandlerCalledToken {
    CompletionHandlerCalledToken() = default;
    template<typename, bool> friend class CompletionHandler;
public:
    CompletionHandlerCalledToken(CompletionHandlerCalledToken&&) = default;
    CompletionHandlerCalledToken& operator=(CompletionHandlerCalledToken&&) = default;

    // Strict defer: lambda takes handler by move and must return CompletionHandlerCalledToken
    // by actually calling the handler (or a propagating async API). No escape hatch.
    template<typename Sig, typename Lambda>
    static CompletionHandlerCalledToken defer(CompletionHandler<Sig, true>&&, Lambda&&);

    // Cheaty defer for burn-down: lambda gets a pre-produced deferred token it can return
    // without proving the inner async callback calls the handler. Convert to defer() as
    // async APIs are updated to propagate CompletionHandlerCalledToken.
    template<typename Sig, typename Lambda>
    static CompletionHandlerCalledToken deferUnchecked(CompletionHandler<Sig, true>&, Lambda&&);

    // For the rare case where a function accepts a nullable enforced handler and needs a
    // token on the no-handler path. Prefer making the handler always present over using this.
    static CompletionHandlerCalledToken forNullHandler() { return { }; }
};

template<typename T>
inline constexpr bool IsCompletionHandlerCalledToken = std::is_same_v<T, CompletionHandlerCalledToken>;

class CompletionHandlerCallThread {
public:
    static inline constexpr auto ConstructionThread = currentThreadLike;
    static inline constexpr auto MainThread = mainThreadLike;
    static inline constexpr auto AnyThread = anyThreadLike;
};

// Wraps a Function to make sure it is always called once and only once.
// When Enforced=true, operator() returns a CompletionHandlerCalledToken instead of void,
// allowing callers to prove at compile time that the handler was invoked.
template <typename Out, typename... In, bool Enforced>
class CompletionHandler<Out(In...), Enforced> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CompletionHandler);
public:
    using OutType = Out;
    using InTypes = std::tuple<In...>;
    using Impl = typename Function<Out(In...)>::Impl;

    CompletionHandler() = default;

    template<typename CallableType>
        requires (std::is_rvalue_reference_v<CallableType&&>)
    CompletionHandler(CallableType&& callable, ThreadLikeAssertion callThread = CompletionHandlerCallThread::ConstructionThread)
        : m_function(std::forward<CallableType>(callable))
        , m_callThread(WTF::move(callThread))
    {
    }

#if defined(__APPLE__)
    // Always use C++ lambdas to create a WTF::CompletionHandler in Objective-C++.
    // Always use Swift closures (implicitly as Objective-C blocks) to create a WTF::CompletionHandler in Swift.
#ifndef __swift__
    CompletionHandler(Out (^block)(In... args), ThreadLikeAssertion = CompletionHandlerCallThread::ConstructionThread) = delete;
#else
    CompletionHandler(Out (^block)(In... args), ThreadLikeAssertion callThread = CompletionHandlerCallThread::ConstructionThread)
        : m_function(block)
        , m_callThread(WTF::move(callThread))
    {
    }
#endif
#endif // defined(__APPLE__)

    CompletionHandler(CompletionHandler&&) = default;
    CompletionHandler& operator=(CompletionHandler&&) = default;

    ~CompletionHandler()
    {
        ASSERT_WITH_MESSAGE(!m_function, "Completion handler should always be called");
        m_callThread = anyThreadLike;
    }

    explicit operator bool() const { return !!m_function; }

    [[nodiscard]] Impl* leak() { return m_function.leak(); }

    auto operator()(In... in) -> std::conditional_t<Enforced && std::is_void_v<Out>, CompletionHandlerCalledToken, Out>
    {
        assertIsCurrent(m_callThread);
        ASSERT_WITH_MESSAGE(m_function, "Completion handler should not be called more than once");
        if constexpr (Enforced && std::is_void_v<Out>) {
            std::exchange(m_function, nullptr)(std::forward<In>(in)...);
            return CompletionHandlerCalledToken { };
        } else
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
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CompletionHandlerWithFinalizer);
public:
    using OutType = Out;
    using InTypes = std::tuple<In...>;

    template<typename CallableType>
        requires (std::is_rvalue_reference_v<CallableType&&>)
    CompletionHandlerWithFinalizer(CallableType&& callable, Function<void(Function<Out(In...)>&)>&& finalizer, ThreadLikeAssertion callThread = CompletionHandlerCallThread::ConstructionThread)
        : m_function(std::forward<CallableType>(callable))
        , m_finalizer(WTF::move(finalizer))
        , m_callThread(callThread)
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

template<typename Out, typename... In, bool Enforced>
class CallableWrapper<CompletionHandler<Out(In...), Enforced>, Out, In...> : public CallableWrapperBase<Out, In...> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CallableWrapper);
public:
    explicit CallableWrapper(CompletionHandler<Out(In...), Enforced>&& completionHandler)
        : m_completionHandler(WTF::move(completionHandler))
    {
        RELEASE_ASSERT(m_completionHandler);
    }
    Out call(In... in) final
    {
        if constexpr (Enforced && std::is_void_v<Out>)
            m_completionHandler(std::forward<In>(in)...);
        else
            return m_completionHandler(std::forward<In>(in)...);
    }
private:
    CompletionHandler<Out(In...), Enforced> m_completionHandler;
};

} // namespace Detail

class CompletionHandlerCallingScope final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CompletionHandlerCallingScope);
public:
    CompletionHandlerCallingScope() = default;

    CompletionHandlerCallingScope(CompletionHandler<void()>&& completionHandler)
        : m_completionHandler(WTF::move(completionHandler))
    { }

    ~CompletionHandlerCallingScope()
    {
        if (m_completionHandler)
            m_completionHandler();
    }

    CompletionHandlerCallingScope(CompletionHandlerCallingScope&&) = default;
    CompletionHandlerCallingScope& operator=(CompletionHandlerCallingScope&&) = default;

    CompletionHandler<void()> release() { return WTF::move(m_completionHandler); }

private:
    CompletionHandler<void()> m_completionHandler;
};

template<typename Out, typename... In> CompletionHandler<Out(In...)> adopt(typename CompletionHandler<Out(In...)>::Impl* impl)
{
    return Function<Out(In...)>(impl, Function<Out(In...)>::Adopt);
}

template<typename Sig, typename Lambda>
CompletionHandlerCalledToken CompletionHandlerCalledToken::defer(CompletionHandler<Sig, true>&& handler, Lambda&& lambda)
{
    static_assert(
        std::is_invocable_r_v<CompletionHandlerCalledToken, Lambda, CompletionHandler<Sig, true>&&>,
        "Lambda passed to CompletionHandlerCalledToken::defer must accept CompletionHandler<Sig, true>&& "
        "and return CompletionHandlerCalledToken on all code paths by actually calling the handler");
    ASSERT(handler);
    return std::forward<Lambda>(lambda)(WTF::move(handler));
}

template<typename Sig, typename Lambda>
CompletionHandlerCalledToken CompletionHandlerCalledToken::deferUnchecked(CompletionHandler<Sig, true>& handler, Lambda&& lambda)
{
    static_assert(
        std::is_invocable_r_v<CompletionHandlerCalledToken, Lambda, CompletionHandler<Sig, true>&, CompletionHandlerCalledToken&&>,
        "Lambda passed to CompletionHandlerCalledToken::deferUnchecked must accept (CompletionHandler<Sig, true>&, CompletionHandlerCalledToken&&) and return CompletionHandlerCalledToken on all code paths");
    ASSERT(handler);
    auto result = std::forward<Lambda>(lambda)(handler, CompletionHandlerCalledToken { });
    RELEASE_ASSERT(!handler, "CompletionHandler was not consumed (moved or called) inside deferUnchecked()");
    return result;
}

} // namespace WTF

using WTF::CompletionHandler;
using WTF::CompletionHandlerCallThread;
using WTF::CompletionHandlerCalledToken;
using WTF::CompletionHandlerCallingScope;
using WTF::CompletionHandlerWithFinalizer;
using WTF::IsCompletionHandlerCalledToken;
