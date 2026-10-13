// Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
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

@MainActor
struct WKWebExtensionControllerTests {
    @Test
    func configuration() {
        var testController = WKWebExtensionController()
        #expect(testController.configuration.isPersistent)
        #expect(testController.configuration.identifier == nil)

        testController = WKWebExtensionController(configuration: .nonPersistent())
        #expect(!testController.configuration.isPersistent)
        #expect(testController.configuration.identifier == nil)

        let identifier = UUID()
        let configuration = WKWebExtensionController.Configuration(identifier: identifier)

        testController = WKWebExtensionController(configuration: configuration)
        #expect(testController.configuration.isPersistent)
        #expect(testController.configuration.identifier == identifier)
    }

    @Test
    func loadingAndUnloadingContexts() throws {
        let testController = WKWebExtensionController(configuration: .nonPersistent())

        #expect(testController.extensions.count == 0)
        #expect(testController.extensionContexts.count == 0)

        #if WTF_PLATFORM_IOS_FAMILY
        let invalidPersistenceExtension = try #require(
            WKWebExtension(
                manifestDictionary: [
                    "manifest_version": 2,
                    "name": "Invalid Persistence",
                    "description": "Invalid Persistence",
                    "version": "1.0",
                    "background": ["page": "background.html", "persistent": true],
                ]
            )
        )
        let invalidPersistenceContext = WKWebExtensionContext(for: invalidPersistenceExtension)

