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

import Foundation
public import SwiftUI

extension View {
    /// Determines whether horizontal swipe gestures trigger backward and forward page navigation.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewBackForwardNavigationGestures(_ value: WebView.BackForwardNavigationGesturesBehavior) -> some View {
        environment(\.webViewAllowsBackForwardNavigationGestures, value)
    }

    /// Determines whether magnify gestures change the view’s magnification.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewMagnificationGestures(_ value: WebView.MagnificationGesturesBehavior) -> some View {
        environment(\.webViewMagnificationGestures, value)
    }

    /// Determines whether pressing a link displays a preview of the destination for the link.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewLinkPreviews(_ value: WebView.LinkPreviewBehavior) -> some View {
        environment(\.webViewAllowsLinkPreview, value)
    }

    /// Determines whether to allow people to select or otherwise interact with text.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewTextSelection<S>(_ selectability: S) -> some View where S: TextSelectability {
        environment(\.webViewTextSelection, S.allowsSelection)
    }

    /// Determines whether a web view can display content full screen.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewElementFullscreenBehavior(_ value: WebView.ElementFullscreenBehavior) -> some View {
        environment(\.webViewElementFullscreenBehavior, value)
    }

    /// Adds an item-based context menu to a WebView, replacing the default set of context menu items.
    ///
    /// - Parameter menu: A closure that produces the menu. The single parameter to the closure describes the type of webpage element that was acted upon.
    /// - Returns: A view that can display an item-based context menu.
    @available(WK_MAC_TBA, *)
    @available(iOS, unavailable)
    @available(visionOS, unavailable)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewContextMenu(
        @ViewBuilder menu: @MainActor @escaping (WebView.ActivatedElementInfo) -> some View
    ) -> some View {
        #if os(macOS)
        let context = ContextMenuContext { info in
            let menuView = menu(info)
            return NSHostingMenu(rootView: menuView)
        }

        return environment(\.webViewContextMenuContext, context)
        #else
        return self
        #endif
    }

    /// Specifies the visibility of the webpage's natural background color within this view.
    ///
    /// By default, WebViews are opaque, and use the page's natural background color as their background color. Use this modifier if you would like to
    /// not use this behavior and instead provide a custom background using SwiftUI.
    ///
    /// - Parameter visibility: The visibility to use for the background.
    /// - Returns: A view with the specified content background visibility.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewContentBackground(_ visibility: Visibility) -> some View {
        environment(\.webViewContentBackground, visibility)
    }

    /// Adds an action to be performed when a value, created from a scroll geometry, changes.
    ///
    /// - Parameters:
    ///   - type: The type of value transformed from a ``ScrollGeometry``.
    ///   - transform: A closure that transforms a ``ScrollGeometry`` to your type.
    ///   - action: A closure to run when the transformed data changes.
    /// - Returns: A view that invokes the action when the relevant part of a web view's scroll geometry changes.
    ///
    /// - Note: The content size of web content may exceed the current size of the view's frame, however it will never be smaller than it.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewOnScrollGeometryChange<T>(
        for type: T.Type,
        of transform: @escaping (ScrollGeometry) -> T,
        action: @escaping (T, T) -> Void
    ) -> some View where T: Hashable {
        let change = OnScrollGeometryChangeContext {
            AnyHashable(transform($0))
        } action: {
            // This is a safe force cast because the result of `transform($0)` above is guaranteed to be a `T`,
            // which is the type of the `base` value of the `AnyHashable` parameters of `action`.
            // swift-format-ignore: NeverForceUnwrap
            action($0.base as! T, $1.base as! T)
        }

        return environment(\.webViewOnScrollGeometryChange, change)
    }

    /// Associates a binding to a scroll position with the web view.
    ///
    /// - Note: ``WebView`` does not support scrolling to a view with an identity. It only supports scrolling to a concrete offset, or to an edge.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewScrollPosition(_ position: Binding<ScrollPosition>) -> some View {
        environment(\.webViewScrollPositionContext, .init(position: position))
    }

    /// Enables or disables scrolling in web views when using particular inputs.
    ///
    /// - Parameters:
    ///   - behavior: Whether scrolling should be enabled or disabled for this input.
    ///   - input: The input for which to enable or disable scrolling.
    /// - Returns: A view with the configured scroll input behavior for web views.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public nonisolated func webViewScrollInputBehavior(_ behavior: ScrollInputBehavior, for input: ScrollInputKind) -> some View {
        environment(\.webViewScrollInputBehaviorContext, .init(behavior: behavior, input: input))
    }
}
