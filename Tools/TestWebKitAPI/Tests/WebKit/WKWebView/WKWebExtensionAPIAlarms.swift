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
struct WKWebExtensionAPIAlarmsTests {
    private let alarmsManifest: [String: Any] = [
        "manifest_version": 3,

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "permissions": ["alarms"],
    ]

    private func loadAndRun(backgroundScript: String) async throws {
        let manager = try loadWebExtension(manifest: alarmsManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func errors() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.test.assertThrows(() => browser.alarms.create(null), /'info' value is invalid, because an object is expected/i)
                browser.test.assertThrows(() => browser.alarms.create(undefined), /'info' value is invalid, because an object is expected/i)

                browser.test.assertThrows(() => browser.alarms.create({ when: 'bad' }), /'when' is expected to be a number, but a string was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ delayInMinutes: 'bad' }), /'delayInMinutes' is expected to be a number, but a string was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ periodInMinutes: 'bad' }), /'periodInMinutes' is expected to be a number, but a string was provided/i)

                browser.test.assertThrows(() => browser.alarms.create({ when: NaN }), /'when' is expected to be a number, but NaN was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ delayInMinutes: NaN }), /'delayInMinutes' is expected to be a number, but NaN was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ periodInMinutes: NaN }), /'periodInMinutes' is expected to be a number, but NaN was provided/i)

                browser.test.assertThrows(() => browser.alarms.create({ when: [ Date.now() + 60000 ] }), /'when' is expected to be a number, but an array was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ delayInMinutes: [ 1 ] }), /'delayInMinutes' is expected to be a number, but an array was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ periodInMinutes: [ 1 ] }), /'periodInMinutes' is expected to be a number, but an array was provided/i)

                browser.test.assertThrows(() => browser.alarms.create({ when: true }), /'when' is expected to be a number, but a boolean was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ delayInMinutes: true }), /'delayInMinutes' is expected to be a number, but a boolean was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ periodInMinutes: true }), /'periodInMinutes' is expected to be a number, but a boolean was provided/i)

                browser.test.assertThrows(() => browser.alarms.create({ when: { time: Date.now() + 60000 } }), /'when' is expected to be a number, but an object was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ delayInMinutes: { value: 1 } }), /'delayInMinutes' is expected to be a number, but an object was provided/i)
                browser.test.assertThrows(() => browser.alarms.create({ periodInMinutes: { value: 1 } }), /'periodInMinutes' is expected to be a number, but an object was provided/i)

                browser.test.assertThrows(() => browser.alarms.create({ delayInMinutes: 1, when: Date.now() + 60000 }), /cannot specify both 'delayInMinutes' and 'when'/i)

                await browser.test.assertRejects(browser.alarms.create('one', { name: 'one', delayInMinutes: 1 }), /cannot specify 'name' when a name is also passed as an argument/i)

                browser.test.notifyPass()
                """
        )
    }

    // FIXME when rdar://174495016 is resolved.
    #if WTF_PLATFORM_MAC
    @Test(.disabled("rdar://174495016"))
    #else
    @Test
    #endif
    func delaySingleShot() async throws {
        try await loadAndRun(
            backgroundScript: """
                const startDate = Date.now()
                const delayInMilliseconds = 100
                var fireCount = 0

                function listener(alarmInfo) {
                  browser.test.assertEq(typeof alarmInfo.name, 'string')
                  browser.test.assertEq(typeof alarmInfo.scheduledTime, 'number')
                  browser.test.assertEq(typeof alarmInfo.periodInMinutes, 'undefined')

                  browser.test.assertEq(alarmInfo.name, 'single')
                  browser.test.assertTrue(alarmInfo.scheduledTime >= startDate + delayInMilliseconds, 'Scheduled date should be the registered date plus delay or later.')

                  if (fireCount++)
                      browser.test.notifyFail('This listener should not have been called more than once.')
                }

                browser.test.assertFalse(browser.alarms.onAlarm.hasListener(listener), 'Should not have onAlarm listener.')

                browser.alarms.onAlarm.addListener(listener)
                browser.test.assertTrue(browser.alarms.onAlarm.hasListener(listener), 'Should have onAlarm listener.')

                browser.alarms.create('single', { delayInMinutes: (delayInMilliseconds / 1000 / 60) })

                setTimeout(() => {
                  if (fireCount === 1)
                      browser.test.notifyPass()
                  else
                      browser.test.notifyFail('This timeout should have been called after the alarm fired once.')
                }, 500)
                """
        )
    }

    @Test
    func delayRepeating() async throws {
        try await loadAndRun(
            backgroundScript: """
                const startDate = Date.now()
                const delayInMilliseconds = 100
                const periodInMilliseconds = 150
                const periodInMinutes = (periodInMilliseconds / 1000 / 60)
                var fireCount = 0

                function listener(alarmInfo) {
                  browser.test.assertEq(typeof alarmInfo.name, 'string')
                  browser.test.assertEq(typeof alarmInfo.scheduledTime, 'number')
                  browser.test.assertEq(typeof alarmInfo.periodInMinutes, 'number')

                  browser.test.assertEq(alarmInfo.name, 'repeat')
                  browser.test.assertTrue(alarmInfo.scheduledTime >= startDate + (!fireCount ? delayInMilliseconds : periodInMilliseconds), 'Scheduled date should be the registered date plus delay or later.')
                  browser.test.assertEq(alarmInfo.periodInMinutes, periodInMinutes)

                  if (++fireCount < 3)
                      return

                  browser.test.notifyPass()
                }

                browser.test.assertFalse(browser.alarms.onAlarm.hasListener(listener), 'Should not have onAlarm listener.')

                browser.alarms.onAlarm.addListener(listener)
                browser.test.assertTrue(browser.alarms.onAlarm.hasListener(listener), 'Should have onAlarm listener.')

                browser.alarms.create('repeat', { delayInMinutes: (delayInMilliseconds / 1000 / 60), periodInMinutes })
                """
        )
    }

    // FIXME when webkit.org/b/311933 is resolved.
    #if WTF_PLATFORM_MAC
    @Test(.disabled("webkit.org/b/311933"))
    #else
    @Test
    #endif
    func whenSingleShot() async throws {
        try await loadAndRun(
            backgroundScript: """
                const startDate = Date.now()
                const delayInMilliseconds = 100
                var fireCount = 0

                function listener(alarmInfo) {
                  browser.test.assertEq(typeof alarmInfo.name, 'string')
                  browser.test.assertEq(typeof alarmInfo.scheduledTime, 'number')
                  browser.test.assertEq(typeof alarmInfo.periodInMinutes, 'undefined')

                  browser.test.assertEq(alarmInfo.name, 'when')
                  browser.test.assertTrue(alarmInfo.scheduledTime >= startDate + delayInMilliseconds, 'Scheduled date should be the registered date plus delay or later.')

                  if (fireCount++)
                      browser.test.notifyFail('This listener should not have been called more than once.')
                }

                browser.test.assertFalse(browser.alarms.onAlarm.hasListener(listener), 'Should not have onAlarm listener.')

                browser.alarms.onAlarm.addListener(listener)
                browser.test.assertTrue(browser.alarms.onAlarm.hasListener(listener), 'Should have onAlarm listener.')

                browser.alarms.create('when', { when: (startDate + delayInMilliseconds) })

                setTimeout(() => {
                  if (fireCount === 1)
                      browser.test.notifyPass()
                  else
                      browser.test.notifyFail('This timeout should have been called after the alarm fired once.')
                }, 500)
                """
        )
    }

    @Test
    func whenRepeating() async throws {
        try await loadAndRun(
            backgroundScript: """
                const startDate = Date.now()
                const delayInMilliseconds = 100
                const periodInMilliseconds = 150
                const periodInMinutes = (periodInMilliseconds / 1000 / 60)
                var fireCount = 0

                function listener(alarmInfo) {
                  browser.test.assertEq(typeof alarmInfo.name, 'string')
                  browser.test.assertEq(typeof alarmInfo.scheduledTime, 'number')
                  browser.test.assertEq(typeof alarmInfo.periodInMinutes, 'number')

                  browser.test.assertEq(alarmInfo.name, 'repeat')
                  browser.test.assertTrue(alarmInfo.scheduledTime >= startDate + (!fireCount ? delayInMilliseconds : periodInMilliseconds), 'Scheduled date should be the registered date plus delay or later.')
                  browser.test.assertEq(alarmInfo.periodInMinutes, periodInMinutes)

                  if (++fireCount < 3)
                      return

                  browser.test.notifyPass()
                }

                browser.test.assertFalse(browser.alarms.onAlarm.hasListener(listener), 'Should not have onAlarm listener.')

                browser.alarms.onAlarm.addListener(listener)
                browser.test.assertTrue(browser.alarms.onAlarm.hasListener(listener), 'Should have onAlarm listener.')

                browser.alarms.create('repeat', { when: (startDate + delayInMilliseconds), periodInMinutes })
                """
        )
    }

    @Test
    func clearSingleAlarm() async throws {
        try await loadAndRun(
            backgroundScript: """
                function listener(alarmInfo) {
                  browser.test.assertEq(alarmInfo.name, 'two', 'Should only be called for alarm two.')

                  browser.test.notifyPass()
                }

                browser.alarms.onAlarm.addListener(listener)

                browser.alarms.create('one', { delayInMinutes: 0.01 })
                browser.alarms.create('two', { delayInMinutes: 0.1 })

                browser.test.assertTrue(await browser.alarms.clear('one'), 'Should return true when an alarm was cleared.')
                browser.test.assertFalse(await browser.alarms.clear('one'), 'Should return false when no alarm matched.')
                browser.test.assertFalse(await browser.alarms.clear('missing'), 'Should return false for an unknown alarm.')
                """
        )
    }

    @Test
    func getSingleAlarm() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.alarms.create('one', { delayInMinutes: 1 })
                browser.alarms.create('two', { delayInMinutes: 1, periodInMinutes: 1 })

                let result = await browser.alarms.get('one')

                browser.test.assertEq(result.name, 'one')
                browser.test.assertEq(typeof result.scheduledTime, 'number')
                browser.test.assertEq(typeof result.periodInMinutes, 'undefined')

                result = await browser.alarms.get('two')

                browser.test.assertEq(result.name, 'two')
                browser.test.assertEq(typeof result.scheduledTime, 'number')
                browser.test.assertEq(typeof result.periodInMinutes, 'number')
                browser.test.assertEq(result.periodInMinutes, 1)

                result = await browser.alarms.get('missing')
                browser.test.assertEq(result, undefined, 'Should be undefined for an unknown alarm.')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func clearAllAlarms() async throws {
        try await loadAndRun(
            backgroundScript: """
                function listener(alarmInfo) {
                  browser.test.notifyFail('This listener should not have been called.')
                }

                browser.alarms.onAlarm.addListener(listener)

                browser.test.assertFalse(await browser.alarms.clearAll(), 'Should return false when there are no alarms.')

                browser.alarms.create('one', { delayInMinutes: 0.01 })
                browser.alarms.create('two', { delayInMinutes: 0.1 })

                browser.test.assertTrue(await browser.alarms.clearAll(), 'Should return true when alarms were cleared.')
                browser.test.assertFalse(await browser.alarms.clearAll(), 'Should return false once all alarms are gone.')

                setTimeout(() => {
                  browser.test.notifyPass()
                }, 500)
                """
        )
    }

    @Test
    func getAllAlarms() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.alarms.create('one', { delayInMinutes: 1 })
                browser.alarms.create('two', { delayInMinutes: 1, periodInMinutes: 1 })

                let result = await browser.alarms.getAll()

                browser.test.assertEq(result.length, 2)
                browser.test.assertTrue(result[0].name === 'one' || result[0].name === 'two')
                browser.test.assertEq(typeof result[0].scheduledTime, 'number')
                browser.test.assertEq(typeof result[0].periodInMinutes, result[0].name === 'one' ? 'undefined' : 'number')

                browser.test.assertTrue(result[1].name === 'one' || result[1].name === 'two')
                browser.test.assertEq(typeof result[1].scheduledTime, 'number')
                browser.test.assertEq(typeof result[1].periodInMinutes, result[1].name === 'one' ? 'undefined' : 'number')

                await browser.alarms.clear('one')

                result = await browser.alarms.getAll()
                browser.test.assertEq(result.length, 1)
                browser.test.assertEq(result[0].name, 'two')

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func unnamedAlarm() async throws {
        try await loadAndRun(
            backgroundScript: """
                function listener(alarmInfo) {
                  browser.test.assertEq(alarmInfo.name, '', 'Should only be called for alarm with an empty string name.')

                  browser.test.notifyPass('This listener should have been called.')
                }

                browser.alarms.onAlarm.addListener(listener)

                browser.alarms.create({ delayInMinutes: 0.01 })
                """
        )
    }

    @Test
    func createAlarms() async throws {
        try await loadAndRun(
            backgroundScript: """
                browser.alarms.create({ name: 'one', periodInMinutes: 1 })
                let result = await browser.alarms.get('one')
                browser.test.assertEq(result.name, 'one', 'Should create the alarm with the name from the info object')
                browser.test.assertEq(result.periodInMinutes, 1)
                browser.test.assertEq(result.delayInMinutes, undefined)

                browser.alarms.create('two', { periodInMinutes: 1 })
                result = await browser.alarms.get('two')
                browser.test.assertEq(result.name, 'two', 'Should create the alarm with the name passed as an argument.')
                browser.test.assertEq(result.periodInMinutes, 1)
                browser.test.assertEq(result.delayInMinutes, undefined)

                browser.test.notifyPass()
                """
        )
    }

    @Test
    func removeListenerDuringEvent() async throws {
        try await loadAndRun(
            backgroundScript: """
                function alarmListener() {
                  browser.alarms.onAlarm.removeListener(alarmListener)
                  browser.test.assertFalse(browser.alarms.onAlarm.hasListener(alarmListener), 'Listener should be removed')
                }

                browser.alarms.onAlarm.addListener(alarmListener)
                browser.alarms.onAlarm.addListener(() => browser.test.notifyPass())

                browser.test.assertTrue(browser.alarms.onAlarm.hasListener(alarmListener), 'Listener should be registered')

                browser.alarms.create('test-alarm', { delayInMinutes: (500 / 1000 / 60) })
                """
        )
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
