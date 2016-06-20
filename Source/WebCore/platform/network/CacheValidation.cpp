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

#include "config.h"
#include "CacheValidation.h"

#include "CookiesStrategy.h"
#include "HTTPHeaderMap.h"
#include "NetworkStorageSession.h"
#include "PlatformCookieJar.h"
#include "PlatformStrategies.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/CurrentTime.h>
#include <wtf/text/StringView.h>

namespace WebCore {

// These response headers are not copied from a revalidated response to the
// cached response headers. For compatibility, this list is based on Chromium's
// net/http/http_response_headers.cc.
const char* const headersToIgnoreAfterRevalidation[] = {
    "allow",
    "connection",
    "etag",
    "keep-alive",
    "last-modified"
    "proxy-authenticate",
    "proxy-connection",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "www-authenticate",
    "x-frame-options",
    "x-xss-protection",
};

// Some header prefixes mean "Don't copy this header from a 304 response.".
// Rather than listing all the relevant headers, we can consolidate them into
// this list, also grabbed from Chromium's net/http/http_response_headers.cc.
const char* const headerPrefixesToIgnoreAfterRevalidation[] = {
    "content-",
    "x-content-",
    "x-webkit-"
};

static inline bool shouldUpdateHeaderAfterRevalidation(const String& header)
{
    for (auto& headerToIgnore : headersToIgnoreAfterRevalidation) {
        if (equalIgnoringASCIICase(header, headerToIgnore))
            return false;
    }
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(headerPrefixesToIgnoreAfterRevalidation); i++) {
        if (header.startsWith(headerPrefixesToIgnoreAfterRevalidation[i], false))
            return false;
    }
    return true;
}

void updateResponseHeadersAfterRevalidation(ResourceResponse& response, const ResourceResponse& validatingResponse)
{
    // Freshening stored response upon validation:
    // http://tools.ietf.org/html/rfc7234#section-4.3.4
    for (const auto& header : validatingResponse.httpHeaderFields()) {
        // Entity headers should not be sent by servers when generating a 304
        // response; misconfigured servers send them anyway. We shouldn't allow
        // such headers to update the original request. We'll base this on the
        // list defined by RFC2616 7.1, with a few additions for extension headers
        // we care about.
        if (!shouldUpdateHeaderAfterRevalidation(header.key))
            continue;
        response.setHTTPHeaderField(header.key, header.value);
    }
}

std::chrono::microseconds computeCurrentAge(const ResourceResponse& response, std::chrono::system_clock::time_point responseTime)
{
    using namespace std::chrono;

    // Age calculation:
    // http://tools.ietf.org/html/rfc7234#section-4.2.3
    // No compensation for latency as that is not terribly important in practice.
    auto dateValue = response.date();
    auto apparentAge = dateValue ? std::max(0us, duration_cast<microseconds>(responseTime - *dateValue)) : 0us;
    auto ageValue = response.age().valueOr(0us);
    auto correctedInitialAge = std::max(apparentAge, ageValue);
    auto residentTime = duration_cast<microseconds>(system_clock::now() - responseTime);
    return correctedInitialAge + residentTime;
}

std::chrono::microseconds computeFreshnessLifetimeForHTTPFamily(const ResourceResponse& response, std::chrono::system_clock::time_point responseTime)
{
    using namespace std::chrono;
    ASSERT(response.url().protocolIsInHTTPFamily());

    // Freshness Lifetime:
    // http://tools.ietf.org/html/rfc7234#section-4.2.1
    auto maxAge = response.cacheControlMaxAge();
    if (maxAge)
        return *maxAge;

    auto date = response.date();
    auto effectiveDate = date.valueOr(responseTime);
    if (auto expires = response.expires())
        return duration_cast<microseconds>(*expires - effectiveDate);

    // Implicit lifetime.
    switch (response.httpStatusCode()) {
    case 301: // Moved Permanently
    case 410: // Gone
        // These are semantically permanent and so get long implicit lifetime.
        return 365 * 24h;
    default:
        // Heuristic Freshness:
        // http://tools.ietf.org/html/rfc7234#section-4.2.2
        if (auto lastModified = response.lastModified())
            return duration_cast<microseconds>((effectiveDate - *lastModified) * 0.1);
        return 0us;
    }
}

