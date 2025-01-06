// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.0)

import Testing
@_spi(Private) import WebKit

fileprivate struct TestURLSchemeHandler: URLSchemeHandler_v0, Sendable {
    struct Failure: Error {
    }

    init(data: Data, mimeType: String) {
        self.data = data
        self.mimeType = mimeType

        (self.replyStream, self.replyContinuation) = AsyncStream.makeStream(of: URL.self)
    }

    let replyStream: AsyncStream<URL>

    private let data: Data
    private let mimeType: String
    private let replyContinuation: AsyncStream<URL>.Continuation

    func reply(for request: URLRequest) -> AsyncThrowingStream<URLSchemeTaskResult_v0, any Error> {
        AsyncThrowingStream { continuation in
            defer {
                replyContinuation.yield(request.url!)
            }

            guard request.url!.absoluteString != "testing:image" else {
                continuation.finish(throwing: Failure())
                return
            }

            let response = URLResponse(url: request.url!, mimeType: mimeType, expectedContentLength: 2, textEncodingName: nil)
            continuation.yield(.response(response))
            continuation.yield(.data(data))

            continuation.finish()
        }
    }
}

// MARK: Tests

@MainActor
struct URLSchemeHandlerTests {
    @Test
    func basicSchemeValidation() async throws {
        let customScheme = URLScheme_v0("my-custom-scheme")
        #expect(customScheme != nil)

        let httpsScheme = URLScheme_v0("https")
        #expect(httpsScheme == nil)

        let invalidScheme = URLScheme_v0("invalid scheme")
        #expect(invalidScheme == nil)
    }

    @Test
    func basicSchemeHandling() async throws {
        let html = """
        <html>
        <img src='testing:image'>
        </html>
        """.data(using: .utf8)!

        let handler = TestURLSchemeHandler(data: html, mimeType: "text/html")
        var configuration = WebPage_v0.Configuration()
        configuration.urlSchemeHandlers[URLScheme_v0("testing")!] = handler

        let page = WebPage_v0(configuration: configuration)

        let url = URL(string: "testing:main")!
        let request = URLRequest(url: url)

        async let replyStream = Array(handler.replyStream.prefix(2))

        page.load(request)

        let expectedReplyURLs = [
            URL(string: "testing:main")!,
            URL(string: "testing:image")!,
        ]

        let actualReplyURLs = await replyStream
        #expect(actualReplyURLs == expectedReplyURLs)
    }
}

#endif
