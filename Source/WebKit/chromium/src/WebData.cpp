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
#include "platform/WebData.h"

#include "SharedBuffer.h"

using namespace WebCore;

namespace WebKit {

class WebDataPrivate : public SharedBuffer {
};

void WebData::reset()
{
    if (m_private) {
        m_private->deref();
        m_private = 0;
    }
}

void WebData::assign(const WebData& other)
{
    WebDataPrivate* p = const_cast<WebDataPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

void WebData::assign(const char* data, size_t size)
{
    assign(static_cast<WebDataPrivate*>(
        SharedBuffer::create(data, size).leakRef()));
}

size_t WebData::size() const
{
    if (!m_private)
        return 0;
    return const_cast<WebDataPrivate*>(m_private)->size();
}

const char* WebData::data() const
{
    if (!m_private)
        return 0;
    return const_cast<WebDataPrivate*>(m_private)->data();
}

WebData::WebData(const PassRefPtr<SharedBuffer>& buffer)
    : m_private(static_cast<WebDataPrivate*>(buffer.leakRef()))
{
}

WebData& WebData::operator=(const PassRefPtr<SharedBuffer>& buffer)
{
    assign(static_cast<WebDataPrivate*>(buffer.leakRef()));
    return *this;
}

WebData::operator PassRefPtr<SharedBuffer>() const
{
    return PassRefPtr<SharedBuffer>(const_cast<WebDataPrivate*>(m_private));
}

void WebData::assign(WebDataPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit
