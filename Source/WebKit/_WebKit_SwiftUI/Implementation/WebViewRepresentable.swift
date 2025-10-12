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

internal import SwiftUI
internal import os
@_spi(CrossImportOverlay) import WebKit
internal import WebKit_Private.WKPreferencesPrivate

extension Logger {
    fileprivate static let webView = Logger(subsystem: "com.apple.WebKit", category: "SwiftUIWebView")
}

@MainActor
struct WebViewRepresentable {
    let page: WebPage
    let safeAreaInsets: EdgeInsets

    func makePlatformView(context: Context) -> CocoaWebViewAdapter {
        // FIXME: Make this more robust by figuring out what happens when a WebPage moves between representables.
        // We can't have multiple owning pages regardless, but we'll want to decide if it's an error, if we can handle it gracefully, and how deterministic it might even be.
        // Perhaps we should keep an ownership assertion which we can tear down in something like dismantleUIView().

        precondition(!page.isBoundToWebView, "This web page is already bound to another web view.")

        let parent = CocoaWebViewAdapter()
        parent.webView = page.backingWebView
        #if os(iOS)
        parent.extrinsicSafeAreaInsets = safeAreaInsets
        #endif
        page.isBoundToWebView = true

        return parent
    }

    func updatePlatformView(_ platformView: CocoaWebViewAdapter, context: Context) {
        let webView = page.backingWebView
        let environment = context.environment

        #if os(iOS)
        platformView.extrinsicSafeAreaInsets = safeAreaInsets
        #endif
        platformView.webView = webView

        webView.allowsBackForwardNavigationGestures = environment.webViewAllowsBackForwardNavigationGestures.value != .disabled
        webView.allowsLinkPreview = environment.webViewAllowsLinkPreview.value != .disabled
        webView.allowsMagnification = environment.webViewMagnificationGestures.value != .disabled

        let isOpaque = environment.webViewContentBackground != .hidden

        #if os(macOS)
        if webView._drawsBackground != isOpaque {
            webView._drawsBackground = isOpaque
        }
        #else
        if webView.isOpaque != isOpaque {
            webView.isOpaque = isOpaque
        }
        #endif

        #if os(visionOS)
        if let scrollInputBehavior = environment.webViewScrollInputBehaviorContext {
            if scrollInputBehavior.input == .look {
                webView.configuration.preferences._overlayRegionsEnabled = scrollInputBehavior.behavior != .disabled
            } else {
                Logger.webView.error("Only the `.look` ScrollInputKind is supported.")
            }
        }
        #endif

        #if os(macOS)
        if let scrollEdgeEffectStyle = environment.webViewScrollEdgeEffectStyleContext {
            webView._usesAutomaticContentInsetBackgroundFill = scrollEdgeEffectStyle.style != .hard
            webView.obscuredContentInsets = .init(top: 0, left: 0, bottom: 0, right: 0)
            webView._automaticallyAdjustsContentInsets = true
        }
        #endif

        if EquatableScrollBounceBehavior(environment.verticalScrollBounceBehavior) == .always
            || EquatableScrollBounceBehavior(environment.verticalScrollBounceBehavior) == .automatic
        {
            webView.alwaysBounceVertical = true
            webView.bouncesVertically = true
        } else if EquatableScrollBounceBehavior(environment.verticalScrollBounceBehavior) == .basedOnSize {
            webView.alwaysBounceVertical = false
            webView.bouncesVertically = true
        }

        if EquatableScrollBounceBehavior(environment.horizontalScrollBounceBehavior) == .always
            || EquatableScrollBounceBehavior(environment.horizontalScrollBounceBehavior) == .automatic
        {
            webView.alwaysBounceHorizontal = true
            webView.bouncesHorizontally = true
        } else if EquatableScrollBounceBehavior(environment.horizontalScrollBounceBehavior) == .basedOnSize {
            webView.alwaysBounceHorizontal = false
            webView.bouncesHorizontally = true
        }

        webView.configuration.preferences.isTextInteractionEnabled = environment.webViewTextSelection
        webView.configuration.preferences.isElementFullscreenEnabled = environment.webViewElementFullscreenBehavior.value == .enabled

        platformView.onScrollGeometryChange = environment.webViewOnScrollGeometryChange

        context.coordinator.update(platformView, configuration: self, context: context)

        #if os(macOS) && !targetEnvironment(macCatalyst)
        if let menu = environment.webViewContextMenuContext?.menu {
            page.setMenuBuilder {
                menu(.init(linkURL: $0.linkURL))
            }
        } else {
            page.setMenuBuilder(nil)
        }
        #endif
    }

    func makeCoordinator() -> WebViewCoordinator {
        WebViewCoordinator(configuration: self)
    }

    func sizeThatFits(_ proposal: ProposedViewSize, platformView: CocoaWebViewAdapter, context: Context) -> CGSize? {
        guard let width = proposal.width, let height = proposal.height else {
            return nil
        }

        // By default, SwiftUI allows representable views to have fractional sizes, however WebKit does not support this
        // (it may result in incorrect behavior such as the size of the content view and scroll view being slightly mismatched).
        //
        // Rounding down is needed to ensure that the view is never bigger than the requested size, otherwise miscellaneous UI
        // issues manifest.
        return CGSize(width: width.rounded(.down), height: height.rounded(.down))
    }

