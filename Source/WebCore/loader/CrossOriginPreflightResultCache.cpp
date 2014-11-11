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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "HTTPHeaderNames.h"
#include "ResourceResponse.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

// These values are at the discretion of the user agent.
static const auto defaultPreflightCacheTimeout = std::chrono::seconds(5);
static const auto maxPreflightCacheTimeout = std::chrono::seconds(600); // Should be short enough to minimize the risk of using a poisoned cache after switching to a secure network.

CrossOriginPreflightResultCache::CrossOriginPreflightResultCache()
{
}

static bool parseAccessControlMaxAge(const String& string, std::chrono::seconds& expiryDelta)
{
    // FIXME: this will not do the correct thing for a number starting with a '+'
    bool ok = false;
    expiryDelta = std::chrono::seconds(string.toUIntStrict(&ok));
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

    set.add(string.substring(start, end - start + 1));
}

template<class HashType>
static bool parseAccessControlAllowList(const String& string, HashSet<String, HashType>& set)
{
    unsigned start = 0;
    size_t end;
    while ((end = string.find(',', start)) != notFound) {
        if (start != end)
            addToAccessControlAllowList(string, start, end - 1, set);
        start = end + 1;
    }
    if (start != string.length())
        addToAccessControlAllowList(string, start, string.length() - 1, set);

    return true;
}

bool CrossOriginPreflightResultCacheItem::parse(const ResourceResponse& response, String& errorDescription)
{
    m_methods.clear();
    if (!parseAccessControlAllowList(response.httpHeaderField(HTTPHeaderName::AccessControlAllowMethods), m_methods)) {
        errorDescription = "Cannot parse Access-Control-Allow-Methods response header field.";
        return false;
    }

    m_headers.clear();
    if (!parseAccessControlAllowList(response.httpHeaderField(HTTPHeaderName::AccessControlAllowHeaders), m_headers)) {
        errorDescription = "Cannot parse Access-Control-Allow-Headers response header field.";
        return false;
    }

    std::chrono::seconds expiryDelta;
    if (parseAccessControlMaxAge(response.httpHeaderField(HTTPHeaderName::AccessControlMaxAge), expiryDelta)) {
        if (expiryDelta > maxPreflightCacheTimeout)
            expiryDelta = maxPreflightCacheTimeout;
    } else
        expiryDelta = defaultPreflightCacheTimeout;

    m_absoluteExpiryTime = std::chrono::steady_clock::now() + expiryDelta;
    return true;
}

bool CrossOriginPreflightResultCacheItem::allowsCrossOriginMethod(const String& method, String& errorDescription) const
{
    if (m_methods.contains(method) || isOnAccessControlSimpleRequestMethodWhitelist(method))
        return true;

    errorDescription = "Method " + method + " is not allowed by Access-Control-Allow-Methods.";
    return false;
}

bool CrossOriginPreflightResultCacheItem::allowsCrossOriginHeaders(const HTTPHeaderMap& requestHeaders, String& errorDescription) const
{
    for (const auto& header : requestHeaders) {
        if (header.keyAsHTTPHeaderName && isOnAccessControlSimpleRequestHeaderWhitelist(header.keyAsHTTPHeaderName.value(), header.value))
            continue;
        if (!m_headers.contains(header.key)) {
            errorDescription = "Request header field " + header.key + " is not allowed by Access-Control-Allow-Headers.";
            return false;
        }
    }
    return true;
}

bool CrossOriginPreflightResultCacheItem::allowsRequest(StoredCredentials includeCredentials, const String& method, const HTTPHeaderMap& requestHeaders) const
{
    String ignoredExplanation;
    if (m_absoluteExpiryTime < std::chrono::steady_clock::now())
        return false;
    if (includeCredentials == AllowStoredCredentials && m_credentials == DoNotAllowStoredCredentials)
        return false;
    if (!allowsCrossOriginMethod(method, ignoredExplanation))
        return false;
    if (!allowsCrossOriginHeaders(requestHeaders, ignoredExplanation))
        return false;
    return true;
}

CrossOriginPreflightResultCache& CrossOriginPreflightResultCache::shared()
{
    ASSERT(isMainThread());

    static NeverDestroyed<CrossOriginPreflightResultCache> cache;
    return cache;
}

void CrossOriginPreflightResultCache::appendEntry(const String& origin, const URL& url, std::unique_ptr<CrossOriginPreflightResultCacheItem> preflightResult)
{
    ASSERT(isMainThread());
    m_preflightHashMap.set(std::make_pair(origin, url), WTF::move(preflightResult));
}

bool CrossOriginPreflightResultCache::canSkipPreflight(const String& origin, const URL& url, StoredCredentials includeCredentials, const String& method, const HTTPHeaderMap& requestHeaders)
{
    ASSERT(isMainThread());
    auto it = m_preflightHashMap.find(std::make_pair(origin, url));
    if (it == m_preflightHashMap.end())
        return false;

    if (it->value->allowsRequest(includeCredentials, method, requestHeaders))
        return true;

    m_preflightHashMap.remove(it);
    return false;
}

void CrossOriginPreflightResultCache::empty()
{
    ASSERT(isMainThread());
    m_preflightHashMap.clear();
}

} // namespace WebCore
