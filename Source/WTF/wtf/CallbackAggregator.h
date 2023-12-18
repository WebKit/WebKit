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

#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

template <DestructionThread destructionThread>
class CallbackAggregatorOnThread : public ThreadSafeRefCounted<CallbackAggregatorOnThread<destructionThread>, destructionThread> {
public:
    static auto create(CompletionHandler<void()>&& callback) { return adoptRef(*new CallbackAggregatorOnThread(WTFMove(callback))); }

    ~CallbackAggregatorOnThread()
    {
        ASSERT(m_wasConstructedOnMainThread == isMainThread());
        if (m_callback)
            m_callback();
    }

private:
    explicit CallbackAggregatorOnThread(CompletionHandler<void()>&& callback)
        : m_callback(WTFMove(callback))
#if ASSERT_ENABLED
        , m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    CompletionHandler<void()> m_callback;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

using CallbackAggregator = CallbackAggregatorOnThread<DestructionThread::Any>;
using MainRunLoopCallbackAggregator = CallbackAggregatorOnThread<DestructionThread::MainRunLoop>;

// EagerCallbackAggregator ensures a callback is executed at its first opportunity, ignoring subsequent triggers.
// It's particularly useful in situations where a callback might be triggered multiple times, but only the first
// execution is necessary. If the callback hasn't been executed by the time of the aggregator's destruction,
// it's automatically called with default parameters, ensuring a single, definitive execution.

template<typename> class EagerCallbackAggregator;
template <typename Out, typename... In>
class EagerCallbackAggregator<Out(In...)> : public RefCounted<EagerCallbackAggregator<Out(In...)>> {
public:
    template<typename CallableType, class = typename std::enable_if<std::is_rvalue_reference<CallableType&&>::value>::type>
    static Ref<EagerCallbackAggregator> create(CallableType&& callback, In... defaultArgs)
    {
        return adoptRef(*new EagerCallbackAggregator(std::forward<CallableType>(callback), std::forward<In>(defaultArgs)...));
    }

    Out operator()(In... args)
    {
        if (m_callback)
            return m_callback(std::forward<In>(args)...);
        return Out();
    }

    ~EagerCallbackAggregator()
    {
        if (m_callback)
            std::apply(m_callback, m_defaultArgs);
    }

private:
    template<typename CallableType>
    explicit EagerCallbackAggregator(CallableType&& callback, In... defaultArgs)
        : m_callback(std::forward<CallableType>(callback))
        , m_defaultArgs(std::forward<In>(defaultArgs)...)
    {
    }

    CompletionHandler<Out(In...)> m_callback;
    std::tuple<In...> m_defaultArgs;
};

} // namespace WTF

using WTF::CallbackAggregator;
using WTF::MainRunLoopCallbackAggregator;
using WTF::EagerCallbackAggregator;
