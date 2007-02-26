/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
    class Event;
    class EventListener;
    class EventTargetNode;
    class SVGElementInstance;
    class XMLHttpRequest;

    typedef int ExceptionCode;

    class EventTarget {
    public:
        virtual EventTargetNode* toNode();
        virtual XMLHttpRequest* toXMLHttpRequest();

#if ENABLE(SVG)
        virtual SVGElementInstance* toSVGElementInstance();
#endif

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) = 0;
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) = 0;
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false) = 0;
    
        void ref() { refEventTarget(); }
        void deref() { derefEventTarget(); }

    protected:
        virtual ~EventTarget();

    private:
        virtual void refEventTarget() = 0;
        virtual void derefEventTarget() = 0;
    };
}

#endif
