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

import Observation
import Testing
@_spi(Private) @_spi(Testing) import WebKit

extension WebPage_v0.NavigationEvent.Kind: @retroactive Equatable {
    public static func == (lhs: Self, rhs: Self) -> Bool {
        switch (lhs, rhs) {
        case (.startedProvisionalNavigation, .startedProvisionalNavigation):
            true
        case (.receivedServerRedirect, .receivedServerRedirect):
            true
        case (.committed, .committed):
            true
        case (.finished, .finished):
            true
        case (.failedProvisionalNavigation(_), .failedProvisionalNavigation(_)):
            true
        case (.failed(_), .failed(_)):
            true
        default:
            false
        }
    }
}

extension WebPage_v0.NavigationEvent: @retroactive Equatable {
    public static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.kind == rhs.kind && lhs.navigationID == rhs.navigationID
    }
}

// MARK: Supporting test types

@MainActor
fileprivate class TestNavigationDecider: NavigationDeciding {
    init() {
        (self.navigationActionStream, self.navigationActionContinuation) = AsyncStream.makeStream(of: WebPage_v0.NavigationAction.self)
        (self.navigationResponseStream, self.navigationResponseContinuation) = AsyncStream.makeStream(of: WebPage_v0.NavigationResponse.self)
    }

    let navigationActionStream: AsyncStream<WebPage_v0.NavigationAction>
    private let navigationActionContinuation: AsyncStream<WebPage_v0.NavigationAction>.Continuation

    let navigationResponseStream: AsyncStream<WebPage_v0.NavigationResponse>
    private let navigationResponseContinuation: AsyncStream<WebPage_v0.NavigationResponse>.Continuation

    var preferencesMutation: (inout WebPage_v0.NavigationPreferences) -> Void = { _ in }

    func decidePolicy(for action: WebPage_v0.NavigationAction, preferences: inout WebPage_v0.NavigationPreferences) async -> WKNavigationActionPolicy {
        preferencesMutation(&preferences)

        navigationActionContinuation.yield(action)
        return .allow
    }

    func decidePolicy(for response: WebPage_v0.NavigationResponse) async -> WKNavigationResponsePolicy {
        navigationResponseContinuation.yield(response)
        return .allow
    }
}

// MARK: Tests

@MainActor
struct WebPageTests {
    @Test
    func basicNavigation() async throws {
        let page = WebPage_v0()

        let html = """
        <html>
        <div>Hello</div>
        </html>
        """

        let expectedEventKinds: [WebPage_v0.NavigationEvent.Kind] = [.startedProvisionalNavigation, .committed, .finished]

        // The existence of the loop is to test and ensure navigations are idempotent.
        for _ in 0..<2 {
            page.load(htmlString: html, baseURL: .aboutBlank)

            var actualEventKinds: [WebPage_v0.NavigationEvent.Kind] = []

            loop:
            for try await event in page.navigations {
                actualEventKinds.append(event.kind)

                switch event.kind {
                case .startedProvisionalNavigation, .receivedServerRedirect, .committed:
                    break

                case .finished, .failedProvisionalNavigation(_), .failed(_):
                    break loop

                @unknown default:
                    fatalError()
                }
            }

            #expect(actualEventKinds == expectedEventKinds)
        }
    }

    @Test
    func sequenceOfNavigations() async throws {
        let page = WebPage_v0()

        let html = """
        <html>
        <div>Hello</div>
        </html>
        """

        page.load(htmlString: html, baseURL: .aboutBlank)
        let navigationIDB = page.load(htmlString: html, baseURL: .aboutBlank)!

        let expectedEvents: [WebPage_v0.NavigationEvent] = [
            .init(kind: .startedProvisionalNavigation, navigationID: navigationIDB),
            .init(kind: .committed, navigationID: navigationIDB),
            .init(kind: .finished, navigationID: navigationIDB),
        ]

        var actualNavigations: [WebPage_v0.NavigationEvent] = []

        loop:
        for try await event in page.navigations {
            actualNavigations.append(event)

            if event.navigationID != navigationIDB {
                continue
            }

            switch event.kind {
            case .startedProvisionalNavigation, .receivedServerRedirect, .committed:
                continue

            case .finished, .failedProvisionalNavigation(_), .failed(_):
                break loop

            @unknown default:
                fatalError()
            }
        }

        #expect(actualNavigations == expectedEvents)
    }

