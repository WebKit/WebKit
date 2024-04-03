/*
 * Copyright (C) 2008-2023 Apple Inc. All Rights Reserved.
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
#include "StructuredSerializeOptions.h"
#include "WebCoreOpaqueRoot.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Scope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MessagePort);

static Lock allMessagePortsLock;
static HashMap<MessagePortIdentifier, ThreadSafeWeakPtr<MessagePort>>& allMessagePorts() WTF_REQUIRES_LOCK(allMessagePortsLock)
{
    static NeverDestroyed<HashMap<MessagePortIdentifier, ThreadSafeWeakPtr<MessagePort>>> map;
    return map;
}

static HashMap<MessagePortIdentifier, ScriptExecutionContextIdentifier>& portToContextIdentifier() WTF_REQUIRES_LOCK(allMessagePortsLock)
{
    static NeverDestroyed<HashMap<MessagePortIdentifier, ScriptExecutionContextIdentifier>> map;
    return map;
}

bool MessagePort::isMessagePortAliveForTesting(const MessagePortIdentifier& identifier)
{
    Locker locker { allMessagePortsLock };
    return allMessagePorts().contains(identifier);
}

void MessagePort::notifyMessageAvailable(const MessagePortIdentifier& identifier)
{
    ASSERT(isMainThread());
    ScriptExecutionContextIdentifier scriptExecutionContextIdentifier;
    ThreadSafeWeakPtr<MessagePort> weakPort;
    {
        Locker locker { allMessagePortsLock };
        scriptExecutionContextIdentifier = portToContextIdentifier().get(identifier);
        weakPort = allMessagePorts().get(identifier);
    }
    if (!scriptExecutionContextIdentifier)
        return;

    ScriptExecutionContext::ensureOnContextThread(scriptExecutionContextIdentifier, [weakPort = WTFMove(weakPort)](auto&) {
        if (RefPtr port = weakPort.get())
            port->messageAvailable();
    });
}

Ref<MessagePort> MessagePort::create(ScriptExecutionContext& scriptExecutionContext, const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    Ref messagePort = adoptRef(*new MessagePort(scriptExecutionContext, local, remote));
    messagePort->suspendIfNeeded();
    return messagePort;
}

MessagePort::MessagePort(ScriptExecutionContext& scriptExecutionContext, const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
    : ActiveDOMObject(&scriptExecutionContext)
    , m_identifier(local)
    , m_remoteIdentifier(remote)
{
    LOG(MessagePorts, "Created MessagePort %s (%p) in process %" PRIu64, m_identifier.logString().utf8().data(), this, Process::identifier().toUInt64());

    Locker locker { allMessagePortsLock };
    // We disable threading assertions since the allMessagePorts() is used from multiple threads in a safe way, using a lock.
    allMessagePorts().set(m_identifier, ThreadSafeWeakPtr { *this });
    portToContextIdentifier().set(m_identifier, scriptExecutionContext.identifier());

    // Make sure the WeakPtrFactory gets initialized eagerly on the thread the MessagePort gets constructed on for thread-safety reasons.
    initializeWeakPtrFactory();

    scriptExecutionContext.createdMessagePort(*this);

    // Don't need to call processMessageWithMessagePortsSoon() here, because the port will not be opened until start() is invoked.
}

MessagePort::~MessagePort()
{
    LOG(MessagePorts, "Destroyed MessagePort %s (%p) in process %" PRIu64, m_identifier.logString().utf8().data(), this, Process::identifier().toUInt64());

    Locker locker { allMessagePortsLock };

    auto iterator = allMessagePorts().find(m_identifier);
    if (iterator != allMessagePorts().end()) {
        // ThreadSafeWeakPtr::get() returns null as soon as the object has started destruction.
        if (RefPtr messagePort = iterator->value.get(); !messagePort) {
            allMessagePorts().remove(iterator);
            portToContextIdentifier().remove(m_identifier);
        }
    }

    if (m_entangled)
        close();

    if (auto* context = scriptExecutionContext())
        context->destroyedMessagePort(*this);
}

void MessagePort::entangle()
{
    MessagePortChannelProvider::fromContext(*protectedScriptExecutionContext()).entangleLocalPortInThisProcessToRemote(m_identifier, m_remoteIdentifier);
}

ExceptionOr<void> MessagePort::postMessage(JSC::JSGlobalObject& state, JSC::JSValue messageValue, StructuredSerializeOptions&& options)
{
    LOG(MessagePorts, "Attempting to post message to port %s (to be received by port %s)", m_identifier.logString().utf8().data(), m_remoteIdentifier.logString().utf8().data());

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(state, messageValue, WTFMove(options.transfer), ports, SerializationForStorage::No, SerializationContext::WorkerPostMessage);
    if (messageData.hasException())
        return messageData.releaseException();

    if (!isEntangled())
        return { };
    ASSERT(scriptExecutionContext());

    Vector<TransferredMessagePort> transferredPorts;
    // Make sure we aren't connected to any of the passed-in ports.
    if (!ports.isEmpty()) {
        for (auto& port : ports) {
            if (port->identifier() == m_identifier || port->identifier() == m_remoteIdentifier)
                return Exception { ExceptionCode::DataCloneError };
        }

        auto disentangleResult = MessagePort::disentanglePorts(WTFMove(ports));
        if (disentangleResult.hasException())
            return disentangleResult.releaseException();
        transferredPorts = disentangleResult.releaseReturnValue();
    }

    MessageWithMessagePorts message { messageData.releaseReturnValue(), WTFMove(transferredPorts) };

    LOG(MessagePorts, "Actually posting message to port %s (to be received by port %s)", m_identifier.logString().utf8().data(), m_remoteIdentifier.logString().utf8().data());

    MessagePortChannelProvider::fromContext(*protectedScriptExecutionContext()).postMessageToRemote(WTFMove(message), m_remoteIdentifier);
    return { };
}

TransferredMessagePort MessagePort::disentangle()
{
    ASSERT(m_entangled);
    m_entangled = false;

    Ref context = *scriptExecutionContext();
    MessagePortChannelProvider::fromContext(context).messagePortDisentangled(m_identifier);

    // We can't receive any messages or generate any events after this, so remove ourselves from the list of active ports.
    context->destroyedMessagePort(*this);
    context->willDestroyActiveDOMObject(*this);
    context->willDestroyDestructionObserver(*this);

    observeContext(nullptr);

    return { identifier(), remoteIdentifier() };
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any non-threadsafe APIs (i.e. should not call into the entangled channel or access mutable variables).
void MessagePort::messageAvailable()
{
    // This MessagePort object might be disentangled because the port is being transferred,
    // in which case we'll notify it that messages are available once a new end point is created.
    RefPtr context = scriptExecutionContext();
    if (!context || context->activeDOMObjectsAreSuspended())
        return;

    context->processMessageWithMessagePortsSoon([pendingActivity = makePendingActivity(*this)] { });
}

void MessagePort::start()
{
    // Do nothing if we've been cloned or closed.
    if (!isEntangled())
        return;

    ASSERT(scriptExecutionContext());
    if (m_started)
        return;

    m_started = true;
    protectedScriptExecutionContext()->processMessageWithMessagePortsSoon([pendingActivity = makePendingActivity(*this)] { });
}

void MessagePort::close()
{
    if (m_isDetached)
        return;
    m_isDetached = true;

    ensureOnMainThread([identifier = m_identifier] {
        MessagePortChannelProvider::singleton().messagePortClosed(identifier);
    });

    removeAllEventListeners();
}

void MessagePort::contextDestroyed()
{
    ASSERT(scriptExecutionContext());

    close();
    ActiveDOMObject::contextDestroyed();
}

void MessagePort::dispatchMessages()
{
    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    // The HTML5 spec specifies that any messages sent to a document that is not fully active should be dropped, so this behavior is OK.
    ASSERT(started());

    RefPtr context = scriptExecutionContext();
    if (!context || context->activeDOMObjectsAreSuspended() || !isEntangled())
        return;

    auto messagesTakenHandler = [this, protectedThis = makePendingActivity(*this)](Vector<MessageWithMessagePorts>&& messages, CompletionHandler<void()>&& completionCallback) mutable {
        auto scopeExit = makeScopeExit(WTFMove(completionCallback));

        LOG(MessagePorts, "MessagePort %s (%p) dispatching %zu messages", m_identifier.logString().utf8().data(), this, messages.size());

        RefPtrAllowingPartiallyDestroyed<ScriptExecutionContext> context = scriptExecutionContext();
        if (!context || !context->globalObject())
            return;

        ASSERT(context->isContextThread());
        auto* globalObject = context->globalObject();
        Ref vm = globalObject->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);

        auto* workerGlobalScope = dynamicDowncast<WorkerGlobalScope>(*context);
        for (auto& message : messages) {
            // close() in Worker onmessage handler should prevent next message from dispatching.
            if (workerGlobalScope && workerGlobalScope->isClosing())
                return;

            auto ports = MessagePort::entanglePorts(*context, WTFMove(message.transferredPorts));
            auto event = MessageEvent::create(*globalObject, message.message.releaseNonNull(), { }, { }, { }, WTFMove(ports));
            if (UNLIKELY(scope.exception())) {
                // Currently, we assume that the only way we can get here is if we have a termination.
                RELEASE_ASSERT(vm->hasPendingTerminationException());
                return;
            }

            // Per specification, each MessagePort object has a task source called the port message queue.
            queueTaskKeepingObjectAlive(*this, TaskSource::PostedMessageQueue, [this, event = WTFMove(event)] {
                dispatchEvent(event.event);
            });
        }
    };

    MessagePortChannelProvider::fromContext(*context).takeAllMessagesForPort(m_identifier, WTFMove(messagesTakenHandler));
}

void MessagePort::dispatchEvent(Event& event)
{
    if (m_isDetached)
        return;

    if (RefPtr globalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext())) {
        if (globalScope->isClosing())
            return;
    }

    EventTarget::dispatchEvent(event);
}

// https://html.spec.whatwg.org/multipage/web-messaging.html#ports-and-garbage-collection
bool MessagePort::virtualHasPendingActivity() const
{
    // If the ScriptExecutionContext has been shut down on this object close()'ed, we can GC.
    auto* context = scriptExecutionContext();
    if (!context || m_isDetached)
        return false;

    // If this MessagePort has no message event handler then there is no point in keeping it alive.
    if (!m_hasMessageEventListener)
        return false;

    return m_entangled;
}

MessagePort* MessagePort::locallyEntangledPort() const
{
    // FIXME: As the header describes, this is an optional optimization.
    // Even in the new async model we should be able to get it right.
    return nullptr;
}

ExceptionOr<Vector<TransferredMessagePort>> MessagePort::disentanglePorts(Vector<RefPtr<MessagePort>>&& ports)
{
    if (ports.isEmpty())
        return Vector<TransferredMessagePort> { };

    // Walk the incoming array - if there are any duplicate ports, or null ports or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
    HashSet<Ref<MessagePort>> portSet;
    for (auto& port : ports) {
        if (!port || !port->m_entangled || !portSet.add(*port).isNewEntry)
            return Exception { ExceptionCode::DataCloneError };
    }

    // Passed-in ports passed validity checks, so we can disentangle them.
    return WTF::map(ports, [](auto& port) {
        return port->disentangle();
    });
}

Vector<RefPtr<MessagePort>> MessagePort::entanglePorts(ScriptExecutionContext& context, Vector<TransferredMessagePort>&& transferredPorts)
{
    LOG(MessagePorts, "Entangling %zu transferred ports to ScriptExecutionContext %s (%p)", transferredPorts.size(), context.url().string().utf8().data(), &context);

    if (transferredPorts.isEmpty())
        return { };

    return WTF::map(transferredPorts, [&](auto& port) -> RefPtr<MessagePort> {
        return MessagePort::entangle(context, WTFMove(port));
    });
}

Ref<MessagePort> MessagePort::entangle(ScriptExecutionContext& context, TransferredMessagePort&& transferredPort)
{
    Ref port = MessagePort::create(context, transferredPort.first, transferredPort.second);
    port->entangle();
    return port;
}

bool MessagePort::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (eventType == eventNames().messageEvent) {
        if (listener->isAttribute())
            start();
        m_hasMessageEventListener = true;
    }

    return EventTarget::addEventListener(eventType, WTFMove(listener), options);
}

bool MessagePort::removeEventListener(const AtomString& eventType, EventListener& listener, const EventListenerOptions& options)
{
    bool result = EventTarget::removeEventListener(eventType, listener, options);

    if (!hasEventListeners(eventNames().messageEvent))
        m_hasMessageEventListener = false;

    return result;
}

const char* MessagePort::activeDOMObjectName() const
{
    return "MessagePort";
}

WebCoreOpaqueRoot root(MessagePort* port)
{
    return WebCoreOpaqueRoot { port };
}

} // namespace WebCore
