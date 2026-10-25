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

#if ENABLE_SWIFTUI && WTF_PLATFORM_MAC

import AppKit
import SwiftUI
import Testing
@_spi(CrossImportOverlay) import WebKit
import WebKit_Private.WKPreferencesPrivate
import WebKit_Private.WKWebViewPrivate
import WebKit_Private._WKInspector
import WebKit_Private._WKInspectorPrivateForTesting
private import TestWebKitAPILibrary

private struct WebViewRepresentable: NSViewRepresentable {
    let webView: WKWebView

    func makeNSView(context: Context) -> WKWebView {
        webView
    }

    func updateNSView(_ nsView: WKWebView, context: Context) {
    }
}

@MainActor
struct WebViewInspectorAttachmentTests {
    private static func makeWindow(rootView: some View) -> NSWindow {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1000, height: 700),
            styleMask: [.titled, .resizable],
            backing: .buffered,
            defer: false
        )
        window.isReleasedWhenClosed = false
        window.contentView = NSHostingView(rootView: rootView)
        window.layoutIfNeeded()
        return window
    }

    private static func attachInspector(to webView: WKWebView, in window: NSWindow) async throws {
        webView.configuration.preferences._developerExtrasEnabled = true
        let inspector = try #require(webView._inspector)
        inspector.show()
        for _ in 0..<100 where !inspector.isVisible {
            try await Task.sleep(for: .milliseconds(100))
        }
        #expect(inspector.isVisible)
        inspector.attach()
        window.layoutIfNeeded()
    }

    private static func expectInspectorAttached(nextTo webView: WKWebView, in window: NSWindow) throws {
        let inspectorWebView = try #require(webView._inspector?.inspectorWebView())
        let contentView = try #require(window.contentView)
        #expect(inspectorWebView.isDescendant(of: contentView))

        let inspectedFrame = webView.convert(webView.bounds, to: nil)
        let inspectorFrame = inspectorWebView.convert(inspectorWebView.bounds, to: nil)
        #expect(inspectedFrame.height > 0)
        #expect(inspectorFrame.height > 0)
        #expect(inspectedFrame.intersection(inspectorFrame).isEmpty)
    }

    // Detaching first keeps the inspector from opening attached in later tests.
    private static func detachAndCloseInspector(_ webView: WKWebView) {
        webView._inspector?.detach()
        webView._inspector?.close()
    }

    private static func exerciseAttachment(webView: WKWebView, window: NSWindow) async throws {
        let originalSize = webView.frame.size
        #expect(originalSize.width > 0 && originalSize.height > 0)

        try await attachInspector(to: webView, in: window)
        try expectInspectorAttached(nextTo: webView, in: window)

        window.setContentSize(NSSize(width: 1100, height: 750))
        try await Task.sleep(for: .milliseconds(100))
        window.layoutIfNeeded()
        try expectInspectorAttached(nextTo: webView, in: window)

        detachAndCloseInspector(webView)
        window.setContentSize(NSSize(width: 1000, height: 700))
        window.layoutIfNeeded()
        #expect(webView.frame.size == originalSize)

        window.close()
    }

    @Test
    func webView() async throws {
        let page = WebPage()
        let window = Self.makeWindow(rootView: WebView(page))
        try await page.load(html: "<body>Inspected page</body>").wait()

        try await Self.exerciseAttachment(webView: page.backingWebView, window: window)
    }

    @Test
    func viewRepresentable() async throws {
        let webView = WKWebView(frame: .zero, configuration: WKWebViewConfiguration())
        let window = Self.makeWindow(rootView: WebViewRepresentable(webView: webView))
        webView.loadHTMLString("<body>Inspected page</body>", baseURL: nil)

        try await Self.exerciseAttachment(webView: webView, window: window)
    }
}

#endif // ENABLE_SWIFTUI && WTF_PLATFORM_MAC
