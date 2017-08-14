/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "NavigatorBeacon.h"

#include "CachedResourceLoader.h"
#include "Document.h"
#include "FetchBody.h"
#include "HTTPParsers.h"
#include "Navigator.h"
#include "URL.h"

namespace WebCore {

ExceptionOr<bool> NavigatorBeacon::sendBeacon(Navigator&, Document& document, const String& url, std::optional<FetchBody::Init>&& body)
{
    URL parsedUrl = document.completeURL(url);

    // Set parsedUrl to the result of the URL parser steps with url and base. If the algorithm returns an error, or if
    // parsedUrl's scheme is not "http" or "https", throw a " TypeError" exception and terminate these steps.
    if (!parsedUrl.isValid())
        return Exception { TypeError, ASCIILiteral("This URL is invalid") };
    if (!parsedUrl.protocolIsInHTTPFamily())
        return Exception { TypeError, ASCIILiteral("Beacons can only be sent over HTTP(S)") };

    if (!document.frame())
        return false;

    auto& contentSecurityPolicy = *document.contentSecurityPolicy();
    if (!document.shouldBypassMainWorldContentSecurityPolicy() && !contentSecurityPolicy.allowConnectToSource(parsedUrl)) {
        // We simulate a network error so we return true here. This is consistent with Blink.
        return true;
    }

    ResourceRequest request(parsedUrl);
    request.setHTTPMethod(ASCIILiteral("POST"));

    FetchOptions options;
    options.credentials = FetchOptions::Credentials::Include;
    options.cache = FetchOptions::Cache::NoCache;
    options.keepAlive = true;
    if (body) {
        options.mode = FetchOptions::Mode::Cors;
        String mimeType;
        auto fetchBody = FetchBody::extract(document, WTFMove(body.value()), mimeType);
        request.setHTTPBody(fetchBody.bodyForInternalRequest(document));
        if (!mimeType.isEmpty()) {
            request.setHTTPContentType(mimeType);
            if (isCrossOriginSafeRequestHeader(HTTPHeaderName::ContentType, mimeType))
                options.mode = FetchOptions::Mode::NoCors;
        }
    }
    document.cachedResourceLoader().requestBeaconResource({ WTFMove(request), options });
    return true;
}

}

