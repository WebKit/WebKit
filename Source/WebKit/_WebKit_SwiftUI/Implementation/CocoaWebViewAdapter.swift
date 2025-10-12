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

public import SwiftUI
@_spi(Private) @_spi(CrossImportOverlay) import WebKit

#if canImport(UIKit)
typealias CocoaView = UIView
#else
typealias CocoaView = NSView
#endif

@MainActor
class CocoaWebViewAdapter: CocoaView, PlatformTextSearching {
    #if os(iOS)
    var extrinsicSafeAreaInsets: EdgeInsets? = nil {
        didSet {
            guard oldValue != extrinsicSafeAreaInsets else {
                return
            }
            safeAreaInsetsDidChange()
        }
    }

    override var safeAreaInsets: UIEdgeInsets {
        guard let extrinsicSafeAreaInsets else {
            return .zero
        }

        var leftSafeArea = extrinsicSafeAreaInsets.leading
        var rightSafeArea = extrinsicSafeAreaInsets.trailing

        if effectiveUserInterfaceLayoutDirection == .rightToLeft {
            leftSafeArea = extrinsicSafeAreaInsets.trailing
            rightSafeArea = extrinsicSafeAreaInsets.leading
        }

        return UIEdgeInsets(
            top: extrinsicSafeAreaInsets.top,
            left: leftSafeArea,
            bottom: extrinsicSafeAreaInsets.bottom,
            right: rightSafeArea
        )
    }
    #endif

    // MARK: PlatformTextSearching conformance

    #if os(macOS)
    typealias FindInteraction = NSTextFinderAdapter
    #else
    typealias FindInteraction = UIFindInteractionAdapter
    #endif

    var isFindNavigatorVisible: Bool {
        #if os(macOS)
        isFindBarVisible
        #else
        webView?.findInteraction?.isFindNavigatorVisible ?? false
        #endif
    }

    lazy var findInteraction: FindInteraction? = {
        #if os(macOS)
        let interaction = NSTextFinder()
        interaction.isIncrementalSearchingEnabled = true
        interaction.incrementalSearchingShouldDimContentView = false
        interaction.client = webView
        interaction.findBarContainer = self
        #else
        guard let interaction = webView?.findInteraction else {
            return nil
        }
        #endif

        return .init(wrapped: interaction)
    }()

    #if os(macOS)
    var isFindBarVisible: Bool = false {
        didSet {
            guard oldValue != isFindBarVisible else {
                return
            }

            #if canImport(SwiftUI, _version: "7.0.57")
            if let isPresented = findContext?.isPresented {
                isPresented.wrappedValue = isFindBarVisible
            }
            #endif

            if isFindBarVisible {
                findBarDidBecomeVisible()
            } else {
                findBarDidBecomeHidden()
            }
        }
    }

    var findBarView: CocoaView? = nil
    #endif

    // MARK: Find-in-Page support

    #if canImport(SwiftUI, _version: "7.0.57")
    var findContext: FindContext?
    #endif

    var scrollPosition: ScrollPositionContext?

    #if os(macOS)
    // This is called by the Find menu items in the Menu Bar
    @objc(performFindPanelAction:)
    func performFindPanelAction(_ sender: Any!) {
        guard let item = sender as? NSMenuItem, let action = NSTextFinder.Action(rawValue: item.tag) else {
            fatalError()
        }

        onNextMainRunLoop { [weak self] in
            self?.findInteraction?.wrapped.performAction(action)
        }
    }

    private func findBarDidBecomeVisible() {
        guard let webView else {
            return
        }

        guard let findBarView else {
            preconditionFailure("find bar view was nil")
        }

        findBarView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(findBarView)

        findBarConstraints = [
            findBarView.widthAnchor.constraint(equalTo: widthAnchor),
            findBarView.leadingAnchor.constraint(equalTo: leadingAnchor),
            findBarView.trailingAnchor.constraint(equalTo: trailingAnchor),
            findBarView.topAnchor.constraint(equalTo: topAnchor),
            webView.topAnchor.constraint(equalTo: findBarView.bottomAnchor),
        ]

        NSLayoutConstraint.activate(findBarConstraints)

        webViewHeightConstraint?.isActive = false
    }

