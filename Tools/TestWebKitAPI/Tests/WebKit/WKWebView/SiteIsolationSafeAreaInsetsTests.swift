// Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if WTF_PLATFORM_IOS_FAMILY

private import TestWebKitAPILibrary
import Testing
import UIKit
import WebKit
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationSafeAreaInsetsTests {
    @Test(.timeLimit(.minutes(1)))
    func crossSiteIframesUseSafeAreaInsetsWhenTheyChangeAndInNewProcesses() async throws {
        let insetElement = """
            <div id='inset' style='padding-top: env(safe-area-inset-top); padding-left: env(safe-area-inset-left)'></div>
            """
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                \(insetElement)
                <iframe src='https://webkit.org/iframe'></iframe>
                """
            }

            Route("/iframe") {
                insetElement
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            webView._unobscuredSafeAreaInsets = UIEdgeInsets(top: 20, left: 10, bottom: 0, right: 0)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            func insetPadding(in frame: WKFrameInfo? = nil) async throws -> String {
                try await webView.callJavaScript(returning: String.self, in: frame) {
                    """
                    let element = document.getElementById('inset');
                    if (!element)
                        return '';
                    let style = getComputedStyle(element);
                    return `${style.paddingTop} ${style.paddingLeft}`;
                    """
                }
            }

            try await waitForCondition("the main frame to use the safe area insets") {
                try await insetPadding() == "20px 10px"
            }
            let childFrame = try #require(await webView.firstChildFrame)
            try await waitForCondition("the cross-site iframe to use the safe area insets") {
                try await insetPadding(in: childFrame) == "20px 10px"
            }

            webView._unobscuredSafeAreaInsets = UIEdgeInsets(top: 30, left: 15, bottom: 0, right: 0)
            try await waitForCondition("the cross-site iframe to use the new safe area insets") {
                try await insetPadding(in: childFrame) == "30px 15px"
            }

            try await webView.callJavaScript { "document.querySelector('iframe').src = 'https://apple.com/iframe'" }
            var navigatedChildFrame: WKFrameInfo?
            try await waitForCondition("the iframe to commit a load from a new site") {
                navigatedChildFrame = await webView.firstChildFrame
                return navigatedChildFrame?.securityOrigin.host == "apple.com"
            }
            let newSiteChildFrame = try #require(navigatedChildFrame)
            try await waitForCondition("the iframe in the new site's process to use the safe area insets") {
                try await insetPadding(in: newSiteChildFrame) == "30px 15px"
            }
        }
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY
