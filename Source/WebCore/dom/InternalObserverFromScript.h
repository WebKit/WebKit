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

#include "InternalObserver.h"
#include "VoidCallback.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class ScriptExecutionContext;
class JSSubscriptionObserverCallback;
class SubscriptionObserverCallback;
struct SubscriptionObserver;

class InternalObserverFromScript final : public InternalObserver {
public:
    static Ref<InternalObserverFromScript> create(ScriptExecutionContext&, RefPtr<JSSubscriptionObserverCallback>);
    static Ref<InternalObserverFromScript> create(ScriptExecutionContext&, SubscriptionObserver&);

    explicit InternalObserverFromScript(ScriptExecutionContext&, RefPtr<JSSubscriptionObserverCallback>);
    explicit InternalObserverFromScript(ScriptExecutionContext&, SubscriptionObserver&);

    void next(JSC::JSValue) final;
    void error(JSC::JSValue) final;
    void complete() final;

    void visitAdditionalChildren(JSC::AbstractSlotVisitor&) const final;
    void visitAdditionalChildren(JSC::SlotVisitor&) const final;

protected:
    // ActiveDOMObject
    void stop() final
    {
        m_next = nullptr;
        m_error = nullptr;
        m_complete = nullptr;
    }

private:
    RefPtr<SubscriptionObserverCallback> m_next;
    RefPtr<SubscriptionObserverCallback> m_error;
    RefPtr<VoidCallback> m_complete;
};

} // namespace WebCore
