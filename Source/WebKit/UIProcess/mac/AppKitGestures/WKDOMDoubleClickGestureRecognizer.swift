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
private import AppKit_Private.NSClickGestureRecognizer_Private

final class WKDOMDoubleClickGestureRecognizer: NSClickGestureRecognizer {
    private struct ClickEvent {
        let location: CGPoint
        let time: TimeInterval
    }

    private var click: ClickEvent? = nil

    override var state: NSGestureRecognizer.State {
        get { super.state }
        set {
            guard newValue == .recognized else {
                super.state = newValue
                return
            }

            super.state = isDoubleClick() ? newValue : .failed
        }
    }

    func resetClick() {
        click = nil
    }

    private func isDoubleClick() -> Bool {
        let location = self.location(in: nil)
        let now = self._timestamp

        guard let previousClick = exchange(&click, with: ClickEvent(location: location, time: now)) else {
            return false
        }

        // Match AppKit's criteria for space and time requirements.

        guard .seconds(now - previousClick.time) <= Constants.clickDoubleTimeThreshold else {
            return false
        }

        let movement = CGPoint(x: location.x - previousClick.location.x, y: location.y - previousClick.location.y)
        guard abs(movement.x) <= Constants.clickMovementThreshold && abs(movement.y) <= Constants.clickMovementThreshold else {
            return false
        }

        click = nil
        return true
    }
}

extension WKDOMDoubleClickGestureRecognizer {
    fileprivate enum Constants {
        static let clickDoubleTimeThreshold: Duration = .milliseconds(350)
        static let clickMovementThreshold: CGFloat = 45
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
