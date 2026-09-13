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

#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#include <wtf/AlignedStorage.h>
#include <wtf/Compiler.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ThreadAssertions.h>

namespace WTF {

// The C ABI a Swift closure reduces to, used by the `CxxCompletionHandler` Swift protocol to build a
// CompletionHandler out of a Swift closure.
using SwiftClosureInvoke = void (* WTF_NONNULL)(void* WTF_NONNULL context, void* WTF_NULLABLE argument);
using SwiftClosureDestroy = void (* WTF_NONNULL)(void* WTF_NONNULL context);

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
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CompletionHandler);
IGNORE_CLANG_WARNINGS_END
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
    CompletionHandler(Out (^ WTF_NULLABLE block)(In... args), ThreadLikeAssertion = CompletionHandlerCallThread::ConstructionThread) = delete;
#endif // defined(__APPLE__)

    CompletionHandler(CompletionHandler&&) = default;
    CompletionHandler& operator=(CompletionHandler&&) = default;

    ~CompletionHandler()
    {
        ASSERT_WITH_MESSAGE(!m_function, "Completion handler should always be called");
        m_callThread = anyThreadLike;
    }

#ifdef __swift__
    // This should only be called within the initializers of the `CxxCompletionHandler` protocols.
    CompletionHandler(SwiftClosureInvoke invoke, SwiftClosureDestroy destroy, void* WTF_NONNULL context) SWIFT_NAME(init(invoke:destroy:context:))
        : CompletionHandler([invoke, owner = std::unique_ptr<void, SwiftClosureDestroy> { context, destroy }](In... arguments) -> Out {
            passArgumentToSwift(invoke, owner.get(), std::forward<In>(arguments)...);
        })
    {
        static_assert(std::is_void_v<Out>, "Swift closures can only be used with completion handlers that return void.");
        static_assert(sizeof...(In) <= 1, "Swift closures can only be used with completion handlers that take at most one argument.");
    }
#endif

    explicit operator bool() const { return !!m_function; }

    [[nodiscard]] Impl* WTF_NULLABLE leak() { return m_function.leak(); }

    Out operator()(In... in)
    {
        assertIsCurrent(m_callThread);
        ASSERT_WITH_MESSAGE(m_function, "Completion handler should not be called more than once");
        return std::exchange(m_function, nullptr)(std::forward<In>(in)...);
    }

private:
#ifdef __swift__
    static void passArgumentToSwift(SwiftClosureInvoke invoke, void* WTF_NONNULL context, In... arguments)
    {
        if constexpr (!sizeof...(In))
            invoke(context, nullptr);
        else {
            using DeclaredArgument = std::tuple_element_t<0, std::tuple<In...>>;
            using ArgumentType = std::remove_cvref_t<DeclaredArgument>;

            static_assert(!std::is_lvalue_reference_v<DeclaredArgument>);

            if constexpr (std::is_copy_constructible_v<ArgumentType>) {
                static_assert(!std::is_rvalue_reference_v<DeclaredArgument>);

                invoke(context, std::addressof(arguments)...);
            } else {
                static_assert(std::is_rvalue_reference_v<DeclaredArgument>);
                static_assert(!std::is_trivially_destructible_v<ArgumentType>);

                AlignedStorage<ArgumentType> storage;
                new (storage.get()) ArgumentType(std::forward<In>(arguments)...);
                invoke(context, storage.get());
            }
        }
    }
#endif

    Function<Out(In...)> m_function;
    NO_UNIQUE_ADDRESS ThreadLikeAssertion m_callThread;
};

// Wraps a Function to make sure it is called at most once.
// If the CompletionHandlerWithFinalizer is destroyed and the function hasn't yet been called,
// the finalizer is invoked with the function as its argument.
template<typename> class CompletionHandlerWithFinalizer;
template <typename Out, typename... In>
class CompletionHandlerWithFinalizer<Out(In...)> {
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CompletionHandlerWithFinalizer);
IGNORE_CLANG_WARNINGS_END
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

template<typename Out, typename... In>
class CallableWrapper<CompletionHandler<Out(In...)>, Out, In...> : public CallableWrapperBase<Out, In...> {
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CallableWrapper);
IGNORE_CLANG_WARNINGS_END
public:
    explicit CallableWrapper(CompletionHandler<Out(In...)>&& completionHandler)
        : m_completionHandler(WTF::move(completionHandler))
    {
        RELEASE_ASSERT(m_completionHandler);
    }
    Out call(In... in) final { return m_completionHandler(std::forward<In>(in)...); }
private:
    CompletionHandler<Out(In...)> m_completionHandler;
};

} // namespace Detail

class CompletionHandlerCallingScope final {
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CompletionHandlerCallingScope);
IGNORE_CLANG_WARNINGS_END
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

    explicit operator bool() const { return !!m_completionHandler; }

private:
    CompletionHandler<void()> m_completionHandler;
};

template<typename Out, typename... In> CompletionHandler<Out(In...)> adopt(typename CompletionHandler<Out(In...)>::Impl* WTF_NONNULL impl)
{
    return Function<Out(In...)>(impl, Function<Out(In...)>::Adopt);
}

using VoidCompletionHandler = CompletionHandler<void()>;
using BoolCompletionHandler = CompletionHandler<void(bool)>;

} // namespace WTF

using WTF::CompletionHandler;
using WTF::CompletionHandlerCallThread;
using WTF::CompletionHandlerCallingScope;
using WTF::CompletionHandlerWithFinalizer;
