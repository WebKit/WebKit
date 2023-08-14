/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "IdleRequestCallback.h"
#include <wtf/Deque.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class WeakPtrImplWithEventTargetData;

class IdleCallbackController : public CanMakeWeakPtr<IdleCallbackController> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    IdleCallbackController(Document&);

    int queueIdleCallback(Ref<IdleRequestCallback>&&, Seconds timeout);
    void removeIdleCallback(int);

    void startIdlePeriod();
    bool isEmpty() const { return m_idleRequestCallbacks.isEmpty() && m_runnableIdleCallbacks.isEmpty(); }

private:
    void queueTaskToStartIdlePeriod();
    void queueTaskToInvokeIdleCallbacks();
    void invokeIdleCallbacks();
    void invokeIdleCallbackTimeout(unsigned identifier);

    unsigned m_idleCallbackIdentifier { 0 };

    struct IdleRequest {
        unsigned identifier { 0 };
        Ref<IdleRequestCallback> callback;
    };

    Deque<IdleRequest> m_idleRequestCallbacks;
    Deque<IdleRequest> m_runnableIdleCallbacks;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
};

} // namespace WebCore
