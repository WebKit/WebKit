// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

import Foundation
private import Synchronization
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.PDFTestHelpers
private import TestWebKitAPILibrary.Helpers.cocoa.SafeBrowsingTestUtilities
private import TestWebKitAPILibrary.Helpers.cocoa.ScreenTimeExtras
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import TestWebKitAPILibrary.Helpers.cocoa.TextExtractionTestingSPI
import Testing
import WebKit
private import WebKit_Private
private import WebKit_Private.WKContentWorldPrivate
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#else
private import UIKit
#endif

// MARK: Supporting helpers

@MainActor
private func makeWebViewForTextExtractionTesting(
    width: CGFloat = 800,
    height: CGFloat = 600,
    textExtraction: Bool = true,
    backgroundTextExtraction: Bool = false,
    fraudulentWebsiteWarning: Bool = false,
    webFeatures: [String] = []
) -> TestWKWebView {
    let configuration = WKWebViewConfiguration()
    configuration.preferences._textExtractionEnabled = textExtraction
    if backgroundTextExtraction {
        configuration._backgroundTextExtractionEnabled = true
    }
    if fraudulentWebsiteWarning {
        configuration.preferences.isFraudulentWebsiteWarningEnabled = true
    }
    for feature in WKPreferences._features() where webFeatures.contains(feature.key) {
        configuration.preferences._setEnabled(true, for: feature)
    }
    return TestWKWebView(frame: CGRect(x: 0, y: 0, width: width, height: height), configuration: configuration)
}

@MainActor
private func worldForCreatingJSHandles() -> WKContentWorld {
    let configuration = _WKContentWorldConfiguration()
    configuration.allowJSHandleCreation = true
    return WKContentWorld.worldWithConfiguration(configuration: configuration)
}

private func extractionConfigurationWithFilteringDisabled() -> _WKTextExtractionConfiguration {
    let configuration = _WKTextExtractionConfiguration()
    configuration.filterOptions = []
    return configuration
}

private func extractNodeIdentifier(_ debugText: String, _ searchText: String) -> String? {
    let nodeIdentifier = /uid=((?:\d+_)*\d+)/

    return debugText.split(separator: "\n")
        .filter { $0.contains(searchText) }
        .compactMap { $0.firstMatch(of: nodeIdentifier) }
        .first
        .map { String($0.1) }
}

@MainActor
private func simulateHostApplicationEnteredBackground(_ webView: TestWKWebView) {
    #if WTF_PLATFORM_IOS_FAMILY
    NotificationCenter.default.post(
        name: UIApplication.didEnterBackgroundNotification,
        object: UIApplication.shared,
        userInfo: ["isSuspendedUnderLock": false]
    )
    NotificationCenter.default.post(
        name: UIScene.didEnterBackgroundNotification,
        object: webView.window?.windowScene,
        userInfo: nil
    )
    #else
    webView.hostWindow()?.orderOut(nil)
    #endif
}

@MainActor
private func waitForCondition(
    _ description: String,
    timeout: Duration = .seconds(5),
    _ condition: () async throws -> Bool
) async throws {
    let deadline = ContinuousClock.now + timeout

    while !(try await condition()) {
        guard ContinuousClock.now < deadline else {
            Issue.record("Timed out waiting for condition: \(description)")
            return
        }
        try await Task.sleep(for: .milliseconds(10))
    }
}

extension WKWebView {
    @MainActor
    fileprivate func debugText(_ configuration: _WKTextExtractionConfiguration? = nil) async throws -> String {
        try #require(await _extractDebugText(with: configuration ?? _WKTextExtractionConfiguration())).textContent
    }
}

@MainActor
private final class SubframeCollector {
    private(set) var frames: [WKFrameInfo] = []

    func install(on delegate: TestNavigationDelegate) {
        delegate.didCommitLoadWithRequestInFrame = { [weak self] _, _, frame in
            guard !frame.isMainFrame, frame.request.url?.scheme != "about" else {
                return
            }
            self?.frames.append(frame)
        }
    }
}

#if WTF_PLATFORM_IOS_FAMILY && !WTF_PLATFORM_MACCATALYST

// MARK: Content-mode helpers

@MainActor
private func matchesMediaQuery(_ webView: TestWKWebView, _ query: String) async throws -> Bool {
    try await webView.callJavaScript(returning: Bool.self) { "return matchMedia('\(query)').matches" }
}

@MainActor
private var isSmallScreenDevice: Bool {
    UIDevice.current.userInterfaceIdiom == .phone
}

@MainActor
private func load(_ webView: TestWKWebView, using contentMode: WKWebpagePreferences.ContentMode) async throws {
    let preferences = WKWebpagePreferences()
    preferences.preferredContentMode = contentMode

    let navigationDelegate = TestNavigationDelegate()
    navigationDelegate.decidePolicyForNavigationActionWithPreferences = { _, _, decisionHandler in
        decisionHandler(.allow, preferences)
    }
    webView.navigationDelegate = navigationDelegate

    webView.loadHTMLString("<body>Hello world</body>", baseURL: Bundle.testResources.resourceURL)
    try await navigationDelegate.waitForDidFinishNavigation()
}

@MainActor
private func expectDesktopClassHardwareEmulation(
    _ webView: TestWKWebView,
    sourceLocation: SourceLocation = #_sourceLocation
) async throws {
    #if ENABLE_IOS_TOUCH_EVENTS
    #expect(
        try await webView.callJavaScript(returning: Int.self) { "return navigator.maxTouchPoints" } == 0,
        sourceLocation: sourceLocation
    )
    #endif
    #if ENABLE_TOUCH_EVENTS
    #expect(
        try await webView.callJavaScript(returning: Bool.self) { "return 'ontouchstart' in window" } == false,
        sourceLocation: sourceLocation
    )
    #endif

    #expect(try await matchesMediaQuery(webView, "(pointer: fine)"), sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(any-pointer: fine)"), sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(hover: hover)"), sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(any-hover: hover)"), sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(pointer: coarse)") == false, sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(hover: none)") == false, sourceLocation: sourceLocation)

    guard isSmallScreenDevice else {
        return
    }

    let innerWidth = try await webView.callJavaScript(returning: Int.self) { "return innerWidth" }
    let innerHeight = try await webView.callJavaScript(returning: Int.self) { "return innerHeight" }
    #expect(try await webView.callJavaScript(returning: Int.self) { "return screen.width" } == innerWidth, sourceLocation: sourceLocation)
    #expect(try await webView.callJavaScript(returning: Int.self) { "return screen.height" } == innerHeight, sourceLocation: sourceLocation)
    #expect(
        try await webView.callJavaScript(returning: Int.self) { "return screen.availWidth" } == innerWidth,
        sourceLocation: sourceLocation
    )
    #expect(
        try await webView.callJavaScript(returning: Int.self) { "return screen.availHeight" } == innerHeight,
        sourceLocation: sourceLocation
    )
}

@MainActor
private func expectNoDesktopClassHardwareEmulation(
    _ webView: TestWKWebView,
    sourceLocation: SourceLocation = #_sourceLocation
) async throws {
    #if ENABLE_IOS_TOUCH_EVENTS
    #expect(
        try await webView.callJavaScript(returning: Int.self) { "return navigator.maxTouchPoints" } == 5,
        sourceLocation: sourceLocation
    )
    #endif
    #if ENABLE_TOUCH_EVENTS
    #expect(
        try await webView.callJavaScript(returning: Bool.self) { "return 'ontouchstart' in window" },
        sourceLocation: sourceLocation
    )
    #endif

    #expect(try await matchesMediaQuery(webView, "(pointer: coarse)"), sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(hover: none)"), sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(pointer: fine)") == false, sourceLocation: sourceLocation)
    #expect(try await matchesMediaQuery(webView, "(hover: hover)") == false, sourceLocation: sourceLocation)

    guard isSmallScreenDevice else {
        return
    }

    let innerWidth = try await webView.callJavaScript(returning: Int.self) { "return innerWidth" }
    #expect(try await webView.callJavaScript(returning: Int.self) { "return screen.width" } != innerWidth, sourceLocation: sourceLocation)
}

#endif // WTF_PLATFORM_IOS_FAMILY && !WTF_PLATFORM_MACCATALYST

#if ENABLE_UNIFIED_PDF

// MARK: PDF helpers

@MainActor
private func makeUnifiedPDFWebView() -> TestWKWebView {
    let configuration = TestPDFBuilder.configurationForUnifiedPDF(withHUDEnabled: false)
    configuration.preferences._textExtractionEnabled = true
    return TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
}

extension WKWebView {
    @MainActor
    fileprivate func load(pdf data: Data, baseURL: URL) async throws {
        load(data, mimeType: "application/pdf", characterEncodingName: "", baseURL: baseURL)
        try await _test_waitForDidFinishNavigation()
    }
}

private func decodeJSONObject(_ text: String) throws -> [String: Any] {
    let data = try #require(text.data(using: .utf8))
    return try #require(try JSONSerialization.jsonObject(with: data) as? [String: Any])
}

#endif // ENABLE_UNIFIED_PDF

#if ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

private let mainFrameMarkup = """
    <!DOCTYPE html>
    <html>
        <head>
            <meta name='viewport' content='width=device-width, initial-scale=1'>
            <style>
                iframe {
                    width: 300px;
                    height: 300px;
                }
            </style>
        </head>
        <body>
            <iframe class='same'></iframe>
            <iframe class='cross'></iframe>
            <a href='https://webkit.org'>Link to WebKit home page</a>
            <script>
                subframeLoadedCount = 0;
                sameOriginFrame = document.querySelector('iframe.same');
                sameOriginFrame.addEventListener('load', () => subframeLoadedCount++, { once: true });
                sameOriginFrame.src = 'subframe-same.html';

                crossOriginFrame = document.querySelector('iframe.cross');
                crossOriginFrame.addEventListener('load', () => subframeLoadedCount++, { once: true });
                crossOriginFrame.src = `http://localhost:${location.port}/subframe-cross.html`;
            </script>
        </body>
    </html>
    """

private func subFrameMarkup(buttonText: String) -> String {
    """
    <!DOCTYPE html>
    <html>
        <body>
            <h1>Click count: <span id='click-count'>0</span></h1>
            <article aria-label='Button container'>
                <button>\(buttonText)</button>
            </article>
            <script>
                const clickCount = document.getElementById('click-count');
                const button = document.querySelector('button');
                button.addEventListener('click', () => {
                    clickCount.textContent = 1 + parseInt(clickCount.textContent);
                });
            </script>
        </body>
    </html>
    """
}

@MainActor
private func makeSubframeServer(crossOriginButtonText: String, sameOriginButtonText: String) -> HTTPServer {
    HTTPServer(protocol: .http) {
        Route("/") {
            mainFrameMarkup
        }
        Route("/subframe-cross.html") {
            subFrameMarkup(buttonText: crossOriginButtonText)
        }
        Route("/subframe-same.html") {
            subFrameMarkup(buttonText: sameOriginButtonText)
        }
    }
}

@MainActor
private func loadSubframePage(
    in webView: TestWKWebView,
    port: Int
) async throws -> [WKFrameInfo] {
    let subframes = SubframeCollector()
    let navigationDelegate = TestNavigationDelegate()
    subframes.install(on: navigationDelegate)
    webView.navigationDelegate = navigationDelegate

    let url = try #require(URL(string: "http://127.0.0.1:\(port)/"))
    webView.load(URLRequest(url: url))
    try await navigationDelegate.waitForDidFinishNavigation()

    try await waitForCondition("subframes to finish loading", timeout: .seconds(2)) {
        try await webView.callJavaScript(returning: Int.self) { "return subframeLoadedCount" } == 2
    }

    return subframes.frames
}

#endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

#if HAVE_SAFARI_SAFE_BROWSING_NAMESPACED_LISTS

// MARK: Safe Browsing filter rules

private let substitutionAndTruncationLists: SafeBrowsingLists = [
    "test1/domains": [".*"],
    "test1/filter": ["return input.length >= 1000 ? '<too long>' : null"],
    "test2/domains": [".*"],
    "test2/filter": ["return input.replaceAll('o', '•').replaceAll('u', 'v')"],
]

