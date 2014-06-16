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

#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
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

bool isOnAccessControlSimpleRequestHeaderWhitelist(const String& name, const String& value)
{
    if (equalIgnoringCase(name, "accept")
        || equalIgnoringCase(name, "accept-language")
        || equalIgnoringCase(name, "content-language")
        || equalIgnoringCase(name, "origin")
        || equalIgnoringCase(name, "referer"))
        return true;

    // Preflight is required for MIME types that can not be sent via form submission.
    if (equalIgnoringCase(name, "content-type")) {
        String mimeType = extractMIMETypeFromMediaType(value);
        return equalIgnoringCase(mimeType, "application/x-www-form-urlencoded")
            || equalIgnoringCase(mimeType, "multipart/form-data")
            || equalIgnoringCase(mimeType, "text/plain");
    }

    return false;
}

bool isSimpleCrossOriginAccessRequest(const String& method, const HTTPHeaderMap& headerMap)
{
    if (!isOnAccessControlSimpleRequestMethodWhitelist(method))
        return false;

    for (const auto& header : headerMap) {
        if (!isOnAccessControlSimpleRequestHeaderWhitelist(header.key, header.value))
            return false;
    }

    return true;
}

bool isOnAccessControlResponseHeaderWhitelist(const String& name)
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<HTTPHeaderSet> allowedCrossOriginResponseHeaders;

    std::call_once(onceFlag, []{
        allowedCrossOriginResponseHeaders.construct<std::initializer_list<String>>({
            "cache-control",
            "content-language",
            "content-type",
            "expires",
            "last-modified",
            "pragma"
        });
    });

    return allowedCrossOriginResponseHeaders.get().contains(name);
}

void updateRequestForAccessControl(ResourceRequest& request, SecurityOrigin* securityOrigin, StoredCredentials allowCredentials)
{
    request.removeCredentials();
    request.setAllowCookies(allowCredentials == AllowStoredCredentials);
    request.setHTTPOrigin(securityOrigin->toString());
}

ResourceRequest createAccessControlPreflightRequest(const ResourceRequest& request, SecurityOrigin* securityOrigin)
{
    ResourceRequest preflightRequest(request.url());
    updateRequestForAccessControl(preflightRequest, securityOrigin, DoNotAllowStoredCredentials);
    preflightRequest.setHTTPMethod("OPTIONS");
    preflightRequest.setHTTPHeaderField(HTTPHeaderName::AccessControlRequestMethod, request.httpMethod());
    preflightRequest.setPriority(request.priority());

    const HTTPHeaderMap& requestHeaderFields = request.httpHeaderFields();

    if (!requestHeaderFields.isEmpty()) {
        StringBuilder headerBuffer;
        
        bool appendComma = false;
        for (const auto& headerField : requestHeaderFields) {
            if (appendComma)
                headerBuffer.appendLiteral(", ");
            else
                appendComma = true;
            
            headerBuffer.append(headerField.key);
        }

        preflightRequest.setHTTPHeaderField(HTTPHeaderName::AccessControlRequestHeaders, headerBuffer.toString().lower());
    }

    return preflightRequest;
}

bool passesAccessControlCheck(const ResourceResponse& response, StoredCredentials includeCredentials, SecurityOrigin* securityOrigin, String& errorDescription)
{
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are to be sent,
    // even with Access-Control-Allow-Credentials set to true.
    const String& accessControlOriginString = response.httpHeaderField(HTTPHeaderName::AccessControlAllowOrigin);
    if (accessControlOriginString == "*" && includeCredentials == DoNotAllowStoredCredentials)
        return true;

    if (securityOrigin->isUnique()) {
        errorDescription = "Cannot make any requests from " + securityOrigin->toString() + ".";
        return false;
    }

    // FIXME: Access-Control-Allow-Origin can contain a list of origins.
    if (accessControlOriginString != securityOrigin->toString()) {
        if (accessControlOriginString == "*")
            errorDescription = "Cannot use wildcard in Access-Control-Allow-Origin when credentials flag is true.";
        else
            errorDescription =  "Origin " + securityOrigin->toString() + " is not allowed by Access-Control-Allow-Origin.";
        return false;
    }

    if (includeCredentials == AllowStoredCredentials) {
        const String& accessControlCredentialsString = response.httpHeaderField(HTTPHeaderName::AccessControlAllowCredentials);
        if (accessControlCredentialsString != "true") {
            errorDescription = "Credentials flag is true, but Access-Control-Allow-Credentials is not \"true\".";
            return false;
        }
    }

    return true;
}

void parseAccessControlExposeHeadersAllowList(const String& headerValue, HTTPHeaderSet& headerSet)
{
    Vector<String> headers;
    headerValue.split(',', false, headers);
    for (unsigned headerCount = 0; headerCount < headers.size(); headerCount++) {
        String strippedHeader = headers[headerCount].stripWhiteSpace();
        if (!strippedHeader.isEmpty())
            headerSet.add(strippedHeader);
    }
}

} // namespace WebCore
