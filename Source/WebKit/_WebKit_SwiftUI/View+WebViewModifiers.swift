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
@_spi(Private) @_spi(CrossImportOverlay) import WebKit

extension EnvironmentValues {
    @Entry
    var webViewAllowsBackForwardNavigationGestures = WebView.BackForwardNavigationGesturesBehavior.automatic

    @Entry
    var webViewAllowsLinkPreview = WebView.LinkPreviewBehavior.automatic

    @Entry
    var webViewTextSelection = true

    @Entry
    var webViewAllowsElementFullscreen = false

    @Entry
    var webViewFindContext: FindContext = .init()

    @Entry
    var webViewContextMenuContext: ContextMenuContext? = nil
}

extension View {
    /// Determines whether horizontal swipe gestures trigger backward and forward page navigation.
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public func webViewBackForwardNavigationGestures(_ value: WebView.BackForwardNavigationGesturesBehavior = .automatic) -> some View {
        environment(\.webViewAllowsBackForwardNavigationGestures, value)
    }

    /// Determines whether pressing a link displays a preview of the destination for the link.
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public func webViewLinkPreviews(_ value: WebView.LinkPreviewBehavior = .automatic) -> some View {
        environment(\.webViewAllowsLinkPreview, value)
    }

    @_spi(Private)
    public func webViewTextSelection<S>(_ selectability: S) -> some View where S : TextSelectability {
        environment(\.webViewTextSelection, S.allowsSelection)
    }

    @_spi(Private)
    public func webViewAllowsElementFullscreen(_ value: Bool = true) -> some View {
        environment(\.webViewAllowsElementFullscreen, value)
    }

    @_spi(Private)
    public func webViewFindNavigator(isPresented: Binding<Bool>) -> some View {
        environment(\.webViewFindContext, .init(isPresented: isPresented))
    }

    @_spi(Private)
    public func webViewFindDisabled(_ isDisabled: Bool = true) -> some View {
        transformEnvironment(\.webViewFindContext) { $0.canFind = !isDisabled }
    }

    @_spi(Private)
    public func webViewReplaceDisabled(_ isDisabled: Bool = true) -> some View {
        transformEnvironment(\.webViewFindContext) { $0.canReplace = !isDisabled }
    }

    @_spi(Private)
    public func webViewContextMenu<M>(@ViewBuilder menuItems: @escaping (WebPage.ElementInfo) -> M) -> some View where M: View {
#if os(macOS)
        let converted = { (info: WebPage.ElementInfo) in
            let menuView = menuItems(info)
            return NSHostingMenu(rootView: menuView)
        }

        return environment(\.webViewContextMenuContext, .init(menu: converted))
#else
        return self
#endif
    }
}

struct ContextMenuContext {
#if os(macOS)
    let menu: (WebPage.ElementInfo) -> NSMenu
#endif
}

struct FindContext {
    var isPresented: Binding<Bool>?
    var canFind = true
    var canReplace = true
}
