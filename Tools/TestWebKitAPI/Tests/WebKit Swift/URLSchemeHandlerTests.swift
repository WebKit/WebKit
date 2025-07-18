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
import WebKit

struct TestURLSchemeHandler: URLSchemeHandler, Sendable {
    struct Failure: Error {
    }

    struct Delays: Hashable, Sendable {
        let beforeYield: Duration
        let beforeFinish: Duration
    }

    enum Execution: String, CaseIterable, Hashable, RawRepresentable, Sendable {
        case sync
        case async
    }

    init(
        data: Data,
        mimeType: String,
        delays: Delays = .init(beforeYield: .zero, beforeFinish: .zero),
        execution: Execution = .sync
    ) {
        self.data = data
        self.mimeType = mimeType
        self.delays = delays
        self.execution = execution

        (self.replyStream, self.replyContinuation) = AsyncStream.makeStream(of: URL.self)
    }

    let replyStream: AsyncStream<URL>

    private let data: Data
    private let mimeType: String
    private let replyContinuation: AsyncStream<URL>.Continuation
    private let delays: Delays
    private let execution: Execution

    func reply(for request: URLRequest) -> AsyncThrowingStream<URLSchemeTaskResult, any Error> {
        AsyncThrowingStream { continuation in
            guard let url = request.url else {
                fatalError()
            }

            guard url.absoluteString != "testing:image" else {
                continuation.finish(throwing: Failure())
                replyContinuation.yield(url)
                return
            }

            let yieldResponse = {
                let response = URLResponse(url: url, mimeType: mimeType, expectedContentLength: 2, textEncodingName: nil)
                continuation.yield(.response(response))
            }

            let yieldDataAndFinish = {
                continuation.yield(.data(data))

                continuation.finish()
                replyContinuation.yield(url)
            }

            switch execution {
            case .sync:
                sleep(UInt32(delays.beforeYield.components.seconds))
                yieldResponse()

                sleep(UInt32(delays.beforeYield.components.seconds))
                yieldDataAndFinish()

            case .async:
                Task {
                    try await Task.sleep(for: delays.beforeYield)
                    yieldResponse()

                    try await Task.sleep(for: delays.beforeFinish)
                    yieldDataAndFinish()
                }
            }
        }
    }
}

// MARK: Tests

@MainActor
struct URLSchemeHandlerTests {
    @Test
    func basicSchemeValidation() async throws {
        let customScheme = URLScheme("my-custom-scheme")
        #expect(customScheme != nil)

        let httpsScheme = URLScheme("https")
        #expect(httpsScheme == nil)

        let invalidScheme = URLScheme("invalid scheme")
        #expect(invalidScheme == nil)
    }

    @Test(
        .bug("https://bugs.webkit.org/show_bug.cgi?id=295741"),
        arguments: [
            TestURLSchemeHandler.Delays(beforeYield: .seconds(2), beforeFinish: .zero),
            TestURLSchemeHandler.Delays(beforeYield: .zero, beforeFinish: .seconds(2)),
        ],
        TestURLSchemeHandler.Execution.allCases
    )
    func navigatingToNewResourceWhileSchemeHandlerIsStillProcessingDoesNotFail(
        delays: TestURLSchemeHandler.Delays,
        execution: TestURLSchemeHandler.Execution
    ) async throws {
        let html = try #require("<html></html>".data(using: .utf8))

        let handler = TestURLSchemeHandler(data: html, mimeType: "text/html", delays: delays, execution: execution)

        var configuration = WebPage.Configuration()
        configuration.urlSchemeHandlers[URLScheme("testing")!] = handler

        let page = WebPage(configuration: configuration)

        var firstEvents: [WebPage.NavigationEvent] = []
        var secondEvents: [WebPage.NavigationEvent] = []

        do {
            for try await firstEvent in page.load(URL(string: "testing://main")) {
                firstEvents.append(firstEvent)

                if firstEvent == .startedProvisionalNavigation {
                    do {
                        for try await secondEvent in page.load(URL(string: "testing://main2")) {
                            secondEvents.append(secondEvent)
                        }
                    } catch {
                        Issue.record("Second navigation unexpectedly threw an error")
                    }
                }
            }
        } catch let error as WebPage.NavigationError {
            guard case .failedProvisionalNavigation = error else {
                Issue.record("Thrown error was expected to be failedProvisionalNavigation")
                return
            }
        }

        // In the failing case, the test will also crash and/or hang and not even reach this point.

        #expect(firstEvents == [.startedProvisionalNavigation])
        #expect(secondEvents == [.startedProvisionalNavigation, .committed, .finished])
    }

    @Test
    func basicSchemeHandling() async throws {
        let html = """
        <html>
        <img src='testing:image'>
        </html>
        """.data(using: .utf8)!

        let handler = TestURLSchemeHandler(data: html, mimeType: "text/html")
        var configuration = WebPage.Configuration()
        configuration.urlSchemeHandlers[URLScheme("testing")!] = handler

        let page = WebPage(configuration: configuration)

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
