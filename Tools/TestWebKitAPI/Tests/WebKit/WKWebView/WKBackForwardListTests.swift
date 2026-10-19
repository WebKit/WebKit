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

import CoreFoundation
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
import Testing
import WebKit
private import WebKit_Private.WKBackForwardListPrivate
private import WebKit_Private.WKNavigationDelegatePrivate
private import WebKit_Private.WKWebViewPrivate

/// A change the back/forward list reported to its navigation delegate, by URL.
private struct BackForwardListChange: Equatable {
    var added: String?
    var removed: [String]
}

/// A navigation delegate that records every change the back/forward list reports.
private final class BackForwardListChangeRecorder: TestNavigationDelegate, WKNavigationDelegatePrivate {
    private(set) var changes: [BackForwardListChange] = []

    // swift-format-ignore: NoLeadingUnderscores
    func _webView(
        _ webView: WKWebView,
        backForwardListItemAdded itemAdded: WKBackForwardListItem?,
        removed itemsRemoved: [WKBackForwardListItem]?
    ) {
        changes.append(
            BackForwardListChange(added: itemAdded?.url.absoluteString, removed: itemsRemoved?.map(\.url.absoluteString) ?? [])
        )
    }
}

@MainActor
final class WKBackForwardListTests {
    private static let initialURLs = ["data:text/html,no%20error%20A", "data:text/html,no%20error%20B", "data:text/html,no%20error%20C"]

    private var webView: WKWebView?

    // A web view only holds its delegates weakly, so the test holds this for as long as it runs.
    private let recorder = BackForwardListChangeRecorder()

    init() async throws {
        let webView = WKWebView()
        for urlString in Self.initialURLs {
            let url = try #require(URL(string: urlString))
            try await webView.loadAndWait(URLRequest(url: url))
        }
        webView.navigationDelegate = recorder
        self.webView = webView
    }

    private static func itemURL(_ index: Int) -> String {
        "data:text/html,item\(index)"
    }

