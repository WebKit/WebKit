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

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "Frame.h"
#include "HTTPParsers.h"
#include "Navigator.h"
#include <wtf/URL.h>

namespace WebCore {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : m_navigator(navigator)
{
}

NavigatorBeacon::~NavigatorBeacon()
{
    for (auto& beacon : m_inflightBeacons)
        beacon->removeClient(*this);
}

NavigatorBeacon* NavigatorBeacon::from(Navigator& navigator)
{
    auto* supplement = static_cast<NavigatorBeacon*>(Supplement<Navigator>::from(&navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = std::make_unique<NavigatorBeacon>(navigator);
        supplement = newSupplement.get();
        provideTo(&navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

const char* NavigatorBeacon::supplementName()
{
    return "NavigatorBeacon";
}

void NavigatorBeacon::notifyFinished(CachedResource& resource)
{
    if (!resource.resourceError().isNull())
        logError(resource.resourceError());

    resource.removeClient(*this);
    bool wasRemoved = m_inflightBeacons.removeFirst(&resource);
    ASSERT_UNUSED(wasRemoved, wasRemoved);
    ASSERT(!m_inflightBeacons.contains(&resource));
}

void NavigatorBeacon::logError(const ResourceError& error)
{
    ASSERT(!error.isNull());

    auto* frame = m_navigator.frame();
    if (!frame)
        return;

    auto* document = frame->document();
    if (!document)
        return;

    ASCIILiteral messageMiddle { ". "_s };
    String description = error.localizedDescription();
    if (description.isEmpty()) {
        if (error.isAccessControl())
            messageMiddle = " due to access control checks."_s;
        else
            messageMiddle = "."_s;
    }

    document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, makeString("Beacon API cannot load "_s, error.failingURL().string(), messageMiddle, description));
}

ExceptionOr<bool> NavigatorBeacon::sendBeacon(Document& document, const String& url, Optional<FetchBody::Init>&& body)
{
    URL parsedUrl = document.completeURL(url);

    // Set parsedUrl to the result of the URL parser steps with url and base. If the algorithm returns an error, or if
    // parsedUrl's scheme is not "http" or "https", throw a "TypeError" exception and terminate these steps.
    if (!parsedUrl.isValid())
        return Exception { TypeError, "This URL is invalid"_s };
    if (!parsedUrl.protocolIsInHTTPFamily())
        return Exception { TypeError, "Beacons can only be sent over HTTP(S)"_s };

    if (!document.frame())
        return false;

    auto& contentSecurityPolicy = *document.contentSecurityPolicy();
    if (!document.shouldBypassMainWorldContentSecurityPolicy() && !contentSecurityPolicy.allowConnectToSource(parsedUrl)) {
        // We simulate a network error so we return true here. This is consistent with Blink.
        return true;
    }

    ResourceRequest request(parsedUrl);
    request.setHTTPMethod("POST"_s);

    FetchOptions options;
    options.credentials = FetchOptions::Credentials::Include;
    options.cache = FetchOptions::Cache::NoCache;
    options.keepAlive = true;
    if (body) {
        options.mode = FetchOptions::Mode::Cors;
        String mimeType;
        auto fetchBody = FetchBody::extract(document, WTFMove(body.value()), mimeType);

        if (fetchBody.hasReadableStream())
            return Exception { TypeError, "Beacons cannot send ReadableStream body"_s };

        request.setHTTPBody(fetchBody.bodyAsFormData(document));
        if (!mimeType.isEmpty()) {
            request.setHTTPContentType(mimeType);
            if (isCrossOriginSafeRequestHeader(HTTPHeaderName::ContentType, mimeType))
                options.mode = FetchOptions::Mode::NoCors;
        }
    }

    auto cachedResource = document.cachedResourceLoader().requestBeaconResource({ WTFMove(request), options });
    if (!cachedResource) {
        logError(cachedResource.error());
        return false;
    }

    ASSERT(!m_inflightBeacons.contains(cachedResource.value().get()));
    m_inflightBeacons.append(cachedResource.value().get());
    cachedResource.value()->addClient(*this);
    return true;
}

ExceptionOr<bool> NavigatorBeacon::sendBeacon(Navigator& navigator, Document& document, const String& url, Optional<FetchBody::Init>&& body)
{
    return NavigatorBeacon::from(navigator)->sendBeacon(document, url, WTFMove(body));
}

}

