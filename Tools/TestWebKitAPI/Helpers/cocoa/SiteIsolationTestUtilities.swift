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

// The Swift counterparts of SiteIsolationTestUtilities.h and the helpers in SiteIsolation.mm, for site isolation tests
// written in Swift.

public import CoreGraphics
import Foundation
public import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
public import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
public import WebKit
private import WebKit_Private.WKFrameInfoPrivate
private import WebKit_Private.WKPreferencesPrivate
public import WebKit_Private.WKUIDelegatePrivate
public import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKFeature
private import WebKit_Private._WKFrameTreeNode
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Swift.String

extension WKWebViewConfiguration {
    /// Creates a configuration whose website data store sends HTTPS loads through a server's proxy.
    ///
    /// The server must use the `.httpsProxy` protocol. Loads from every `https` host then reach the server, so a
    /// test can use several sites, such as `https://example.com` and `https://webkit.org`. The server routes
    /// requests by path alone, so each route needs a path that's distinct across all of those sites.
    ///
    /// - Parameter server: The configuration of the server.
    public convenience init(httpsProxyFor server: HTTPServer.Configuration) {
        self.init()

        let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
        storeConfiguration.httpsProxy = server.httpsProxy
        websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
    }

    /// Turns an experimental feature on or off for the web views created with the configuration.
    ///
    /// - Parameters:
    ///   - key: The key of the feature, such as `IPCTestingAPIEnabled`.
    ///   - enabled: Whether to turn the feature on.
    public func setFeature(_ key: String, enabled: Bool) {
        guard let feature = WKPreferences._features().first(where: { $0.key == key }) else {
            fatalError("No experimental feature has the key \"\(key)\"")
        }
        preferences._setEnabled(enabled, for: feature)
    }
}

/// Creates a web view with site isolation enabled, and a navigation delegate that trusts any server certificate.
///
/// A web view only holds its navigation delegate weakly, so keep the delegate alive for as long as the web view
/// loads anything.
///
/// - Parameters:
///   - configuration: The configuration for the web view, such as one created with
///     ``WebKit/WKWebViewConfiguration/init(httpsProxyFor:)``. This enables site isolation in its preferences.
///   - frame: The frame of the web view.
/// - Returns: The web view and its navigation delegate.
@MainActor
public func siteIsolatedWebViewAndDelegate(
    configuration: WKWebViewConfiguration,
    frame: CGRect = .zero
) -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
    webViewAndDelegate(configuration: configuration, frame: frame, siteIsolationEnabled: true)
}

/// Creates a web view with site isolation enabled or disabled, and a navigation delegate that trusts any server
/// certificate.
///
/// Use this to run the same test with and without site isolation. A test that only needs site isolation can use
/// ``siteIsolatedWebViewAndDelegate(configuration:frame:)`` instead.
///
/// A web view only holds its navigation delegate weakly, so keep the delegate alive for as long as the web view
/// loads anything.
///
/// - Parameters:
///   - configuration: The configuration for the web view, such as one created with
///     ``WebKit/WKWebViewConfiguration/init(httpsProxyFor:)``. This sets whether site isolation is enabled in its
///     preferences.
///   - frame: The frame of the web view.
///   - siteIsolationEnabled: Whether to enable site isolation.
/// - Returns: The web view and its navigation delegate.
@MainActor
public func webViewAndDelegate(
    configuration: WKWebViewConfiguration,
    frame: CGRect = .zero,
    siteIsolationEnabled: Bool
) -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
    configuration.preferences._siteIsolationEnabled = siteIsolationEnabled

    let navigationDelegate = TestNavigationDelegate()
    navigationDelegate.allowAnyTLSCertificate()

    let webView = TestWKWebView(frame: frame, configuration: configuration)
    webView.navigationDelegate = navigationDelegate

    return (webView, navigationDelegate)
}

/// A site-isolated web view that has loaded a page, and focused the cross-origin iframe in it.
@MainActor
public struct WebViewWithFocusedCrossOriginIframe {
    /// Markup for a main frame with some text and a 400x300 cross-origin iframe with ID `iframe`, loaded from
    /// `https://webkit.org/iframe`.
    public static let mainFrameHTML =
        "<body style='margin: 0'>main frame text"
        + "<iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"

