// Copyright (C) 2026 Apple Inc. All rights reserved.
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

@objc
@implementation
extension WKPointerTrackingGestureRecognizer {
    weak var pointerTrackingDelegate: (any WKPointerTrackingGestureRecognizerDelegate)?
}

extension WKPointerTrackingGestureRecognizer {
    private func touchesOnView(of event: NSEvent) -> Set<NSTouch> {
        event.touches(matching: .touching, in: view)
    }

    func internalTouchBegan(atLocationInWindow location: CGPoint, modifierFlags: NSEvent.ModifierFlags) {
        pointerTrackingDelegate?
            .pointerTrackingGestureRecognizer(self, didUpdateTouchWith: .began, locationInWindow: location, modifierFlags: modifierFlags)
    }

    func internalTouchMoved(toLocationInWindow location: CGPoint, modifierFlags: NSEvent.ModifierFlags) {
        pointerTrackingDelegate?
            .pointerTrackingGestureRecognizer(self, didUpdateTouchWith: .changed, locationInWindow: location, modifierFlags: modifierFlags)
    }

    func internalTouchEnded(atLocationInWindow location: CGPoint, modifierFlags: NSEvent.ModifierFlags) {
        pointerTrackingDelegate?
            .pointerTrackingGestureRecognizer(self, didUpdateTouchWith: .ended, locationInWindow: location, modifierFlags: modifierFlags)
    }

    func internalTouchCancelled() {
        pointerTrackingDelegate?
            .pointerTrackingGestureRecognizer(self, didUpdateTouchWith: .cancelled, locationInWindow: .zero, modifierFlags: [])
    }
}

extension WKPointerTrackingGestureRecognizer {
    @_implementationOnly
    open override func touchesBegan(with event: NSEvent) {
        super.touchesBegan(with: event)

        // For now, the pointer follows a single touch.
        let touches = touchesOnView(of: event)
        guard touches.count == 1, let touch = touches.first else {
            internalTouchCancelled()
            return
        }

        internalTouchBegan(atLocationInWindow: touch.location(in: nil), modifierFlags: event.modifierFlags)
    }

    @_implementationOnly
    open override func touchesMoved(with event: NSEvent) {
        super.touchesMoved(with: event)

        let touches = touchesOnView(of: event)
        guard touches.count == 1, let touch = touches.first else {
            return
        }

        internalTouchMoved(toLocationInWindow: touch.location(in: nil), modifierFlags: event.modifierFlags)
    }

    @_implementationOnly
    open override func touchesEnded(with event: NSEvent) {
        super.touchesEnded(with: event)

        guard let touch = event.touches(matching: .ended, in: view).first else {
            return
        }

        internalTouchEnded(atLocationInWindow: touch.location(in: nil), modifierFlags: event.modifierFlags)
    }

    @_implementationOnly
    open override func touchesCancelled(with event: NSEvent) {
        super.touchesCancelled(with: event)
        internalTouchCancelled()
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
