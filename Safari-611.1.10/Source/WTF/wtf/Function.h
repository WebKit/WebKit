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

namespace WTF {

namespace Detail {

template<typename Out, typename... In>
class CallableWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~CallableWrapperBase() { }
    virtual Out call(In...) = 0;
};

template<typename, typename, typename...> class CallableWrapper;

template<typename CallableType, typename Out, typename... In>
class CallableWrapper : public CallableWrapperBase<Out, In...> {
public:
    explicit CallableWrapper(CallableType&& callable)
        : m_callable(WTFMove(callable)) { }
    CallableWrapper(const CallableWrapper&) = delete;
    CallableWrapper& operator=(const CallableWrapper&) = delete;
    Out call(In... in) final { return m_callable(std::forward<In>(in)...); }
private:
    CallableType m_callable;
};

} // namespace Detail

template<typename> class Function;

template<typename Out, typename... In> Function<Out(In...)> adopt(Detail::CallableWrapperBase<Out, In...>*);

template <typename Out, typename... In>
class Function<Out(In...)> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Impl = Detail::CallableWrapperBase<Out, In...>;

    Function() = default;
    Function(std::nullptr_t) { }

    template<typename CallableType, class = typename std::enable_if<!(std::is_pointer<CallableType>::value && std::is_function<typename std::remove_pointer<CallableType>::type>::value) && std::is_rvalue_reference<CallableType&&>::value>::type>
    Function(CallableType&& callable)
        : m_callableWrapper(makeUnique<Detail::CallableWrapper<CallableType, Out, In...>>(WTFMove(callable))) { }

    template<typename FunctionType, class = typename std::enable_if<std::is_pointer<FunctionType>::value && std::is_function<typename std::remove_pointer<FunctionType>::type>::value>::type>
    Function(FunctionType f)
        : m_callableWrapper(makeUnique<Detail::CallableWrapper<FunctionType, Out, In...>>(WTFMove(f))) { }

    Out operator()(In... in) const
    {
        ASSERT(m_callableWrapper);
        return m_callableWrapper->call(std::forward<In>(in)...);
    }

    explicit operator bool() const { return !!m_callableWrapper; }

    template<typename CallableType, class = typename std::enable_if<!(std::is_pointer<CallableType>::value && std::is_function<typename std::remove_pointer<CallableType>::type>::value) && std::is_rvalue_reference<CallableType&&>::value>::type>
    Function& operator=(CallableType&& callable)
    {
        m_callableWrapper = makeUnique<Detail::CallableWrapper<CallableType, Out, In...>>(WTFMove(callable));
        return *this;
    }

    template<typename FunctionType, class = typename std::enable_if<std::is_pointer<FunctionType>::value && std::is_function<typename std::remove_pointer<FunctionType>::type>::value>::type>
    Function& operator=(FunctionType f)
    {
        m_callableWrapper = makeUnique<Detail::CallableWrapper<FunctionType, Out, In...>>(WTFMove(f));
        return *this;
    }

    Function& operator=(std::nullptr_t)
    {
        m_callableWrapper = nullptr;
        return *this;
    }

    Impl* leak()
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
};

template<typename Out, typename... In> Function<Out(In...)> adopt(Detail::CallableWrapperBase<Out, In...>* impl)
{
    return Function<Out(In...)>(impl, Function<Out(In...)>::Adopt);
}

} // namespace WTF

using WTF::Function;
