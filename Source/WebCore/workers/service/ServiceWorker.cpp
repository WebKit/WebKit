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

#if ENABLE(SERVICE_WORKER)

#include "Document.h"
#include "EventNames.h"
#include "Logging.h"
#include "MessagePort.h"
#include "SWClientConnection.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerProvider.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/NeverDestroyed.h>

#define WORKER_RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorker::" fmt, this, ##__VA_ARGS__)
#define WORKER_RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorker::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

Ref<ServiceWorker> ServiceWorker::getOrCreate(ScriptExecutionContext& context, ServiceWorkerData&& data)
{
    if (auto existingServiceWorker = context.serviceWorker(data.identifier))
        return *existingServiceWorker;
    return adoptRef(*new ServiceWorker(context, WTFMove(data)));
}

ServiceWorker::ServiceWorker(ScriptExecutionContext& context, ServiceWorkerData&& data)
    : ActiveDOMObject(&context)
    , m_data(WTFMove(data))
{
    suspendIfNeeded();

    context.registerServiceWorker(*this);

    relaxAdoptionRequirement();
    updatePendingActivityForEventDispatch();

    WORKER_RELEASE_LOG_IF_ALLOWED("ServiceWorker: ID: %llu, state: %hhu", identifier().toUInt64(), m_data.state);
}

ServiceWorker::~ServiceWorker()
{
    if (auto* context = scriptExecutionContext())
        context->unregisterServiceWorker(*this);
}

void ServiceWorker::scheduleTaskToUpdateState(State state)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return;

    context->postTask([this, protectedThis = makeRef(*this), state](ScriptExecutionContext&) {
        ASSERT(this->state() != state);

        WORKER_RELEASE_LOG_IF_ALLOWED("scheduleTaskToUpdateState: Updating service worker %llu state from %hhu to %hhu. Registration ID: %llu", identifier().toUInt64(), m_data.state, state, registrationIdentifier().toUInt64());
        m_data.state = state;
        if (state != State::Installing && !m_isStopped) {
            ASSERT(m_pendingActivityForEventDispatch);
            dispatchEvent(Event::create(eventNames().statechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
        }

        updatePendingActivityForEventDispatch();
    });
}

ExceptionOr<void> ServiceWorker::postMessage(ScriptExecutionContext& context, JSC::JSValue messageValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    if (m_isStopped || !context.sessionID().isValid())
        return Exception { InvalidStateError };

    if (state() == State::Redundant)
        return Exception { InvalidStateError, "Service Worker state is redundant"_s };

    // FIXME: Invoke Run Service Worker algorithm with serviceWorker as the argument.

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

    ServiceWorkerOrClientIdentifier sourceIdentifier;
    if (is<ServiceWorkerGlobalScope>(context))
        sourceIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();
    else {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(context.sessionID());
        sourceIdentifier = ServiceWorkerClientIdentifier { connection.serverConnectionIdentifier(), downcast<Document>(context).identifier() };
    }

    MessageWithMessagePorts message = { messageData.releaseReturnValue(), portsOrException.releaseReturnValue() };
    callOnMainThread([sessionID = context.sessionID(), destinationIdentifier = identifier(), message = WTFMove(message), sourceIdentifier = WTFMove(sourceIdentifier)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(sessionID);
        connection.postMessageToServiceWorker(destinationIdentifier, WTFMove(message), sourceIdentifier);
    });
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

bool ServiceWorker::canSuspendForDocumentSuspension() const
{
    // FIXME: We should do better as this prevents the page from entering PageCache when there is a Service Worker.
    return !hasPendingActivity();
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

bool ServiceWorker::isAlwaysOnLoggingAllowed() const
{
    auto* context = scriptExecutionContext();
    if (!context)
        return false;

    auto* container = context->serviceWorkerContainer();
    if (!container)
        return false;

    return container->isAlwaysOnLoggingAllowed();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
