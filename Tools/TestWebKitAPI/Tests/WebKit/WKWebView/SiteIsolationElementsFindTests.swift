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

#if WTF_PLATFORM_MAC

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKInspector
private import WebKit_Private._WKInspectorPrivateForTesting

import struct Foundation.URL
import struct Swift.String

// Both cases select nodes in the Elements tab, which saves the selection in a setting that they share.
@MainActor
@Suite(.serialized, .timeLimit(.minutes(1)))
struct SiteIsolationElementsFindTests {
    @Test(arguments: [true, false])
    func elementsTabFindCountsHighlightsAndRevealsMatchesInCrossOriginIframe(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <div id='main' data-find='elements-find-needle'></div>\
                <div><div><iframe src='https://webkit.org/subframe'></iframe></div></div>
                """
            }

            Route("/subframe") {
                "<p id='frame' data-find='elements-find-needle'></p>"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.preferences._developerExtrasEnabled = true
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: siteIsolationEnabled
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let inspector = try #require(webView._inspector)
            inspector.show()
            defer { inspector.close() }
            let frontend = try #require(inspector.inspectorWebView())

            // The unminified frontend of a Debug build takes a few seconds to load. With site isolation, the iframe's
            // document comes from its own frame target.
            try await waitForCondition("the Web Inspector frontend to load", timeout: .seconds(15)) {
                try await frontend.callJavaScript(returning: Bool.self, arguments: ["siteIsolationEnabled": siteIsolationEnabled]) {
                    """
                    if (!window.InspectorFrontendAPI?._loaded)
                        return false;
                    if (!siteIsolationEnabled)
                        return true;
                    return WI.targets.some((target) => target instanceof WI.FrameTarget
                        && WI.domManager.frameTargetDocumentForTarget(target)?.documentURL?.includes('webkit.org'));
                    """
                }
            }

            try await frontend.callJavaScript { "WI.showElementsTab()" }
            try await waitForCondition("the Elements tab to show the DOM tree") {
                try await frontend.callJavaScript(returning: Bool.self) {
                    """
                    return !!WI.tabBrowser.selectedTabContentView?.contentBrowser?.currentContentView?.domTreeOutline?.rootDOMNode
                        && !WI._messagesToDispatch.length;
                    """
                }
            }

            // The DOM tree hasn't loaded the elements around the iframe, so with site isolation the iframe's document
            // isn't in the page's tree yet.
            let frameDocumentIsInPageTree = try await frontend.callJavaScript(returning: Bool.self) {
                """
                return WI.targets.some((target) => target instanceof WI.FrameTarget
                    && !!WI.domManager.frameTargetDocumentForTarget(target)?.parentNode);
                """
            }
            #expect(!frameDocumentIsInPageTree)

            // These go through the ContentBrowser methods that the find banner calls.
            let contentBrowserAndView = """
                let contentBrowser = WI.tabBrowser.selectedTabContentView.contentBrowser;
                let view = contentBrowser.currentContentView;
                """

            let findStarted = try await frontend.callJavaScript(returning: Bool.self) {
                """
                \(contentBrowserAndView)
                if (!(view instanceof WI.DOMTreeContentView))
                    return false;
                window.elementsTabFindCount = null;
                view.awaitEvent(WI.ContentView.Event.NumberOfSearchResultsDidChange, window).then(() => {
                    window.elementsTabFindCount = view.numberOfSearchResults;
                });
                contentBrowser.findBannerPerformSearch(null, ['elements', 'find', 'needle'].join('-'));
                return true;
                """
            }
            try #require(findStarted)
            try await waitForCondition("the find to report its number of results") {
                try await frontend.callJavaScript(returning: Bool.self) { "return window.elementsTabFindCount !== null" }
            }
            #expect(try await frontend.callJavaScript(returning: Int.self) { "return window.elementsTabFindCount" } == 2)

            let steps = try await frontend.callJavaScript(returning: [String].self) {
                """
                \(contentBrowserAndView)
                let findBanner = {showing: true};
                let describeSelection = () => {
                    let treeElement = view.domTreeOutline.selectedTreeElement;
                    let node = treeElement?.representedObject;
                    let description = node?.getAttribute?.('id') || 'no element';
                    if (node?.owningTarget)
                        description += ' in frame target';
                    if (treeElement?.title?.querySelector?.('.' + WI.DOMTreeElement.SearchHighlightStyleClassName))
                        description += ', highlighted';
                    return description;
                };
                let steps = [];
                for (let i = 0; i < view.numberOfSearchResults; ++i) {
                    contentBrowser.findBannerRevealNextResult(findBanner);
                    steps.push(describeSelection());
                }
                for (let i = 0; i < 2; ++i) {
                    contentBrowser.findBannerRevealPreviousResult(findBanner);
                    steps.push('previous: ' + describeSelection());
                }
                return steps;
                """
            }
            let frameMatch = siteIsolationEnabled ? "frame in frame target" : "frame"
            #expect(
                steps == [
                    "main, highlighted",
                    "\(frameMatch), highlighted",
                    "previous: main, highlighted",
                    "previous: \(frameMatch), highlighted",
                ]
            )

            let findCleared = try await frontend.callJavaScript(returning: Bool.self) {
                """
                \(contentBrowserAndView)
                contentBrowser.findBannerSearchCleared(null);
                return view.numberOfSearchResults === null
                    && !view.domTreeOutline.element.querySelector('.' + WI.DOMTreeElement.SearchHighlightStyleClassName)
                    && !WI.domManager._activeSearches.has(view);
                """
            }
            #expect(findCleared)

            // The Elements tab selects the node it selected last when it opens again, even in a later run.
            try await frontend.callJavaScript { "new WI.Setting('last-selected-node-path', null).reset()" }
        }
    }
}

#endif // WTF_PLATFORM_MAC
