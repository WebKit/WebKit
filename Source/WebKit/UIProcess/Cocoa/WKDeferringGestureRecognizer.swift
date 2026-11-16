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

import Foundation
import WebKit_Internal

@objc
@implementation
extension WKDeferringGestureRecognizer {
    private weak var deferringGestureDelegate: (any WKDeferringGestureRecognizerDelegate)? = nil

    var immediatelyFailsAfterTouchEnd: Bool = false

    init(deferringGestureDelegate: any WKDeferringGestureRecognizerDelegate) {
        self.deferringGestureDelegate = deferringGestureDelegate
        super.init(target: nil, action: nil)
    }

    // -initWithCoder: is only a designated initializer for NSGestureRecognizer
    #if WTF_PLATFORM_MAC
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @objc
    public required dynamic init?(coder: NSCoder) {
        super.init(coder: coder)
    }
    #endif

    @objc
    override init(target: Any?, action: Selector?) {
        super.init(target: target, action: action)
    }

    @objc(shouldDeferGestureRecognizer:)
    func shouldDefer(_ gestureRecognizer: PlatformGestureRecognizer) -> Bool {
        deferringGestureDelegate?.deferringGestureRecognizer(self, shouldDeferOtherGestureRecognizer: gestureRecognizer) ?? false
    }

    func endDeferralShouldPreventGestures(_ shouldPreventGestures: Bool) {
        state = shouldPreventGestures ? .ended : .failed
    }
}

// MARK: Overrides

extension WKDeferringGestureRecognizer {
    func internalSetState(previousState: PlatformGestureRecognizerState, newValue: PlatformGestureRecognizerState) {
        if previousState != state {
            deferringGestureDelegate?.deferringGestureRecognizer(self, didTransitionTo: newValue)
        }
    }

    func internalTouchesBegan(with event: PlatformEvent, callSuper: () -> Void) {
        let shouldDeferGestures =
            deferringGestureDelegate?.deferringGestureRecognizer(self, shouldDeferGesturesForEventThatWillBeginTouches: event) ?? false
        callSuper()

        if !shouldDeferGestures {
            state = .failed
        }
    }

    func internalTouchesEnded(with event: PlatformEvent, callSuper: () -> Void) {
        callSuper()

        if immediatelyFailsAfterTouchEnd {
            state = .failed
        }

        deferringGestureDelegate?.deferringGestureRecognizer(self, didEndTouchesWith: event)
    }

    func internalTouchesCancelled(with event: PlatformEvent, callSuper: () -> Void) {
        callSuper()
        state = .failed
    }

    func internalCanBePrevented(by gestureRecognizer: PlatformGestureRecognizer) -> Bool {
        false
    }
}

#if WTF_PLATFORM_IOS_FAMILY

extension WKDeferringGestureRecognizer {
    @_implementationOnly
    open override var state: UIGestureRecognizer.State {
        get {
            super.state
        }
        set {
            let previousState = state
            super.state = newValue

            internalSetState(previousState: previousState, newValue: newValue)
        }
    }

    @_implementationOnly
    open override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
        internalTouchesBegan(with: event) {
            super.touchesBegan(touches, with: event)
        }
    }

    @_implementationOnly
    open override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent) {
        internalTouchesEnded(with: event) {
            super.touchesEnded(touches, with: event)
        }
    }

    @_implementationOnly
    open override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent) {
        internalTouchesCancelled(with: event) {
            super.touchesCancelled(touches, with: event)
        }
    }

    @_implementationOnly
    open override func canBePrevented(by gestureRecognizer: UIGestureRecognizer) -> Bool {
        internalCanBePrevented(by: gestureRecognizer)
    }
}

#elseif HAVE_APPKIT_GESTURES_SUPPORT

extension WKDeferringGestureRecognizer {
    @_implementationOnly
    open override var state: NSGestureRecognizer.State {
        get {
            super.state
        }
        set {
            let previousState = state
            super.state = newValue

            internalSetState(previousState: previousState, newValue: newValue)
        }
    }

    @_implementationOnly
    open override func touchesBegan(with event: NSEvent) {
        internalTouchesBegan(with: event) {
            super.touchesBegan(with: event)
        }
    }

    @_implementationOnly
    open override func touchesEnded(with event: NSEvent) {
        internalTouchesEnded(with: event) {
            super.touchesEnded(with: event)
        }
    }

    @_implementationOnly
    open override func touchesCancelled(with event: NSEvent) {
        internalTouchesCancelled(with: event) {
            super.touchesCancelled(with: event)
        }
    }

    @_implementationOnly
    open override func canBePrevented(by gestureRecognizer: NSGestureRecognizer) -> Bool {
        internalCanBePrevented(by: gestureRecognizer)
    }

    // swift-format-ignore: NoLeadingUnderscores
    @_implementationOnly
    open override func _acceptedEventMask() -> NSEvent.EventTypeMask {
        // Intentionally restricted to direct touches; expand if other event masks are ever needed.
        .directTouch
    }
}

#endif
