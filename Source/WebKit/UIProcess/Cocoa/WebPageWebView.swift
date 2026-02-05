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

import Foundation
internal import WebKit_Internal

// SPI for the cross-import overlay.
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@MainActor
@_spi(CrossImportOverlay)
public final class WebPageWebView: WKWebView {
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public weak var delegate: (any Delegate)? = nil

    #if os(iOS)
    override func findInteraction(_ interaction: UIFindInteraction, didBegin session: UIFindSession) {
        super.findInteraction(interaction, didBegin: session)
        delegate?.findInteraction(interaction, didBegin: session)
    }

    override func findInteraction(_ interaction: UIFindInteraction, didEnd session: UIFindSession) {
        super.findInteraction(interaction, didEnd: session)
        delegate?.findInteraction(interaction, didEnd: session)
    }

    #if USE_APPLE_INTERNAL_SDK
    override func supportsTextReplacement() -> Bool {
        guard let delegate else {
            return super.supportsTextReplacement()
        }

        return super.supportsTextReplacement() && delegate.supportsTextReplacement()
    }
    #else
    override var supportsTextReplacement: Bool {
        guard let delegate else {
            return super.supportsTextReplacement
        }

        return super.supportsTextReplacement && delegate.supportsTextReplacement()
    }
    #endif // USE_APPLE_INTERNAL_SDK
    #endif

    func geometryDidChange(_ geometry: WKScrollGeometryAdapter) {
        delegate?.geometryDidChange(geometry)
    }
}

extension WebPageWebView {
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @MainActor
    public protocol Delegate: AnyObject {
        #if os(iOS)
        func findInteraction(_ interaction: UIFindInteraction, didBegin session: UIFindSession)

        func findInteraction(_ interaction: UIFindInteraction, didEnd session: UIFindSession)

        func supportsTextReplacement() -> Bool
        #endif

        func geometryDidChange(_ geometry: WKScrollGeometryAdapter)
    }
}

extension WebPageWebView {
    // MARK: Platform-agnostic scrolling capabilities

    #if canImport(UIKit)

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var alwaysBounceVertical: Bool {
        get { scrollView.alwaysBounceVertical }
        set { scrollView.alwaysBounceVertical = newValue }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var alwaysBounceHorizontal: Bool {
        get { scrollView.alwaysBounceHorizontal }
        set { scrollView.alwaysBounceHorizontal = newValue }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var bouncesVertically: Bool {
        get { scrollView.bouncesVertically }
        set { scrollView.bouncesVertically = newValue }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var bouncesHorizontally: Bool {
        get { scrollView.bouncesHorizontally }
        set { scrollView.bouncesHorizontally = newValue }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var allowsMagnification: Bool {
        get { self._allowsMagnification }
        set { self._allowsMagnification = newValue }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func setContentOffset(x: Double?, y: Double?, animated: Bool) {
        let currentOffset = scrollView.contentOffset
        let newOffset = CGPoint(x: x ?? currentOffset.x, y: y ?? currentOffset.y)

        scrollView.setContentOffset(newOffset, animated: animated)
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func scrollTo(edge: NSDirectionalRectEdge, animated: Bool) {
        self._scroll(to: _WKRectEdge(edge), animated: animated)
    }

    #else

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var alwaysBounceVertical: Bool {
        get { self._alwaysBounceVertical }
        set { self._alwaysBounceVertical = newValue }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var alwaysBounceHorizontal: Bool {
        get { self._alwaysBounceHorizontal }
        set { self._alwaysBounceHorizontal = newValue }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var bouncesVertically: Bool {
        get { self._rubberBandingEnabled.contains(.top) && self._rubberBandingEnabled.contains(.bottom) }
        set { self._rubberBandingEnabled.formUnion([.top, .bottom]) }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var bouncesHorizontally: Bool {
        get { self._rubberBandingEnabled.contains(.left) && self._rubberBandingEnabled.contains(.right) }
        set { self._rubberBandingEnabled.formUnion([.left, .right]) }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func setContentOffset(x: Double?, y: Double?, animated: Bool) {
        self._setContentOffset(x: x.map(NSNumber.init(value:)), y: y.map(NSNumber.init(value:)), animated: animated)
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func scrollTo(edge: NSDirectionalRectEdge, animated: Bool) {
        self._scroll(to: _WKRectEdge(edge), animated: animated)
    }

    #endif
}

extension WebPageWebView {
    // swift-format-ignore: NoLeadingUnderscores
    override var _nameForVisualIdentificationOverlay: String {
        "WebView (SwiftUI)"
    }
}

extension WebPageWebView {
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func setNeedsScrollGeometryUpdates(_ value: Bool) {
        self._setNeedsScrollGeometryUpdates(value)
    }
}

#endif
