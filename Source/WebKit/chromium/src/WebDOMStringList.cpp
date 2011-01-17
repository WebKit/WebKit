/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebDOMStringList.h"

#include "DOMStringList.h"
#include "WebString.h"

using namespace WebCore;

namespace WebKit {

WebDOMStringList::WebDOMStringList()
{
    m_private = WebCore::DOMStringList::create();
}

void WebDOMStringList::reset()
{
    m_private.reset();
}

void WebDOMStringList::assign(const WebDOMStringList& other)
{
    m_private = other.m_private;
}

void WebDOMStringList::append(const WebString& string)
{
    m_private->append(string);
}

unsigned WebDOMStringList::length() const
{
    if (m_private.isNull())
        return 0;
    return m_private->length();
}

WebString WebDOMStringList::item(unsigned index) const
{
    return m_private->item(index);
}

WebDOMStringList::WebDOMStringList(const WTF::PassRefPtr<WebCore::DOMStringList>& item)
    : m_private(item)
{
}

WebDOMStringList& WebDOMStringList::operator=(const WTF::PassRefPtr<WebCore::DOMStringList>& item)
{
    m_private = item;
    return *this;
}

WebDOMStringList::operator WTF::PassRefPtr<WebCore::DOMStringList>() const
{
    return m_private.get();
}

} // namespace WebKit
