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

#include "config.h"

#if ENABLE(SERVICE_WORKER)
#include "ServiceWorkerClient.h"

#include "MessagePort.h"
#include "SWContextManager.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThread.h"
#include "ServiceWorkerWindowClient.h"

namespace WebCore {

Ref<ServiceWorkerClient> ServiceWorkerClient::getOrCreate(ServiceWorkerGlobalScope& context, ServiceWorkerClientData&& data)
{
    if (auto* client = context.serviceWorkerClient(data.identifier))
        return *client;

    if (data.type == ServiceWorkerClientType::Window)
        return ServiceWorkerWindowClient::create(context, WTFMove(data));

    return adoptRef(*new ServiceWorkerClient { context, WTFMove(data) });
}

ServiceWorkerClient::ServiceWorkerClient(ServiceWorkerGlobalScope& context, ServiceWorkerClientData&& data)
    : ContextDestructionObserver(&context)
    , m_data(WTFMove(data))
{
    context.addServiceWorkerClient(*this);
}

ServiceWorkerClient::~ServiceWorkerClient()
{
    if (auto* context = scriptExecutionContext())
        downcast<ServiceWorkerGlobalScope>(*context).removeServiceWorkerClient(*this);
}

const URL& ServiceWorkerClient::url() const
{
    return m_data.url;
}

auto ServiceWorkerClient::type() const -> Type
{
    return m_data.type;
}

auto ServiceWorkerClient::frameType() const -> FrameType
{
    return m_data.frameType;
}

String ServiceWorkerClient::id() const
{
    return identifier().toString();
}

ExceptionOr<void> ServiceWorkerClient::postMessage(ScriptExecutionContext& context, JSC::JSValue messageValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    auto* execState = context.execState();
    ASSERT(execState);

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(*execState, messageValue, WTFMove(transfer), ports, SerializationContext::WorkerPostMessage);
    if (messageData.hasException())
        return messageData.releaseException();

    // Disentangle the port in preparation for sending it to the remote context.
    auto portsOrException = MessagePort::disentanglePorts(WTFMove(ports));
    if (portsOrException.hasException())
        return portsOrException.releaseException();

    MessageWithMessagePorts message = { messageData.releaseReturnValue(), portsOrException.releaseReturnValue() };
    auto sourceIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();
    callOnMainThread([message = WTFMove(message), destinationIdentifier = identifier(), sourceIdentifier, sourceOrigin = context.origin().isolatedCopy()] () mutable {
        if (auto* connection = SWContextManager::singleton().connection())
            connection->postMessageToServiceWorkerClient(destinationIdentifier, WTFMove(message), sourceIdentifier, sourceOrigin);
    });

    return { };
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
