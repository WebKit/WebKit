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
#include "Logging.h"
#include "MessageEvent.h"
#include "MessagePortChannelProvider.h"
#include "MessageWithMessagePorts.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

static HashMap<MessagePortIdentifier, MessagePort*>& allMessagePorts()
{
    static NeverDestroyed<HashMap<MessagePortIdentifier, MessagePort*>> map;
    return map;
}

static Lock& allMessagePortsLock()
{
    static NeverDestroyed<Lock> lock;
    return lock;
}

void MessagePort::ref() const
{
    ++m_refCount;
}

void MessagePort::deref() const
{
    // MessagePort::existingMessagePortForIdentifier() is unique in that it holds a raw pointer to a MessagePort
    // but might create a RefPtr from it.
    // If that happens on one thread at the same time that a MessagePort is being deref'ed and destroyed on a
    // different thread then Bad Things could happen.
    // This custom deref() function is designed to handle that contention by guaranteeing that nobody can be
    // creating a RefPtr inside existingMessagePortForIdentifier while the object is mid-deletion.

    if (!--m_refCount) {
        Locker<Lock> locker(allMessagePortsLock());

        if (m_refCount)
            return;

        allMessagePorts().remove(m_identifier);
        delete this;
    }
}

RefPtr<MessagePort> MessagePort::existingMessagePortForIdentifier(const MessagePortIdentifier& identifier)
{
    Locker<Lock> locker(allMessagePortsLock());

    return allMessagePorts().get(identifier);
}

Ref<MessagePort> MessagePort::create(ScriptExecutionContext& scriptExecutionContext, const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    return adoptRef(*new MessagePort(scriptExecutionContext, local, remote));
}

