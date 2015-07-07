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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "XMLHttpRequestUpload.h"

#include "Event.h"
#include "EventException.h"
#include "XMLHttpRequestProgressEvent.h"
#include <wtf/Assertions.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

XMLHttpRequestUpload::XMLHttpRequestUpload(XMLHttpRequest* xmlHttpRequest)
    : m_xmlHttpRequest(xmlHttpRequest)
    , m_lengthComputable(false)
    , m_loaded(0)
    , m_total(0)
{
}

void XMLHttpRequestUpload::dispatchThrottledProgressEvent(bool lengthComputable, unsigned long long loaded, unsigned long long total)
{
    m_lengthComputable = lengthComputable;
    m_loaded = loaded;
    m_total = total;

    dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().progressEvent, lengthComputable, loaded, total));
}

void XMLHttpRequestUpload::dispatchProgressEvent(const AtomicString &type)
{
    ASSERT(type == eventNames().loadstartEvent || type == eventNames().progressEvent || type == eventNames().loadEvent || type == eventNames().loadendEvent || type == eventNames().abortEvent || type == eventNames().errorEvent || type == eventNames().timeoutEvent);

    if (type == eventNames().loadstartEvent) {
        m_lengthComputable = false;
        m_loaded = 0;
        m_total = 0;
    }

    dispatchEvent(XMLHttpRequestProgressEvent::create(type, m_lengthComputable, m_loaded, m_total));
}


} // namespace WebCore
