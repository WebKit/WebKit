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

// The size of the web views, which leaves room to scroll in.
private let webViewFrame = CGRect(x: 0, y: 0, width: 800, height: 600)

// Exposes the safe area insets that a frame sees through env(), in the order top, right, bottom and left.
private let safeAreaInsetsStyle = """
    <style>
    :root {
        --insets: env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);
    }
    </style>
    """

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

    @Test(.timeLimit(.minutes(1)))
    func crossSiteIframeUsesSafeAreaInsetsSetBeforeLoad() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "\(safeAreaInsetsStyle)<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "\(safeAreaInsetsStyle)<body>iframe</body>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: webViewFrame
            )
            defer { extendLifetime(navigationDelegate) }
            webView._unobscuredSafeAreaInsets = UIEdgeInsets(top: 10, left: 20, bottom: 34, right: 40)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let expectedInsets = "10px 40px 34px 20px"
            try await waitForCondition("the main frame to use the safe area insets") {
                try await webView.envSafeAreaInsets() == expectedInsets
            }
            let childFrame = try #require(await webView.firstChildFrame)
            try await waitForCondition("the cross-site iframe to use the safe area insets") {
                try await webView.envSafeAreaInsets(in: childFrame) == expectedInsets
            }

            let iframeProcess = try #require(await webView.frameTreesInProcesses().processIdentifier(hosting: "https://webkit.org"))
            #expect(iframeProcess != webView._webProcessIdentifier)
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func crossSiteIframeUsesSafeAreaInsetsSetAfterLoad() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "\(safeAreaInsetsStyle)<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "\(safeAreaInsetsStyle)<body>iframe</body>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: webViewFrame
            )
            defer { extendLifetime(navigationDelegate) }

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            await webView.nextPresentationUpdate()

            // Nothing has set the insets yet, so the iframe starts with the same default insets as the main frame.
            let initialInsets = try await webView.envSafeAreaInsets()
            let childFrame = try #require(await webView.firstChildFrame)
            try await waitForCondition("the cross-site iframe to load with the default safe area insets") {
                try await webView.envSafeAreaInsets(in: childFrame) == initialInsets
            }
            let iframeProcess = try #require(await webView.frameTreesInProcesses().processIdentifier(hosting: "https://webkit.org"))
            #expect(iframeProcess != webView._webProcessIdentifier)

            let expectedInsets = "12px 48px 36px 24px"
            webView._unobscuredSafeAreaInsets = UIEdgeInsets(top: 12, left: 24, bottom: 36, right: 48)
            try await waitForCondition("the cross-site iframe to use the new safe area insets") {
                try await webView.envSafeAreaInsets(in: childFrame) == expectedInsets
            }
            #expect(try await webView.envSafeAreaInsets() == expectedInsets)
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func crossSiteIframeInNewProcessUsesSafeAreaInsetsAfterNavigatingToThirdSite() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "\(safeAreaInsetsStyle)<iframe id='frame' src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "\(safeAreaInsetsStyle)<body>webkit iframe</body>"
            }

            Route("/third") {
                "\(safeAreaInsetsStyle)<body>apple iframe</body>"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            // With the shared process, the iframe would join the process that already hosts it, rather than get a new one.
            configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: false)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration, frame: webViewFrame)
            defer { extendLifetime(navigationDelegate) }
            webView._unobscuredSafeAreaInsets = UIEdgeInsets(top: 15, left: 25, bottom: 35, right: 45)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let expectedInsets = "15px 45px 35px 25px"
            let firstSiteChildFrame = try #require(await webView.firstChildFrame)
            try await waitForCondition("the iframe from the first site to use the safe area insets") {
                try await webView.envSafeAreaInsets(in: firstSiteChildFrame) == expectedInsets
            }
            let firstSiteProcess = try #require(await webView.frameTreesInProcesses().processIdentifier(hosting: "https://webkit.org"))
            #expect(firstSiteProcess != webView._webProcessIdentifier)

            try await webView.callJavaScript { "document.getElementById('frame').src = 'https://apple.com/third'" }
            var navigatedChildFrame: WKFrameInfo?
            try await waitForCondition("the iframe to commit a load from the third site") {
                navigatedChildFrame = await webView.firstChildFrame
                return navigatedChildFrame?.securityOrigin.host == "apple.com"
            }
            let thirdSiteChildFrame = try #require(navigatedChildFrame)
            try await waitForCondition("the iframe in the third site's process to use the safe area insets") {
                try await webView.envSafeAreaInsets(in: thirdSiteChildFrame) == expectedInsets
            }

            let thirdSiteProcess = try #require(await webView.frameTreesInProcesses().processIdentifier(hosting: "https://apple.com"))
            #expect(thirdSiteProcess != webView._webProcessIdentifier)
            #expect(thirdSiteProcess != firstSiteProcess)
        }
    }

    #if ENABLE_IPC_TESTING_API
    @Test(.timeLimit(.minutes(1)))
    func unchangedSafeAreaInsetsAreNotSentAgainToCrossSiteIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "\(safeAreaInsetsStyle)<body style='height: 4000px'><iframe src='https://webkit.org/iframe'></iframe></body>"
            }

            Route("/iframe") {
                "\(safeAreaInsetsStyle)<body>iframe</body>"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.setFeature("IPCTestingAPIEnabled", enabled: true)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration, frame: webViewFrame)
            defer { extendLifetime(navigationDelegate) }
            webView._unobscuredSafeAreaInsets = UIEdgeInsets(top: 10, left: 20, bottom: 30, right: 40)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            await webView.nextPresentationUpdate()

            let childFrame = try #require(await webView.firstChildFrame)
            try await waitForCondition("the cross-site iframe to use the safe area insets") {
                try await webView.envSafeAreaInsets(in: childFrame) == "10px 40px 30px 20px"
            }

            try await webView.callJavaScript(in: childFrame) {
                """
                window.safeAreaInsetUpdateCount = 0;
                IPC.addIncomingMessageListener('UI', message => {
                    if (message.name == IPC.messages.WebPage_SetUnobscuredSafeAreaInsets.name)
                        ++window.safeAreaInsetUpdateCount;
                });
                """
            }

            func safeAreaInsetUpdateCount() async throws -> Int {
                try await webView.callJavaScript(returning: Int.self, in: childFrame) { "return window.safeAreaInsetUpdateCount" }
            }

            // Scrolling changes the visible content rect, which the web view sends to the main frame's process, but the
            // iframe's process only needs the insets again if they change.
            for offset in 1...3 {
                webView.scrollView.setContentOffset(CGPoint(x: 0, y: offset * 100), animated: false)
                await webView.nextVisibleContentRectUpdate()
            }
            #expect(try await safeAreaInsetUpdateCount() == 0)

            webView._unobscuredSafeAreaInsets = UIEdgeInsets(top: 10, left: 20, bottom: 44, right: 40)
            await webView.nextVisibleContentRectUpdate()
            #expect(try await safeAreaInsetUpdateCount() == 1)
            #expect(try await webView.envSafeAreaInsets(in: childFrame) == "10px 40px 44px 20px")
        }
    }
    #endif // ENABLE_IPC_TESTING_API
}

extension WKWebView {
    // The safe area insets that a frame sees through env(), as its top, right, bottom and left lengths.
    fileprivate func envSafeAreaInsets(in frame: WKFrameInfo? = nil) async throws -> String {
        try await callJavaScript(returning: String.self, in: frame) {
            "return getComputedStyle(document.documentElement).getPropertyValue('--insets')"
        }
    }

    // Waits for the next visible content rect update, which is when the web view sends its unobscured safe area insets.
    fileprivate func nextVisibleContentRectUpdate() async {
        await withCheckedContinuation { continuation in
            _do(afterNextVisibleContentRectUpdate: {
                continuation.resume()
            })
        }
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY
