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
#include "ActiveDOMObject.h"
#include "Blob.h"
#include "BlobURL.h"
#include "MemoryCache.h"
#include "PublicURLManager.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include <wtf/MainThread.h>

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
    MemoryCache::removeRequestFromSessionCaches(scriptExecutionContext, request);

    scriptExecutionContext->publicURLManager().revoke(url);
}

} // namespace WebCore
