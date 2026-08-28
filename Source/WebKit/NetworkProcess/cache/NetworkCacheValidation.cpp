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
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/TrackingPreventionTypes.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringView.h>

namespace WebKit::NetworkCache {
using namespace WebCore;

static String cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const ResourceRequest& request)
{
    auto cookieHeader = session.cookieRequestHeaderFieldValue(request.firstPartyForCookies(), SameSiteInfo::create(request), request.url(), std::nullopt, std::nullopt, CookieJar::shouldIncludeSecureCookies(request.url()), ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No, IsKnownCrossSiteTracker::No).first;
    auto digest = computeCookieHeaderDigestForVary(cookieHeader);
    if (!digest)
        return { };
    return base64EncodeToString(*digest);
}

static String headerValueForVary(const ResourceRequest& request, StringView headerName, const NetworkStorageSession& session)
{
    // Explicit handling for cookies is needed because they are added magically by the networking layer.
    // FIXME: The value might have changed between making the request and retrieving the cookie here.
    // We could fetch the cookie when making the request but that seems overkill as the case is very rare and it
    // is a blocking operation. This should be sufficient to cover reasonable cases.
    if (headerName == httpHeaderNameString(HTTPHeaderName::Cookie))
        return cookieRequestHeaderFieldValue(session, request);
    return request.httpHeaderField(headerName);
}

Vector<std::pair<String, String>> collectVaryingRequestHeaders(NetworkStorageSession* storageSession, const ResourceRequest& request, const ResourceResponse& response)
{
    if (!storageSession)
        return { };

    auto varyValue = response.httpHeaderField(HTTPHeaderName::Vary);
    if (varyValue.isEmpty())
        return { };

    Vector<std::pair<String, String>> headers;
    for (auto varyHeaderName : StringView(varyValue).split(',')) {
        auto headerName = varyHeaderName.trim(isUnicodeCompatibleASCIIWhitespace<char16_t>);
        headers.append(std::pair { headerName.toString(), headerValueForVary(request, headerName, *storageSession) });
    }
    return headers;
}

bool verifyVaryingRequestHeaders(NetworkStorageSession* storageSession, const Vector<std::pair<String, String>>& varyingRequestHeaders, const ResourceRequest& request)
{
    if (!storageSession)
        return false;

    for (auto& varyingRequestHeader : varyingRequestHeaders) {
        // FIXME: Vary: * in response would ideally trigger a cache delete instead of a store.
        if (varyingRequestHeader.first == "*"_s)
            return false;
        if (headerValueForVary(request, varyingRequestHeader.first, *storageSession) != varyingRequestHeader.second)
            return false;
    }
    return true;
}

} // namespace WebKit::NetworkCache
