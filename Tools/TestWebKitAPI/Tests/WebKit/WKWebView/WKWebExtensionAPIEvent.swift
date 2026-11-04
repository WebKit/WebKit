// Copyright (C) 2023-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing

import struct Swift.String

@MainActor
struct WKWebExtensionAPIEventTests {
    private let eventManifest: [String: Any] = [
        "manifest_version": 3,
        "background": [
            "scripts": ["background.js"],
            "persistent": false,
        ],
    ]

    private let eventAlarmsManifest: [String: Any] = [
        "manifest_version": 3,
        "background": [
            "scripts": ["background.js"],
            "persistent": false,
        ],
        "permissions": ["alarms"],
    ]

    @Test
    func errors() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.runtime.onStartup.addListener('bad'), /'listener' value is invalid, because a function is expected/i)
            browser.test.assertThrows(() => browser.runtime.onStartup.removeListener('bad'), /'listener' value is invalid, because a function is expected/i)
            browser.test.assertThrows(() => browser.runtime.onStartup.hasListener('bad'), /'listener' value is invalid, because a function is expected/i)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: eventManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func testEventListener() async throws {
        let backgroundScript = """
            function listener() { }
            browser.test.assertFalse(browser.runtime.onStartup.hasListener(listener), 'Should not have listener')
            browser.runtime.onStartup.addListener(listener)

            browser.test.assertTrue(browser.runtime.onStartup.hasListener(listener), 'Should have listener')
            browser.runtime.onStartup.removeListener(listener)
            browser.test.assertFalse(browser.runtime.onStartup.hasListener(listener), 'Should not have listener')
            browser.runtime.onStartup.addListener(listener)

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: eventManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func addingSameListenerRepeatedlyRegistersItOnce() async throws {
        let backgroundScript = """
            let callCount = 0
            function listener() { ++callCount }

            for (let i = 0; i < 1000; ++i)
                browser.alarms.onAlarm.addListener(listener)

            // Registered after the repeated listener, so it runs after every call to it in the same dispatch.
            browser.alarms.onAlarm.addListener(() => {
                browser.test.assertEq(callCount, 1, 'Repeated addListener calls with the same function should register it once')
                browser.test.notifyPass()
            })

            browser.alarms.create('repeatedListener', { delayInMinutes: 100 / 1000 / 60 })
            """

        let manager = try loadWebExtension(manifest: eventAlarmsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func removingRepeatedlyAddedListenerRemovesIt() async throws {
        let backgroundScript = """
            function listener() { }

            browser.alarms.onAlarm.addListener(listener)
            browser.alarms.onAlarm.addListener(listener)
            browser.alarms.onAlarm.addListener(listener)
            browser.test.assertTrue(browser.alarms.onAlarm.hasListener(listener), 'Should have listener')

            browser.alarms.onAlarm.removeListener(listener)
            browser.test.assertFalse(browser.alarms.onAlarm.hasListener(listener), 'A single removeListener should remove the listener')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: eventAlarmsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func distinctListenersAreAllCalled() async throws {
        let backgroundScript = """
            let callCount = 0

            for (let i = 0; i < 3; ++i)
                browser.alarms.onAlarm.addListener(() => { ++callCount })

            browser.alarms.onAlarm.addListener(() => {
                browser.test.assertEq(callCount, 3, 'Distinct listener functions should each be called')
                browser.test.notifyPass()
            })

            browser.alarms.create('distinctListeners', { delayInMinutes: 100 / 1000 / 60 })
            """

        let manager = try loadWebExtension(manifest: eventAlarmsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
