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
@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
public struct WebView: View {
    /// Create a new WebView.
    ///
    /// - Parameter page: The ``WebPage`` that should be associated with this ``WebView``. It is a programming error to create multiple ``WebView``s with the same ``WebPage``.
    public init(_ page: WebPage) {
        self.storage = .webPage(page)
    }

    /// Create a new WebView with the specified URL.
    ///
    /// For example, you can create a WebView that displays one of two URLs depending on the state of a toggle:
    ///
    /// ```swift
    /// struct URLView: View {
    ///     @State private var url: URL? = nil
    ///     @State private var toggle = false
    ///
    ///     var body: some View {
    ///         VStack {
    ///             Button("Toggle") {
    ///                 toggle.toggle()
    ///             }
    ///             WebView(url: url)
    ///         }
    ///         .onChange(of: toggle, initial: true) {
    ///             url = toggle ? URL(string: "https://www.webkit.org") : URL(string: "https://www.apple.com")
    ///         }
    ///     }
    /// }
    /// ```
    ///
    /// - Parameter url: The URL to display in the view. If this value is non-nil or changes to become a non-nil value, the new URL is loaded into the view.
    public init(url: URL?) {
        self.storage = .state(State(initialValue: WebPage()), url)
    }

    private let storage: Storage

    @ViewBuilder
    private var representable: some View {
        #if os(iOS)
        GeometryReader { proxy in
            WebViewRepresentable(page: storage.webPage, safeAreaInsets: proxy.safeAreaInsets)
                .ignoresSafeArea()
        }
        #else
        WebViewRepresentable(page: storage.webPage, safeAreaInsets: .init())
        #endif
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var body: some View {
        representable
            .onChange(of: storage.url, initial: true) {
                guard let url = storage.url else {
                    return
                }

                storage.webPage.load(URLRequest(url: url))
            }
    }
}

extension WebView {
    /// A type that defines the behavior of how horizontal swipe gestures trigger backward and forward page navigation.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct BackForwardNavigationGesturesBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        /// The automatic behavior.
        ///
        /// The web view automatically chooses whether horizontal swipe gestures trigger backward and forward page navigation.
        /// By default, web views use the ``WebView/BackForwardNavigationGesturesBehavior/enabled`` behavior.
        public static let automatic: BackForwardNavigationGesturesBehavior = .init(.automatic)

        /// Backward and forward navigation gestures are enabled.
        ///
        /// The web view allows horizontal swipe gestures to trigger backward and forward page navigation.
        public static let enabled: BackForwardNavigationGesturesBehavior = .init(.enabled)

        /// Backward and forward navigation gestures are disabled.
        ///
        /// The web view prevents horizontal swipe gestures from triggering backward and forward page navigation.
        public static let disabled: BackForwardNavigationGesturesBehavior = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }

    /// The options for controlling the behavior for how magnification gestures interact with web views.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct MagnificationGesturesBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        /// The automatic behavior.
        ///
        /// The web view automatically chooses whether magnify gestures change the web view’s magnification.
        /// By default, web views use the ``WebView/MagnificationGesturesBehavior/enabled`` behavior.
        public static let automatic: MagnificationGesturesBehavior = .init(.automatic)

        /// Magnify gestures are enabled.
        ///
        /// The web view allows magnify gestures to change its magnification.
        public static let enabled: MagnificationGesturesBehavior = .init(.enabled)

        /// Magnify gestures are disabled.
        ///
        /// The web view prevents magnify gestures from changing its magnification.
        public static let disabled: MagnificationGesturesBehavior = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }

    /// A type specifying the behavior for the presentation of link previews when pressing a link.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct LinkPreviewBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        /// The automatic behavior.
        ///
        /// The web view automatically chooses whether pressing a link displays a preview of the destination for the link.
        /// By default, web views use the ``WebView/LinkPreviewBehavior/enabled`` behavior.
        public static let automatic: LinkPreviewBehavior = .init(.automatic)

        /// Link previews are enabled.
        ///
        /// The web view allows pressing a link to display a preview of the destination for the link.
        public static let enabled: LinkPreviewBehavior = .init(.enabled)

        /// Link previews are disabled.
        ///
        /// The web view prevents pressing a link from displaying a preview of the destination for the link.
        public static let disabled: LinkPreviewBehavior = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }

    /// The behavior that determines whether a web view can display content full screen.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct ElementFullscreenBehavior: Sendable {
        enum Value {
            case automatic
            case enabled
            case disabled
        }

        /// The automatic behavior.
        ///
        /// The web view automatically chooses whether content can be displayed in full screen.
        /// By default, web views use the ``WebView/ElementFullscreenBehavior/disabled`` behavior.
        public static let automatic: ElementFullscreenBehavior = .init(.automatic)

        /// Element full screen is enabled.
        ///
        /// The web view allows content to be displayed in full screen.
        public static let enabled: ElementFullscreenBehavior = .init(.enabled)

        /// Element full screen is disabled.
        ///
        /// The web view prevents content from being displayed in full screen.
        public static let disabled: ElementFullscreenBehavior = .init(.disabled)

        init(_ value: Value) {
            self.value = value
        }

        let value: Value
    }

    /// Contains information about an element the user activated in a webpage, which may be used to configure a context menu for that element.
    ///
    /// For links, the information contains the URL that is linked to.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct ActivatedElementInfo: Hashable, Sendable {
        /// The URL of the link that the user clicked.
        public let linkURL: URL?
    }
}

extension WebView {
    private enum Storage: DynamicProperty {
        case state(State<WebPage>, URL?)
        case webPage(WebPage)

        var webPage: WebPage {
            switch self {
            case .state(let state, _): state.wrappedValue
            case .webPage(let webPage): webPage
            }
        }

        var url: URL? {
            switch self {
            case .state(_, let url): url
            case .webPage: nil
            }
        }
    }
}
