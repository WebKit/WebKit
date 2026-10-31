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

#if ENABLE_IPC_TESTING_API

private import TestWebKitAPILibrary
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

private let findSecretsFunction = """
    function findSecrets(buffer, secrets) {
        const bytes = new Uint8Array(buffer);
        const texts = [
            new TextDecoder('latin1').decode(bytes),
            new TextDecoder('utf-16le').decode(bytes),
            new TextDecoder('utf-16le').decode(bytes.subarray(1)),
        ];
        return secrets.filter(secret => texts.some(text => text.includes(secret)));
    }
    """

@MainActor
struct SiteIsolationBackForwardFrameStateTests {
    enum BackForwardMessage: CaseIterable {
        case allItems
        case itemAtIndex

        var name: String {
            switch self {
            case .allItems: "WebBackForwardList_BackForwardAllItems"
            case .itemAtIndex: "WebBackForwardList_BackForwardItemAtIndexForWebContent"
            }
        }

        var arguments: String {
            switch self {
            case .allItems: "[frameID]"
            case .itemAtIndex: "[{ type: 'int32_t', value: 0 }, frameID]"
            }
        }
    }

    enum LocationNavigation: CaseIterable {
        case assign
        case replace

        var script: String {
            switch self {
            case .assign: "location.href = 'https://webkit.org/destination'"
            case .replace: "location.replace('https://webkit.org/destination')"
            }
        }

        var expectedSecrets: [String] {
            switch self {
            case .assign: ["webkit-iframe-secret"]
            case .replace: []
            }
        }
    }

    @Test(arguments: BackForwardMessage.allCases)
    func backForwardReplyOnlyIncludesFramesHostedByRequestingProcess(message: BackForwardMessage) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main-frame-secret") {
                "<iframe src='https://webkit.org/iframe-secret'></iframe>"
            }

            Route("/iframe-secret") {
                "<iframe src='https://example.com/grandchild-secret'></iframe>"
            }

            Route("/grandchild-secret") {
                "grandchild"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            try configuration.setFeature("IPCTestingAPIEnabled", enabled: true)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main-frame-secret"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote(children: [.local("https://example.com")])]),
                    .remote(children: [.local("https://webkit.org", children: [.remote])]),
                ]
            )

            let iframe = try #require(await webView.firstChildFrame)
            let mainFrameID = try await ipcFrameID(of: nil, in: webView)
            let iframeID = try await ipcFrameID(of: iframe, in: webView)

            #expect(
                try await secrets(in: message, namingFrameWithID: mainFrameID, from: iframe, in: webView)
                    == ["iframe-secret"]
            )
            #expect(
                try await secrets(in: message, namingFrameWithID: mainFrameID, from: nil, in: webView)
                    == ["main-frame-secret", "grandchild-secret"]
            )
            #expect(
                try await secrets(in: message, namingFrameWithID: iframeID, from: nil, in: webView)
                    == ["grandchild-secret"]
            )
        }
    }

    @Test(arguments: LocationNavigation.allCases)
    func reattachedHistoryItemOnlyIncludesFramesHostedByNewProcess(navigation: LocationNavigation) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/opener") {
                """
                <iframe src='https://webkit.org/webkit-iframe-secret'></iframe>
                <iframe src='https://apple.com/apple-iframe-secret'></iframe>
                """
            }

            Route("/webkit-iframe-secret") {
                "webkit"
            }

            Route("/apple-iframe-secret") {
                "apple"
            }

            Route("/opened") {
                "opened"
            }

            Route("/destination") {
                "destination"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            try configuration.setFeature("IPCTestingAPIEnabled", enabled: true)
            // With the shared process, the iframes and the opened window would all end up in one process.
            try configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: false)
            configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
            let (opener, openerNavigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            let openedWebViewRecorder = OpenedWebViewRecorder()
            defer { withExtendedLifetime(openedWebViewRecorder) {} }
            opener.uiDelegate = openedWebViewRecorder

            opener.load(URLRequest(url: try #require(URL(string: "https://example.com/opener"))))
            try await openerNavigationDelegate.waitForDidFinishNavigation()
            #expect(
                await opener.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote, .remote]),
                    .remote(children: [.local("https://webkit.org"), .remote]),
                    .remote(children: [.remote, .local("https://apple.com")]),
                ]
            )

            try await opener.callJavaScript { "window.open('https://webkit.org/opened')" }
            let (opened, openedNavigationDelegate) = try await openedWebViewRecorder.nextOpenedWebView()
            try await openedNavigationDelegate.waitForDidFinishNavigation()
            let webKitProcess = try #require(
                await opener.frameTreesInProcesses().processIdentifier(hosting: "https://webkit.org")
            )
            #expect(await opened.frameTreesInProcesses().processIdentifier(hosting: "https://webkit.org") == webKitProcess)

            try await opened.callJavaScript(arguments: ["secrets": ["webkit-iframe-secret", "apple-iframe-secret"]]) {
                """
                \(findSecretsFunction)
                window.reattachSecrets = [];
                IPC.addIncomingMessageListener('UI', message => {
                    if (message.name == IPC.messages.WebPage_SetCurrentHistoryItemForReattach.name)
                        window.reattachSecrets.push(findSecrets(message.buffer, secrets));
                });
                """
            }

            try await opener.evaluateJavaScriptWithoutUserGesture(navigation.script)
            try await openerNavigationDelegate.waitForDidFinishNavigation()
            #expect(await opener.frameTreesInProcesses().processIdentifier(hosting: "https://webkit.org") == webKitProcess)

            let reattachSecrets = try await opened.callJavaScript(returning: [[String]].self) {
                "return window.reattachSecrets"
            }
            #expect(reattachSecrets.last == navigation.expectedSecrets)
        }
    }

    private func ipcFrameID(of frame: WKFrameInfo?, in webView: WKWebView) async throws -> String {
        try await webView.callJavaScript(returning: String.self, in: frame) { "return String(IPC.frameID)" }
    }

    private func secrets(
        in message: BackForwardMessage,
        namingFrameWithID requestedFrameID: String,
        from frame: WKFrameInfo?,
        in webView: WKWebView
    ) async throws -> [String] {
        try await webView.callJavaScript(
            returning: [String].self,
            in: frame,
            arguments: [
                "requestedFrameID": requestedFrameID,
                "secrets": ["main-frame-secret", "iframe-secret", "grandchild-secret"],
            ]
        ) {
            """
            \(findSecretsFunction)
            const frameID = { type: 'FrameID', value: [BigInt(requestedFrameID)] };
            const reply = IPC.sendSyncMessage('UI', IPC.pageID, IPC.messages.\(message.name).name, 1000, \(message.arguments));
            return findSecrets(reply.buffer, secrets);
            """
        }
    }
}

#endif // ENABLE_IPC_TESTING_API
