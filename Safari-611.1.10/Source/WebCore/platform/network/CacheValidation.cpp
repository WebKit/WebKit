/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "CookieJar.h"
#include "HTTPHeaderMap.h"
#include "NetworkStorageSession.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SameSiteInfo.h"
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// These response headers are not copied from a revalidated response to the
// cached response headers. For compatibility, this list is based on Chromium's
// net/http/http_response_headers.cc.
static const char* const headersToIgnoreAfterRevalidation[] = {
    "allow",
    "connection",
    "etag",
    "keep-alive",
    "last-modified",
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
static const char* const headerPrefixesToIgnoreAfterRevalidation[] = {
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
    for (auto& prefixToIgnore : headerPrefixesToIgnoreAfterRevalidation) {
        // FIXME: Would be more efficient if we added an overload of
        // startsWithIgnoringASCIICase that takes a const char*.
        if (header.startsWithIgnoringASCIICase(prefixToIgnore))
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

Seconds computeCurrentAge(const ResourceResponse& response, WallTime responseTime)
{
    // Age calculation:
    // http://tools.ietf.org/html/rfc7234#section-4.2.3
    // No compensation for latency as that is not terribly important in practice.
    auto dateValue = response.date();
    auto apparentAge = dateValue ? std::max(0_us, responseTime - *dateValue) : 0_us;
    auto ageValue = response.age().valueOr(0_us);
    auto correctedInitialAge = std::max(apparentAge, ageValue);
    auto residentTime = WallTime::now() - responseTime;
    return correctedInitialAge + residentTime;
}

Seconds computeFreshnessLifetimeForHTTPFamily(const ResourceResponse& response, WallTime responseTime)
{
    if (!response.url().protocolIsInHTTPFamily())
        return 0_us;

    // Freshness Lifetime:
    // http://tools.ietf.org/html/rfc7234#section-4.2.1
    auto maxAge = response.cacheControlMaxAge();
    if (maxAge)
        return *maxAge;

    auto date = response.date();
    auto effectiveDate = date.valueOr(responseTime);
    if (auto expires = response.expires())
        return *expires - effectiveDate;

    // Implicit lifetime.
    switch (response.httpStatusCode()) {
    case 301: // Moved Permanently
    case 410: // Gone
        // These are semantically permanent and so get long implicit lifetime.
        return 24_h * 365;
    default:
        // Heuristic Freshness:
        // http://tools.ietf.org/html/rfc7234#section-4.2.2
        if (auto lastModified = response.lastModified())
            return (effectiveDate - *lastModified) * 0.1;
        return 0_us;
    }
}

void updateRedirectChainStatus(RedirectChainCacheStatus& redirectChainCacheStatus, const ResourceResponse& response)
{
    if (redirectChainCacheStatus.status == RedirectChainCacheStatus::Status::NotCachedRedirection)
        return;
    if (response.cacheControlContainsNoStore() || response.cacheControlContainsNoCache() || response.cacheControlContainsMustRevalidate()) {
        redirectChainCacheStatus.status = RedirectChainCacheStatus::Status::NotCachedRedirection;
        return;
    }

    redirectChainCacheStatus.status = RedirectChainCacheStatus::Status::CachedRedirection;
    auto responseTimestamp = WallTime::now();
    // Store the nearest end of cache validity date
    auto endOfValidity = responseTimestamp + computeFreshnessLifetimeForHTTPFamily(response, responseTimestamp) - computeCurrentAge(response, responseTimestamp);
    redirectChainCacheStatus.endOfValidity = std::min(redirectChainCacheStatus.endOfValidity, endOfValidity);
}

bool redirectChainAllowsReuse(RedirectChainCacheStatus redirectChainCacheStatus, ReuseExpiredRedirectionOrNot reuseExpiredRedirection)
{
    switch (redirectChainCacheStatus.status) {
    case RedirectChainCacheStatus::Status::NoRedirection:
        return true;
    case RedirectChainCacheStatus::Status::NotCachedRedirection:
        return false;
    case RedirectChainCacheStatus::Status::CachedRedirection:
        return reuseExpiredRedirection || WallTime::now() <= redirectChainCacheStatus.endOfValidity;
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
                    result.maxAge = Seconds { maxAge };
            } else if (equalLettersIgnoringASCIICase(directives[i].first, "max-stale")) {
                // https://tools.ietf.org/html/rfc7234#section-5.2.1.2
                if (result.maxStale) {
                    // First max-stale directive wins if there are multiple ones.
                    continue;
                }
                if (directives[i].second.isEmpty()) {
                    // if no value is assigned to max-stale, then the client is willing to accept a stale response of any age.
                    result.maxStale = Seconds::infinity();
                    continue;
                }
                bool ok;
                double maxStale = directives[i].second.toDouble(&ok);
                if (ok)
                    result.maxStale = Seconds { maxStale };
            } else if (equalLettersIgnoringASCIICase(directives[i].first, "immutable")) {
                result.immutable = true;
            } else if (equalLettersIgnoringASCIICase(directives[i].first, "stale-while-revalidate")) {
                if (result.staleWhileRevalidate) {
                    // First stale-while-revalidate directive wins if there are multiple ones.
                    continue;
                }
                bool ok;
                double staleWhileRevalidate = directives[i].second.toDouble(&ok);
                if (ok)
                    result.staleWhileRevalidate = Seconds { staleWhileRevalidate };
            }
        }
    }

    if (!result.noCache) {
        // Handle Pragma: no-cache
        // This is deprecated and equivalent to Cache-control: no-cache
        // Don't bother tokenizing the value; handling that exactly right is not important.
        result.noCache = headers.get(HTTPHeaderName::Pragma).containsIgnoringASCIICase("no-cache");
    }

    return result;
}

static String cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const ResourceRequest& request)
{
    return session.cookieRequestHeaderFieldValue(request.firstPartyForCookies(), SameSiteInfo::create(request), request.url(), WTF::nullopt, WTF::nullopt, request.url().protocolIs("https") ? IncludeSecureCookies::Yes : IncludeSecureCookies::No, ShouldAskITP::Yes, ShouldRelaxThirdPartyCookieBlocking::No).first;
}

