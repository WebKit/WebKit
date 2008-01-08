/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SecurityOrigin.h"

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "KURL.h"
#include "PlatformString.h"
#include "SecurityOriginData.h"

namespace WebCore {

SecurityOrigin::SecurityOrigin(const KURL& url)
    : m_port(0)
    , m_portSet(false)
    , m_noAccess(false)
    , m_domainWasSetInDOM(false)
{
    if (url.isEmpty())
      return;

    m_protocol = url.protocol().lower();

    // These protocols do not represent principals.
    if (m_protocol == "about" || m_protocol == "javascript")
        m_protocol = String();

    if (m_protocol.isEmpty())
        return;

    // data: URLs are not allowed access to anything other than themselves.
    if (m_protocol == "data")
        m_noAccess = true;

    m_host = url.host().lower();
    m_port = url.port();

    if (m_port)
        m_portSet = true;
}

bool SecurityOrigin::isEmpty() const
{
    return m_protocol.isEmpty();
}

PassRefPtr<SecurityOrigin> SecurityOrigin::createForFrame(Frame* frame)
{
    if (!frame)
        return new SecurityOrigin(KURL());

    FrameLoader* loader = frame->loader();

    RefPtr<SecurityOrigin> origin = new SecurityOrigin(loader->url());
    if (!origin->isEmpty())
        return origin;

    // If we do not obtain a principal from the URL, then we try to find a
    // principal via the frame hierarchy.

    Frame* openerFrame = frame->tree()->parent();
    if (!openerFrame) {
        openerFrame = loader->opener();
        if (!openerFrame)
            return origin;
    }

    Document* openerDocument = openerFrame->document();
    if (!openerDocument)
        return origin;

    // We alias the SecurityOrigins to match Firefox, see Bug 15313
    // http://bugs.webkit.org/show_bug.cgi?id=15313
    return openerDocument->securityOrigin();
}

void SecurityOrigin::setDomainFromDOM(const String& newDomain)
{
    m_domainWasSetInDOM = true;
    m_host = newDomain.lower();
}

bool SecurityOrigin::canAccess(const SecurityOrigin* other) const
{
    if (FrameLoader::shouldTreatSchemeAsLocal(m_protocol))
        return true;

    if (m_noAccess || other->m_noAccess)
        return false;

    // Here are two cases where we should permit access:
    //
    // 1) Neither document has set document.domain.  In this case, we insist
    //    that the scheme, host, and port of the URLs match.
    //
    // 2) Both documents have set document.domain.  In this case, we insist
    //    that the documents have set document.domain to the same value and
    //    that the scheme of the URLs match.
    //
    // This matches the behavior of Firefox 2 and Internet Explorer 6.
    //
    // Internet Explorer 7 and Opera 9 are more strict in that they require
    // the port numbers to match when both pages have document.domain set.
    //
    // FIXME: Evaluate whether we can tighten this policy to require matched
    //        port numbers.
    //
    // Opera 9 allows access when only one page has set document.domain, but
    // this is a security vulnerability.

    if (m_protocol == other->m_protocol) {
        if (!m_domainWasSetInDOM && !other->m_domainWasSetInDOM) {
            if (m_host == other->m_host && m_port == other->m_port)
                return true;
        }
        if (m_domainWasSetInDOM && other->m_domainWasSetInDOM) {
            if (m_host == other->m_host)
                return true;
        }
    }

    return false;
}

bool SecurityOrigin::isSecureTransitionTo(const KURL& url) const
{ 
    // New window created by the application
    if (isEmpty())
        return true;

    if (FrameLoader::shouldTreatSchemeAsLocal(m_protocol))
        return true;

    return equalIgnoringCase(m_host, String(url.host())) && equalIgnoringCase(m_protocol, String(url.protocol())) && m_port == url.port();
}

String SecurityOrigin::toString() const
{
    return m_protocol + ":" + m_host + ":" + String::number(m_port);
}

SecurityOriginData SecurityOrigin::securityOriginData() const
{
    return SecurityOriginData(m_protocol, m_host, m_port);
}

} // namespace WebCore