    /// The web view, which is 800x600.
    public let webView: TestWKWebView

    /// The navigation delegate of the web view, which trusts any server certificate.
    public let navigationDelegate: TestNavigationDelegate

    /// The focused iframe.
    public let childFrame: WKFrameInfo

    // swift-format-ignore: NeverForceUnwrap
    /// Loads `https://example.com/mainframe` in a site-isolated web view, then focuses its first child frame.
    ///
    /// The first child frame must be an iframe with ID `iframe`, as in ``mainFrameHTML``.
    ///
    /// - Parameter configuration: The configuration for the web view, which must send HTTPS loads to the test's
    ///   server, such as one created with ``WebKit/WKWebViewConfiguration/init(httpsProxyFor:)``.
    /// - Throws: The navigation error if the page fails to load, or ``ConditionTimedOut`` if the iframe doesn't get
    ///   focus.
    public init(configuration: WKWebViewConfiguration) async throws {
        let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
            configuration: configuration,
            frame: CGRect(x: 0, y: 0, width: 800, height: 600)
        )

        // Well formed, so this cannot fail.
        webView.load(URLRequest(url: URL(string: "https://example.com/mainframe")!))
        try await navigationDelegate.waitForDidFinishNavigation()
        await webView.nextPresentationUpdate()
        #if WTF_PLATFORM_IOS_FAMILY
        webView.focusInWindow()
        #endif

        try await webView.callJavaScript { "document.getElementById('iframe').focus()" }

        // Frame infos are snapshots, so get a new one each time to see whether the iframe has focus yet.
        var childFrame: WKFrameInfo?
        try await waitForCondition("the cross-origin iframe to have focus") {
            childFrame = await webView.firstChildFrame
            return childFrame?._isFocused ?? false
        }

        self.webView = webView
        self.navigationDelegate = navigationDelegate
        // waitForCondition only returns once childFrame is a focused frame.
        self.childFrame = childFrame!
    }
}

extension WKWebView {
    /// Runs a script that changes the selection in a frame, then waits for the selection attributes of the web view
    /// to reflect the change.
    ///
    /// Some commands check the UI process's editor state before sending anything to a web process, so they need it
    /// to be up to date with the selection.
    ///
    /// - Parameters:
    ///   - frame: The frame to run the script in.
    ///   - script: A script that changes the selection, such as `getSelection().selectAllChildren(document.body)`.
    ///   - expectedSelection: The kind of selection the script makes.
    /// - Throws: Any error raised while running the script, or ``ConditionTimedOut`` if the selection attributes don't
    ///   come to include `expectedSelection`.
    public func setSelection(
        in frame: WKFrameInfo,
        script: String,
        expecting expectedSelection: _WKSelectionAttributes
    ) async throws {
        try await callJavaScript(in: frame) { script }
        try await waitForCondition("the selection attributes to include \(expectedSelection)") {
            _selectionAttributes.contains(expectedSelection)
        }
    }

    /// The first child frame of the main frame, or `nil` if the main frame has no child frames.
    public var firstChildFrame: WKFrameInfo? {
        get async {
            let mainFrame: _WKFrameTreeNode? = await _frames()
            return mainFrame?.childFrames.first?.info
        }
    }
}

// MARK: Frame trees

/// A frame and its descendants, as one web process sees them.
///
/// With site isolation, each web process that hosts part of a page has its own frame tree for the page. A frame is
/// local in the process that hosts its document, and remote in every other process. Compare the trees of every process
/// with the trees a test expects:
///
/// ```swift
/// #expect(await webView.frameTreesInProcesses() == [
///     .local("https://example.com", children: [.remote]),
///     .remote(children: [.local("https://webkit.org")]),
/// ])
/// ```
public struct FrameTree: Sendable, Equatable, CustomStringConvertible {
    /// The origin of the frame, such as `https://example.com`, or `nil` if the frame is remote.
    public let origin: String?

