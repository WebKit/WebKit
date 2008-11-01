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

class CloseMessagePortTimer : public TimerBase {
public:
    CloseMessagePortTimer(PassRefPtr<MessagePort> port)
        : m_port(port)
    {
        ASSERT(m_port);
    }

private:
    virtual void fired()
    {
        ASSERT(!m_port->active());

        // Closing may destroy the port, dispatch any remaining messages now.
        if (m_port->queueIsOpen())
            m_port->dispatchMessages();

        m_port->dispatchCloseEvent();
        delete this;
    }

    RefPtr<MessagePort> m_port;
};

MessagePort::MessagePort(ScriptExecutionContext* scriptExecutionContext)
    : m_entangledPort(0)
    , m_queueIsOpen(false)
    , m_scriptExecutionContext(scriptExecutionContext)
    , m_pendingCloseEvent(false)
    , m_jsWrapperIsInaccessible(false)
{
    scriptExecutionContext->createdMessagePort(this);
}

MessagePort::~MessagePort()
{
    if (m_entangledPort)
        unentangle();

    if (m_scriptExecutionContext)
        m_scriptExecutionContext->destroyedMessagePort(this);
}

PassRefPtr<MessagePort> MessagePort::clone(ScriptExecutionContext* newOwner, ExceptionCode& ec)
{
    if (!m_entangledPort) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    RefPtr<MessagePort> remotePort = m_entangledPort;
    RefPtr<MessagePort> newPort = MessagePort::create(newOwner);

    // Move all the events in the port message queue of original port to the port message queue of new port, if any, leaving the new port's port message queue in its initial closed state.
    // If events are posted (e.g. from a worker thread) while this code is executing, there is no guarantee whether they end up in the original or new port's message queue.
    RefPtr<Event> event;
    while (m_messageQueue.tryGetMessage(event))
        newPort->m_messageQueue.append(event);

    entangle(remotePort.get(), newPort.get()); // The port object will be unentangled.
    return newPort;
}

void MessagePort::postMessage(const String& message, ExceptionCode& ec)
{
    postMessage(message, 0, ec);
}

void MessagePort::postMessage(const String& message, MessagePort* dataPort, ExceptionCode& ec)
{
    if (!m_entangledPort || !m_scriptExecutionContext || !m_entangledPort->m_scriptExecutionContext)
        return;

    RefPtr<MessagePort> newMessagePort;
    if (dataPort) {
        if (dataPort == this || dataPort == m_entangledPort) {
            ec = INVALID_ACCESS_ERR;
            return;
        }
        newMessagePort = dataPort->clone(m_entangledPort->m_scriptExecutionContext, ec);
        if (ec)
            return;
    }

    DOMWindow* window = (m_scriptExecutionContext->isDocument() && m_entangledPort->m_scriptExecutionContext->isDocument()) ?
        static_cast<Document*>(m_scriptExecutionContext)->domWindow() : 0;
    m_entangledPort->m_messageQueue.append(MessageEvent::create(message, "", "", window, newMessagePort.get()));
    if (m_entangledPort->m_queueIsOpen)
        m_entangledPort->m_scriptExecutionContext->processMessagePortMessagesSoon();
}

PassRefPtr<MessagePort> MessagePort::startConversation(ScriptExecutionContext* scriptExecutionContext, const String& message)
{
    RefPtr<MessagePort> port1 = MessagePort::create(scriptExecutionContext);
    if (!m_entangledPort || !m_scriptExecutionContext || !m_entangledPort->m_scriptExecutionContext)
        return port1;
    RefPtr<MessagePort> port2 = MessagePort::create(m_entangledPort->m_scriptExecutionContext);

    entangle(port1.get(), port2.get());

    DOMWindow* window = (m_scriptExecutionContext->isDocument() && m_entangledPort->m_scriptExecutionContext->isDocument()) ?
        static_cast<Document*>(m_scriptExecutionContext)->domWindow() : 0;
    m_entangledPort->m_messageQueue.append(MessageEvent::create(message, "", "", window, port2.get()));
    if (m_entangledPort->m_queueIsOpen)
        m_entangledPort->m_scriptExecutionContext->processMessagePortMessagesSoon();
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

    MessagePort* otherPort = m_entangledPort;
    unentangle();

    queueCloseEvent();
    otherPort->queueCloseEvent();
}

void MessagePort::entangle(MessagePort* port1, MessagePort* port2)
{
    if (port1->m_entangledPort) {
        ASSERT(port1->m_entangledPort != port2);
        port1->unentangle();
    }

    if (port2->m_entangledPort) {
        ASSERT(port2->m_entangledPort != port1);
        port2->unentangle();
    }

    port1->m_entangledPort = port2;
    port2->m_entangledPort = port1;
}

void MessagePort::unentangle()
{
    ASSERT(this == m_entangledPort->m_entangledPort);

    m_entangledPort->m_entangledPort = 0;
    m_entangledPort = 0;
}

void MessagePort::contextDestroyed()
{
    if (m_entangledPort) {
        RefPtr<MessagePort> survivingPort = m_entangledPort;
        unentangle();
        if (survivingPort->m_scriptExecutionContext != m_scriptExecutionContext) // Otherwise, survivingPort won't really survive.
            survivingPort->queueCloseEvent();
    }
    m_scriptExecutionContext = 0;
}

void MessagePort::dispatchMessages()
{
    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    ASSERT(queueIsOpen());

    RefPtr<Event> evt;
    while (m_messageQueue.tryGetMessage(evt)) {

        ASSERT(evt->type() == EventNames::messageEvent);

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

    CloseMessagePortTimer* timer = new CloseMessagePortTimer(this);
    timer->startOneShot(0);
}

void MessagePort::dispatchCloseEvent()
{
    ASSERT(m_pendingCloseEvent);
    m_pendingCloseEvent = false;

    RefPtr<Event> evt = Event::create(EventNames::closeEvent, false, true);
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
    EventListenersMap::iterator iter = m_eventListeners.find(eventType.impl());
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType.impl(), listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
            if (*listenerIter == eventListener)
                return;
        }
        
        listeners.append(eventListener);
        m_eventListeners.add(eventType.impl(), listeners);
    }    
}

void MessagePort::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool useCapture)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType.impl());
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
    if (event->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }
    
    ListenerVector listenersCopy = m_eventListeners.get(event->type().impl());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listenerIter->get()->handleEvent(event.get(), false);
    }
    
    return !event->defaultPrevented();
}

bool MessagePort::hasPendingActivity()
{
    // We only care about the result of this function when there is no entangled port, or it is inaccessible, so no more messages can be added to the queue.
    // Thus, using MessageQueue::isEmpty() does not cause a race condition here.
    return m_pendingCloseEvent || (m_queueIsOpen && !m_messageQueue.isEmpty());
}

} // namespace WebCore
