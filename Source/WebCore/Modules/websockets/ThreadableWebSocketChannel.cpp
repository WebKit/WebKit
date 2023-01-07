/*
 * Copyright (C) 2009, 2012 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ThreadableWebSocketChannel.h"

#include "ContentRuleListResults.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "HTTPHeaderValues.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "SocketProvider.h"
#include "ThreadableWebSocketChannelClientWrapper.h"
#include "UserContentProvider.h"
#include "WebSocketChannel.h"
#include "WebSocketChannelClient.h"
#include "WorkerGlobalScope.h"
#include "WorkerRunLoop.h"
#include "WorkerThread.h"
#include "WorkerThreadableWebSocketChannel.h"

namespace WebCore {

Ref<ThreadableWebSocketChannel> ThreadableWebSocketChannel::create(Document& document, WebSocketChannelClient& client, SocketProvider& provider)
{
#if USE(CURL) || USE(SOUP)
    bool enabled = true;
#elif HAVE(NSURLSESSION_WEBSOCKET)
    bool enabled = document.settings().isNSURLSessionWebSocketEnabled();
#else
    bool enabled = false;
#endif
    if (enabled) {
        if (auto channel = provider.createWebSocketChannel(document, client))
            return channel.releaseNonNull();
    }

#if USE(SOUP)
    RELEASE_ASSERT_NOT_REACHED();
#else
    return WebSocketChannel::create(document, client, provider);
#endif
}

Ref<ThreadableWebSocketChannel> ThreadableWebSocketChannel::create(ScriptExecutionContext& context, WebSocketChannelClient& client, SocketProvider& provider)
{
    if (is<WorkerGlobalScope>(context)) {
        WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(context);
        WorkerRunLoop& runLoop = workerGlobalScope.thread().runLoop();
        return WorkerThreadableWebSocketChannel::create(workerGlobalScope, client, makeString("webSocketChannelMode", runLoop.createUniqueId()), provider);
    }

    return create(downcast<Document>(context), client, provider);
}

ThreadableWebSocketChannel::ThreadableWebSocketChannel()
    : m_identifier(WebSocketIdentifier::generateThreadSafe())
{
}

std::optional<ThreadableWebSocketChannel::ValidatedURL> ThreadableWebSocketChannel::validateURL(Document& document, const URL& requestedURL)
{
    ValidatedURL validatedURL { requestedURL, true };
    if (auto* page = document.page()) {
        if (!page->allowsLoadFromURL(requestedURL, MainFrameMainResource::No))
            return { };
#if ENABLE(CONTENT_EXTENSIONS)
        if (auto* documentLoader = document.loader()) {
            auto results = page->userContentProvider().processContentRuleListsForLoad(*page, validatedURL.url, ContentExtensions::ResourceType::WebSocket, *documentLoader);
            if (results.summary.blockedLoad)
                return { };
            if (results.summary.madeHTTPS) {
                ASSERT(validatedURL.url.protocolIs("ws"_s));
                validatedURL.url.setProtocol("wss"_s);
            }
            validatedURL.areCookiesAllowed = !results.summary.blockedCookies;
        }
#else
        UNUSED_PARAM(document);
#endif
    }
    return validatedURL;
}

std::optional<ResourceRequest> ThreadableWebSocketChannel::webSocketConnectRequest(Document& document, const URL& url)
{
    auto validatedURL = validateURL(document, url);
    if (!validatedURL)
        return { };

    ResourceRequest request { validatedURL->url };
    request.setHTTPUserAgent(document.userAgent(validatedURL->url));
    request.setDomainForCachePartition(document.domainForCachePartition());
    request.setAllowCookies(validatedURL->areCookiesAllowed);
    request.setFirstPartyForCookies(document.firstPartyForCookies());
    request.setHTTPHeaderField(HTTPHeaderName::Origin, document.securityOrigin().toString());

    if (auto* documentLoader = document.loader())
        request.setIsAppInitiated(documentLoader->lastNavigationWasAppInitiated());

    FrameLoader::addSameSiteInfoToRequestIfNeeded(request, &document);

    // Add no-cache headers to avoid compatibility issue.
    // There are some proxies that rewrite "Connection: upgrade"
    // to "Connection: close" in the response if a request doesn't contain
    // these headers.
    request.addHTTPHeaderField(HTTPHeaderName::Pragma, HTTPHeaderValues::noCache());
    request.addHTTPHeaderField(HTTPHeaderName::CacheControl, HTTPHeaderValues::noCache());

    auto httpURL = request.url();
    httpURL.setProtocol(url.protocolIs("ws"_s) ? "http"_s : "https"_s);
    auto requestOrigin = SecurityOrigin::create(httpURL);
    if (document.settings().fetchMetadataEnabled() && requestOrigin->isPotentiallyTrustworthy()) {
        request.addHTTPHeaderField(HTTPHeaderName::SecFetchDest, "websocket"_s);
        request.addHTTPHeaderField(HTTPHeaderName::SecFetchMode, "websocket"_s);

        if (document.securityOrigin().isSameOriginAs(requestOrigin.get()))
            request.addHTTPHeaderField(HTTPHeaderName::SecFetchSite, "same-origin"_s);
        else if (document.securityOrigin().isSameSiteAs(requestOrigin))
            request.addHTTPHeaderField(HTTPHeaderName::SecFetchSite, "same-site"_s);
        else
            request.addHTTPHeaderField(HTTPHeaderName::SecFetchSite, "cross-site"_s);
    }

    return request;
}

} // namespace WebCore
