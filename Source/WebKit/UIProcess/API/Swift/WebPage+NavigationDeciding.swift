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

// MARK: Supporting types

extension WebPage {
    /// An object that contains information about an action that causes navigation to occur.
    ///
    /// A `NavigationAction` value is intended to be used to make policy decisions about whether to
    /// allow navigation within a web page via a `NavigationDeciding`.
    @MainActor
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct NavigationAction {
        init(_ wrapped: WKNavigationAction) {
            self.wrapped = wrapped
        }

        /// The frame that requested the navigation.
        public var source: FrameInfo { .init(wrapped.sourceFrame) }

        /// The frame in which to display the new content.
        public var target: FrameInfo? { wrapped.targetFrame.map(FrameInfo.init(_:)) }

        /// The type of action that triggered the navigation.
        public var navigationType: WKNavigationType { wrapped.navigationType }

        /// The URL request object associated with the navigation action.
        public var request: URLRequest { wrapped.request }

        /// Indicates whether the web content provided an attribute that indicates a download.
        public var shouldPerformDownload: Bool { wrapped.shouldPerformDownload }

        #if canImport(UIKit)
        /// The number of the mouse button that caused the navigation request.
        public var buttonNumber: UIEvent.ButtonMask { wrapped.buttonNumber }
        #else
        /// The number of the mouse button that caused the navigation request.
        public var buttonNumber: Int { wrapped.buttonNumber }
        #endif

        /// Whether or not the navigation is a redirect from a content rule list.
        public var isContentRuleListRedirect: Bool { wrapped.isContentRuleListRedirect }

        // SPI for the cross-import overlay.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        @_spi(CrossImportOverlay)
        public var wrapped: WKNavigationAction
    }

    /// An object that contains the response to a navigation request, and which you use to make navigation-related policy decisions.
    ///
    /// A `NavigationResponse` value is intended to be used to make policy decisions about whether to
    /// allow navigation within a web page via a `NavigationDeciding`.
    @MainActor
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct NavigationResponse {
        init(_ wrapped: WKNavigationResponse) {
            self.wrapped = wrapped
        }

        // FIXME: This needs to be made API.
        // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
        @_spi(Private)
        public var isForMainFrame: Bool { wrapped.isForMainFrame }

        /// The frame’s response.
        public var response: URLResponse { wrapped.response }

        /// Indicates whether WebKit is capable of displaying the response’s MIME type natively.
        public var canShowMimeType: Bool { wrapped.canShowMIMEType }

        var wrapped: WKNavigationResponse
    }
}

// MARK: NavigationDeciding protocol

extension WebPage {
    /// Allows providing custom behavior to handle navigation changes and to coordinate these changes for the web page's main page.
    ///
    /// For example, you might use these methods to restrict navigation from specific links within your content.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public protocol NavigationDeciding {
        /// Determines permission to navigate to new content based on the specified preferences and action information.
        ///
        /// The web page calls this method after the interaction occurs but before it attempts to load any content.
        ///
        /// - Parameters:
        ///   - action: Details about the action that triggered the navigation request.
        ///   - preferences: The preferences to use when displaying the new webpage.
        /// - Returns: The navigation policy for the action.
        @MainActor
        mutating func decidePolicy(
            for action: WebPage.NavigationAction,
            preferences: inout WebPage.NavigationPreferences
        ) async -> WKNavigationActionPolicy

        /// Determines permission to navigate to new content after the response to the navigation request is known.
        ///
        /// - Parameter response: Descriptive information about the navigation response.
        /// - Returns: The navigation policy for the response.
        @MainActor
        mutating func decidePolicy(for response: WebPage.NavigationResponse) async -> WKNavigationResponsePolicy

        /// Determines the response to an authentication challenge.
        ///
        /// - Parameter challenge: The authentication challenge.
        /// - Returns: The option to use to handle the challenge, and the credential to use for authentication when the disposition is ``URLSession/AuthChallengeDisposition/useCredential``.
        @MainActor
        mutating func decideAuthenticationChallengeDisposition(
            for challenge: URLAuthenticationChallenge
        ) async -> (URLSession.AuthChallengeDisposition, URLCredential?)
    }
}

// MARK: Default implementation

@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
extension WebPage.NavigationDeciding {
    /// By default, this method immediately returns with a policy of `.allow`.
    @MainActor
    public func decidePolicy(
        for action: WebPage.NavigationAction,
        preferences: inout WebPage.NavigationPreferences
    ) async -> WKNavigationActionPolicy {
        .allow
    }

    /// By default, this method immediately returns with a policy of `.allow`.
    @MainActor
    public func decidePolicy(for response: WebPage.NavigationResponse) async -> WKNavigationResponsePolicy {
        .allow
    }

    /// By default, this method immediately returns with a disposition of `performDefaultHandling` and a `nil` credential.
    @MainActor
    public func decideAuthenticationChallengeDisposition(
        for challenge: URLAuthenticationChallenge
    ) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        (.performDefaultHandling, nil)
    }
}

#endif
