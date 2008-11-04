/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "MessageEvent.h"

#include "DOMWindow.h"
#include "EventNames.h"

namespace WebCore {

MessageEvent::MessageEvent()
{
}

MessageEvent::MessageEvent(const String& data, const String& origin, const String& lastEventId, PassRefPtr<DOMWindow> source, PassRefPtr<MessagePort> messagePort)
    : Event(eventNames().messageEvent, false, true)
    , m_data(data)
    , m_origin(origin)
    , m_lastEventId(lastEventId)
    , m_source(source)
    , m_messagePort(messagePort)
{
}

MessageEvent::~MessageEvent()
{
}

void MessageEvent::initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& data, const String& origin, const String& lastEventId, DOMWindow* source, MessagePort* messagePort)
{
    if (dispatched())
        return;
        
    initEvent(type, canBubble, cancelable);
    
    m_data = data;
    m_origin = origin;
    m_lastEventId = lastEventId;
    m_source = source;
    m_messagePort = messagePort;
}

bool MessageEvent::isMessageEvent() const 
{
    return true;
}

} // namespace WebCore
