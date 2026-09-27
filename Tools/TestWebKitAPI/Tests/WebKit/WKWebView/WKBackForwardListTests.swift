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
private import WebKit_Private.WKWebViewPrivate

@MainActor
final class WKBackForwardListTests {
    private var webView: WKWebView?

    init() async throws {
        let webView = WKWebView()
        for urlString in ["data:text/html,no%20error%20A", "data:text/html,no%20error%20B", "data:text/html,no%20error%20C"] {
            let url = try #require(URL(string: urlString))
            try await webView.loadAndWait(URLRequest(url: url))
        }
        self.webView = webView
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
}