    private func findBarDidBecomeHidden() {
        findBarView?.translatesAutoresizingMaskIntoConstraints = true

        for constraint in findBarConstraints {
            constraint.isActive = false
        }

        findBarConstraints = []

        webViewHeightConstraint?.isActive = true

        findBarView?.removeFromSuperview()
        webView?._hideFindUI()

        onNextMainRunLoop { [weak self] in
            guard let webView = self?.webView else {
                return
            }

            webView.window?.makeFirstResponder(webView)
        }
    }
    #endif

    // MARK: Scroll Geometry

    var onScrollGeometryChange: OnScrollGeometryChangeContext? = nil {
        willSet {
            webView?.setNeedsScrollGeometryUpdates(newValue != nil)
        }
    }

    private var currentScrollGeometry = ScrollGeometry(
        contentOffset: .zero,
        contentSize: .zero,
        contentInsets: .init(),
        containerSize: .zero
    )

    // MARK: Constraints

    private var webViewConstraints: [NSLayoutConstraint] = []
    private var findBarConstraints: [NSLayoutConstraint] = []
    private var webViewHeightConstraint: NSLayoutConstraint? = nil

    private func removeConstraints() {
        for constraint in webViewConstraints + findBarConstraints {
            constraint.isActive = false
        }

        webViewHeightConstraint?.isActive = false

        webViewConstraints = []
        findBarConstraints = []
        webViewHeightConstraint = nil
    }

    private func activateConstraints() {
        guard let webView else {
            preconditionFailure("web view was nil")
        }

        webViewConstraints = [
            webView.widthAnchor.constraint(equalTo: widthAnchor),
            webView.leadingAnchor.constraint(equalTo: leadingAnchor),
            webView.bottomAnchor.constraint(equalTo: bottomAnchor),
        ]

        let heightConstraint = webView.heightAnchor.constraint(equalTo: heightAnchor)
        self.webViewHeightConstraint = heightConstraint

        NSLayoutConstraint.activate(webViewConstraints + [heightConstraint])
    }

    // MARK: Main content view

    var webView: WebPageWebView? = nil {
        willSet {
            guard webView != newValue else {
                return
            }

            removeConstraints()

            webView?.removeFromSuperview()
        }
        didSet {
            guard oldValue != webView, let webView else {
                return
            }

            webView.translatesAutoresizingMaskIntoConstraints = false
            addSubview(webView)

            activateConstraints()

            webView.delegate = self
            #if os(macOS)
            findInteraction?.wrapped.client = webView
            #endif
        }
    }
}

#if os(macOS)
extension CocoaWebViewAdapter: @preconcurrency NSTextFinderBarContainer {
    func contentView() -> CocoaView? {
        webView
    }

    func findBarViewDidChangeHeight() {
    }
}
#endif

extension CocoaWebViewAdapter: WebPageWebView.Delegate {
    #if os(iOS)
    func findInteraction(_ interaction: UIFindInteraction, didBegin session: UIFindSession) {
        #if canImport(SwiftUI, _version: "7.0.57")
        if let isPresented = findContext?.isPresented {
            isPresented.wrappedValue = true
        }
        #endif
    }

    func findInteraction(_ interaction: UIFindInteraction, didEnd session: UIFindSession) {
        #if canImport(SwiftUI, _version: "7.0.57")
        if let isPresented = findContext?.isPresented {
            isPresented.wrappedValue = false
        }
        #endif
    }

    func supportsTextReplacement() -> Bool {
        #if canImport(SwiftUI, _version: "7.0.57")
        findContext?.supportsReplace ?? false
        #else
        false
        #endif
    }
    #endif // os(iOS)

    func geometryDidChange(_ geometry: WKScrollGeometryAdapter) {
        let newScrollGeometry = ScrollGeometry(geometry)

        defer {
            self.currentScrollGeometry = newScrollGeometry
        }

        let oldScrollGeometry = self.currentScrollGeometry

        guard let onScrollGeometryChange = self.onScrollGeometryChange else {
            return
        }

        let transformedNew = onScrollGeometryChange.transform(newScrollGeometry)
        let transformedOld = onScrollGeometryChange.transform(oldScrollGeometry)

        guard transformedOld != transformedNew else {
            return
        }

        onScrollGeometryChange.action(transformedOld, transformedNew)
    }
}

#endif
