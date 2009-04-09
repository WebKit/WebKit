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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "AtomicString.h"
#include "DOMWindow.h"
#include "Document.h"
#include "EventException.h"
#include "EventNames.h"
#include "MessageEvent.h"
#include "SecurityOrigin.h"
#include "Timer.h"

namespace WebCore {

class MessagePortCloseEventTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<MessagePortCloseEventTask> create(PassRefPtr<MessagePort> port)
    {
        return adoptRef(new MessagePortCloseEventTask(port));
    }

private:
    MessagePortCloseEventTask(PassRefPtr<MessagePort> port)
        : m_port(port)
    {
        ASSERT(m_port);
    }

    virtual void performTask(ScriptExecutionContext* unusedContext)
    {
        ASSERT_UNUSED(unusedContext, unusedContext == m_port->scriptExecutionContext());
        ASSERT(!m_port->active());

        // Closing may destroy the port, dispatch any remaining messages now.
        if (m_port->queueIsOpen())
            m_port->dispatchMessages();

        m_port->dispatchCloseEvent();
    }

    RefPtr<MessagePort> m_port;
};

PassRefPtr<MessagePort::EventData> MessagePort::EventData::create(const String& message, PassRefPtr<MessagePort> port)
{
    return adoptRef(new EventData(message, port));
}

MessagePort::EventData::EventData(const String& message, PassRefPtr<MessagePort> messagePort)
    : message(message.copy())
    , messagePort(messagePort)
{
}

MessagePort::EventData::~EventData()
{
}

MessagePort::MessagePort(ScriptExecutionContext* scriptExecutionContext)
    : m_entangledPort(0)
    , m_queueIsOpen(false)
    , m_scriptExecutionContext(scriptExecutionContext)
    , m_pendingCloseEvent(false)
{
    if (scriptExecutionContext)
        scriptExecutionContext->createdMessagePort(this);
}

MessagePort::~MessagePort()
{
    if (m_entangledPort)
        unentangle();

    if (m_scriptExecutionContext)
        m_scriptExecutionContext->destroyedMessagePort(this);
}

PassRefPtr<MessagePort> MessagePort::clone(ExceptionCode& ec)
{
    if (!m_entangledPort) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    RefPtr<MessagePortProxy> remotePort = m_entangledPort;
    RefPtr<MessagePort> newPort = MessagePort::create(0);

    // Move all the events in the port message queue of original port to the port message queue of new port, if any, leaving the new port's port message queue in its initial closed state.
    // If events are posted (e.g. from a worker thread) while this code is executing, there is no guarantee whether they end up in the original or new port's message queue.
    RefPtr<EventData> eventData;
    while (m_messageQueue.tryGetMessage(eventData))
        newPort->m_messageQueue.append(eventData);

    entangle(remotePort.get(), newPort.get()); // The port object will be unentangled.
    return newPort;
}

void MessagePort::postMessage(const String& message, ExceptionCode& ec)
{
    postMessage(message, 0, ec);
}

void MessagePort::postMessage(const String& message, MessagePort* dataPort, ExceptionCode& ec)
{
    if (!m_entangledPort || !m_scriptExecutionContext)
        return;

    RefPtr<MessagePort> newMessagePort;
    if (dataPort) {
        if (dataPort == this || dataPort == m_entangledPort) {
            ec = INVALID_ACCESS_ERR;
            return;
        }
        ec = 0;
        newMessagePort = dataPort->clone(ec);
        if (ec)
            return;
    }

    m_entangledPort->deliverMessage(message, newMessagePort);
}

void MessagePort::deliverMessage(const String& message, PassRefPtr<MessagePort> dataPort)
{
    m_messageQueue.append(EventData::create(message, dataPort));
    if (m_queueIsOpen && m_scriptExecutionContext)
        m_scriptExecutionContext->processMessagePortMessagesSoon();
}

PassRefPtr<MessagePort> MessagePort::startConversation(ScriptExecutionContext* scriptExecutionContext, const String& message)
{
    RefPtr<MessagePort> port1 = MessagePort::create(scriptExecutionContext);
    if (!m_entangledPort || !m_scriptExecutionContext)
        return port1;
    RefPtr<MessagePort> port2 = MessagePort::create(0);

    entangle(port1.get(), port2.get());

    m_entangledPort->deliverMessage(message, port2);
    return port1;
}

