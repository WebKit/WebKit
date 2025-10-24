/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebLocalFrameLoaderClient.h"

#import "APIInjectedBundlePageResourceLoadClient.h"
#import "WebFrame.h"

namespace WebKit {

WebCore::IntPoint WebLocalFrameLoaderClient::accessibilityRemoteFrameOffset()
{
    RefPtr webPage = m_frame->page();
    return webPage ? webPage->accessibilityRemoteFrameOffset() : IntPoint();
}

RemoteAXObjectRef WebLocalFrameLoaderClient::accessibilityRemoteObject()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return 0;

    return webPage->accessibilityRemoteObject();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void WebLocalFrameLoaderClient::setIsolatedTree(Ref<WebCore::AXIsolatedTree>&& tree)
{
    ASSERT(isMainRunLoop());
    if (RefPtr webPage = m_frame->page())
        webPage->setIsolatedTree(WTFMove(tree));
}
#endif

void WebLocalFrameLoaderClient::willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier identifier, NSCachedURLResponse* response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return completionHandler(response);

    return completionHandler(webPage->injectedBundleResourceLoadClient().shouldCacheResponse(*webPage, m_frame, identifier) ? response : nil);
}

std::optional<double> WebLocalFrameLoaderClient::dataDetectionReferenceDate()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return std::nullopt;

    return webPage->dataDetectionReferenceDate();
}

} // namespace WebKit
