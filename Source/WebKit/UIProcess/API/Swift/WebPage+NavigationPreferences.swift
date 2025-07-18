// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation
internal import WebKit_Internal

extension WebPage {
    /// A type that specifies the behaviors to use when loading and rendering page content.
    ///
    /// Create a `NavigationPreferences` value when you want to change the default rendering behavior of
    /// your web page. Typically, iOS devices render web content for a mobile experience, and Mac devices
    /// render content for a desktop experience.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct NavigationPreferences: Sendable {
        /// Options to indicate how to render web view content.
        ///
        /// Browsers often render webpages differently based on device type. For example, Safari provides a
        /// desktop-class experience when displaying webpages on Mac and iPad, but it displays a mobile experience
        /// when displaying pages on iPhone. Use content modes to specify how you want your web page to render
        /// content within your app.
        public enum ContentMode: Sendable {
            /// The content mode that is appropriate for the current device.
            case recommended

            /// The content mode that represents a mobile experience.
            case mobile

            /// The content mode that represents a desktop experience.
            case desktop
        }

        /// Preference for loading a webpage with HTTPS, and how failures should be handled.
        public enum UpgradeToHTTPSPolicy: Sendable {
            case keepAsRequested

            case automaticFallbackToHTTP

            case userMediatedFallbackToHTTP

            case errorOnFailure
        }

        /// Creates a new NavigationPreferences value.
        public init() {
        }

        /// The content mode for the web view to use when it loads and renders a webpage.
        ///
        /// The default value of this property is `recommended`. The web page ignores this preference for subframe navigation.
        public var preferredContentMode: ContentMode = .recommended

        /// Indicates whether JavaScript from web content is allowed to run.
        ///
        /// The default value of this property is `true`. If you change the value to `false`, the web page doesn’t
        /// execute JavaScript code referenced by the web content. That includes JavaScript code found in inline `<script>`
        /// elements, `javascript:` URLs, and all other referenced JavaScript content.
        public var allowsContentJavaScript: Bool = true

        /// Used when performing a top-level navigation to a webpage.
        ///
        /// The default value is `.keepAsRequested`. The stated preference is ignored on subframe navigation, and it may be ignored based on system configuration.
        /// The `WebPage.Configuration.upgradeKnownHostsToHTTPS` property supersedes this property for known hosts.
        public var preferredHTTPSNavigationPolicy: UpgradeToHTTPSPolicy = .keepAsRequested

        var backingIsLockdownModeEnabled: Bool? = nil

        /// A Boolean value that indicates whether to use Lockdown Mode in the web page.
        ///
        /// By default, this reflects whether the user has enabled Lockdown Mode on the device. Update this preference to
        /// override the device setting when you implement a per-website or similar setting.
        public var isLockdownModeEnabled: Bool {
            get { backingIsLockdownModeEnabled ?? false }
            set { backingIsLockdownModeEnabled = newValue }
        }
    }
}

// MARK: Adapters

extension WebPage.NavigationPreferences.ContentMode {
    init(_ wrapped: WKWebpagePreferences.ContentMode) {
        self =
            switch wrapped {
            case .recommended: .recommended
            case .mobile: .mobile
            case .desktop: .desktop
            @unknown default:
                fatalError()
            }
    }
}

extension WebPage.NavigationPreferences.UpgradeToHTTPSPolicy {
    init(_ wrapped: WKWebpagePreferences.UpgradeToHTTPSPolicy) {
        self =
            switch wrapped {
            case .keepAsRequested: .keepAsRequested
            case .automaticFallbackToHTTP: .automaticFallbackToHTTP
            case .userMediatedFallbackToHTTP: .userMediatedFallbackToHTTP
            case .errorOnFailure: .errorOnFailure
            @unknown default:
                fatalError()
            }
    }
}

extension WebPage.NavigationPreferences {
    @MainActor
    init(_ wrapped: WKWebpagePreferences) {
        self.init()

        self.preferredContentMode = .init(wrapped.preferredContentMode)
        self.preferredHTTPSNavigationPolicy = .init(wrapped.preferredHTTPSNavigationPolicy)

        self.allowsContentJavaScript = wrapped.allowsContentJavaScript
        self.isLockdownModeEnabled = wrapped.isLockdownModeEnabled
    }
}

#endif
