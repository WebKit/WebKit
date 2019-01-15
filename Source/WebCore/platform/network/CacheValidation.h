/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <pal/SessionID.h>
#include <wtf/Markable.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CookiesStrategy;
class HTTPHeaderMap;
class NetworkStorageSession;
class ResourceRequest;
class ResourceResponse;

struct RedirectChainCacheStatus {
    enum class Status : uint8_t {
        NoRedirection,
        NotCachedRedirection,
        CachedRedirection
    };
    RedirectChainCacheStatus()
        : endOfValidity(WallTime::infinity())
        , status(Status::NoRedirection)
    { }
    WallTime endOfValidity;
    Status status;
};

WEBCORE_EXPORT Seconds computeCurrentAge(const ResourceResponse&, WallTime responseTimestamp);
WEBCORE_EXPORT Seconds computeFreshnessLifetimeForHTTPFamily(const ResourceResponse&, WallTime responseTimestamp);
WEBCORE_EXPORT void updateResponseHeadersAfterRevalidation(ResourceResponse&, const ResourceResponse& validatingResponse);
WEBCORE_EXPORT void updateRedirectChainStatus(RedirectChainCacheStatus&, const ResourceResponse&);

enum ReuseExpiredRedirectionOrNot { DoNotReuseExpiredRedirection, ReuseExpiredRedirection };
WEBCORE_EXPORT bool redirectChainAllowsReuse(RedirectChainCacheStatus, ReuseExpiredRedirectionOrNot);

struct CacheControlDirectives {
    constexpr CacheControlDirectives()
        : noCache(false)
        , noStore(false)
        , mustRevalidate(false)
        , immutable(false)
        { }

    Markable<Seconds, Seconds::MarkableTraits> maxAge;
    Markable<Seconds, Seconds::MarkableTraits> maxStale;
    bool noCache : 1;
    bool noStore : 1;
    bool mustRevalidate : 1;
    bool immutable : 1;
};
WEBCORE_EXPORT CacheControlDirectives parseCacheControlDirectives(const HTTPHeaderMap&);

WEBCORE_EXPORT Vector<std::pair<String, String>> collectVaryingRequestHeaders(NetworkStorageSession&, const ResourceRequest&, const ResourceResponse&);
WEBCORE_EXPORT Vector<std::pair<String, String>> collectVaryingRequestHeaders(CookiesStrategy&, const ResourceRequest&, const ResourceResponse&, const PAL::SessionID&);
WEBCORE_EXPORT bool verifyVaryingRequestHeaders(NetworkStorageSession&, const Vector<std::pair<String, String>>& varyingRequestHeaders, const ResourceRequest&);
WEBCORE_EXPORT bool verifyVaryingRequestHeaders(CookiesStrategy&, const Vector<std::pair<String, String>>& varyingRequestHeaders, const ResourceRequest&, const PAL::SessionID&);

WEBCORE_EXPORT bool isStatusCodeCacheableByDefault(int statusCode);
WEBCORE_EXPORT bool isStatusCodePotentiallyCacheable(int statusCode);

}
