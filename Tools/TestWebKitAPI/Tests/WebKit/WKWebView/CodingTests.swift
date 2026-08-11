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

import Testing
import WebKit
private import WebKit_Private
private import WebKit_Private.WKPreferencesPrivate

@MainActor
struct CodingTests {
    @Test
    func wkPreferences() throws {
        let a = WKPreferences()

        // Change all defaults to something else.
        a.minimumFontSize = 10
        a.javaScriptEnabled = false
        a.shouldPrintBackgrounds = true
        a.tabFocusesLinks = true

        #if WTF_PLATFORM_IOS_FAMILY
        a.javaScriptCanOpenWindowsAutomatically = true
        #else
        a.javaScriptCanOpenWindowsAutomatically = false
        #endif

        let b = try encodeAndDecode(a)

        #expect(a.minimumFontSize == b.minimumFontSize)
        #expect(a.javaScriptEnabled == b.javaScriptEnabled)
        #expect(a.javaScriptCanOpenWindowsAutomatically == b.javaScriptCanOpenWindowsAutomatically)
        #expect(a.shouldPrintBackgrounds == b.shouldPrintBackgrounds)
        #expect(a.tabFocusesLinks == b.tabFocusesLinks)
    }

    @Test
    func wkProcessPoolShared() throws {
        let a = try #require(WKProcessPool._shared())
        let b = try encodeAndDecode(a)

        #expect(a === b)
    }

    @Test
    func wkProcessPool() throws {
        let pool = try encodeAndDecode(WKProcessPool())
        #expect(pool !== WKProcessPool._shared())
    }

    @Test
    func wkWebsiteDataStoreDefault() throws {
        let a = WKWebsiteDataStore.default()
        let b = try encodeAndDecode(a)

        #expect(a === b)
    }

    @Test
    func wkWebsiteDataStoreNonPersistent() throws {
        let store = try encodeAndDecode(WKWebsiteDataStore.nonPersistent())
        #expect(!store.isPersistent)
    }

    @Test
    func wkWebViewConfiguration() throws {
        let a = WKWebViewConfiguration()

        // Change all defaults to something else.
        a.suppressesIncrementalRendering = true
        a.applicationNameForUserAgent = "Application Name"
        a.allowsAirPlayForMediaPlayback = false

        #if WTF_PLATFORM_IOS_FAMILY
        a.dataDetectorTypes = .all
        a.allowsInlineMediaPlayback = true
        a.requiresUserActionForMediaPlayback = false
        a.selectionGranularity = .character
        a.allowsPictureInPictureMediaPlayback = false
        #endif // WTF_PLATFORM_IOS_FAMILY

        let b = try encodeAndDecode(a)

        #expect(a.suppressesIncrementalRendering == b.suppressesIncrementalRendering)
        #expect(a.applicationNameForUserAgent == b.applicationNameForUserAgent)
        #expect(a.allowsAirPlayForMediaPlayback == b.allowsAirPlayForMediaPlayback)

        #if WTF_PLATFORM_IOS_FAMILY
        #expect(a.dataDetectorTypes == b.dataDetectorTypes)
        #expect(a.allowsInlineMediaPlayback == b.allowsInlineMediaPlayback)
        #expect(a.requiresUserActionForMediaPlayback == b.requiresUserActionForMediaPlayback)
        #expect(a.selectionGranularity == b.selectionGranularity)
        #expect(a.allowsPictureInPictureMediaPlayback == b.allowsPictureInPictureMediaPlayback)
        #endif // WTF_PLATFORM_IOS_FAMILY
    }

    @Test
    func wkWebView() throws {
        let a = WKWebView()

        // Change all defaults to something else.
        a.allowsBackForwardNavigationGestures = true
        a.customUserAgent = "CustomUserAgent"

        #if WTF_PLATFORM_IOS_FAMILY
        a.allowsLinkPreview = true
        #if WTF_PLATFORM_IOS || WTF_PLATFORM_MACCATALYST || WTF_PLATFORM_VISION
        a.isFindInteractionEnabled = true
        #endif
        #else
        a.allowsLinkPreview = false
        a.allowsMagnification = false
        a.magnification = 2
        #endif

        let b = try encodeAndDecode(a)

        #expect(a.allowsBackForwardNavigationGestures == b.allowsBackForwardNavigationGestures)
        #expect(a.customUserAgent == b.customUserAgent)
        #expect(a.allowsLinkPreview == b.allowsLinkPreview)

        #if WTF_PLATFORM_MAC
        #expect(a.allowsMagnification == b.allowsMagnification)
        #expect(a.magnification == b.magnification)
        #endif

        #if WTF_PLATFORM_IOS || WTF_PLATFORM_MACCATALYST || WTF_PLATFORM_VISION
        #expect(a.isFindInteractionEnabled == b.isFindInteractionEnabled)
        #endif
    }

    @Test
    func wkWebViewSameConfiguration() throws {
        // First, encode two WKWebViews sharing the same configuration.
        let data = try {
            let configuration = WKWebViewConfiguration()

            let a = WKWebView(frame: .zero, configuration: configuration)
            let b = WKWebView(frame: .zero, configuration: configuration)

            return try NSKeyedArchiver.archivedData(withRootObject: [a, b] as NSArray, requiringSecureCoding: false)
        }()

        // Then, decode and verify that the important configuration properties are the same.
        let array = try #require(NSKeyedUnarchiver.unarchiveObject(with: data) as? NSArray)

        let aView = try #require(array[0] as? WKWebView)
        let bView = try #require(array[1] as? WKWebView)

        let a = aView.configuration
        let b = bView.configuration

        #expect(a.processPool === b.processPool)
        #expect(a.preferences === b.preferences)
        #expect(a.userContentController === b.userContentController)
        #expect(a.websiteDataStore === b.websiteDataStore)
    }
}

private func encodeAndDecode<T>(_ value: T) throws -> T where T: NSCoding, T: NSObject {
    if let secureCoding = value as? any NSSecureCoding {
        let data = try NSKeyedArchiver.archivedData(withRootObject: secureCoding, requiringSecureCoding: true)
        // The contract of `NSKeyedUnarchiver.unarchivedObject` states it does not return `nil` if no error is thrown.
        // swift-format-ignore: NeverForceUnwrap
        return try NSKeyedUnarchiver.unarchivedObject(ofClass: T.self, from: data)!
    }

    let data = try NSKeyedArchiver.archivedData(withRootObject: value, requiringSecureCoding: false)
    // Tautologically guaranteed.
    // swift-format-ignore: NeverForceUnwrap
    return NSKeyedUnarchiver.unarchiveObject(with: data) as! T
}