    // Unlike WKWebView.loadAndWait(_:), these leave the recorder as the navigation delegate during the navigation.
    private func navigate(to urlString: String) async throws {
        let webView = try #require(self.webView)
        webView.load(URLRequest(url: try #require(URL(string: urlString))))
        try await recorder.waitForDidFinishNavigation()
    }

    private func navigate(to item: WKBackForwardListItem) async throws {
        let webView = try #require(self.webView)
        webView.go(to: item)
        try await recorder.waitForDidFinishNavigation()
    }

    /// Returns the changes the back/forward list reported while `body` ran.
    private func recordChanges(during body: () async throws -> Void) async rethrows -> [BackForwardListChange] {
        let start = recorder.changes.count
        try await body()
        return Array(recorder.changes.dropFirst(start))
    }

    private func expectRepeatedAccessDoesNotRetain<T: AnyObject>(
        _ item: T,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ accessor: () -> T?
    ) {
        let retainCountBefore = CFGetRetainCount(item)
        for _ in 0..<10 {
            autoreleasepool {
                #expect(accessor() === item, sourceLocation: sourceLocation)
            }
        }
        #expect(CFGetRetainCount(item) == retainCountBefore, sourceLocation: sourceLocation)
    }

    @Test
    func itemAccessorsDoNotLeakReferences() async throws {
        let webView = try #require(self.webView)
        webView.goBack()
        try await webView._test_waitForDidFinishNavigation()

        let list = webView.backForwardList
        let (backItem, currentItem, forwardItem) = try autoreleasepool {
            (
                try #require(list.backList.last),
                try #require(list.currentItem),
                try #require(list.forwardList.first)
            )
        }

        expectRepeatedAccessDoesNotRetain(backItem) { list.backItem }
        expectRepeatedAccessDoesNotRetain(forwardItem) { list.forwardItem }
        expectRepeatedAccessDoesNotRetain(backItem) { list.item(at: -1) }
        expectRepeatedAccessDoesNotRetain(currentItem) { list.currentItem }
    }

    @Test
    func itemsAreDestroyedWithWebView() async throws {
        weak var weakItem1: WKBackForwardListItem?
        weak var weakItem2: WKBackForwardListItem?

        try autoreleasepool {
            let backList = try #require(webView).backForwardList.backList
            try #require(backList.count == 2)
            weakItem1 = backList[0]
            weakItem2 = backList[1]
        }

        webView?._close()
        webView = nil

        let deadline = ContinuousClock.now + .seconds(5)
        while weakItem1 != nil || weakItem2 != nil, ContinuousClock.now < deadline {
            try await Task.sleep(for: .milliseconds(10))
        }

        #expect(weakItem1 == nil)
        #expect(weakItem2 == nil)
    }

    @Test
    func removeAllItemsAfterCloseDoesNothing() throws {
        let webView = try #require(self.webView)
        let list = webView.backForwardList
        webView._close()

        // Closing the page empties the list and disconnects it from the page, so this has nothing to do.
        list._removeAllItems()

        #expect(list.currentItem == nil)
        #expect(list.backList.isEmpty)
    }

    @Test
    func addingItemAtCapacityRemovesOldestItem() async throws {
        let list = try #require(webView).backForwardList

        // The list holds at most 100 items.
        for index in Self.initialURLs.count..<100 {
            try await navigate(to: Self.itemURL(index))
        }
        #expect(list.backList.count == 99)
        #expect(list.backList.first?.url.absoluteString == Self.initialURLs[0])

        let changes = try await recordChanges { try await navigate(to: Self.itemURL(100)) }

        #expect(changes == [BackForwardListChange(added: Self.itemURL(100), removed: [Self.initialURLs[0]])])
        #expect(list.backList.count == 99)
        #expect(list.backList.first?.url.absoluteString == Self.initialURLs[1])
        #expect(list.currentItem?.url.absoluteString == Self.itemURL(100))
    }

    @Test
    func addingItemRemovesForwardItemsLastFirst() async throws {
        let list = try #require(webView).backForwardList

        // entries = [A(current), B, C]
        try await navigate(to: try #require(list.backList.first))
        #expect(list.forwardList.count == 2)

        let changes = try await recordChanges { try await navigate(to: Self.itemURL(0)) }

        #expect(changes == [BackForwardListChange(added: Self.itemURL(0), removed: [Self.initialURLs[2], Self.initialURLs[1]])])
        #expect(list.backList.map(\.url.absoluteString) == [Self.initialURLs[0]])
        #expect(list.forwardList.isEmpty)
    }

    @Test
    func clearReportsRemovedItemsInListOrder() async throws {
        let list = try #require(webView).backForwardList
        try await navigate(to: Self.itemURL(0))
        try await navigate(to: Self.itemURL(1))

        // entries = [A, B, C(current), 0, 1]
        let backList = list.backList
        try #require(backList.count == 4)
        try await navigate(to: backList[2])
        #expect(list.currentItem?.url.absoluteString == Self.initialURLs[2])

        let changes = await recordChanges { list._clear() }

        let removed = [Self.initialURLs[0], Self.initialURLs[1], Self.itemURL(0), Self.itemURL(1)]
        #expect(changes == [BackForwardListChange(added: nil, removed: removed)])
        #expect(list.backList.isEmpty)
        #expect(list.forwardList.isEmpty)
        #expect(list.currentItem?.url.absoluteString == Self.initialURLs[2])
    }

    @Test
    func removeAllItemsNotifiesEvenWhenEmpty() async throws {
        let list = try #require(webView).backForwardList

        let firstChanges = await recordChanges { list._removeAllItems() }
        #expect(firstChanges == [BackForwardListChange(added: nil, removed: Self.initialURLs)])
        #expect(list.currentItem == nil)

        let secondChanges = await recordChanges { list._removeAllItems() }
        #expect(secondChanges == [BackForwardListChange(added: nil, removed: [])])
    }
}