void updateRedirectChainStatus(RedirectChainCacheStatus& redirectChainCacheStatus, const ResourceResponse& response)
{
    using namespace std::chrono;

    if (redirectChainCacheStatus.status == RedirectChainCacheStatus::NotCachedRedirection)
        return;
    if (response.cacheControlContainsNoStore() || response.cacheControlContainsNoCache() || response.cacheControlContainsMustRevalidate()) {
        redirectChainCacheStatus.status = RedirectChainCacheStatus::NotCachedRedirection;
        return;
    }

    redirectChainCacheStatus.status = RedirectChainCacheStatus::CachedRedirection;
    auto responseTimestamp = system_clock::now();
    // Store the nearest end of cache validity date
    auto endOfValidity = responseTimestamp + computeFreshnessLifetimeForHTTPFamily(response, responseTimestamp) - computeCurrentAge(response, responseTimestamp);
    redirectChainCacheStatus.endOfValidity = std::min(redirectChainCacheStatus.endOfValidity, endOfValidity);
}

bool redirectChainAllowsReuse(RedirectChainCacheStatus redirectChainCacheStatus, ReuseExpiredRedirectionOrNot reuseExpiredRedirection)
{
    switch (redirectChainCacheStatus.status) {
    case RedirectChainCacheStatus::NoRedirection:
        return true;
    case RedirectChainCacheStatus::NotCachedRedirection:
        return false;
    case RedirectChainCacheStatus::CachedRedirection:
        return reuseExpiredRedirection || std::chrono::system_clock::now() <= redirectChainCacheStatus.endOfValidity;
    }
    ASSERT_NOT_REACHED();
    return false;
}

inline bool isCacheHeaderSeparator(UChar c)
{
    // http://tools.ietf.org/html/rfc7230#section-3.2.6
    switch (c) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
        return true;
    default:
        return false;
    }
}

inline bool isControlCharacterOrSpace(UChar character)
{
    return character <= ' ' || character == 127;
}

inline StringView trimToNextSeparator(StringView string)
{
    return string.substring(0, string.find(isCacheHeaderSeparator));
}

static Vector<std::pair<String, String>> parseCacheHeader(const String& header)
{
    Vector<std::pair<String, String>> result;

    String safeHeaderString = header.removeCharacters(isControlCharacterOrSpace);
    StringView safeHeader = safeHeaderString;
    unsigned max = safeHeader.length();
    unsigned pos = 0;
    while (pos < max) {
        size_t nextCommaPosition = safeHeader.find(',', pos);
        size_t nextEqualSignPosition = safeHeader.find('=', pos);
        if (nextEqualSignPosition == notFound && nextCommaPosition == notFound) {
            // Add last directive to map with empty string as value
            result.append({ trimToNextSeparator(safeHeader.substring(pos, max - pos)).toString(), emptyString() });
            return result;
        }
        if (nextCommaPosition != notFound && (nextCommaPosition < nextEqualSignPosition || nextEqualSignPosition == notFound)) {
            // Add directive to map with empty string as value
            result.append({ trimToNextSeparator(safeHeader.substring(pos, nextCommaPosition - pos)).toString(), emptyString() });
            pos += nextCommaPosition - pos + 1;
            continue;
        }
        // Get directive name, parse right hand side of equal sign, then add to map
        String directive = trimToNextSeparator(safeHeader.substring(pos, nextEqualSignPosition - pos)).toString();
        pos += nextEqualSignPosition - pos + 1;

        StringView value = safeHeader.substring(pos, max - pos);
        if (value[0] == '"') {
            // The value is a quoted string
            size_t nextDoubleQuotePosition = value.find('"', 1);
            if (nextDoubleQuotePosition == notFound) {
                // Parse error; just use the rest as the value
                result.append({ directive, trimToNextSeparator(value.substring(1)).toString() });
                return result;
            }
            // Store the value as a quoted string without quotes
            result.append({ directive, value.substring(1, nextDoubleQuotePosition - 1).toString() });
            pos += (safeHeader.find('"', pos) - pos) + nextDoubleQuotePosition + 1;
            // Move past next comma, if there is one
            size_t nextCommaPosition2 = safeHeader.find(',', pos);
            if (nextCommaPosition2 == notFound)
                return result; // Parse error if there is anything left with no comma
            pos += nextCommaPosition2 - pos + 1;
            continue;
        }
        // The value is a token until the next comma
        size_t nextCommaPosition2 = value.find(',');
        if (nextCommaPosition2 == notFound) {
            // The rest is the value; no change to value needed
            result.append({ directive, trimToNextSeparator(value).toString() });
            return result;
        }
        // The value is delimited by the next comma
        result.append({ directive, trimToNextSeparator(value.substring(0, nextCommaPosition2)).toString() });
        pos += (safeHeader.find(',', pos) - pos) + 1;
    }
    return result;
}

