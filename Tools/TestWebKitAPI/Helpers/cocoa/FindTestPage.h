/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#ifdef __cplusplus

#import "HTTPServer.h"
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

@class TestNavigationDelegate;
@class TestWKWebView;
@class WKWebViewConfiguration;

namespace TestWebKitAPI {

struct FrameSpec {
    String host { "a.com"_s };
    String body;
    String iframeAttributes;
    Vector<FrameSpec> children;
};

enum class SiteIsolation : bool { Off, On };

class FindTestPage {
public:
    FindTestPage(const FrameSpec& root, SiteIsolation, RetainPtr<WKWebViewConfiguration> = nil, HTTPServer::ResponseMap&& resources = { });
    FindTestPage(const String& path, SiteIsolation, RetainPtr<WKWebViewConfiguration>, HTTPServer::ResponseMap&& resources);
    ~FindTestPage();

    TestWKWebView *webView() const { return m_webView.get(); }

private:
    void load(const String& url, SiteIsolation, RetainPtr<WKWebViewConfiguration>);

    HTTPServer m_server;
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<TestNavigationDelegate> m_navigationDelegate;
};

} // namespace TestWebKitAPI

#endif // __cplusplus
