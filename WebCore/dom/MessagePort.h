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

#include "AtomicStringHash.h"
#include "EventListener.h"
#include "EventTarget.h"

#include <wtf/HashMap.h>
#include <wtf/MessageQueue.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

    class AtomicStringImpl;
    class DOMWindow;
    class Event;
    class Frame;
    class ScriptExecutionContext;
    class String;
    class WorkerContext;

    class MessagePort : public ThreadSafeShared<MessagePort>, public EventTarget {
    public:
        static PassRefPtr<MessagePort> create(ScriptExecutionContext* scriptExecutionContext) { return adoptRef(new MessagePort(scriptExecutionContext)); }
        ~MessagePort();

        PassRefPtr<MessagePort> clone(ScriptExecutionContext*, ExceptionCode&);

        bool active() const { return m_entangledPort; }
        void postMessage(const String& message, ExceptionCode&);
        void postMessage(const String& message, MessagePort*, ExceptionCode&);
        PassRefPtr<MessagePort> startConversation(ScriptExecutionContext*, const String& message);
        void start();
        void close();

        bool queueIsOpen() const { return m_queueIsOpen; }

        MessagePort* entangledPort() { return m_entangledPort; }
        static void entangle(MessagePort*, MessagePort*);
        void unentangle();

        void contextDestroyed();
        virtual ScriptExecutionContext* scriptExecutionContext() const { return m_scriptExecutionContext; }

        virtual MessagePort* toMessagePort() { return this; }

        void queueCloseEvent();
        void dispatchMessages();

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);

        typedef Vector<RefPtr<EventListener> > ListenerVector;
        typedef HashMap<AtomicString, ListenerVector> EventListenersMap;
        EventListenersMap& eventListeners() { return m_eventListeners; }

        using ThreadSafeShared<MessagePort>::ref;
        using ThreadSafeShared<MessagePort>::deref;

        bool hasPendingActivity();

        void setOnmessage(PassRefPtr<EventListener> eventListener) { m_onMessageListener = eventListener; }
        EventListener* onmessage() const { return m_onMessageListener.get(); }

        void setOnclose(PassRefPtr<EventListener> eventListener) { m_onCloseListener = eventListener; }
        EventListener* onclose() const { return m_onCloseListener.get(); }

        void setJSWrapperIsInaccessible() { m_jsWrapperIsInaccessible = true; }
        bool jsWrapperIsInaccessible() const { return m_jsWrapperIsInaccessible; }

    private:
        friend class CloseMessagePortTimer;

        MessagePort(ScriptExecutionContext*);

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

        void dispatchCloseEvent();

        MessagePort* m_entangledPort;
        
        struct EventData {
            EventData();
            EventData(const String&, PassRefPtr<DOMWindow>, PassRefPtr<MessagePort>);
            ~EventData();

            String message;
            RefPtr<DOMWindow> window;
            RefPtr<MessagePort> messagePort;
        };
        MessageQueue<EventData> m_messageQueue;
        bool m_queueIsOpen;

        ScriptExecutionContext* m_scriptExecutionContext;

        RefPtr<EventListener> m_onMessageListener;
        RefPtr<EventListener> m_onCloseListener;

        EventListenersMap m_eventListeners;

        bool m_pendingCloseEvent;
        bool m_jsWrapperIsInaccessible;
    };

} // namespace WebCore

#endif // MessagePort_h
