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

#import "_WKFullscreenDelegate.h"

namespace WebKit {

FullscreenClient::FullscreenClient(WKFullscreenClientView *webView)
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
#endif
}

void FullscreenClient::willEnterFullscreen(WebPageProxy*)
{
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewWillEnterFullscreen)
        [m_delegate.get() _webViewWillEnterFullscreen:m_webView];
#else
    if (m_delegateMethods.webViewWillEnterElementFullscreen)
        [m_delegate.get() _webViewWillEnterElementFullscreen:m_webView];
#endif
}

void FullscreenClient::didEnterFullscreen(WebPageProxy*)
{
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewDidEnterFullscreen)
        [m_delegate.get() _webViewDidEnterFullscreen:m_webView];
#else
    if (m_delegateMethods.webViewDidEnterElementFullscreen)
        [m_delegate.get() _webViewDidEnterElementFullscreen:m_webView];
#endif
}

void FullscreenClient::willExitFullscreen(WebPageProxy*)
{
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewWillExitFullscreen)
        [m_delegate.get() _webViewWillExitFullscreen:m_webView];
#else
    if (m_delegateMethods.webViewWillExitElementFullscreen)
        [m_delegate.get() _webViewWillExitElementFullscreen:m_webView];
#endif
}

void FullscreenClient::didExitFullscreen(WebPageProxy*)
{
#if PLATFORM(MAC)
    if (m_delegateMethods.webViewDidExitFullscreen)
        [m_delegate.get() _webViewDidExitFullscreen:m_webView];
#else
    if (m_delegateMethods.webViewDidExitElementFullscreen)
        [m_delegate.get() _webViewDidExitElementFullscreen:m_webView];
#endif
}

} // namespace WebKit
