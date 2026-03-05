/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/SwiftBridging.h>

namespace WTF {

namespace Detail {

class CallableWrapperAllocatorBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CallableWrapperAllocatorBase);
public:
    virtual ~CallableWrapperAllocatorBase() { }
};

template<typename Out, typename... In>
class CallableWrapperBase : public CallableWrapperAllocatorBase {
public:
    virtual ~CallableWrapperBase() { }
    virtual Out call(In...) = 0;
};

template<typename, typename, typename...> class CallableWrapper;

template<typename CallableType, typename Out, typename... In>
class CallableWrapper : public CallableWrapperBase<Out, In...> {
public:
    explicit CallableWrapper(CallableType&& callable)
        : m_callable(WTF::move(callable)) { }
    CallableWrapper(const CallableWrapper&) = delete;
    CallableWrapper& operator=(const CallableWrapper&) = delete;
    Out call(In... in) final { return m_callable(std::forward<In>(in)...); }
private:
    CallableType m_callable;
};

} // namespace Detail

template<typename Out, typename... In> Function<Out(In...)> adopt(Detail::CallableWrapperBase<Out, In...>*);

template <typename Out, typename... In>
class Function<Out(In...)> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Function);
public:
    using Impl = Detail::CallableWrapperBase<Out, In...>;

    Function() = default;
    Function(std::nullptr_t) { }

    template<typename CallableType>
        requires (!(std::is_pointer_v<CallableType> && std::is_function_v<typename std::remove_pointer_t<CallableType>>) && std::is_rvalue_reference_v<CallableType&&>)
    Function(CallableType&& callable)
        : m_callableWrapper(makeUnique<Detail::CallableWrapper<CallableType, Out, In...>>(std::forward<CallableType>(callable))) { }

    template<typename FunctionType>
        requires (std::is_pointer_v<FunctionType> && std::is_function_v<typename std::remove_pointer_t<FunctionType>>)
    Function(FunctionType f)
        : m_callableWrapper(makeUnique<Detail::CallableWrapper<FunctionType, Out, In...>>(std::forward<FunctionType>(f))) { }

#if defined(__APPLE__)
    // Always use C++ lambdas to create a WTF::Function in Objective-C++.
    // Always use Swift closures (implicitly as Objective-C blocks) to create a WTF::Function in Swift.
#ifndef __swift__
    Function(Out (^block)(In... args)) = delete;
#else
    Function(Out (^block)(In... args))
        : m_callableWrapper(makeUnique<Detail::CallableWrapper<Out (^)(In...), Out, In...>>(std::forward<Out (^)(In...)>(block)))
    {
    }
#endif
#endif // defined(__APPLE__)

    Out operator()(In... in) const
    {
        ASSERT(m_callableWrapper);
        return m_callableWrapper->call(std::forward<In>(in)...);
    }

    explicit operator bool() const { return !!m_callableWrapper; }

    template<typename CallableType>
        requires (!(std::is_pointer_v<CallableType> && std::is_function_v<typename std::remove_pointer_t<CallableType>>) && std::is_rvalue_reference_v<CallableType&&>)
    Function& operator=(CallableType&& callable)
    {
        m_callableWrapper = makeUnique<Detail::CallableWrapper<CallableType, Out, In...>>(std::forward<CallableType>(callable));
        return *this;
    }

    template<typename FunctionType>
        requires (std::is_pointer_v<FunctionType> && std::is_function_v<typename std::remove_pointer_t<FunctionType>>)
    Function& operator=(FunctionType f)
    {
        m_callableWrapper = makeUnique<Detail::CallableWrapper<FunctionType, Out, In...>>(std::forward<FunctionType>(f));
        return *this;
    }

    Function& operator=(std::nullptr_t)
    {
        m_callableWrapper = nullptr;
        return *this;
    }

    [[nodiscard]] Impl* leak()
    {
        return m_callableWrapper.release();
    }

private:
    enum AdoptTag { Adopt };
    Function(Impl* impl, AdoptTag)
        : m_callableWrapper(impl)
    {
    }

    friend Function adopt<Out, In...>(Impl*);

    std::unique_ptr<Impl> m_callableWrapper;
} SWIFT_ESCAPABLE;

template<typename Out, typename... In> Function<Out(In...)> adopt(Detail::CallableWrapperBase<Out, In...>* impl)
{
    return Function<Out(In...)>(impl, Function<Out(In...)>::Adopt);
}

} // namespace WTF
