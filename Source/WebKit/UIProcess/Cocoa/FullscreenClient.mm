/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "FullscreenClient.h"

#import "WKWebViewInternal.h"
#import "_WKFullscreenDelegate.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FullscreenClient);

FullscreenClient::FullscreenClient(WKWebView *webView)
    : m_webView(webView)
{
}

RetainPtr<id <_WKFullscreenDelegate>> FullscreenClient::delegate()
{
    return m_delegate.get();
}

void FullscreenClient::setDelegate(id <_WKFullscreenDelegate> delegate)
{
    m_delegate = delegate;

#if PLATFORM(MAC)
    m_delegateMethods.webViewWillEnterFullscreen = [delegate respondsToSelector:@selector(_webViewWillEnterFullscreen:)];
    m_delegateMethods.webViewDidEnterFullscreen = [delegate respondsToSelector:@selector(_webViewDidEnterFullscreen:)];
    m_delegateMethods.webViewWillExitFullscreen = [delegate respondsToSelector:@selector(_webViewWillExitFullscreen:)];
    m_delegateMethods.webViewDidExitFullscreen = [delegate respondsToSelector:@selector(_webViewDidExitFullscreen:)];
#else
    m_delegateMethods.webViewWillEnterElementFullscreen = [delegate respondsToSelector:@selector(_webViewWillEnterElementFullscreen:)];
    m_delegateMethods.webViewDidEnterElementFullscreen = [delegate respondsToSelector:@selector(_webViewDidEnterElementFullscreen:)];
    m_delegateMethods.webViewWillExitElementFullscreen = [delegate respondsToSelector:@selector(_webViewWillExitElementFullscreen:)];
    m_delegateMethods.webViewDidExitElementFullscreen = [delegate respondsToSelector:@selector(_webViewDidExitElementFullscreen:)];
    m_delegateMethods.webViewRequestPresentingViewController = [delegate respondsToSelector:@selector(_webView:requestPresentingViewControllerWithCompletionHandler:)];
#endif
#if ENABLE(QUICKLOOK_FULLSCREEN)
    m_delegateMethods.webViewDidFullscreenImageWithQuickLook = [delegate respondsToSelector:@selector(_webView:didFullscreenImageWithQuickLook:)];
#endif
#if ENABLE(LINEAR_MEDIA_PLAYER)
    m_delegateMethods.webViewPreventDockingFromElementFullscreen = [delegate respondsToSelector:@selector(_webViewPreventDockingFromElementFullscreen:)];
#endif
}

void FullscreenClient::willEnterFullscreen(WebPageProxy*)
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"fullscreenState"];
    [webView didChangeValueForKey:@"fullscreenState"];
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewWillEnterFullscreen)
        [m_delegate.get() _webViewWillEnterFullscreen:webView.get()];
#else
    if (m_delegateMethods.webViewWillEnterElementFullscreen)
        [m_delegate.get() _webViewWillEnterElementFullscreen:webView.get()];
#endif

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    [webView _addReasonToHideTopScrollPocket:HideScrollPocketReason::FullScreen];
#endif
}

void FullscreenClient::didEnterFullscreen(WebPageProxy*)
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"fullscreenState"];
    [webView didChangeValueForKey:@"fullscreenState"];
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewDidEnterFullscreen)
        [m_delegate.get() _webViewDidEnterFullscreen:webView.get()];
#else
    if (m_delegateMethods.webViewDidEnterElementFullscreen)
        [m_delegate.get() _webViewDidEnterElementFullscreen:webView.get()];
#endif

#if ENABLE(QUICKLOOK_FULLSCREEN)
    if (auto fullScreenController = [webView fullScreenWindowController]) {
        CGSize imageDimensions = fullScreenController.imageDimensions;
        if (fullScreenController.isUsingQuickLook && m_delegateMethods.webViewDidFullscreenImageWithQuickLook)
            [m_delegate.get() _webView:webView.get() didFullscreenImageWithQuickLook:imageDimensions];
    }
#endif // ENABLE(QUICKLOOK_FULLSCREEN)
}

void FullscreenClient::willExitFullscreen(WebPageProxy*)
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"fullscreenState"];
    [webView didChangeValueForKey:@"fullscreenState"];
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewWillExitFullscreen)
        [m_delegate.get() _webViewWillExitFullscreen:webView.get()];
#else
    if (m_delegateMethods.webViewWillExitElementFullscreen)
        [m_delegate.get() _webViewWillExitElementFullscreen:webView.get()];
#endif

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    [webView _removeReasonToHideTopScrollPocket:HideScrollPocketReason::FullScreen];
#endif
}

void FullscreenClient::didExitFullscreen(WebPageProxy*)
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"fullscreenState"];
    [webView didChangeValueForKey:@"fullscreenState"];
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewDidExitFullscreen)
        [m_delegate.get() _webViewDidExitFullscreen:webView.get()];
#else
    if (m_delegateMethods.webViewDidExitElementFullscreen)
        [m_delegate.get() _webViewDidExitElementFullscreen:webView.get()];
#endif
}

#if PLATFORM(IOS_FAMILY)
void FullscreenClient::requestPresentingViewController(CompletionHandler<void(UIViewController *, NSError *)>&& completionHandler)
{
    if (!m_delegateMethods.webViewRequestPresentingViewController)
        return completionHandler(nil, nil);

    [m_delegate _webView:m_webView.get().get() requestPresentingViewControllerWithCompletionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
}
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
bool FullscreenClient::preventDocking(WebPageProxy*)
{
    RetainPtr webView = m_webView.get();
    if (m_delegateMethods.webViewPreventDockingFromElementFullscreen)
        return [m_delegate.get() _webViewPreventDockingFromElementFullscreen:webView.get()];

    return false;
}
#endif
} // namespace WebKit
