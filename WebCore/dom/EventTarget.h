/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef EventTarget_h
#define EventTarget_h

#include <wtf/Forward.h>

namespace WebCore {

    class AtomicString;
    class DOMApplicationCache;
    class Event;
    class EventListener;
    class EventTargetNode;
    class MessagePort;
    class RegisteredEventListener;
    class ScriptExecutionContext;
    class SVGElementInstance;
    class WorkerContext;
    class XMLHttpRequest;
    class XMLHttpRequestUpload;

    typedef int ExceptionCode;

    template<typename T> class DeprecatedValueList;
    typedef DeprecatedValueList<RefPtr<RegisteredEventListener> > RegisteredEventListenerList;

    class EventTarget {
    public:
        virtual MessagePort* toMessagePort();
        virtual EventTargetNode* toNode();
        virtual XMLHttpRequest* toXMLHttpRequest();
        virtual XMLHttpRequestUpload* toXMLHttpRequestUpload();
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        virtual DOMApplicationCache* toDOMApplicationCache();
#endif
#if ENABLE(SVG)
        virtual SVGElementInstance* toSVGElementInstance();
#endif
#if ENABLE(WORKERS)
        virtual WorkerContext* toWorkerContext();
#endif

        virtual ScriptExecutionContext* scriptExecutionContext() const = 0;

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) = 0;
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) = 0;
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&) = 0;

        void ref() { refEventTarget(); }
        void deref() { derefEventTarget(); }

        // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
        // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
        virtual void* preDispatchEventHandler(Event*) { return 0; }
        virtual void postDispatchEventHandler(Event*, void* dataFromPreDispatch) { }

    protected:
        virtual ~EventTarget();

    private:
        virtual void refEventTarget() = 0;
        virtual void derefEventTarget() = 0;
    };

#ifndef NDEBUG
void forbidEventDispatch();
void allowEventDispatch();
bool eventDispatchForbidden();
#else
inline void forbidEventDispatch() { }
inline void allowEventDispatch() { }
#endif // NDEBUG 

}
#endif
