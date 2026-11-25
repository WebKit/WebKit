// Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if WTF_PLATFORM_MAC && ENABLE_REVEAL

import AppKit
import WebKit_Internal
import pal.Core.cocoa.RevealSoftLink
import wtf

@objc
@implementation
extension WKRevealItemPresenter {
    @nonobjc
    private weak var webView: WKWebView?

    @nonobjc
    private let item: RVItem

    let frameInView: CGRect

    let menuLocationInView: CGPoint

    @nonobjc
    private var presenter: RVPresenter?

    @nonobjc
    private var presentingContext: RVPresentingContext?

    @nonobjc
    private var isHighlightingItem = false

    var shouldUseDefaultHighlight = false

    init(webView: WKWebView, item: RVItem, frame frameInView: CGRect, menuLocation menuLocationInView: CGPoint) {
        self.webView = webView
        self.item = item
        self.frameInView = frameInView
        self.menuLocationInView = menuLocationInView
        super.init()

        // Safety: These are safe since the names and types are statically written.
        // FIXME: (rdar://193567775) WebKit should not soft link Reveal.
        guard PAL.isRevealFrameworkAvailable(),
            let presenterClass = unsafe softLinkedClass(named: "RVPresenter", as: RVPresenter.self),
            let presentingContextClass = unsafe softLinkedClass(named: "RVPresentingContext", as: RVPresentingContext.self)
        else {
            return
        }

        presenter = presenterClass.init()
        presentingContext = presentingContextClass.init(pointerLocationInView: menuLocationInView, in: webView, highlightDelegate: self)
    }

    func showContextMenu() {
        guard let webView, let presenter, let presentingContext else {
            return
        }

        guard let menuItems = presenter.menuItems(for: item, documentContext: nil, presentingContext: presentingContext, options: nil),
            !menuItems.isEmpty
        else {
            return
        }

        let menu = NSMenu(title: "")
        menu.autoenablesItems = false
        menu.items = menuItems

        let clickLocationInWindow = webView.convert(menuLocationInView, to: nil)
        // Safety: (rdar://30282391) NSView.window should be weak.
        let windowNumber = unsafe webView.window?.windowNumber ?? 0
        guard
            let event = NSEvent.mouseEvent(
                with: .leftMouseDown,
                location: clickLocationInWindow,
                modifierFlags: [],
                timestamp: 0,
                windowNumber: windowNumber,
                context: nil,
                eventNumber: 0,
                clickCount: 1,
                pressure: 1
            )
        else {
            return
        }
        NSMenu.popUpContextMenu(menu, with: event, for: webView)

        callDidFinishPresentationIfNeeded()
    }

    @nonobjc
    private final func callDidFinishPresentationIfNeeded() {
        guard !isHighlightingItem, let impl = webView?._impl() else {
            return
        }

        impl.didFinishPresentation(self)
    }

    // MARK: RVPresenterHighlightDelegate

    @objc(revealContext:rectsForItem:)
    func revealContext(_ context: RVPresentingContext, rectsFor item: RVItem) -> [NSValue] {
        [NSValue(rect: frameInView)]
    }

    @objc(revealContext:shouldUseDefaultHighlightForItem:)
    func revealContext(_ context: RVPresentingContext, shouldUseDefaultHighlightFor item: RVItem) -> Bool {
        shouldUseDefaultHighlight
    }

    @objc(revealContext:startHighlightingItem:)
    func revealContext(_ context: RVPresentingContext, startHighlighting item: RVItem) {
        isHighlightingItem = true
    }

    @objc(revealContext:stopHighlightingItem:)
    func revealContext(_ context: RVPresentingContext, stopHighlighting item: RVItem) {
        isHighlightingItem = false
        callDidFinishPresentationIfNeeded()
    }
}

#endif // WTF_PLATFORM_MAC && ENABLE_REVEAL
