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

#import "APIIconLoadingClient.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

@class WKWebView;
@protocol _WKIconLoadingDelegate;

namespace WebKit {

class IconLoadingDelegate {
public:
    explicit IconLoadingDelegate(WKWebView *);
    ~IconLoadingDelegate();

    std::unique_ptr<API::IconLoadingClient> createIconLoadingClient();

    RetainPtr<id <_WKIconLoadingDelegate> > delegate();
    void setDelegate(id <_WKIconLoadingDelegate>);

private:
    class IconLoadingClient : public API::IconLoadingClient {
    public:
        explicit IconLoadingClient(IconLoadingDelegate&);
        ~IconLoadingClient();

    private:
        void getLoadDecisionForIcon(const WebCore::LinkIcon&, WTF::CompletionHandler<void(WTF::Function<void(API::Data*, WebKit::CallbackBase::Error)>&&)>&&) override;

        IconLoadingDelegate& m_iconLoadingDelegate;
    };

    WKWebView *m_webView;
    WeakObjCPtr<id <_WKIconLoadingDelegate> > m_delegate;

    struct {
        bool webViewShouldLoadIconWithParametersCompletionHandler : 1;
    } m_delegateMethods;
};

} // namespace WebKit
