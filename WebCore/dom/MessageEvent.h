/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef MessageEvent_h
#define MessageEvent_h

#include "Event.h"
#include "MessagePort.h"

namespace WebCore {

    class DOMWindow;

    class MessageEvent : public Event {
    public:
        static PassRefPtr<MessageEvent> create()
        {
            return adoptRef(new MessageEvent);
        }
        static PassRefPtr<MessageEvent> create(const String& data, const String& origin, const String& lastEventId, PassRefPtr<DOMWindow> source, PassRefPtr<MessagePort> messagePort)
        {
            return adoptRef(new MessageEvent(data, origin, lastEventId, source, messagePort));
        }
        virtual ~MessageEvent();

        void initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& data, const String& origin, const String& lastEventId, DOMWindow* source, MessagePort*);
        
        const String& data() const { return m_data; }
        const String& origin() const { return m_origin; }
        const String& lastEventId() const { return m_lastEventId; }
        DOMWindow* source() const { return m_source.get(); }
        MessagePort* messagePort() const { return m_messagePort.get(); }
        
        virtual bool isMessageEvent() const;

    private:    
        MessageEvent();
        MessageEvent(const String& data, const String& origin, const String& lastEventId, PassRefPtr<DOMWindow> source, PassRefPtr<MessagePort> messagePort);

        String m_data;
        String m_origin;
        String m_lastEventId;
        RefPtr<DOMWindow> m_source;
        RefPtr<MessagePort> m_messagePort;
    };

} // namespace WebCore

#endif // MessageEvent_h
