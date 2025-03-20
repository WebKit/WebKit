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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation
internal import WebKit_Internal

@MainActor
@_spi(CrossImportOverlay)
public final class WebPageWebView: WKWebView {
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

    override func supportsTextReplacement() -> Bool {
        guard let delegate else {
            return super.supportsTextReplacement()
        }

        return super.supportsTextReplacement() && delegate.supportsTextReplacement()
    }
#endif

    func geometryDidChange(_ geometry: WKScrollGeometryAdapter) {
        delegate?.geometryDidChange(geometry)
    }
}

extension WebPageWebView {
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
    public var alwaysBounceVertical: Bool {
        get { scrollView.alwaysBounceVertical }
        set { scrollView.alwaysBounceVertical = newValue }
    }

    public var alwaysBounceHorizontal: Bool {
        get { scrollView.alwaysBounceHorizontal }
        set { scrollView.alwaysBounceHorizontal = newValue }
    }

    public var bouncesVertically: Bool {
        get { scrollView.bouncesVertically }
        set { scrollView.bouncesVertically = newValue }
    }

    public var bouncesHorizontally: Bool {
        get { scrollView.bouncesHorizontally }
        set { scrollView.bouncesHorizontally = newValue }
    }

    public var allowsMagnification: Bool {
        get { self._allowsMagnification }
        set { self._allowsMagnification = newValue }
    }

    public func setContentOffset(_ offset: CGPoint, animated: Bool) {
        scrollView.setContentOffset(offset, animated: animated)
    }

    public func scrollTo(edge: NSDirectionalRectEdge, animated: Bool) {
        self._scroll(to: _WKRectEdge(edge), animated: animated)
    }
#else
    public var alwaysBounceVertical: Bool {
        get { self._alwaysBounceVertical }
        set { self._alwaysBounceVertical = newValue }
    }

    public var alwaysBounceHorizontal: Bool {
        get { self._alwaysBounceHorizontal }
        set { self._alwaysBounceHorizontal = newValue }
    }

    public var bouncesVertically: Bool {
        get { self._rubberBandingEnabled.contains(.top) && self._rubberBandingEnabled.contains(.bottom) }
        set { self._rubberBandingEnabled.formUnion([.top, .bottom]) }
    }

    public var bouncesHorizontally: Bool {
        get { self._rubberBandingEnabled.contains(.left) && self._rubberBandingEnabled.contains(.right) }
        set { self._rubberBandingEnabled.formUnion([.left, .right]) }
    }

    public func setContentOffset(_ offset: CGPoint, animated: Bool) {
        self._setContentOffset(offset, animated: animated)
    }

    public func scrollTo(edge: NSDirectionalRectEdge, animated: Bool) {
        self._scroll(to: _WKRectEdge(edge), animated: animated)
    }
#endif
}

#endif
