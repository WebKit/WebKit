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

namespace WebCore {

SecurityOrigin::SecurityOrigin()
    : m_port(0)
    , m_portSet(false)
    , m_noAccess(false)
    , m_domainWasSetInDOM(false)
{
}

void SecurityOrigin::clear()
{
    m_protocol = String();
    m_host = String();
    m_port = 0;
    m_portSet = false;
    m_noAccess = false;
    m_domainWasSetInDOM = false;
}

bool SecurityOrigin::isEmpty() const
{
    return m_protocol.isEmpty();
}

void SecurityOrigin::setForFrame(Frame* frame)
{
    clear();

    FrameLoader* loader = frame->loader();
    const KURL& securityPolicyURL = loader->url();

    if (!securityPolicyURL.isEmpty()) {
        m_protocol = securityPolicyURL.protocol().lower();
        m_host = securityPolicyURL.host().lower();
        m_port = securityPolicyURL.port();
        if (m_port)
            m_portSet = true;

        // data: URLs are not allowed access to anything other than themselves.
        if (m_protocol == "data") {
            m_noAccess = true;
            return;
        }

        // Only in the case of about:blank or javascript: URLs (which create documents using the "about" 
        // protocol) do we want to use the parent or openers URL as the origin.
        if (m_protocol != "about")
            return;
    }

    Frame* openerFrame = frame->tree()->parent();
    if (!openerFrame) {
        openerFrame = loader->opener();
        if (!openerFrame)
            return;
    }

    Document* openerDocument = openerFrame->document();
    if (!openerDocument)
        return;

    *this = openerDocument->securityOrigin();
}

void SecurityOrigin::setDomainFromDOM(const String& newDomain)
{
    m_domainWasSetInDOM = true;
    m_host = newDomain.lower();
}

bool SecurityOrigin::allowsAccessFrom(const SecurityOrigin& other) const
{
    if (m_protocol == "file")
        return true;

    if (m_noAccess || other.m_noAccess)
        return false;

    if (m_domainWasSetInDOM && other.m_domainWasSetInDOM && m_host == other.m_host)
        return true;
 
    return m_host == other.m_host && m_protocol == other.m_protocol && m_port == other.m_port;
}

bool SecurityOrigin::isSecureTransitionTo(const KURL& url) const
{ 
    // New window created by the application
    if (isEmpty())
        return true;

    if (m_protocol == "file")
        return true;

    return equalIgnoringCase(m_host, String(url.host())) && equalIgnoringCase(m_protocol, String(url.protocol())) && m_port == url.port();
}

} // namespace WebCore
