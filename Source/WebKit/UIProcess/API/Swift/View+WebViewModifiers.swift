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
public import SwiftUI

extension EnvironmentValues {
    @Entry var webViewAllowsBackForwardNavigationGestures = false

    @Entry var webViewAllowsLinkPreview = true

    @Entry var webViewAllowsTabFocusingLinks = false

    @Entry var webViewAllowsTextInteraction = true

    @Entry var webViewAllowsElementFullscreen = false

    @Entry var webViewFindContext: FindContext = .init()

    @Entry var webViewContextMenuContext: ContextMenuContext? = nil
}

extension View {
    @_spi(Private)
    public func webViewAllowsBackForwardNavigationGestures(_ value: Bool = true) -> some View {
        environment(\.webViewAllowsBackForwardNavigationGestures, value)
    }

    @_spi(Private)
    public func webViewAllowsLinkPreview(_ value: Bool = true) -> some View {
        environment(\.webViewAllowsLinkPreview, value)
    }

    @_spi(Private)
    public func webViewAllowsTabFocusingLinks(_ value: Bool = true) -> some View {
        environment(\.webViewAllowsTabFocusingLinks, value)
    }

    @_spi(Private)
    public func webViewAllowsTextInteraction(_ value: Bool = true) -> some View {
        environment(\.webViewAllowsTextInteraction, value)
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
    public func webViewContextMenu<M>(@ViewBuilder menuItems: @escaping (WebPage_v0.ElementInfo) -> M) -> some View where M: View {
#if os(macOS)
        let converted = { (info: WebPage_v0.ElementInfo) in
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
    let menu: (WebPage_v0.ElementInfo) -> NSMenu
#endif
}

struct FindContext {
    var isPresented: Binding<Bool>?
    var canFind = true
    var canReplace = true
}

#endif
