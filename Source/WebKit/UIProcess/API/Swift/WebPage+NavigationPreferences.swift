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

#if ENABLE_SWIFTUI

import Foundation

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

        /// Security restriction modes for WebView content.
        @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
        @available(watchOS, unavailable)
        @available(tvOS, unavailable)
        public enum SecurityRestrictionMode: Sendable {
            /// No additional security restrictions beyond WebKit defaults.
            case none
            /// Enhanced security protections optimized for maintaining web compatibility. Disables JIT compilation and enables increased MTE adoption.
            case maximizeCompatibility
            /// Maximum security restrictions including feature disablement. Applied automatically by the system in Lockdown Mode.
            case lockdown
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

        var backingSecurityRestrictionMode: SecurityRestrictionMode? = nil

        /// Security restriction mode for this navigation.
        ///
        /// Security restriction modes provide different levels of security hardening for high-risk browsing contexts.
        /// `SecurityRestrictionMode.maximizeCompatibility` provides additional hardening while maintaining full web compatibility:
        /// - JavaScript JIT compilation disabled (interpreter-only execution)
        /// - Increased Memory Tagging Extension (MTE) coverage across allocations in the WebContent process
        /// Setting a security restriction mode creates separate, isolated WebContent processes for the specified protection level.
        /// This preference only applies to main frame navigations and will be ignored for subframe navigations. When set for a main frame, all subframe content and opened windows inherit the same security restrictions.
        /// When the system has chosen `SecurityRestrictionMode.lockdown` (e.g., in Lockdown Mode), attempts to set a less restrictive mode will fail silently.
        /// The default value is `SecurityRestrictionMode.none`.
        @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
        @available(watchOS, unavailable)
        @available(tvOS, unavailable)
        public var securityRestrictionMode: SecurityRestrictionMode {
            get { backingSecurityRestrictionMode ?? .none }
            set { backingSecurityRestrictionMode = newValue }
        }

        /// Used to make changes to the network request that will be used for this navigation's main resource load.
        @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
        @available(watchOS, unavailable)
        @available(tvOS, unavailable)
        public var alternateRequest: URLRequest? = nil

        /// Used to apply a custom `referer` header to all resource loads in the frame of this navigation.
        @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
        @available(watchOS, unavailable)
        @available(tvOS, unavailable)
        public var overrideReferrer: Swift.String? = nil
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

@available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
extension WebPage.NavigationPreferences.SecurityRestrictionMode {
    init(_ wrapped: WKSecurityRestrictionMode) {
        self =
            switch wrapped {
            case .none: .none
            case .maximizeCompatibility: .maximizeCompatibility
            case .lockdown: .lockdown
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
        self.securityRestrictionMode = .init(wrapped.securityRestrictionMode)

        self.alternateRequest = wrapped.alternateRequest
        self.overrideReferrer = wrapped.overrideReferrer
    }
}

#endif
