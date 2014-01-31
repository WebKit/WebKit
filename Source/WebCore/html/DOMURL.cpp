/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2014 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "DOMURL.h"

#include "SecurityOrigin.h"

#if ENABLE(BLOB)
#include "ActiveDOMObject.h"
#include "Blob.h"
#include "BlobURL.h"
#include "MemoryCache.h"
#include "PublicURLManager.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include <wtf/MainThread.h>
#endif // ENABLE(BLOB)

namespace WebCore {

PassRefPtr<DOMURL> DOMURL::create(const String& url, const String& base, ExceptionCode& ec) 
{
    return adoptRef(new DOMURL(url, base, ec)); 
}

PassRefPtr<DOMURL> DOMURL::create(const String& url, const DOMURL* base, ExceptionCode& ec) 
{
    ASSERT(base);
    return adoptRef(new DOMURL(url, *base, ec)); 
}

PassRefPtr<DOMURL> DOMURL::create(const String& url, ExceptionCode& ec) 
{
    return adoptRef(new DOMURL(url, ec)); 
}

inline DOMURL::DOMURL(const String& url, const String& base, ExceptionCode& ec)
    : m_baseURL(URL(), base)
    , m_url(m_baseURL, url)
{
    if (!m_baseURL.isValid() || !m_url.isValid())
        ec = TypeError;
}

inline DOMURL::DOMURL(const String& url, const DOMURL& base, ExceptionCode& ec)
    : m_baseURL(base.href())
    , m_url(m_baseURL, url)
{
    if (!m_baseURL.isValid() || !m_url.isValid())
        ec = TypeError;
}

inline DOMURL::DOMURL(const String& url, ExceptionCode& ec)
    : m_baseURL(blankURL())
    , m_url(m_baseURL, url)
{
    if (!m_url.isValid())
        ec = TypeError;
}

const URL& DOMURL::href() const
{
    return m_url;
}

void DOMURL::setHref(const String& url)
{
    m_url = URL(m_baseURL, url);
}

void DOMURL::setHref(const String& url, ExceptionCode& ec)
{
    setHref(url);
    if (!m_url.isValid())
        ec = TypeError;
}

const String& DOMURL::toString() const
{
    return href().string();
}

String DOMURL::origin() const
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(href());
    return origin->toString();
}

String DOMURL::protocol() const
{
    return href().protocol() + ':';
}

void DOMURL::setProtocol(const String& value)
{
    URL url = href();
    url.setProtocol(value);
    setHref(url.string());
}

String DOMURL::username() const
{
    return href().user();
}

void DOMURL::setUsername(const String& user)
{
    URL url = href();
    url.setUser(user);
    setHref(url);
}

String DOMURL::password() const
{
    return href().pass();
}

void DOMURL::setPassword(const String& pass)
{
    URL url = href();
    url.setPass(pass);
    setHref(url);
}

String DOMURL::host() const
{
    const URL& url = href();
    if (url.hostEnd() == url.pathStart())
        return url.host();
    if (isDefaultPortForProtocol(url.port(), url.protocol()))
        return url.host();
    return url.host() + ':' + String::number(url.port());
}

// This function does not allow leading spaces before the port number.
static unsigned parsePortFromStringPosition(const String& value, unsigned portStart, unsigned& portEnd)
{
    portEnd = portStart;
    while (isASCIIDigit(value[portEnd]))
        ++portEnd;
    return value.substring(portStart, portEnd - portStart).toUInt();
}

void DOMURL::setHost(const String& value)
{
    if (value.isEmpty())
        return;
    URL url = href();
    if (!url.canSetHostOrPort())
        return;

    size_t separator = value.find(':');
    if (!separator)
        return;

    if (separator == notFound)
        url.setHostAndPort(value);
    else {
        unsigned portEnd;
        unsigned port = parsePortFromStringPosition(value, separator + 1, portEnd);
        if (!port) {
            // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
            // specifically goes against RFC 3986 (p3.2) and
            // requires setting the port to "0" if it is set to empty string.
            url.setHostAndPort(value.substring(0, separator + 1) + '0');
        } else {
            if (isDefaultPortForProtocol(port, url.protocol()))
                url.setHostAndPort(value.substring(0, separator));
            else
                url.setHostAndPort(value.substring(0, portEnd));
        }
    }
    setHref(url.string());
}

