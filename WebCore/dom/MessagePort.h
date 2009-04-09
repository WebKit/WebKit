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
#include "MessagePortProxy.h"

#include <wtf/HashMap.h>
#include <wtf/MessageQueue.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

    class AtomicStringImpl;
    class Event;
    class Frame;
    class ScriptExecutionContext;
    class String;
    class WorkerContext;

    class MessagePort : public MessagePortProxy, public EventTarget {
    public:
        static PassRefPtr<MessagePort> create(ScriptExecutionContext* scriptExecutionContext) { return adoptRef(new MessagePort(scriptExecutionContext)); }
        ~MessagePort();

        PassRefPtr<MessagePort> clone(ExceptionCode&); // Returns a port that isn't attached to any context.

        bool active() const { return m_entangledPort; }
        void postMessage(const String& message, ExceptionCode&);
        void postMessage(const String& message, MessagePort*, ExceptionCode&);
        PassRefPtr<MessagePort> startConversation(ScriptExecutionContext*, const String& message);
        void start();
        void close();

        // Implementations of MessagePortProxy APIs
        virtual void entangle(MessagePortProxy*);
        virtual void unentangle();
        virtual void deliverMessage(const String& message, PassRefPtr<MessagePort>);
        virtual void queueCloseEvent();

        bool queueIsOpen() const { return m_queueIsOpen; }

        MessagePortProxy* entangledPort() { return m_entangledPort; }
        static void entangle(MessagePortProxy*, MessagePortProxy*);

        void contextDestroyed();
        void attachToContext(ScriptExecutionContext*);
        virtual ScriptExecutionContext* scriptExecutionContext() const;

        virtual MessagePort* toMessagePort() { return this; }

        void dispatchMessages();

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);

        typedef Vector<RefPtr<EventListener> > ListenerVector;
        typedef HashMap<AtomicString, ListenerVector> EventListenersMap;
        EventListenersMap& eventListeners() { return m_eventListeners; }

        using RefCounted<MessagePortProxy>::ref;
        using RefCounted<MessagePortProxy>::deref;

        bool hasPendingActivity();

        // FIXME: Per current spec, setting onmessage should automagically start the port (unlike addEventListener("message", ...)).
        void setOnmessage(PassRefPtr<EventListener> eventListener) { m_onMessageListener = eventListener; }
        EventListener* onmessage() const { return m_onMessageListener.get(); }

        void setOnclose(PassRefPtr<EventListener> eventListener) { m_onCloseListener = eventListener; }
        EventListener* onclose() const { return m_onCloseListener.get(); }

    private:
        friend class MessagePortCloseEventTask;

        MessagePort(ScriptExecutionContext*);

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

        void dispatchCloseEvent();

        MessagePortProxy* m_entangledPort;

        // FIXME: EventData is necessary to pass messages to other threads. In single threaded case, we can just queue a created event.
        struct EventData : public RefCounted<EventData> {
            static PassRefPtr<EventData> create(const String& message, PassRefPtr<MessagePort>);
            ~EventData();

            String message;
            RefPtr<MessagePort> messagePort;

        private:
            EventData(const String& message, PassRefPtr<MessagePort>);
        };
        MessageQueue<RefPtr<EventData> > m_messageQueue; // FIXME: No need to use MessageQueue in single threaded case.
        bool m_queueIsOpen;

        ScriptExecutionContext* m_scriptExecutionContext;

        RefPtr<EventListener> m_onMessageListener;
        RefPtr<EventListener> m_onCloseListener;

        EventListenersMap m_eventListeners;

        bool m_pendingCloseEvent; // The port is GC protected while waiting for a close event to be dispatched.
    };

} // namespace WebCore

#endif // MessagePort_h