    static func dismantlePlatformView(_ platformView: CocoaWebViewAdapter, coordinator: WebViewCoordinator) {
        coordinator.configuration.page.isBoundToWebView = false
    }
}

@MainActor
final class WebViewCoordinator {
    init(configuration: WebViewRepresentable) {
        self.configuration = configuration
    }

    var configuration: WebViewRepresentable

    func update(_ view: CocoaWebViewAdapter, configuration: WebViewRepresentable, context: WebViewRepresentable.Context) {
        self.configuration = configuration

        #if canImport(SwiftUI, _version: "7.0.57")
        updateFindInteraction(view, context: context)
        #endif
        updateScrollPosition(view, context: context)
    }

    private func updateScrollPosition(_ view: CocoaWebViewAdapter, context: WebViewRepresentable.Context) {
        guard let webView = view.webView else {
            return
        }

        let environment = context.environment

        // FIXME: Use the binding to update the `isPositionedByUser` property when applicable.

        let scrollPosition = environment.webViewScrollPositionContext

        let scrollPositionDidNotChange = view.scrollPosition?.position?.wrappedValue == scrollPosition.position?.wrappedValue
        guard !scrollPositionDidNotChange else {
            return
        }

        view.scrollPosition = scrollPosition
        let scrollPositionValue = scrollPosition.position?.wrappedValue

        if let point = scrollPositionValue?.point {
            webView.setContentOffset(x: point.x, y: point.y, animated: context.transaction.isAnimated)
        } else if let edge = scrollPositionValue?.edge {
            webView.scrollTo(edge: NSDirectionalRectEdge(edge), animated: context.transaction.isAnimated)
        } else if let x = scrollPositionValue?.x {
            webView.setContentOffset(x: x, y: nil, animated: context.transaction.isAnimated)
        } else if let y = scrollPositionValue?.y {
            webView.setContentOffset(x: nil, y: y, animated: context.transaction.isAnimated)
        }
    }

    #if canImport(SwiftUI, _version: "7.0.57")
    private func updateFindInteraction(_ view: CocoaWebViewAdapter, context: WebViewRepresentable.Context) {
        guard let webView = view.webView else {
            return
        }

        let environment = context.environment

        let findContext = environment.findContext
        view.findContext = findContext

        #if os(iOS)
        webView.isFindInteractionEnabled = findContext != nil
        #endif

        guard let findInteraction = view.findInteraction else {
            return
        }

        let isFindNavigatorVisible = view.isFindNavigatorVisible

        // Showing or hiding the find navigator can change the first responder, which triggers a graph cycle if done synchronously.
        if let findContext, findContext.isPresented?.wrappedValue == true && !isFindNavigatorVisible {
            onNextMainRunLoop {
                findInteraction.presentFindNavigator(showingReplace: false)
            }
        } else if findContext?.isPresented?.wrappedValue == false && isFindNavigatorVisible {
            onNextMainRunLoop {
                findInteraction.dismissFindNavigator()
            }
        }
    }
    #endif // canImport(SwiftUI, _version: "7.0.57")
}

#if canImport(UIKit)
extension WebViewRepresentable: UIViewRepresentable {
    func makeUIView(context: Context) -> CocoaWebViewAdapter {
        makePlatformView(context: context)
    }

    func updateUIView(_ uiView: CocoaWebViewAdapter, context: Context) {
        updatePlatformView(uiView, context: context)
    }

    func sizeThatFits(_ proposal: ProposedViewSize, uiView: CocoaWebViewAdapter, context: Context) -> CGSize? {
        sizeThatFits(proposal, platformView: uiView, context: context)
    }

    static func dismantleUIView(_ uiView: CocoaWebViewAdapter, coordinator: WebViewCoordinator) {
        dismantlePlatformView(uiView, coordinator: coordinator)
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

    func sizeThatFits(_ proposal: ProposedViewSize, nsView: CocoaWebViewAdapter, context: Context) -> CGSize? {
        sizeThatFits(proposal, platformView: nsView, context: context)
    }

    static func dismantleNSView(_ nsView: CocoaWebViewAdapter, coordinator: WebViewCoordinator) {
        dismantlePlatformView(nsView, coordinator: coordinator)
    }
}
#endif

// FIXME: (rdar://145030632) Remove this workaround when possible.
struct EquatableScrollBounceBehavior: Equatable {
    static let automatic = Self(.automatic)

    static let always = Self(.always)

    static let basedOnSize = Self(.basedOnSize)

    init(_ behavior: ScrollBounceBehavior) {
        self.behavior = behavior
    }

    let behavior: ScrollBounceBehavior

    static func == (lhs: EquatableScrollBounceBehavior, rhs: EquatableScrollBounceBehavior) -> Bool {
        unsafeBitCast(lhs.behavior, to: Int8.self) == unsafeBitCast(rhs.behavior, to: Int8.self)
    }
}

#endif
