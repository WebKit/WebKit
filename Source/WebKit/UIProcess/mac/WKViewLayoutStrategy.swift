// Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#if WTF_PLATFORM_MAC

import AppKit
import WebKit_Internal

private enum LayoutBehavior {
    case viewSize
    case fixedSize
    case dynamicSizeComputedFromViewScale
    case dynamicSizeComputedFromMinimumDocumentSize

    init(_ mode: WKLayoutMode) {
        // Any other mode behaves like kWKLayoutModeViewSize.
        self =
            switch mode {
            case WKLayoutMode(kWKLayoutModeFixedSize): .fixedSize
            case WKLayoutMode(kWKLayoutModeDynamicSizeComputedFromViewScale): .dynamicSizeComputedFromViewScale
            case WKLayoutMode(kWKLayoutModeDynamicSizeComputedFromMinimumDocumentSize): .dynamicSizeComputedFromMinimumDocumentSize
            default: .viewSize
            }
    }
}

@objc
@implementation
extension WKViewLayoutStrategy {
    @nonobjc
    private weak var webView: WKWebView?

    @nonobjc
    private let behavior: LayoutBehavior

    let layoutMode: WKLayoutMode

    init(webView: WKWebView, mode: WKLayoutMode) {
        self.webView = webView
        self.behavior = LayoutBehavior(mode)
        self.layoutMode = mode
        super.init()

        if let page = webView._protectedPage().get() {
            switch behavior {
            case .viewSize:
                page.setUseFixedLayout(false)
            case .fixedSize, .dynamicSizeComputedFromViewScale:
                page.setUseFixedLayout(true)
            case .dynamicSizeComputedFromMinimumDocumentSize:
                page.setShouldScaleViewToFitDocument(true)
            }
        } else {
            // WebViewImpl creates its first strategy before the web view has a page. That strategy is always
            // kWKLayoutModeViewSize, and a new page doesn't use fixed layout, so there is nothing to change yet.
            assert(behavior == .viewSize)
        }

        updateLayout()
    }

    func invalidate() {
        webView = nil
    }

    func didChangeViewScale() {
        updateLayout()
    }

    func didChangeFrameSize() {
        guard let webView, let impl = webView._impl() else {
            return
        }

        if impl.clipsToVisibleRect() {
            impl.updateViewExposedRect()
        }
        impl.setDrawingAreaSize(webView.frame.size)

        updateLayout()
    }

    func willChangeLayoutStrategy() {
        guard behavior == .dynamicSizeComputedFromMinimumDocumentSize, let page = webView?._protectedPage().get() else {
            return
        }

        page.setShouldScaleViewToFitDocument(false)
        page.scaleView(1)
    }

    // Only the layout size computed from the view scale depends on the view's frame and scale.
    @nonobjc
    private final func updateLayout() {
        guard behavior == .dynamicSizeComputedFromViewScale, let webView, let page = webView._protectedPage().get(),
            let impl = webView._impl()
        else {
            return
        }

        let inverseScale: CGFloat = 1 / page.viewScaleFactor()
        let size = webView.frame.size
        impl.setFixedLayoutSize(CGSize(width: size.width * inverseScale, height: size.height * inverseScale))
    }
}

#endif // WTF_PLATFORM_MAC
