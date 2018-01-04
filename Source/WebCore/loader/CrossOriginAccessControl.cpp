/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "CrossOriginAccessControl.h"

#include "CrossOriginPreflightResultCache.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool isOnAccessControlSimpleRequestMethodWhitelist(const String& method)
{
    return method == "GET" || method == "HEAD" || method == "POST";
}

bool isSimpleCrossOriginAccessRequest(const String& method, const HTTPHeaderMap& headerMap)
{
    if (!isOnAccessControlSimpleRequestMethodWhitelist(method))
        return false;

    for (const auto& header : headerMap) {
        if (!header.keyAsHTTPHeaderName || !isCrossOriginSafeRequestHeader(header.keyAsHTTPHeaderName.value(), header.value))
            return false;
    }

    return true;
}

void updateRequestForAccessControl(ResourceRequest& request, SecurityOrigin& securityOrigin, StoredCredentialsPolicy storedCredentialsPolicy)
{
    request.removeCredentials();
    request.setAllowCookies(storedCredentialsPolicy == StoredCredentialsPolicy::Use);
    request.setHTTPOrigin(securityOrigin.toString());
}

ResourceRequest createAccessControlPreflightRequest(const ResourceRequest& request, SecurityOrigin& securityOrigin, const String& referrer)
{
    ResourceRequest preflightRequest(request.url());
    static const double platformDefaultTimeout = 0;
    preflightRequest.setTimeoutInterval(platformDefaultTimeout);
    updateRequestForAccessControl(preflightRequest, securityOrigin, StoredCredentialsPolicy::DoNotUse);
    preflightRequest.setHTTPMethod("OPTIONS");
    preflightRequest.setHTTPHeaderField(HTTPHeaderName::AccessControlRequestMethod, request.httpMethod());
    preflightRequest.setPriority(request.priority());
    if (!referrer.isNull())
        preflightRequest.setHTTPReferrer(referrer);

    const HTTPHeaderMap& requestHeaderFields = request.httpHeaderFields();

    if (!requestHeaderFields.isEmpty()) {
        Vector<String> unsafeHeaders;
        for (const auto& headerField : requestHeaderFields.commonHeaders()) {
            if (!isCrossOriginSafeRequestHeader(headerField.key, headerField.value))
                unsafeHeaders.append(httpHeaderNameString(headerField.key).toStringWithoutCopying().convertToASCIILowercase());
        }
        for (const auto& headerField : requestHeaderFields.uncommonHeaders())
            unsafeHeaders.append(headerField.key.convertToASCIILowercase());

        std::sort(unsafeHeaders.begin(), unsafeHeaders.end(), WTF::codePointCompareLessThan);

        StringBuilder headerBuffer;

        bool appendComma = false;
        for (const auto& headerField : unsafeHeaders) {
            if (appendComma)
                headerBuffer.append(',');
            else
                appendComma = true;

            headerBuffer.append(headerField);
        }
        if (!headerBuffer.isEmpty())
            preflightRequest.setHTTPHeaderField(HTTPHeaderName::AccessControlRequestHeaders, headerBuffer.toString());
    }

    return preflightRequest;
}

bool isValidCrossOriginRedirectionURL(const URL& redirectURL)
{
    return SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(redirectURL.protocol().toStringWithoutCopying())
        && redirectURL.user().isEmpty()
        && redirectURL.pass().isEmpty();
}

HTTPHeaderNameSet httpHeadersToKeepFromCleaning(const HTTPHeaderMap& headers)
{
    HTTPHeaderNameSet headersToKeep;
    if (headers.contains(HTTPHeaderName::ContentType))
        headersToKeep.add(HTTPHeaderName::ContentType);
    if (headers.contains(HTTPHeaderName::Referer))
        headersToKeep.add(HTTPHeaderName::Referer);
    if (headers.contains(HTTPHeaderName::Origin))
        headersToKeep.add(HTTPHeaderName::Origin);
    if (headers.contains(HTTPHeaderName::UserAgent))
        headersToKeep.add(HTTPHeaderName::UserAgent);
    if (headers.contains(HTTPHeaderName::AcceptEncoding))
        headersToKeep.add(HTTPHeaderName::AcceptEncoding);
    return headersToKeep;
}

void cleanHTTPRequestHeadersForAccessControl(ResourceRequest& request, const HashSet<HTTPHeaderName, WTF::IntHash<HTTPHeaderName>, WTF::StrongEnumHashTraits<HTTPHeaderName>>& headersToKeep)
{
    // Remove headers that may have been added by the network layer that cause access control to fail.
    if (!headersToKeep.contains(HTTPHeaderName::ContentType) && !isCrossOriginSafeRequestHeader(HTTPHeaderName::ContentType, request.httpContentType()))
        request.clearHTTPContentType();
    if (!headersToKeep.contains(HTTPHeaderName::Referer))
        request.clearHTTPReferrer();
    if (!headersToKeep.contains(HTTPHeaderName::Origin))
        request.clearHTTPOrigin();
    if (!headersToKeep.contains(HTTPHeaderName::UserAgent))
        request.clearHTTPUserAgent();
    if (!headersToKeep.contains(HTTPHeaderName::AcceptEncoding))
        request.clearHTTPAcceptEncoding();
}

bool passesAccessControlCheck(const ResourceResponse& response, StoredCredentialsPolicy storedCredentialsPolicy, SecurityOrigin& securityOrigin, String& errorDescription)
{
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are to be sent,
    // even with Access-Control-Allow-Credentials set to true.
    const String& accessControlOriginString = response.httpHeaderField(HTTPHeaderName::AccessControlAllowOrigin);
    if (accessControlOriginString == "*" && storedCredentialsPolicy == StoredCredentialsPolicy::DoNotUse)
        return true;

    String securityOriginString = securityOrigin.toString();
    if (accessControlOriginString != securityOriginString) {
        if (accessControlOriginString == "*")
            errorDescription = ASCIILiteral("Cannot use wildcard in Access-Control-Allow-Origin when credentials flag is true.");
        else if (accessControlOriginString.find(',') != notFound)
            errorDescription = ASCIILiteral("Access-Control-Allow-Origin cannot contain more than one origin.");
        else
            errorDescription = makeString("Origin ", securityOriginString, " is not allowed by Access-Control-Allow-Origin.");
        return false;
    }

    if (storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        const String& accessControlCredentialsString = response.httpHeaderField(HTTPHeaderName::AccessControlAllowCredentials);
        if (accessControlCredentialsString != "true") {
            errorDescription = "Credentials flag is true, but Access-Control-Allow-Credentials is not \"true\".";
            return false;
        }
    }

    return true;
}

bool validatePreflightResponse(const ResourceRequest& request, const ResourceResponse& response, StoredCredentialsPolicy storedCredentialsPolicy, SecurityOrigin& securityOrigin, String& errorDescription)
{
    if (!response.isSuccessful()) {
        errorDescription = ASCIILiteral("Preflight response is not successful");
        return false;
    }

    if (!passesAccessControlCheck(response, storedCredentialsPolicy, securityOrigin, errorDescription))
        return false;

    auto result = std::make_unique<CrossOriginPreflightResultCacheItem>(storedCredentialsPolicy);
    if (!result->parse(response, errorDescription)
        || !result->allowsCrossOriginMethod(request.httpMethod(), errorDescription)
        || !result->allowsCrossOriginHeaders(request.httpHeaderFields(), errorDescription)) {
        return false;
    }

    CrossOriginPreflightResultCache::singleton().appendEntry(securityOrigin.toString(), request.url(), WTFMove(result));
    return true;
}

} // namespace WebCore