    /// The child frames of the frame.
    ///
    /// Their order doesn't matter when comparing trees.
    public let children: [FrameTree]

    /// The identifier of the process the tree comes from, or `nil` for a tree that a test expects.
    ///
    /// Comparing trees ignores this.
    public let processIdentifier: pid_t?

    /// A remote frame without child frames.
    public static let remote = FrameTree.remote(children: [])

    private init(origin: String?, children: [FrameTree], processIdentifier: pid_t?) {
        self.origin = origin
        self.children = children
        self.processIdentifier = processIdentifier
    }

    @MainActor
    fileprivate init(_ node: _WKFrameTreeNode) {
        // _WKFrameTreeNode.h has no nullability annotations, but every node has frame info.
        let info: WKFrameInfo = node.info

        let origin: String?
        if info._isLocalFrame {
            let securityOrigin = info.securityOrigin
            let port = securityOrigin.port != 0 ? ":\(securityOrigin.port)" : ""
            origin = "\(securityOrigin.protocol)://\(securityOrigin.host)\(port)"
        } else {
            origin = nil
        }

        self.init(origin: origin, children: node.childFrames.map(FrameTree.init), processIdentifier: info._processIdentifier)
    }

    /// Creates a frame that is local to the process.
    ///
    /// - Parameters:
    ///   - origin: The origin of the frame, such as `https://example.com`.
    ///   - children: The child frames of the frame.
    /// - Returns: The frame.
    public static func local(_ origin: String, children: [FrameTree] = []) -> FrameTree {
        FrameTree(origin: origin, children: children, processIdentifier: nil)
    }

    /// Creates a frame that is remote in the process, because another process hosts its document.
    ///
    /// - Parameter children: The child frames of the frame.
    /// - Returns: The frame.
    public static func remote(children: [FrameTree]) -> FrameTree {
        FrameTree(origin: nil, children: children, processIdentifier: nil)
    }

    // Compare the children as multisets rather than sets: their order doesn't matter, but sibling frames are often
    // identical, such as the remote frames for two iframes from the same site.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func == (lhs: FrameTree, rhs: FrameTree) -> Bool {
        lhs.origin == rhs.origin && lhs.children.hasSameElementsInAnyOrder(as: rhs.children)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: String {
        var result = origin ?? "remote"
        if let processIdentifier {
            result += " (pid \(processIdentifier))"
        }
        if !children.isEmpty {
            result += " { \(children.map(\.description).joined(separator: ", ")) }"
        }
        return result
    }

    fileprivate func containsLocalFrame(withOrigin origin: String) -> Bool {
        self.origin == origin || children.contains { $0.containsLocalFrame(withOrigin: origin) }
    }
}

/// The frame trees of a page in each of its web processes.
///
/// Their order doesn't matter when comparing them, so a test can write them in any order.
public struct FrameTreesInProcesses: Sendable, Equatable, ExpressibleByArrayLiteral, CustomStringConvertible {
    /// The frame tree in each process.
    public let trees: [FrameTree]

