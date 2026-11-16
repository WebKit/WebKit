/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkCacheValidation.h"

#include "NetworkStorageSession.h"
#include <WebCore/CacheValidation.h>
#include <WebCore/CookieJar.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/TrackingPreventionTypes.h>
#include <wtf/text/StringView.h>

namespace WebKit::NetworkCache {
using namespace WebCore;

// Each helper is used on both sides of its own collect/verify pair, so the two need not agree.
static String cookieRequestHeaderFieldValueForVary(const NetworkStorageSession& session, const ResourceRequest& request)
{
    auto cookieHeader = session.cookieRequestHeaderFieldValue(request.firstPartyForCookies(), SameSiteInfo::create(request), request.url(), std::nullopt, std::nullopt, CookieJar::shouldIncludeSecureCookies(request.url()), ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No, IsKnownCrossSiteTracker::No).first;
    return encodeCookieHeaderDigestForVary(computeCookieHeaderDigestForVary(cookieHeader));
}

Vector<std::pair<String, String>> collectVaryingRequestHeaders(NetworkStorageSession* storageSession, const ResourceRequest& request, const ResourceResponse& response)
{
    if (!storageSession)
        return { };
    return WebCore::collectVaryingRequestHeaders(response, [&] (StringView headerName) {
        return headerValueForVary(request, headerName, [&] {
            return cookieRequestHeaderFieldValueForVary(*storageSession, request);
        });
    });
}

bool verifyVaryingRequestHeaders(NetworkStorageSession* storageSession, const Vector<std::pair<String, String>>& varyingRequestHeaders, const ResourceRequest& request)
{
    if (!storageSession)
        return false;
    return WebCore::verifyVaryingRequestHeaders(varyingRequestHeaders, [&] (const String& headerName) {
        return headerValueForVary(request, headerName, [&] {
            return cookieRequestHeaderFieldValueForVary(*storageSession, request);
        });
    });
}

} // namespace WebKit::NetworkCache
