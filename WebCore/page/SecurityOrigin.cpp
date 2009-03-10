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

#include "CString.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "PlatformString.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

static bool isDefaultPortForProtocol(unsigned short port, const String& protocol)
{
    if (protocol.isEmpty())
        return false;

    typedef HashMap<String, unsigned> DefaultPortsMap;
    DEFINE_STATIC_LOCAL(DefaultPortsMap, defaultPorts, ());
    if (defaultPorts.isEmpty()) {
        defaultPorts.set("http", 80);
        defaultPorts.set("https", 443);
        defaultPorts.set("ftp", 21);
        defaultPorts.set("ftps", 990);
    }
    return defaultPorts.get(protocol) == port;
}

SecurityOrigin::SecurityOrigin(const KURL& url)
    : m_protocol(url.protocol().isNull() ? "" : url.protocol().lower())
    , m_host(url.host().isNull() ? "" : url.host().lower())
    , m_port(url.port())
    , m_noAccess(false)
    , m_universalAccess(false)
    , m_domainWasSetInDOM(false)
{
    // These protocols do not create security origins; the owner frame provides the origin
    if (m_protocol == "about" || m_protocol == "javascript")
        m_protocol = "";

    // Some URLs are not allowed access to anything other than themselves.
    if (FrameLoader::shouldTreatURLSchemeAsNoAccess(m_protocol))
        m_noAccess = true;

    // document.domain starts as m_host, but can be set by the DOM.
    m_domain = m_host;

    // By default, only local SecurityOrigins can load local resources.
    m_canLoadLocalResources = isLocal();

    if (isDefaultPortForProtocol(m_port, m_protocol))
        m_port = 0;
}

SecurityOrigin::SecurityOrigin(const SecurityOrigin* other)
    : m_protocol(other->m_protocol.copy())
    , m_host(other->m_host.copy())
    , m_domain(other->m_domain.copy())
    , m_port(other->m_port)
    , m_noAccess(other->m_noAccess)
    , m_universalAccess(other->m_universalAccess)
    , m_domainWasSetInDOM(other->m_domainWasSetInDOM)
    , m_canLoadLocalResources(other->m_canLoadLocalResources)
{
}

bool SecurityOrigin::isEmpty() const
{
    return m_protocol.isEmpty();
}

PassRefPtr<SecurityOrigin> SecurityOrigin::create(const KURL& url)
{
    if (!url.isValid())
        return adoptRef(new SecurityOrigin(KURL()));
    return adoptRef(new SecurityOrigin(url));
}

PassRefPtr<SecurityOrigin> SecurityOrigin::createEmpty()
{
    return create(KURL());
}

PassRefPtr<SecurityOrigin> SecurityOrigin::copy()
{
    return adoptRef(new SecurityOrigin(this));
}

void SecurityOrigin::setDomainFromDOM(const String& newDomain)
{
    m_domainWasSetInDOM = true;
    m_domain = newDomain.lower();
}

bool SecurityOrigin::canAccess(const SecurityOrigin* other) const
{  
    if (m_universalAccess)
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
        } else if (m_domainWasSetInDOM && other->m_domainWasSetInDOM) {
            if (m_domain == other->m_domain)
                return true;
        }
    }
    
    return false;
}

bool SecurityOrigin::canRequest(const KURL& url) const
{
    if (m_universalAccess)
        return true;

    if (m_noAccess)
        return false;

    RefPtr<SecurityOrigin> targetOrigin = SecurityOrigin::create(url);

    // We call isSameSchemeHostPort here instead of canAccess because we want
    // to ignore document.domain effects.
    return isSameSchemeHostPort(targetOrigin.get());
}

void SecurityOrigin::grantLoadLocalResources()
{
    // This method exists only to support backwards compatibility with older
    // versions of WebKit.  Granting privileges to some, but not all, documents
    // in a SecurityOrigin is a security hazard because the documents without
    // the privilege can obtain the privilege by injecting script into the
    // documents that have been granted the privilege.
    ASSERT(FrameLoader::allowSubstituteDataAccessToLocal());
    m_canLoadLocalResources = true;
}

void SecurityOrigin::grantUniversalAccess()
{
    m_universalAccess = true;
}

bool SecurityOrigin::isLocal() const
{
    return FrameLoader::shouldTreatURLSchemeAsLocal(m_protocol);
}

bool SecurityOrigin::isSecureTransitionTo(const KURL& url) const
{ 
    // New window created by the application
    if (isEmpty())
        return true;

    RefPtr<SecurityOrigin> other = SecurityOrigin::create(url);
    return canAccess(other.get());
}

String SecurityOrigin::toString() const
{
    if (isEmpty())
        return "null";

    if (m_noAccess)
        return "null";

    if (m_protocol == "file")
        return String("file://");

    Vector<UChar> result;
    result.reserveInitialCapacity(m_protocol.length() + m_host.length() + 10);
    append(result, m_protocol);
    append(result, "://");
    append(result, m_host);

    if (m_port) {
        append(result, ":");
        append(result, String::number(m_port));
    }

    return String::adopt(result);
}

PassRefPtr<SecurityOrigin> SecurityOrigin::createFromString(const String& originString)
{
    return SecurityOrigin::create(KURL(KURL(), originString));
}

static const char SeparatorCharacter = '_';

PassRefPtr<SecurityOrigin> SecurityOrigin::createFromDatabaseIdentifier(const String& databaseIdentifier)
{ 
    // Make sure there's a first separator
    int separator1 = databaseIdentifier.find(SeparatorCharacter);
    if (separator1 == -1)
        return create(KURL());
        
    // Make sure there's a second separator
    int separator2 = databaseIdentifier.find(SeparatorCharacter, separator1 + 1);
    if (separator2 == -1)
        return create(KURL());
        
    // Make sure there's not a third separator
    if (databaseIdentifier.reverseFind(SeparatorCharacter) != separator2)
        return create(KURL());
        
    // Make sure the port section is a valid port number or doesn't exist
    bool portOkay;
    int port = databaseIdentifier.right(databaseIdentifier.length() - separator2 - 1).toInt(&portOkay);
    if (!portOkay && separator2 + 1 == static_cast<int>(databaseIdentifier.length()))
        return create(KURL());
    
    if (port < 0 || port > 65535)
        return create(KURL());
        
    // Split out the 3 sections of data
    String protocol = databaseIdentifier.substring(0, separator1);
    String host = databaseIdentifier.substring(separator1 + 1, separator2 - separator1 - 1);
    return create(KURL(KURL(), protocol + "://" + host + ":" + String::number(port)));
}

String SecurityOrigin::databaseIdentifier() const 
{
    DEFINE_STATIC_LOCAL(String, separatorString, (&SeparatorCharacter, 1));
    return m_protocol + separatorString + m_host + separatorString + String::number(m_port); 
}

bool SecurityOrigin::equal(const SecurityOrigin* other) const 
{
    if (!isSameSchemeHostPort(other))
        return false;

    if (m_domainWasSetInDOM != other->m_domainWasSetInDOM)
        return false;

    if (m_domainWasSetInDOM && m_domain != other->m_domain)
        return false;

    return true;
}

bool SecurityOrigin::isSameSchemeHostPort(const SecurityOrigin* other) const 
{
    if (m_host != other->m_host)
        return false;

    if (m_protocol != other->m_protocol)
        return false;

    if (m_port != other->m_port)
        return false;

    return true;
}

} // namespace WebCore
