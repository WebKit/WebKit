// Copyright (C) 2025 Apple Inc. All rights reserved.
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

// FIXME: Remove !SWIFT_WEBKIT_TOOLCHAIN once Swift toolchain is fixed (see webkit.org/b/307344).
#if ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.2) && !SWIFT_WEBKIT_TOOLCHAIN

import Testing
@_spi(Testing) import WebKit

private struct NeverLoadingSchemeHandler: URLSchemeHandler {
    // This force unwrap is safe because the scheme is a static String.
    // swift-format-ignore: NeverForceUnwrap
    @MainActor
    static let scheme = URLScheme("never-loading")!

    nonisolated func reply(for request: URLRequest) -> some AsyncSequence<URLSchemeTaskResult, any Error> {
        AsyncThrowingStream { _ in }
    }
}

@MainActor
struct WebPageNavigationTests {
    @Test
    func basicNavigationProducesExpectedNavigationEvents() async throws {
        let page = WebPage()

        let html = "<html><div>Hello</div></html>"
        let sequence = page.load(html: html)

        let expected: [WebPage.NavigationEvent] = [.startedProvisionalNavigation, .committed, .finished]
        let actual = try await Array(sequence)

        #expect(actual == expected)
    }

    @Test
    func failedNavigationProducesExpectedNavigationError() async throws {
        let page = WebPage()

        let sequence = page.load(URL(string: "about:foo"))

        var actual: [WebPage.NavigationEvent] = []
        let expected: [WebPage.NavigationEvent] = [.startedProvisionalNavigation]

        await #expect(throws: (any Error).self) {
            // Safety: this is actually safe; false positive is rdar://154775389
            for try await unsafe event in sequence {
                actual.append(event)
            }
        }

        #expect(actual == expected)
    }

    @Test
    func explicitlyStopLoadingProgrammaticNavigation() async throws {
        var configuration = WebPage.Configuration()
        configuration.urlSchemeHandlers[NeverLoadingSchemeHandler.scheme] = NeverLoadingSchemeHandler()

        let page = WebPage(configuration: configuration)
        let sequence = page.load(URL(string: "never-loading:///index.html"))

        // FIXME: `#expect` should work here, but due to a Swift Testing issue causes the test to hang.
        do {
            // Safety: this is actually safe; false positive is rdar://154775389
            for try await unsafe event in sequence where event == .startedProvisionalNavigation {
                page.stopLoading()
            }
            Issue.record("Stopping page load should trigger an error and therefore the loop should never finish.")
        } catch {
            #expect(error is WebPage.NavigationError)
        }
    }

    @Test
    func stopLoadingProgrammaticNavigationViaTaskCancellation() async throws {
        var configuration = WebPage.Configuration()
        configuration.urlSchemeHandlers[NeverLoadingSchemeHandler.scheme] = NeverLoadingSchemeHandler()
        let page = WebPage(configuration: configuration)

        let allNavigations = page.navigations
        let sequence = page.load(URL(string: "never-loading:///index.html"))

        var task: Task<Void, any Error>? = nil

        await withCheckedContinuation { continuation in
            task = Task {
                // Safety: this is actually safe; false positive is rdar://154775389
                for try await unsafe event in sequence {
                    if event == .startedProvisionalNavigation {
                        continuation.resume()
                    } else {
                        Issue.record("No other event should occur since the load is indefinite.")
                    }
                }
            }
        }

        try #require(task).cancel()

        let expectedEvents: [WebPage.NavigationEvent] = [.startedProvisionalNavigation]
        var actualEvents: [WebPage.NavigationEvent] = []

        // FIXME: `#expect` should work here, but due to a Swift Testing issue causes the test to hang.
        do {
            // Safety: this is actually safe; false positive is rdar://154775389
            for try await unsafe event in allNavigations {
                actualEvents.append(event)
            }
            Issue.record("The stream is indefinite and therefore should never reach here.")
        } catch {
            #expect(error is WebPage.NavigationError)
        }

        #expect(actualEvents == expectedEvents)
    }

    @Test
    func failedNavigationWithWebContentProcessTerminated() async throws {
        var configuration = WebPage.Configuration()
        configuration.urlSchemeHandlers[NeverLoadingSchemeHandler.scheme] = NeverLoadingSchemeHandler()

        let page = WebPage(configuration: configuration)
        let sequence = page.load(URL(string: "never-loading:///index.html"))

        // FIXME: `#expect` should work here, but a Swift Testing issue causes the test to hang.
        do {
            // Safety: this is actually safe; false positive is rdar://154775389
            for try await unsafe event in sequence where event == .startedProvisionalNavigation {
                page.terminateWebContentProcess()
            }
            Issue.record("Terminating the web content process should trigger an error and therefore the loop should never finish.")
        } catch {
            #expect(error is WebPage.NavigationError)
        }
    }

    @Test
    func navigationProceedsAfterDiscardingNavigationStream() async throws {
        let page = WebPage()

        let html = "<title>A title</title>"
        page.load(html: html)

        // A timeout is used since observing the navigation sequence itself alters the outcome of this test.
        try await Task.sleep(for: .seconds(10))

        #expect(page.title == "A title")
    }
}

#endif // ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.2)
