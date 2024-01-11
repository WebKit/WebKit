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
#include "ServiceWorker.h"

#include "Document.h"
#include "EventNames.h"
#include "Logging.h"
#include "MessagePort.h"
#include "SWClientConnection.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerThread.h"
#include "StructuredSerializeOptions.h"
#include "WorkerSWClientConnection.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/EnumTraits.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

#define WORKER_RELEASE_LOG(fmt, ...) RELEASE_LOG(ServiceWorker, "%p - ServiceWorker::" fmt, this, ##__VA_ARGS__)
#define WORKER_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ServiceWorker, "%p - ServiceWorker::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ServiceWorker);

Ref<ServiceWorker> ServiceWorker::getOrCreate(ScriptExecutionContext& context, ServiceWorkerData&& data)
{
    if (auto existingServiceWorker = context.serviceWorker(data.identifier))
        return *existingServiceWorker;
    auto serviceWorker = adoptRef(*new ServiceWorker(context, WTFMove(data)));
    serviceWorker->suspendIfNeeded();
    return serviceWorker;
}

ServiceWorker::ServiceWorker(ScriptExecutionContext& context, ServiceWorkerData&& data)
    : ActiveDOMObject(&context)
    , m_data(WTFMove(data))
{
    context.registerServiceWorker(*this);

    relaxAdoptionRequirement();
    updatePendingActivityForEventDispatch();

    WORKER_RELEASE_LOG("serviceWorkerID=%llu, state=%hhu", identifier().toUInt64(), enumToUnderlyingType(m_data.state));
}

ServiceWorker::~ServiceWorker()
{
    if (auto* context = scriptExecutionContext())
        context->unregisterServiceWorker(*this);
}

void ServiceWorker::updateState(State state)
{
    WORKER_RELEASE_LOG("updateState: Updating service worker %llu state from %hhu to %hhu. registrationID=%llu", identifier().toUInt64(), enumToUnderlyingType(m_data.state), enumToUnderlyingType(state), registrationIdentifier().toUInt64());
    m_data.state = state;
    if (state != State::Installing && !m_isStopped) {
        ASSERT(m_pendingActivityForEventDispatch);
        dispatchEvent(Event::create(eventNames().statechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }

    updatePendingActivityForEventDispatch();
}

SWClientConnection& ServiceWorker::swConnection()
{
    ASSERT(scriptExecutionContext());
    if (is<WorkerGlobalScope>(scriptExecutionContext()))
        return downcast<WorkerGlobalScope>(scriptExecutionContext())->swClientConnection();
    return ServiceWorkerProvider::singleton().serviceWorkerConnection();
}

ExceptionOr<void> ServiceWorker::postMessage(JSC::JSGlobalObject& globalObject, JSC::JSValue messageValue, StructuredSerializeOptions&& options)
{
    if (m_isStopped)
        return Exception { ExceptionCode::InvalidStateError };

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(globalObject, messageValue, WTFMove(options.transfer), ports, SerializationForStorage::No, SerializationContext::WorkerPostMessage);
    if (messageData.hasException())
        return messageData.releaseException();

    // Disentangle the port in preparation for sending it to the remote context.
    auto portsOrException = MessagePort::disentanglePorts(WTFMove(ports));
    if (portsOrException.hasException())
        return portsOrException.releaseException();

    auto& context = *scriptExecutionContext();
    // FIXME: Maybe we could use a ScriptExecutionContextIdentifier for service workers too.
    ServiceWorkerOrClientIdentifier sourceIdentifier;
    if (is<ServiceWorkerGlobalScope>(context))
        sourceIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();
    else
        sourceIdentifier = context.identifier();

    MessageWithMessagePorts message { messageData.releaseReturnValue(), portsOrException.releaseReturnValue() };
    swConnection().postMessageToServiceWorker(identifier(), WTFMove(message), sourceIdentifier);
    return { };
}

EventTargetInterface ServiceWorker::eventTargetInterface() const
{
    return ServiceWorkerEventTargetInterfaceType;
}

ScriptExecutionContext* ServiceWorker::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

const char* ServiceWorker::activeDOMObjectName() const
{
    return "ServiceWorker";
}

void ServiceWorker::stop()
{
    m_isStopped = true;
    removeAllEventListeners();
    scriptExecutionContext()->unregisterServiceWorker(*this);
    updatePendingActivityForEventDispatch();
}

void ServiceWorker::updatePendingActivityForEventDispatch()
{
    // ServiceWorkers can dispatch events until they become redundant or they are stopped.
    if (m_isStopped || state() == State::Redundant) {
        m_pendingActivityForEventDispatch = nullptr;
        return;
    }
    if (m_pendingActivityForEventDispatch)
        return;
    m_pendingActivityForEventDispatch = makePendingActivity(*this);
}

} // namespace WebCore
