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
struct WKWebExtensionWindowTests {
    @Test
    func openWindows() throws {
        let contextOne = WKWebExtensionContext(for: try #require(WKWebExtension(manifestDictionary: ["manifest_version": 3])))
        let contextTwo = WKWebExtensionContext(for: try #require(WKWebExtension(manifestDictionary: ["manifest_version": 3])))

        let windowOne = TestWebExtensionWindow()
        let windowTwo = TestWebExtensionWindow()

        let openWindows = [windowOne, windowTwo]
        let reversedOpenWindows = [windowTwo, windowOne]

        let controllerDelegate = TestWebExtensionsDelegate()
        controllerDelegate.openWindows = { context in
            context == contextOne ? openWindows : reversedOpenWindows
        }

        let controller = WKWebExtensionController()
        controller.delegate = controllerDelegate

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == [])
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == [])

        try controller.load(contextOne)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == openWindows)
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == [])

        try controller.load(contextTwo)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == openWindows)
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == reversedOpenWindows)

        contextOne.didFocusWindow(windowTwo)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == reversedOpenWindows)
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == reversedOpenWindows)

        contextOne.didFocusWindow(windowOne)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == openWindows)
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == reversedOpenWindows)

        controller.didFocusWindow(windowOne)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == openWindows)
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == openWindows)

        try controller.unload(contextOne)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == [])
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == openWindows)

        controller.didFocusWindow(windowTwo)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == [])
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == reversedOpenWindows)

        controller.didCloseWindow(windowTwo)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == [])
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == [windowOne])

        controller.didOpenWindow(windowTwo)

        #expect(contextOne.openWindows as? [TestWebExtensionWindow] == [])
        #expect(contextTwo.openWindows as? [TestWebExtensionWindow] == reversedOpenWindows)
    }

    @Test
    func focusedWindow() throws {
        let contextOne = WKWebExtensionContext(for: try #require(WKWebExtension(manifestDictionary: ["manifest_version": 3])))
        let contextTwo = WKWebExtensionContext(for: try #require(WKWebExtension(manifestDictionary: ["manifest_version": 3])))

        let windowOne = TestWebExtensionWindow()
        let windowTwo = TestWebExtensionWindow()

        let controllerDelegate = TestWebExtensionsDelegate()
        controllerDelegate.openWindows = { _ in [windowOne, windowTwo] }
        controllerDelegate.focusedWindow = { context in
            context == contextOne ? windowTwo : nil
        }

        let controller = WKWebExtensionController()
        controller.delegate = controllerDelegate

        #expect(contextOne.focusedWindow == nil)
        #expect(contextTwo.focusedWindow == nil)

        try controller.load(contextOne)

        #expect(contextOne.focusedWindow === windowTwo)
        #expect(contextTwo.focusedWindow == nil)

        try controller.load(contextTwo)

        #expect(contextOne.focusedWindow === windowTwo)
        #expect(contextTwo.focusedWindow == nil)

        contextOne.didFocusWindow(windowOne)

        #expect(contextOne.focusedWindow === windowOne)
        #expect(contextTwo.focusedWindow == nil)

        contextOne.didFocusWindow(nil)

        #expect(contextOne.focusedWindow == nil)
        #expect(contextTwo.focusedWindow == nil)

        controller.didFocusWindow(windowOne)

        #expect(contextOne.focusedWindow === windowOne)
        #expect(contextTwo.focusedWindow === windowOne)

        try controller.unload(contextOne)

        #expect(contextOne.focusedWindow == nil)
        #expect(contextTwo.focusedWindow === windowOne)

        controller.didFocusWindow(windowTwo)

        #expect(contextOne.focusedWindow == nil)
        #expect(contextTwo.focusedWindow === windowTwo)

        controller.didCloseWindow(windowTwo)

        #expect(contextOne.focusedWindow == nil)
        #expect(contextTwo.focusedWindow == nil)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
