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

#include "Function.h"

namespace WTF {

template<typename> class CompletionHandler;

// Wraps a WTF::Function to make sure it is always called once and only once.
template <typename Out, typename... In>
class CompletionHandler<Out(In...)> {
public:
    CompletionHandler() = default;

    template<typename CallableType, class = typename std::enable_if<std::is_rvalue_reference<CallableType&&>::value>::type>
    CompletionHandler(CallableType&& callable)
        : m_function(WTFMove(callable))
    {
    }

    CompletionHandler(CompletionHandler&&) = default;
    CompletionHandler& operator=(CompletionHandler&&) = default;

    ~CompletionHandler()
    {
        ASSERT_WITH_MESSAGE(!m_function, "Completion handler should always be called");
    }

    explicit operator bool() const { return !!m_function; }

    Out operator()(In... in) const
    {
        ASSERT_WITH_MESSAGE(m_function, "Completion handler should not be called more than once");
        auto function = WTFMove(m_function);
        return function(std::forward<In>(in)...);
    }

private:
    mutable WTF::Function<Out(In...)> m_function;
};

class CompletionHandlerCallingScope {
public:
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
using WTF::CompletionHandlerCallingScope;