MessagePort::MessagePort(ScriptExecutionContext& scriptExecutionContext, const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
    : ActiveDOMObject(&scriptExecutionContext)
    , m_identifier(local)
    , m_remoteIdentifier(remote)
{
    LOG(MessagePorts, "Created MessagePort %s (%p)", m_identifier.logString().utf8().data(), this);

    Locker<Lock> locker(allMessagePortsLock());
    allMessagePorts().set(m_identifier, this);

    m_scriptExecutionContext->createdMessagePort(*this);
    suspendIfNeeded();

    // Don't need to call processMessageWithMessagePortsSoon() here, because the port will not be opened until start() is invoked.
}

MessagePort::~MessagePort()
{
    LOG(MessagePorts, "Destroyed MessagePort %s (%p)", m_identifier.logString().utf8().data(), this);

    ASSERT(allMessagePortsLock().isLocked());

    if (m_entangled)
        close();

    if (m_scriptExecutionContext)
        m_scriptExecutionContext->destroyedMessagePort(*this);
}

void MessagePort::entangle()
{
    MessagePortChannelProvider::singleton().entangleLocalPortInThisProcessToRemote(m_identifier, m_remoteIdentifier);
}

ExceptionOr<void> MessagePort::postMessage(JSC::ExecState& state, JSC::JSValue messageValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    LOG(MessagePorts, "Attempting to post message to port %s (to be received by port %s)", m_identifier.logString().utf8().data(), m_remoteIdentifier.logString().utf8().data());

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(state, messageValue, WTFMove(transfer), ports);
    if (messageData.hasException())
        return messageData.releaseException();

    if (!isEntangled())
        return { };
    ASSERT(m_scriptExecutionContext);

    TransferredMessagePortArray transferredPorts;
    // Make sure we aren't connected to any of the passed-in ports.
    if (!ports.isEmpty()) {
        for (auto& port : ports) {
            if (port->identifier() == m_identifier || port->identifier() == m_remoteIdentifier)
                return Exception { DataCloneError };
        }

        auto disentangleResult = MessagePort::disentanglePorts(WTFMove(ports));
        if (disentangleResult.hasException())
            return disentangleResult.releaseException();
        transferredPorts = disentangleResult.releaseReturnValue();
    }

    MessageWithMessagePorts message { messageData.releaseReturnValue(), WTFMove(transferredPorts) };

    LOG(MessagePorts, "Actually posting message to port %s (to be received by port %s)", m_identifier.logString().utf8().data(), m_remoteIdentifier.logString().utf8().data());

    MessagePortChannelProvider::singleton().postMessageToRemote(WTFMove(message), m_remoteIdentifier);
    return { };
}

void MessagePort::disentangle()
{
    ASSERT(m_entangled);

    m_entangled = false;
    MessagePortChannelProvider::singleton().messagePortDisentangled(m_identifier);

    // We can't receive any messages or generate any events after this, so remove ourselves from the list of active ports.
    ASSERT(m_scriptExecutionContext);
    m_scriptExecutionContext->destroyedMessagePort(*this);
    m_scriptExecutionContext->willDestroyActiveDOMObject(*this);
    m_scriptExecutionContext->willDestroyDestructionObserver(*this);

    m_scriptExecutionContext = nullptr;
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any non-threadsafe APIs (i.e. should not call into the entangled channel or access mutable variables).
void MessagePort::messageAvailable()
{
    // This MessagePort object might be disentangled because the port is being transferred,
    // in which case we'll notify it that messages are available once a new end point is created.
    if (!m_scriptExecutionContext)
        return;

    m_scriptExecutionContext->processMessageWithMessagePortsSoon();
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
    m_scriptExecutionContext->processMessageWithMessagePortsSoon();
}

void MessagePort::close()
{
    if (m_closed)
        return;

    MessagePortChannelProvider::singleton().messagePortClosed(m_identifier);

    m_closed = true;
}

void MessagePort::contextDestroyed()
{
    ASSERT(m_scriptExecutionContext);
    if (!m_closed)
        close();

    m_scriptExecutionContext = nullptr;
}

void MessagePort::dispatchMessages()
{
    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    // The HTML5 spec specifies that any messages sent to a document that is not fully active should be dropped, so this behavior is OK.
    ASSERT(started());

    if (!isEntangled())
        return;

    auto messagesTakenHandler = [this, protectedThis = makeRef(*this)](Vector<MessageWithMessagePorts>&& messages, Function<void()>&& completionCallback) mutable {
        auto innerHandler = [this, otherProtectedThis = WTFMove(protectedThis)](Vector<MessageWithMessagePorts>&& messages) {
            LOG(MessagePorts, "MessagePort %s (%p) dispatching %zu messages", m_identifier.logString().utf8().data(), this, messages.size());

            if (!m_scriptExecutionContext)
                return;

            ASSERT(m_scriptExecutionContext->isContextThread());

            bool contextIsWorker = is<WorkerGlobalScope>(*m_scriptExecutionContext);
            for (auto& message : messages) {
                // close() in Worker onmessage handler should prevent next message from dispatching.
                if (contextIsWorker && downcast<WorkerGlobalScope>(*m_scriptExecutionContext).isClosing())
                    return;
                auto ports = MessagePort::entanglePorts(*m_scriptExecutionContext, WTFMove(message.transferredPorts));
                dispatchEvent(MessageEvent::create(WTFMove(ports), WTFMove(message.message)));
            }
        };

        if (!m_scriptExecutionContext)
            return;

        if (m_scriptExecutionContext->isContextThread()) {
            innerHandler(WTFMove(messages));
            completionCallback();
            return;
        }

        m_scriptExecutionContext->postTask([innerHandler = WTFMove(innerHandler), messages = WTFMove(messages), completionCallback = WTFMove(completionCallback)](ScriptExecutionContext&) mutable {
            innerHandler(WTFMove(messages));
            RunLoop::main().dispatch([completionCallback = WTFMove(completionCallback)] {
                completionCallback();
            });
        });
    };

    MessagePortChannelProvider::singleton().takeAllMessagesForPort(m_identifier, WTFMove(messagesTakenHandler));
}

bool MessagePort::hasPendingActivity() const
{
    // The spec says that entangled message ports should always be treated as if they have a strong reference.
    // We'll also stipulate that the queue needs to be open (if the app drops its reference to the port before start()-ing it, then it's not really entangled as it's unreachable).
    if (m_started && isEntangled() && MessagePortChannelProvider::singleton().hasMessagesForPorts_temporarySync(m_identifier, m_remoteIdentifier))
        return true;

    return false;
}

MessagePort* MessagePort::locallyEntangledPort() const
{
    // FIXME: As the header describes, this is an optional optimization.
    // Even in the new async model we should be able to get it right.
    return nullptr;
}

ExceptionOr<TransferredMessagePortArray> MessagePort::disentanglePorts(Vector<RefPtr<MessagePort>>&& ports)
{
    if (ports.isEmpty())
        return TransferredMessagePortArray { };

    // Walk the incoming array - if there are any duplicate ports, or null ports or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
    HashSet<MessagePort*> portSet;
    for (auto& port : ports) {
        if (!port || !port->m_entangled || !portSet.add(port.get()).isNewEntry)
            return Exception { DataCloneError };
    }

    // Passed-in ports passed validity checks, so we can disentangle them.
    TransferredMessagePortArray portArray;
    portArray.reserveInitialCapacity(ports.size());
    for (auto& port : ports) {
        portArray.uncheckedAppend({ port->identifier(), port->remoteIdentifier() });
        port->disentangle();
    }

    return WTFMove(portArray);
}

Vector<RefPtr<MessagePort>> MessagePort::entanglePorts(ScriptExecutionContext& context, TransferredMessagePortArray&& transferredPorts)
{
    LOG(MessagePorts, "Entangling %zu transferred ports to ScriptExecutionContext %s (%p)", transferredPorts.size(), context.url().string().utf8().data(), &context);

    if (transferredPorts.isEmpty())
        return { };

    Vector<RefPtr<MessagePort>> ports;
    ports.reserveInitialCapacity(transferredPorts.size());
    for (auto& transferredPort : transferredPorts) {
        auto port = MessagePort::create(context, transferredPort.first, transferredPort.second);
        port->entangle();
        ports.uncheckedAppend(WTFMove(port));
    }
    return ports;
}

bool MessagePort::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (listener->isAttribute() && eventType == eventNames().messageEvent)
        start();
    return EventTargetWithInlineData::addEventListener(eventType, WTFMove(listener), options);
}

const char* MessagePort::activeDOMObjectName() const
{
    return "MessagePort";
}

bool MessagePort::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity() || (!m_started || m_closed);
}

} // namespace WebCore