private let listsToCheckNetworkAndDOMIsolation: SafeBrowsingLists = [
    "isolation/domains": [".*"],
    "isolation/filter": [
        """
        let dom = '<dom:?>';
        try {
            const text = (typeof document !== 'undefined' && document && document.body) ? document.body.innerText.trim() : '';
            dom = text ? `<dom:leaked:${text}>` : '<dom:empty>';
        } catch (e) {
            dom = `<dom:threw:${e.name}>`;
        }
        let net = '<net:?>';
        try {
            const response = await fetch('http://127.0.0.1:1/should-never-load');
            net = `<net:loaded:${response.status}>`;
        } catch (e) {
            net = '<net:blocked>';
        }
        return `${dom}|${net}`;
        """
    ],
]

private func plainHTMLConfiguration() -> _WKTextExtractionConfiguration {
    let configuration = _WKTextExtractionConfiguration()
    configuration.outputFormat = .HTML
    configuration.nodeIdentifierInclusion = .none
    configuration.includeRects = false
    return configuration
}

#endif // HAVE_SAFARI_SAFE_BROWSING_NAMESPACED_LISTS

// MARK: Tests

@MainActor
struct TextExtractionTests {
    private let webView: TestWKWebView

    init() {
        webView = makeWebViewForTextExtractionTesting()
    }

    #if WTF_PLATFORM_MAC
    @Test
    func selectPopupMenu() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let debugText = try await webView.debugText()
        let click = _WKTextExtractionInteraction(action: .click)
        click.nodeIdentifier = extractNodeIdentifier(debugText, "menu")

        let clickResult = try #require(await webView._performInteraction(click))
        #expect(clickResult.error == nil)
        #expect(clickResult.summary != nil)
        #expect(try await webView.debugText().contains("nativePopupMenu"))

        let selectOption = _WKTextExtractionInteraction(action: .selectMenuItem)
        selectOption.text = "Three"
        let selectOptionResult = try #require(await webView._performInteraction(selectOption))
        #expect(selectOptionResult.error == nil)
        #expect(selectOptionResult.summary == "Successfully updated option in select element")

