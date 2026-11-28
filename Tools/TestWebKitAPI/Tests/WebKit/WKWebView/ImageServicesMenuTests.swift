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

#if ENABLE_SERVICE_CONTROLS

import AppKit
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import TestWebKitAPILibrary.Helpers.cocoa.WKWebViewConfigurationExtras
import Testing
import UniformTypeIdentifiers
import WebKit
private import WebKit_Private.WKUIDelegatePrivate
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebViewPrivateForTestingMac
private import WebKit_Private._WKAttachment

import struct Swift.String

private let markup = """
    <body contenteditable>
    <p>Replace me</p>
    <img src="icon.png">
    <script>
    async function waitForAndClickImageControls() {
        let button = null;
        do {
            await new Promise(requestAnimationFrame);
            button = internals.shadowRoot(document.querySelector("img")).getElementById("image-controls-button");
        } while (!button);
        button.click();
    }
    </script>
    </body>
    """

@MainActor
private final class AttachmentInvalidationRecorder: NSObject, WKUIDelegatePrivate {
    private(set) var invalidatedAttachmentIdentifiers: [String] = []

    // swift-format-ignore: NoLeadingUnderscores
    @objc(_webView:didInvalidateDataForAttachment:)
    func _webView(_ webView: WKWebView, didInvalidateDataFor attachment: _WKAttachment) {
        invalidatedAttachmentIdentifiers.append(attachment.uniqueIdentifier)
    }
}

// Each test tracks a pop-up menu in a nested run loop, so they must not run concurrently with each other.
@MainActor
@Suite(.serialized)
struct ImageServicesMenuTests {
    private let webView: TestWKWebView

    private let attachmentIdentifier: String

    init() async throws {
        let configuration = WKWebViewConfiguration._test_configurationWithTestPlugInClassName(
            "WebProcessPlugInWithInternals",
            configureJSCForTesting: true
        )
        configuration._attachmentElementEnabled = true
        configuration._imageControlsEnabled = true

        webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 300, height: 300), configuration: configuration)
        webView.hostWindow()?.orderFrontRegardless()

        try await webView.load(html: markup)

        // Giving the image an attachment identifier is what shows its image controls.
        attachmentIdentifier = try await webView.callJavaScript(returning: String.self) {
            "return HTMLAttachmentElement.getAttachmentIdentifier(document.querySelector('img'))"
        }
    }

    // Opens the image's services menu, and shares the items as a sharing service that edits them would.
    private func shareFromServicesMenu(_ items: [Any]) async throws {
        webView.startEvaluatingJavaScript("waitForAndClickImageControls()")

        // The menu tracks in a nested run loop, which still runs main actor work. Building it enumerates sharing
        // extensions, which can be slow.
        try await waitForCondition("the services menu to appear", timeout: .seconds(30)) {
            webView._activeMenu != nil
        }

        let menu = try #require(webView._activeMenu)
        defer {
            menu.cancelTracking()
        }

        let service = NSSharingService(title: "Edit", image: NSImage(), alternateImage: nil) {}
        try sharingServiceDelegate().sharingService?(service, didShareItems: items)
    }

    private func sharingServiceDelegate() throws -> any NSSharingServiceDelegate {
        // WebKit doesn't export the class, so look it up at runtime.
        let delegateClass: AnyClass = try #require(NSClassFromString("WKSharingServicePickerDelegate"))
        let delegate = (delegateClass as AnyObject).value(forKey: "sharedSharingServicePickerDelegate")
        return try #require(delegate as? any NSSharingServiceDelegate)
    }

    private func bodyText() async throws -> String {
        try await webView.callJavaScript(returning: String.self) { "return document.body.innerText" }
    }

    @Test
    func sharingAttributedStringReplacesSelection() async throws {
        try await webView.callJavaScript { "getSelection().selectAllChildren(document.querySelector('p'))" }

        try await shareFromServicesMenu([NSAttributedString(string: "Shared")])

        try await waitForCondition("the selection to be replaced") {
            try await bodyText().contains("Shared")
        }
        #expect(try await !bodyText().contains("Replace me"))
    }

    @Test
    func sharingImageDataReplacesSelection() async throws {
        try await webView.callJavaScript { "getSelection().selectAllChildren(document.querySelector('p'))" }

        let url = try #require(Bundle.testResources.url(forResource: "large-red-square", withExtension: "png"))
        try await shareFromServicesMenu([try Data(contentsOf: url)])

        try await waitForCondition("the selection to be replaced with an image") {
            try await webView.callJavaScript(returning: Int.self) { "return document.images.length" } == 2
        }
        #expect(try await !bodyText().contains("Replace me"))
    }

    @Test
    func sharingItemProviderReplacesImageAttachmentData() async throws {
        let recorder = AttachmentInvalidationRecorder()
        webView.uiDelegate = recorder

        let url = try #require(Bundle.testResources.url(forResource: "large-red-square", withExtension: "png"))
        let data = try Data(contentsOf: url)

        let itemProvider = NSItemProvider()
        itemProvider.registerDataRepresentation(for: .png) { completionHandler in
            completionHandler(data, nil)
            return nil
        }

        try await shareFromServicesMenu([itemProvider])

        try await waitForCondition("the attachment's data to be replaced") {
            webView._attachment(forIdentifier: attachmentIdentifier)?.info?.data == data
        }
        #expect(recorder.invalidatedAttachmentIdentifiers.contains(attachmentIdentifier))
    }
}

#endif // ENABLE_SERVICE_CONTROLS
