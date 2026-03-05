// Copyright (C) 2025 Apple Inc. All rights reserved.
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
internal import WebKit_Private

extension WKWebViewConfiguration {
    convenience init(_ wrapped: WebPage.Configuration) {
        self.init()

        self.websiteDataStore = wrapped.websiteDataStore
        self.userContentController = wrapped.userContentController
        self.webExtensionController = wrapped.webExtensionController

        self.defaultWebpagePreferences = WKWebpagePreferences(wrapped.defaultNavigationPreferences)

        self.applicationNameForUserAgent = wrapped.applicationNameForUserAgent
        self.limitsNavigationsToAppBoundDomains = wrapped.limitsNavigationsToAppBoundDomains
        self.upgradeKnownHostsToHTTPS = wrapped.upgradeKnownHostsToHTTPS
        self.suppressesIncrementalRendering = wrapped.suppressesIncrementalRendering
        self.allowsInlinePredictions = wrapped.allowsInlinePredictions
        self.supportsAdaptiveImageGlyph = wrapped.supportsAdaptiveImageGlyph
        self._loadsSubresources = wrapped.loadsSubresources

        #if !os(visionOS)
        self.showsSystemScreenTimeBlockingView = wrapped.showsSystemScreenTimeBlockingView
        #endif

        #if os(iOS)
        self.dataDetectorTypes = wrapped.dataDetectorTypes
        self.ignoresViewportScaleLimits = wrapped.ignoresViewportScaleLimits

        if wrapped.mediaPlaybackBehavior != .automatic {
            self.allowsInlineMediaPlayback = wrapped.mediaPlaybackBehavior == .allowsInlinePlayback
        }
        #endif

        #if os(macOS)
        self.userInterfaceDirectionPolicy = wrapped.userInterfaceDirectionPolicy
        #endif

        for (scheme, handler) in wrapped.urlSchemeHandlers {
            let handlerAdapter = WKURLSchemeHandlerAdapter(handler)
            self.setURLSchemeHandler(handlerAdapter, forURLScheme: scheme.rawValue)
        }
    }
}

#endif