        #expect(!invalidPersistenceContext.isLoaded)
        let invalidPersistenceError = #expect(throws: WKWebExtension.Error.self) {
            try testController.load(invalidPersistenceContext)
        }
        #expect(invalidPersistenceError?.code == .invalidBackgroundPersistence)

        #expect(testController.extensions.count == 0)
        #expect(testController.extensionContexts.count == 0)
        #endif

        let testExtensionOne = try #require(
            WKWebExtension(manifestDictionary: ["manifest_version": 2, "name": "Test One", "description": "Test One", "version": "1.0"])
        )
        let testContextOne = WKWebExtensionContext(for: testExtensionOne)

        #expect(testExtensionOne.errors.count == 0)
        #expect(!testContextOne.isLoaded)
        #expect(testController.extensionContext(for: testExtensionOne) == nil)

        let testExtensionTwo = try #require(
            WKWebExtension(manifestDictionary: ["manifest_version": 2, "name": "Test Two", "description": "Test Two", "version": "1.0"])
        )
        let testContextTwo = WKWebExtensionContext(for: testExtensionTwo)

        #expect(testExtensionTwo.errors.count == 0)
        #expect(!testContextTwo.isLoaded)
        #expect(testController.extensionContext(for: testExtensionTwo) == nil)

        try testController.load(testContextOne)

        #expect(testExtensionOne.errors.count == 0)
        #expect(testContextOne.isLoaded)

        #expect(testController.extensions.count == 1)
        #expect(testController.extensionContexts.count == 1)

        let alreadyLoadedError = #expect(throws: WKWebExtensionContext.Error.self) {
            try testController.load(testContextOne)
        }
        #expect(alreadyLoadedError?.code == .alreadyLoaded)

        #expect(testExtensionOne.errors.count == 0)
        #expect(testContextOne.isLoaded)

        #expect(testController.extensions.count == 1)
        #expect(testController.extensionContexts.count == 1)

        try testController.load(testContextTwo)

        #expect(testExtensionTwo.errors.count == 0)
        #expect(testContextTwo.isLoaded)

        #expect(testController.extensions.count == 2)
        #expect(testController.extensionContexts.count == 2)

        try testController.unload(testContextOne)

        #expect(testExtensionTwo.errors.count == 0)
        #expect(!testContextOne.isLoaded)

        #expect(testController.extensions.count == 1)
        #expect(testController.extensionContexts.count == 1)

        try testController.unload(testContextTwo)

        #expect(testExtensionTwo.errors.count == 0)
        #expect(!testContextTwo.isLoaded)

        #expect(testController.extensions.count == 0)
        #expect(testController.extensionContexts.count == 0)

        let notLoadedError = #expect(throws: WKWebExtensionContext.Error.self) {
            try testController.unload(testContextOne)
        }
        #expect(notLoadedError?.code == .notLoaded)

        #expect(testExtensionTwo.errors.count == 0)
        #expect(!testContextOne.isLoaded)
    }

    @Test
    func backgroundPageLoading() async throws {
        let resources: [String: Any] = [
            "background.html": "<body>Hello world!</body>",
            "background.js": "console.log('Hello World!')",
        ]

        var manifest: [String: Any] = [
            "manifest_version": 2,
            "name": "Test One",
            "description": "Test One",
            "version": "1.0",
            "background": ["page": "background.html", "persistent": false],
        ]

        var testExtension = try #require(WKWebExtension(manifestDictionary: manifest, resources: resources))
        var testContext = WKWebExtensionContext(for: testExtension)
        let testController = WKWebExtensionController(configuration: .nonPersistent())

        #expect(testExtension.errors.isEmpty)

        try testController.load(testContext)

        // Wait for the background to load.
        try await Task.sleep(for: .seconds(4))

        // No errors means success.
        #expect(testExtension.errors.isEmpty)

        try testController.unload(testContext)

        #expect(testExtension.errors.isEmpty)

        manifest["background"] = ["service_worker": "background.js"]

        testExtension = try #require(WKWebExtension(manifestDictionary: manifest, resources: resources))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)

        try testController.load(testContext)

        // Wait for the background to load.
        try await Task.sleep(for: .seconds(4))

        // No errors means success.
        #expect(testExtension.errors.isEmpty)

        try testController.unload(testContext)

        #expect(testExtension.errors.isEmpty)

        WKWebExtension.MatchPattern.registerCustomURLScheme("test-extension")
        testContext.baseURL = try #require(URL(string: "test-extension://aaabbbcccddd"))

        try testController.load(testContext)

        // Wait for the background to load.
        try await Task.sleep(for: .seconds(4))

        // No errors means success.
        #expect(testExtension.errors.isEmpty)

        try testController.unload(testContext)

        #expect(testExtension.errors.isEmpty)
    }

    @Test
    func backgroundPageWithModulesLoading() async throws {
        let resources: [String: Any] = [
            "main.js": "import { x } from './exports.js'; x;",
            "exports.js": "const x = 805; export { x };",
        ]

        var manifest: [String: Any] = [
            "manifest_version": 2,
            "name": "Test One",
            "description": "Test One",
            "version": "1.0",
            "background": ["scripts": ["main.js", "exports.js"], "type": "module", "persistent": false],
        ]

        var testExtension = try #require(WKWebExtension(manifestDictionary: manifest, resources: resources))
        var testContext = WKWebExtensionContext(for: testExtension)
        let testController = WKWebExtensionController(configuration: .nonPersistent())
        defer {
            for context in testController.extensionContexts {
                try? testController.unload(context)
            }
        }

        #expect(testExtension.errors.isEmpty)

        try testController.load(testContext)

        // Wait for the background to load.
        try await Task.sleep(for: .seconds(4))

        // No errors means success.
        #expect(testExtension.errors.isEmpty)

        try testController.unload(testContext)

        #expect(testExtension.errors.isEmpty)

        manifest["background"] = ["service_worker": "main.js", "type": "module"]

        testExtension = try #require(WKWebExtension(manifestDictionary: manifest, resources: resources))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)

        try testController.load(testContext)

        // Wait for the background to load.
        try await Task.sleep(for: .seconds(4))

        // No errors means success.
        #expect(testExtension.errors.isEmpty)
    }

    @Test
    func backgroundWithServiceWorkerPreferredEnvironment() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "preferred_environment": ["service_worker", "document"],
                "service_worker": "service_worker.js",
                "scripts": ["background.js"],
                "page": "background.html",
            ],
        ]

        let serviceWorkerScript = """
            browser.test.assertTrue('ServiceWorkerGlobalScope' in self && self instanceof ServiceWorkerGlobalScope, 'Global scope should be ServiceWorkerGlobalScope');

            browser.test.notifyPass()
            """

        let backgroundScript = """
            browser.test.notifyFail('This background script should not be used')
            """

        let resources: [String: Any] = [
            "service_worker.js": serviceWorkerScript,
            "background.js": backgroundScript,
            "background.html": "<script src='background.js'></script>",
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        try await manager.run()
    }

    @Test
    func backgroundWithPageDocumentPreferredEnvironment() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "preferred_environment": ["document", "service_worker"],
                "service_worker": "service_worker.js",
                "scripts": ["other-background.js"],
                "page": "background.html",
            ],
        ]

        let serviceWorkerScript = """
            browser.test.notifyFail('Service worker should not be used')
            """

        let notUsedbackgroundScript = """
            browser.test.notifyFail('This background script should not be used')
            """

        let backgroundScript = """
            browser.test.assertTrue('Window' in self && self instanceof Window, 'Global scope should be Window')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "service_worker.js": serviceWorkerScript,
            "other-background.js": notUsedbackgroundScript,
            "background.js": backgroundScript,
            "background.html": "<script src='background.js'></script>",
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        try await manager.run()
    }

    @Test
    func backgroundWithScriptsDocumentPreferredEnvironment() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "preferred_environment": "document",
                "scripts": ["background.js"],
            ],
        ]

        let serviceWorkerScript = """
            browser.test.notifyFail('Service worker should not be used')
            """

        let backgroundScript = """
            browser.test.assertTrue('Window' in self && self instanceof Window, 'Global scope should be Window')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "service_worker.js": serviceWorkerScript,
            "background.js": backgroundScript,
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        try await manager.run()
    }

    @Test
    func backgroundWithMultipleDocumentModuleScripts() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "preferred_environment": "document",
                "scripts": ["module1.js", "module2.js"],
                "type": "module",
            ],
        ]

        let module1 = """
            self.testValue = 'Test value set in Module 1';
            """

        let module2 = """
            import { valueFromModule3 } from './module3.js'

            browser.test.assertEq(self.testValue, 'Test value set in Module 1', 'Module 1 value should be accessible')
            browser.test.assertEq(valueFromModule3, 'Value from Module 3', 'Value from Module 3 should be accessible')

            browser.test.notifyPass();
            """

        let module3 = """
            export const valueFromModule3 = 'Value from Module 3';
            """

        let resources: [String: Any] = [
            "module1.js": module1,
            "module2.js": module2,
            "module3.js": module3,
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        try await manager.run()
    }

    @Test
    func backgroundWithMultipleServiceWorkerScripts() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "preferred_environment": "service_worker",
                "scripts": ["script1.js", "script2.js"],
            ],
        ]

        let script1 = """
            self.testValue = 'Test value set in Script 1'
            """

        let script2 = """
            browser.test.assertEq(self.testValue, 'Test value set in Script 1')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "script1.js": script1,
            "script2.js": script2,
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        try await manager.run()
    }

    @Test
    func backgroundWithMultipleServiceWorkerModuleScripts() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "preferred_environment": "service_worker",
                "scripts": ["module1.js", "module2.js"],
                "type": "module",
            ],
        ]

        let module1 = """
            self.testValue = 'Test value set in Module 1'
            """

        let module2 = """
            import { valueFromModule3 } from './module3.js'

            browser.test.assertEq(self.testValue, 'Test value set in Module 1')
            browser.test.assertEq(valueFromModule3, 'Value from Module 3')

            browser.test.notifyPass()
            """

        let module3 = """
            export const valueFromModule3 = 'Value from Module 3'
            """

        let resources: [String: Any] = [
            "module1.js": module1,
            "module2.js": module2,
            "module3.js": module3,
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        try await manager.run()
    }

    @Test
    func contentScriptLoading() async throws {
        var server = try HTTPServer(protocol: .https) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "Hello World"
            }
        }

        let manifest: [String: Any] = ["manifest_version": 3, "content_scripts": [["js": ["content.js"], "matches": ["*://localhost/*"]]]]

        let contentScript = """
            // Exposed to content scripts
            browser.test.assertEq(typeof browser.runtime.id, 'string')
            browser.test.assertEq(typeof browser.runtime.getManifest(), 'object')
            browser.test.assertEq(typeof browser.runtime.getURL(''), 'string')

            // Not exposed to content scripts
            browser.test.assertEq(browser.runtime.getPlatformInfo, undefined)
            browser.test.assertEq(browser.runtime.lastError, undefined)

            // Finish
            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["content.js": contentScript])
            let context = try #require(manager.context)

            let matchPattern = try WKWebExtension.MatchPattern(string: "*://localhost/*")
            context.setPermissionStatus(.grantedExplicitly, for: matchPattern)

            let webViewConfiguration = WKWebViewConfiguration()
            webViewConfiguration.webExtensionController = manager.controller

            let webView = WKWebView(frame: .zero, configuration: webViewConfiguration)
            let navigationDelegate = TestNavigationDelegate()

            var authenticationMethods: [String] = []
            navigationDelegate.didReceiveAuthenticationChallenge = { _, challenge, callback in
                authenticationMethods.append(challenge.protectionSpace.authenticationMethod)
                callback(.useCredential, challenge.protectionSpace.serverTrust.map { URLCredential(trust: $0) })
            }

            webView.navigationDelegate = navigationDelegate

            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()

            #expect(authenticationMethods.allSatisfy { $0 == NSURLAuthenticationMethodServerTrust })
        }
    }

    @Test
    func cssUserOrigin() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body style='color: red'></body>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "css": ["style.css"],
                    "js": ["content.js"],
                    "css_origin": "user",
                ]
            ],
        ]

        let styleSheet = "body { color: green !important }"

        let contentScript = """
            let computedColor = window.getComputedStyle(document.body).color
            browser.test.assertEq(computedColor, 'rgb(0, 128, 0)', 'Color should be green')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "style.css": styleSheet,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func cssAuthorOrigin() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<style> body { color: red } </style>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "css": ["style.css"],
                    "js": ["content.js"],
                    "css_origin": "author",
                ]
            ],
        ]

        let styleSheet = "body { color: green !important }"

        let contentScript = """
            let computedColor = getComputedStyle(document.body).color
            browser.test.assertEq(computedColor, 'rgb(0, 128, 0)', 'Color should be green')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "style.css": styleSheet,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func contentSecurityPolicyV2BlockingImageLoad() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/image.svg", headerFields: ["Content-Type": "image/svg+xml"]) {
                "<svg xmlns='http://www.w3.org/2000/svg'></svg>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_security_policy": "script-src 'self'; img-src 'none'",
        ]

        try await server.run { configuration in
            let backgroundScript = """
                var img = document.createElement('img')
                img.src = '\(configuration.localhostAddress.absoluteString)image.svg'

                img.onerror = () => {
                  browser.test.notifyPass()
                }

                img.onload = () => {
                  browser.test.notifyFail('The image should not load')
                }

                document.body.appendChild(img)
                """

            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
            try await manager.run()
        }
    }

    @Test
    func contentSecurityPolicyV3BlockingImageLoad() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/image.svg", headerFields: ["Content-Type": "image/svg+xml"]) {
                "<svg xmlns='http://www.w3.org/2000/svg'></svg>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_security_policy": [
                "extension_pages": "script-src 'self'; img-src 'none'"
            ],
        ]

        try await server.run { configuration in
            let backgroundScript = """
                var img = document.createElement('img')
                img.src = '\(configuration.localhostAddress.absoluteString)image.svg'

                img.onerror = () => {
                  browser.test.notifyPass()
                }

                img.onload = () => {
                  browser.test.notifyFail('The image should not load')
                }

                document.body.appendChild(img)
                """

            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
            try await manager.run()
        }
    }

    @Test
    func webAccessibleResources() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let contentScript = """
            var imgGood = document.createElement('img')
            imgGood.src = browser.runtime.getURL('good.svg')

            var imgBad = document.createElement('img')
            imgBad.src = browser.runtime.getURL('bad.svg')

            var goodLoaded = false
            var badFailed = false

            imgGood.onload = () => {
              goodLoaded = true
              if (badFailed)
                browser.test.notifyPass()
            }

            imgGood.onerror = () => {
              browser.test.notifyFail('The good image should load')
            }

            imgBad.onload = () => {
              browser.test.notifyFail('The bad image should not load')
            }

            imgBad.onerror = () => {
              badFailed = true
              if (goodLoaded)
                browser.test.notifyPass()
            }

            document.body.appendChild(imgGood)
            document.body.appendChild(imgBad)
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "web_accessible_resources": [
                [
                    "resources": ["g*.svg"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let resources: [String: Any] = [
            "content.js": contentScript,
            "good.svg": "<svg xmlns='http://www.w3.org/2000/svg'></svg>",
            "bad.svg": "<svg xmlns='http://www.w3.org/2000/svg'></svg>",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func webAccessibleResourcesWithLeadingSlash() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let contentScript = """
            var img = document.createElement('img')
            img.src = browser.runtime.getURL('img.svg')

            img.onload = () => {
              browser.test.notifyPass()
            }

            img.onerror = () => {
              browser.test.notifyFail('The image should load')
            }

            document.body.appendChild(img)
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "web_accessible_resources": [
                [
                    "resources": ["/img.svg"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let resources: [String: Any] = [
            "content.js": contentScript,
            "img.svg": "<svg xmlns='http://www.w3.org/2000/svg'></svg>",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func webAccessibleResourceInSubframeFromAboutBlank() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let extensionManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Runtime Test",
            "description": "Runtime Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "web_accessible_resources": [
                [
                    "resources": ["*.html"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let iframeScript = """
            browser.test.notifyPass()
            """

        let contentScript = """
            (function() {
              const iframe = document.createElement('iframe')
              document.documentElement.appendChild(iframe)
              iframe.contentWindow.location = new URL(browser.runtime.getURL('extension-frame.html')).href
            })()
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
            "extension-frame.js": iframeScript,
            "extension-frame.html": "<script type='module' src='extension-frame.js'></script>",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: extensionManifest, resources: resources)
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func webAccessibleResourcesV2() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let contentScript = """
            var imgGood = document.createElement('img')
            imgGood.src = browser.runtime.getURL('good.svg')

            var imgBad = document.createElement('img')
            imgBad.src = browser.runtime.getURL('bad.svg')

            var goodLoaded = false
            var badFailed = false

            imgGood.onload = () => {
              goodLoaded = true
              if (badFailed)
                browser.test.notifyPass()
            }

            imgGood.onerror = () => {
              browser.test.notifyFail('The good image should load')
            }

            imgBad.onload = () => {
              browser.test.notifyFail('The bad image should not load')
            }

            imgBad.onerror = () => {
              badFailed = true
              if (goodLoaded)
                browser.test.notifyPass()
            }

            document.body.appendChild(imgGood)
            document.body.appendChild(imgBad)
            """

        let manifest: [String: Any] = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],

            "web_accessible_resources": ["good.svg"],
        ]

        let resources: [String: Any] = [
            "content.js": contentScript,
            "good.svg": "<svg xmlns='http://www.w3.org/2000/svg'></svg>",
            "bad.svg": "<svg xmlns='http://www.w3.org/2000/svg'></svg>",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
