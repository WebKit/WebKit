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

extension WebPage_v0 {
    @MainActor
    @_spi(Private)
    public struct NavigationAction: Sendable {
        init(_ wrapped: WKNavigationAction) {
            self.wrapped = wrapped
        }

        public var source: FrameInfo { .init(wrapped.sourceFrame) }

        public var target: FrameInfo? { wrapped.targetFrame.map(FrameInfo.init(_:)) }

        public var navigationType: WKNavigationType { wrapped.navigationType }

        public var request: URLRequest { wrapped.request }

        public var shouldPerformDownload: Bool { wrapped.shouldPerformDownload }

#if canImport(UIKit)
        public var buttonNumber: UIEvent.ButtonMask { wrapped.buttonNumber }
#else
        public var buttonNumber: Int { wrapped.buttonNumber }
#endif

        var wrapped: WKNavigationAction
    }

    @MainActor
    @_spi(Private)
    public struct NavigationResponse: Sendable {
        init(_ wrapped: WKNavigationResponse) {
            self.wrapped = wrapped
        }

        public var isForMainFrame: Bool { wrapped.isForMainFrame }

        public var response: URLResponse { wrapped.response }

        public var canShowMimeType: Bool { wrapped.canShowMIMEType }

        var wrapped: WKNavigationResponse
    }
}

// MARK: NavigationDeciding protocol

@_spi(Private)
public protocol NavigationDeciding {
    @MainActor
    func decidePolicy(for action: WebPage_v0.NavigationAction, preferences: inout WebPage_v0.NavigationPreferences) async -> WKNavigationActionPolicy

    @MainActor
    func decidePolicy(for response: WebPage_v0.NavigationResponse) async -> WKNavigationResponsePolicy

    @MainActor
    func decideAuthenticationChallengeDisposition(for challenge: URLAuthenticationChallenge) async -> (URLSession.AuthChallengeDisposition, URLCredential?)
}

// MARK: Default implementation

@_spi(Private)
public extension NavigationDeciding {
    @MainActor
    func decidePolicy(for action: WebPage_v0.NavigationAction, preferences: inout WebPage_v0.NavigationPreferences) async -> WKNavigationActionPolicy {
        .allow
    }

    @MainActor
    func decidePolicy(for response: WebPage_v0.NavigationResponse) async -> WKNavigationResponsePolicy {
        .allow
    }

    @MainActor
    func decideAuthenticationChallengeDisposition(for challenge: URLAuthenticationChallenge) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        (.performDefaultHandling, nil)
    }
}

 #endif
