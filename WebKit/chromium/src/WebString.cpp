/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "config.h"
#include "WebString.h"

#include "AtomicString.h"
#include "CString.h"
#include "PlatformString.h"

#include "WebCString.h"

namespace WebKit {

class WebStringPrivate : public WebCore::StringImpl {
};

void WebString::reset()
{
    if (m_private) {
        m_private->deref();
        m_private = 0;
    }
}

void WebString::assign(const WebString& other)
{
    assign(const_cast<WebStringPrivate*>(other.m_private));
}

void WebString::assign(const WebUChar* data, size_t length)
{
    assign(static_cast<WebStringPrivate*>(
        WebCore::StringImpl::create(data, length).get()));
}

size_t WebString::length() const
{
    return m_private ? const_cast<WebStringPrivate*>(m_private)->length() : 0;
}

const WebUChar* WebString::data() const
{
    return m_private ? const_cast<WebStringPrivate*>(m_private)->characters() : 0;
}

WebCString WebString::utf8() const
{
    return WebCore::String(m_private).utf8();
}

WebString WebString::fromUTF8(const char* data, size_t length)
{
    return WebCore::String::fromUTF8(data, length);
}

WebString WebString::fromUTF8(const char* data)
{
    return WebCore::String::fromUTF8(data);
}

WebString::WebString(const WebCore::String& s)
    : m_private(static_cast<WebStringPrivate*>(s.impl()))
{
    if (m_private)
        m_private->ref();
}

WebString& WebString::operator=(const WebCore::String& s)
{
    assign(static_cast<WebStringPrivate*>(s.impl()));
    return *this;
}

WebString::operator WebCore::String() const
{
    return m_private;
}

WebString::WebString(const WebCore::AtomicString& s)
    : m_private(0)
{
    assign(s.string());
}

WebString& WebString::operator=(const WebCore::AtomicString& s)
{
    assign(s.string());
    return *this;
}

WebString::operator WebCore::AtomicString() const
{
    return WebCore::AtomicString(static_cast<WebCore::StringImpl *>(m_private));
}

void WebString::assign(WebStringPrivate* p)
{
    // Take care to handle the case where m_private == p
    if (p)
        p->ref();
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit
