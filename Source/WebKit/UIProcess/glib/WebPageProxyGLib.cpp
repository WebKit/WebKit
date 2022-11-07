/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include "WebPageProxy.h"

#include "PageClient.h"
#include "WebKitWebResourceLoadManager.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/UserAgent.h>

namespace WebKit {

String WebPageProxy::userAgentForURL(const URL& url)
{
    if (url.isNull() || !preferences().needsSiteSpecificQuirks())
        return this->userAgent();

    auto userAgent = WebCore::standardUserAgentForURL(url);
    return userAgent.isNull() ? this->userAgent() : userAgent;
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

void WebPageProxy::saveRecentSearches(const String&, const Vector<WebCore::RecentSearch>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(const String&, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebPageProxy::didInitiateLoadForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceRequest&& request)
{
    if (auto* manager = pageClient().webResourceLoadManager())
        manager->didInitiateLoad(resourceID, frameID, WTFMove(request));
}

void WebPageProxy::didSendRequestForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceRequest&& request, WebCore::ResourceResponse&& redirectResponse)
{
    if (auto* manager = pageClient().webResourceLoadManager())
        manager->didSendRequest(resourceID, frameID, WTFMove(request), WTFMove(redirectResponse));
}

void WebPageProxy::didReceiveResponseForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceResponse&& response)
{
    if (auto* manager = pageClient().webResourceLoadManager())
        manager->didReceiveResponse(resourceID, frameID, WTFMove(response));
}

void WebPageProxy::didFinishLoadForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceError&& error)
{
    if (auto* manager = pageClient().webResourceLoadManager())
        manager->didFinishLoad(resourceID, frameID, WTFMove(error));
}

} // namespace WebKit
