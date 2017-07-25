/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "MessagePort.h"

#include "Document.h"
#include "EventNames.h"
#include "MessageEvent.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

MessagePort::MessagePort(ScriptExecutionContext& scriptExecutionContext)
    : m_scriptExecutionContext(&scriptExecutionContext)
{
    m_scriptExecutionContext->createdMessagePort(*this);

    // Don't need to call processMessagePortMessagesSoon() here, because the port will not be opened until start() is invoked.
}

MessagePort::~MessagePort()
{
    close();
    if (m_scriptExecutionContext)
        m_scriptExecutionContext->destroyedMessagePort(*this);
}

ExceptionOr<void> MessagePort::postMessage(JSC::ExecState& state, JSC::JSValue messageValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    Vector<RefPtr<MessagePort>> ports;
    auto message = SerializedScriptValue::create(state, messageValue, WTFMove(transfer), ports);
    if (message.hasException())
        return message.releaseException();

    if (!isEntangled())
        return { };
    ASSERT(m_scriptExecutionContext);

    std::unique_ptr<MessagePortChannelArray> channels;
    // Make sure we aren't connected to any of the passed-in ports.
    if (!ports.isEmpty()) {
        for (auto& dataPort : ports) {
            if (dataPort == this || m_entangledChannel->isConnectedTo(dataPort.get()))
                return Exception { DataCloneError };
        }
        auto disentangleResult = MessagePort::disentanglePorts(WTFMove(ports));
        if (disentangleResult.hasException())
            return disentangleResult.releaseException();
        channels = disentangleResult.releaseReturnValue();
    }
    m_entangledChannel->postMessageToRemote(message.releaseReturnValue(), WTFMove(channels));
    return { };
}

std::unique_ptr<MessagePortChannel> MessagePort::disentangle()
{
    ASSERT(m_entangledChannel);

    m_entangledChannel->disentangle();

    // We can't receive any messages or generate any events after this, so remove ourselves from the list of active ports.
    ASSERT(m_scriptExecutionContext);
    m_scriptExecutionContext->destroyedMessagePort(*this);
    m_scriptExecutionContext = nullptr;

    return WTFMove(m_entangledChannel);
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any non-threadsafe APIs (i.e. should not call into the entangled channel or access mutable variables).
void MessagePort::messageAvailable()
{
    ASSERT(m_scriptExecutionContext);
    m_scriptExecutionContext->processMessagePortMessagesSoon();
}

void MessagePort::start()
{
    // Do nothing if we've been cloned or closed.
    if (!isEntangled())
        return;

    ASSERT(m_scriptExecutionContext);
    if (m_started)
        return;

    m_started = true;
    m_scriptExecutionContext->processMessagePortMessagesSoon();
}

void MessagePort::close()
{
    if (isEntangled())
        m_entangledChannel->close();
    m_closed = true;
}

void MessagePort::entangle(std::unique_ptr<MessagePortChannel>&& remote)
{
    // Only invoked to set our initial entanglement.
    ASSERT(!m_entangledChannel);
    ASSERT(m_scriptExecutionContext);

    // Don't entangle the ports if the channel is closed.
    if (remote->entangleIfOpen(this))
        m_entangledChannel = WTFMove(remote);
}

void MessagePort::contextDestroyed()
{
    ASSERT(m_scriptExecutionContext);
    // Must be closed before blowing away the cached context, to ensure that we get no more calls to messageAvailable().
    // ScriptExecutionContext::closeMessagePorts() takes care of that.
    ASSERT(m_closed);
    m_scriptExecutionContext = nullptr;
}

void MessagePort::dispatchMessages()
{
    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    // The HTML5 spec specifies that any messages sent to a document that is not fully active should be dropped, so this behavior is OK.
    ASSERT(started());

    if (!m_entangledChannel)
        return;

    bool contextIsWorker = is<WorkerGlobalScope>(*m_scriptExecutionContext);

    auto pendingMessages = m_entangledChannel->takeAllMessagesFromRemote();
    for (auto& message : pendingMessages) {
        // close() in Worker onmessage handler should prevent next message from dispatching.
        if (contextIsWorker && downcast<WorkerGlobalScope>(*m_scriptExecutionContext).isClosing())
            return;

        auto ports = MessagePort::entanglePorts(*m_scriptExecutionContext, WTFMove(message->channels));
        dispatchEvent(MessageEvent::create(WTFMove(ports), WTFMove(message->message)));
    }
}

bool MessagePort::hasPendingActivity()
{
    // The spec says that entangled message ports should always be treated as if they have a strong reference.
    // We'll also stipulate that the queue needs to be open (if the app drops its reference to the port before start()-ing it, then it's not really entangled as it's unreachable).
    if (m_started && m_entangledChannel && m_entangledChannel->hasPendingActivity())
        return true;
    if (isEntangled() && !locallyEntangledPort())
        return true;
    return false;
}

MessagePort* MessagePort::locallyEntangledPort()
{
    return m_entangledChannel ? m_entangledChannel->locallyEntangledPort(m_scriptExecutionContext) : nullptr;
}

ExceptionOr<std::unique_ptr<MessagePortChannelArray>> MessagePort::disentanglePorts(Vector<RefPtr<MessagePort>>&& ports)
{
    if (ports.isEmpty())
        return nullptr;

    // Walk the incoming array - if there are any duplicate ports, or null ports or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
    HashSet<MessagePort*> portSet;
    for (auto& port : ports) {
        if (!port || port->isNeutered() || !portSet.add(port.get()).isNewEntry)
            return Exception { DataCloneError };
    }

    // Passed-in ports passed validity checks, so we can disentangle them.
    auto portArray = std::make_unique<MessagePortChannelArray>(ports.size());
    for (unsigned i = 0 ; i < ports.size(); ++i)
        (*portArray)[i] = ports[i]->disentangle();
    return WTFMove(portArray);
}

Vector<RefPtr<MessagePort>> MessagePort::entanglePorts(ScriptExecutionContext& context, std::unique_ptr<MessagePortChannelArray>&& channels)
{
    if (!channels || !channels->size())
        return { };

    Vector<RefPtr<MessagePort>> portArray;
    portArray.reserveInitialCapacity(channels->size());
    for (unsigned int i = 0; i < channels->size(); ++i) {
        auto port = MessagePort::create(context);
        port->entangle(WTFMove((*channels)[i]));
        portArray.uncheckedAppend(WTFMove(port));
    }
    return portArray;
}

bool MessagePort::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (listener->isAttribute() && eventType == eventNames().messageEvent)
        start();
    return EventTargetWithInlineData::addEventListener(eventType, WTFMove(listener), options);
}

} // namespace WebCore