        await webView.nextPresentationUpdate()
        #expect(try await webView.callJavaScript(returning: String.self) { "return select.value" } == "Three")
    }
    #endif // WTF_PLATFORM_MAC

    @Test
    func interactionDebugDescription() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let debugText = try await webView.debugText()
        let testButtonID = extractNodeIdentifier(debugText, "Test")
        let emailID = extractNodeIdentifier(debugText, "email")
        let composeID = extractNodeIdentifier(debugText, "Compose")
        let selectID = extractNodeIdentifier(debugText, "select")

        #if ENABLE_TEXT_EXTRACTION_FILTER
        #expect(!debugText.contains("crazy ones"))
        #endif

        do {
            let interaction = _WKTextExtractionInteraction(action: .click)

            interaction.nodeIdentifier = testButtonID
            #expect(
                try await interaction.debugDescription(in: webView)
                    == "Click on button labeled “Click Me” with id “test-button”, with rendered text “Test”"
            )

            interaction.nodeIdentifier = emailID
            #expect(
                try await interaction.debugDescription(in: webView)
                    == "Click on input of type email with placeholder “Recipient address”"
            )

            interaction.nodeIdentifier = composeID
            #expect(
                try await interaction.debugDescription(in: webView)
                    == """
                    Click on editable div labeled “Compose a new message” with class “message-body”, \
                    with rendered text “Subject 'The quick brown fox jumped over the lazy dog'”, \
                    containing child labeled “Heading”
                    """
            )
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .textInput)

            interaction.nodeIdentifier = emailID
            interaction.text = "squirrelfish@webkit.org"
            interaction.replaceAll = true
            #expect(
                try await interaction.debugDescription(in: webView)
                    == """
                    Enter text “squirrelfish@webkit.org” into input of type email with placeholder \
                    “Recipient address”, replacing any existing content
                    """
            )

            interaction.nodeIdentifier = composeID
            interaction.text = "«Testing»"
            interaction.replaceAll = false
            #expect(
                try await interaction.debugDescription(in: webView)
                    == """
                    Enter text “'Testing'” into editable div labeled “Compose a new message” with \
                    class “message-body”, with rendered text “Subject 'The quick brown fox jumped \
                    over the lazy dog'”, containing child labeled “Heading”
                    """
            )
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .selectMenuItem)
            interaction.nodeIdentifier = selectID
            interaction.text = "Three"
            let expected = "Select menu item “Three” in select with role “menu”"
            #expect(try await interaction.debugDescription(in: webView) == expected)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            let clickLocation = try await webView.elementMidpoint(selector: "#test-button")
            interaction.location = clickLocation
            let x = Int(clickLocation.x.rounded(.toNearestOrEven))
            let y = Int(clickLocation.y.rounded(.toNearestOrEven))
            let expected =
                "Click at coordinates (\(x), \(y)) on child node of button labeled “Click Me” with id “test-button”, with rendered text “Test”"
            #expect(try await interaction.debugDescription(in: webView) == expected)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.text = "Subject"
            let expected = "Click on “Subject” in child node of editable h3 labeled “Heading”, with rendered text “Subject”"
            #expect(try await interaction.debugDescription(in: webView) == expected)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)

            interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Open menu")
            var expected = "Click on img labeled “Open menu” under link with href “/menu” with id “menu-link”"
            #expect(try await interaction.debugDescription(in: webView) == expected)

            interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Checkmark icon")
            expected = "Click on img labeled “Checkmark icon” under button labeled “Submit form” with id “submit-with-icon”"
            #expect(try await interaction.debugDescription(in: webView) == expected)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Open menu")

            try await webView.callJavaScript {
                "document.getElementById('menu-link').setAttribute('href', '/search?q=webkit&lang=en')"
            }
            var expected = "Click on img labeled “Open menu” under link with href “/search?…” with id “menu-link”"
            #expect(try await interaction.debugDescription(in: webView) == expected)

            let longPathComponent = "".padding(toLength: 99, withPad: "a", startingAt: 0)
            try await webView.callJavaScript {
                "document.getElementById('menu-link').setAttribute('href', '/\(longPathComponent)')"
            }
            let expectedHref = "/\(longPathComponent.prefix(78))…"
            expected = "Click on img labeled “Open menu” under link with href “\(expectedHref)” with id “menu-link”"
            #expect(try await interaction.debugDescription(in: webView) == expected)
        }
    }

    @Test
    func interactionDescriptionUsesAdjacentTextForUnlabeledIcon() async throws {
        try await webView.load(
            html: """
                <div><span>Full Name</span><svg width='16' height='16' class='pencil1' onclick=''></svg></div>\
                <div><span>Password</span><span>********</span><svg width='16' height='16' class='pencil2' onclick=''></svg></div>
                """
        )

        let debugText = try await webView.debugText()
        let nameIconID = try #require(extractNodeIdentifier(debugText, "pencil1"))
        let passwordIconID = try #require(extractNodeIdentifier(debugText, "pencil2"))

        let interaction = _WKTextExtractionInteraction(action: .click)

        interaction.nodeIdentifier = nameIconID
        var expected = "Click on svg with class “pencil1” after rendered text “Full Name”"
        #expect(try await interaction.debugDescription(in: webView) == expected)

        interaction.nodeIdentifier = passwordIconID
        expected = "Click on svg with class “pencil2” after rendered text “Password ********”"
        #expect(try await interaction.debugDescription(in: webView) == expected)
    }

    @Test
    func interactionDescriptionAndSearchTextForLabellessIcons() async throws {
        try await webView.load(
            html: """
                <style>i, button { display: inline-block; width: 16px; height: 16px }</style>\
                <div class='group-one'><div class='head-one'><i class='chevron-one' onclick=''></i>\
                <i class='lock-icon'></i><span>Notifications</span></div>\
                <div class='sub-one' style='max-height: 0; overflow: hidden'><div>Email notifications</div></div></div>\
                <div class='group-two'><div class='head-two'><i class='chevron-two' onclick=''></i>\
                <i class='lock-icon'></i><span>Security</span></div>\
                <div class='sub-two' style='max-height: 0; overflow: hidden'><div>Change password</div></div></div>\
                <div class='row-sort'><span>Sort</span><i class='sort-caret' onclick=''></i><span>ascending</span></div>\
                <div class='row-space'><i class='icon-space' onclick=''> </i><span>Space Case</span></div>\
                <div class='row-bravo'><span>Bravo Label</span><button onclick=''></button></div>
                """
        )

        let debugText = try await webView.debugText()

        func makeClickInteraction(_ className: String, _ searchText: String?) throws -> _WKTextExtractionInteraction {
            let identifier = try #require(extractNodeIdentifier(debugText, className))

            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.nodeIdentifier = identifier
            interaction.text = searchText

            return interaction
        }

        func clickDescription(_ className: String, _ searchText: String? = nil) async throws -> String {
            try await makeClickInteraction(className, searchText).debugDescription(in: webView)
        }

        func cannotDescribeClick(_ className: String, _ searchText: String?) async throws -> Bool {
            do {
                _ = try await makeClickInteraction(className, searchText).debugDescription(in: webView)
                return false
            } catch {
                return true
            }
        }

        #expect(try await clickDescription("chevron-one") == "Click on i with class “chevron-one” before rendered text “Notifications”")
        #expect(try await clickDescription("chevron-two") == "Click on i with class “chevron-two” before rendered text “Security”")
        #expect(
            try await clickDescription("sort-caret") == "Click on i with class “sort-caret” between rendered text “Sort” and “ascending”"
        )
        #expect(try await clickDescription("icon-space") == "Click on i with class “icon-space” before rendered text “Space Case”")
        #expect(
            try await clickDescription("button") == "Click on button after rendered text “Bravo Label” under div with class “row-bravo”"
        )

        #expect(
            try await clickDescription("chevron-two", "Security")
                == "Click on “Security” in child node of span under div with class “head-two”, with rendered text “Security”"
        )
        #expect(try await cannotDescribeClick("chevron-two", "Notifications"))
        #expect(try await cannotDescribeClick("chevron-two", "Nonexistent"))
    }

    @Test
    func interactionDescriptionIncludesAssociatedLabelText() async throws {
        try await webView.load(
            html: """
                <label for='email-field'>Email address</label><input id='email-field'>\
                <label>Phone number <input id='p1'></label>\
                <label for='city-name'>City</label><input id='city-name' aria-label='Town'>\
                <label for='notes-field'>Notes</label><textarea id='notes-field'></textarea>\
                <label for='save-button'>Save changes</label><button id='save-button'><img aria-label='Icon'></button>
                """
        )

        let debugText = try await webView.debugText()
        let interaction = _WKTextExtractionInteraction(action: .click)

        interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Email address")
        var expected = "Click on input labeled “Email address” with id “email-field”"
        #expect(try await interaction.debugDescription(in: webView) == expected)

        interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Phone number")
        expected = "Click on input labeled “Phone number”"
        #expect(try await interaction.debugDescription(in: webView) == expected)

        interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Town")
        expected = "Click on input labeled “Town” with id “city-name”"
        #expect(try await interaction.debugDescription(in: webView) == expected)

        interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Notes")
        expected = "Click on textarea labeled “Notes” with id “notes-field”"
        #expect(try await interaction.debugDescription(in: webView) == expected)

        interaction.nodeIdentifier = extractNodeIdentifier(debugText, "Icon")
        expected = "Click on img labeled “Icon” under button labeled “Save changes” with id “save-button”"
        #expect(try await interaction.debugDescription(in: webView) == expected)
    }

    @Test
    func interactionClicksThroughOccludingOverlay() async throws {
        try await webView.load(
            html: """
                <a aria-label='Log in' href='#' style='position:absolute; top:10px; left:10px; width:80px; height:60px;'>\
                <span id='login-button' role='button' style='display:block; width:100%; height:100%;'>\
                <svg width='36' height='36' viewBox='0 0 36 36'><circle cx='18' cy='18' r='18' fill='black'></circle></svg>\
                </span></a>\
                <div style='position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.1);'></div>\
                <script>window.loginClicked = false;\
                document.getElementById('login-button').addEventListener('click', () => { window.loginClicked = true; });</script>
                """
        )

        let debugText = try await webView.debugText()
        let loginLinkID = try #require(extractNodeIdentifier(debugText, "Log in"))

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = loginLinkID
        let result = try #require(await webView._performInteraction(interaction))
        #expect(result.error == nil)

        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.loginClicked" })
    }

    @Test
    func interactionWithSearchTextSpanningBlockBoundary() async throws {
        try await webView.load(
            html: """
                <a aria-label='Account nav entry' href='#settings'>\
                <span style='display:block'>05</span><span style='display:block'>SETTINGS</span></a>\
                <script>window.settingsClicked = false;\
                document.querySelector('a').addEventListener('click', () => { window.settingsClicked = true; });</script>
                """
        )

        let debugText = try await webView.debugText()
        let settingsLinkID = try #require(extractNodeIdentifier(debugText, "Account nav entry"))

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = settingsLinkID
        interaction.text = "05 SETTINGS"

        let expectedDescription = "Click on link with href “#settings” labeled “Account nav entry”, with rendered text “05 SETTINGS”"
        #expect(try await interaction.debugDescription(in: webView) == expectedDescription)

        var result = try #require(await webView._performInteraction(interaction))
        #expect(result.error == nil)
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.settingsClicked" })

        try await webView.callJavaScript { "window.settingsClicked = false" }
        interaction.text = "05SETTINGS"

        #expect(try await interaction.debugDescription(in: webView) == expectedDescription)

        result = try #require(await webView._performInteraction(interaction))
        #expect(result.error == nil)
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.settingsClicked" })

        try await webView.callJavaScript { "window.settingsClicked = false" }
        interaction.text = "05\u{00a0}SETTINGS"

        result = try #require(await webView._performInteraction(interaction))
        #expect(result.error == nil)
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.settingsClicked" })
    }

    @Test
    func interactionDebugDescriptionWithStaleNodeIdentifier() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = "999999999_999999999"
        interaction.text = "Test"

        await #expect(throws: Never.self) {
            try await interaction.debugDescription(in: webView)
        }
    }

    @Test
    func interactionDebugDescriptionWithUnresolvableNodeIdentifier() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = "999999999"

        await #expect(throws: (any Error).self) {
            try await interaction.debugDescription(in: webView)
        }
    }

    @Test
    func interactionDebugDescriptionWithoutTargetElement() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let interaction = _WKTextExtractionInteraction(action: .scroll)
        #expect(try await interaction.debugDescription(in: webView) == "Scroll to next page")
    }

    @Test
    func interactionResultSummary() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let debugText = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
        let testButtonID = extractNodeIdentifier(debugText, "Test")
        let emailID = extractNodeIdentifier(debugText, "email")
        let selectID = extractNodeIdentifier(debugText, "select")

        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.nodeIdentifier = testButtonID
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            #expect(result.summary == "Clicked on button labeled “Click Me” with id “test-button”, with rendered text “Test”")
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .textInput)
            interaction.nodeIdentifier = emailID
            interaction.text = "squirrelfish@webkit.org"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            #expect(result.summary == "Inserted text into input of type email with placeholder “Recipient address”")
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .selectMenuItem)
            interaction.nodeIdentifier = selectID
            interaction.text = "Three"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            #expect(result.summary == "Successfully updated option in select element")
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.text = "Subject"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            #expect(
                result.summary
                    == "Clicked on “Subject” in child node of editable h3 labeled “Heading”, with rendered text “Subject”"
            )
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.text = "this text does not exist anywhere on the page"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error != nil)
            #expect(result.summary == nil)
        }
    }

    @Test
    func interactionSearchTextMatchesAccessibilityLabel() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let debugText = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
        let testButtonID = extractNodeIdentifier(debugText, "Test")

        func clickCount() async throws -> Int {
            try await webView.callJavaScript(returning: Int.self) { "return parseInt(document.querySelector('.click-count').textContent)" }
        }

        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.nodeIdentifier = testButtonID
            interaction.text = "Click Me"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            #expect(try await clickCount() == 1)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.nodeIdentifier = testButtonID
            interaction.text = "lick m"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            #expect(try await clickCount() == 2)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.nodeIdentifier = testButtonID
            interaction.text = "not the label"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error != nil)
            #expect(try await clickCount() == 2)
        }
    }

    @Test
    func interactionRemapsStaleNodeIdentifier() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")
        let staleIdentifier = try #require(
            extractNodeIdentifier(try await webView.debugText(extractionConfigurationWithFilteringDisabled()), "Test")
        )

        try await webView.load(testPageNamed: "debug-text-extraction")
        let currentIdentifier = try #require(
            extractNodeIdentifier(try await webView.debugText(extractionConfigurationWithFilteringDisabled()), "Test")
        )
        #expect(staleIdentifier != currentIdentifier)

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = staleIdentifier

        let description = try await interaction.debugDescription(in: webView)
        #expect(description.contains("Click Me"))

        let result = try #require(await webView._performInteraction(interaction))
        #expect(result.error == nil)
        let summary = try #require(result.summary)
        #expect(summary.contains("Click Me"))
        #expect(summary.contains("stale"))
        #expect(summary.contains("re-resolved"))

        let clickCount = try await webView.callJavaScript(returning: Int.self) {
            "return parseInt(document.querySelector('.click-count').textContent)"
        }
        #expect(clickCount == 1)
    }

    @Test
    func interactionRemapsStaleNodeIdentifierWithURL() async throws {
        func makeDebugTextConfiguration() -> _WKTextExtractionConfiguration {
            let configuration = extractionConfigurationWithFilteringDisabled()
            configuration.includeURLs = true
            configuration.shortenURLs = true
            configuration.includeRects = false
            return configuration
        }

        let accountPageMarkup = """
            <div role='tablist'>\
            <a href='https://example.com/account' role='tab' aria-selected='true' aria-label='Account tab'>Account</a>\
            <a href='https://example.com/security' role='tab' aria-selected='false' aria-label='Security tab'>Security</a>\
            </div>
            """

        let securityPageMarkup = """
            <div role='tablist'>\
            <a href='https://example.com/account' role='tab' aria-selected='false' aria-label='Account tab'>Account</a>\
            <a href='https://example.com/security' role='tab' aria-selected='true' aria-label='Security tab'>Security</a>\
            </div>\
            <script>\
            window.accountTabClicked = false;\
            document.querySelector("a[href='https://example.com/account']").addEventListener('click', event => {\
                event.preventDefault();\
                window.accountTabClicked = true;\
            });\
            </script>
            """

        try await webView.load(html: accountPageMarkup, baseURL: URL(string: "https://example.com/account"))
        let staleAccountTabIdentifier = try #require(
            extractNodeIdentifier(try await webView.debugText(makeDebugTextConfiguration()), "Account tab")
        )

        try await webView.load(html: securityPageMarkup, baseURL: URL(string: "https://example.com/security"))
        let currentAccountTabIdentifier = try #require(
            extractNodeIdentifier(try await webView.debugText(makeDebugTextConfiguration()), "Account tab")
        )
        #expect(staleAccountTabIdentifier != currentAccountTabIdentifier)

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = staleAccountTabIdentifier

        let result = try #require(await webView._performInteraction(interaction))
        #expect(result.error == nil)
        let summary = try #require(result.summary)
        #expect(summary.contains("stale"))
        #expect(summary.contains("re-resolved"))
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.accountTabClicked" })
    }

    @Test
    func interactionReportsStaleNodeWhenRemapCandidateIsAlsoStale() async throws {
        let verificationURL = URL(string: "https://example.com/verify")
        let chooseMethodMarkup = """
            <div>\
            <p>Keeping your account safe</p>\
            <p>Choose one to continue</p>\
            <label><input type='radio' name='method' aria-label='Email verification'>Email</label>\
            <label><input type='radio' name='method' aria-label='Text verification'>Text</label>\
            <button id='continue'>Continue</button>\
            </div>
            """

        let enterCodeMarkup = """
            <div>\
            <p>Enter verification code</p>\
            <p>It may take a few minutes to arrive</p>\
            <input type='text' aria-label='Verification code'>\
            <button id='verify'>Verify</button>\
            </div>
            """

        try await webView.load(html: chooseMethodMarkup, baseURL: verificationURL)
        let staleIdentifier = try #require(
            extractNodeIdentifier(try await webView.debugText(extractionConfigurationWithFilteringDisabled()), "Email verification")
        )

        try await webView.load(html: chooseMethodMarkup, baseURL: verificationURL)
        let supersededIdentifier = try #require(
            extractNodeIdentifier(try await webView.debugText(extractionConfigurationWithFilteringDisabled()), "Email verification")
        )
        #expect(staleIdentifier != supersededIdentifier)

        try await webView.load(html: enterCodeMarkup, baseURL: verificationURL)
        let latestDebugText = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
        #expect(!latestDebugText.contains("Email verification"))

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = staleIdentifier

        let result = try #require(await webView._performInteraction(interaction))
        #expect(result.summary == nil)
        let errorDescription = try #require(
            (result.error as NSError?)?.userInfo[NSDebugDescriptionErrorKey] as? String
        )
        #expect(errorDescription.contains("re-extract the page"))
        #expect(!errorDescription.contains("re-resolved"))
    }

    @Test
    func targetNodeAndClientAttributes() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")
        try await webView.callJavaScript {
            "getSelection().selectAllChildren(document.querySelector('h3[aria-label=\"Heading\"]'))"
        }

        let world = worldForCreatingJSHandles()
        let editorHandle = try #require(await webView.querySelector("div[contenteditable]", in: world))
        let headingHandle = try #require(await webView.querySelector("h3[aria-label='Heading']", in: world))

        let configuration = _WKTextExtractionConfiguration()
        configuration.targetNode = editorHandle
        configuration.addClientAttribute("extra-data-1", value: "abc", forNode: editorHandle)
        configuration.addClientAttribute("extra-data-1", value: "123", forNode: headingHandle)
        configuration.addClientAttribute("extra-data-2", value: "xyz", forNode: headingHandle)

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("Compose a new message"))
        #expect(debugText.contains("label='Compose a new message' extra-data-1=abc"))
        #expect(debugText.contains("label=Heading extra-data-1=123 extra-data-2=xyz"))
        #expect(debugText.contains("Subject"))
        #expect(debugText.contains("The quick brown fox jumped over the lazy dog"))
        #expect(!debugText.contains("select,"))
        #expect(!debugText.contains("Click Me"))
        #expect(!debugText.contains("Recipient address"))
    }

    @Test
    func targetNodeWithSameOriginSubframe() async throws {
        try await webView.load(html: "<div id='target'><p>main content</p><iframe srcdoc='<p>subframe content</p>'></iframe></div>")

        let world = worldForCreatingJSHandles()
        let targetHandle = try #require(await webView.querySelector("#target", in: world))

        let configuration = _WKTextExtractionConfiguration()
        configuration.targetNode = targetHandle

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("main content"))
        #expect(debugText.contains("subframe content"))
    }

    @Test
    func includeTagNamePrefixesContentBlocks() async throws {
        try await webView.load(html: "<h1>Alpha</h1><p>Bravo</p><div><h2>Charlie</h2></div>")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .textTree
        configuration.includeTagName = true

        let debugText = try await webView.debugText(configuration)

        #expect(debugText.contains("h1 'Alpha'"))
        #expect(debugText.contains("p 'Bravo'"))
        #expect(debugText.contains("h2 'Charlie'"))

        #expect(!debugText.contains("div"))
    }

    @Test
    func includeTagNameTagsBlockWithOnlyInlineContent() async throws {
        try await webView.load(html: "<div>Delta</div>")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .textTree
        configuration.includeTagName = true

        #expect(try await webView.debugText(configuration).contains("div 'Delta'"))
    }

    @Test
    func includeTagNameDisabledByDefault() async throws {
        try await webView.load(html: "<h1>Alpha</h1><p>Bravo</p>")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .textTree

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("'Alpha'"))
        #expect(debugText.contains("'Bravo'"))
        #expect(!debugText.contains("h1 '"))
        #expect(!debugText.contains("p '"))
    }

    @Test
    func includeTagNameCollapsesMixedInlineAndBlockContent() async throws {
        try await webView.load(html: "<div>lead text<h2>Charlie</h2></div>")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .textTree
        configuration.includeTagName = true

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("h2 'Charlie'"))
        #expect(debugText.contains("'lead text'"))
        #expect(!debugText.contains("div"))
    }

    @Test
    func includeTagNameDropsEmptyAndWhitespaceBlocks() async throws {
        try await webView.load(html: "<p>Alpha</p><div>   </div>")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .textTree
        configuration.includeTagName = true

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("p 'Alpha'"))
        #expect(!debugText.contains("div"))
    }

    @Test
    func includeTagNameKeepsSemanticLabelForStructuralContainers() async throws {
        try await webView.load(html: "<blockquote>Quote text.</blockquote>")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .textTree
        configuration.includeTagName = true

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("block-quote 'Quote text.'"))
        #expect(!debugText.contains("blockquote"))
    }

    @Test
    func includeTagNameFlattensInlineElementsIntoBlockText() async throws {
        try await webView.load(html: "<p>First <b>bold</b> and <span>emphasized</span> text.</p>")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .textTree
        configuration.includeTagName = true

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("p 'First bold and emphasized text.'"))
        #expect(!debugText.contains("b '"))
        #expect(!debugText.contains("span"))
    }

    @Test
    func extractFromDocumentWithoutBody() async throws {
        let url = try #require(URL(string: "data:application/xml,<root><item>hello%20world</item></root>"))
        try await webView.loadAndWait(URLRequest(url: url))

        _ = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
    }

    @Test
    func replacementStrings() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let debugTextWithoutReplacements = try await webView.debugText()
        #expect(debugTextWithoutReplacements.contains("The quick brown fox jumped over the lazy dog"))

        let configuration = _WKTextExtractionConfiguration()
        configuration.replacementStrings = [
            "fox": "cat",
            "dog": "mouse",
            "lazy": "",
        ]

        let debugTextWithReplacements = try await webView.debugText(configuration)
        #expect(!debugTextWithReplacements.contains("fox"))
        #expect(!debugTextWithReplacements.contains("dog"))
        #expect(!debugTextWithReplacements.contains("lazy"))
        #expect(debugTextWithReplacements.contains("The quick brown cat jumped over the  mouse"))
    }

    @Test
    func replacementStringsLongestMatchWins() async throws {
        try await webView.load(html: "<p>John Appleseed met John.</p>")

        let configuration = _WKTextExtractionConfiguration()
        configuration.replacementStrings = [
            "John": "<redacted-name>",
            "John Appleseed": "<redacted-full-name>",
        ]

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("<redacted-full-name> met <redacted-name>."))
        #expect(!debugText.contains("<redacted-name> Appleseed"))
    }

    @Test
    func replacementStringsCaseAndQuoteInsensitive() async throws {
        try await webView.load(html: "<p>Hello WORLD. It’s a test.</p>")

        let configuration = _WKTextExtractionConfiguration()
        configuration.replacementStrings = [
            "hello world": "<greeting>",
            "it's": "<contraction>",
        ]

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("<greeting>"))
        #expect(debugText.contains("<contraction>"))
        #expect(!debugText.contains("Hello WORLD"))
        #expect(!debugText.contains("It’s"))
    }

    @Test
    func replacementStringsDiacriticInsensitive() async throws {
        try await webView.load(html: "<p>Visited café in Zürich.</p>")

        let configuration = _WKTextExtractionConfiguration()
        configuration.replacementStrings = ["cafe": "<spot>"]

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("Visited <spot> in Zürich."))
        #expect(!debugText.contains("café"))
        #expect(!debugText.contains("Zurich"))
    }

    @Test
    func replacementStringsWordBoundaries() async throws {
        func textAfterReplacing(_ markup: String, _ replacementStrings: [String: String]) async throws -> String {
            try await webView.load(html: markup)
            let configuration = _WKTextExtractionConfiguration()
            configuration.replacementStrings = replacementStrings
            return try await webView.debugText(configuration)
        }

        do {
            let text = try await textAfterReplacing(
                "<p>Two-factor authentication</p><p>Location customization</p><p>Cat pictures</p>",
                ["Cat": "Jane"]
            )
            #expect(text.contains("Two-factor authentication"))
            #expect(text.contains("Location customization"))
            #expect(text.contains("Jane pictures"))
            #expect(!text.contains("authentiJaneion"))
            #expect(!text.contains("LoJaneion"))
        }
        do {
            let text = try await textAfterReplacing(
                "<p>At least 8 characters</p><button>Cancel</button><p>L is an initial</p>",
                ["L": "Marie"]
            )
            #expect(text.contains("At least 8 characters"))
            #expect(text.contains("Cancel"))
            #expect(text.contains("Marie is an initial"))
            #expect(!text.contains("Marieeast"))
            #expect(!text.contains("CanceMarie"))
        }
        do {
            let text = try await textAfterReplacing(
                "<p>wenson@me.com</p><p>wenson.hsieh@me.com</p><p>wenson-hsieh</p><p>wensonhsieh</p>",
                ["Wenson": "jane"]
            )
            #expect(text.contains("jane@me.com"))
            #expect(text.contains("jane.hsieh@me.com"))
            #expect(text.contains("jane-hsieh"))
            #expect(text.contains("wensonhsieh"))
            #expect(!text.contains("janehsieh"))
        }
        do {
            let text = try await textAfterReplacing(
                "<p>王謝李明</p><p>서울특별시</p>",
                [
                    "謝李": "<redacted-name>",
                    "울특": "<redacted-place>",
                ]
            )
            #expect(text.contains("王<redacted-name>明"))
            #expect(text.contains("서<redacted-place>별시"))
        }
    }

    @Test
    func replacementStringsAppliedToInteractionDescription() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let debugText = try await webView.debugText()
        let composeID = extractNodeIdentifier(debugText, "Compose")

        let replacementConfiguration = _WKTextExtractionConfiguration()
        replacementConfiguration.replacementStrings = [
            "FOX": "cat",
            "compose a new message": "[redacted subject]",
        ]
        _ = try await webView.debugText(replacementConfiguration)

        let interaction = _WKTextExtractionInteraction(action: .click)
        interaction.nodeIdentifier = composeID
        let description = try await interaction.debugDescription(in: webView)

        #expect(description.contains("[redacted subject]"))
        #expect(description.contains("brown cat jumped over the lazy dog"))
        #expect(!description.contains("Compose a new message"))
        #expect(!description.contains("fox"))

        let result = try #require(await webView._performInteraction(interaction))
        #expect(result.error == nil)

        let summary = try #require(result.summary)
        #expect(summary.contains("[redacted subject]"))
        #expect(summary.contains("brown cat jumped over the lazy dog"))
        #expect(!summary.contains("Compose a new message"))
        #expect(!summary.contains("fox"))

        try await webView.load(
            html: """
                <body style='margin:0; overflow:hidden; height:600px'>\
                <div aria-label='Secret Project Alpha' style='width:800px; height:600px; overflow-y:scroll'>\
                <div style='height:5000px'>lots of content</div></div></body>
                """
        )

        let containerConfiguration = _WKTextExtractionConfiguration()
        containerConfiguration.replacementStrings = ["Secret Project Alpha": "[redacted container]"]
        _ = try await webView.debugText(containerConfiguration)

        let scroll = _WKTextExtractionInteraction(action: .scroll)
        let scrollResult = try #require(await webView._performInteraction(scroll))
        #expect(scrollResult.error == nil)

        let scrollSummary = try #require(scrollResult.summary)
        #expect(scrollSummary.contains("[redacted container]"))
        #expect(!scrollSummary.contains("Secret Project Alpha"))
    }

    @Test
    func visibleTextOnly() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let configuration = _WKTextExtractionConfiguration()
        configuration.configureForMinimalOutput()

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("Test"))
        #expect(debugText.contains("foo"))
        #expect(debugText.contains("Subject"))
        #expect(debugText.contains("“The quick brown fox jumped over the lazy dog”"))
        #expect(debugText.contains("0"))
        #if ENABLE_TEXT_EXTRACTION_FILTER
        #expect(!debugText.contains("Here’s to the crazy ones"))
        #expect(!debugText.contains("The round pegs in the square holes"))
        #expect(!debugText.contains("The ones who see things differently"))
        #expect(!debugText.contains("And they have no respect for the status quo"))
        #expect(!debugText.contains("They push the human race forward"))
        #expect(!debugText.contains("Because the people who are crazy"))
        #endif // ENABLE_TEXT_EXTRACTION_FILTER
    }

    @Test
    func zeroWordLimit() async throws {
        try await webView.load(html: "Hello")

        let configuration = extractionConfigurationWithFilteringDisabled()
        configuration.outputFormat = .plainText
        configuration.maxWordsPerParagraph = 0

        #expect(try await webView.debugText(configuration) == "…")
    }

    @Test
    func skipNearlyTransparentContentByDefault() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <body>
                    <div>visible container text</div>
                    <div style="opacity: 0">transparent container text</div>
                    <label for="labeled-field">Visible label</label>
                    <input id="labeled-field" style="opacity: 0" placeholder="labeled transparent field">
                    <input style="opacity: 0" placeholder="unlabeled transparent field">
                </body>
                </html>
                """
        )

        let defaultText = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
        #expect(defaultText.contains("visible container text"))
        #expect(!defaultText.contains("transparent container text"))
        #expect(defaultText.contains("labeled transparent field"))
        #expect(!defaultText.contains("unlabeled transparent field"))
    }

    @Test
    func extractTransparentCheckboxOverVisualProxy() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                <style>
                .control { position: relative; width: 40px; height: 40px; }
                .control input { position: absolute; inset: 0; width: 40px; height: 40px; margin: 0; opacity: 0; }
                .box { position: absolute; left: 11px; top: 11px; width: 18px; height: 18px; border: 2px solid #5f6368; border-radius: 2px; }
                </style>
                </head>
                <body>
                    <div class="control">
                        <input type="checkbox" aria-label="Select photos" checked>
                        <div class="box"><svg aria-hidden="true" viewBox="0 0 24 24" width="14" height="14"><path d="M1 12 8 19 22 4"></path></svg></div>
                    </div>
                    <input type="checkbox" aria-label="Genuinely invisible" style="opacity: 0">
                </body>
                </html>
                """
        )

        let defaultText = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
        #expect(defaultText.contains("Select photos"))
        #expect(defaultText.contains("checkbox"))
        #expect(defaultText.contains("checked"))
        #expect(!defaultText.contains("Genuinely invisible"))
        #expect(!defaultText.contains("image"))
    }

    @Test
    func extractTransparentOneTimeCodeFieldOverVisualProxies() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                <style>
                .otp { position: relative; width: 336px; height: 48px; }
                .otp input { position: absolute; inset: 0; width: 336px; height: 48px; margin: 0; opacity: 0.02; color: transparent; }
                .otp .box { position: absolute; top: 0; width: 46px; height: 48px; border: 1px solid #5f6368; background-color: #171920; }
                </style>
                </head>
                <body>
                    <div class="otp">
                        <input type="text" autocomplete="one-time-code" aria-label="Security code">
                        <div class="box" aria-hidden="true" style="left: 0"></div>
                        <div class="box" aria-hidden="true" style="left: 58px"></div>
                        <div class="box" aria-hidden="true" style="left: 116px"></div>
                        <div class="box" aria-hidden="true" style="left: 174px"></div>
                        <div class="box" aria-hidden="true" style="left: 232px"></div>
                        <div class="box" aria-hidden="true" style="left: 290px"></div>
                    </div>
                    <input type="text" autocomplete="one-time-code" aria-label="Genuinely invisible code" style="opacity: 0.02">
                </body>
                </html>
                """
        )

        let defaultText = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
        #expect(defaultText.contains("Security code"))
        #expect(!defaultText.contains("Genuinely invisible code"))
    }

    @Test
    func minimalHTMLOutput() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html>\
                    <body>\
                        <p>Hello <span class='asdf'>world</span></p>\
                        <input data-name='form' type='password' placeholder='Password field' />\
                        <div contenteditable>This <span id='target'>text</span> is editable</div>\
                    </body>\
                </html>
                """
        )

        let configuration = _WKTextExtractionConfiguration()
        configuration.configureForMinimalOutput()
        configuration.outputFormat = .HTML
        configuration.filterOptions = []

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("Hello world"))
        #expect(debugText.contains("This text is editable"))
        #expect(!debugText.contains("data-name"))
        #expect(!debugText.contains("form"))
        #expect(!debugText.contains("target"))
        #expect(!debugText.contains("asdf"))
    }

    @Test
    func nestedDetailsDoesNotHang() async throws {
        try await webView.load(html: "<details><details>x<summary></summary></details></details>")

        _ = await webView._requestAllText()
    }

    @Test
    func filterOptions() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        func extractText(filterOptions: _WKTextExtractionFilterOptions) async throws -> _WKTextExtractionResult {
            let configuration = _WKTextExtractionConfiguration()
            configuration.filterOptions = filterOptions
            return try #require(await webView._extractDebugText(with: configuration))
        }

        do {
            let result = try await extractText(filterOptions: [])
            #expect(result.textContent.contains("“The quick brown fox jumped over the lazy dog”"))
            #expect(result.textContent.contains("Here’s to the crazy ones"))
            #expect(!result.filteredOutAnyText)
        }
        do {
            let result = try await extractText(filterOptions: .textRecognition)
            #expect(result.textContent.contains("“The quick brown fox jumped over the lazy dog”"))
            #if ENABLE_TEXT_EXTRACTION_FILTER
            #expect(!result.textContent.contains("Here’s to the crazy ones"))
            #expect(result.filteredOutAnyText)
            #endif
        }
        do {
            let result = try await extractText(filterOptions: .classifier)
            #expect(result.textContent.contains("“The quick brown fox jumped over the lazy dog”"))
            #expect(result.textContent.contains("Here’s to the crazy ones"))
            #expect(!result.filteredOutAnyText)
        }
    }

    @Test
    func filterRedundantTextInLinks() async throws {
        try await webView.load(
            html: """
                <body>\
                <a class='first' href='http://apple.com'>apple</a>\
                <a class='second' href='http://webkit.org'>webkit</a>\
                </body>
                """
        )

        let world = worldForCreatingJSHandles()
        let firstLink = try #require(await webView.querySelector(".first", in: world))
        let secondLink = try #require(await webView.querySelector(".second", in: world))

        let configuration = _WKTextExtractionConfiguration()
        configuration.includeURLs = false
        configuration.includeRects = false
        configuration.nodeIdentifierInclusion = .none
        configuration.addClientAttribute("href", value: "url1.com", forNode: firstLink)
        configuration.addClientAttribute("href", value: "webkit.org", forNode: secondLink)

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("link href=url1.com 'apple'"))
        #expect(debugText.contains("link href=webkit.org"))
    }

    @Test
    func nodesToSkip() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let world = worldForCreatingJSHandles()
        let selectHandle = try #require(await webView.querySelector("select", in: world))
        let inputHandle = try #require(await webView.querySelector("input[type=email]", in: world))
        let editorHandle = try #require(await webView.querySelector("div[contenteditable]", in: world))
        let hiddenTextHandle = try #require(await webView.querySelector("h4", in: world))

        let configuration = _WKTextExtractionConfiguration()
        configuration.nodesToSkip = [editorHandle, selectHandle, inputHandle, hiddenTextHandle]
        configuration.outputFormat = .markdown
        configuration.nodeIdentifierInclusion = .none
        configuration.includeRects = false

        let lines = try await webView.debugText(configuration).components(separatedBy: "\n")
        #expect(lines.count == 4)
        #expect(lines[0] == "Test")
        #expect(lines[1] == "subject SUBJECT")
        #expect(lines[2] == "![]() [](file:///menu)![]()")
        #expect(lines[3] == "0")
    }

    @Test
    func requestJSHandleForNodeIdentifier() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let extractionConfiguration = _WKTextExtractionConfiguration()
        extractionConfiguration.includeRects = false
        extractionConfiguration.includeURLs = false
        let extractionResult = try #require(await webView._extractDebugText(with: extractionConfiguration))

        func plainConfiguration() -> _WKTextExtractionConfiguration {
            let configuration = _WKTextExtractionConfiguration()
            configuration.includeRects = false
            configuration.includeURLs = false
            configuration.nodeIdentifierInclusion = .none
            return configuration
        }

        let composeID = extractNodeIdentifier(extractionResult.textContent, "Compose a new message")

        let subjectConfiguration = plainConfiguration()
        subjectConfiguration.targetNode = await extractionResult.requestJSHandle(
            forNodeIdentifier: composeID,
            searchText: "Subject"
        )
        #expect(try await webView.debugText(subjectConfiguration) == "root\n\tlabel=Heading 'Subject'")

        let bodyConfiguration = plainConfiguration()
        bodyConfiguration.targetNode = await extractionResult.requestJSHandle(
            forNodeIdentifier: nil,
            searchText: "The quick brown fox"
        )
        let expected = "root '\u{201C}The quick brown fox jumped over the lazy dog\u{201D}'"
        #expect(try await webView.debugText(bodyConfiguration) == expected)

        #expect(await extractionResult.requestJSHandle(forNodeIdentifier: composeID, searchText: "text that does not exist") != nil)
        #expect(await extractionResult.requestJSHandle(forNodeIdentifier: nil, searchText: "text that does not exist") == nil)
    }

    @Test
    func requestJSHandleForStaleNodeIdentifier() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let extractionConfiguration = _WKTextExtractionConfiguration()
        extractionConfiguration.includeRects = false
        extractionConfiguration.includeURLs = false
        let extractionResult = try #require(await webView._extractDebugText(with: extractionConfiguration))

        let nodeID = try #require(extractNodeIdentifier(extractionResult.textContent, "Compose a new message"))
        try await webView.load(html: "<body>different content</body>")
        #expect(await extractionResult.requestJSHandle(forNodeIdentifier: nodeID, searchText: nil) == nil)
    }

    @Test
    func requestJSHandleForNodeIdentifierCaseSensitive() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let extractionConfiguration = _WKTextExtractionConfiguration()
        extractionConfiguration.includeRects = false
        extractionConfiguration.includeURLs = false
        let extractionResult = try #require(await webView._extractDebugText(with: extractionConfiguration))

        func configuration(targetingSearchText searchText: String) async -> _WKTextExtractionConfiguration {
            let configuration = _WKTextExtractionConfiguration()
            configuration.includeRects = false
            configuration.includeURLs = false
            configuration.nodeIdentifierInclusion = .none
            configuration.targetNode = await extractionResult.requestJSHandle(forNodeIdentifier: nil, searchText: searchText)
            return configuration
        }

        let lowercaseSubjectText = try await webView.debugText(await configuration(targetingSearchText: "subject"))
        #expect(lowercaseSubjectText == "root\n\tlabel=Lowercase 'subject'")

        let uppercaseSubjectText = try await webView.debugText(await configuration(targetingSearchText: "SUBJECT"))
        #expect(uppercaseSubjectText == "root\n\tlabel=Uppercase 'SUBJECT'")
    }

    @Test
    func requestContainerJSHandleForNodeIdentifier() async throws {
        try await webView.load(testPageNamed: "debug-text-product")

        let extractionConfiguration = _WKTextExtractionConfiguration()
        extractionConfiguration.includeRects = false
        extractionConfiguration.includeURLs = false
        extractionConfiguration.nodeIdentifierInclusion = .allContainers
        let extractionResult = try #require(await webView._extractDebugText(with: extractionConfiguration))

        func markdownConfiguration(targeting handle: _WKJSHandle?) -> _WKTextExtractionConfiguration {
            let configuration = _WKTextExtractionConfiguration()
            configuration.includeRects = false
            configuration.outputFormat = .markdown
            configuration.nodeIdentifierInclusion = .none
            configuration.targetNode = handle
            return configuration
        }

        let handleFromSearchText = await extractionResult.requestContainerJSHandle(
            forNodeIdentifier: nil,
            searchText: "Premium Wireless Headphones"
        )
        let debugText1 = try await webView.debugText(markdownConfiguration(targeting: handleFromSearchText))

        let priceID = extractNodeIdentifier(extractionResult.textContent, "$99.99")
        let handleFromNodeID = await extractionResult.requestContainerJSHandle(forNodeIdentifier: priceID, searchText: nil)
        let debugText2 = try await webView.debugText(markdownConfiguration(targeting: handleFromNodeID))

        #expect(debugText1 == debugText2)
        #expect(debugText1.contains("Sale - 20% Off"))
        #expect(debugText1.contains("In Stock - Ships within 24 hours"))
        #expect(!debugText1.contains("Customers Also Bought"))

        var handle = await extractionResult.requestContainerJSHandle(forNodeIdentifier: priceID, searchText: "text that does not exist")
        #expect(handle != nil)

        handle = await extractionResult.requestContainerJSHandle(forNodeIdentifier: nil, searchText: "text that does not exist")
        #expect(handle == nil)
    }

    @Test
    func requestContainerJSHandleForSearchTexts() async throws {
        try await webView.load(testPageNamed: "debug-text-product")

        let extractionConfiguration = _WKTextExtractionConfiguration()
        extractionConfiguration.includeRects = false
        extractionConfiguration.includeURLs = false
        extractionConfiguration.nodeIdentifierInclusion = .allContainers
        let extractionResult = try #require(await webView._extractDebugText(with: extractionConfiguration))

        let firstSectionID = extractNodeIdentifier(extractionResult.textContent, "section")
        let handle = await extractionResult.requestContainerJSHandle(
            forSearchTexts: ["Premium Wireless Headphones", "Ships within 24 hours"],
            nodeIdentifier: firstSectionID
        )

        let configuration = _WKTextExtractionConfiguration()
        configuration.includeRects = false
        configuration.outputFormat = .markdown
        configuration.nodeIdentifierInclusion = .none
        configuration.targetNode = handle

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("Premium Wireless Headphones"))
        #expect(debugText.contains("Ships within 24 hours"))
        #expect(!debugText.contains("Customers Also Bought"))
        #expect(!debugText.contains("The noise cancellation is incredible"))
    }

    @Test
    func requestContainerJSHandleForSearchTextsFallsBackToBodyWhenTargetMisses() async throws {
        try await webView.load(testPageNamed: "debug-text-product")

        let extractionConfiguration = _WKTextExtractionConfiguration()
        extractionConfiguration.includeRects = false
        extractionConfiguration.includeURLs = false
        extractionConfiguration.includeAccessibilityAttributes = true
        extractionConfiguration.nodeIdentifierInclusion = .allContainers
        let extractionResult = try #require(await webView._extractDebugText(with: extractionConfiguration))

        let reviewsSectionID = try #require(extractNodeIdentifier(extractionResult.textContent, "5 Reviews"))

        let handle = await extractionResult.requestContainerJSHandle(
            forSearchTexts: ["Premium Wireless Headphones", "Ships within 24 hours"],
            nodeIdentifier: reviewsSectionID
        )

        let configuration = _WKTextExtractionConfiguration()
        configuration.includeRects = false
        configuration.outputFormat = .markdown
        configuration.nodeIdentifierInclusion = .none
        configuration.targetNode = handle

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("Premium Wireless Headphones"))
        #expect(debugText.contains("Ships within 24 hours"))
        #expect(!debugText.contains("Customer Reviews"))
    }

    @Test
    func requestJSHandleForNodeInDetachedSubframe() async throws {
        let subframes = SubframeCollector()
        let navigationDelegate = TestNavigationDelegate()
        subframes.install(on: navigationDelegate)
        webView.navigationDelegate = navigationDelegate

        webView.loadHTMLString(
            "<h1>Subframe</h1> <iframe id='child' srcdoc=\"<button id='target'>subframe target</p>\"></iframe>",
            baseURL: nil
        )
        try await navigationDelegate.waitForDidFinishNavigation()

        try await waitForCondition("subframe to finish loading", timeout: .seconds(3)) {
            try await webView.callJavaScript(returning: Bool.self) {
                """
                return !!document.getElementById('child').contentDocument \
                    && document.getElementById('child').contentDocument.readyState === 'complete';
                """
            }
        }

        let extractionConfiguration = _WKTextExtractionConfiguration()
        extractionConfiguration.additionalFrames = subframes.frames
        let extractionResult = try #require(await webView._extractDebugText(with: extractionConfiguration))

        let buttonIdentifier = try #require(extractNodeIdentifier(extractionResult.textContent, "subframe target"))

        try await webView.callJavaScript { "document.querySelector('iframe').remove()" }
        await webView.nextPresentationUpdate()

        #expect(await extractionResult.requestJSHandle(forNodeIdentifier: buttonIdentifier, searchText: nil) == nil)
        let textContent = try await webView.callJavaScript(returning: String.self) { "return document.querySelector('h1').textContent" }
        #expect(textContent == "Subframe")
    }

    @Test
    func resolveTargetNodeFromSelectorData() async throws {
        let selectorData: Data
        do {
            let originalWebView = makeWebViewForTextExtractionTesting()
            try await originalWebView.load(testPageNamed: "debug-text-extraction")

            let world = worldForCreatingJSHandles()
            let subjectHandle = try #require(await originalWebView.querySelector("h3", in: world))
            selectorData = try #require(await originalWebView._getSelectorPathDataForNode(subjectHandle))
        }

        let newWebView = makeWebViewForTextExtractionTesting()
        try await newWebView.load(testPageNamed: "debug-text-extraction")

        let resolvedHandle = try #require(await newWebView._getNodeForSelectorPathData(selectorData))

        let configuration = _WKTextExtractionConfiguration()
        configuration.includeRects = false
        configuration.includeURLs = false
        configuration.nodeIdentifierInclusion = .none
        configuration.targetNode = resolvedHandle

        #expect(try await newWebView.debugText(configuration) == "root\n\tlabel=Heading 'Subject'")
    }

    @Test
    func clickInteractionWithElementHandle() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head><meta name='viewport' content='width=device-width, initial-scale=1'></head>
                <body>
                    <button id='submit' onclick="document.getElementById('result').textContent = 'clicked'">Submit</button>
                    <div id='result'>none</div>
                </body>
                </html>
                """
        )

        let world = worldForCreatingJSHandles()
        let buttonHandle = try #require(await webView.querySelector("#submit", in: world))
        let selectorData = try #require(await webView._getSelectorPathDataForNode(buttonHandle))
        let elementHandle = try #require(await webView._getNodeForSelectorPathData(selectorData))

        let click = _WKTextExtractionInteraction(action: .click)
        click.elementHandle = elementHandle
        click.nodeIdentifier = "0_99999" // Intentionally invalid node UID.

        let description = try await click.debugDescription(in: webView)
        #expect(description.contains("Submit"))

        let result = try #require(await webView._performInteraction(click))
        #expect(result.error == nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return document.getElementById('result').textContent" } == "clicked"
        )
    }

    @Test
    func clickInteractionWithTextOnly() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        #expect(try await webView.callJavaScript(returning: String.self) { "return clickCount.textContent" } == "0")

        let click = _WKTextExtractionInteraction(action: .click)
        click.text = "Test"

        let result = try #require(await webView._performInteraction(click))
        #expect(result.error == nil)
        #expect(try await webView.callJavaScript(returning: String.self) { "return clickCount.textContent" } == "1")
    }

    @Test
    func scrollToRevealFallsBackToFullDocumentWhenNodeMisses() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                    <meta name='viewport' content='width=device-width, initial-scale=1'>
                </head>
                <body>
                    <button id='anchor'>Anchor Section</button>
                    <div style='height: 5000px'></div>
                    <div id='target'>Reveal Me Down Here</div>
                </body>
                </html>
                """
        )

        #expect(try await webView.callJavaScript(returning: Double.self) { "return window.scrollY" } == 0)

        let configuration = _WKTextExtractionConfiguration()
        configuration.includeRects = false
        configuration.nodeIdentifierInclusion = .allContainers
        let debugText = try await webView.debugText(configuration)
        let anchorID = try #require(extractNodeIdentifier(debugText, "Anchor Section"))

        let scroll = _WKTextExtractionInteraction(action: .scroll)
        scroll.nodeIdentifier = anchorID
        scroll.text = "Reveal Me Down Here"

        let result = try #require(await webView._performInteraction(scroll))
        #expect(result.error == nil)
        #expect(try await webView.callJavaScript(returning: Double.self) { "return window.scrollY" } > 0)

        let missingScroll = _WKTextExtractionInteraction(action: .scroll)
        missingScroll.nodeIdentifier = anchorID
        missingScroll.text = "This text does not exist anywhere"

        let missingResult = try #require(await webView._performInteraction(missingScroll))
        #expect(missingResult.error != nil)
    }

    @Test
    func clickInteractionWhileInBackground() async throws {
        let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400, backgroundTextExtraction: true)

        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                    <meta name='viewport' content='width=device-width, initial-scale=1'>
                </head>
                <body>
                    <button>Click Me</button>
                    <div id='result'>pending</div>
                    <script>
                        document.querySelector('button').addEventListener('click', async () => {
                            for (let i = 0; i < 3; ++i) {
                                await new Promise(resolve => setTimeout(resolve, 50));
                                await new Promise(requestAnimationFrame);
                            }
                            document.getElementById('result').textContent = 'completed';
                        });
                    </script>
                </body>
                </html>
                """
        )

        simulateHostApplicationEnteredBackground(webView)

        let debugText = try await webView.debugText()
        let buttonID = try #require(extractNodeIdentifier(debugText, "Click Me"))

        let click = _WKTextExtractionInteraction(action: .click)
        click.nodeIdentifier = buttonID

        let result = try #require(await webView._performInteraction(click))
        #expect(result.error == nil)

        try await waitForCondition("result text to become 'completed'") {
            let textContent = try await webView.callJavaScript(returning: String.self) {
                "return document.getElementById('result').textContent"
            }
            return textContent == "completed"
        }
    }

    @Test
    func invalidURLsAreSkipped() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <body>
                    <a href="https://example.com/valid">Valid link</a>
                    <a href="   https://not a valid url   ">Link with spaces</a>
                    <a href="https://example
                .com/newline">Link with newline</a>
                    <a href="">Empty href</a>
                    <img src="https://example.com/valid.png" alt="Valid image">
                    <img src="   not a valid src   " alt="Image with spaces">
                    <img src="https://example
                .com/broken.png" alt="Image with newline">
                    <img src="" alt="Empty src">
                </body>
                </html>
                """
        )

        for format in [_WKTextExtractionOutputFormat.textTree, .markdown] {
            let configuration = _WKTextExtractionConfiguration()
            configuration.includeURLs = true
            configuration.shortenURLs = true
            configuration.outputFormat = format

            let debugText = try await webView.debugText(configuration)
            #expect(debugText.contains("Valid link"))
            #expect(debugText.contains("example.com"))
            #expect(debugText.contains("Valid image"))

            #expect(debugText.contains("Link with spaces"))
            #expect(debugText.contains("Link with newline"))
            #expect(debugText.contains("Image with spaces"))
            #expect(debugText.contains("Image with newline"))

            #expect(!debugText.contains("not a valid"))
        }
    }

    @Test
    func authorShadowDOMText() async throws {
        try await webView.load(
            html: #"""
                <!DOCTYPE html>
                <html>
                <body>
                    <p>Before shadow content</p>
                    <custom-button variant="primary" text="Next"></custom-button>
                    <custom-button variant="secondary" text="Cancel"></custom-button>
                    <my-card></my-card>
                    <script>
                    class CustomButton extends HTMLElement {
                        connectedCallback() {
                            this.attachShadow({ mode: 'open' });
                            const text = this.getAttribute('text') || '';
                            this.shadowRoot.innerHTML = `
                                <button type="button">
                                    <span class="button__label">${text}</span>
                                </button>
                            `;
                        }
                    }
                    customElements.define('custom-button', CustomButton);

                    class MyCard extends HTMLElement {
                        connectedCallback() {
                            this.attachShadow({ mode: 'open' });
                            this.shadowRoot.innerHTML = `
                                <div>
                                    <h2>Shadow heading</h2>
                                    <p>Shadow paragraph text</p>
                                    <a href="https://example.com">Shadow link</a>
                                </div>
                            `;
                        }
                    }
                    customElements.define('my-card', MyCard);
                    </script>
                </body>
                </html>
                """#
        )

        let debugText = try await webView.debugText()
        #expect(debugText.contains("Before shadow content"))
        #expect(debugText.contains("Next"))
        #expect(debugText.contains("Cancel"))
        #expect(debugText.contains("Shadow heading"))
        #expect(debugText.contains("Shadow paragraph text"))
        #expect(debugText.contains("Shadow link"))
    }

    @Test
    func clickInteractionWithExtractionContext() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                    <meta name='viewport' content='width=device-width, initial-scale=1'>
                </head>
                <body>
                    <button onclick="document.getElementById('result').textContent = 'original'">Edit</button>
                    <div id='result'>none</div>
                </body>
                </html>
                """
        )

        let extractionResult = try #require(await webView._extractDebugText(with: _WKTextExtractionConfiguration()))
        #expect(extractionResult.textContent.contains("Edit"))

        try await webView.callJavaScript {
            """
            let btn = document.createElement('button');\
            btn.textContent = 'Edit';\
            btn.onclick = () => document.getElementById('result').textContent = 'new';\
            document.body.insertBefore(btn, document.body.firstChild); true;
            """
        }

        let click = _WKTextExtractionInteraction(action: .click, extractionContext: extractionResult)
        click.text = "Edit"
        _ = try #require(await webView._performInteraction(click))

        let textContent = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('result').textContent"
        }
        #expect(textContent == "original")
    }

    @Test
    func shortenURLsWithTopHostName() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html><body>\
                    <a href="https://webkit.org/blog/post">Same-host link</a>\
                    <a href="https://webkit.org">Root link</a>\
                    <a href="https://example.com/other">Cross-host link</a>\
                </body></html>
                """,
            baseURL: URL(string: "http://webkit.org")
        )

        let configuration = _WKTextExtractionConfiguration()
        configuration.includeURLs = true
        configuration.shortenURLs = true

        let debugText = try await webView.debugText(configuration)
        #expect(debugText.contains("url=/blog/post"))
        #expect(debugText.contains("url=/"))
        #expect(debugText.contains("example.com/other"))
    }

    @Test
    func extractionContextPrefersInteractiveElement() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                    <meta name='viewport' content='width=device-width, initial-scale=1'>
                </head>
                <body>
                    <h1>Sign in</h1>
                    <button onclick="document.getElementById('result').textContent = 'submitted'">Sign in</button>
                    <div id="result">none</div>
                </body>
                </html>
                """
        )

        let extractionResult = try #require(await webView._extractDebugText(with: _WKTextExtractionConfiguration()))
        #expect(extractionResult.textContent.contains("Sign in"))

        let click = _WKTextExtractionInteraction(action: .click, extractionContext: extractionResult)
        click.text = "Sign in"
        let result = try #require(await webView._performInteraction(click))
        #expect(result.error == nil)

        let textContent = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('result').textContent"
        }
        #expect(textContent == "submitted")
    }

    #if ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
    @Test
    func resultOrigin() async throws {
        var server = HTTPServer(protocol: .http) {
            Route("/") {
                "<html><body>Hello world</body></html>"
            }
        }

        try await server.run { serverConfiguration in
            let webView = makeWebViewForTextExtractionTesting()
            let url = try #require(URL(string: "http://127.0.0.1:\(serverConfiguration.port)/"))
            try await webView.loadAndWait(URLRequest(url: url))

            let extractionResult = try #require(await webView._extractDebugText(with: _WKTextExtractionConfiguration()))
            #expect(extractionResult.textContent.contains("Hello world"))

            let origin = try #require(extractionResult.origin)
            #expect(origin.`protocol` == "http")
            #expect(origin.host == "127.0.0.1")
            #expect(origin.port == serverConfiguration.port)
        }
    }

    @Test
    func subframeInteractions() async throws {
        var server = makeSubframeServer(
            crossOriginButtonText: "Cross origin: click here",
            sameOriginButtonText: "Same origin: click here"
        )

        try await server.run { serverConfiguration in
            let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400)
            let subframes = try await loadSubframePage(in: webView, port: serverConfiguration.port)

            let extractionConfiguration = _WKTextExtractionConfiguration()
            extractionConfiguration.includeRects = false
            extractionConfiguration.includeURLs = false
            extractionConfiguration.additionalFrames = subframes

            let world = worldForCreatingJSHandles()
            for subframe in subframes {
                let button = try #require(await webView.querySelector("button", in: world, frame: subframe))
                extractionConfiguration.addClientAttribute("foo", value: "bar", forNode: button)
            }

            let debugText = try await webView.debugText(extractionConfiguration)
            #expect(debugText.matches(of: /foo=bar/).count == 2)

            for buttonText in ["Same origin: click here", "Cross origin: click here"] {
                let interaction = _WKTextExtractionInteraction(action: .click)
                interaction.nodeIdentifier = extractNodeIdentifier(debugText, buttonText)

                #expect(
                    try await interaction.debugDescription(in: webView)
                        == "Click on button under article labeled “Button container”, with rendered text “\(buttonText)”"
                )

                let result = try #require(await webView._performInteraction(interaction))
                #expect(result.error == nil)
            }

            let debugTextAfterClicks = try await webView.debugText(extractionConfiguration)
            #expect(debugTextAfterClicks.matches(of: /Click count: 1/).count == 2)
        }
    }

    @Test
    func subframeOriginInDebugText() async throws {
        var server = makeSubframeServer(crossOriginButtonText: "Cross", sameOriginButtonText: "Same")

        try await server.run { serverConfiguration in
            let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400)
            _ = try await loadSubframePage(in: webView, port: serverConfiguration.port)

            let configuration = _WKTextExtractionConfiguration()
            configuration.includeRects = false
            configuration.includeURLs = true

            let debugText = try await webView.debugText(configuration)
            #expect(debugText.contains("origin=localhost:\(serverConfiguration.port)"))
            #expect(!debugText.contains("origin=http://localhost"))
            #expect(!debugText.contains("origin=127.0.0.1"))
        }
    }

    @Test
    func requestFrameInfoForNodeIdentifier() async throws {
        var server = makeSubframeServer(
            crossOriginButtonText: "Cross origin: click here",
            sameOriginButtonText: "Same origin: click here"
        )

        try await server.run { serverConfiguration in
            let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400)
            let subframes = try await loadSubframePage(in: webView, port: serverConfiguration.port)

            let extractionConfiguration = _WKTextExtractionConfiguration()
            extractionConfiguration.includeRects = false
            extractionConfiguration.additionalFrames = subframes

            let result = try #require(await webView._extractDebugText(with: extractionConfiguration))
            let debugText = result.textContent

            @MainActor
            func frameInfo(forNodeIdentifier nodeIdentifier: String) async -> WKFrameInfo? {
                await result.requestFrameInfo(forNodeIdentifier: nodeIdentifier)
            }

            @MainActor
            func frameInfo(for searchText: String) async throws -> WKFrameInfo {
                let nodeIdentifier = try #require(extractNodeIdentifier(debugText, searchText))
                return try #require(await frameInfo(forNodeIdentifier: nodeIdentifier))
            }

            let crossOriginFrameInfo = try await frameInfo(for: "origin=localhost:\(serverConfiguration.port)")
            #expect(!crossOriginFrameInfo.isMainFrame)
            #expect(crossOriginFrameInfo.request.url?.path == "/subframe-cross.html")

            let crossOriginButtonFrameInfo = try await frameInfo(for: "Cross origin: click here")
            #expect(!crossOriginButtonFrameInfo.isMainFrame)
            #expect(crossOriginButtonFrameInfo.request.url?.path == "/subframe-cross.html")

            let sameOriginButtonFrameInfo = try await frameInfo(for: "Same origin: click here")
            #expect(!sameOriginButtonFrameInfo.isMainFrame)
            #expect(sameOriginButtonFrameInfo.request.url?.path == "/subframe-same.html")

            #expect(try await frameInfo(for: "Link to WebKit home page").isMainFrame)

            #expect(await frameInfo(forNodeIdentifier: "not-a-node-identifier") == nil)
        }
    }
    #endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

    @Test
    func hoverInteractionWithTextOnly() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html>\
                <head>\
                    <meta name='viewport' content='width=device-width, initial-scale=1'>\
                </head>\
                <body>\
                    <div id='target' style='padding: 20px;'>Hover Me</div>\
                    <div id='result'>none</div>\
                    <script>\
                        document.getElementById('target').addEventListener('mouseover', () => {\
                            document.getElementById('result').textContent = 'hovered';\
                        });\
                    </script>\
                </body>\
                </html>
                """
        )

        #expect(try await webView.callJavaScript(returning: String.self) { "return result.textContent" } == "none")

        let hover = _WKTextExtractionInteraction(action: .hover)
        hover.text = "Hover Me"

        let result = try #require(await webView._performInteraction(hover))
        #expect(result.error == nil)
        #expect(try await webView.callJavaScript(returning: String.self) { "return result.textContent" } == "hovered")
    }

    @Test
    func hoverInteractionWithExtractionContext() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                    <meta name='viewport' content='width=device-width, initial-scale=1'>
                </head>
                <body>
                    <button id='target'>Menu</button>
                    <div id='result'>none</div>
                    <script>
                        document.getElementById('target').addEventListener('mouseenter', () => {
                            document.getElementById('result').textContent = 'entered';
                        });
                    </script>
                </body>
                </html>
                """
        )

        let extractionResult = try #require(await webView._extractDebugText(with: _WKTextExtractionConfiguration()))
        #expect(extractionResult.textContent.contains("Menu"))

        let hover = _WKTextExtractionInteraction(action: .hover, extractionContext: extractionResult)
        hover.text = "Menu"
        let result = try #require(await webView._performInteraction(hover))
        #expect(result.error == nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return document.getElementById('result').textContent" } == "entered"
        )
    }

    @Test
    func hoverDoesNotClick() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <head>
                    <meta name='viewport' content='width=device-width, initial-scale=1'>
                </head>
                <body>
                    <div id='target' style='padding: 20px;'>Target</div>
                    <div id='hover-result'>none</div>
                    <div id='click-result'>none</div>
                    <script>
                        let target = document.getElementById('target');
                        target.addEventListener('mouseover', () => {
                            document.getElementById('hover-result').textContent = 'hovered';
                        });
                        target.addEventListener('click', () => {
                            document.getElementById('click-result').textContent = 'clicked';
                        });
                    </script>
                </body>
                </html>
                """
        )

        let hover = _WKTextExtractionInteraction(action: .hover)
        hover.text = "Target"

        let result = try #require(await webView._performInteraction(hover))
        #expect(result.error == nil)

        var textContent = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('hover-result').textContent"
        }
        #expect(textContent == "hovered")

        textContent = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('click-result').textContent"
        }
        #expect(textContent == "none")
    }

    @Test
    func interactedElementBounds() async throws {
        try await webView.load(testPageNamed: "debug-text-extraction")

        let debugText = try await webView.debugText(extractionConfigurationWithFilteringDisabled())
        let testButtonID = extractNodeIdentifier(debugText, "Test")
        let emailID = extractNodeIdentifier(debugText, "email")

        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.nodeIdentifier = testButtonID
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            let bounds = result.interactedElementBounds
            #expect(!bounds.isNull)
            #expect(bounds.width > 0)
            #expect(bounds.height > 0)
            #expect(bounds.maxX < 800)
            #expect(bounds.maxY < 600)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .textInput)
            interaction.nodeIdentifier = emailID
            interaction.text = "hello@webkit.org"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
            let bounds = result.interactedElementBounds
            #expect(!bounds.isNull)
            #expect(bounds.width > 0)
            #expect(bounds.height > 0)
        }
        do {
            let interaction = _WKTextExtractionInteraction(action: .click)
            interaction.text = "this text does not exist anywhere on the page"
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error != nil)
            #expect(result.interactedElementBounds.isNull)
        }
    }

    #if WTF_PLATFORM_MAC
    @Test
    func keyPressInsertsCharactersInOrder() async throws {
        let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400)
        try await webView.load(html: "<input type='text' id='q'>")
        try await webView.callJavaScript { "document.getElementById('q').focus()" }

        for character in ["a", "b", "c"] {
            let interaction = _WKTextExtractionInteraction(action: .keyPress)
            interaction.text = character
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
        }

        #expect(try await webView.callJavaScript(returning: String.self) { "return document.getElementById('q').value" } == "abc")
    }

    @Test
    func keyPressInsertsBracketCharacters() async throws {
        let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400)
        try await webView.load(html: "<input type='text' id='q'>")
        try await webView.callJavaScript { "document.getElementById('q').focus()" }

        for character in ["[", "{", "]", "}"] {
            let interaction = _WKTextExtractionInteraction(action: .keyPress)
            interaction.text = character
            let result = try #require(await webView._performInteraction(interaction))
            #expect(result.error == nil)
        }

        #expect(try await webView.callJavaScript(returning: String.self) { "return document.getElementById('q').value" } == "[{]}")
    }
    #endif // WTF_PLATFORM_MAC

    #if WTF_PLATFORM_IOS_FAMILY
    @Test
    func clickInteractionWithStalledDisplayLink() async throws {
        let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400, backgroundTextExtraction: true)

        try await webView.load(
            html: """
                <!DOCTYPE html>
                <html>
                <body>
                    <button>Click Me</button>
                    <div id='result'>pending</div>
                    <script>
                        document.querySelector('button').addEventListener('click', async function() {
                            for (let i = 0; i < 3; ++i) {
                                await new Promise(resolve => setTimeout(resolve, 50));
                                await new Promise(requestAnimationFrame);
                            }
                            document.getElementById('result').textContent = 'completed';
                        });
                    </script>
                </body>
                </html>
                """
        )

        simulateHostApplicationEnteredBackground(webView)

        let debugText = try await webView.debugText()
        let buttonID = try #require(extractNodeIdentifier(debugText, "Click Me"))

        let displayLinkHandler: AnyClass = try #require(NSClassFromString("WKDisplayLinkHandler"))
        let suppressDisplayLink: @convention(block) (AnyObject, CADisplayLink) -> Void = { _, _ in }

        try await withSwizzledObjectiveCInstanceMethod(
            replacing: displayLinkHandler,
            name: Selector(("displayLinkFired:")),
            with: suppressDisplayLink
        ) {
            let click = _WKTextExtractionInteraction(action: .click)
            click.nodeIdentifier = buttonID

            let result = try #require(await webView._performInteraction(click))
            #expect(result.error == nil)

            try await waitForCondition("rendering updates to continue after the display link stopped delivering callbacks") {
                try await webView.callJavaScript(returning: String.self) {
                    "return document.getElementById('result').textContent"
                } == "completed"
            }
        }
    }
    #endif // WTF_PLATFORM_IOS_FAMILY

    #if WTF_PLATFORM_IOS_FAMILY && !WTF_PLATFORM_MACCATALYST
    @Test
    func desktopClassHardwareEmulationInDesktopContentMode() async throws {
        func makeContentModeWebView(backgroundTextExtraction: Bool) -> TestWKWebView {
            makeWebViewForTextExtractionTesting(
                width: 400,
                height: 400,
                textExtraction: false,
                backgroundTextExtraction: backgroundTextExtraction
            )
        }

        let webView = makeContentModeWebView(backgroundTextExtraction: true)
        do {
            try await load(webView, using: .desktop)
            try await expectDesktopClassHardwareEmulation(webView)
        }
        do {
            try await load(webView, using: .mobile)
            try await expectNoDesktopClassHardwareEmulation(webView)
        }
        do {
            let mobileWebView = makeContentModeWebView(backgroundTextExtraction: true)
            try await load(mobileWebView, using: .mobile)
            try await expectNoDesktopClassHardwareEmulation(mobileWebView)
        }
        do {
            let webViewWithoutTextExtraction = makeContentModeWebView(backgroundTextExtraction: false)
            try await load(webViewWithoutTextExtraction, using: .desktop)
            try await expectNoDesktopClassHardwareEmulation(webViewWithoutTextExtraction)
        }
    }
    #endif // WTF_PLATFORM_IOS_FAMILY && !WTF_PLATFORM_MACCATALYST

    #if ENABLE_UNIFIED_PDF
    @Test
    func extractFromPDFAsMarkdown() async throws {
        let webView = makeUnifiedPDFWebView()
        try await webView.load(pdf: TestPDFBuilder.pdfData(), baseURL: try #require(URL(string: "https://www.example.com/test.pdf")))

        let configuration = _WKTextExtractionConfiguration()
        configuration.outputFormat = .markdown

        let text = try await webView.debugText(configuration)
        #expect(text.contains("Test PDF Content"))
        #expect(text.contains("555-555-1234"))
    }

    @Test
    func extractFromPDFAsTextTree() async throws {
        let webView = makeUnifiedPDFWebView()
        try await webView.load(pdf: TestPDFBuilder.pdfData(), baseURL: try #require(URL(string: "https://www.example.com/test.pdf")))

        let configuration = _WKTextExtractionConfiguration()
        configuration.outputFormat = .textTree

        let text = try await webView.debugText(configuration)
        #expect(text.hasPrefix("root"))
        #expect(text.contains("Test PDF Content"))
        #expect(text.contains("555-555-1234"))
    }

    @Test
    func extractFromPDFAsHTML() async throws {
        let webView = makeUnifiedPDFWebView()
        try await webView.load(pdf: TestPDFBuilder.pdfData(), baseURL: try #require(URL(string: "https://www.example.com/test.pdf")))

        let configuration = _WKTextExtractionConfiguration()
        configuration.outputFormat = .HTML

        let text = try await webView.debugText(configuration)
        #expect(text.hasPrefix("<body>"))
        #expect(text.hasSuffix("</body>"))
        #expect(text.contains("Test PDF Content"))
        #expect(text.contains("555-555-1234"))
    }

    @Test
    func extractFromPDFAsJSON() async throws {
        let webView = makeUnifiedPDFWebView()
        try await webView.load(pdf: TestPDFBuilder.pdfData(), baseURL: try #require(URL(string: "https://www.example.com/test.pdf")))

        let configuration = _WKTextExtractionConfiguration()
        configuration.outputFormat = .JSON

        let json = try decodeJSONObject(await webView.debugText(configuration))
        #expect(json["type"] as? String == "root")

        let children = try #require(json["children"] as? [[String: Any]])
        #expect(children.count == 1)

        let child = try #require(children.first)
        #expect(child["type"] as? String == "text")

        let content = try #require(child["content"] as? String)
        #expect(content.contains("Test PDF Content"))
        #expect(content.contains("555-555-1234"))
    }

    @Test
    func extractFromPDFAsPlainText() async throws {
        let webView = makeUnifiedPDFWebView()
        try await webView.load(pdf: TestPDFBuilder.pdfData(), baseURL: try #require(URL(string: "https://www.example.com/test.pdf")))

        let configuration = _WKTextExtractionConfiguration()
        configuration.outputFormat = .plainText

        let text = try await webView.debugText(configuration)
        #expect(text.contains("Test PDF Content"))
        #expect(text.contains("555-555-1234"))
    }

    @Test
    func extractFromPDFLink() async throws {
        let webView = makeUnifiedPDFWebView()
        try await webView.load(
            pdf: TestPDFBuilder.pdfDataWithLink(),
            baseURL: try #require(URL(string: "https://www.example.com/test-with-link.pdf"))
        )

        func debugText(_ outputFormat: _WKTextExtractionOutputFormat) async throws -> String {
            let configuration = _WKTextExtractionConfiguration()
            configuration.outputFormat = outputFormat
            return try await webView.debugText(configuration)
        }

        #expect(try await debugText(.markdown).contains("[our website](https://www.example.com/)"))
        #expect(try await debugText(.HTML).contains("<a href='https://www.example.com/'>our website</a>"))

        do {
            let json = try decodeJSONObject(await debugText(.JSON))
            let children = try #require(json["children"] as? [[String: Any]])
            let linkNode = try #require(children.first { $0["type"] as? String == "link" })

            #expect(linkNode["url"] as? String == "https://www.example.com/")

            let linkChildren = try #require(linkNode["children"] as? [[String: Any]])
            #expect(linkChildren.count == 1)
            #expect(linkChildren.first?["content"] as? String == "our website")
        }
        do {
            let text = try await debugText(.textTree)
            #expect(text.contains("link"))
            #expect(text.contains("url=https://www.example.com/"))
            #expect(text.contains("our website"))
        }
    }
    #endif // ENABLE_UNIFIED_PDF

    #if HAVE_SAFARI_SAFE_BROWSING_NAMESPACED_LISTS
    @Test
    func filteringRules() async throws {
        try await withStubbedSafeBrowsingLists(substitutionAndTruncationLists) {
            try await webView.load(testPageNamed: "debug-text-extraction")

            let debugText = try await webView.debugText(plainHTMLConfiguration())
            #expect(debugText.contains("<input type='email' placeholder='Recipient address'>f••</input>"))
            #expect(debugText.contains("<h3 aria-label='Heading'>Svbject</h3>"))
            #expect(debugText.contains("The qvick br•wn f•x jvmped •ver the lazy d•g"))
        }
    }

    #if ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
    @Test
    func filteringRulesAreIsolated() async throws {
        var server = HTTPServer(protocol: .http) {
            Route("/should-never-load") {
                "leaked"
            }
        }

        try await server.run { _ in
            try await withStubbedSafeBrowsingLists(listsToCheckNetworkAndDOMIsolation) {
                try await webView.load(testPageNamed: "debug-text-extraction")

                let debugText = try await webView.debugText(plainHTMLConfiguration())
                #expect(debugText.contains("&lt;dom:empty&gt;|&lt;net:blocked&gt;"))
                #expect(!debugText.contains("&lt;dom:leaked:"))
                #expect(!debugText.contains("&lt;net:loaded:"))
            }
        }

        #expect(server.totalRequests == 0)
    }
    #endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

    #if ENABLE_TEXT_EXTRACTION_FILTER
    @Test
    func filterExtractedStringPassthrough() async throws {
        try await withStubbedSafeBrowsingLists(substitutionAndTruncationLists) {
            try await webView.load(html: "")

            #expect(await webView._filterExtractedString("Hello world", options: []) == "Hello world")
        }
    }

    @Test
    func filterExtractedStringRulesApplied() async throws {
        try await withStubbedSafeBrowsingLists(substitutionAndTruncationLists) {
            try await webView.load(html: "")

            #expect(await webView._filterExtractedString("The quick brown fox", options: .rules) == "The qvick br•wn f•x")
        }
    }

    @Test
    func filterExtractedStringRulesTruncates() async throws {
        try await withStubbedSafeBrowsingLists(substitutionAndTruncationLists) {
            try await webView.load(html: "")

            let longString = "".padding(toLength: 1000, withPad: "x", startingAt: 0)
            #expect(await webView._filterExtractedString(longString, options: .rules) == "<too long>")
        }
    }

    @Test
    func filterExtractedStringEmptyInput() async throws {
        try await withStubbedSafeBrowsingLists(substitutionAndTruncationLists) {
            try await webView.load(html: "")

            #expect(await webView._filterExtractedString("", options: .rules) == "")
        }
    }
    #endif // ENABLE_TEXT_EXTRACTION_FILTER
    #endif // HAVE_SAFARI_SAFE_BROWSING_NAMESPACED_LISTS

    @Test
    func requestTextExtractionInSVGDocument() async throws {
        let url = try #require(
            URL(
                string: "data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%3E"
                    + "%3Ctext%20x%3D%2210%22%20y%3D%2220%22%3EHello%3C%2Ftext%3E%3C%2Fsvg%3E"
            )
        )
        try await webView.loadAndWait(URLRequest(url: url))

        #expect(await webView._requestTextExtraction(nil) != nil)
    }

    #if HAVE_SAFE_BROWSING
    @Test
    func safeBrowsingWarningBlocksTextExtraction() async throws {
        let webView = makeWebViewForTextExtractionTesting(width: 400, height: 400, fraudulentWebsiteWarning: true)

        try await withStubbedSharedLookupContext(TestLookupContext.shared()) {
            let url = try #require(Bundle.testResources.url(forResource: "debug-text-extraction", withExtension: "html"))
            webView.load(URLRequest(url: url))

            try await waitForCondition("safe browsing warning to appear", timeout: .seconds(3)) {
                webView._safeBrowsingWarning != nil
            }

            #expect(!(try await webView.debugText().contains("Test")))

            webView.visitUnsafeSite()
            try await webView._test_waitForDidFinishNavigation()

            #expect(!(try await webView.debugText().contains("Test")))
        }
    }

    #if ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
    @Test
    func delayedSafeBrowsingWarningBlocksTextExtraction() async throws {
        DelayedLookupContext.delayDuration = 1

        var server = HTTPServer(protocol: .httpsProxy) {
            Route("/test") {
                "test"
            }
        }

        try await server.run { serverConfiguration in
            let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
            storeConfiguration.httpsProxy = serverConfiguration.httpsProxy

            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
            configuration.preferences.isFraudulentWebsiteWarningEnabled = true
            configuration.preferences._textExtractionEnabled = true

            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)

            let navigationDelegate = TestNavigationDelegate()
            navigationDelegate.allowAnyTLSCertificate()
            webView.navigationDelegate = navigationDelegate

            try await withStubbedSharedLookupContext(DelayedLookupContext.shared()) {
                try await webView.callJavaScript { "window.location = 'https://example2.com/test'" }

                try await waitForCondition("delayed safe browsing warning to appear", timeout: .seconds(5)) {
                    webView._safeBrowsingWarning != nil
                }

                webView.visitUnsafeSite()

                #expect(!(try await webView.debugText().contains("test")))
            }
        }
    }

    @Test
    func backgroundTextExtractionBlocksUserMediatedHTTPFallback() async throws {
        var server = HTTPServer(protocol: .httpsProxy) {
            Route("/secure") {
                "hi"
            }
        }

        try await server.run { serverConfiguration in
            let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
            storeConfiguration.httpsProxy = serverConfiguration.httpsProxy

            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
            configuration._backgroundTextExtractionEnabled = true
            configuration.defaultWebpagePreferences.preferredHTTPSNavigationPolicy = .userMediatedFallbackToHTTP

            let webView = TestWKWebView(frame: .zero, configuration: configuration)

            let url = try #require(URL(string: "https://site.example/secure"))
            webView.load(URLRequest(url: url))

            await #expect(throws: (any Error).self) {
                try await webView._test_waitForDidFinishNavigation()
            }
            #expect(webView._safeBrowsingWarning == nil)
        }
    }
    #endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
    #endif // HAVE_SAFE_BROWSING

    #if ENABLE_SCREEN_TIME
    @Test
    func screenTimeBlocksTextExtraction() async throws {
        let checkedScreenTime = Mutex(false)
        let enforcesChildRestrictions: @convention(block) @Sendable (AnyObject) -> Bool = { _ in
            checkedScreenTime.withLock { $0 = true }
            return true
        }

        try await withSwizzledObjectiveCInstanceMethod(
            replacing: testSTScreenTimeConfigurationClass(),
            name: Selector(("enforcesChildRestrictions")),
            with: enforcesChildRestrictions
        ) {
            let webView = makeWebViewForTextExtractionTesting(width: 400, height: 300, webFeatures: ["ScreenTimeEnabled"])

            let url = try #require(URL(string: "http://webkit.org"))
            webView.loadSimulatedRequest(URLRequest(url: url), responseHTML: "<body>Hello world. This is a test</body>")
            try await webView._test_waitForDidFinishNavigation()
            await webView.nextPresentationUpdate()

            try await waitForCondition("ScreenTime to check for child restrictions") {
                checkedScreenTime.withLock { $0 }
            }

            let controller = webView._screenTimeWebpageController()
            controller.setURLIsBlocked(true)
            #expect(!(try await webView.debugText().contains("Hello world")))

            controller.setURLIsBlocked(false)
            #expect(try await webView.debugText().contains("Hello world"))
        }
    }
    #endif // ENABLE_SCREEN_TIME
}
