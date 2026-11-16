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

import struct Swift.String

// Editing commands act on the focused frame's selection, so they must be sent to the process containing the focused
// frame. These tests put the selection in a cross-origin iframe and check that each command takes effect there. If the
// command is sent to the main frame's process instead, it finds the main frame's empty selection and does nothing.

@MainActor
struct SiteIsolationEditingTests {
    @Test
    func listCommandsInCrossOriginIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                WebViewWithFocusedCrossOriginIframe.mainFrameHTML
            }

            Route("/iframe") {
                "<body contenteditable><ul><li>One</li><li id='item'>Two</li></ul></body>"
            }
        }

        try await server.run { serverConfiguration in
            let page = try await WebViewWithFocusedCrossOriginIframe(configuration: .init(httpsProxyFor: serverConfiguration))
            let webView = page.webView
            let childFrame = page.childFrame
            try await webView.setSelection(in: childFrame, script: "getSelection().setPosition(item.firstChild, 1)", expecting: .isCaret)

            let listDepth = {
                try await webView.callJavaScript(returning: Int.self, in: childFrame) {
                    """
                    let depth = 0;
                    for (let element = item.parentElement; element; element = element.parentElement) {
                        if (element.matches('ol, ul'))
                            ++depth;
                    }
                    return depth;
                    """
                }
            }
            #expect(try await listDepth() == 1)

            webView._increaseListLevel(nil)
            try await waitForCondition("the list item to be nested in another list") {
                try await listDepth() == 2
            }

            webView._decreaseListLevel(nil)
            try await waitForCondition("the list item to be back in a single list") {
                try await listDepth() == 1
            }

            webView._changeListType(nil)
            try await waitForCondition("the list to become an ordered list") {
                try await webView.callJavaScript(returning: String.self, in: childFrame) { "return item.closest('ol, ul').tagName" } == "OL"
            }
        }
    }
}
