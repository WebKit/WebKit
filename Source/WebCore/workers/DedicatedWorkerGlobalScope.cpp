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

#include "config.h"
#include "DedicatedWorkerGlobalScope.h"

#include "ContentSecurityPolicyResponseHeaders.h"
#include "DOMWindow.h"
#include "DedicatedWorkerThread.h"
#include "MessageEvent.h"
#include "SecurityOrigin.h"
#include "WorkerObjectProxy.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DedicatedWorkerGlobalScope);

Ref<DedicatedWorkerGlobalScope> DedicatedWorkerGlobalScope::create(const URL& url, Ref<SecurityOrigin>&& origin, const String& name, const String& identifier, const String& userAgent, bool isOnline, DedicatedWorkerThread& thread, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, PAL::SessionID sessionID)
{
    auto context = adoptRef(*new DedicatedWorkerGlobalScope(url, WTFMove(origin), name, identifier, userAgent, isOnline, thread, shouldBypassMainWorldContentSecurityPolicy, WTFMove(topOrigin), timeOrigin, connectionProxy, socketProvider, sessionID));
    if (!shouldBypassMainWorldContentSecurityPolicy)
        context->applyContentSecurityPolicyResponseHeaders(contentSecurityPolicyResponseHeaders);
    return context;
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(const URL& url, Ref<SecurityOrigin>&& origin, const String& name, const String& identifier, const String& userAgent, bool isOnline, DedicatedWorkerThread& thread, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, PAL::SessionID sessionID)
    : WorkerGlobalScope(url, WTFMove(origin), identifier, userAgent, isOnline, thread, shouldBypassMainWorldContentSecurityPolicy, WTFMove(topOrigin), timeOrigin, connectionProxy, socketProvider, sessionID)
    , m_name(name)
{
}

DedicatedWorkerGlobalScope::~DedicatedWorkerGlobalScope() = default;

EventTargetInterface DedicatedWorkerGlobalScope::eventTargetInterface() const
{
    return DedicatedWorkerGlobalScopeEventTargetInterfaceType;
}

ExceptionOr<void> DedicatedWorkerGlobalScope::postMessage(JSC::ExecState& state, JSC::JSValue messageValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    Vector<RefPtr<MessagePort>> ports;
    auto message = SerializedScriptValue::create(state, messageValue, WTFMove(transfer), ports, SerializationContext::WorkerPostMessage);
    if (message.hasException())
        return message.releaseException();

    // Disentangle the port in preparation for sending it to the remote context.
    auto channels = MessagePort::disentanglePorts(WTFMove(ports));
    if (channels.hasException())
        return channels.releaseException();

    thread().workerObjectProxy().postMessageToWorkerObject({ message.releaseReturnValue(), channels.releaseReturnValue() });
    return { };
}

ExceptionOr<void> DedicatedWorkerGlobalScope::importScripts(const Vector<String>& urls)
{
    auto result = Base::importScripts(urls);
    thread().workerObjectProxy().reportPendingActivity(hasPendingActivity());
    return result;
}

DedicatedWorkerThread& DedicatedWorkerGlobalScope::thread()
{
    return static_cast<DedicatedWorkerThread&>(Base::thread());
}

} // namespace WebCore
