// Copyright (C) 2024 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if ENABLE_SWIFTUI

import Foundation

@_spi(Internal)
@MainActor public class WebPage_v0 {
    public let navigations: Navigations

    private let backingNavigationDelegate: WKNavigationDelegateAdapter

    private lazy var backingWebView: WKWebView = {
        let webView = WKWebView(frame: .zero)
        webView.navigationDelegate = backingNavigationDelegate
        return webView
    }()

    public init() {
        let (stream, continuation) = AsyncStream.makeStream(of: NavigationEvent.self)
        self.navigations = Navigations(source: stream)
        self.backingNavigationDelegate = WKNavigationDelegateAdapter(navigationProgressContinuation: continuation)
    }

    @discardableResult public func load(_ request: URLRequest) -> NavigationID? {
        self.backingWebView.load(request).map(NavigationID.init(_:))
    }

    @discardableResult public func load(htmlString: String, baseURL: URL) -> NavigationID? {
        self.backingWebView.loadHTMLString(htmlString, baseURL: baseURL).map(NavigationID.init(_:))
    }
}

#endif
