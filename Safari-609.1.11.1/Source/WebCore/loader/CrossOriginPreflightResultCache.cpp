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
#include "HTTPParsers.h"
#include "ResourceResponse.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// These values are at the discretion of the user agent.
static const auto defaultPreflightCacheTimeout = 5_s;
static const auto maxPreflightCacheTimeout = 600_s; // Should be short enough to minimize the risk of using a poisoned cache after switching to a secure network.

CrossOriginPreflightResultCache::CrossOriginPreflightResultCache()
{
}

static bool parseAccessControlMaxAge(const String& string, Seconds& expiryDelta)
{
    // FIXME: this will not do the correct thing for a number starting with a '+'
    bool ok = false;
    expiryDelta = Seconds(static_cast<double>(string.toUIntStrict(&ok)));
    return ok;
}

bool CrossOriginPreflightResultCacheItem::parse(const ResourceResponse& response)
{
    m_methods = parseAccessControlAllowList(response.httpHeaderField(HTTPHeaderName::AccessControlAllowMethods));
    m_headers = parseAccessControlAllowList<ASCIICaseInsensitiveHash>(response.httpHeaderField(HTTPHeaderName::AccessControlAllowHeaders));

    Seconds expiryDelta = 0_s;
    if (parseAccessControlMaxAge(response.httpHeaderField(HTTPHeaderName::AccessControlMaxAge), expiryDelta)) {
        if (expiryDelta > maxPreflightCacheTimeout)
            expiryDelta = maxPreflightCacheTimeout;
    } else
        expiryDelta = defaultPreflightCacheTimeout;

    m_absoluteExpiryTime = MonotonicTime::now() + expiryDelta;
    return true;
}

bool CrossOriginPreflightResultCacheItem::allowsCrossOriginMethod(const String& method, StoredCredentialsPolicy storedCredentialsPolicy, String& errorDescription) const
{
    if (m_methods.contains(method) || (m_methods.contains("*") && storedCredentialsPolicy != StoredCredentialsPolicy::Use) || isOnAccessControlSimpleRequestMethodWhitelist(method))
        return true;

    errorDescription = "Method " + method + " is not allowed by Access-Control-Allow-Methods.";
    return false;
}

bool CrossOriginPreflightResultCacheItem::allowsCrossOriginHeaders(const HTTPHeaderMap& requestHeaders, StoredCredentialsPolicy storedCredentialsPolicy, String& errorDescription) const
{
    bool validWildcard = m_headers.contains("*") && storedCredentialsPolicy != StoredCredentialsPolicy::Use;
    for (const auto& header : requestHeaders) {
        if (header.keyAsHTTPHeaderName && isCrossOriginSafeRequestHeader(header.keyAsHTTPHeaderName.value(), header.value))
            continue;
        if (!m_headers.contains(header.key) && !validWildcard) {
            errorDescription = "Request header field " + header.key + " is not allowed by Access-Control-Allow-Headers.";
            return false;
        }
    }
    return true;
}

bool CrossOriginPreflightResultCacheItem::allowsRequest(StoredCredentialsPolicy storedCredentialsPolicy, const String& method, const HTTPHeaderMap& requestHeaders) const
{
    String ignoredExplanation;
    if (m_absoluteExpiryTime < MonotonicTime::now())
        return false;
    if (storedCredentialsPolicy == StoredCredentialsPolicy::Use && m_storedCredentialsPolicy == StoredCredentialsPolicy::DoNotUse)
        return false;
    if (!allowsCrossOriginMethod(method, storedCredentialsPolicy, ignoredExplanation))
        return false;
    if (!allowsCrossOriginHeaders(requestHeaders, storedCredentialsPolicy, ignoredExplanation))
        return false;
    return true;
}

CrossOriginPreflightResultCache& CrossOriginPreflightResultCache::singleton()
{
    ASSERT(isMainThread());

    static NeverDestroyed<CrossOriginPreflightResultCache> cache;
    return cache;
}

void CrossOriginPreflightResultCache::appendEntry(const String& origin, const URL& url, std::unique_ptr<CrossOriginPreflightResultCacheItem> preflightResult)
{
    ASSERT(isMainThread());
    m_preflightHashMap.set(std::make_pair(origin, url), WTFMove(preflightResult));
}

bool CrossOriginPreflightResultCache::canSkipPreflight(const String& origin, const URL& url, StoredCredentialsPolicy storedCredentialsPolicy, const String& method, const HTTPHeaderMap& requestHeaders)
{
    ASSERT(isMainThread());
    auto it = m_preflightHashMap.find(std::make_pair(origin, url));
    if (it == m_preflightHashMap.end())
        return false;

    if (it->value->allowsRequest(storedCredentialsPolicy, method, requestHeaders))
        return true;

    m_preflightHashMap.remove(it);
    return false;
}

void CrossOriginPreflightResultCache::clear()
{
    ASSERT(isMainThread());
    m_preflightHashMap.clear();
}

} // namespace WebCore