void MessagePort::start()
{
    if (m_queueIsOpen || !m_scriptExecutionContext)
        return;

    m_queueIsOpen = true;
    m_scriptExecutionContext->processMessagePortMessagesSoon();
}

void MessagePort::close()
{
    if (!m_entangledPort)
        return;

    MessagePortProxy* otherPort = m_entangledPort;
    unentangle();

    queueCloseEvent();
    otherPort->queueCloseEvent();
}

void MessagePort::entangle(MessagePortProxy* port1, MessagePortProxy* port2)
{
    port1->entangle(port2);
    port2->entangle(port1);
}

void MessagePort::entangle(MessagePortProxy* remote)
{
    // Unentangle from our current port first.
    if (m_entangledPort) {
        ASSERT(m_entangledPort != remote);
        unentangle();
    }
    m_entangledPort = remote;
}

void MessagePort::unentangle()
{
    // Unentangle our end before unentangling the other end.
    if (m_entangledPort) {
        MessagePortProxy* remote = m_entangledPort;
        m_entangledPort = 0;
        remote->unentangle();
    }
}

void MessagePort::contextDestroyed()
{
    ASSERT(m_scriptExecutionContext);

    if (m_entangledPort)
        unentangle();

    m_scriptExecutionContext = 0;
}

void MessagePort::attachToContext(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT(!m_scriptExecutionContext);
    ASSERT(!m_queueIsOpen);

    m_scriptExecutionContext = scriptExecutionContext;
    m_scriptExecutionContext->createdMessagePort(this);
    
    // FIXME: Need to call processMessagePortMessagesSoon()?
}

ScriptExecutionContext* MessagePort::scriptExecutionContext() const
{
    return m_scriptExecutionContext;
}

void MessagePort::dispatchMessages()
{
    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    // FIXME: Such messages should be dispatched if the document returns from page cache. They are only allowed to be lost if the document is discarded.
    ASSERT(queueIsOpen());

    RefPtr<EventData> eventData;
    while (m_messageQueue.tryGetMessage(eventData)) {

        ASSERT(!eventData->messagePort || !eventData->messagePort->m_scriptExecutionContext);
        if (eventData->messagePort)
            eventData->messagePort->attachToContext(m_scriptExecutionContext);

        RefPtr<Event> evt = MessageEvent::create(eventData->message, "", "", 0, eventData->messagePort);

        if (m_onMessageListener) {
            evt->setTarget(this);
            evt->setCurrentTarget(this);
            m_onMessageListener->handleEvent(evt.get(), false);
        }

        ExceptionCode ec = 0;
        dispatchEvent(evt.release(), ec);
        ASSERT(!ec);
    }
}

void MessagePort::queueCloseEvent()
{
    ASSERT(!m_pendingCloseEvent);
    m_pendingCloseEvent = true;

    m_scriptExecutionContext->postTask(MessagePortCloseEventTask::create(this));
}

void MessagePort::dispatchCloseEvent()
{
    ASSERT(m_pendingCloseEvent);
    m_pendingCloseEvent = false;

    RefPtr<Event> evt = Event::create(eventNames().closeEvent, false, true);
    if (m_onCloseListener) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onCloseListener->handleEvent(evt.get(), false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void MessagePort::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
            if (*listenerIter == eventListener)
                return;
        }
        
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    }    
}

void MessagePort::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end())
        return;
    
    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
        if (*listenerIter == eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
    }
}

bool MessagePort::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec)
{
    if (!event || event->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }
    
    ListenerVector listenersCopy = m_eventListeners.get(event->type());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listenerIter->get()->handleEvent(event.get(), false);
    }
    
    return !event->defaultPrevented();
}

bool MessagePort::hasPendingActivity()
{
    return m_pendingCloseEvent || (m_queueIsOpen && !m_messageQueue.isEmpty());
}

} // namespace WebCore
