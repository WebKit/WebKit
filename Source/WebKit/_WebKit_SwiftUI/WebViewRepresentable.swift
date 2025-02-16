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

public import SwiftUI
@_spi(Private) @_spi(CrossImportOverlay) import WebKit
internal import WebKit_Internal

@MainActor
struct WebViewRepresentable {
    let owner: WebView

    func makePlatformView(context: Context) -> CocoaWebViewAdapter {
        // FIXME: Make this more robust by figuring out what happens when a WebPage moves between representables.
        // We can't have multiple owners regardless, but we'll want to decide if it's an error, if we can handle it gracefully, and how deterministic it might even be.
        // Perhaps we should keep an ownership assertion which we can tear down in something like dismantleUIView().

        precondition(!owner.page.isBoundToWebView, "This web page is already bound to another web view.")

        let parent = CocoaWebViewAdapter()
        parent.webView = owner.page.backingWebView
        owner.page.isBoundToWebView = true

        return parent
    }

    func updatePlatformView(_ platformView: CocoaWebViewAdapter, context: Context) {
        let webView = owner.page.backingWebView
        let environment = context.environment

        platformView.webView = webView

        webView.allowsBackForwardNavigationGestures = environment.webViewAllowsBackForwardNavigationGestures.value != .disabled
        webView.allowsLinkPreview = environment.webViewAllowsLinkPreview.value != .disabled

        webView.configuration.preferences.isTextInteractionEnabled = environment.webViewTextSelection
        webView.configuration.preferences.isElementFullscreenEnabled = environment.webViewAllowsElementFullscreen

        context.coordinator.update(platformView, configuration: self, environment: environment)

#if os(macOS) && !targetEnvironment(macCatalyst)
        owner.page.setMenuBuilder(environment.webViewContextMenuContext?.menu)
#endif
    }

    func makeCoordinator() -> WebViewCoordinator {
        WebViewCoordinator(configuration: self)
    }
}

@MainActor
final class WebViewCoordinator {
    init(configuration: WebViewRepresentable) {
        self.configuration = configuration
    }

    var configuration: WebViewRepresentable

    func update(_ view: CocoaWebViewAdapter, configuration: WebViewRepresentable, environment: EnvironmentValues) {
        self.configuration = configuration

        self.updateFindInteraction(view, environment: environment)
    }

    private func updateFindInteraction(_ view: CocoaWebViewAdapter, environment: EnvironmentValues) {
        guard let webView = view.webView else {
            return
        }

        let findContext = environment.webViewFindContext
        view.findContext = findContext

#if os(iOS)
        webView.isFindInteractionEnabled = findContext.canFind
#endif

        guard let findInteraction = view.findInteraction else {
            return
        }

        let isFindNavigatorVisible = view.isFindNavigatorVisible

        // Showing or hiding the find navigator can change the first responder, which triggers a graph cycle if done synchronously.
        if findContext.canFind && findContext.isPresented?.wrappedValue == true && !isFindNavigatorVisible {
            onNextMainRunLoop {
                findInteraction.presentFindNavigator(showingReplace: false)
            }
        } else if findContext.isPresented?.wrappedValue == false && isFindNavigatorVisible {
            onNextMainRunLoop {
                findInteraction.dismissFindNavigator()
            }
        }
    }
}

#if canImport(UIKit)
extension WebViewRepresentable: UIViewRepresentable {
    func makeUIView(context: Context) -> CocoaWebViewAdapter {
        makePlatformView(context: context)
    }

    func updateUIView(_ uiView: CocoaWebViewAdapter, context: Context) {
        updatePlatformView(uiView, context: context)
    }
}
#else
extension WebViewRepresentable: NSViewRepresentable {
    func makeNSView(context: Context) -> CocoaWebViewAdapter {
        makePlatformView(context: context)
    }

    func updateNSView(_ nsView: CocoaWebViewAdapter, context: Context) {
        updatePlatformView(nsView, context: context)
    }
}
#endif
