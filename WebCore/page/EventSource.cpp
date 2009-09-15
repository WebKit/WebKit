/*
 * Copyright (C) 2009 Ericsson AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#if ENABLE(EVENTSOURCE)

#include "EventSource.h"

#include "Cache.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventException.h"
#include "PlatformString.h"
#include "MessageEvent.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "TextResourceDecoder.h"
#include "ThreadableLoader.h"

namespace WebCore {

const unsigned long long EventSource::defaultReconnectDelay = 3000;

EventSource::EventSource(const String& url, ScriptExecutionContext* context, ExceptionCode& ec)
    : ActiveDOMObject(context, this)
    , m_state(CONNECTING)
    , m_reconnectTimer(this, &EventSource::reconnectTimerFired)
    , m_failSilently(false)
    , m_requestInFlight(false)
    , m_reconnectDelay(defaultReconnectDelay)
{
    if (url.isEmpty() || !(m_url = context->completeURL(url)).isValid()) {
        ec = SYNTAX_ERR;
        return;
    }
    // FIXME: should support cross-origin requests
    if (!scriptExecutionContext()->securityOrigin()->canRequest(m_url)) {
        ec = SECURITY_ERR;
        return;
    }

    m_origin = scriptExecutionContext()->securityOrigin()->toString();
    m_decoder = TextResourceDecoder::create("text/plain", "UTF-8");

    setPendingActivity(this);
    connect();
}

EventSource::~EventSource()
{
}

void EventSource::connect()
{
    ResourceRequest request(m_url);
    request.setHTTPMethod("GET");
    request.setHTTPHeaderField("Accept", "text/event-stream");
    request.setHTTPHeaderField("Cache-Control", "no-cache");
    if (!m_lastEventId.isEmpty())
        request.setHTTPHeaderField("Last-Event-ID", m_lastEventId);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = true;
    options.sniffContent = false;
    options.allowCredentials = true;

    m_loader = ThreadableLoader::create(scriptExecutionContext(), this, request, options);

    m_requestInFlight = true;

    if (!scriptExecutionContext()->isWorkerContext())
        cache()->loader()->nonCacheRequestInFlight(m_url);
}

void EventSource::endRequest()
{
    m_requestInFlight = false;

    if (!m_failSilently)
        dispatchGenericEvent(eventNames().errorEvent);

    if (!scriptExecutionContext()->isWorkerContext())
        cache()->loader()->nonCacheRequestComplete(m_url);

    if (m_state != CLOSED)
        scheduleReconnect();
    else
        unsetPendingActivity(this);
}

void EventSource::scheduleReconnect()
{
    m_state = CONNECTING;
    m_reconnectTimer.startOneShot(m_reconnectDelay / 1000);
}

void EventSource::reconnectTimerFired(Timer<EventSource>*)
{
    connect();
}

String EventSource::url() const
{
    return m_url.string();
}

EventSource::State EventSource::readyState() const
{
    return m_state;
}

void EventSource::close()
{
    if (m_state == CLOSED)
        return;

    if (m_reconnectTimer.isActive()) {
        m_reconnectTimer.stop();
        unsetPendingActivity(this);
    }

    m_state = CLOSED;
    m_failSilently = true;

    if (m_requestInFlight)
        m_loader->cancel();
}

ScriptExecutionContext* EventSource::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void EventSource::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
            if (**listenerIter == *eventListener)
                return;
        }

        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    }
}

void EventSource::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end())
        return;

    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
        if (**listenerIter == *eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
    }
}

bool EventSource::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec)
{
    if (!event || event->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    EventListener* attributeListener = m_attributeListeners.get(event->type()).get();
    if (attributeListener) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        attributeListener->handleEvent(event.get(), false);
    }

    ListenerVector listenersCopy = m_eventListeners.get(event->type());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listenerIter->get()->handleEvent(event.get(), false);
    }

    return !event->defaultPrevented();
}

void EventSource::didReceiveResponse(const ResourceResponse& response)
{
    int statusCode = response.httpStatusCode();
    if (statusCode == 200 && response.httpHeaderField("Content-Type") == "text/event-stream") {
        m_state = OPEN;
        dispatchGenericEvent(eventNames().openEvent);
    } else {
        if (statusCode <= 200 || statusCode > 299)
            m_state = CLOSED;
        m_loader->cancel();
    }
}

void EventSource::didReceiveData(const char* data, int length)
{
    append(m_receiveBuf, m_decoder->decode(data, length));
    parseEventStream();
}

void EventSource::didFinishLoading(unsigned long)
{
    if (m_receiveBuf.size() > 0 || m_data.size() > 0) {
        append(m_receiveBuf, "\n\n");
        parseEventStream();
    }
    m_state = CONNECTING;
    endRequest();
}

void EventSource::didFail(const ResourceError& error)
{
    int canceled = error.isCancellation();
    if (((m_state == CONNECTING) && !canceled) || ((m_state == OPEN) && canceled))
        m_state = CLOSED;
    endRequest();
}

void EventSource::didFailRedirectCheck()
{
    m_state = CLOSED;
    m_loader->cancel();
}

void EventSource::parseEventStream()
{
    unsigned int bufPos = 0;
    unsigned int bufSize = m_receiveBuf.size();
    for (;;) {
        int lineLength = -1;
        int fieldLength = -1;
        int carriageReturn = 0;
        for (unsigned int i = bufPos; lineLength < 0 && i < bufSize; i++) {
            switch (m_receiveBuf[i]) {
            case ':':
                if (fieldLength < 0)
                    fieldLength = i - bufPos;
                break;
            case '\n':
                if (i > bufPos && m_receiveBuf[i - 1] == '\r') {
                    carriageReturn++;
                    i--;
                }
                lineLength = i - bufPos;
                break;
            }
        }

        if (lineLength < 0)
            break;

        parseEventStreamLine(bufPos, fieldLength, lineLength);
        bufPos += lineLength + carriageReturn + 1;
    }

    if (bufPos == bufSize)
        m_receiveBuf.clear();
    else if (bufPos)
        m_receiveBuf.remove(0, bufPos);
}

void EventSource::parseEventStreamLine(unsigned int bufPos, int fieldLength, int lineLength)
{
    if (!lineLength) {
        if (!m_data.isEmpty())
            dispatchMessageEvent();
        if (!m_eventName.isEmpty())
            m_eventName = "";
    } else if (fieldLength) {
        bool noValue = fieldLength < 0;

        String field(&m_receiveBuf[bufPos], noValue ? lineLength : fieldLength);
        int step;
        if (noValue)
            step = lineLength;
        else if (m_receiveBuf[bufPos + fieldLength + 1] != ' ')
            step = fieldLength + 1;
        else
            step = fieldLength + 2;
        bufPos += step;
        int valueLength = lineLength - step;

        if (field == "data") {
            if (m_data.size() > 0)
                m_data.append('\n');
            if (valueLength)
                m_data.append(&m_receiveBuf[bufPos], valueLength);
        } else if (field == "event")
            m_eventName = valueLength ? String(&m_receiveBuf[bufPos], valueLength) : "";
        else if (field == "id")
            m_lastEventId = valueLength ? String(&m_receiveBuf[bufPos], valueLength) : "";
        else if (field == "retry") {
            if (!valueLength)
                m_reconnectDelay = defaultReconnectDelay;
            else {
                String value(&m_receiveBuf[bufPos], valueLength);
                bool ok;
                unsigned long long retry = value.toUInt64(&ok);
                if (ok)
                    m_reconnectDelay = retry;
            }
        }
    }
}

void EventSource::dispatchGenericEvent(const AtomicString& type)
{
    RefPtr<Event> evt = Event::create(type, false, false);
    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void EventSource::dispatchMessageEvent()
{
    RefPtr<MessageEvent> evt = MessageEvent::create();
    String eventName = m_eventName.isEmpty() ? eventNames().messageEvent.string() : m_eventName;
    evt->initMessageEvent(eventName, false, false, String::adopt(m_data), m_origin, m_lastEventId, 0, 0);
    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void EventSource::stop()
{
    close();
}

} // namespace WebCore

#endif // ENABLE(EVENTSOURCE)
