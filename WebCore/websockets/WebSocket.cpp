/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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

#if ENABLE(WEB_SOCKETS)

#include "WebSocket.h"

#include "CString.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Logging.h"
#include "MessageEvent.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "WebSocketChannel.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

class ProcessWebSocketEventTask : public ScriptExecutionContext::Task {
public:
    typedef void (WebSocket::*Method)(Event*);
    static PassRefPtr<ProcessWebSocketEventTask> create(PassRefPtr<WebSocket> webSocket, Method method, PassRefPtr<Event> event)
    {
        return adoptRef(new ProcessWebSocketEventTask(webSocket, method, event));
    }
    virtual void performTask(ScriptExecutionContext*)
    {
        (m_webSocket.get()->*m_method)(m_event.get());
    }

  private:
    ProcessWebSocketEventTask(PassRefPtr<WebSocket> webSocket, Method method, PassRefPtr<Event> event)
        : m_webSocket(webSocket)
        , m_method(method)
        , m_event(event) { }

    RefPtr<WebSocket> m_webSocket;
    Method m_method;
    RefPtr<Event> m_event;
};

static bool isValidProtocolString(const WebCore::String& protocol)
{
    if (protocol.isNull())
        return true;
    if (protocol.isEmpty())
        return false;
    const UChar* characters = protocol.characters();
    for (size_t i = 0; i < protocol.length(); i++) {
        if (characters[i] < 0x21 || characters[i] > 0x7E)
            return false;
    }
    return true;
}

WebSocket::WebSocket(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_state(CONNECTING)
{
}

WebSocket::~WebSocket()
{
    close();
}

void WebSocket::connect(const KURL& url, ExceptionCode& ec)
{
    connect(url, String(), ec);
}

void WebSocket::connect(const KURL& url, const String& protocol, ExceptionCode& ec)
{
    LOG(Network, "WebSocket %p connect to %s protocol=%s", this, url.string().utf8().data(), protocol.utf8().data());
    m_url = url;
    m_protocol = protocol;

    if (!m_url.protocolIs("ws") && !m_url.protocolIs("wss")) {
        LOG_ERROR("Error: wrong url for WebSocket %s", url.string().utf8().data());
        m_state = CLOSED;
        ec = SYNTAX_ERR;
        return;
    }
    if (!isValidProtocolString(m_protocol)) {
        LOG_ERROR("Error: wrong protocol for WebSocket %s", m_protocol.utf8().data());
        m_state = CLOSED;
        ec = SYNTAX_ERR;
        return;
    }
    // FIXME: if m_url.port() is blocking port, raise SECURITY_ERR.

    m_channel = WebSocketChannel::create(scriptExecutionContext(), this, m_url, m_protocol);
    m_channel->connect();
}

bool WebSocket::send(const String& message, ExceptionCode& ec)
{
    LOG(Network, "WebSocket %p send %s", this, message.utf8().data());
    if (m_state == CONNECTING) {
        ec = INVALID_STATE_ERR;
        return false;
    }
    // No exception is raised if the connection was once established but has subsequently been closed.
    if (m_state == CLOSED)
        return false;
    // FIXME: check message is valid utf8.
    return m_channel->send(message);
}

void WebSocket::close()
{
    LOG(Network, "WebSocket %p close", this);
    if (m_state == CLOSED)
        return;
    m_state = CLOSED;
    m_channel->close();
}

const KURL& WebSocket::url() const
{
    return m_url;
}

WebSocket::State WebSocket::readyState() const
{
    return m_state;
}

unsigned long WebSocket::bufferedAmount() const
{
    if (m_state == OPEN)
        return m_channel->bufferedAmount();
    return 0;
}

ScriptExecutionContext* WebSocket::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void WebSocket::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter)
            if (*listenerIter == eventListener)
                return;

        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    }
}

void WebSocket::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end())
        return;

    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter)
        if (*listenerIter == eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
}

bool WebSocket::dispatchEvent(PassRefPtr<Event> evt, ExceptionCode& ec)
{
    if (!evt || evt->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    ListenerVector listenersCopy = m_eventListeners.get(evt->type());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        listenerIter->get()->handleEvent(evt.get(), false);
    }
    return !evt->defaultPrevented();
}

void WebSocket::didConnect()
{
    LOG(Network, "WebSocket %p didConnect", this);
    if (m_state != CONNECTING) {
        didClose();
        return;
    }
    m_state = OPEN;
    scriptExecutionContext()->postTask(ProcessWebSocketEventTask::create(this, &WebSocket::dispatchOpenEvent, Event::create(eventNames().openEvent, false, false)));
}

void WebSocket::didReceiveMessage(const String& msg)
{
    LOG(Network, "WebSocket %p didReceiveMessage %s", this, msg.utf8().data());
    if (m_state != OPEN)
        return;
    RefPtr<MessageEvent> evt = MessageEvent::create();
    // FIXME: origin, lastEventId, source, messagePort.
    evt->initMessageEvent(eventNames().messageEvent, false, false, msg, "", "", 0, 0);
    scriptExecutionContext()->postTask(ProcessWebSocketEventTask::create(this, &WebSocket::dispatchMessageEvent, evt));
}

void WebSocket::didClose()
{
    LOG(Network, "WebSocket %p didClose", this);
    m_state = CLOSED;
    scriptExecutionContext()->postTask(ProcessWebSocketEventTask::create(this, &WebSocket::dispatchCloseEvent, Event::create(eventNames().closeEvent, false, false)));
}

void WebSocket::dispatchOpenEvent(Event* evt)
{
    if (m_onopen) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onopen->handleEvent(evt, false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt, ec);
    ASSERT(!ec);
}

void WebSocket::dispatchMessageEvent(Event* evt)
{
    if (m_onmessage) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onmessage->handleEvent(evt, false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt, ec);
    ASSERT(!ec);
}

void WebSocket::dispatchCloseEvent(Event* evt)
{
    if (m_onclose) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onclose->handleEvent(evt, false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt, ec);
    ASSERT(!ec);
}

}  // namespace WebCore

#endif
