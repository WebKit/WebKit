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
private import WebKit_Private.WKWebExtensionPrivate

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#else
private import UIKit
#endif

@MainActor
struct WKWebExtensionTests {
    private func matchingError(_ errors: [any Error], code: WKWebExtension.Error.Code) -> (any Error)? {
        errors.first { error in
            let error = error as NSError
            return error.domain == WKWebExtension.errorDomain && error.code == code.rawValue
        }
    }

    private func matchPattern(
        _ string: String,
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column
    ) throws -> WKWebExtension.MatchPattern {
        try requireNonNil(
            WKWebExtension.MatchPattern.cachedPattern(string: string),
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column
        )
    }

    /// Loads an extension that sends 'Load Tab', then loads a page from a local server in its default tab and waits for
    /// the extension to finish.
    private func loadAndRunWithTestPage(
        _ webExtension: WKWebExtension,
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column
    ) async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        try await server.run { configuration in
            let manager = parseWebExtension(webExtension)

            manager.load()
            try await manager.waitForTestMessage("Load Tab")

            let context = try requireNonNil(manager.context, fileID: fileID, filePath: filePath, line: line, column: column)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try requireNonNil(manager.defaultTab?.webView, fileID: fileID, filePath: filePath, line: line, column: column)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func basicManifestParsing() {
        // `manifest` bridges a nil dictionary to an empty one, so read it through Objective-C to tell the two apart.
        func parse(_ manifestString: String) -> NSDictionary? {
            WKWebExtension(resources: ["manifest.json": manifestString])?.value(forKey: "manifest") as? NSDictionary
        }

        expectNil(parse(""))
        expectNil(parse("[]"))

        expectEqual(parse("{}"), [:] as NSDictionary)
        expectEqual(parse("{ \"manifest_version\": 1 }"), ["manifest_version": 1] as NSDictionary)
        expectEqual(parse("{ \"manifest_version\": 4 }"), ["manifest_version": 4] as NSDictionary)
        expectEqual(parse("{ \"manifest_version\": -2 }"), ["manifest_version": -2] as NSDictionary)
        expectEqual(parse("{ \"manifest_version\": 0 }"), ["manifest_version": 0] as NSDictionary)
        expectEqual(parse("{ \"manifest_version\": \"1\" }"), ["manifest_version": "1"] as NSDictionary)

        expectEqual(parse("{ \"manifest_version\": 2 }"), ["manifest_version": 2] as NSDictionary)
        expectEqual(parse("{ \"manifest_version\": 3 }"), ["manifest_version": 3] as NSDictionary)

        var testResult: NSDictionary = ["manifest_version": 2, "name": "Test", "version": "1.0"]
        expectEqual(parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\" }"), testResult)

        testResult = ["manifest_version": 3, "name": "Test", "version": "1.0"]
        expectEqual(parse("{ \"manifest_version\": 3, \"name\": \"Test\", \"version\": \"1.0\" }"), testResult)
    }

