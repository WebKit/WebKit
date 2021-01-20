/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "APIResourceLoadClient.h"
#import "WKFoundation.h"
#import <wtf/WeakObjCPtr.h>

@class WKWebView;

@protocol _WKResourceLoadDelegate;

namespace API {
class ResourceLoadClient;
}

namespace WebKit {

class ResourceLoadDelegate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ResourceLoadDelegate(WKWebView *);
    ~ResourceLoadDelegate();

    std::unique_ptr<API::ResourceLoadClient> createResourceLoadClient();

    RetainPtr<id<_WKResourceLoadDelegate>> delegate();
    void setDelegate(id<_WKResourceLoadDelegate>);

private:
    class ResourceLoadClient : public API::ResourceLoadClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit ResourceLoadClient(ResourceLoadDelegate&);
        ~ResourceLoadClient();

    private:
        // API::ResourceLoadClient
        void didSendRequest(ResourceLoadInfo&&, WebCore::ResourceRequest&&) const final;
        void didPerformHTTPRedirection(ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&) const final;
        void didReceiveChallenge(ResourceLoadInfo&&, WebCore::AuthenticationChallenge&&) const final;
        void didReceiveResponse(ResourceLoadInfo&&, WebCore::ResourceResponse&&) const final;
        void didCompleteWithError(ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceError&&) const final;

        ResourceLoadDelegate& m_resourceLoadDelegate;
    };

    WeakObjCPtr<WKWebView> m_webView;
    WeakObjCPtr<id <_WKResourceLoadDelegate> > m_delegate;

    struct {
        bool didSendRequest : 1;
        bool didPerformHTTPRedirection : 1;
        bool didReceiveChallenge : 1;
        bool didReceiveResponse : 1;
        bool didCompleteWithError : 1;
    } m_delegateMethods;
};

} // namespace WebKit
