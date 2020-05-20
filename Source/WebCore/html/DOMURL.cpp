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

#include "ActiveDOMObject.h"
#include "Blob.h"
#include "BlobURL.h"
#include "MemoryCache.h"
#include "PublicURLManager.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "URLSearchParams.h"
#include <wtf/MainThread.h>

namespace WebCore {

inline DOMURL::DOMURL(URL&& completeURL, const URL& baseURL)
    : m_baseURL(baseURL)
    , m_url(WTFMove(completeURL))
{
}

ExceptionOr<Ref<DOMURL>> DOMURL::create(const String& url, const URL& base)
{
    if (!base.isValid())
        return Exception { TypeError };
    URL completeURL { base, url };
    if (!completeURL.isValid())
        return Exception { TypeError };
    return adoptRef(*new DOMURL(WTFMove(completeURL), base));
}

ExceptionOr<Ref<DOMURL>> DOMURL::create(const String& url, const String& base)
{
    return create(url, URL { URL { }, base });
}

ExceptionOr<Ref<DOMURL>> DOMURL::create(const String& url, const DOMURL& base)
{
    return create(url, base.href());
}

ExceptionOr<Ref<DOMURL>> DOMURL::create(const String& url)
{
    URL baseURL { aboutBlankURL() };
    URL completeURL { baseURL, url };
    if (!completeURL.isValid())
        return Exception { TypeError };
    return adoptRef(*new DOMURL(WTFMove(completeURL), WTFMove(baseURL)));
}

DOMURL::~DOMURL()
{
    if (m_searchParams)
        m_searchParams->associatedURLDestroyed();
}

ExceptionOr<void> DOMURL::setHref(const String& url)
{
    URL completeURL { m_baseURL, url };
    if (!completeURL.isValid())
        return Exception { TypeError };
    m_url = WTFMove(completeURL);
    if (m_searchParams)
        m_searchParams->updateFromAssociatedURL();
    return { };
}

void DOMURL::setQuery(const String& query)
{
    m_url.setQuery(query);
}

String DOMURL::createObjectURL(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
{
    return createPublicURL(scriptExecutionContext, blob);
}

String DOMURL::createPublicURL(ScriptExecutionContext& scriptExecutionContext, URLRegistrable& registrable)
{
    URL publicURL = BlobURL::createPublicURL(scriptExecutionContext.securityOrigin());
    if (publicURL.isEmpty())
        return String();

    scriptExecutionContext.publicURLManager().registerURL(publicURL, registrable);

    return publicURL.string();
}

URLSearchParams& DOMURL::searchParams()
{
    if (!m_searchParams)
        m_searchParams = URLSearchParams::create(search(), this);
    return *m_searchParams;
}
    
void DOMURL::revokeObjectURL(ScriptExecutionContext& scriptExecutionContext, const String& urlString)
{
    URL url(URL(), urlString);
    ResourceRequest request(url);
    request.setDomainForCachePartition(scriptExecutionContext.domainForCachePartition());

    MemoryCache::removeRequestFromSessionCaches(scriptExecutionContext, request);

    scriptExecutionContext.publicURLManager().revoke(url);
}

} // namespace WebCore
