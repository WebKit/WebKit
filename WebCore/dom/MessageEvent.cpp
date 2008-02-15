/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#if ENABLE(CROSS_DOCUMENT_MESSAGING)

#include "DOMWindow.h"
#include "EventNames.h"
#include "MessageEvent.h"

namespace WebCore {

using namespace EventNames;

MessageEvent::MessageEvent()
{
}

MessageEvent::MessageEvent(const String& data, const String& domain, const String& uri, DOMWindow* source)
    : Event(messageEvent, true, true)
    , m_data(data)
    , m_domain(domain)
    , m_uri(uri)
    , m_source(source)
{
}

MessageEvent::~MessageEvent()
{
}

void MessageEvent::initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& data, const String& domain, const String& uri, DOMWindow* source)
{
    if (dispatched())
        return;
        
    initEvent(type, canBubble, cancelable);
    
    m_data = data;
    m_domain = domain;
    m_uri = uri;
    m_source = source;
}

bool MessageEvent::isMessageEvent() const 
{
    return true;
}

} // namespace WebCore

#endif // ENABLE(CROSS_DOCUMENT_MESSAGING)