    @Test
    func navigationWithFailedProvisionalNavigationEvent() async throws {
        let page = WebPage_v0()

        let request = URLRequest(url: URL(string: "about:foo")!)
        page.load(request)

        let expectedEventKinds: [WebPage_v0.NavigationEvent.Kind] = [.startedProvisionalNavigation, .failedProvisionalNavigation(underlyingError: WKError(.unknown))]

        var actualEventKinds: [WebPage_v0.NavigationEvent.Kind] = []

        loop:
        for await event in page.navigations {
            actualEventKinds.append(event.kind)

            switch event.kind {
            case .startedProvisionalNavigation, .receivedServerRedirect, .committed:
                break

            case .finished, .failedProvisionalNavigation(_), .failed(_):
                break loop

            @unknown default:
                fatalError()
            }
        }

        #expect(actualEventKinds == expectedEventKinds)
    }

    @Test
    func observableProperties() async throws {
        let page = WebPage_v0()

        let html = """
        <html>
        <head>
            <title>Title</title>
        </head>
        <body></body>
        </html>
        """

        #expect(page.url == nil)
        #expect(page.title == "")
        #expect(!page.isLoading)
        #expect(page.estimatedProgress == 0.0)
        #expect(page.serverTrust == nil)
        #expect(!page.hasOnlySecureContent)
        #expect(page.themeColor == nil)

        // FIXME: (283456) Make this test more comprehensive once Observation supports observing a stream of changes to properties.
    }

    @Test
    func decidePolicyForNavigationActionFragment() async throws {
        let decider = TestNavigationDecider()
        let page = WebPage_v0(navigationDecider: decider)

        let html = "<script>window.location.href='#fragment';</script>"

        let baseURL = URL(string: "http://webkit.org")!
        page.load(htmlString: html, baseURL: baseURL)

        let actions = await Array(decider.navigationActionStream.prefix(2))

        #expect(actions[0].request.url!.absoluteString == "http://webkit.org/")
        #expect(actions[1].request.url!.absoluteString == "http://webkit.org/#fragment")
    }

    @Test
    func navigationPreferencesMutationDuringNavigation() async throws {
        let decider = TestNavigationDecider()
        let page = WebPage_v0(navigationDecider: decider)

        let html = "<script>var foo = 'bar'</script>"

        decider.preferencesMutation = {
            $0.allowsContentJavaScript = false
        }

        let id = page.load(htmlString: html, baseURL: .aboutBlank)!

        let actions = await Array(decider.navigationActionStream.prefix(1))
        #expect(actions[0].request.url == .aboutBlank)

        for await event in page.navigations where event.navigationID == id {
            if case .finished = event.kind {
                break
            }
        }

        do {
            _ = try await page.callAsyncJavaScript("return foo;", contentWorld: .page)
            Issue.record()
        } catch {
            #expect(error.localizedDescription == "A JavaScript exception occurred")
        }
    }

    @Test
    func javaScriptEvaluation() async throws {
        let page = WebPage_v0()

        let arguments = [
            "a": 1,
            "b": 2,
        ]

        let result = try await page.callAsyncJavaScript("return a + b;", arguments: arguments) as! Int
        #expect(result == 3)

        let nilResult = try await page.callAsyncJavaScript("console.log('hi')")
        #expect(nilResult == nil)
    }

    @Test
    func decidePolicyForNavigationResponse() async throws {
        let decider = TestNavigationDecider()
        let page = WebPage_v0(navigationDecider: decider)

        let simpleURL = Bundle.testResources.url(forResource: "simple", withExtension: "html")!
        let request = URLRequest(url: simpleURL)

        page.load(request)

        let responses = await Array(decider.navigationResponseStream.prefix(1))

        #expect(responses[0].response.url!.absoluteString == simpleURL.absoluteString)
    }
}

#endif
