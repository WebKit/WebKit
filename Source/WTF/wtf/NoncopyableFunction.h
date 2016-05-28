/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

// FIXME: We could make this templated to support other lambdas than void() and make this more reusable.
class NoncopyableFunction {
public:
    NoncopyableFunction() = default;

    template<typename CallableType, class = typename std::enable_if<std::is_rvalue_reference<CallableType&&>::value>::type>
    NoncopyableFunction(CallableType&& callable)
        : m_callableWrapper(std::make_unique<CallableWrapper<CallableType>>(WTFMove(callable)))
    {
    }

    void operator()() const
    {
        if (m_callableWrapper)
            m_callableWrapper->call();
    }

    explicit operator bool() const { return !!m_callableWrapper; }

    template<typename CallableType, class = typename std::enable_if<std::is_rvalue_reference<CallableType&&>::value>::type>
    NoncopyableFunction& operator=(CallableType&& callable)
    {
        m_callableWrapper = std::make_unique<CallableWrapper<CallableType>>(WTFMove(callable));
        return *this;
    }

    NoncopyableFunction& operator=(std::nullptr_t)
    {
        m_callableWrapper = nullptr;
        return *this;
    }

private:
    class CallableWrapperBase {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~CallableWrapperBase() { }

        virtual void call() = 0;
    };

    template<typename CallableType>
    class CallableWrapper final : public CallableWrapperBase {
    public:
        explicit CallableWrapper(CallableType&& callable)
            : m_callable(WTFMove(callable))
        {
        }

        CallableWrapper(const CallableWrapper&) = delete;
        CallableWrapper& operator=(const CallableWrapper&) = delete;

        void call() final { m_callable(); }

    private:
        CallableType m_callable;
    };

    std::unique_ptr<CallableWrapperBase> m_callableWrapper;
};

} // namespace WTF

using WTF::NoncopyableFunction;
