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

#if ENABLE_SWIFTUI

import Testing
@_spi(Testing) import WebKit
private import TestWebKitAPILibrary
import struct Swift.String
import struct Foundation.URL

#if compiler(>=6.4)

@MainActor
struct UserContentControllerTests {
    @Test
    func jsBufferInjectsWebKitNamespace() async throws {
        let buffer = "abc".utf8Span.span.bytes
        let configuration = WebPage.Configuration()
        configuration.userContentController.addBuffer(buffer, name: "testBuffer", to: .page)
        defer {
            configuration.userContentController.removeBuffer(named: "testBuffer", from: .page)
        }

        let page = WebPage(configuration: configuration)
        try await page.load(html: "<body>test</body>").wait()

        let bufferString = try await page.callJavaScript(returning: String.self) {
            """
            return window.webkit.buffers.testBuffer.asLatin1String();
            """
        }
        #expect(bufferString == "abc")

        // The other WebKitNamespace attributes shouldn't be accessible.

        let evaluateScript: Void? = try await page.callJavaScript(returning: Void?.self) {
            """
            return window.webkit.evaluateScript;
            """
        }
        #expect(evaluateScript == nil)

        let createJSHandle: Void? = try await page.callJavaScript(returning: Void?.self) {
            """
            return window.webkit.createJSHandle;
            """
        }
        #expect(createJSHandle == nil)

        let serializeNode: Void? = try await page.callJavaScript(returning: Void?.self) {
            """
            return window.webkit.serializeNode;
            """
        }
        #expect(serializeNode == nil)
    }
}

#endif // compiler(>=6.4)

#endif // ENABLE_SWIFTUI
