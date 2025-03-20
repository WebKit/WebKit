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

public import SwiftUI
public import WebKit

/// A view that displays some web content.
///
/// Connect a ``WebView`` with a ``WebPage`` to fully control the browsing experience, including essential functionality such as loading a URL.
/// Any updates to the webpage propagate the information to the view.
@available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
public struct WebView: View {
    /// Create a new WebView.
    ///
    /// - Parameter page: The ``WebPage`` that should be associated with this ``WebView``. It is a programming error to create multiple ``WebView``s with the same ``WebPage``.
    public init(_ page: WebPage) {
        self.page = page
    }

    let page: WebPage

    public var body: some View {
        WebViewRepresentable(owner: self)
    }
}

extension WebView {
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct BackForwardNavigationGesturesBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        public static let automatic: BackForwardNavigationGesturesBehavior = .init(.automatic)

        public static let enabled: BackForwardNavigationGesturesBehavior = .init(.enabled)

        public static let disabled: BackForwardNavigationGesturesBehavior = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }

    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct MagnificationGesturesBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        public static let automatic: Self = .init(.automatic)

        public static let enabled: Self = .init(.enabled)

        public static let disabled: Self = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }

    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct LinkPreviewBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        public static let automatic: LinkPreviewBehavior = .init(.automatic)

        public static let enabled: LinkPreviewBehavior = .init(.enabled)

        public static let disabled: LinkPreviewBehavior = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }

    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct ElementFullscreenBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        public static let automatic: ElementFullscreenBehavior = .init(.automatic)

        public static let enabled: ElementFullscreenBehavior = .init(.enabled)

        public static let disabled: ElementFullscreenBehavior = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }
 }
