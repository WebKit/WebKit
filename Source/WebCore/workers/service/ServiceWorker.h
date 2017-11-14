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

#if ENABLE(SERVICE_WORKER)

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ServiceWorkerData.h"
#include "URL.h"
#include <heap/Strong.h>
#include <wtf/RefCounted.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class Frame;

class ServiceWorker final : public RefCounted<ServiceWorker>, public EventTargetWithInlineData, public ContextDestructionObserver {
public:
    using State = ServiceWorkerState;
    static Ref<ServiceWorker> getOrCreate(ScriptExecutionContext&, ServiceWorkerData&&);

    virtual ~ServiceWorker();

    const URL& scriptURL() const { return m_data.scriptURL; }

    State state() const { return m_data.state; }
    
    void scheduleTaskToUpdateState(State);

    ExceptionOr<void> postMessage(ScriptExecutionContext&, JSC::JSValue message, Vector<JSC::Strong<JSC::JSObject>>&&);

    ServiceWorkerIdentifier identifier() const { return m_data.identifier; }

    using RefCounted::ref;
    using RefCounted::deref;

    static const HashMap<ServiceWorkerIdentifier, HashSet<ServiceWorker*>>& allWorkers();

private:
    ServiceWorker(ScriptExecutionContext&, ServiceWorkerData&&);
    static HashMap<ServiceWorkerIdentifier, HashSet<ServiceWorker*>>& mutableAllWorkers();

    EventTargetInterface eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    ServiceWorkerData m_data;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
