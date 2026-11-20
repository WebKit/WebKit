// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#if HAVE_APPKIT_GESTURES_SUPPORT

import AppKit
import WebKit_Internal
import os
private import WebCore_Private

extension WebKit.InteractionInformationAtPosition {
    var representsDraggableElement: Bool {
        isLink || isImage || isAttachment || isDHTMLDraggable || isColorInput || prefersDraggingOverTextSelection
    }

    var representsSelectableContent: Bool {
        isSelectable() || isFocusableWithSelectableText()
    }

    var prefersDirectManipulation: Bool {
        isRangeInput || isARIASlider || hasDirectionalResizeCursor || isInResizeControl || isCustomSlider
            || WebKit.CxxInteropSupport.isInteractiveModel(self)
    }

    var representsSecondaryClickableElement: Bool {
        if prefersDirectManipulation {
            return false
        }

        if representsSelectableContent {
            return true
        }

        if shouldTreatLongClickAsSecondaryClickQuirk {
            return true
        }

        if isImage {
            return true
        }

        return isOverVideo && selectability != .UnselectableDueToFocusableElement
    }
}

extension WebKit.InteractionInformationRequest {
    init(touchAt locationInView: NSPoint) {
        self.init(WebCore.IntPoint(locationInView))
        inputSource = .Touch
    }
}

extension WKAppKitGestureController {
    private static let sharedPositionInformationToleranceRadius: Int32 = 15
    static let panPositionInformationToleranceRadius = sharedPositionInformationToleranceRadius
    static let mouseDownPositionInformationToleranceRadius = sharedPositionInformationToleranceRadius

    // Rounds the recognizer's allowable movement up and converts it as arm64 converts a double to an int,
    // saturating, with NaN as 0, where Int32(_:) would trap.
    func positionInformationToleranceRadius(for recognizer: NSPressGestureRecognizer?) -> Int32 {
        let movement = (recognizer?.allowableMovement ?? 0).rounded(.up)
        return movement.isNaN ? 0 : Int32(min(max(movement, Double(Int32.min)), Double(Int32.max)))
    }

    func isPositionInformationValid(at locationInView: NSPoint, radius: Int32) -> Bool {
        let request = WebKit.InteractionInformationRequest(touchAt: locationInView)
        return positionInformation.currentIsApproximatelyValid(request, radius: radius)
    }

    func isPointInScrollbar(at locationInView: NSPoint) -> Bool {
        view?._impl()?.isPointInScrollbar(locationInView) ?? false
    }

    func isInsideHoverActivatedByClickBounds(at locationInView: NSPoint) -> Bool {
        guard let page = view?._protectedPage().get(), let bounds = page.hoverActivatedByClickBounds().value else {
            return false
        }
        return bounds.contains(WebCore.IntPoint(locationInView))
    }

    func isHoverActivatedByClick(at locationInView: NSPoint) -> Bool {
        if !isPositionInformationValid(at: locationInView, radius: Self.panPositionInformationToleranceRadius) {
            return isInsideHoverActivatedByClickBounds(at: locationInView)
        }

        return positionInformation.currentInformation.isHoverActivatedByClick
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
