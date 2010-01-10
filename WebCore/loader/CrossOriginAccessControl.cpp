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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "AtomicString.h"
#include "HTTPParsers.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include <wtf/Threading.h>

namespace WebCore {

bool isOnAccessControlSimpleRequestMethodWhitelist(const String& method)
{
    return method == "GET" || method == "HEAD" || method == "POST";
}

bool isOnAccessControlSimpleRequestHeaderWhitelist(const String& name, const String& value)
{
    if (equalIgnoringCase(name, "accept") || equalIgnoringCase(name, "accept-language") || equalIgnoringCase(name, "content-language"))
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

    HTTPHeaderMap::const_iterator end = headerMap.end();
    for (HTTPHeaderMap::const_iterator it = headerMap.begin(); it != end; ++it) {
        if (!isOnAccessControlSimpleRequestHeaderWhitelist(it->first, it->second))
            return false;
    }

    return true;
}

typedef HashSet<String, CaseFoldingHash> HTTPHeaderSet;
static HTTPHeaderSet* createAllowedCrossOriginResponseHeadersSet()
{
    HTTPHeaderSet* headerSet = new HashSet<String, CaseFoldingHash>;
    
    headerSet->add("cache-control");
    headerSet->add("content-language");
    headerSet->add("content-type");
    headerSet->add("expires");
    headerSet->add("last-modified");
    headerSet->add("pragma");

    return headerSet;
}

bool isOnAccessControlResponseHeaderWhitelist(const String& name)
{
    AtomicallyInitializedStatic(HTTPHeaderSet*, allowedCrossOriginResponseHeaders = createAllowedCrossOriginResponseHeadersSet());

    return allowedCrossOriginResponseHeaders->contains(name);
}

bool passesAccessControlCheck(const ResourceResponse& response, bool includeCredentials, SecurityOrigin* securityOrigin)
{
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are to be sent,
    // even with Access-Control-Allow-Credentials set to true.
    const String& accessControlOriginString = response.httpHeaderField("Access-Control-Allow-Origin");
    if (accessControlOriginString == "*" && !includeCredentials)
        return true;

    if (securityOrigin->isUnique())
        return false;

    RefPtr<SecurityOrigin> accessControlOrigin = SecurityOrigin::createFromString(accessControlOriginString);
    if (!accessControlOrigin->isSameSchemeHostPort(securityOrigin))
        return false;

    if (includeCredentials) {
        const String& accessControlCredentialsString = response.httpHeaderField("Access-Control-Allow-Credentials");
        if (accessControlCredentialsString != "true")
            return false;
    }

    return true;
}

} // namespace WebCore
