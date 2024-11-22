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

extension WebPage_v0 {
    @MainActor
    @_spi(Private)
    public struct Configuration: Sendable {
        public var websiteDataStore: WKWebsiteDataStore = .default()

        public var userContentController: WKUserContentController = WKUserContentController()

        public var webExtensionController: WKWebExtensionController? = nil

        public var defaultNavigationPreferences: WebPage_v0.NavigationPreferences = WebPage_v0.NavigationPreferences()

        public var urlSchemeHandlers: [URLScheme_v0 : any URLSchemeHandler_v0] = [:]

        public var applicationNameForUserAgent: String? = nil

        public var limitsNavigationsToAppBoundDomains: Bool = false

        public var upgradeKnownHostsToHTTPS: Bool = true

        public var suppressesIncrementalRendering: Bool = false

        public var allowsInlinePredictions: Bool = false

        public var supportsAdaptiveImageGlyph: Bool = false

#if os(iOS)
        public var dataDetectorTypes: WKDataDetectorTypes = .none

        public var ignoresViewportScaleLimits: Bool = false
#endif

        public init() {
        }
    }
}

// MARK: Adapters

extension WKWebViewConfiguration {
    convenience init(wrapping wrapped: WebPage_v0.Configuration) {
        self.init()

        self.websiteDataStore = wrapped.websiteDataStore
        self.userContentController = wrapped.userContentController
        self.webExtensionController = wrapped.webExtensionController

        self.defaultWebpagePreferences = WKWebpagePreferences(wrapping: wrapped.defaultNavigationPreferences)

        self.applicationNameForUserAgent = wrapped.applicationNameForUserAgent
        self.limitsNavigationsToAppBoundDomains = wrapped.limitsNavigationsToAppBoundDomains
        self.upgradeKnownHostsToHTTPS = wrapped.upgradeKnownHostsToHTTPS
        self.suppressesIncrementalRendering = wrapped.suppressesIncrementalRendering
        self.allowsInlinePredictions = wrapped.allowsInlinePredictions
        self.supportsAdaptiveImageGlyph = wrapped.supportsAdaptiveImageGlyph

#if os(iOS)
        self.dataDetectorTypes = wrapped.dataDetectorTypes
        self.ignoresViewportScaleLimits = wrapped.ignoresViewportScaleLimits
#endif

        for (scheme, handler) in wrapped.urlSchemeHandlers {
            let handlerAdapter = WKURLSchemeHandlerAdapter(wrapping: handler)
            self._uncheckedSetURLSchemeHandler(handlerAdapter, forCanonicalURLScheme: scheme.canonicalScheme)
        }
    }
}

#endif
