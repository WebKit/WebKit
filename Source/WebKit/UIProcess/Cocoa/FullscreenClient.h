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
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(MAC)
@class NSView;
using WKFullscreenClientView = NSView;
#else
@class WKWebView;
using WKFullscreenClientView = WKWebView;
#endif

@protocol _WKFullscreenDelegate;

namespace WebKit {

class FullscreenClient : public API::FullscreenClient {
public:
    explicit FullscreenClient(WKFullscreenClientView *);
    ~FullscreenClient() { };

    bool isType(API::FullscreenClient::Type target) const override { return target == API::FullscreenClient::WebKitType; };

    RetainPtr<id<_WKFullscreenDelegate>> delegate();
    void setDelegate(id<_WKFullscreenDelegate>);

    void willEnterFullscreen(WebPageProxy*) override;
    void didEnterFullscreen(WebPageProxy*) override;
    void willExitFullscreen(WebPageProxy*) override;
    void didExitFullscreen(WebPageProxy*) override;

private:
    WKFullscreenClientView *m_webView;
    WeakObjCPtr<id <_WKFullscreenDelegate> > m_delegate;

    struct {
#if PLATFORM(MAC)
        bool webViewWillEnterFullscreen : 1;
        bool webViewDidEnterFullscreen : 1;
        bool webViewWillExitFullscreen : 1;
        bool webViewDidExitFullscreen : 1;
#else
        bool webViewWillEnterElementFullscreen : 1;
        bool webViewDidEnterElementFullscreen : 1;
        bool webViewWillExitElementFullscreen : 1;
        bool webViewDidExitElementFullscreen : 1;
#endif
    } m_delegateMethods;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::FullscreenClient) \
static bool isType(const API::FullscreenClient& client) { return client.isType(API::FullscreenClient::WebKitType); } \
SPECIALIZE_TYPE_TRAITS_END()