CacheControlDirectives parseCacheControlDirectives(const HTTPHeaderMap& headers)
{
    using namespace std::chrono;

    CacheControlDirectives result;

    String cacheControlValue = headers.get(HTTPHeaderName::CacheControl);
    if (!cacheControlValue.isEmpty()) {
        auto directives = parseCacheHeader(cacheControlValue);

        size_t directivesSize = directives.size();
        for (size_t i = 0; i < directivesSize; ++i) {
            // A no-cache directive with a value is only meaningful for proxy caches.
            // It should be ignored by a browser level cache.
            // http://tools.ietf.org/html/rfc7234#section-5.2.2.2
            if (equalLettersIgnoringASCIICase(directives[i].first, "no-cache") && directives[i].second.isEmpty())
                result.noCache = true;
            else if (equalLettersIgnoringASCIICase(directives[i].first, "no-store"))
                result.noStore = true;
            else if (equalLettersIgnoringASCIICase(directives[i].first, "must-revalidate"))
                result.mustRevalidate = true;
            else if (equalLettersIgnoringASCIICase(directives[i].first, "max-age")) {
                if (result.maxAge) {
                    // First max-age directive wins if there are multiple ones.
                    continue;
                }
                bool ok;
                double maxAge = directives[i].second.toDouble(&ok);
                if (ok)
                    result.maxAge = duration_cast<microseconds>(duration<double>(maxAge));
            } else if (equalLettersIgnoringASCIICase(directives[i].first, "max-stale")) {
                // https://tools.ietf.org/html/rfc7234#section-5.2.1.2
                if (result.maxStale) {
                    // First max-stale directive wins if there are multiple ones.
                    continue;
                }
                if (directives[i].second.isEmpty()) {
                    // if no value is assigned to max-stale, then the client is willing to accept a stale response of any age.
                    result.maxStale = microseconds::max();
                    continue;
                }
                bool ok;
                double maxStale = directives[i].second.toDouble(&ok);
                if (ok)
                    result.maxStale = duration_cast<microseconds>(duration<double>(maxStale));
            }
        }
    }

    if (!result.noCache) {
        // Handle Pragma: no-cache
        // This is deprecated and equivalent to Cache-control: no-cache
        // Don't bother tokenizing the value, it is not important
        String pragmaValue = headers.get(HTTPHeaderName::Pragma);

        result.noCache = pragmaValue.contains("no-cache", false);
    }

    return result;
}

static String headerValueForVary(const ResourceRequest& request, const String& headerName, SessionID sessionID)
{
    // Explicit handling for cookies is needed because they are added magically by the networking layer.
    // FIXME: The value might have changed between making the request and retrieving the cookie here.
    // We could fetch the cookie when making the request but that seems overkill as the case is very rare and it
    // is a blocking operation. This should be sufficient to cover reasonable cases.
    if (headerName == httpHeaderNameString(HTTPHeaderName::Cookie)) {
        auto* cookieStrategy = platformStrategies() ? platformStrategies()->cookiesStrategy() : nullptr;
        if (!cookieStrategy) {
            ASSERT(sessionID == SessionID::defaultSessionID());
            return cookieRequestHeaderFieldValue(NetworkStorageSession::defaultStorageSession(), request.firstPartyForCookies(), request.url());
        }
        return cookieStrategy->cookieRequestHeaderFieldValue(sessionID, request.firstPartyForCookies(), request.url());
    }
    return request.httpHeaderField(headerName);
}

Vector<std::pair<String, String>> collectVaryingRequestHeaders(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, SessionID sessionID)
{
    String varyValue = response.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isEmpty())
        return { };
    Vector<String> varyingHeaderNames;
    varyValue.split(',', /*allowEmptyEntries*/ false, varyingHeaderNames);
    Vector<std::pair<String, String>> varyingRequestHeaders;
    varyingRequestHeaders.reserveCapacity(varyingHeaderNames.size());
    for (auto& varyHeaderName : varyingHeaderNames) {
        String headerName = varyHeaderName.stripWhiteSpace();
        String headerValue = headerValueForVary(request, headerName, sessionID);
        varyingRequestHeaders.append(std::make_pair(headerName, headerValue));
    }
    return varyingRequestHeaders;
}

bool verifyVaryingRequestHeaders(const Vector<std::pair<String, String>>& varyingRequestHeaders, const WebCore::ResourceRequest& request, SessionID sessionID)
{
    for (auto& varyingRequestHeader : varyingRequestHeaders) {
        // FIXME: Vary: * in response would ideally trigger a cache delete instead of a store.
        if (varyingRequestHeader.first == "*")
            return false;
        String headerValue = headerValueForVary(request, varyingRequestHeader.first, sessionID);
        if (headerValue != varyingRequestHeader.second)
            return false;
    }
    return true;
}

}
