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
private import TestWebKitAPILibrary.Helpers.cocoa.NSLocaleSPI
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionPrivate

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#else
private import UIKit
#endif

// The language override is process-wide, so these tests must not run concurrently with each other.
@MainActor
@Suite(.serialized)
struct WKWebExtensionAPILocalizationTests {
    private let messageKey = "message"
    private let placeholdersKey = "placeholders"
    private let placeholderDictionaryContentKey = "content"

    private let localizationManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Localization Test",
        "description": "Localization Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],

        "default_locale": "en",
    ]

    private let messages: [String: [String: String]] = [
        "extension_name": [
            "message": "Web Extension Test Extension",
            "description": "The display name for the extension.",
        ],

        "extension_description": [
            "message": "This is a Web Extension. Tell us what your extension does here",
            "description": "Description of what the extension does.",
        ],
    ]

    private let baseURLString = "test-extension://76C788B8-3374-400D-8259-40E5B9DF79D3"

    private let textDirection: String
    private let reversedTextDirection: String
    private let startEdge: String
    private let endEdge: String

    private let currentLocaleString: String
    private let currentSystemLocaleString: String

    init() {
        // Swift Testing creates the suite for each test before running it, so these are read before the test overrides
        // AppleLanguages: the expected values come from the system locale, not the override.
        let currentLocale = NSLocale.current as NSLocale
        let writingDirection = NSParagraphStyle.defaultWritingDirection(forLanguage: currentLocale.languageCode)
        textDirection = writingDirection == .leftToRight ? "ltr" : "rtl"
        reversedTextDirection = writingDirection == .leftToRight ? "rtl" : "ltr"
        startEdge = writingDirection == .leftToRight ? "left" : "right"
        endEdge = writingDirection == .leftToRight ? "right" : "left"

        currentLocaleString = Self.localeStringInWebExtensionFormat(currentLocale)
        currentSystemLocaleString = NSLocale._deviceLanguage()
    }

    private static func localeStringInWebExtensionFormat(_ locale: NSLocale) -> String {
        if locale.languageCode.isEmpty {
            return "und"
        }

        if let countryCode = locale.countryCode, !countryCode.isEmpty {
            return "\(locale.languageCode)-\(countryCode)"
        }
        return locale.languageCode
    }

    private func constructJSArrayOfStrings(_ elements: [String]) -> String {
        "['\(elements.joined(separator: "', '"))']"
    }

    /// Each preferred locale identifier followed by its language code, without duplicates.
    private func acceptedLanguages(for preferredLocaleIdentifiers: [String]) -> [String] {
        var acceptedLanguages: [String] = []
        for localeIdentifier in preferredLocaleIdentifiers {
            let languageCode = NSLocale(localeIdentifier: localeIdentifier).languageCode
            for language in [localeIdentifier, languageCode] where !acceptedLanguages.contains(language) {
                acceptedLanguages.append(language)
            }
        }
        return acceptedLanguages
    }

    private func loadAndRun(resources: [String: Any]) async throws {
        let manager = try loadWebExtension(manifest: localizationManifest, resources: resources)
        try await manager.run()
    }

    @Test
    func errors() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.i18n.getMessage(''), /'name' value is invalid, because it cannot be empty/i)
            browser.test.assertThrows(() => browser.i18n.getMessage(), /a required argument is missing/i)
            browser.test.assertThrows(() => browser.i18n.getMessage(123), /'name' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.i18n.getMessage(null), /'name' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.i18n.getMessage(undefined), /'name' value is invalid, because a string is expected/i)

            // Finish
            browser.test.notifyPass()
            """

        let manager = parseWebExtension(
            manifest: localizationManifest,
            resources: ["background.js": backgroundScript, "_locales/en/messages.json": messages]
        )
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        try await manager.loadAndRun()
    }

    @Test
    func i18n() async throws {
        let preferredLocaleIdentifiers = NSLocale.preferredLanguages

        let acceptedLanguagesString = constructJSArrayOfStrings(acceptedLanguages(for: preferredLocaleIdentifiers))
        let preferredLanguagesString = constructJSArrayOfStrings(preferredLocaleIdentifiers)

        let backgroundScript = """
            const acceptedLanguages = \(acceptedLanguagesString)
            const preferredLanguages = \(preferredLanguagesString)
            const currentUILanguage = '\(currentLocaleString)'
            const currentSystemUILanguage = '\(currentSystemLocaleString)'
            const textDirection = '\(textDirection)'
            const reversedTextDirection = '\(reversedTextDirection)'
            const startEdge = '\(startEdge)'
            const endEdge = '\(endEdge)'

            browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Web Extension Test Extension')
            browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')
            browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), 'en')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), textDirection)
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), reversedTextDirection)
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), startEdge)
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), endEdge)
            browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')

            browser.test.assertFalse(currentUILanguage === 'und')
            browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)
            browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)

            browser.test.assertFalse(currentSystemUILanguage === 'und')
            browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)
            browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(
            manifest: localizationManifest,
            resources: ["background.js": backgroundScript, "_locales/en/messages.json": messages]
        )
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        try await manager.loadAndRun()
    }

    @Test
    func i18nWithFallback() async throws {
        try await withAppleLanguages(["en-US"]) {
            let preferredLocaleIdentifiers = NSLocale.preferredLanguages

            let acceptedLanguagesString = constructJSArrayOfStrings(acceptedLanguages(for: preferredLocaleIdentifiers))
            let preferredLanguagesString = constructJSArrayOfStrings(preferredLocaleIdentifiers)

            let backgroundScript = """
                const acceptedLanguages = \(acceptedLanguagesString)
                const preferredLanguages = \(preferredLanguagesString)
                const currentUILanguage = '\(currentLocaleString)'
                const currentSystemUILanguage = '\(currentSystemLocaleString)'
                const textDirection = '\(textDirection)'
                const reversedTextDirection = '\(reversedTextDirection)'
                const startEdge = '\(startEdge)'
                const endEdge = '\(endEdge)'

                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Web Extension Test Extension')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default String')
                browser.test.assertEq(browser.i18n.getMessage('regional_name'), 'Regional String')
                browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')
                browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), 'en_US')
                browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), textDirection)
                browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), reversedTextDirection)
                browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), startEdge)
                browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), endEdge)
                browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')

                browser.test.assertFalse(currentUILanguage === 'und')
                browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)
                browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)

                browser.test.assertFalse(currentSystemUILanguage === 'und')
                browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)
                browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)

                browser.test.notifyPass()
                """

            let manifest: [String: Any] = [
                "manifest_version": 3,

                "name": "Localization Test",
                "description": "Localization Test",
                "version": "1",

                "background": [
                    "scripts": ["background.js"],
                    "type": "module",
                    "persistent": false,
                ],

                "default_locale": "fr",
            ]

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default String",
                    "description": "The test name.",
                ]
            ]

            let regionalMessages: [String: [String: String]] = [
                "regional_name": [
                    "message": "Regional String",
                    "description": "The regional name.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/fr/messages.json": defaultMessages,
                "_locales/en/messages.json": messages,
                "_locales/en_US/messages.json": regionalMessages,
            ]

            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            // Set a base URL so it is a known value and not the default random one.
            WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
            context.baseURL = try #require(URL(string: baseURLString))

            try await manager.loadAndRun()
        }
    }

    @Test
    func i18nWithoutMessages() async throws {
        let preferredLocaleIdentifiers = NSLocale.preferredLanguages

        let acceptedLanguagesString = constructJSArrayOfStrings(acceptedLanguages(for: preferredLocaleIdentifiers))
        let preferredLanguagesString = constructJSArrayOfStrings(preferredLocaleIdentifiers)

        let backgroundScript = """
            const acceptedLanguages = \(acceptedLanguagesString)
            const preferredLanguages = \(preferredLanguagesString)
            const currentUILanguage = '\(currentLocaleString)'
            const currentSystemUILanguage = '\(currentSystemLocaleString)'

            browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')
            browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), '')
            browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')

            browser.test.assertFalse(currentUILanguage === 'und')
            browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)
            browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)

            browser.test.assertFalse(currentSystemUILanguage === 'und')
            browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)
            browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: localizationManifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        try await manager.loadAndRun()
    }

    @Test
    func i18nWithoutDefaultLocale() async throws {
        let preferredLocaleIdentifiers = NSLocale.preferredLanguages

        let manifestWithoutLocale: [String: Any] = [
            "manifest_version": 3,

            "name": "Localization Test",
            "description": "Localization Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let acceptedLanguagesString = constructJSArrayOfStrings(acceptedLanguages(for: preferredLocaleIdentifiers))
        let preferredLanguagesString = constructJSArrayOfStrings(preferredLocaleIdentifiers)

        let backgroundScript = """
            const acceptedLanguages = \(acceptedLanguagesString)
            const preferredLanguages = \(preferredLanguagesString)
            const currentUILanguage = '\(currentLocaleString)'
            const currentSystemUILanguage = '\(currentSystemLocaleString)'

            browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')
            browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), '')
            browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), '')
            browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')

            browser.test.assertFalse(currentUILanguage === 'und')
            browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)
            browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)

            browser.test.assertFalse(currentSystemUILanguage === 'und')
            browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)
            browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)

            browser.test.notifyPass()
            """

        let manager = parseWebExtension(manifest: manifestWithoutLocale, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        try await manager.loadAndRun()
    }

    @Test
    func placeholders() async throws {
        let localizationDictionary: [String: [String: Any]] = [
            "key1": [
                messageKey: "$placeholder$",
                placeholdersKey: [
                    "placeholder": [
                        placeholderDictionaryContentKey: "replacement value"
                    ]
                ],
            ],
            "key2": [
                messageKey: "$Placeholder$ + $PlAcEhOlDeR$",
                placeholdersKey: [
                    "placeholder": [
                        placeholderDictionaryContentKey: "replacement value"
                    ]
                ],
            ],
            "key3": [
                messageKey: "$placeholder$ and $place_holder$",
                placeholdersKey: [
                    "placeholder": [
                        placeholderDictionaryContentKey: "replacement value"
                    ],
                    "pLaCe_hoLder": [
                        placeholderDictionaryContentKey: "different value"
                    ],
                ],
            ],
            "key4": [
                messageKey: "$placeholder$ with $$5",
                placeholdersKey: [
                    "unusedplaceholder": [
                        placeholderDictionaryContentKey: "replacement value"
                    ]
                ],
            ],
            "key5": [
                messageKey: "$placeholder$ and $positional$",
                placeholdersKey: [
                    "unusedplaceholder": [
                        placeholderDictionaryContentKey: "replacement value"
                    ],
                    "positional": [
                        placeholderDictionaryContentKey: "$2"
                    ],
                ],
            ],
            "key6": [
                messageKey: "$1 and $2 but not $3 or $0",
                placeholdersKey: [
                    "unusedplaceholder": [
                        placeholderDictionaryContentKey: "replacement value"
                    ]
                ],
            ],
            "key7": [
                messageKey: "$dontreplaceme or me$ or th$is or $$th$$at$",
                placeholdersKey: [
                    "unusedplaceholder": [
                        placeholderDictionaryContentKey: "replacement value"
                    ]
                ],
            ],
            "key8": [
                messageKey: "$total$명, 총 $found$명",
                placeholdersKey: [
                    "total": [
                        placeholderDictionaryContentKey: "1234"
                    ],
                    "found": [
                        placeholderDictionaryContentKey: "42"
                    ],
                ],
            ],
            "key9": [
                messageKey: "v$version$",
                placeholdersKey: [
                    "version": [
                        placeholderDictionaryContentKey: "$1"
                    ]
                ],
            ],
            "key10": [
                messageKey: "v$major$.$minor$版",
                placeholdersKey: [
                    "major": [
                        placeholderDictionaryContentKey: "$1"
                    ],
                    "minor": [
                        placeholderDictionaryContentKey: "$2"
                    ],
                ],
            ],
            "key11": [
                messageKey: "$index$ $volume$",
                placeholdersKey: [
                    "index": [
                        placeholderDictionaryContentKey: "$1"
                    ],
                    "volume": [
                        placeholderDictionaryContentKey: "$2"
                    ],
                ],
            ],
            "key12": [
                messageKey: "$prefix$$suffix$",
                placeholdersKey: [
                    "prefix": [
                        placeholderDictionaryContentKey: "$1"
                    ],
                    "suffix": [
                        placeholderDictionaryContentKey: "$2"
                    ],
                ],
            ],
            "key13": [
                messageKey: "$count$ apples",
                placeholdersKey: [
                    "count": [
                        placeholderDictionaryContentKey: "$1"
                    ]
                ],
            ],
        ]

        let backgroundScript = """
            // Variable setup
            var placeholders = \(constructJSArrayOfStrings(["irrelevant", "unused"]))

            // Test predefined localization
            browser.test.assertEq(browser.i18n.getMessage('key1', placeholders), 'replacement value')
            browser.test.assertEq(browser.i18n.getMessage('key2', placeholders), 'replacement value + replacement value')
            browser.test.assertEq(browser.i18n.getMessage('key3', placeholders), 'replacement value and different value')
            browser.test.assertEq(browser.i18n.getMessage('key4', placeholders), ' with $5')
            browser.test.assertEq(browser.i18n.getMessage('key7', placeholders), '$dontreplaceme or me$ or th$is or $th$at$')
            browser.test.assertEq(browser.i18n.getMessage('key8', placeholders), '1234명, 총 42명')

            var placeholders = \(constructJSArrayOfStrings(["3", "4"]))
            browser.test.assertEq(browser.i18n.getMessage('key9', placeholders), 'v3')
            browser.test.assertEq(browser.i18n.getMessage('key10', placeholders), 'v3.4版')
            browser.test.assertEq(browser.i18n.getMessage('key11', placeholders), '3 4')
            browser.test.assertEq(browser.i18n.getMessage('key12', placeholders), '34')

            placeholders = \(constructJSArrayOfStrings(["irrelevant", "argument value"]))
            browser.test.assertEq(browser.i18n.getMessage('key5', placeholders), ' and argument value')

            placeholders = \(constructJSArrayOfStrings(["value1", "value2"]))
            browser.test.assertEq(browser.i18n.getMessage('key6', placeholders), 'value1 and value2 but not  or ')

            browser.test.assertEq(browser.i18n.getMessage('key13', [5]), '5 apples')
            browser.test.assertEq(browser.i18n.getMessage('key13', [0]), '0 apples')
            browser.test.assertEq(browser.i18n.getMessage('key11', [5, 6]), '5 6')
            browser.test.assertEq(browser.i18n.getMessage('key11', [5, 'trackers']), '5 trackers')
            browser.test.assertEq(browser.i18n.getMessage('key11', [true, null]), 'true null')
            browser.test.assertEq(browser.i18n.getMessage('key11', [{}, [1, 2, 3]]), '[object Object] 1,2,3')

            // Finish
            browser.test.notifyPass()
            """

        let manager = parseWebExtension(
            manifest: localizationManifest,
            resources: ["background.js": backgroundScript, "_locales/en/messages.json": localizationDictionary]
        )
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        try await manager.loadAndRun()
    }

    @Test
    func cssLocalization() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let htmlContent = "<link rel='stylesheet' href='test.css'><script src='test.js'></script>"

        let cssContent = """
            body {
              content: '__MSG_@@extension_id__';
              direction: __MSG_@@bidi_dir__;',
              text-align: __MSG_@@bidi_start_edge__;
            }
            """

        let scriptContent = """
            window.addEventListener('load', () => {
              const bodyStyle = getComputedStyle(document.body)
              browser.test.assertEq(bodyStyle.content, '"76C788B8-3374-400D-8259-40E5B9DF79D3"', 'The content should be')
              browser.test.assertEq(bodyStyle.direction, 'ltr', 'The direction should be')
              browser.test.assertEq(bodyStyle.textAlign, 'start', 'The text-align should be')

              browser.test.notifyPass()
            })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "test.html": htmlContent,
            "test.js": scriptContent,
            "test.css": cssContent,
            "_locales/en/messages.json": messages,
        ]

        let manager = parseWebExtension(manifest: localizationManifest, resources: resources)
        let context = try #require(manager.context)

        // Set a base URL so it is a known value and not the default random one.
        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        context.baseURL = try #require(URL(string: baseURLString))

        manager.load()
        try await manager.waitForTestMessage("Load Tab")

        let testPageURL = try #require(URL(string: "test.html", relativeTo: context.baseURL))
        let defaultTab = try #require(manager.defaultTab)
        defaultTab.changeWebViewIfNeeded(for: testPageURL, for: context)
        try #require(defaultTab.webView).load(URLRequest(url: testPageURL))

        try await manager.run()
    }

    @Test
    func cssLocalizationInContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "default_locale": "en",

            "name": "Localization Test",
            "description": "Localization Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
            ],

            "content_scripts": [
                [
                    "matches": ["<all_urls>"],
                    "css": ["content.css"],
                    "js": ["content.js"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentStyleSheet = """
            body {
              content: '__MSG_@@extension_id__';
              direction: '__MSG_@@bidi_dir__';
              text-align: '__MSG_@@bidi_start_edge__';
            }
            """

        let contentScript = """
            window.addEventListener('load', () => {
              const bodyStyle = getComputedStyle(document.body)
              browser.test.assertEq(bodyStyle.content, '"76C788B8-3374-400D-8259-40E5B9DF79D3"', 'CSS content should be')
              browser.test.assertEq(bodyStyle.direction, 'ltr', 'CSS direction should be')
              browser.test.assertEq(bodyStyle.textAlign, 'start', 'CSS text-align should be')

              browser.test.notifyPass()
            })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
            "content.css": contentStyleSheet,
            "_locales/en/messages.json": messages,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            // Set a base URL so it is a known value and not the default random one.
            WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
            context.baseURL = try #require(URL(string: baseURLString))

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func cssLocalizationInRegisteredContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "default_locale": "en",

            "name": "Localization Test",
            "description": "Localization Test",
            "version": "1.0",

            "permissions": ["scripting"],

            "background": [
                "scripts": ["background.js"],
                "type": "module",
            ],
        ]

        let backgroundScript = """
            await browser.scripting.registerContentScripts([{
              id: 'test',
              matches: ['<all_urls>'],
              css: ['content.css'],
              js: ['content.js']
            }])

            browser.test.sendMessage('Load Tab')
            """

        let contentStyleSheet = """
            body {
              content: '__MSG_@@extension_id__';
              direction: '__MSG_@@bidi_dir__';
              text-align: '__MSG_@@bidi_start_edge__';
            }
            """

        let contentScript = """
            window.addEventListener('load', () => {
              const bodyStyle = getComputedStyle(document.body)
              browser.test.assertEq(bodyStyle.content, '"76C788B8-3374-400D-8259-40E5B9DF79D3"', 'CSS content should be')
              browser.test.assertEq(bodyStyle.direction, 'ltr', 'CSS direction should be')
              browser.test.assertEq(bodyStyle.textAlign, 'start', 'CSS text-align should be')

              browser.test.notifyPass()
            })
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
            "content.css": contentStyleSheet,
            "_locales/en/messages.json": messages,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            // Set a base URL so it is a known value and not the default random one.
            WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
            context.baseURL = try #require(URL(string: baseURLString))

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func i18nChineseSimplified() async throws {
        try await withAppleLanguages(["zh-Hans-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_CN/messages.json": simplifiedChineseMessages,
                "_locales/zh_TW/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseSimplifiedOnly() async throws {
        try await withAppleLanguages(["zh-Hans-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_CN/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseSimplifiedScript() async throws {
        try await withAppleLanguages(["zh-Hans-US", "en-US", "zh-HK"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_Hans/messages.json": simplifiedChineseMessages,
                "_locales/zh_Hant/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseTraditional() async throws {
        try await withAppleLanguages(["zh-Hant-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_CN/messages.json": simplifiedChineseMessages,
                "_locales/zh_TW/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseSimplifiedInTaiwan() async throws {
        try await withAppleLanguages(["zh-Hans-TW"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_CN/messages.json": simplifiedChineseMessages,
                "_locales/zh_TW/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseTraditionalInChina() async throws {
        try await withAppleLanguages(["zh-Hant-CN"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_CN/messages.json": simplifiedChineseMessages,
                "_locales/zh_TW/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseSimplifiedInHongKong() async throws {
        try await withAppleLanguages(["zh-Hans-HK"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let cantoneseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "香港擴展",
                    "description": "The name of the extension in Cantonese (Hong Kong).",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_CN/messages.json": simplifiedChineseMessages,
                "_locales/zh_TW/messages.json": traditionalChineseMessages,
                "_locales/zh_HK/messages.json": cantoneseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseTraditionalOnly() async throws {
        try await withAppleLanguages(["zh-Hant-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_TW/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseTraditionalScript() async throws {
        try await withAppleLanguages(["zh-Hant-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let traditionalChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "繁體中文擴展",
                    "description": "The name of the extension in Traditional Chinese.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh_Hans/messages.json": simplifiedChineseMessages,
                "_locales/zh_Hant/messages.json": traditionalChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseLanguageFallback() async throws {
        try await withAppleLanguages(["zh-Hans-CN"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('regional_name'), '简体中文扩展')
                browser.test.assertEq(browser.i18n.getMessage('language_name'), '中文扩展')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let simplifiedChineseMessages: [String: [String: String]] = [
                "regional_name": [
                    "message": "简体中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let genericChineseMessages: [String: [String: String]] = [
                "language_name": [
                    "message": "中文扩展",
                    "description": "The name of the extension in generic Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh/messages.json": genericChineseMessages,
                "_locales/zh_CN/messages.json": simplifiedChineseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nChineseTraditionalFallback() async throws {
        try await withAppleLanguages(["zh-Hant"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('language_name'), '中文擴展', 'Generic Chinese message should be used')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String', 'Default fallback message should be used')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let genericChineseMessages: [String: [String: String]] = [
                "language_name": [
                    "message": "中文擴展",
                    "description": "The name of the extension in Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh/messages.json": genericChineseMessages,
            ]

            let webExtension = try #require(WKWebExtension(manifestDictionary: localizationManifest, resources: resources))
            let manager = parseWebExtension(webExtension)

            try await manager.loadAndRun()
        }
    }

    @Test
    func i18nChineseSimplifiedFallback() async throws {
        try await withAppleLanguages(["zh-Hans"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('language_name'), '中文扩展', 'Generic Chinese message should be used')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String', 'Default fallback message should be used')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let genericChineseMessages: [String: [String: String]] = [
                "language_name": [
                    "message": "中文扩展",
                    "description": "The name of the extension in Simplified Chinese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/zh/messages.json": genericChineseMessages,
            ]

            let webExtension = try #require(WKWebExtension(manifestDictionary: localizationManifest, resources: resources))
            let manager = parseWebExtension(webExtension)

            try await manager.loadAndRun()
        }
    }

    @Test
    func i18nPortugueseBrazilian() async throws {
        try await withAppleLanguages(["pt-BR"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Brasileira')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let brazilianPortugueseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensão Brasileira",
                    "description": "The name of the extension in Brazilian Portuguese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/pt_BR/messages.json": brazilianPortugueseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nPortugueseEuropean() async throws {
        try await withAppleLanguages(["pt-PT"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Portuguesa')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let europeanPortugueseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensão Portuguesa",
                    "description": "The name of the extension in European Portuguese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/pt_PT/messages.json": europeanPortugueseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nPortugueseUnitedStates() async throws {
        try await withAppleLanguages(["pt-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Portuguesa')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let europeanPortugueseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensão Portuguesa",
                    "description": "The name of the extension in European Portuguese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/pt_PT/messages.json": europeanPortugueseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nPortugueseUnitedStatesFallbackToBrazilian() async throws {
        try await withAppleLanguages(["pt-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Brasileira')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let brazilianPortugueseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensão Brasileira",
                    "description": "The name of the extension in Brazilian Portuguese.",
                ]
            ]

            let europeanPortugueseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensão Portuguesa",
                    "description": "The name of the extension in European Portuguese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/pt_BR/messages.json": brazilianPortugueseMessages,
                "_locales/pt_PT/messages.json": europeanPortugueseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nPortugueseUnitedStatesFallbackToGeneric() async throws {
        try await withAppleLanguages(["pt-US"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Portuguesa Genérica')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let genericPortugueseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensão Portuguesa Genérica",
                    "description": "The name of the extension in generic Portuguese.",
                ]
            ]

            let europeanPortugueseMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensão Portuguesa",
                    "description": "The name of the extension in European Portuguese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/pt/messages.json": genericPortugueseMessages,
                "_locales/pt_PT/messages.json": europeanPortugueseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nPortugueseLanguageFallback() async throws {
        try await withAppleLanguages(["pt-BR"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('regional_name'), 'Extensão Brasileira')
                browser.test.assertEq(browser.i18n.getMessage('language_name'), 'Extensão Portuguesa Genérica')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let brazilianPortugueseMessages: [String: [String: String]] = [
                "regional_name": [
                    "message": "Extensão Brasileira",
                    "description": "The name of the extension in Brazilian Portuguese.",
                ]
            ]

            let genericPortugueseMessages: [String: [String: String]] = [
                "language_name": [
                    "message": "Extensão Portuguesa Genérica",
                    "description": "The name of the extension in generic Portuguese.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/pt/messages.json": genericPortugueseMessages,
                "_locales/pt_BR/messages.json": brazilianPortugueseMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nSpanishLatinAmerica() async throws {
        try await withAppleLanguages(["es-419"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensión en español')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let latinAmericanSpanishMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensión en español",
                    "description": "The name of the extension in Latin American Spanish.",
                ]
            ]

            let castilianSpanishMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensión en castellano",
                    "description": "The name of the extension in Castilian Spanish.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/es_419/messages.json": latinAmericanSpanishMessages,
                "_locales/es_ES/messages.json": castilianSpanishMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nSpanishMexico() async throws {
        try await withAppleLanguages(["es-MX"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensión en español')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let latinAmericanSpanishMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensión en español",
                    "description": "The name of the extension in Latin American Spanish.",
                ]
            ]

            let castilianSpanishMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensión en castellano",
                    "description": "The name of the extension in Castilian Spanish.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/es_419/messages.json": latinAmericanSpanishMessages,
                "_locales/es_ES/messages.json": castilianSpanishMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nSpanishSpain() async throws {
        try await withAppleLanguages(["es-ES"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensión en castellano')
                browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "default_name": [
                    "message": "Default English String",
                    "description": "The default name in English.",
                ]
            ]

            let castilianSpanishMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensión en castellano",
                    "description": "The name of the extension in Castilian Spanish.",
                ]
            ]

            let latinAmericanSpanishMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Extensión en español",
                    "description": "The name of the extension in Latin American Spanish.",
                ]
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
                "_locales/es_ES/messages.json": castilianSpanishMessages,
                "_locales/es_419/messages.json": latinAmericanSpanishMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }

    @Test
    func i18nSwedishDefaultToEnglish() async throws {
        try await withAppleLanguages(["sv-SE"]) {
            let backgroundScript = """
                browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Default English Name', 'Should fall back to the default English name.')
                browser.test.assertEq(browser.i18n.getMessage('default_description'), 'Default English Description', 'Should fall back to the default English description.')

                browser.test.notifyPass()
                """

            let defaultMessages: [String: [String: String]] = [
                "extension_name": [
                    "message": "Default English Name",
                    "description": "The name of the extension in English.",
                ],
                "default_description": [
                    "message": "Default English Description",
                    "description": "A default description in English.",
                ],
            ]

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "_locales/en/messages.json": defaultMessages,
            ]

            try await loadAndRun(resources: resources)
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