    @Test
    func displayStringParsing() throws {
        var testManifestDictionary: [String: Any] = ["manifest_version": 2]
        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNil(testExtension.displayName)
        expectNil(testExtension.displayShortName)
        expectNil(testExtension.displayVersion)
        expectNil(testExtension.version)
        expectNil(testExtension.displayDescription)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))

        testManifestDictionary["name"] = "Test"
        testManifestDictionary["version"] = "1.0"
        testManifestDictionary["description"] = "Test description."
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.displayName, "Test")
        expectEqual(testExtension.displayShortName, "Test")
        expectEqual(testExtension.displayVersion, "1.0")
        expectEqual(testExtension.version, "1.0")
        expectEqual(testExtension.displayDescription, "Test description.")
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["short_name"] = "Tst"
        testManifestDictionary["version_name"] = "1.0 Final"
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.displayName, "Test")
        expectEqual(testExtension.displayShortName, "Tst")
        expectEqual(testExtension.displayVersion, "1.0 Final")
        expectEqual(testExtension.version, "1.0")
        expectEqual(testExtension.displayDescription, "Test description.")
        expectTrue(testExtension.errors.isEmpty)
    }

    @Test
    func defaultLocaleParsing() throws {
        // Test no default locale.
        var testManifestDictionary: [String: Any] = ["manifest_version": 3, "name": "Test", "version": "1.0", "description": "Test"]
        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNil(testExtension.defaultLocale)
        expectTrue(testExtension.errors.isEmpty)

        // Test English without a region.
        testManifestDictionary["default_locale"] = "en"
        testExtension = try requireNonNil(
            WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["_locales/en/messages.json": "{}"])
        )
        expectTrue(testExtension.errors.isEmpty)

        var defaultLocale = testExtension.defaultLocale as NSLocale?
        expectNotNil(defaultLocale)
        expectEqual(defaultLocale?.localeIdentifier, "en")
        expectEqual(defaultLocale?.languageCode, "en")
        expectNil(defaultLocale?.countryCode)

        // Test English with US region.
        testManifestDictionary["default_locale"] = "en_US"
        testExtension = try requireNonNil(
            WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["_locales/en_US/messages.json": "{}"])
        )
        expectTrue(testExtension.errors.isEmpty)

        defaultLocale = testExtension.defaultLocale as NSLocale?
        expectNotNil(defaultLocale)
        expectEqual(defaultLocale?.localeIdentifier, "en_US")
        expectEqual(defaultLocale?.languageCode, "en")
        expectEqual(defaultLocale?.countryCode, "US")

        // Test Simplified Chinese.
        testManifestDictionary["default_locale"] = "zh_CN"
        testExtension = try requireNonNil(
            WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["_locales/zh_CN/messages.json": "{}"])
        )
        expectTrue(testExtension.errors.isEmpty)

        defaultLocale = testExtension.defaultLocale as NSLocale?
        expectNotNil(defaultLocale)
        expectEqual(defaultLocale?.localeIdentifier, "zh_CN")
        expectEqual(defaultLocale?.languageCode, "zh")
        expectEqual(defaultLocale?.countryCode, "CN")

        // Test invalid locale.
        testManifestDictionary["default_locale"] = "invalid"
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNil(testExtension.defaultLocale)
        expectEqual(testExtension.errors.count, 1)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    @Test
    func displayStringParsingWithLocalization() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 2,
            "default_locale": "en_US",

            "name": "__MSG_default_name__",
            "short_name": "__MSG_regional_name__",
            "version": "1.0",
            "description": "__MSG_default_description__",
        ]

        let defaultMessages = [
            "default_name": [
                "message": "Default String",
                "description": "The test name.",
            ],
            "default_description": [
                "message": "Default Description",
                "description": "The test description.",
            ],
        ]

        let regionalMessages = [
            "regional_name": [
                "message": "Regional String",
                "description": "The regional name.",
            ]
        ]

        let resources: [String: Any] = [
            "_locales/en/messages.json": defaultMessages,
            "_locales/en_US/messages.json": regionalMessages,
        ]

        var webExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))

        expectEqual(webExtension.displayName, "Default String")
        expectEqual(webExtension.displayShortName, "Regional String")
        expectEqual(webExtension.displayVersion, "1.0")
        expectEqual(webExtension.version, "1.0")
        expectEqual(webExtension.displayDescription, "Default Description")
        expectTrue(webExtension.errors.isEmpty)

        testManifestDictionary["short_name"] = "__MSG_default_name__"
        webExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))

        expectEqual(webExtension.displayShortName, "Default String")
        expectTrue(webExtension.errors.isEmpty)
    }

    @Test
    func multipleIconSizes() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "version": "1.0",
            "description": "Test",

            "icons": [
                "16": "icon-16.png",
                "32": "icon-32.png",
                "64": "icon-64.png",
            ],

            "action": [
                "default_icon": [
                    "16": "action-icon-16.png",
                    "32": "action-icon-32.png",
                    "64": "action-icon-64.png",
                ]
            ],
        ]

        #if WTF_PLATFORM_IOS_FAMILY
        let screenScale = UITraitCollection.current.displayScale
        #else
        let screenScale = NSScreen.main?.backingScaleFactor ?? 1
        #endif

        let requestedSize = CGSize(width: 16, height: 16)
        let color16: CocoaColor
        let color32: CocoaColor
        let color64: CocoaColor

        if requestedSize.width * screenScale <= 16 {
            color16 = .black
            color32 = .white
            color64 = .white
        } else if requestedSize.width * screenScale <= 32 {
            color16 = .white
            color32 = .black
            color64 = .white
        } else {
            color16 = .white
            color32 = .white
            color64 = .black
        }

        let icon16 = makePNGData(size: CGSize(width: 16, height: 16), color: color16)
        let icon32 = makePNGData(size: CGSize(width: 32, height: 32), color: color32)
        let icon64 = makePNGData(size: CGSize(width: 64, height: 64), color: color64)

        let resources: [String: Any] = [
            "icon-16.png": icon16,
            "icon-32.png": icon32,
            "icon-64.png": icon64,
            "action-icon-16.png": icon16,
            "action-icon-32.png": icon32,
            "action-icon-64.png": icon64,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.icon(for: requestedSize))
        expectEqual(icon.size, requestedSize)
        #if WTF_PLATFORM_IOS_FAMILY
        expectEqual(icon.scale, screenScale)
        #endif
        expectTrue(compareColors(pixelColor(of: icon), .black))

        let actionIcon = try requireNonNil(testExtension.actionIcon(for: requestedSize))
        expectEqual(actionIcon.size, requestedSize)
        #if WTF_PLATFORM_IOS_FAMILY
        expectEqual(actionIcon.scale, screenScale)
        #endif
        expectTrue(compareColors(pixelColor(of: actionIcon), .black))
    }

    @Test
    func iconErrorsOnce() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "version": "1.0",
            "description": "Test",

            "icons": [
                "16": "icon-16.png",
                "32": "icon-32.png",
                "64": "missing-icon.png",
            ],

            "action": [
                "default_icon": [
                    "16": "action-icon-16.png",
                    "32": "action-icon-32.png",
                    "64": "missing-action-icon.png",
                ]
            ],
        ]

        let icon16 = makePNGData(size: CGSize(width: 16, height: 16), color: .black)
        let icon32 = makePNGData(size: CGSize(width: 32, height: 32), color: .white)

        let resources: [String: Any] = [
            "icon-16.png": icon16,
            "icon-32.png": icon32,
            "action-icon-16.png": icon16,
            "action-icon-32.png": icon32,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectEqual(testExtension.errors.count, 0)

        // Request the icons multiple times to trigger access to the image resources.
        for _ in 0..<3 {
            expectNil(testExtension.icon(for: CGSize(width: 64, height: 64)))
            expectNil(testExtension.actionIcon(for: CGSize(width: 64, height: 64)))
        }

        // A total of 4 errors are expected: one per missing resource, and one per icon type (normal and action).
        expectEqual(testExtension.errors.count, 4)
    }

    @Test
    func iconsWithUnsupportedFormatsAreRejected() throws {
        let photoshopBytes: [UInt8] = [
            UInt8(ascii: "8"), UInt8(ascii: "B"), UInt8(ascii: "P"), UInt8(ascii: "S"), 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
        ]

        let photoshopData = Data(photoshopBytes)
        let photoshopDataURL = "data:image/png;base64," + photoshopData.base64EncodedString()

        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
            "icons": ["128": "icon-128.psd"],
            "action": ["default_icon": ["128": photoshopDataURL]],
        ]

        var resources: [String: Any] = ["icon-128.psd": photoshopData]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectNil(testExtension.icon(for: CGSize(width: 128, height: 128)))
        expectNil(testExtension.actionIcon(for: CGSize(width: 128, height: 128)))
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))

        // The MIME type derived from the icon path must not allow bypassing the format restriction.
        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
            "icons": ["128": "icon-128.svg"],
        ]

        resources = ["icon-128.svg": photoshopData]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectNil(testExtension.icon(for: CGSize(width: 128, height: 128)))
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    @Test
    func svgIconViaIconsKeyLoads() throws {
        // A real SVG supplied through the plain `icons` key must still decode (via the dedicated SVG
        // path), not be rejected by the bitmap allow-list. This is the positive companion to
        // iconsWithUnsupportedFormatsAreRejected().
        let iconSVG =
            "<svg width='100' height='100' xmlns='http://www.w3.org/2000/svg'><circle cx='50' cy='50' r='50' fill='white' /></svg>"

        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
            "icons": ["128": "icon-128.svg"],
        ]

        let resources: [String: Any] = ["icon-128.svg": iconSVG]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectNotNil(testExtension.icon(for: CGSize(width: 128, height: 128)))
        expectNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    #if WTF_PLATFORM_MAC

    @Test
    func pdfIconLoads() throws {
        // PDF is a supported icon format on macOS, for both the `icons` key and an action's `default_icon`.
        let iconPDF = makePDFData(size: CGSize(width: 128, height: 128), color: .black)

        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
            "icons": ["128": "icon-128.pdf"],
            "action": ["default_icon": ["128": "action-icon-128.pdf"]],
        ]

        let resources: [String: Any] = [
            "icon-128.pdf": iconPDF,
            "action-icon-128.pdf": iconPDF,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))

        let icon = testExtension.icon(for: CGSize(width: 128, height: 128))
        expectNotNil(icon)
        if let icon {
            expectTrue(compareColors(pixelColor(of: icon), .black))
        }

        expectNotNil(testExtension.actionIcon(for: CGSize(width: 128, height: 128)))

        expectNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    @Test
    func mixedBitmapAndPDFIconsBothResolve() throws {
        // A manifest mixing a vector format with a bitmap one, where neither may be handed back in place of
        // the other. bestIconSize() picks the smallest size at or above the ideal pixel size, falling back to
        // the largest, so these two requests select their formats at any screen scale.
        let iconPDF = makePDFData(size: CGSize(width: 16, height: 16), color: .black)
        let iconPNG = makePNGData(size: CGSize(width: 128, height: 128), color: .white)

        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
            "icons": ["16": "icon-16.pdf", "128": "icon-128.png"],
        ]

        let resources: [String: Any] = ["icon-16.pdf": iconPDF, "icon-128.png": iconPNG]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))

        // Nothing at or above this ideal size, so the largest entry wins: the bitmap.
        let bitmapIcon = testExtension.icon(for: CGSize(width: 128, height: 128))
        expectNotNil(bitmapIcon)
        if let bitmapIcon {
            expectTrue(compareColors(pixelColor(of: bitmapIcon), .white))
        }

        let pdfIcon = testExtension.icon(for: CGSize(width: 8, height: 8))
        expectNotNil(pdfIcon)
        if let pdfIcon {
            expectTrue(compareColors(pixelColor(of: pdfIcon), .black))
        }

        expectNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    @Test
    func pdfActionIconAsStringLoads() throws {
        // The Firefox string form of `default_icon`, rather than a size-to-path object. It resolves through
        // populateActionPropertiesIfNeeded(), which calls iconForPath() directly rather than via bestIcon().
        let iconPDF = makePDFData(size: CGSize(width: 128, height: 128), color: .black)

        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
            "action": ["default_icon": "action-icon.pdf"],
        ]

        // No `icons` key, so the extension-icon fallback cannot mask a failure here.
        let testExtension = try requireNonNil(
            WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["action-icon.pdf": iconPDF])
        )

        expectNotNil(testExtension.actionIcon(for: CGSize(width: 128, height: 128)))
        expectNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    @Test
    func malformedPDFIconIsReported() throws {
        // A PDF that cannot be rasterized must still be reported, matching the other formats.
        let malformedPDF = Data("%PDF-1.7 truncated, no objects or trailer".utf8)

        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
            "icons": ["128": "icon-128.pdf"],
        ]

        let testExtension = try requireNonNil(
            WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["icon-128.pdf": malformedPDF])
        )

        expectNil(testExtension.icon(for: CGSize(width: 128, height: 128)))
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    #endif // WTF_PLATFORM_MAC

    @Test
    func iconsWithWrongTypeIsReported() throws {
        for wrongTypeValue in ["icon-128.png", ["icon-128.png"], 128] as [Any] {
            let testManifestDictionary: [String: Any] = [
                "manifest_version": 3,
                "name": "Test",
                "version": "1.0",
                "description": "Test",
                "icons": wrongTypeValue,
            ]

            let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))

            expectNil(testExtension.icon(for: CGSize(width: 128, height: 128)))
            expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        }
    }

    @Test
    func missingIconsIsNotReported() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "version": "1.0",
            "description": "Test",
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))

        expectNil(testExtension.icon(for: CGSize(width: 128, height: 128)))
        expectTrue(testExtension.errors.isEmpty)
    }

    @Test
    func symbolImageIcon() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "version": "1.0",
            "description": "Test",

            "icons": [
                "16": "symbol:star"
            ],

            "action": [
                "default_icon": "symbol:heart.fill"
            ],
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.icon(for: CGSize(width: 16, height: 16)))
        expectTrue(icon.isKind(of: CocoaImage.self))
        expectTrue(icon.isSymbol)

        let actionIcon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))
        expectTrue(actionIcon.isKind(of: CocoaImage.self))
        expectTrue(actionIcon.isSymbol)
    }

    @Test
    func privateSymbolImageIcon() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "version": "1.0",
            "description": "Test",

            "icons": [
                "16": "symbol:moon.and.stars.artframe"
            ],

            "action": [
                "default_icon": "symbol:person.lanyardcard.fill"
            ],
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.icon(for: CGSize(width: 16, height: 16)))
        expectTrue(icon.isKind(of: CocoaImage.self))
        expectTrue(icon.isSymbol)

        let actionIcon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))
        expectTrue(actionIcon.isKind(of: CocoaImage.self))
        expectTrue(actionIcon.isSymbol)
    }

    #if ENABLE_WK_WEB_EXTENSIONS_ICON_VARIANTS
    @Test
    func multipleIconVariants() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "version": "1.0",
            "description": "Test",

            "icon_variants": [
                ["32": "dark-32.png", "64": "dark-64.png", "color_schemes": ["dark"]],
                ["32": "light-32.png", "64": "light-64.png", "color_schemes": ["light"]],
            ],
        ]

        let dark16Icon = makePNGData(size: CGSize(width: 32, height: 32), color: .white)
        let dark32Icon = makePNGData(size: CGSize(width: 64, height: 64), color: .white)
        let light16Icon = makePNGData(size: CGSize(width: 32, height: 32), color: .black)
        let light32Icon = makePNGData(size: CGSize(width: 64, height: 64), color: .black)

        let resources: [String: Any] = [
            "dark-32.png": dark16Icon,
            "dark-64.png": dark32Icon,
            "light-32.png": light16Icon,
            "light-64.png": light32Icon,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        try withAppearance(.dark) {
            let iconDark16 = try requireNonNil(testExtension.icon(for: CGSize(width: 16, height: 16)))
            expectEqual(iconDark16.size, CGSize(width: 16, height: 16))
            expectTrue(compareColors(pixelColor(of: iconDark16), .white))

            let iconDark32 = try requireNonNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
            expectEqual(iconDark32.size, CGSize(width: 32, height: 32))
            expectTrue(compareColors(pixelColor(of: iconDark32), .white))
        }

        try withAppearance(.light) {
            let iconLight16 = try requireNonNil(testExtension.icon(for: CGSize(width: 16, height: 16)))
            expectEqual(iconLight16.size, CGSize(width: 16, height: 16))
            expectTrue(compareColors(pixelColor(of: iconLight16), .black))

            let iconLight32 = try requireNonNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
            expectEqual(iconLight32.size, CGSize(width: 32, height: 32))
            expectTrue(compareColors(pixelColor(of: iconLight32), .black))
        }
    }

    @Test
    func singleIconVariant() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Single Variant",
            "version": "1.0",
            "description": "Test with single icon variant",

            "icon_variants": [
                ["32": "icon-32.png"]
            ],
        ]

        let icon32 = makePNGData(size: CGSize(width: 32, height: 32), color: .white)

        let resources: [String: Any] = [
            "icon-32.png": icon32
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        var icon = try requireNonNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
        expectEqual(icon.size, CGSize(width: 32, height: 32))

        icon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 32, height: 32)))
        expectEqual(icon.size, CGSize(width: 32, height: 32))
    }

    @Test
    func iconVariantWithUnknownPropertyIsSkipped() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Unknown Property",
            "version": "1.0",
            "description": "Test with an unknown icon variant property",

            "icon_variants": [
                ["32": "unknown-32.png", "contrast": "high"],
                ["32": "known-32.png"],
            ],
        ]

        let unknownIcon = makePNGData(size: CGSize(width: 32, height: 32), color: .white)
        let knownIcon = makePNGData(size: CGSize(width: 32, height: 32), color: .black)

        let resources: [String: Any] = [
            "unknown-32.png": unknownIcon,
            "known-32.png": knownIcon,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
        expectEqual(icon.size, CGSize(width: 32, height: 32))
        expectTrue(compareColors(pixelColor(of: icon), .black))
    }

    @Test
    func iconVariantWithOnlyUnknownPropertyHasNoIcon() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Only Unknown Property",
            "version": "1.0",
            "description": "Test with a single icon variant with an unknown property",

            "icon_variants": [
                ["32": "unknown-32.png", "contrast": "high"]
            ],
        ]

        let resources: [String: Any] = [
            "unknown-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .white)
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))

        expectNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses UIScreen.screens to find the largest screen scale")
    #endif
    @Test
    func iconVariantSelectionUsesPixelSize() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Variant Pixel Size",
            "version": "1.0",
            "description": "Test choosing between icon variants by pixel size",

            "icon_variants": [
                ["16": "icon-16.png"],
                ["32": "icon-32.png"],
            ],
        ]

        let resources: [String: Any] = [
            "icon-16.png": makePNGData(size: CGSize(width: 16, height: 16), color: .black),
            "icon-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .white),
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        var largestScreenScale: CGFloat = 1
        #if USE_APPKIT
        for screen in NSScreen.screens {
            largestScreenScale = max(largestScreenScale, screen.backingScaleFactor)
        }
        #else
        for screen in UIScreen.screens {
            largestScreenScale = max(largestScreenScale, screen.scale)
        }
        #endif

        // A 16 point icon needs 32 pixels on a 2x screen, so the 32 pixel variant is the better match there.
        let expectedColor: CocoaColor = largestScreenScale >= 2 ? .white : .black

        let icon = try requireNonNil(testExtension.icon(for: CGSize(width: 16, height: 16)))
        expectTrue(compareColors(pixelColor(of: icon), expectedColor))
    }

    @Test
    func anySizeIconVariant() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Any Size",
            "version": "1.0",
            "description": "Test with any size icon",

            "icon_variants": [
                ["any": "icon-any.svg"]
            ],
        ]

        let iconAny =
            "<svg width='100' height='100' xmlns='http://www.w3.org/2000/svg'><circle cx='50' cy='50' r='50' fill='white' /></svg>"

        let resources: [String: Any] = [
            "icon-any.svg": iconAny
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        var icon = try requireNonNil(testExtension.icon(for: CGSize(width: 64, height: 64)))
        expectEqual(icon.size, CGSize(width: 64, height: 64))

        icon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 64, height: 64)))
        expectEqual(icon.size, CGSize(width: 64, height: 64))
    }

    @Test
    func iconVariantPrefersLargeEnoughSizeOverAny() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Size Over Any",
            "version": "1.0",
            "description": "Test preferring an explicit size that is large enough over any",

            "icon_variants": [
                ["any": "icon-any.svg", "256": "icon-256.png"]
            ],
        ]

        let iconAny =
            "<svg width='100' height='100' xmlns='http://www.w3.org/2000/svg'><rect width='100' height='100' fill='black' /></svg>"

        let resources: [String: Any] = [
            "icon-any.svg": iconAny,
            "icon-256.png": makePNGData(size: CGSize(width: 256, height: 256), color: .white),
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.icon(for: CGSize(width: 16, height: 16)))
        expectTrue(compareColors(pixelColor(of: icon), .white))
    }

    @Test
    func iconVariantUsesAnyWhenNoSizeIsLargeEnough() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Any Over Smaller Sizes",
            "version": "1.0",
            "description": "Test preferring any over explicit sizes that are too small",

            "icon_variants": [
                ["any": "icon-any.svg", "16": "icon-16.png"]
            ],
        ]

        let iconAny =
            "<svg width='100' height='100' xmlns='http://www.w3.org/2000/svg'><rect width='100' height='100' fill='black' /></svg>"

        let resources: [String: Any] = [
            "icon-any.svg": iconAny,
            "icon-16.png": makePNGData(size: CGSize(width: 16, height: 16), color: .white),
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.icon(for: CGSize(width: 64, height: 64)))
        expectTrue(compareColors(pixelColor(of: icon), .black))
    }

    @Test
    func iconVariantsWithWrongTypeRecordsError() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "version": "1.0",
            "description": "Test with a non-array icon_variants",

            "icon_variants": "icon.png",
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))

        expectNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
        expectEqual(testExtension.errors.count, 1)
    }

    @Test
    func iconVariantsSkipsNonObjectEntries() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test",
            "version": "1.0",
            "description": "Test with a non-object entry in icon_variants",

            "icon_variants": [
                "icon-16.png",
                ["32": "icon-32.png"],
            ] as [Any],
        ]

        let resources: [String: Any] = [
            "icon-32.png": makePNGData(size: CGSize(width: 32, height: 32), color: .white)
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))

        let icon = testExtension.icon(for: CGSize(width: 32, height: 32))
        expectNotNil(icon)
    }

    @Test
    func noIconVariants() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test No Variants",
            "version": "1.0",
            "description": "Test with no icon variants",
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))
        expectTrue(testExtension.errors.isEmpty)

        var icon = testExtension.icon(for: CGSize(width: 32, height: 32))
        expectNil(icon)

        icon = testExtension.actionIcon(for: CGSize(width: 32, height: 32))
        expectNil(icon)
    }

    @Test
    func iconsAndIconVariantsSpecified() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Icons and Variants",
            "version": "1.0",
            "description": "Test with both icons and icon variants specified",

            "icons": [
                "32": "icon-legacy.png"
            ],

            "icon_variants": [
                ["32": "icon-variant.png"]
            ],
        ]

        let iconLegacy = makePNGData(size: CGSize(width: 32, height: 32), color: .black)
        let iconVariant = makePNGData(size: CGSize(width: 32, height: 32), color: .white)

        let resources: [String: Any] = [
            "icon-legacy.png": iconLegacy,
            "icon-variant.png": iconVariant,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        var icon = try requireNonNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
        expectEqual(icon.size, CGSize(width: 32, height: 32))
        expectTrue(compareColors(pixelColor(of: icon), .white))

        icon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 32, height: 32)))
        expectEqual(icon.size, CGSize(width: 32, height: 32))
        expectTrue(compareColors(pixelColor(of: icon), .white))
    }

    @Test
    func actionIconVariantsMultiple() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Action Multiple Variants",
            "version": "1.0",
            "description": "Test action with multiple icon variants",

            "action": [
                "icon_variants": [
                    ["32": "action-dark-32.png", "64": "action-dark-64.png", "color_schemes": ["dark"]],
                    ["32": "action-light-32.png", "64": "action-light-64.png", "color_schemes": ["light"]],
                ]
            ],
        ]

        let dark32Icon = makePNGData(size: CGSize(width: 32, height: 32), color: .white)
        let dark64Icon = makePNGData(size: CGSize(width: 64, height: 64), color: .white)
        let light32Icon = makePNGData(size: CGSize(width: 32, height: 32), color: .black)
        let light64Icon = makePNGData(size: CGSize(width: 64, height: 64), color: .black)

        let resources: [String: Any] = [
            "action-dark-32.png": dark32Icon,
            "action-dark-64.png": dark64Icon,
            "action-light-32.png": light32Icon,
            "action-light-64.png": light64Icon,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        try withAppearance(.dark) {
            let iconDark32 = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 32, height: 32)))
            expectEqual(iconDark32.size, CGSize(width: 32, height: 32))
            expectTrue(compareColors(pixelColor(of: iconDark32), .white))

            let iconDark64 = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 64, height: 64)))
            expectEqual(iconDark64.size, CGSize(width: 64, height: 64))
            expectTrue(compareColors(pixelColor(of: iconDark64), .white))
        }

        try withAppearance(.light) {
            let iconLight32 = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 32, height: 32)))
            expectEqual(iconLight32.size, CGSize(width: 32, height: 32))
            expectTrue(compareColors(pixelColor(of: iconLight32), .black))

            let iconLight64 = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 64, height: 64)))
            expectEqual(iconLight64.size, CGSize(width: 64, height: 64))
            expectTrue(compareColors(pixelColor(of: iconLight64), .black))
        }
    }

    @Test
    func actionIconSingleVariant() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Action Single Variant",
            "version": "1.0",
            "description": "Test action with a single icon variant",

            "action": [
                "icon_variants": [
                    ["32": "action-icon-32.png"]
                ]
            ],
        ]

        let icon32 = makePNGData(size: CGSize(width: 32, height: 32), color: .white)

        let resources: [String: Any] = [
            "action-icon-32.png": icon32
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 32, height: 32)))
        expectEqual(icon.size, CGSize(width: 32, height: 32))
        expectTrue(compareColors(pixelColor(of: icon), .white))

        expectNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
    }

    @Test
    func actionIconAnySizeVariant() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Action Any Size",
            "version": "1.0",
            "description": "Test action with any size icon",

            "action": [
                "icon_variants": [
                    ["any": "action-icon-any.svg"]
                ]
            ],
        ]

        let iconAny =
            "<svg width='100' height='100' xmlns='http://www.w3.org/2000/svg'><circle cx='50' cy='50' r='50' fill='white' /></svg>"

        let resources: [String: Any] = [
            "action-icon-any.svg": iconAny
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 64, height: 64)))
        expectEqual(icon.size, CGSize(width: 64, height: 64))

        expectNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
    }

    @Test
    func actionNoIconVariants() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Action No Variants",
            "version": "1.0",
            "description": "Test action with no icon variants",
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))
        expectTrue(testExtension.errors.isEmpty)

        var icon = testExtension.actionIcon(for: CGSize(width: 32, height: 32))
        expectNil(icon)

        icon = testExtension.icon(for: CGSize(width: 32, height: 32))
        expectNil(icon)
    }

    @Test
    func actionIconsAndIconVariantsSpecified() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Action Icons and Variants",
            "version": "1.0",
            "description": "Test action with both icons and icon variants specified",

            "action": [
                "default_icon": [
                    "32": "action-icon-legacy.png"
                ],
                "icon_variants": [
                    ["32": "action-icon-variant.png"]
                ],
            ],
        ]

        let iconLegacy = makePNGData(size: CGSize(width: 32, height: 32), color: .black)
        let iconVariant = makePNGData(size: CGSize(width: 32, height: 32), color: .white)

        let resources: [String: Any] = [
            "action-icon-legacy.png": iconLegacy,
            "action-icon-variant.png": iconVariant,
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: resources))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 32, height: 32)))
        expectEqual(icon.size, CGSize(width: 32, height: 32))
        expectTrue(compareColors(pixelColor(of: icon), .white))

        expectNil(testExtension.icon(for: CGSize(width: 32, height: 32)))
    }

    @Test
    func symbolImageIconVariant() throws {
        let testManifestDictionary: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Symbol Variant",
            "version": "1.0",
            "description": "Test SF Symbol icon variant",

            "icon_variants": [
                ["any": "symbol:star"]
            ],

            "action": [
                "icon_variants": [
                    ["any": "symbol:heart.fill"]
                ]
            ],
        ]

        let testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))
        expectTrue(testExtension.errors.isEmpty)

        let icon = try requireNonNil(testExtension.icon(for: CGSize(width: 64, height: 64)))
        expectTrue(icon.isKind(of: CocoaImage.self))
        expectTrue(icon.isSymbol)

        let actionIcon = try requireNonNil(testExtension.actionIcon(for: CGSize(width: 64, height: 64)))
        expectTrue(actionIcon.isKind(of: CocoaImage.self))
        expectTrue(actionIcon.isSymbol)
    }
    #endif // ENABLE_WK_WEB_EXTENSIONS_ICON_VARIANTS

    @Test
    func actionParsing() throws {
        var testManifestDictionary: [String: Any] = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0"]
        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0", "browser_action": [:]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0", "page_action": [:]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = [
            "manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0", "browser_action": [:], "page_action": [:],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0", "safari_action": [:]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = [
            "manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0",
            "browser_action": ["default_title": "Button Title"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectEqual(testExtension.displayActionLabel, "Button Title")
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = [
            "manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0",
            "page_action": ["default_title": "Button Title"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectEqual(testExtension.displayActionLabel, "Button Title")
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        // action should be ignored in manifest v2, with an error recorded.
        testManifestDictionary = [
            "manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0", "action": ["default_title": "Button Title"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        // Manifest v3 looks for the "action" key.
        testManifestDictionary = [
            "manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0", "action": ["default_title": "Button Title"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectEqual(testExtension.displayActionLabel, "Button Title")
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        // Manifest v3 should never find a browser_action, and records an error.
        testManifestDictionary = [
            "manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0",
            "browser_action": ["default_title": "Button Title"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        // Or a page action.
        testManifestDictionary = [
            "manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0",
            "page_action": ["default_title": "Button Title"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        let imageData = makePNGData(size: CGSize(width: 16, height: 16), color: .green)

        testManifestDictionary = [
            "manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0",
            "action": ["default_title": "Button Title", "default_icon": "test.png"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["test.png": imageData]))
        expectTrue(testExtension.errors.isEmpty)
        expectEqual(testExtension.displayActionLabel, "Button Title")
        expectNotNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = [
            "manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0",
            "action": ["default_title": "Button Title", "default_icon": ["16": "test.png"]],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["test.png": imageData]))
        expectTrue(testExtension.errors.isEmpty)
        expectEqual(testExtension.displayActionLabel, "Button Title")
        expectNotNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = [
            "manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0", "icons": ["16": "test.png"],
            "action": ["default_title": "Button Title"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: ["test.png": imageData]))
        expectTrue(testExtension.errors.isEmpty)
        expectEqual(testExtension.displayActionLabel, "Button Title")
        expectNotNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        testManifestDictionary = ["manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0", "action": [:]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectNil(testExtension.displayActionLabel)
        expectNil(testExtension.actionIcon(for: CGSize(width: 16, height: 16)))

        // Invalid cases

        testManifestDictionary = [
            "manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0",
            "action": ["default_title": "Button Title", "default_icon": "test.png"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary, resources: [:]))
        expectEqual(testExtension.errors.count, 2)
    }

    @Test
    func contentScriptsParsing() throws {
        var testManifestDictionary: [String: Any] = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0"]

        testManifestDictionary["content_scripts"] = [["js": ["test.js", 1, ""], "css": [false, "test.css", ""], "matches": ["*://*/"]]]
        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [
            [
                "js": ["test.js", 1, ""], "css": [false, "test.css", ""], "matches": ["*://*/"],
                "exclude_matches": ["*://*.example.com/"],
            ]
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [
            ["js": ["test.js", 1, ""], "css": [false, "test.css", ""], "matches": ["*://*.example.com/"]]
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": ["*://*.example.com/"], "world": "MAIN"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [
            ["css": [false, "test.css", ""], "css_origin": "user", "matches": ["*://*.example.com/"]]
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [
            ["css": [false, "test.css", ""], "css_origin": "author", "matches": ["*://*.example.com/"]]
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasInjectedContent)

        // Invalid cases

        testManifestDictionary["content_scripts"] = []
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectFalse(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = ["invalid": true]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectFalse(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": []]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectFalse(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": ["*://*.example.com/"], "run_at": "invalid"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectTrue(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [["js": ["test.js"], "matches": ["*://*.example.com/"], "world": "INVALID"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectTrue(testExtension.hasInjectedContent)

        testManifestDictionary["content_scripts"] = [
            ["css": [false, "test.css", ""], "css_origin": "bad", "matches": ["*://*.example.com/"]]
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectTrue(testExtension.hasInjectedContent)
    }

    @Test
    func permissionsParsing() throws {
        // Neither of the "permissions" and "optional_permissions" keys are defined.
        var testManifestDictionary: [String: Any] = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0"]
        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotNil(testExtension.requestedPermissions)
        expectEqual(testExtension.requestedPermissions.count, 0)
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)
        expectNotNil(testExtension.optionalPermissions)
        expectEqual(testExtension.optionalPermissions.count, 0)
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "permissions" key alone is defined but is empty.
        testManifestDictionary = ["manifest_version": 2, "permissions": []]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotNil(testExtension.requestedPermissions)
        expectEqual(testExtension.requestedPermissions.count, 0)
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)
        expectNotNil(testExtension.optionalPermissions)
        expectEqual(testExtension.optionalPermissions.count, 0)
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "optional_permissions" key alone is defined but is empty.
        testManifestDictionary = ["manifest_version": 2, "optional_permissions": []]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotNil(testExtension.requestedPermissions)
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectNotNil(testExtension.optionalPermissions)
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissions.count, 0)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)
        expectEqual(testExtension.optionalPermissions.count, 0)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "permissions" and "optional_permissions" keys are defined as invalid types.
        testManifestDictionary = ["manifest_version": 2, "permissions": 1, "optional_permissions": "foo"]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectNotNil(testExtension.requestedPermissions)
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectNotNil(testExtension.optionalPermissions)
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissions.count, 0)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)
        expectEqual(testExtension.optionalPermissions.count, 0)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "permissions" key is defined with an invalid permission.
        testManifestDictionary = ["manifest_version": 2, "permissions": ["invalid"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissions, [])
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)

        // The "permissions" key is defined with a valid permission.
        testManifestDictionary = ["manifest_version": 2, "permissions": ["tabs"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissions, [.tabs])
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)

        // The "permissions" key is defined with a valid and an invalid permission.
        testManifestDictionary = ["manifest_version": 2, "permissions": ["tabs", "invalid"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissions, [.tabs])
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)

        // The "permissions" key is defined with a valid permission and a valid origin.
        testManifestDictionary = ["manifest_version": 2, "permissions": ["tabs", "http://www.webkit.org/"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissions, [.tabs])
        expectEqual(testExtension.requestedPermissionMatchPatterns.first?.description, "http://www.webkit.org/")

        // The "permissions" key is defined with a valid permission and an invalid origin.
        testManifestDictionary = ["manifest_version": 2, "permissions": ["tabs", "foo://www.webkit.org/"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissions, [.tabs])
        expectNotNil(testExtension.requestedPermissionMatchPatterns)
        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 0)

        // The "optional_permissions" key is defined with an invalid permission.
        testManifestDictionary = ["manifest_version": 2, "optional_permissions": ["invalid"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.optionalPermissions, [])
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "optional_permissions" key is defined with a valid permission.
        testManifestDictionary = ["manifest_version": 2, "optional_permissions": ["tabs"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.optionalPermissions, [.tabs])
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "optional_permissions" key is defined with a valid and an invalid permission.
        testManifestDictionary = ["manifest_version": 2, "optional_permissions": ["tabs", "invalid"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.optionalPermissions, [.tabs])
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "optional_permissions" key is defined with a valid permission and a valid origin.
        testManifestDictionary = ["manifest_version": 2, "optional_permissions": ["tabs", "http://www.webkit.org/"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.optionalPermissions, [.tabs])
        expectEqual(testExtension.optionalPermissionMatchPatterns.first?.description, "http://www.webkit.org/")

        // The "optional_permissions" key is defined with a valid permission and an invalid origin.
        testManifestDictionary = ["manifest_version": 2, "optional_permissions": ["tabs", "foo://www.webkit.org/"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.optionalPermissions, [.tabs])
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "optional_permissions" key is defined with a valid permission and a forbidden optional permission.
        testManifestDictionary = ["manifest_version": 2, "optional_permissions": ["tabs", "geolocation"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.optionalPermissions, [.tabs])
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // The "optional_permissions" key contains a permission already defined in the "permissions" key.
        testManifestDictionary = ["manifest_version": 2, "permissions": ["tabs", "geolocation"], "optional_permissions": ["tabs"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissions, [.tabs])
        expectNotNil(testExtension.optionalPermissions)
        expectEqual(testExtension.optionalPermissions.count, 0)

        // The "optional_permissions" key contains an origin already defined in the "permissions" key.
        testManifestDictionary = [
            "manifest_version": 2, "permissions": ["http://www.webkit.org/"], "optional_permissions": ["http://www.webkit.org/"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissionMatchPatterns.first?.description, "http://www.webkit.org/")
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // Make sure manifest v2 extensions ignore hosts from host_permissions (this should only be checked for manifest v3).
        testManifestDictionary = [
            "manifest_version": 2, "permissions": ["http://www.webkit.org/"], "optional_permissions": ["http://www.example.com/"],
            "host_permissions": ["https://webkit.org/"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 1)
        try expectTrue(testExtension.requestedPermissionMatchPatterns.contains(matchPattern("http://www.webkit.org/")))
        try expectFalse(testExtension.requestedPermissionMatchPatterns.contains(matchPattern("https://webkit.org/")))
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 1)
        try expectTrue(testExtension.optionalPermissionMatchPatterns.contains(matchPattern("http://www.example.com/")))
        try expectFalse(testExtension.optionalPermissionMatchPatterns.contains(matchPattern("https://webkit.org/")))

        // Make sure manifest v3 parses hosts from host_permissions, and ignores hosts in permissions and optional_permissions.
        testManifestDictionary = [
            "manifest_version": 3, "permissions": ["http://www.webkit.org/"], "optional_permissions": ["http://www.example.com/"],
            "host_permissions": ["https://webkit.org/"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 1)
        expectEqual(testExtension.requestedPermissionMatchPatterns.first?.description, "https://webkit.org/")
        expectNotNil(testExtension.optionalPermissionMatchPatterns)
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 0)

        // Make sure manifest v3 parses optional_host_permissions.
        testManifestDictionary = [
            "manifest_version": 3, "optional_host_permissions": ["http://www.example.com/"], "host_permissions": ["https://webkit.org/"],
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectEqual(testExtension.requestedPermissionMatchPatterns.count, 1)
        expectEqual(testExtension.requestedPermissionMatchPatterns.first?.description, "https://webkit.org/")
        expectEqual(testExtension.optionalPermissionMatchPatterns.count, 1)
        expectEqual(testExtension.optionalPermissionMatchPatterns.first?.description, "http://www.example.com/")
    }

    @Test
    func backgroundParsing() throws {
        var testManifestDictionary: [String: Any] = ["manifest_version": 2, "name": "Test", "description": "Test", "version": "1.0"]

        #if WTF_PLATFORM_IOS_FAMILY
        testManifestDictionary["background"] = ["scripts": ["test.js"], "persistent": false]
        #else
        testManifestDictionary["background"] = ["scripts": ["test.js"]]
        #endif

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        #if WTF_PLATFORM_IOS_FAMILY
        expectFalse(testExtension.hasPersistentBackgroundContent)
        #else
        expectTrue(testExtension.hasPersistentBackgroundContent)
        #endif
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = ["page": "test.html", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        #if WTF_PLATFORM_IOS_FAMILY
        testManifestDictionary["background"] = ["scripts": ["test-1.js", "", "test-2.js"], "persistent": false]
        #else
        testManifestDictionary["background"] = ["scripts": ["test-1.js", "", "test-2.js"], "persistent": true]
        #endif

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        #if WTF_PLATFORM_IOS_FAMILY
        expectFalse(testExtension.hasPersistentBackgroundContent)
        #else
        expectTrue(testExtension.hasPersistentBackgroundContent)
        #endif
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = ["service_worker": "test.js"]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectTrue(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = ["scripts": ["test-1.js", "test-2.js"], "service_worker": "test.js", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = ["page": "test.html", "service_worker": "test.js", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "scripts": ["test-1.js", "test-2.js"], "page": "test.html", "service_worker": "test.js", "persistent": false,
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectEqual(testExtension.errors.count, 1)
        expectEqual((testExtension.errors.first as NSError?)?.code, WKWebExtension.Error.Code.invalidManifestEntry.rawValue)

        testManifestDictionary["background"] = ["service_worker": "test.js", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectTrue(testExtension._hasServiceWorkerBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = ["service_worker": "test.js", "type": "module", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectTrue(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = ["scripts": ["test.js", "test2.js"], "type": "module", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension._hasModularBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        // Invalid cases

        #if WTF_PLATFORM_IOS_FAMILY
        testManifestDictionary["background"] = ["page": "test.html", "persistent": true]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectTrue(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNotNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))
        #endif

        testManifestDictionary["manifest_version"] = 3
        testManifestDictionary["background"] = ["page": "test.html", "persistent": true]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNotNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["manifest_version"] = 2
        testManifestDictionary["background"] = ["service_worker": "test.js", "persistent": true]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectFalse(testExtension._hasModularBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNotNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = [:]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = ["invalid"]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = ["scripts": [], "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = ["page": "", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = ["page": ["test.html"], "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = ["scripts": [["test.js"]], "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = ["service_worker": "", "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))

        testManifestDictionary["background"] = ["service_worker": ["test.js"], "persistent": false]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectFalse(testExtension.hasBackgroundContent)
        expectFalse(testExtension.hasPersistentBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
        expectNil(matchingError(testExtension.errors, code: .invalidBackgroundPersistence))
    }

    @Test
    func backgroundDefaultPrecedence() throws {
        var testManifestDictionary: [String: Any] = ["manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0"]

        testManifestDictionary["background"] = ["page": "background.html", "scripts": ["background.js"]]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        var testContext = WKWebExtensionContext(for: testExtension)

        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectEqual(testContext._backgroundContentURL?.lastPathComponent, "background.html")
        expectEqual(testExtension.errors.count, 1)
        expectEqual((testExtension.errors.first as NSError?)?.code, WKWebExtension.Error.Code.invalidManifestEntry.rawValue)

        testManifestDictionary["background"] = ["page": "background.html", "scripts": ["background.js"], "service_worker": "worker.js"]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectEqual(testContext._backgroundContentURL?.lastPathComponent, "background.html")
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["background"] = ["page": "background.html"]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))

        expectTrue(testExtension.hasBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = ["scripts": ["background.js"], "service_worker": "worker.js"]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        testContext = WKWebExtensionContext(for: testExtension)

        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectEqual(testContext._backgroundContentURL?.lastPathComponent, "_generated_background_page.html")
        expectTrue(testExtension.errors.isEmpty)
    }

    @Test
    func backgroundPreferredEnvironmentParsing() throws {
        var testManifestDictionary: [String: Any] = ["manifest_version": 3, "name": "Test", "description": "Test", "version": "1.0"]

        testManifestDictionary["background"] = [
            "preferred_environment": ["service_worker", "document"],
            "service_worker": "background.js",
            "scripts": ["background.js"],
            "page": "background.html",
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectTrue(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": ["document", "service_worker"],
            "service_worker": "background.js",
            "scripts": ["background.js"],
            "page": "background.html",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": "service_worker",
            "service_worker": "background.js",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectTrue(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": ["document"],
            "page": "background.html",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": "document",
            "scripts": ["background.js"],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": ["document", "service_worker"],
            "scripts": ["background.js"],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": ["document", 42, "unknown"],
            "scripts": ["background.js"],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": ["unknown", 42],
            "page": "background.html",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": "unknown",
            "service_worker": "background.js",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectTrue(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["background"] = [
            "preferred_environment": ["unknown", "document"],
            "service_worker": "background.js",
            "page": "background.html",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectTrue(testExtension.errors.isEmpty)

        // Invalid cases

        testManifestDictionary["background"] = [
            "preferred_environment": [],
            "service_worker": "background.js",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectTrue(testExtension._hasServiceWorkerBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))

        testManifestDictionary["background"] = [
            "preferred_environment": 42,
            "service_worker": "background.js",
            "page": "background.html",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasBackgroundContent)
        expectFalse(testExtension._hasServiceWorkerBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))

        testManifestDictionary["background"] = [
            "preferred_environment": ["service_worker", "document"]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))

        testManifestDictionary["background"] = [
            "preferred_environment": "document",
            "service_worker": "background.js",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))

        testManifestDictionary["background"] = [
            "preferred_environment": "service_worker",
            "page": "background.html",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasBackgroundContent)
        expectNotEqual(testExtension.errors.count, 0)
        expectNotNil(matchingError(testExtension.errors, code: .invalidManifestEntry))
    }

    @Test
    func optionsPageParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_page": "options.html",
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasOptionsPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_page": "",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOptionsPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_page": 123,
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOptionsPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [
                "page": "options.html"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasOptionsPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [
                "bad": "options.html"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOptionsPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [
                "page": 123
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOptionsPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [:],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOptionsPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "options_ui": [
                "page": ""
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOptionsPage)
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

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [
                "bad": "newtab.html"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectFalse(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [
                "newtab": 123
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [:],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_url_overrides": [
                "newtab": ""
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [
                "newtab": "newtab.html"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)
        expectTrue(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [
                "newtab": 123
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [:],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOverrideNewTabPage)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "chrome_url_overrides": [
                "newtab": ""
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
        expectFalse(testExtension.hasOverrideNewTabPage)
    }

    @Test
    func messageSerializationParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["message_serialization"] = "structured_clone"
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["message_serialization"] = "json"
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["message_serialization"] = "xml"
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["message_serialization"] = 123
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["message_serialization"] = ""
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
    }

    @Test
    func contentSecurityPolicyParsing() throws {
        var testManifestDictionaryV2: [String: Any] = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "content_security_policy": "script-src 'self'; object-src 'self'",
        ]

        var testManifestDictionaryV3: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "content_security_policy": [
                "extension_pages": "script-src 'self'; object-src 'self'"
            ],
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionaryV2))
        expectTrue(testExtension.errors.isEmpty)

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionaryV3))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionaryV3["content_security_policy"] = [
            "sandbox": "sandbox allow-scripts allow-forms allow-popups allow-modals; script-src 'self'"
        ]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionaryV3))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionaryV3["content_security_policy"] = [:]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionaryV3))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionaryV2["content_security_policy"] = 123
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionaryV2))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionaryV2["content_security_policy"] = ["invalid", "type"]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionaryV2))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionaryV3["content_security_policy"] = ["extension_pages": 123]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionaryV3))
        expectEqual(testExtension.errors.count, 1)
    }

    @Test
    func webAccessibleResourcesV2() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "web_accessible_resources": ["images/*.png", "styles/*.css"],
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["web_accessible_resources"] = []
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["web_accessible_resources"] = "bad"
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["web_accessible_resources"] = [:]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
    }

    @Test
    func webAccessibleResourcesV3() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "web_accessible_resources": [
                [
                    "resources": ["images/*.png", "styles/*.css"],
                    "matches": ["<all_urls>"],
                ],
                [
                    "resources": ["scripts/*.js"],
                    "matches": ["*://localhost/*"],
                ],
            ],
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["web_accessible_resources"] = [
            [
                "resources": [],
                "matches": [],
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary["web_accessible_resources"] = [
            [
                "resources": "bad",
                "matches": ["<all_urls>"],
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["web_accessible_resources"] = [
            [
                "resources": ["images/*.png"],
                "matches": "bad",
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["web_accessible_resources"] = [
            [
                "matches": ["<all_urls>"]
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["web_accessible_resources"] = [
            [
                "resources": []
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.errors.count, 1)
    }

    @Test
    func commandsParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "commands": [
                "show-popup": [
                    "suggested_key": [
                        "default": "Ctrl+Shift+P",
                        "mac": "Command+Shift+P",
                    ],
                    "description": "Show the popup",
                ]
            ],
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "action": [
                "default_title": "Test Action"
            ],
            "commands": [:],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_action": [
                "default_title": "Test Action"
            ],
            "commands": [:],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "page_action": [
                "default_title": "Test Action"
            ],
            "commands": [:],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "action": [
                "default_title": "Test Action"
            ],
            "commands": [
                "_execute_action": [:]
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_action": [
                "default_title": "Test Action"
            ],
            "commands": [
                "_execute_browser_action": [:]
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "page_action": [
                "default_title": "Test Action"
            ],
            "commands": [
                "_execute_page_action": [:]
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "commands": [
                "show-popup": [:]
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasCommands)
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "action": [
                "default_title": "Test Action"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "browser_action": [
                "default_title": "Test Action"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 2,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "page_action": [
                "default_title": "Test Action"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasCommands)
        expectTrue(testExtension.errors.isEmpty)

        testManifestDictionary = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "commands": [
                "show-popup": "Invalid"
            ],
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasCommands)
        expectEqual(testExtension.errors.count, 1)
    }

    @Test
    func declarativeNetRequestParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "Test",
            "description": "Test",
            "version": "1.0",
            "permissions": ["declarativeNetRequest"],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "test",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectTrue(testExtension.hasContentModificationRules)
        expectTrue(testExtension.errors.isEmpty)

        // Missing id
        testManifestDictionary["declarative_net_request"] = [
            "rule_resources": [
                [
                    "enabled": true,
                    "path": "rules.json",
                ]
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasContentModificationRules)
        expectEqual(testExtension.errors.count, 1)

        // Missing enabled
        testManifestDictionary["declarative_net_request"] = [
            "rule_resources": [
                [
                    "id": "test",
                    "path": "rules.json",
                ]
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasContentModificationRules)
        expectEqual(testExtension.errors.count, 1)

        // Missing path
        testManifestDictionary["declarative_net_request"] = [
            "rule_resources": [
                [
                    "id": "test",
                    "enabled": true,
                ]
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectFalse(testExtension.hasContentModificationRules)
        expectEqual(testExtension.errors.count, 1)

        // Duplicate names
        testManifestDictionary["declarative_net_request"] = [
            "rule_resources": [
                [
                    "id": "test",
                    "enabled": true,
                    "path": "rules.json",
                ],
                [
                    "id": "test",
                    "enabled": true,
                    "path": "rules2.json",
                ],
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        // There should still be the first rule loaded.
        expectTrue(testExtension.hasContentModificationRules)
        // But also an error.
        expectEqual(testExtension.errors.count, 1)

        // One valid rule, one invalid
        testManifestDictionary["declarative_net_request"] = [
            "rule_resources": [
                [
                    "id": "test",
                    "enabled": true,
                    "path": "rules.json",
                ],
                [
                    "enabled": true,
                    "path": "rules2.json",
                ],
            ]
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        // There should still be the first rule loaded.
        expectTrue(testExtension.hasContentModificationRules)
        // But also an error.
        expectEqual(testExtension.errors.count, 1)

        // No rule sets
        testManifestDictionary["declarative_net_request"] = [
            "rule_resources": []
        ]

        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        // There shouldn't be any rules.
        expectFalse(testExtension.hasContentModificationRules)
        // Or any errors.
        expectEqual(testExtension.errors.count, 0)
    }

    @Test
    func externallyConnectableParsing() throws {
        var testManifestDictionary: [String: Any] = [
            "manifest_version": 3,
            "name": "ExternallyConnectableTest",
            "description": "ExternallyConnectableTest",
            "version": "1.0",
            "externally_connectable": [:],
        ]

        var testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 0)
        expectEqual(testExtension.errors.count, 1)

        // Expect an error since 'externally_connectable' is specified, but there are no valid match patterns or extension ids.
        testManifestDictionary["externally_connectable"] = ["matches": []]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 0)
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["externally_connectable"] = ["matches": [""]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 0)
        expectEqual(testExtension.errors.count, 1)

        testManifestDictionary["externally_connectable"] = ["matches": [], "ids": [""]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 0)
        expectEqual(testExtension.errors.count, 1)

        // Should not expect an error if <all_urls> or "*://*/*" are specified, since wildcard hosts are allowed.
        testManifestDictionary["externally_connectable"] = ["matches": ["<all_urls>", "*://*/*"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 2)
        expectEqual(testExtension.errors.count, 0)

        // Should not expect an error for a wildcard subdomain of a top level domain.
        testManifestDictionary["externally_connectable"] = ["matches": ["*://*.com/"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 1)
        expectEqual(testExtension.errors.count, 0)

        // Should have a match for *://*.example.com/*.
        let matchingPattern = try matchPattern("*://*.example.com/")
        testManifestDictionary["externally_connectable"] = ["matches": ["*://*.example.com/"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 1)
        expectEqual(testExtension.errors.count, 0)

        expectEqual(testExtension.allRequestedMatchPatterns.first?.matches(matchingPattern), true)

        testManifestDictionary["externally_connectable"] = ["matches": ["*://*.example.com/"], "ids": ["*"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 1)
        expectEqual(testExtension.errors.count, 0)

        testManifestDictionary["externally_connectable"] = ["ids": ["*"]]
        testExtension = try requireNonNil(WKWebExtension(manifestDictionary: testManifestDictionary))
        expectEqual(testExtension.allRequestedMatchPatterns.count, 0)
        expectEqual(testExtension.errors.count, 0)

        // FIXME: <https://webkit.org/b/269299> Add more tests for externally_connectable "ids" keys.
    }

    @Test
    func loadFromDirectory() async throws {
        let extensionURL = try requireNonNil(Bundle.testResources.url(forResource: "web-extension", withExtension: ""))

        let webExtension = try await WKWebExtension(resourceBaseURL: extensionURL)
        try await loadAndRunWithTestPage(webExtension)
    }

    @Test
    func iconPathWithQueryFragmentAndEscapes() async throws {
        let iconData = makePNGData(size: CGSize(width: 32, height: 32), color: .black)
        let resources: [String: Any] = ["icon-32.png": iconData, "my icon-32.png": iconData]

        for iconPath in ["icon-32.png", "icon-32.png?v=2#section", "my%20icon-32.png", "my icon-32.png"] {
            let manifest: [String: Any] = [
                "manifest_version": 3,
                "name": "Test",
                "description": "Test",
                "version": "1",
                "icons": ["32": iconPath],
            ]

            let inMemoryExtension = WKWebExtension(manifestDictionary: manifest, resources: resources)
            expectNotNil(inMemoryExtension?.icon(for: CGSize(width: 32, height: 32)), "in-memory icon path: \(iconPath)")

            let directoryURL = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString, directoryHint: .isDirectory)
            try? FileManager.default.createDirectory(at: directoryURL, withIntermediateDirectories: true)
            try? JSONSerialization.data(withJSONObject: manifest).write(to: directoryURL.appending(path: "manifest.json"), options: .atomic)
            try? iconData.write(to: directoryURL.appending(path: "icon-32.png"), options: .atomic)
            try? iconData.write(to: directoryURL.appending(path: "my icon-32.png"), options: .atomic)

            let directoryExtension = try? await WKWebExtension(resourceBaseURL: directoryURL)
            expectNotNil(directoryExtension?.icon(for: CGSize(width: 32, height: 32)), "on-disk icon path: \(iconPath)")

            try? FileManager.default.removeItem(at: directoryURL)
        }
    }

    @Test
    func loadFromDirectoryWithoutTrailingSlash() async throws {
        let extensionURLAbsoluteString = try requireNonNil(Bundle.testResources.url(forResource: "web-extension", withExtension: ""))
            .absoluteString
        let extensionURLAbsoluteStringWithoutTrailingSlash = String(extensionURLAbsoluteString.dropLast())
        let extensionURL = try requireNonNil(URL(string: extensionURLAbsoluteStringWithoutTrailingSlash))

        let webExtension = try await WKWebExtension(resourceBaseURL: extensionURL)
        try await loadAndRunWithTestPage(webExtension)
    }

    @Test
    func loadFromZipArchiveWithoutParentDirectory() async throws {
        let extensionURL = try requireNonNil(
            Bundle.testResources.url(forResource: "web-extension-without-parent-directory", withExtension: "zip")
        )

        let webExtension = try await WKWebExtension(resourceBaseURL: extensionURL)
        try await loadAndRunWithTestPage(webExtension)
    }

    @Test
    func loadFromZipArchiveWithParentDirectory() async throws {
        let extensionURL = try requireNonNil(
            Bundle.testResources.url(forResource: "web-extension-with-parent-directory", withExtension: "zip")
        )

        let webExtension = try await WKWebExtension(resourceBaseURL: extensionURL)
        try await loadAndRunWithTestPage(webExtension)
    }

    @Test
    func loadFromChromeExtensionArchive() async throws {
        let extensionURL = try requireNonNil(Bundle.testResources.url(forResource: "web-extension", withExtension: "crx"))

        let webExtension = try await WKWebExtension(resourceBaseURL: extensionURL)
        try await loadAndRunWithTestPage(webExtension)
    }

    @Test
    func loadFromMacAppExtensionBundle() async throws {
        let extensionBundleURL = try requireNonNil(Bundle.testResources.url(forResource: "web-extension-mac", withExtension: "appex"))

        let extensionBundle = try requireNonNil(Bundle(url: extensionBundleURL))

        let webExtension = try await WKWebExtension(appExtensionBundle: extensionBundle)
        try await loadAndRunWithTestPage(webExtension)
    }

    @Test
    func loadFromiOSAppExtensionBundle() async throws {
        let extensionBundleURL = try requireNonNil(Bundle.testResources.url(forResource: "web-extension-ios", withExtension: "appex"))

        let extensionBundle = try requireNonNil(Bundle(url: extensionBundleURL))

        let webExtension = try await WKWebExtension(appExtensionBundle: extensionBundle)
        try await loadAndRunWithTestPage(webExtension)
    }

    @Test
    func loadWithBadURL() async {
        let badURL = URL(fileURLWithPath: "/does/not/exist", isDirectory: true, relativeTo: nil)
        expectNotNil(badURL)

        var badExtension: WKWebExtension?
        var thrownError: (any Error)?
        do {
            badExtension = try await WKWebExtension(resourceBaseURL: badURL)
        } catch {
            thrownError = error
        }

        expectNil(badExtension)
        expectNotNil(thrownError)
    }

    @Test
    func contentScriptImport() async throws {
        let pageScript = """
            const registration = await navigator.serviceWorker.register('./service-worker.js')
            let worker = registration.installing

            worker.addEventListener('statechange', () => {
              if (worker.state === 'activated')
                window.postMessage('ready', '*')
            })
            """

        let serviceWorkerScript = """
            self.addEventListener('install', (event) => self.skipWaiting())
            self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()))

            self.addEventListener('fetch', (event) => {
              const url = new URL(event.request.url)
              if (url.pathname.endsWith('/good.js'))
                event.respondWith(fetch('./bad.js'))
              else
                event.respondWith(fetch(event.request.url))
            })
            """

        let badScript = "browser.test.notifyFail('Injected script from Service Worker executed.')"
        let goodScript = "browser.test.notifyPass()"

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module' src='page.js'></script>"
            }

            Route("/page.js", headerFields: ["Content-Type": "application/javascript"]) {
                pageScript
            }

            Route("/service-worker.js", headerFields: ["Content-Type": "application/javascript"]) {
                serviceWorkerScript
            }

            Route("/bad.js", headerFields: ["Content-Type": "application/javascript"]) {
                badScript
            }

            Route("/good.js", headerFields: ["Content-Type": "application/javascript"]) {
                goodScript
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Extension",
            "description": "Test Extension",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "matches": ["*://*/*"],
                    "js": ["content.js"],
                ]
            ],
        ]

        let backgroundScript = "browser.test.sendMessage('Load Tab')"

        try await server.run { configuration in
            // Built for each run, so the import always names the current server's port.
            let goodScriptURL = configuration.localhostAddress.appending(path: "good.js")

            let contentScript = """
                window.addEventListener('message', (event) => {
                  browser.test.assertEq(event.data, 'ready', 'Expected message to be ready')
                  import('\(goodScriptURL.absoluteString)')
                })
                """

            let resources: [String: Any] = [
                "background.js": backgroundScript,
                "content.js": contentScript,
            ]

            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func contentScriptNotInjectedForExcludedMatchPattern() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/test", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Extension",
            "description": "Test Extension",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "matches": ["*://*/*"],
                    "js": ["content.js"],
                    "exclude_matches": ["*://*/test"],
                ]
            ],
        ]

        let contentScript = """
            browser.runtime.sendMessage('Hello from content script')
            """

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender) => {
              browser.test.notifyFail('Content script should not have been injected.')
            })

            browser.test.sendMessage('Load Tab')

            setTimeout(() => {
              browser.test.notifyPass()
            }, 1000)
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)

            let url = configuration.localhostAddress.appending(path: "test")
            let context = try requireNonNil(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func multipleContentScriptsInjectedWhenMatched() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/only-this-path", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Extension",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "matches": ["*://*/only-this-path"],
                    "js": ["scriptA.js", "scriptB.js"],
                ]
            ],
        ]

        let scriptA = """
            browser.runtime.sendMessage('Script A ran')
            """

        let scriptB = """
            browser.runtime.sendMessage('Script B ran')
            """

        let backgroundScript = """
            let seen = new Set()

            browser.runtime.onMessage.addListener((message, sender) => {
              seen.add(message)
              if (seen.has('Script A ran') && seen.has('Script B ran'))
                  browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "scriptA.js": scriptA,
            "scriptB.js": scriptB,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)

            let url = configuration.localhostAddress.appending(path: "only-this-path")
            let context = try requireNonNil(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }

    @Test
    func multipleContentScriptsNotInjectedWhenNotMatched() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/non-matching", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Extension",
            "description": "Test Extension",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "matches": ["*://*/only-this-path"],
                    "js": ["scriptA.js", "scriptB.js"],
                ]
            ],
        ]

        let scriptA = """
            browser.runtime.sendMessage('Script A ran')
            """

        let scriptB = """
            browser.runtime.sendMessage('Script B ran')
            """

        let backgroundScript = """
            browser.runtime.onMessage.addListener((message, sender) => {
              browser.test.notifyFail(`No content scripts should have injected, but got: ${message}`)
            })

            browser.test.sendMessage('Load Tab')

            setTimeout(() => browser.test.notifyPass(), 1000)
            """

        let resources: [String: Any] = [
            "background.js": backgroundScript,
            "scriptA.js": scriptA,
            "scriptB.js": scriptB,
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)

            let url = configuration.localhostAddress.appending(path: "non-matching")
            let context = try requireNonNil(manager.context)
            context.setPermissionStatus(.grantedExplicitly, for: url)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: url))

            try await manager.run()
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
