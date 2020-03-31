/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "PostMessageOptions.h"
#include "WorkerGlobalScope.h"

namespace JSC {
class CallFrame;
class JSObject;
class JSValue;
}

namespace WebCore {

class ContentSecurityPolicyResponseHeaders;
class DedicatedWorkerThread;
class MessagePort;
class RequestAnimationFrameCallback;
class SerializedScriptValue;

#if ENABLE(OFFSCREEN_CANVAS)
class WorkerAnimationController;

using CallbackId = int;
#endif

class DedicatedWorkerGlobalScope final : public WorkerGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(DedicatedWorkerGlobalScope);
public:
    static Ref<DedicatedWorkerGlobalScope> create(const WorkerParameters&, Ref<SecurityOrigin>&&, DedicatedWorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*);
    virtual ~DedicatedWorkerGlobalScope();

    const String& name() const { return m_name; }

    ExceptionOr<void> postMessage(JSC::JSGlobalObject&, JSC::JSValue message, PostMessageOptions&&);

    DedicatedWorkerThread& thread();

#if ENABLE(OFFSCREEN_CANVAS)
    CallbackId requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    void cancelAnimationFrame(CallbackId);
#endif

private:
    using Base = WorkerGlobalScope;

    DedicatedWorkerGlobalScope(const WorkerParameters&, Ref<SecurityOrigin>&&, DedicatedWorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*);

    bool isDedicatedWorkerGlobalScope() const final { return true; }
    ExceptionOr<void> importScripts(const Vector<String>& urls) final;
    EventTargetInterface eventTargetInterface() const final;

    String m_name;

#if ENABLE(OFFSCREEN_CANVAS)
    RefPtr<WorkerAnimationController> m_workerAnimationController;
#endif
};

} // namespace WebCore