    fileprivate init(_ trees: [FrameTree]) {
        self.trees = trees
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init(arrayLiteral trees: FrameTree...) {
        self.init(trees)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func == (lhs: FrameTreesInProcesses, rhs: FrameTreesInProcesses) -> Bool {
        lhs.trees.hasSameElementsInAnyOrder(as: rhs.trees)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: String {
        "[\(trees.map(\.description).joined(separator: "; "))]"
    }

    /// Returns the identifier of the process that hosts frames with an origin.
    ///
    /// - Parameter origin: An origin, such as `https://webkit.org`.
    /// - Returns: The identifier of the process, or `nil` if no process has a local frame with the origin.
    public func processIdentifier(hosting origin: String) -> pid_t? {
        trees.first { $0.containsLocalFrame(withOrigin: origin) }?.processIdentifier
    }
}

extension WKWebView {
    /// Gets the frame tree of the page in each of its web processes.
    ///
    /// - Returns: The frame trees, starting with the one from the process that hosts the main frame.
    public func frameTreesInProcesses() async -> FrameTreesInProcesses {
        let trees: Set<_WKFrameTreeNode>? = await _frameTrees()
        // Put the main frame's process first, so that the trees read like the expected trees in most tests.
        let mainFrameProcessFirst = (trees ?? []).map(FrameTree.init).sorted { $0.origin != nil && $1.origin == nil }
        return FrameTreesInProcesses(mainFrameProcessFirst)
    }
}

// MARK: Processes

/// Returns whether a process is running.
///
/// - Parameter processIdentifier: The identifier of the process, such as one from ``FrameTree/processIdentifier``.
/// - Returns: Whether the process is running.
public func isProcessRunning(_ processIdentifier: pid_t) -> Bool {
    kill(processIdentifier, 0) == 0
}

// MARK: Opened windows

/// A UI delegate that opens a site-isolated web view for each window a page opens, so that a test can wait for them in
/// order.
///
/// The recorder keeps the windows that open before a test waits for them. A web view only holds its UI delegate weakly,
/// so keep the recorder alive for as long as the web view can open windows.
@MainActor
public final class OpenedWebViewRecorder: NSObject, WKUIDelegate {
    private var openedWebViews: [(webView: TestWKWebView, navigationDelegate: TestNavigationDelegate)] = []

    /// Waits for the next window to open, if one hasn't already.
    ///
    /// - Parameter timeout: How long to wait for the window.
    /// - Returns: The web view of the window and its navigation delegate, which trusts any server certificate.
    /// - Throws: ``ConditionTimedOut`` if no window opens before the timeout.
    public func nextOpenedWebView(
        timeout: Duration = .seconds(10)
    ) async throws -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
        try await waitForCondition("a window to open", timeout: timeout) {
            !openedWebViews.isEmpty
        }
        return openedWebViews.removeFirst()
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func webView(
        _ webView: WKWebView,
        createWebViewWith configuration: WKWebViewConfiguration,
        for navigationAction: WKNavigationAction,
        windowFeatures: WKWindowFeatures
    ) -> WKWebView? {
        let openedWebView = siteIsolatedWebViewAndDelegate(configuration: configuration)
        openedWebViews.append(openedWebView)
        return openedWebView.webView
    }
}

// MARK: Alerts

/// A UI delegate that records the message of each JavaScript alert, so that a test can wait for them in order.
///
/// The recorder keeps alerts that arrive before a test waits for them, so a test can start a load or script that shows
/// an alert, and then wait for it. A web view only holds its UI delegate weakly, so keep the recorder alive for as long
/// as the web view can show alerts.
///
/// It can also present the dialogs of a `WebPage`.
@MainActor
public class AlertRecorder: NSObject, WKUIDelegate {
    private var messages: [String] = []

    /// Waits for the next alert, if it hasn't already been shown.
    ///
    /// - Parameter timeout: How long to wait for the alert.
    /// - Returns: The message of the alert.
    /// - Throws: ``ConditionTimedOut`` if no alert is shown before the timeout.
    public func nextAlert(timeout: Duration = .seconds(10)) async throws -> String {
        try await waitForCondition("a JavaScript alert", timeout: timeout) {
            !messages.isEmpty
        }
        return messages.removeFirst()
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func webView(
        _ webView: WKWebView,
        runJavaScriptAlertPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo
    ) async {
        messages.append(message)
    }
}

/// An ``AlertRecorder`` that also lets the page run beforeunload panels, and answers each one by leaving the page.
///
/// A frame only dispatches beforeunload if its page can run a beforeunload panel, which takes a UI delegate that
/// implements the panel method.
@MainActor
public final class BeforeUnloadAlertRecorder: AlertRecorder, WKUIDelegatePrivate {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation, NoLeadingUnderscores
    public func _webView(
        _ webView: WKWebView,
        runBeforeUnloadConfirmPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo
    ) async -> Bool {
        true
    }
}

#if ENABLE_SWIFTUI

extension AlertRecorder: WebPage.DialogPresenting {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func handleJavaScriptAlert(message: String, initiatedBy frame: WebPage.FrameInfo) async {
        messages.append(message)
    }
}

#endif // ENABLE_SWIFTUI