String DOMURL::hostname() const
{
    return href().host();
}

void DOMURL::setHostname(const String& value)
{
    // Before setting new value:
    // Remove all leading U+002F SOLIDUS ("/") characters.
    unsigned i = 0;
    unsigned hostLength = value.length();
    while (value[i] == '/')
        i++;

    if (i == hostLength)
        return;

    URL url = href();
    if (!url.canSetHostOrPort())
        return;

    url.setHost(value.substring(i));
    setHref(url.string());
}

String DOMURL::port() const
{
    if (href().hasPort())
        return String::number(href().port());

    return emptyString();
}

void DOMURL::setPort(const String& value)
{
    URL url = href();
    if (!url.canSetHostOrPort())
        return;

    // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
    // specifically goes against RFC 3986 (p3.2) and
    // requires setting the port to "0" if it is set to empty string.
    // FIXME: http://url.spec.whatwg.org/ doesn't appear to require this; test what browsers do
    unsigned port = value.toUInt();
    if (isDefaultPortForProtocol(port, url.protocol()))
        url.removePort();
    else
        url.setPort(port);

    setHref(url.string());
}

String DOMURL::pathname() const
{
    return href().path();
}

void DOMURL::setPathname(const String& value)
{
    URL url = href();
    if (!url.canSetPathname())
        return;

    if (value[0] == '/')
        url.setPath(value);
    else
        url.setPath("/" + value);

    setHref(url.string());
}

String DOMURL::search() const
{
    String query = href().query();
    return query.isEmpty() ? emptyString() : "?" + query;
}

void DOMURL::setSearch(const String& value)
{
    URL url = href();
    String newSearch = (value[0] == '?') ? value.substring(1) : value;
    // Make sure that '#' in the query does not leak to the hash.
    url.setQuery(newSearch.replaceWithLiteral('#', "%23"));

    setHref(url.string());
}

String DOMURL::hash() const
{
    String fragmentIdentifier = href().fragmentIdentifier();
    if (fragmentIdentifier.isEmpty())
        return emptyString();
    return AtomicString(String("#" + fragmentIdentifier));
}

void DOMURL::setHash(const String& value)
{
    URL url = href();
    if (value[0] == '#')
        url.setFragmentIdentifier(value.substring(1));
    else
        url.setFragmentIdentifier(value);
    setHref(url.string());
}

#if ENABLE(BLOB)

String DOMURL::createObjectURL(ScriptExecutionContext* scriptExecutionContext, Blob* blob)
{
    if (!scriptExecutionContext || !blob)
        return String();
    return createPublicURL(scriptExecutionContext, blob);
}

String DOMURL::createPublicURL(ScriptExecutionContext* scriptExecutionContext, URLRegistrable* registrable)
{
    URL publicURL = BlobURL::createPublicURL(scriptExecutionContext->securityOrigin());
    if (publicURL.isEmpty())
        return String();

    scriptExecutionContext->publicURLManager().registerURL(scriptExecutionContext->securityOrigin(), publicURL, registrable);

    return publicURL.string();
}

void DOMURL::revokeObjectURL(ScriptExecutionContext* scriptExecutionContext, const String& urlString)
{
    if (!scriptExecutionContext)
        return;

    URL url(URL(), urlString);
    ResourceRequest request(url);
#if ENABLE(CACHE_PARTITIONING)
    request.setCachePartition(scriptExecutionContext->topOrigin()->cachePartition());
#endif
    MemoryCache::removeRequestFromCache(scriptExecutionContext, request);

    scriptExecutionContext->publicURLManager().revoke(url);
}

#endif // ENABLE(BLOB)

} // namespace WebCore
