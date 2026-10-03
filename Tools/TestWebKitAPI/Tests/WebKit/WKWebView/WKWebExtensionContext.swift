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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionContextPrivate
private import WebKit_Private.WKWebExtensionControllerConfigurationPrivate
private import WebKit_Private.WKWebExtensionPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#else
private import UIKit
#endif

@MainActor
struct WKWebExtensionContextTests {
    @Test
    func defaultPermissionChecks() throws {
        // Extensions are expected to have no permissions or access by default.
        // Only Requested states should be reported with out any granting / denying.

        let exampleURL = try #require(URL(string: "https://example.com/"))
        let webkitURL = try #require(URL(string: "https://webkit.org/"))
        let unknownURL = try #require(URL(string: "https://unknown.com/"))

        var testManifestDictionary: [String: Any] = [
            "manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0", "permissions": [],
        ]
        var testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        var testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .unknown)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .unknown)
        #expect(testContext.permissionStatus(for: webkitURL) == .unknown)
        #expect(testContext.permissionStatus(for: unknownURL) == .unknown)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        testManifestDictionary["permissions"] = ["tabs", "https://*.example.com/*"]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .unknown)
        #expect(testContext.permissionStatus(for: unknownURL) == .unknown)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        testManifestDictionary["permissions"] = ["tabs", "<all_urls>"]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: unknownURL) == .requestedImplicitly)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        testManifestDictionary["permissions"] = ["tabs", "*://*/*"]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: unknownURL) == .requestedImplicitly)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        testManifestDictionary["manifest_version"] = 3
        testManifestDictionary["permissions"] = []
        testManifestDictionary["host_permissions"] = []
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .unknown)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .unknown)
        #expect(testContext.permissionStatus(for: webkitURL) == .unknown)
        #expect(testContext.permissionStatus(for: unknownURL) == .unknown)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        testManifestDictionary["permissions"] = ["tabs"]
        testManifestDictionary["host_permissions"] = ["https://*.example.com/*"]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .unknown)
        #expect(testContext.permissionStatus(for: unknownURL) == .unknown)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        testManifestDictionary["host_permissions"] = ["<all_urls>"]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: unknownURL) == .requestedImplicitly)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        testManifestDictionary["host_permissions"] = ["*://*/*"]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .requestedImplicitly)
        #expect(testContext.permissionStatus(for: unknownURL) == .requestedImplicitly)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)
    }

    @Test
    func permissionGranting() async throws {
        let exampleURL = try #require(URL(string: "https://example.com/"))
        let webkitURL = try #require(URL(string: "https://webkit.org/"))

        var testManifestDictionary: [String: Any] = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0"]
        testManifestDictionary["permissions"] = ["tabs", "https://*.example.com/*"]

        // Test defaults.
        let testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        let testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(!testContext.hasPermission(.tabs))
        #expect(!testContext.hasPermission(.cookies))
        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccessToAllHosts)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(!testContext.hasAccess(to: webkitURL))
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.tabs) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: WKWebExtension.Permission.cookies) == .unknown)
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedExplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .unknown)
        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        // Grant a specific permission.
        testContext.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.tabs)

        #expect(testContext.hasPermission(.tabs))
        #expect(testContext.grantedPermissions.count == 1)

        // Grant a specific URL.
        testContext.setPermissionStatus(.grantedExplicitly, for: exampleURL)

        #expect(testContext.hasAccess(to: exampleURL))
        #expect(testContext.grantedPermissionMatchPatterns.count == 1)

        // Deny a specific URL.
        testContext.setPermissionStatus(.deniedExplicitly, for: exampleURL)

        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 1)

        // Deny a specific permission.
        testContext.setPermissionStatus(.deniedExplicitly, for: WKWebExtension.Permission.tabs)

        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.deniedPermissions.count == 1)

        // Reset all permissions.
        testContext.setPermissionStatus(.unknown, for: exampleURL)
        testContext.setPermissionStatus(.unknown, for: WKWebExtension.Permission.tabs)

        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        // Grant the all URLs match pattern.
        testContext.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.MatchPattern.allURLs())

        #expect(testContext.grantedPermissionMatchPatterns.count == 1)
        #expect(testContext.hasAccess(to: exampleURL))
        #expect(testContext.permissionStatus(for: exampleURL) == .grantedImplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .grantedImplicitly)

        // Reset a specific URL (should do nothing).
        testContext.setPermissionStatus(.unknown, for: exampleURL)

        #expect(testContext.grantedPermissionMatchPatterns.count == 1)
        #expect(testContext.hasAccess(to: exampleURL))
        #expect(testContext.permissionStatus(for: exampleURL) == .grantedImplicitly)

        // Deny a specific URL (should do nothing).
        testContext.setPermissionStatus(.deniedExplicitly, for: exampleURL)

        #expect(testContext.grantedPermissionMatchPatterns.count == 1)
        #expect(testContext.deniedPermissionMatchPatterns.count == 1)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(testContext.permissionStatus(for: exampleURL) == .deniedExplicitly)
        #expect(testContext.permissionStatus(for: webkitURL) == .grantedImplicitly)

        // Reset all match patterns.
        testContext.grantedPermissionMatchPatterns = [:]
        testContext.deniedPermissionMatchPatterns = [:]

        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        // Mass grant with the permission setter.
        testContext.grantedPermissions = [.tabs: .distantFuture]

        #expect(testContext.hasPermission(.tabs))
        #expect(testContext.grantedPermissions.count == 1)

        // Mass deny with the permission setter.
        testContext.deniedPermissions = [.tabs: .distantFuture]

        #expect(!testContext.hasPermission(.tabs))
        #expect(testContext.deniedPermissions.count == 1)
        #expect(testContext.grantedPermissions.count == 0)

        // Mass grant with the permission setter again.
        testContext.grantedPermissions = [.tabs: .distantFuture]

        #expect(testContext.hasPermission(.tabs))
        #expect(testContext.grantedPermissions.count == 1)
        #expect(testContext.deniedPermissions.count == 0)

        // Mass grant with the match pattern setter.
        testContext.grantedPermissionMatchPatterns = [WKWebExtension.MatchPattern.allURLs(): .distantFuture]

        #expect(testContext.hasAccessToAllURLs)
        #expect(testContext.hasAccess(to: exampleURL))
        #expect(testContext.permissionStatus(for: exampleURL) == .grantedImplicitly)
        #expect(testContext.grantedPermissionMatchPatterns.count == 1)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        // Mass deny with the match pattern setter.
        testContext.deniedPermissionMatchPatterns = [WKWebExtension.MatchPattern.allURLs(): .distantFuture]

        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(testContext.deniedPermissionMatchPatterns.count == 1)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)

        // Mass grant with the match pattern setter again.
        testContext.grantedPermissionMatchPatterns = [WKWebExtension.MatchPattern.allURLs(): .distantFuture]

        #expect(testContext.hasAccessToAllURLs)
        #expect(testContext.hasAccess(to: exampleURL))
        #expect(testContext.permissionStatus(for: exampleURL) == .grantedImplicitly)
        #expect(testContext.grantedPermissionMatchPatterns.count == 1)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        // Reset all permissions.
        testContext.grantedPermissionMatchPatterns = [:]
        testContext.deniedPermissionMatchPatterns = [:]
        testContext.grantedPermissions = [:]
        testContext.deniedPermissions = [:]

        #expect(testContext.grantedPermissions.count == 0)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)
        #expect(testContext.deniedPermissions.count == 0)
        #expect(testContext.deniedPermissionMatchPatterns.count == 0)

        // Test granting a match pattern that expire in 2 seconds.
        testContext.setPermissionStatus(
            .grantedExplicitly,
            for: WKWebExtension.MatchPattern.allURLs(),
            expirationDate: Date(timeIntervalSinceNow: 2)
        )

        #expect(testContext.hasAccessToAllURLs)
        #expect(testContext.hasAccess(to: exampleURL))
        #expect(testContext.permissionStatus(for: exampleURL) == .grantedImplicitly)
        #expect(testContext.grantedPermissionMatchPatterns.count == 1)

        // Sleep until after the match pattern expires.
        try await Task.sleep(for: .seconds(3))

        #expect(!testContext.hasAccessToAllURLs)
        #expect(!testContext.hasAccess(to: exampleURL))
        #expect(testContext.permissionStatus(for: exampleURL) == .requestedExplicitly)
        #expect(testContext.grantedPermissionMatchPatterns.count == 0)

        // Test granting a permission that expire in 2 seconds.
        testContext.setPermissionStatus(
            .grantedExplicitly,
            for: WKWebExtension.Permission.tabs,
            expirationDate: Date(timeIntervalSinceNow: 2)
        )

        #expect(testContext.hasPermission(.tabs))
        #expect(testContext.grantedPermissions.count == 1)

        // Sleep until after the permission expires.
        try await Task.sleep(for: .seconds(3))

        #expect(!testContext.hasPermission(.tabs))
        #expect(testContext.grantedPermissions.count == 0)
    }

    @Test
    func contentScriptsParsing() throws {
        var testManifestDictionary: [String: Any] = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0"]

        testManifestDictionary["content_scripts"] = [["js": ["test.js", 1, ""], "css": [false, "test.css", ""], "matches": ["*://*/"]]]
        var testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        var testContext = WKWebExtensionContext(for: testExtension)

        let webkitURL = try #require(URL(string: "https://webkit.org/"))
        let exampleURL = try #require(URL(string: "https://example.com/"))

        #expect(testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [
            ["js": ["test.js", 1, ""], "css": [false, "test.css", ""], "matches": ["*://*/"], "exclude_matches": ["*://*.example.com/"]]
        ]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(testContext.hasInjectedContent(for: webkitURL))
        #expect(!testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [
            ["js": ["test.js", 1, ""], "css": [false, "test.css", ""], "matches": ["*://*.example.com/"]]
        ]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": ["*://*.example.com/"], "world": "MAIN"]]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [
            ["css": [false, "test.css", ""], "css_origin": "user", "matches": ["*://*.example.com/"]]
        ]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [
            ["css": [false, "test.css", ""], "css_origin": "author", "matches": ["*://*.example.com/"]]
        ]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))

        // Invalid cases

        testManifestDictionary["content_scripts"] = []
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(!testExtension.errors.isEmpty)
        #expect(!testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(!testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = ["invalid": true]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(!testExtension.errors.isEmpty)
        #expect(!testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(!testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": []]]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(!testExtension.errors.isEmpty)
        #expect(!testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(!testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": ["*://*.example.com/"], "run_at": "invalid"]]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(!testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": ["*://*.example.com/"], "world": "INVALID"]]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(!testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))

        testManifestDictionary["content_scripts"] = [
            ["css": [false, "test.css", ""], "css_origin": "bad", "matches": ["*://*.example.com/"]]
        ]
        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        #expect(!testExtension.errors.isEmpty)
        #expect(testContext.hasInjectedContent)
        #expect(!testContext.hasInjectedContent(for: webkitURL))
        #expect(testContext.hasInjectedContent(for: exampleURL))
    }

    @Test
    func optionsPageURLParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_page": "options.html",
        ]

        var testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        var testContext = WKWebExtensionContext(for: testExtension)
        var expectedOptionsURL = URL(string: "options.html", relativeTo: testContext.baseURL)?.absoluteURL
        #expect(testContext.optionsPageURL == expectedOptionsURL)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_page": 123,
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.optionsPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [
                "page": "options.html"
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        expectedOptionsURL = URL(string: "options.html", relativeTo: testContext.baseURL)?.absoluteURL
        #expect(testContext.optionsPageURL == expectedOptionsURL)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [
                "page": 123
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.optionsPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_page": "",
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.optionsPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [:],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.optionsPageURL == nil)
    }

    @Test
    func urlOverridesParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [
                "newtab": "newtab.html"
            ],
        ]

        var testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        var testContext = WKWebExtensionContext(for: testExtension)
        var expectedURL = URL(string: "newtab.html", relativeTo: testContext.baseURL)?.absoluteURL
        #expect(testContext.overrideNewTabPageURL == expectedURL)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [
                "newtab": ""
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.overrideNewTabPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [
                "newtab": 123
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.overrideNewTabPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [:],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.overrideNewTabPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [
                "newtab": "newtab.html"
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        expectedURL = URL(string: "newtab.html", relativeTo: testContext.baseURL)?.absoluteURL
        #expect(testContext.overrideNewTabPageURL == expectedURL)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [
                "newtab": 123
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.overrideNewTabPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [:],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.overrideNewTabPageURL == nil)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [
                "newtab": ""
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.overrideNewTabPageURL == nil)
    }

    @Test
    func commandsParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "description": "Test",
            "version": "1.0",

            "action": [
                "default_title": "Test Action"
            ],

            "commands": [
                "toggle-feature": [
                    "suggested_key": [
                        "default": "Alt+Shift+U",
                        "linux": "Shift+Ctrl+U",
                    ],
                    "description": "Send A Thing",
                ],
                "do-another-thing": [
                    "suggested_key": [
                        "default": "Alt+Shift+Y",
                        "mac": "Ctrl+Shift+Y",
                    ],
                    "description": "Find A Thing",
                ],
                "special-command": [
                    "suggested_key": [
                        "default": "Alt+F10"
                    ],
                    "description": "Do A Thing",
                ],
                "escape-command": [
                    "suggested_key": [
                        "ios": "MacCtrl+Down"
                    ],
                    "description": "Be A Thing",
                ],
                "unassigned-command": [
                    "description": "Maybe A Thing"
                ],
            ],
        ]

        var testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        var testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.webExtension.errors.isEmpty)
        #expect(testContext.commands.count == 6)

        var foundCommand: WKWebExtension.Command?

        for command in testContext.commands {
            if command.id == "toggle-feature" {
                foundCommand = command

                #expect(command.title == "Send A Thing")
                #if WTF_PLATFORM_MAC
                #expect(command.activationKey == "u")
                #expect(command.modifierFlags == [.option, .shift])
                #else
                #expect(command.activationKey == "U")
                #expect(command.modifierFlags == [.alternate, .shift])
                #endif
            } else if command.id == "do-another-thing" {
                #expect(command.title == "Find A Thing")
                #if WTF_PLATFORM_MAC
                #expect(command.activationKey == "y")
                #expect(command.modifierFlags == [.command, .shift])
                #else
                #expect(command.activationKey == "Y")
                #expect(command.modifierFlags == [.command, .shift])
                #endif
            } else if command.id == "special-command" {
                #expect(command.title == "Do A Thing")
                #expect(command.activationKey == "\u{F70D}")
                #if WTF_PLATFORM_MAC
                #expect(command.modifierFlags == .option)
                #else
                #expect(command.modifierFlags == .alternate)
                #endif
            } else if command.id == "escape-command" {
                #expect(command.title == "Be A Thing")
                #expect(command.activationKey == "\u{F701}")
                #expect(command.modifierFlags == .control)
            } else if command.id == "unassigned-command" {
                #expect(command.title == "Maybe A Thing")
                #expect(command.activationKey == nil)
                #expect(command.modifierFlags.isEmpty)
            } else if command.id == "_execute_action" {
                #expect(command.title == "Test Action")
                #expect(command.activationKey == nil)
                #expect(command.modifierFlags.isEmpty)
            }
        }

        let testCommand = try #require(foundCommand)

        testCommand.activationKey = nil

        #expect(testCommand.activationKey == nil)
        #expect(testCommand.modifierFlags.isEmpty)

        testCommand.activationKey = "\u{F70D}"

        #expect(testCommand.activationKey == "\u{F70D}")
        #if WTF_PLATFORM_MAC
        #expect(testCommand.modifierFlags == [.option, .shift])
        #else
        #expect(testCommand.modifierFlags == [.alternate, .shift])
        #endif

        testCommand.activationKey = "M"

        #if WTF_PLATFORM_MAC
        #expect(testCommand.activationKey == "m")
        #expect(testCommand.modifierFlags == [.option, .shift])
        #else
        #expect(testCommand.activationKey == "M")
        #expect(testCommand.modifierFlags == [.alternate, .shift])
        #endif

        testCommand.modifierFlags = []

        #expect(testCommand.activationKey == nil)
        #expect(testCommand.modifierFlags.isEmpty)

        testCommand.modifierFlags = [.command, .shift]

        #if WTF_PLATFORM_MAC
        #expect(testCommand.activationKey == "m")
        #else
        #expect(testCommand.activationKey == "M")
        #endif
        #expect(testCommand.modifierFlags == [.command, .shift])

        let activationKeyException = exceptionRaised(
            setting: testCommand,
            forKey: #keyPath(WKWebExtension.Command.activationKey),
            to: "F10"
        )
        #expect(activationKeyException == nil || activationKeyException == .internalInconsistencyException)
        #if WTF_PLATFORM_MAC
        #expect(testCommand.activationKey == "m")
        #else
        #expect(testCommand.activationKey == "M")
        #endif

        let modifierFlagsException = exceptionRaised(
            setting: testCommand,
            forKey: #keyPath(WKWebExtension.Command.modifierFlags),
            to: 1 << 16
        )
        #expect(modifierFlagsException == nil || modifierFlagsException == .internalInconsistencyException)
        #expect(testCommand.modifierFlags == [.command, .shift])

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "commands": [:],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.webExtension.errors.isEmpty)
        #expect(testContext.commands.count == 0)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "commands": [
                "command-without-description": [
                    "suggested_key": [
                        "default": "Ctrl+Shift+X"
                    ]
                ]
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.errors.count == 1)
        #expect(testContext.commands.count == 0)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "commands": [
                "Invalid"
            ],
        ]

        testExtension = try #require(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)
        #expect(testContext.errors.count == 1)
        #expect(testContext.commands.count == 0)
    }

    @Test
    func loadNonExistentImage() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            const img = new Image()
            img.src = 'non-existent-image.png'
            img.onload = () => {
              browser.test.notifyFail('Image should not load successfully')
            }
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtension.Error.Code.resourceNotFound.rawValue)
        #expect(
            error.localizedDescription == "Unable to find “non-existent-image.png” in the extension’s resources. It is an invalid path."
        )
    }

    @Test
    func topLevelThrowInModuleBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = "throw new Error('Top level module error')"

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Error: Top level module error (background.js:1:16)")
    }

    @Test
    func referenceErrorInBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = "undeclaredVariable.foo"

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "ReferenceError: Can't find variable: undeclaredVariable (background.js:1:19)")
    }

    @Test
    func callingMissingBrowserAPIInBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = "browser.runtime.nonExistentMethod()"

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)

        let expectedDescription =
            "TypeError: browser.runtime.nonExistentMethod is not a function. (In 'browser.runtime.nonExistentMethod()', "
            + "'browser.runtime.nonExistentMethod' is undefined) (background.js:1:34)"
        #expect(error.localizedDescription == expectedDescription)
    }

    @Test
    func uncaughtScriptErrorInBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        // Use setTimeout so the throw happens as a runtime uncaught exception, not a module evaluation rejection.
        let backgroundScript = "setTimeout(() => { throw new Error('Test uncaught error') }, 0)"

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Error: Test uncaught error (background.js:1:58)")
    }

    @Test
    func unhandledPromiseRejectionInBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = "Promise.reject(new Error('Test rejection'))"

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Error: Test rejection")
    }

    @Test
    func uncaughtScriptErrorInServiceWorkerBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "service_worker": "background.js"
            ],
        ]

        let backgroundScript = "setTimeout(() => { throw new Error('Service worker uncaught error') }, 0)"

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Error: Service worker uncaught error (background.js:1:68)")
    }

    @Test
    func unhandledPromiseRejectionInServiceWorkerBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "service_worker": "background.js"
            ],
        ]

        let backgroundScript = "Promise.reject(new Error('Service worker rejection'))"

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Error: Service worker rejection")
    }

    @Test
    func syntaxErrorInBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],
        ]

        // A bare syntax error: fails at parse time, so exception->stack() will be empty.
        // Validates that the source URL fallback via error.sourceURL is correctly reported.
        let backgroundScript = ")("

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "SyntaxError: Unexpected token ')' (background.js:1)")
    }

    @Test
    func noErrorForCaughtExceptionsInBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            // Caught synchronous exception — should not populate errors.
            try { throw new Error('Caught error') } catch (e) {}

            // Handled promise rejection — should not populate errors.
            Promise.reject(new Error('Handled rejection')).catch(() => {})

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)
        try await manager.run()

        #expect(context.errors.count == 0)
    }

    @Test
    func uncaughtScriptErrorInContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                ]
            ],
        ]

        let contentScript = "throw new Error('Content script error')"

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["content.js": contentScript])
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForContextError()

            #expect(context.errors.count == 1)

            let error = try #require(context.errors.first) as NSError
            #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
            #expect(error.localizedDescription == "Error: Content script error (content.js:1:40)")
        }
    }

    @Test
    func uncaughtScriptErrorInMainWorldContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                    "world": "MAIN",
                ]
            ],
        ]

        let contentScript = "throw new Error('Main world error')"

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["content.js": contentScript])
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForContextError()

            #expect(context.errors.count == 1)

            let error = try #require(context.errors.first) as NSError
            #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
            #expect(error.localizedDescription == "Error: Main world error (content.js:1:36)")
        }
    }

    @Test
    func pageScriptErrorNotReportedToExtension() async throws {
        // Verify that errors thrown by page scripts (not extension scripts) are not reported to the extension,
        // even when a main-world content script is active on the same page.
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>throw new Error('Page error')</script>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                    "world": "MAIN",
                ]
            ],
        ]

        // The content script itself does not throw; only the page script does.
        let contentScript = "let result = 2 + 2;"

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["content.js": contentScript])
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await Task.sleep(for: .seconds(3))
            try manager.checkCollectedFailures()

            #expect(context.errors.isEmpty)
        }
    }

    @Test
    func consoleErrorDoesNotEvaluateArgumentsTwice() async throws {
        // Verify that console.error() does not call toString() on its arguments more than once when a
        // main-world content script has registered script error callbacks. https://webkit.org/b/314458

        let pageMarkup = """
            <script>var arg1Count = 0; var arg2Count = 0; console.error({ toString() { ++arg1Count; return 'error'; } }, \
            { toString() { ++arg2Count; return 'extra'; } });</script>
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                pageMarkup
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                    "world": "MAIN",
                ]
            ],
        ]

        // Content script runs at document_end (after page scripts), reads both toString call counts,
        // and reports them. The first argument should be evaluated exactly once (for the ConsoleMessage
        // text); extra arguments should never be evaluated for the error callback.
        let contentScript = "browser.test.sendMessage('counts', [window.arg1Count, window.arg2Count])"

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["content.js": contentScript])
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            let counts = try await manager.waitForTestMessage("counts")
            let result = try #require(counts as? [Int])

            #expect(result == [1, 0])
            #expect(context.errors.isEmpty)
        }
    }

    @Test
    func uncaughtScriptErrorInEventListener() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "action": [:],
        ]

        let backgroundScript = """
            browser.action.onClicked.addListener((tab) => {
              throw new Error('Error in event listener')
            })
            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Ready")

        context.performAction(for: manager.defaultTab)

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Error: Error in event listener (background.js:2:18)")
    }

    @Test
    func topLevelThrowInPopup() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "action": [
                "default_popup": "popup.html"
            ],
        ]

        let popupScript = """
            browser.test.sendMessage('Ready')
            throw new Error('Popup error')
            """

        let resources: [String: Any] = [
            "popup.html": "<script type='module' src='popup.js'></script>",
            "popup.js": popupScript,
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: resources)
        let context = try #require(manager.context)

        let action = try await manager.nextPresentedAction {
            context.performAction(for: manager.defaultTab)
        }

        #expect(action.popupWebView != nil)

        try await manager.waitForTestMessage("Ready")

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Error: Popup error (popup.js:2:16)")
    }

    @Test
    func consoleErrorReportedNotLogOrWarn() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            console.log('This is a log message')
            console.warn('This is a warning message')
            console.error('This is an error message')
            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Ready")

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "This is an error message (background.js:3:14)")
    }

    @Test
    func consoleAssertWithMessage() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            console.assert(true, 'This should not appear')
            console.assert(false, 'Something went wrong')
            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Ready")

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "Something went wrong (background.js:2:15)")
    }

    @Test
    func consoleAssertWithoutMessage() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let backgroundScript = """
            console.assert(true)
            console.assert(false)
            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        let context = try #require(manager.context)

        try await manager.waitForTestMessage("Ready")

        try await manager.waitForContextError()

        #expect(context.errors.count == 1)

        let error = try #require(context.errors.first) as NSError
        #expect(error.code == WKWebExtensionContextErrorScriptExecutionError.rawValue)
        #expect(error.localizedDescription == "(background.js:2:15)")
    }

    @Test
    func cleanUpOldOriginDataAfterMigration() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,
            "name": "Test Extension",
            "description": "Test",
            "version": "1.0",
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]
        let backgroundScript = """
            localStorage.setItem('testkey', 'testvalue')
            browser.test.sendMessage('Ready')
            """

        let manager = parseWebExtension(manifest: manifest, resources: ["background.js": backgroundScript], configuration: ._temporary())
        let context = try #require(manager.context)
        context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        manager.load()
        try await manager.waitForTestMessage("Ready")

        let oldOriginURL = context.baseURL
        let dataStore: WKWebsiteDataStore = manager.controller.configuration.defaultWebsiteDataStore

        let oldLocalStorageDirectoryPath = await dataStore._originDirectory(
            forTesting: oldOriginURL,
            topOrigin: oldOriginURL,
            type: WKWebsiteDataTypeLocalStorage
        )
        #expect(FileManager.default.fileExists(atPath: oldLocalStorageDirectoryPath))

        let oldServiceWorkerDirectoryPath = await dataStore._originDirectory(
            forTesting: oldOriginURL,
            topOrigin: oldOriginURL,
            type: WKWebsiteDataTypeServiceWorkerRegistrations
        )
        #expect(!FileManager.default.fileExists(atPath: oldServiceWorkerDirectoryPath))

        try manager.controller.unload(context)
        manager.context = nil

        let readLocalStorageBackgroundScript = """
            browser.test.assertEq(localStorage.getItem('testkey'), 'testvalue')
            browser.test.sendMessage('Migrated')
            """

        let newExtension = try #require(
            WKWebExtension(manifestDictionary: manifest, resources: ["background.js": readLocalStorageBackgroundScript])
        )
        let newContext = WKWebExtensionContext(for: newExtension)
        newContext.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"
        #expect(newContext.baseURL != oldOriginURL)

        try manager.controller.load(newContext)

        manager.context = newContext
        try await manager.waitForTestMessage("Migrated")

        #expect(!FileManager.default.fileExists(atPath: oldLocalStorageDirectoryPath))
        #expect(!FileManager.default.fileExists(atPath: oldServiceWorkerDirectoryPath))

        let oldOriginDirectory = (oldLocalStorageDirectoryPath as NSString).deletingLastPathComponent
        #expect(!FileManager.default.fileExists(atPath: oldOriginDirectory))
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
