// Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#if ENABLE_WK_WEB_EXTENSIONS

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionPrivate

import struct Swift.String

@MainActor
struct WKWebExtensionTabTests {
    @Test
    func openTabs() throws {
        let contextOne = WKWebExtensionContext(for: try #require(WKWebExtension(manifestDictionary: ["manifest_version": 3])))
        let contextTwo = WKWebExtensionContext(for: try #require(WKWebExtension(manifestDictionary: ["manifest_version": 3])))

        let windowOne = TestWebExtensionWindow()
        let windowTwo = TestWebExtensionWindow()

        let tabOne = TestWebExtensionTab()
        let tabTwo = TestWebExtensionTab()
        let tabThree = TestWebExtensionTab()
        let tabFour = TestWebExtensionTab()

        windowOne.tabs = [tabOne]
        windowTwo.tabs = [tabTwo, tabThree]

        let controllerDelegate = TestWebExtensionsDelegate()
        controllerDelegate.openWindows = { _ in [windowOne, windowTwo] }

        let controller = WKWebExtensionController()
        controller.delegate = controllerDelegate

        #expect(contextOne.openTabs.isEmpty)
        #expect(contextTwo.openTabs.isEmpty)

        try controller.load(contextOne)

        #expect(contextOne.openTabs == [tabOne, tabTwo, tabThree])
        #expect(contextTwo.openTabs.isEmpty)

        try controller.load(contextTwo)

        #expect(contextOne.openTabs == [tabOne, tabTwo, tabThree])
        #expect(contextTwo.openTabs == [tabOne, tabTwo, tabThree])

        contextOne.didOpenTab(tabFour)

        #expect(contextOne.openTabs == [tabOne, tabTwo, tabThree, tabFour])
        #expect(contextTwo.openTabs == [tabOne, tabTwo, tabThree])

        windowTwo.tabs = [tabTwo, tabThree, tabFour]
        contextTwo.didOpenTab(tabFour)

        #expect(contextOne.openTabs == [tabOne, tabTwo, tabThree, tabFour])
        #expect(contextTwo.openTabs == [tabOne, tabTwo, tabThree, tabFour])

        windowOne.tabs = []
        controller.didCloseTab(tabOne, windowIsClosing: false)

        #expect(contextOne.openTabs == [tabTwo, tabThree, tabFour])
        #expect(contextTwo.openTabs == [tabTwo, tabThree, tabFour])

        try controller.unload(contextOne)

        #expect(contextOne.openTabs.isEmpty)
        #expect(contextTwo.openTabs == [tabTwo, tabThree, tabFour])

        windowOne.tabs = [tabOne]
        controller.didOpenTab(tabOne)

        #expect(contextOne.openTabs.isEmpty)
        #expect(contextTwo.openTabs == [tabOne, tabTwo, tabThree, tabFour])

        controller.didCloseWindow(windowOne)

        #expect(contextOne.openTabs.isEmpty)
        #expect(contextTwo.openTabs == [tabTwo, tabThree, tabFour])

        controller.didCloseWindow(windowTwo)

        #expect(contextOne.openTabs.isEmpty)
        #expect(contextTwo.openTabs.isEmpty)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
