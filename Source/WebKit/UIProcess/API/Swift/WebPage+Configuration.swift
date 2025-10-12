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
    /// A configuration type that specifies the preferences and behaviors of a webpage.
    @MainActor
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct Configuration {
        /// Creates a new configuration value.
        public init() {
        }

        /// The object you use to get and set the site’s cookies and to track the cached data objects.
        ///
        /// To create a private web-browsing session, create a non-persistent data store using the `nonPersistent()`
        /// method and assign it to this property. For more information, see `WKWebsiteDataStore`.
        public var websiteDataStore: WKWebsiteDataStore = .default()

        /// The object that coordinates interactions between your app’s native code and the webpage’s
        /// scripts and other content.
        public var userContentController: WKUserContentController = WKUserContentController()

        /// The web extension controller to associate with the webpage.
        public var webExtensionController: WKWebExtensionController? = nil

        /// The default preferences to use when loading and rendering content.
        ///
        /// Use this property to specify the JavaScript settings and content mode for new navigations.
        /// When the webpage navigates to a new resource, it passes the default preferences to its
        /// navigation decider, which can modify the preferences if desired.
        public var defaultNavigationPreferences: WebPage.NavigationPreferences = WebPage.NavigationPreferences()

        /// Allows registering an object to load resources associated with a specified URL scheme.
        public var urlSchemeHandlers: [URLScheme: any URLSchemeHandler] = [:]

        /// Allows specifying how web resources may access device sensors.
        ///
        /// The default implementation returns `WKPermissionDecision.prompt` for all requests.
        public var deviceSensorAuthorization: WebPage.DeviceSensorAuthorization = WebPage.DeviceSensorAuthorization(decision: .prompt)

        /// The app name that appears in the user agent string.
        public var applicationNameForUserAgent: Swift.String? = nil

        /// Indicates whether the web view limits navigation to pages within the app’s domain.
        ///
        /// The default value of this property is `false`.
        public var limitsNavigationsToAppBoundDomains: Bool = false

        /// Indicates whether the web view should automatically upgrade supported HTTP requests to HTTPS.
        ///
        /// The default value of this property is `true`.
        public var upgradeKnownHostsToHTTPS: Bool = true

        /// Indicates whether the web view suppresses content rendering until the content is fully loaded into memory.
        ///
        /// The default value of this property is `false`.
        public var suppressesIncrementalRendering: Bool = false

        /// Indicates whether the webpage allows media playback over AirPlay.
        ///
        /// The default value of this property is `true`.
        public var allowsAirPlayForMediaPlayback: Bool = true

        /// Indicates whether the webpage loads all of its subresources in addition to the main resource.
        ///
        /// The default value of this property is `true`.
        public var loadsSubresources: Bool = true

        /// Indicates whether inline predictions are allowed.
        ///
        /// The default value is `false`. If false, inline predictions are disabled regardless of the system setting.
        /// If true, they are enabled based on the system setting.
        public var allowsInlinePredictions: Bool = false

        /// Indicates whether insertion of adaptive image glyphs is allowed.
        ///
        /// The default value is `false`. If `false`, adaptive image glyphs are inserted as regular images.
        /// If `true`, they are inserted with the full adaptive sizing behavior.
        public var supportsAdaptiveImageGlyph: Bool = false

        private var backingShowsSystemScreenTimeBlockingView = true

        /// Indicates whether the webpage should use the system Screen Time blocking view.
        ///
        /// The default value is `true`. If `true`, the system Screen Time blocking view is shown when blocked by Screen Time.
        /// If `false`, a blurred view of the web content is shown instead.
        @available(visionOS, unavailable)
        public var showsSystemScreenTimeBlockingView: Bool {
            get { backingShowsSystemScreenTimeBlockingView }
            set { backingShowsSystemScreenTimeBlockingView = newValue }
        }

        #if os(iOS)
        /// The types of data detectors to apply to the webpage's content.
        ///
        /// Data detectors add interactivity to web content by creating links for specially formatted text.
        /// For example, the `.link` type causes the apple.com portion of the text “Visit apple.com” to
        /// become a link to the Apple website.
        ///
        /// The default value of this property is an empty OptionSet.
        public var dataDetectorTypes: WKDataDetectorTypes = []

        /// Determines whether a webpage allows scaling of the webpage.
        ///
        /// When set to `true`, this property overrides the user-scalable HTML property in a webpage, and lets
        /// the webpage scale its view's content regardless of the author’s intent.
        ///
        /// The default value of this property is `false`.
        public var ignoresViewportScaleLimits: Bool = false

        /// Indicates whether HTML5 videos play inline or use the native full-screen controller.
        public var mediaPlaybackBehavior: MediaPlaybackBehavior = .automatic
        #endif

        #if os(macOS)
        /// The directionality of user interface elements.
        ///
        /// The default value of this property is `.content`.
        public var userInterfaceDirectionPolicy: WKUserInterfaceDirectionPolicy = .content
        #endif
    }
}

extension WebPage {
    /// A type that describes the authorization permissions policy for the device's sensors a web resource may access.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct DeviceSensorAuthorization {
        /// The kind of sensor permission a web resource may request to access.
        public enum Permission: Hashable, Sendable {
            /// The orientation and motion of the device.
            case deviceOrientationAndMotion

            /// A media capture device, like a microphone or camera.
            case mediaCapture(WKMediaCaptureType)
        }

        let decisionHandler: (Permission, WebPage.FrameInfo, WKSecurityOrigin) async -> WKPermissionDecision

        /// Creates a new `DeviceSensorAuthorization` using the specified policy.
        ///
        /// - Parameter decisionHandler: A closure which decides the permission decision for an authorization request,
        /// which may be based on the kind of permission, the webpage frame information, or the security origin.
        public init(decisionHandler: @escaping (Permission, WebPage.FrameInfo, WKSecurityOrigin) async -> WKPermissionDecision) {
            self.decisionHandler = decisionHandler
        }

        /// A convenience initializer to create a DeviceSensorAuthorization that always uses the same permission decision.
        public init(decision: WKPermissionDecision) {
            self.init { _, _, _ in decision }
        }
    }
}

extension WebPage.Configuration {
    /// The behavior used when playing HTML video within a page.
    @available(iOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    @available(macOS, unavailable)
    public enum MediaPlaybackBehavior: Sendable {
        /// Use the default system value, which is `alwaysFullscreen` for iPhone and `allowsInlinePlayback` for iPad.
        case automatic

        /// Allows videos to play inline. When adding a video element to an HTML document on iPhone, you must also include the `playsinline` attribute.
        case allowsInlinePlayback

        /// Use the native fullscreen controller.
        case alwaysFullscreen
    }
}

#endif
