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

// Pasteboard helpers for the Swift copy and paste API tests.

private import CoreGraphics
public import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import WebKit
private import WebKit_Private.WKWebViewConfigurationPrivate

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
public import AppKit
#endif

#if WTF_PLATFORM_MAC
extension NSPasteboard.PasteboardType {
    // Swift marks AppKit's legacy pasteboard type constants unavailable, and the replacements it suggests are different
    // types, so these spell out the legacy types' values.

    /// The value of `NSHTMLPboardType`.
    public static let legacyHTML = Self("Apple HTML pasteboard type")

    /// The value of `NSURLPboardType`.
    public static let legacyURL = Self("Apple URL pasteboard type")
}
#endif

/// Creates a 400×400 web view whose pages can read data transfer items and, unless `customPasteboardDataEnabled` is
/// false, custom pasteboard data.
///
/// - Parameters:
///   - customPasteboardDataEnabled: Whether pages can read and write custom pasteboard data.
///   - colorFilterEnabled: Whether the web view's configuration enables the color filter.
///   - lockdownModeEnabled: Whether the web view loads pages in Lockdown Mode.
/// - Returns: The web view.
@MainActor
public func makeWebViewWithCustomPasteboardData(
    enabled customPasteboardDataEnabled: Bool = true,
    colorFilterEnabled: Bool = false,
    lockdownModeEnabled: Bool = false
) -> TestWKWebView {
    #if WTF_PLATFORM_IOS_FAMILY
    instantiateUIApplicationIfNeeded()
    #endif

    let configuration = WKWebViewConfiguration()
    configuration._colorFilterEnabled = colorFilterEnabled
    if lockdownModeEnabled {
        configuration.defaultWebpagePreferences.isLockdownModeEnabled = true
    }

    let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
    webView.configuration.setFeature("DataTransferItemsEnabled", enabled: true)
    webView.configuration.setFeature("CustomPasteboardDataEnabled", enabled: customPasteboardDataEnabled)
    return webView
}

#if WTF_PLATFORM_IOS_FAMILY
/// Creates the shared `UIApplication` if nothing has yet, as `TestWebKitAPI::Util::instantiateUIApplicationIfNeeded()`
/// does.
@MainActor
private func instantiateUIApplicationIfNeeded() {
    // Swift imports `UIApplication.shared` as nonoptional, but it's nil until something instantiates the application.
    guard unsafe (UIApplication.self as AnyObject).perform(#selector(getter: UIApplication.shared)) == nil else {
        return
    }

    UIApplicationInitialize()
    UIApplicationInstantiateSingleton(UIApplication.self)
}
#endif
