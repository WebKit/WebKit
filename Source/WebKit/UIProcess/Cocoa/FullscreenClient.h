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

#pragma once

#import "WKFoundation.h"

#import "APIFullscreenClient.h"
#import "WKWebView.h"
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakObjCPtr.h>

@protocol _WKFullscreenDelegate;

namespace WebKit {

class FullscreenClient final : public API::FullscreenClient {
    WTF_MAKE_TZONE_ALLOCATED(FullscreenClient);
public:
    explicit FullscreenClient(WKWebView *);
    ~FullscreenClient() { };

    bool isType(API::FullscreenClient::Type target) const final { return target == API::FullscreenClient::WebKitType; };

    RetainPtr<id<_WKFullscreenDelegate>> delegate();
    void setDelegate(id<_WKFullscreenDelegate>);

    void willEnterFullscreen(WebPageProxy*) final;
    void didEnterFullscreen(WebPageProxy*) final;
    void willExitFullscreen(WebPageProxy*) final;
    void didExitFullscreen(WebPageProxy*) final;

#if PLATFORM(IOS_FAMILY)
    void requestPresentingViewController(CompletionHandler<void(UIViewController *, NSError *)>&&) final;
#endif

private:
    WKWebView *m_webView;
    WeakObjCPtr<id <_WKFullscreenDelegate> > m_delegate;

    struct {
#if PLATFORM(MAC)
        bool webViewWillEnterFullscreen : 1 { false };
        bool webViewDidEnterFullscreen : 1 { false };
        bool webViewWillExitFullscreen : 1 { false };
        bool webViewDidExitFullscreen : 1 { false };
#else
        bool webViewWillEnterElementFullscreen : 1 { false };
        bool webViewDidEnterElementFullscreen : 1 { false };
        bool webViewWillExitElementFullscreen : 1 { false };
        bool webViewDidExitElementFullscreen : 1 { false };
#endif
#if ENABLE(QUICKLOOK_FULLSCREEN)
        bool webViewDidFullscreenImageWithQuickLook : 1 { false };
#endif
#if PLATFORM(IOS_FAMILY)
        bool webViewRequestPresentingViewController : 1 { false };
#endif
    } m_delegateMethods;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::FullscreenClient) \
static bool isType(const API::FullscreenClient& client) { return client.isType(API::FullscreenClient::WebKitType); } \
SPECIALIZE_TYPE_TRAITS_END()