static String cookieRequestHeaderFieldValue(const CookieJar* cookieJar, const ResourceRequest& request)
{
    if (!cookieJar)
        return { };

    return cookieJar->cookieRequestHeaderFieldValue(request.firstPartyForCookies(), SameSiteInfo::create(request), request.url(), WTF::nullopt, WTF::nullopt, request.url().protocolIs("https") ? IncludeSecureCookies::Yes : IncludeSecureCookies::No).first;
}

static String headerValueForVary(const ResourceRequest& request, const String& headerName, Function<String()>&& cookieRequestHeaderFieldValueFunction)
{
    // Explicit handling for cookies is needed because they are added magically by the networking layer.
    // FIXME: The value might have changed between making the request and retrieving the cookie here.
    // We could fetch the cookie when making the request but that seems overkill as the case is very rare and it
    // is a blocking operation. This should be sufficient to cover reasonable cases.
    if (headerName == httpHeaderNameString(HTTPHeaderName::Cookie))
        return cookieRequestHeaderFieldValueFunction();
    return request.httpHeaderField(headerName);
}

static Vector<std::pair<String, String>> collectVaryingRequestHeadersInternal(const ResourceResponse& response, Function<String(const String& headerName)>&& headerValueForVaryFunction)
{
    String varyValue = response.httpHeaderField(HTTPHeaderName::Vary);
    if (varyValue.isEmpty())
        return { };
    Vector<String> varyingHeaderNames = varyValue.split(',');
    Vector<std::pair<String, String>> varyingRequestHeaders;
    varyingRequestHeaders.reserveCapacity(varyingHeaderNames.size());
    for (auto& varyHeaderName : varyingHeaderNames) {
        String headerName = varyHeaderName.stripWhiteSpace();
        String headerValue = headerValueForVaryFunction(headerName);
        varyingRequestHeaders.append(std::make_pair(headerName, headerValue));
    }
    return varyingRequestHeaders;
}

Vector<std::pair<String, String>> collectVaryingRequestHeaders(NetworkStorageSession* storageSession, const ResourceRequest& request, const ResourceResponse& response)
{
    if (!storageSession)
        return { };
    return collectVaryingRequestHeadersInternal(response, [&] (const String& headerName) {
        return headerValueForVary(request, headerName, [&] {
            return cookieRequestHeaderFieldValue(*storageSession, request);
        });
    });
}

Vector<std::pair<String, String>> collectVaryingRequestHeaders(const CookieJar* cookieJar, const ResourceRequest& request, const ResourceResponse& response)
{
    return collectVaryingRequestHeadersInternal(response, [&] (const String& headerName) {
        return headerValueForVary(request, headerName, [&] {
            return cookieRequestHeaderFieldValue(cookieJar, request);
        });
    });
}

static bool verifyVaryingRequestHeadersInternal(const Vector<std::pair<String, String>>& varyingRequestHeaders, Function<String(const String&)>&& headerValueForVary)
{
    for (auto& varyingRequestHeader : varyingRequestHeaders) {
        // FIXME: Vary: * in response would ideally trigger a cache delete instead of a store.
        if (varyingRequestHeader.first == "*")
            return false;
        if (headerValueForVary(varyingRequestHeader.first) != varyingRequestHeader.second)
            return false;
    }
    return true;
}

bool verifyVaryingRequestHeaders(NetworkStorageSession* storageSession, const Vector<std::pair<String, String>>& varyingRequestHeaders, const ResourceRequest& request)
{
    if (!storageSession)
        return false;
    return verifyVaryingRequestHeadersInternal(varyingRequestHeaders, [&] (const String& headerName) {
        return headerValueForVary(request, headerName, [&] {
            return cookieRequestHeaderFieldValue(*storageSession, request);
        });
    });
}

bool verifyVaryingRequestHeaders(const CookieJar* cookieJar, const Vector<std::pair<String, String>>& varyingRequestHeaders, const ResourceRequest& request)
{
    return verifyVaryingRequestHeadersInternal(varyingRequestHeaders, [&] (const String& headerName) {
        return headerValueForVary(request, headerName, [&] {
            return cookieRequestHeaderFieldValue(cookieJar, request);
        });
    });
}

// http://tools.ietf.org/html/rfc7231#page-48
bool isStatusCodeCacheableByDefault(int statusCode)
{
    switch (statusCode) {
    case 200: // OK
    case 203: // Non-Authoritative Information
    case 204: // No Content
    case 206: // Partial Content
    case 300: // Multiple Choices
    case 301: // Moved Permanently
    case 404: // Not Found
    case 405: // Method Not Allowed
    case 410: // Gone
    case 414: // URI Too Long
    case 501: // Not Implemented
        return true;
    default:
        return false;
    }
}

bool isStatusCodePotentiallyCacheable(int statusCode)
{
    switch (statusCode) {
    case 201: // Created
    case 202: // Accepted
    case 205: // Reset Content
    case 302: // Found
    case 303: // See Other
    case 307: // Temporary redirect
    case 403: // Forbidden
    case 406: // Not Acceptable
    case 415: // Unsupported Media Type
        return true;
    default:
        return false;
    }
}

}
