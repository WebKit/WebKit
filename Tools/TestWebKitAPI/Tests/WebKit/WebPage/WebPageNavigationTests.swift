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

#if ENABLE_SWIFTUI

import Testing
private import Observation
@_spi(Testing) import WebKit
private import TestWebKitAPILibrary

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
    func secureCodingExemptClassesAppliedToAuxiliaryProcess() async throws {
        UserDefaults.standard.register(defaults: [
            "WebKitCrashOnSecureCodingWithExemptClassesKey": ["NSURLRequest", "NSError"]
        ])

        let page = WebPage()
        try await page.load(html: "<body></body>").wait()
    }

    @Test
    func failedNavigationProducesExpectedNavigationError() async throws {
        let page = WebPage()

        let sequence = page.load(URL(string: "about:foo"))

        var actual: [WebPage.NavigationEvent] = []
        let expected: [WebPage.NavigationEvent] = [.startedProvisionalNavigation]

        await #expect(throws: (any Error).self) {
            for try await event in sequence {
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
            for try await event in sequence where event == .startedProvisionalNavigation {
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
                for try await event in sequence {
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
            for try await event in allNavigations {
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
            for try await event in sequence where event == .startedProvisionalNavigation {
                page.terminateWebContentProcess()
            }
            Issue.record("Terminating the web content process should trigger an error and therefore the loop should never finish.")
        } catch {
            #expect(error is WebPage.NavigationError)
        }
    }

    @Test(.disabled("This test is too slow"))
    func navigationProceedsAfterDiscardingNavigationStream() async throws {
        let page = WebPage()

        let html = "<title>A title</title>"
        page.load(html: html)

        // A timeout is used since observing the navigation sequence itself alters the outcome of this test.
        try await Task.sleep(for: .seconds(10))

        #expect(page.title == "A title")
    }

    @Test(.bug("https://bugs.webkit.org/show_bug.cgi?id=321578"))
    func backForwardListNotifiesObserversOfEveryChange() async throws {
        let page = WebPage()

        let urls = try [1, 2, 3].map { try #require(URL(string: "about:blank?\($0)")) }

        var changes = Observations { page.backForwardList }.makeAsyncIterator()
        var previousChange = await changes.next()

        for (index, url) in urls.enumerated() {
            try await page.load(url).wait()

            // A single navigation may produce more than one notification, so this waits for the one that
            // reflects the navigation.
            var change = try #require(await changes.next())
            while change.currentItem?.url != url {
                change = try #require(await changes.next())
            }

            // Successive values must not compare equal, otherwise SwiftUI modifiers that diff values,
            // such as `onChange(of:)`, never observe the navigation.
            #expect(change != previousChange, "loading \(url) produced a value equal to the previous one")
            previousChange = change

            #expect(change.backList.map(\.url) == Array(urls.prefix(index)))
            #expect(change.forwardList.isEmpty)
        }
    }

    @Test(.bug("https://bugs.webkit.org/show_bug.cgi?id=321578"))
    func navigatingToBackForwardListItemNotifiesObservers() async throws {
        let page = WebPage()

        let firstURL = try #require(URL(string: "about:blank?1"))
        let secondURL = try #require(URL(string: "about:blank?2"))

        try await page.load(firstURL).wait()
        try await page.load(secondURL).wait()

        var changes = Observations { page.backForwardList }.makeAsyncIterator()
        let changeBeforeNavigating = await changes.next()

        let firstItem = try #require(page.backForwardList.backList.last)

        try await page.load(firstItem).wait()

        // A single navigation may produce more than one notification, so this waits for the one that
        // reflects the navigation.
        var change = try #require(await changes.next())
        while change.currentItem?.url != firstURL {
            change = try #require(await changes.next())
        }

        #expect(change != changeBeforeNavigating)

        #expect(change.backList.isEmpty)
        #expect(change.forwardList.map(\.url) == [secondURL])
    }

    @Test(.bug("https://bugs.webkit.org/show_bug.cgi?id=321578"))
    func backForwardListItemIdentifiersAreStableAcrossSnapshots() async throws {
        let page = WebPage()

        let urls = try [1, 2, 3].map { try #require(URL(string: "about:blank?\($0)")) }

        try await page.load(urls[0]).wait()
        try await page.load(urls[1]).wait()

        let snapshot = page.backForwardList
        let identifiers = (snapshot.backList + [try #require(snapshot.currentItem)]).map(\.id)

        // Distinct entries must have distinct identifiers.
        #expect(Set(identifiers).count == identifiers.count)

        try await page.load(urls[2]).wait()

        // The same entries must keep their identifiers in a subsequent snapshot of the list, otherwise
        // SwiftUI views that identify items by `id`, such as `ForEach`, needlessly recreate them.
        let subsequentSnapshot = page.backForwardList

        #expect(subsequentSnapshot.backList.map(\.id) == identifiers)
        #expect(subsequentSnapshot.currentItem?.id != identifiers.last)
    }

    @Test(.bug("https://bugs.webkit.org/show_bug.cgi?id=321578"))
    func backForwardListItemsReflectUpdatesToTheirContents() async throws {
        let page = WebPage()

        let changes = Observations { page.backForwardList }

        // A URL is loaded rather than an HTML string so that the load produces an item in the list; a
        // string loaded with the default base URL of `about:blank` does not.
        let url = try #require(URL(string: "data:text/html,%3Ctitle%3EA%20title%3C/title%3E"))
        try await page.load(url).wait()

        // The item exists as soon as the navigation completes; only its title is not yet known.
        _ = try #require(page.backForwardList.currentItem)

        // The title of the document is not known when the item is added to the list as the navigation
        // commits, and is updated afterwards without the list itself changing.
        let change = try await #require(changes.first { @Sendable in await $0.currentItem?.title != nil })

        #expect(change.currentItem?.title == "A title")
        #expect(page.backForwardList.currentItem?.title == "A title")
    }

    @Test(.bug("https://bugs.webkit.org/show_bug.cgi?id=321578"))
    func backForwardListsWithDifferingHistoriesAreNotEqual() async throws {
        let page = WebPage()

        let firstURL = try #require(URL(string: "about:blank?1"))
        let secondURL = try #require(URL(string: "about:blank?2"))

        try await page.load(firstURL).wait()
        let afterFirstLoad = page.backForwardList

        #expect(afterFirstLoad != WebPage().backForwardList)
        #expect(afterFirstLoad == page.backForwardList)

        try await page.load(secondURL).wait()

        // Values captured before and after a navigation must not compare equal, otherwise SwiftUI modifiers
        // that diff values, such as `onChange(of:)`, never observe the navigation.
        #expect(afterFirstLoad != page.backForwardList)
    }

    @Test(.bug("https://bugs.webkit.org/show_bug.cgi?id=321578"))
    func backForwardListSubscriptAccessesItemsRelativeToCurrentItem() async throws {
        let page = WebPage()

        let urls = try [1, 2, 3].map { try #require(URL(string: "about:blank?\($0)")) }
        for url in urls {
            try await page.load(url).wait()
        }

        // The current item is the last of the three loads, so only the two preceding items are reachable.
        let listAfterLoading = page.backForwardList

        #expect(listAfterLoading[-1]?.url == urls[1])
        #expect(listAfterLoading[-2]?.url == urls[0])
        #expect(listAfterLoading[-3] == nil)
        #expect(listAfterLoading[0]?.url == urls[2])
        #expect(listAfterLoading[1] == nil)

        #expect(listAfterLoading[-1] == listAfterLoading.backList.last)
        #expect(listAfterLoading[0] == listAfterLoading.currentItem)

        // After navigating to the first of the three loads, only the two following items are reachable.
        try await page.load(try #require(listAfterLoading.backList.first)).wait()

        let listAfterNavigating = page.backForwardList

        #expect(listAfterNavigating[-1] == nil)
        #expect(listAfterNavigating[0]?.url == urls[0])
        #expect(listAfterNavigating[1]?.url == urls[1])
        #expect(listAfterNavigating[2]?.url == urls[2])
        #expect(listAfterNavigating[3] == nil)

        #expect(listAfterNavigating[0] == listAfterNavigating.currentItem)
        #expect(listAfterNavigating[1] == listAfterNavigating.forwardList.first)
    }
}

#endif // ENABLE_SWIFTUI
