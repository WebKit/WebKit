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

#ifndef EventSource_h
#define EventSource_h

#if ENABLE(EVENTSOURCE)

#include "ActiveDOMObject.h"
#include "AtomicStringHash.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "KURL.h"
#include "ThreadableLoaderClient.h"
#include "Timer.h"

#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

    class ResourceResponse;
    class TextResourceDecoder;
    class ThreadableLoader;

    class EventSource : public RefCounted<EventSource>, public EventTarget, private ThreadableLoaderClient, public ActiveDOMObject {
    public:
        static PassRefPtr<EventSource> create(const String& url, ScriptExecutionContext* context, ExceptionCode& ec) { return adoptRef(new EventSource(url, context, ec)); }
        virtual ~EventSource();

        static const unsigned long long defaultReconnectDelay;

        String url() const;

        enum State {
            CONNECTING = 0,
            OPEN = 1,
            CLOSED = 2,
        };

        State readyState() const;

        void setOnopen(PassRefPtr<EventListener> eventListener) { m_attributeListeners.set(eventNames().openEvent, eventListener); }
        EventListener* onopen() const { return m_attributeListeners.get(eventNames().openEvent).get(); }

        void setOnmessage(PassRefPtr<EventListener> eventListener) { m_attributeListeners.set(eventNames().messageEvent, eventListener); }
        EventListener* onmessage() const { return m_attributeListeners.get(eventNames().messageEvent).get(); }

        void setOnerror(PassRefPtr<EventListener> eventListener) { m_attributeListeners.set(eventNames().errorEvent, eventListener); }
        EventListener* onerror() const { return m_attributeListeners.get(eventNames().errorEvent).get(); }

        void close();

        using RefCounted<EventSource>::ref;
        using RefCounted<EventSource>::deref;

        virtual EventSource* toEventSource() { return this; }
        virtual ScriptExecutionContext* scriptExecutionContext() const;

        typedef Vector<RefPtr<EventListener> > ListenerVector;
        typedef HashMap<AtomicString, ListenerVector> EventListenersMap;

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);
        EventListenersMap& eventListeners() { return m_eventListeners; }

        virtual void stop();

    private:
        EventSource(const String& url, ScriptExecutionContext* context, ExceptionCode& ec);

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

        virtual void didReceiveResponse(const ResourceResponse& response);
        virtual void didReceiveData(const char* data, int length);
        virtual void didFinishLoading(unsigned long);
        virtual void didFail(const ResourceError& error);
        virtual void didFailRedirectCheck();

        void connect();
        void endRequest();
        void scheduleReconnect();
        void reconnectTimerFired(Timer<EventSource>*);
        void parseEventStream();
        void parseEventStreamLine(unsigned int pos, int fieldLength, int lineLength);
        void dispatchGenericEvent(const AtomicString& type);
        void dispatchMessageEvent();

        KURL m_url;
        State m_state;

        HashMap<AtomicString, RefPtr<EventListener> > m_attributeListeners;
        EventListenersMap m_eventListeners;

        RefPtr<TextResourceDecoder> m_decoder;
        RefPtr<ThreadableLoader> m_loader;
        Timer<EventSource> m_reconnectTimer;
        Vector<UChar> m_receiveBuf;
        bool m_failSilently;
        bool m_requestInFlight;

        String m_eventName;
        Vector<UChar> m_data;
        String m_lastEventId;
        unsigned long long m_reconnectDelay;
        String m_origin;
    };

} // namespace WebCore

#endif // ENABLE(EVENTSOURCE)

#endif // EventSource_h
