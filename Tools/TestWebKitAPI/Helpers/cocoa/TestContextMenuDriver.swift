// Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#if WTF_PLATFORM_IOS_FAMILY && USE_UICONTEXTMENU

import CoreGraphics
import Foundation
// Not a private import, because the class's members use the SPI types this header declares or imports.
import TestWebKitAPILibrary.Helpers.cocoa.TestContextMenuDriver
import UIKit

@objc
@implementation
extension TestContextMenuDriver {
    weak var delegate: (any _UIClickInteractionDriverDelegate)?

    weak var view: UIView?

    var allowableMovement: CGFloat = 0

    var primaryGestureRecognizer: UIGestureRecognizer?

    var touchDuration: TimeInterval = 0

    var cancelsTouchesInView = false

    class var requiresForceCapability: Bool {
        false
    }

    class var prefersCancelsTouchesInView: Bool {
        false
    }

    var hasExceededAllowableMovement: Bool {
        false
    }

    var isCurrentlyAcceleratedByForce: Bool {
        false
    }

    var clicksUpAutomaticallyAfterTimeout: Bool {
        false
    }

    var maximumEffectProgress: CGFloat {
        0
    }

    var inputPrecision: _UIInputPrecision {
        .unknown
    }

    @objc(locationInCoordinateSpace:)
    func location(in coordinateSpace: (any UICoordinateSpace)?) -> CGPoint {
        .zero
    }

    func cancelInteraction() {
    }

    @objc(begin:)
    func begin() async -> Bool {
        guard let delegate else {
            return false
        }

        return await withCheckedContinuation { continuation in
            delegate.clickDriver(
                self,
                shouldBegin: { result in
                    // Swift imports this completion handler as @Sendable, but UIKit calls it on the main thread.
                    MainActor.assumeIsolated {
                        self.delegate?.clickDriver(self, didPerform: .began)
                    }
                    continuation.resume(returning: result == .begin)
                }
            )
        }
    }

    func clickDown() {
        delegate?.clickDriver(self, didPerform: .clickedDown)
    }

    func clickUp() {
        delegate?.clickDriver(self, didPerform: .clickedUp)
    }

    func end() {
        delegate?.clickDriver(self, didPerform: .ended)
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY && USE_UICONTEXTMENU
