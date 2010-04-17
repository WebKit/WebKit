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
#include "WebSecurityOrigin.h"

#include "KURL.h"
#include "SecurityOrigin.h"
#include "WebString.h"
#include "WebURL.h"
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

class WebSecurityOriginPrivate : public SecurityOrigin {
};

WebSecurityOrigin WebSecurityOrigin::createFromDatabaseIdentifier(const WebString& databaseIdentifier)
{
    return WebSecurityOrigin(SecurityOrigin::createFromDatabaseIdentifier(databaseIdentifier));
}

WebSecurityOrigin WebSecurityOrigin::createFromString(const WebString& origin)
{
    return WebSecurityOrigin(SecurityOrigin::createFromString(origin));
}

WebSecurityOrigin WebSecurityOrigin::create(const WebURL& url)
{
    return WebSecurityOrigin(SecurityOrigin::create(url));
}

void WebSecurityOrigin::reset()
{
    assign(0);
}

void WebSecurityOrigin::assign(const WebSecurityOrigin& other)
{
    WebSecurityOriginPrivate* p = const_cast<WebSecurityOriginPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

WebString WebSecurityOrigin::protocol() const
{
    ASSERT(m_private);
    return m_private->protocol();
}

WebString WebSecurityOrigin::host() const
{
    ASSERT(m_private);
    return m_private->host();
}

unsigned short WebSecurityOrigin::port() const
{
    ASSERT(m_private);
    return m_private->port();
}

bool WebSecurityOrigin::isEmpty() const
{
    ASSERT(m_private);
    return m_private->isEmpty();
}

bool WebSecurityOrigin::canAccess(const WebSecurityOrigin& other) const
{
    ASSERT(m_private);
    ASSERT(other.m_private);
    return m_private->canAccess(other.m_private);
}

bool WebSecurityOrigin::canRequest(const WebURL& url) const
{
    ASSERT(m_private);
    return m_private->canRequest(url);
}

WebString WebSecurityOrigin::toString() const
{
    ASSERT(m_private);
    return m_private->toString();
}

WebString WebSecurityOrigin::databaseIdentifier()
{
    ASSERT(m_private);
    return m_private->databaseIdentifier();
}

WebSecurityOrigin::WebSecurityOrigin(const WTF::PassRefPtr<WebCore::SecurityOrigin>& origin)
    : m_private(static_cast<WebSecurityOriginPrivate*>(origin.releaseRef()))
{
}

WebSecurityOrigin& WebSecurityOrigin::operator=(const WTF::PassRefPtr<WebCore::SecurityOrigin>& origin)
{
    assign(static_cast<WebSecurityOriginPrivate*>(origin.releaseRef()));
    return *this;
}

WebSecurityOrigin::operator WTF::PassRefPtr<WebCore::SecurityOrigin>() const
{
    return PassRefPtr<SecurityOrigin>(const_cast<WebSecurityOriginPrivate*>(m_private));
}

void WebSecurityOrigin::assign(WebSecurityOriginPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit
