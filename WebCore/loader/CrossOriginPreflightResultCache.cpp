/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "CrossOriginPreflightResultCache.h"

#include "CrossOriginAccessControl.h"
#include "ResourceResponse.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

// These values are at the discretion of the user agent.
static const unsigned defaultPreflightCacheTimeoutSeconds = 5;
static const unsigned maxPreflightCacheTimeoutSeconds = 600; // Should be short enough to minimize the risk of using a poisoned cache after switching to a secure network.

static bool parseAccessControlMaxAge(const String& string, unsigned& expiryDelta)
{
    // FIXME: this will not do the correct thing for a number starting with a '+'
    bool ok = false;
    expiryDelta = string.toUIntStrict(&ok);
    return ok;
}

template<class HashType>
static void addToAccessControlAllowList(const String& string, unsigned start, unsigned end, HashSet<String, HashType>& set)
{
    StringImpl* stringImpl = string.impl();
    if (!stringImpl)
        return;

    // Skip white space from start.
    while (start <= end && isSpaceOrNewline((*stringImpl)[start]))
        ++start;

    // only white space
    if (start > end) 
        return;

    // Skip white space from end.
    while (end && isSpaceOrNewline((*stringImpl)[end]))
        --end;

    // substringCopy() is called on the strings because the cache is accessed on multiple threads.
    set.add(string.substringCopy(start, end - start + 1));
}

template<class HashType>
static bool parseAccessControlAllowList(const String& string, HashSet<String, HashType>& set)
{
    int start = 0;
    int end;
    while ((end = string.find(',', start)) != -1) {
        if (start == end)
            return false;

        addToAccessControlAllowList(string, start, end - 1, set);
        start = end + 1;
    }
    if (start != static_cast<int>(string.length()))
        addToAccessControlAllowList(string, start, string.length() - 1, set);

    return true;
}

bool CrossOriginPreflightResultCacheItem::parse(const ResourceResponse& response)
{
    m_methods.clear();
    if (!parseAccessControlAllowList(response.httpHeaderField("Access-Control-Allow-Methods"), m_methods))
        return false;

    m_headers.clear();
    if (!parseAccessControlAllowList(response.httpHeaderField("Access-Control-Allow-Headers"), m_headers))
        return false;

    unsigned expiryDelta;
    if (parseAccessControlMaxAge(response.httpHeaderField("Access-Control-Max-Age"), expiryDelta)) {
        if (expiryDelta > maxPreflightCacheTimeoutSeconds)
            expiryDelta = maxPreflightCacheTimeoutSeconds;
    } else
        expiryDelta = defaultPreflightCacheTimeoutSeconds;

    m_absoluteExpiryTime = currentTime() + expiryDelta;
    return true;
}

bool CrossOriginPreflightResultCacheItem::allowsCrossOriginMethod(const String& method) const
{
    return m_methods.contains(method) || isOnAccessControlSimpleRequestMethodWhitelist(method);
}

bool CrossOriginPreflightResultCacheItem::allowsCrossOriginHeaders(const HTTPHeaderMap& requestHeaders) const
{
    HTTPHeaderMap::const_iterator end = requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
        if (!m_headers.contains(it->first) && !isOnAccessControlSimpleRequestHeaderWhitelist(it->first, it->second))
            return false;
    }
    return true;
}

bool CrossOriginPreflightResultCacheItem::allowsRequest(bool includeCredentials, const String& method, const HTTPHeaderMap& requestHeaders) const
{
    if (m_absoluteExpiryTime < currentTime())
        return false;
    if (includeCredentials && !m_credentials)
        return false;
    if (!allowsCrossOriginMethod(method))
        return false;
    if (!allowsCrossOriginHeaders(requestHeaders))
        return false;
    return true;
}

CrossOriginPreflightResultCache& CrossOriginPreflightResultCache::shared()
{
    AtomicallyInitializedStatic(CrossOriginPreflightResultCache&, cache = *new CrossOriginPreflightResultCache);
    return cache;
}

void CrossOriginPreflightResultCache::appendEntry(const String& origin, const KURL& url, CrossOriginPreflightResultCacheItem* preflightResult)
{
    MutexLocker lock(m_mutex);
    // Note that the entry may already be present in the HashMap if another thread is accessing the same location.
    m_preflightHashMap.set(std::make_pair(origin.copy(), url.copy()), preflightResult);
}

bool CrossOriginPreflightResultCache::canSkipPreflight(const String& origin, const KURL& url, bool includeCredentials, const String& method, const HTTPHeaderMap& requestHeaders)
{
    MutexLocker lock(m_mutex);
    CrossOriginPreflightResultHashMap::iterator cacheIt = m_preflightHashMap.find(std::make_pair(origin, url));
    if (cacheIt == m_preflightHashMap.end())
        return false;

    if (cacheIt->second->allowsRequest(includeCredentials, method, requestHeaders))
        return true;

    delete cacheIt->second;
    m_preflightHashMap.remove(cacheIt);
    return false;
}

void CrossOriginPreflightResultCache::empty()
{
    MutexLocker lock(m_mutex);
    deleteAllValues(m_preflightHashMap);
    m_preflightHashMap.clear();
}

} // namespace WebCore
