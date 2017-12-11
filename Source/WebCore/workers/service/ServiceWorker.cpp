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
#include "MessagePort.h"
#include "SWClientConnection.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerProvider.h"
#include <runtime/JSCJSValueInlines.h>
#include <wtf/NeverDestroyed.h>

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

        m_data.state = state;
        if (state != State::Installing && !m_isStopped) {
            ASSERT(m_pendingActivityForEventDispatch);
            dispatchEvent(Event::create(eventNames().statechangeEvent, false, false));
        }

        updatePendingActivityForEventDispatch();
    });
}

ExceptionOr<void> ServiceWorker::postMessage(ScriptExecutionContext& context, JSC::JSValue messageValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    if (m_isStopped)
        return Exception { InvalidStateError };

    if (state() == State::Redundant)
        return Exception { InvalidStateError, ASCIILiteral("Service Worker state is redundant") };

    // FIXME: Invoke Run Service Worker algorithm with serviceWorker as the argument.

    auto* execState = context.execState();
    ASSERT(execState);

    Vector<RefPtr<MessagePort>> ports;
    auto message = SerializedScriptValue::create(*execState, messageValue, WTFMove(transfer), ports, SerializationContext::WorkerPostMessage);
    if (message.hasException())
        return message.releaseException();

    // Disentangle the port in preparation for sending it to the remote context.
    auto channelsOrException = MessagePort::disentanglePorts(WTFMove(ports));
    if (channelsOrException.hasException())
        return channelsOrException.releaseException();

    // FIXME: Support sending the channels.
    auto channels = channelsOrException.releaseReturnValue();
    if (channels && !channels->isEmpty())
        return Exception { NotSupportedError, ASCIILiteral("Passing MessagePort objects to postMessage is not yet supported") };

    if (is<ServiceWorkerGlobalScope>(context)) {
        auto sourceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();
        callOnMainThread([sessionID = context.sessionID(), destinationIdentifier = identifier(), sourceWorkerIdentifier, message = WTFMove(message)]() mutable {
            auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(sessionID);
            connection.postMessageToServiceWorker(destinationIdentifier, message.releaseReturnValue(), sourceWorkerIdentifier);
        });
        return { };
    }

    auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(context.sessionID());
    // FIXME: We should be able to send only the client identifier and look up the clientData on server side.
    auto sourceClientData = ServiceWorkerClientData::from(context, connection);
    ServiceWorkerClientIdentifier sourceClientIdentifier { connection.serverConnectionIdentifier(), downcast<Document>(context).identifier() };
    connection.postMessageToServiceWorker(identifier(), message.releaseReturnValue(), WTFMove(sourceClientIdentifier), WTFMove(sourceClientData));

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

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
