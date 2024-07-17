/*
 * Copyright (C) 2024 Keith Cirkel <webkit@keithcirkel.co.uk>. All rights reserved.
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

#include "AbortController.h"
#include "ActiveDOMObject.h"
#include "ScriptExecutionContext.h"
#include "ScriptWrappable.h"
#include "SubscriptionObserverCallback.h"
#include "VoidCallback.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class Subscriber final : public ActiveDOMObject, public ScriptWrappable, public RefCounted<Subscriber> {
    WTF_MAKE_ISO_ALLOCATED(Subscriber);

public:
    void next(JSC::JSValue);
    void complete();
    void error(JSC::JSValue);
    void addTeardown(Ref<VoidCallback>);

    bool active() { return m_active; }
    AbortSignal& signal() { return m_abortController->signal(); }

    static Ref<Subscriber> create(ScriptExecutionContext&);

    static Ref<Subscriber> create(ScriptExecutionContext&, RefPtr<SubscriptionObserverCallback> next);

    static Ref<Subscriber> create(ScriptExecutionContext&, RefPtr<SubscriptionObserverCallback> next,
        RefPtr<SubscriptionObserverCallback> error,
        RefPtr<VoidCallback>);

    explicit Subscriber(ScriptExecutionContext&, RefPtr<SubscriptionObserverCallback> next,
        RefPtr<SubscriptionObserverCallback> error,
        RefPtr<VoidCallback>);

    void followSignal(AbortSignal&);

    // JSCustomMarkFunction; for JSSubscriberCustom
    SubscriptionObserverCallback* nextCallbackConcurrently() { return m_next.get(); }
    SubscriptionObserverCallback* errorCallbackConcurrently() { return m_error.get(); }
    VoidCallback* completeCallbackConcurrently() { return m_complete.get(); }
    Vector<VoidCallback*> teardownCallbacksConcurrently()
    {
        Locker locker { m_teardownsLock };
        return m_teardowns.map([](auto& callback) { return callback.ptr(); });
    }

    // ActiveDOMObject
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    bool m_active = true;
    Lock m_teardownsLock;
    Ref<AbortController> m_abortController;
    RefPtr<SubscriptionObserverCallback> m_next;
    RefPtr<SubscriptionObserverCallback> m_error;
    RefPtr<VoidCallback> m_complete;
    Vector<Ref<VoidCallback>> m_teardowns WTF_GUARDED_BY_LOCK(m_teardownsLock);

    void close(JSC::JSValue);

    bool isActive() const
    {
        return m_active && !isInactiveDocument();
    }

    bool isInactiveDocument() const;

    void reportErrorObject(JSC::JSValue);

    // ActiveDOMObject
    void stop() final
    {
        m_active = false;
        m_next = nullptr;
        m_error = nullptr;
        m_complete = nullptr;
        Locker locker { m_teardownsLock };
        m_teardowns.clear();
    }
    bool virtualHasPendingActivity() const final { return m_active; }
};

} // namespace WebCore
