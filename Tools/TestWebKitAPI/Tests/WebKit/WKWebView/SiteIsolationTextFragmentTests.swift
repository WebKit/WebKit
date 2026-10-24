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

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationTextFragmentTests {
    @Test(.timeLimit(.minutes(1)))
    func textFragmentDirectiveFromSelectionInFocusedCrossOriginIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                WebViewWithFocusedCrossOriginIframe.mainFrameHTML
            }

            Route("/iframe") {
                "<body>subframe text</body>"
            }
        }

        try await server.run { serverConfiguration in
            let page = try await WebViewWithFocusedCrossOriginIframe(configuration: .init(httpsProxyFor: serverConfiguration))
            let webView = page.webView
            let childFrame = page.childFrame
            try await webView.setSelection(in: childFrame, script: "getSelection().selectAllChildren(document.body)", expecting: .isRange)

            let url: URL? = await webView._textFragmentDirectiveFromSelection()
            let urlString = url?.absoluteString ?? ""
            #expect(urlString.hasPrefix("https://webkit.org/iframe#:~:text="))
            #expect(urlString.contains("subframe"))
        }
    }
}
