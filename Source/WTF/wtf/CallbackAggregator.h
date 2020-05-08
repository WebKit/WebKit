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
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

class CallbackAggregator : public ThreadSafeRefCounted<CallbackAggregator> {
public:
    static Ref<CallbackAggregator> create(CompletionHandler<void()>&& callback) { return adoptRef(*new CallbackAggregator(WTFMove(callback))); }

    ~CallbackAggregator()
    {
        ASSERT(m_wasConstructedOnMainThread == isMainThread());
        if (m_callback)
            m_callback();
    }

private:
    explicit CallbackAggregator(CompletionHandler<void()>&& callback)
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

} // namespace WTF

using WTF::CallbackAggregator;
