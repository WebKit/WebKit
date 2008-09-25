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

#ifndef MessagePort_h
#define MessagePort_h

#include "EventListener.h"
#include "EventTarget.h"

#include <wtf/HashMap.h>
#include <wtf/MessageQueue.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

    class AtomicStringImpl;
    class Document;
    class Event;
    class String;

    class MessagePort : public RefCounted<MessagePort>, public EventTarget {
    public:
        static PassRefPtr<MessagePort> create(Document* document) { return adoptRef(new MessagePort(document)); }
        ~MessagePort();

        PassRefPtr<MessagePort> clone(Document*, ExceptionCode&);

        bool active() const { return m_entangledPort; }
        void postMessage(const String& message, ExceptionCode&);
        void postMessage(const String& message, MessagePort*, ExceptionCode&);
        PassRefPtr<MessagePort> startConversation(Document*, const String& message);
        void start();
        void close();

        bool queueIsOpen() const { return m_queueIsOpen; }

        MessagePort* entangledPort() { return m_entangledPort; }
        static void entangle(MessagePort*, MessagePort*);
        void unentangle();

        void contextDestroyed() { m_document = 0; } 
        Document* document() { return m_document; }
        virtual MessagePort* toMessagePort() { return this; }

        void dispatchMessages();
        void queueCloseEvent();
        void dispatchCloseEvent();

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);

        typedef Vector<RefPtr<EventListener> > ListenerVector;
        typedef HashMap<AtomicStringImpl*, ListenerVector> EventListenersMap;
        EventListenersMap& eventListeners() { return m_eventListeners; }

        using RefCounted<MessagePort>::ref;
        using RefCounted<MessagePort>::deref;

        void setOnMessageListener(PassRefPtr<EventListener> eventListener) { m_onMessageListener = eventListener; }
        EventListener* onMessageListener() const { return m_onMessageListener.get(); }

        void setOnCloseListener(PassRefPtr<EventListener> eventListener) { m_onCloseListener = eventListener; }
        EventListener* onCloseListener() const { return m_onCloseListener.get(); }

    private:
        MessagePort(Document*);

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

        MessagePort* m_entangledPort;
        MessageQueue<RefPtr<Event> > m_messageQueue;
        bool m_queueIsOpen;

        Document* m_document; // Will be 0 if the context does not contain a document (e.g. if it's a worker thread).

        RefPtr<EventListener> m_onMessageListener;
        RefPtr<EventListener> m_onCloseListener;

        EventListenersMap m_eventListeners;
    };

} // namespace WebCore

#endif // MessagePort_h
