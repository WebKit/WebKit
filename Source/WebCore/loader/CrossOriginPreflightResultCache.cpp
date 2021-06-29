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
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

// These values are at the discretion of the user agent.
static const auto defaultPreflightCacheTimeout = 5_s;
static const auto maxPreflightCacheTimeout = 600_s; // Should be short enough to minimize the risk of using a poisoned cache after switching to a secure network.

CrossOriginPreflightResultCache::CrossOriginPreflightResultCache()
{
}

static bool parseAccessControlMaxAge(const String& string, Seconds& expiryDelta)
{
    // FIXME: This should probably reject strings that have a leading "+".
    auto parsedInteger = parseInteger<uint64_t>(string);
    expiryDelta = Seconds(static_cast<double>(parsedInteger.value_or(0)));
    return parsedInteger.has_value();
}

Expected<UniqueRef<CrossOriginPreflightResultCacheItem>, String> CrossOriginPreflightResultCacheItem::create(StoredCredentialsPolicy policy, const ResourceResponse& response)
{
    auto methods = parseAccessControlAllowList(response.httpHeaderField(HTTPHeaderName::AccessControlAllowMethods));
    if (!methods)
        return makeUnexpected(makeString("Header Access-Control-Allow-Methods has an invalid value: ", response.httpHeaderField(HTTPHeaderName::AccessControlAllowMethods)));

    auto headers = parseAccessControlAllowList<ASCIICaseInsensitiveHash>(response.httpHeaderField(HTTPHeaderName::AccessControlAllowHeaders));
    if (!headers)
        return makeUnexpected(makeString("Header Access-Control-Allow-Headers has an invalid value: ", response.httpHeaderField(HTTPHeaderName::AccessControlAllowHeaders)));

    Seconds expiryDelta = 0_s;
    if (parseAccessControlMaxAge(response.httpHeaderField(HTTPHeaderName::AccessControlMaxAge), expiryDelta)) {
        if (expiryDelta > maxPreflightCacheTimeout)
            expiryDelta = maxPreflightCacheTimeout;
    } else
        expiryDelta = defaultPreflightCacheTimeout;

    return makeUniqueRef<CrossOriginPreflightResultCacheItem>(MonotonicTime::now() + expiryDelta, policy, WTFMove(*methods), WTFMove(*headers));
}

std::optional<String> CrossOriginPreflightResultCacheItem::validateMethodAndHeaders(const String& method, const HTTPHeaderMap& requestHeaders) const
{
    if (!allowsCrossOriginMethod(method, m_storedCredentialsPolicy))
        return makeString("Method ", method, " is not allowed by Access-Control-Allow-Methods.");

    if (auto badHeader = validateCrossOriginHeaders(requestHeaders, m_storedCredentialsPolicy))
        return makeString("Request header field ", *badHeader, " is not allowed by Access-Control-Allow-Headers.");
    return { };
}

bool CrossOriginPreflightResultCacheItem::allowsCrossOriginMethod(const String& method, StoredCredentialsPolicy storedCredentialsPolicy) const
{
    return m_methods.contains(method) || (m_methods.contains("*") && storedCredentialsPolicy != StoredCredentialsPolicy::Use) || isOnAccessControlSimpleRequestMethodAllowlist(method);
}

std::optional<String> CrossOriginPreflightResultCacheItem::validateCrossOriginHeaders(const HTTPHeaderMap& requestHeaders, StoredCredentialsPolicy storedCredentialsPolicy) const
{
    bool validWildcard = m_headers.contains("*") && storedCredentialsPolicy != StoredCredentialsPolicy::Use;
    for (const auto& header : requestHeaders) {
        if (header.keyAsHTTPHeaderName && isCrossOriginSafeRequestHeader(header.keyAsHTTPHeaderName.value(), header.value))
            continue;
        if (!m_headers.contains(header.key) && !validWildcard)
            return header.key;
    }
    return { };
}

bool CrossOriginPreflightResultCacheItem::allowsRequest(StoredCredentialsPolicy storedCredentialsPolicy, const String& method, const HTTPHeaderMap& requestHeaders) const
{
    if (m_absoluteExpiryTime < MonotonicTime::now())
        return false;
    if (storedCredentialsPolicy == StoredCredentialsPolicy::Use && m_storedCredentialsPolicy == StoredCredentialsPolicy::DoNotUse)
        return false;
    if (!allowsCrossOriginMethod(method, storedCredentialsPolicy))
        return false;
    if (auto badHeader = validateCrossOriginHeaders(requestHeaders, storedCredentialsPolicy))
        return false;
    return true;
}

CrossOriginPreflightResultCache& CrossOriginPreflightResultCache::singleton()
{
    ASSERT(isMainThread());

    static NeverDestroyed<CrossOriginPreflightResultCache> cache;
    return cache;
}

void CrossOriginPreflightResultCache::appendEntry(PAL::SessionID sessionID, const String& origin, const URL& url, std::unique_ptr<CrossOriginPreflightResultCacheItem> preflightResult)
{
    ASSERT(isMainThread());
    m_preflightHashMap.set(std::make_tuple(sessionID, origin, url), WTFMove(preflightResult));
}

bool CrossOriginPreflightResultCache::canSkipPreflight(PAL::SessionID sessionID, const String& origin, const URL& url, StoredCredentialsPolicy storedCredentialsPolicy, const String& method, const HTTPHeaderMap& requestHeaders)
{
    ASSERT(isMainThread());
    auto it = m_preflightHashMap.find(std::make_tuple(sessionID, origin, url));
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
