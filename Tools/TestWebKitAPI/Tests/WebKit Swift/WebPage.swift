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
import WebKit
@_spi(Private) import WebKit
@_spi(Testing) import WebKit

// MARK: Helper extension functions

extension Array {
    @MainActor
    fileprivate init(async sequence: some AsyncSequence<Element, Never>) async {
        self.init()

        for try await element in sequence {
            append(element)
        }
    }
}

extension URL {
    fileprivate static var aboutBlank: URL {
        URL(string: "about:blank")!
    }
}

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
}

#endif
