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

static bool isDefaultPortForProtocol(unsigned short port, String protocol)
{
    if (protocol.isEmpty())
        return false;

    static HashMap<String, unsigned> defaultPorts;
    if (defaultPorts.isEmpty()) {
        defaultPorts.set("http", 80);
        defaultPorts.set("https", 443);
        defaultPorts.set("ftp", 21);
        defaultPorts.set("ftps", 990);
    }
    return defaultPorts.get(protocol) == port;
}

SecurityOrigin::SecurityOrigin(const String& protocol, const String& host, unsigned short port)
    : m_protocol(protocol.isNull() ? "" : protocol.lower())
    , m_host(host.isNull() ? "" : host.lower())
    , m_port(port)
    , m_portSet(port)
    , m_noAccess(false)
    , m_domainWasSetInDOM(false)
{
    // These protocols do not create security origins; the owner frame provides the origin
    if (m_protocol == "about" || m_protocol == "javascript")
        m_protocol = "";

    // data: URLs are not allowed access to anything other than themselves.
    if (m_protocol == "data")
        m_noAccess = true;


    if (isDefaultPortForProtocol(m_port, m_protocol)) {
        m_port = 0;
        m_portSet = false;
    }   
}

bool SecurityOrigin::isEmpty() const
{
    return m_protocol.isEmpty();
}

PassRefPtr<SecurityOrigin> SecurityOrigin::create(const String& protocol, const String& host, unsigned short port, SecurityOrigin* ownerFrameOrigin)
{
    RefPtr<SecurityOrigin> origin = new SecurityOrigin(protocol, host, port);

    // If we do not obtain a meaningful origin from the URL, then we try to find one
    // via the frame hierarchy.
    // We alias the SecurityOrigins to match Firefox, see Bug 15313
    // http://bugs.webkit.org/show_bug.cgi?id=15313
    if (origin->isEmpty() && ownerFrameOrigin)
        return ownerFrameOrigin;

    return origin.release();
}

PassRefPtr<SecurityOrigin> SecurityOrigin::createForFrame(Frame* frame)
{
    if (!frame)
        return create("", "", 0, 0);

    FrameLoader* loader = frame->loader();
    KURL url = loader->url();

    Frame* ownerFrame = frame->tree()->parent();
    if (!ownerFrame)
        ownerFrame = loader->opener();

    SecurityOrigin* ownerFrameOrigin = 0;
    if (ownerFrame && ownerFrame->document())
        ownerFrameOrigin = ownerFrame->document()->securityOrigin();

    return create(url.protocol(), url.host(), url.port(), ownerFrameOrigin);
}

PassRefPtr<SecurityOrigin> SecurityOrigin::copy()
{
    return create(m_protocol.copy(), m_host.copy(), m_port, 0);
}


void SecurityOrigin::setDomainFromDOM(const String& newDomain)
{
    m_domainWasSetInDOM = true;
    m_host = newDomain.lower();
}

bool SecurityOrigin::canAccess(const SecurityOrigin* other, Reason& reason) const
{  
    if (FrameLoader::shouldTreatSchemeAsLocal(m_protocol))
        return true;

    if (m_noAccess || other->m_noAccess) {
        reason = SecurityOrigin::GenericMismatch;
        return false;
    }

    // Here are three cases where we should permit access:
    //
    // 1) Neither document has set document.domain.  In this case, we insist
    //    that the scheme, host, and port of the URLs match.
    //
    // 2) Both documents have set document.domain.  In this case, we insist
    //    that the documents have set document.domain to the same value and
    //    that the scheme of the URLs match.
    //
    // 3) As a special case if only one of the documents has set document.domain but
    //    there is a host and port match we deny access but signal this to the client. 
    //    In this case Window::allowsAccessFrom() will recheck against the lexical global
    //    object and allow access if that check passes.
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
        } else if (m_domainWasSetInDOM && other->m_domainWasSetInDOM) {
            if (m_host == other->m_host)
                return true;
        } else {
            if (m_host == other->m_host && m_port == other->m_port) {
                reason = DomainSetInDOMMismatch;
                return false;
            }
        }
    }
    
    reason = SecurityOrigin::GenericMismatch;
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

static const char SeparatorCharacter = '_';

PassRefPtr<SecurityOrigin> SecurityOrigin::createFromIdentifier(const String& stringIdentifier)
{ 
    // Make sure there's a first separator
    int separator1 = stringIdentifier.find(SeparatorCharacter);
    if (separator1 == -1)
        return create("", "", 0, 0);
        
    // Make sure there's a second separator
    int separator2 = stringIdentifier.find(SeparatorCharacter, separator1 + 1);
    if (separator2 == -1)
        return create("", "", 0, 0);
        
    // Make sure there's not a third separator
    if (stringIdentifier.reverseFind(SeparatorCharacter) != separator2)
        return create("", "", 0, 0);
        
    // Make sure the port section is a valid port number or doesn't exist
    bool portOkay;
    int port = stringIdentifier.right(stringIdentifier.length() - separator2 - 1).toInt(&portOkay);
    if (!portOkay && separator2 + 1 == static_cast<int>(stringIdentifier.length()))
        return create("", "", 0, 0);
    
    if (port < 0 || port > 65535)
        return create("", "", 0, 0);
        
    // Split out the 3 sections of data
    String protocol = stringIdentifier.substring(0, separator1);
    String host = stringIdentifier.substring(separator1 + 1, separator2 - separator1 - 1);
    return create(protocol, host, port, 0);
}
    
    
String SecurityOrigin::stringIdentifier() const 
{
    static String separatorString = String(&SeparatorCharacter, 1);
    return m_protocol + separatorString + m_host + separatorString + String::number(m_port); 
}

} // namespace WebCore
