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
private import AppKit_Private.NSPressGestureRecognizer_Private

@objc
@implementation
extension WKMouseTrackingGestureRecognizer {
    @nonobjc
    private var mouseLocationOffsetInWindow: CGSize = .zero

    final override func reset() {
        mouseLocationOffsetInWindow = .zero
        super.reset()
    }

    func beginTrackingMouse() {
        mouseLocationOffsetInWindow = .zero
    }

    func beginTrackingMouseInherited(fromWindowLocation windowLocation: CGPoint) {
        let start = startLocationInWindow
        mouseLocationOffsetInWindow = CGSize(
            width: start.x - windowLocation.x,
            height: start.y - windowLocation.y
        )
    }

    var startLocationInWindow: CGPoint {
        _startLocation(in: nil)
    }

    var mouseLocationInWindow: CGPoint {
        let location = self.location(in: nil)
        return CGPoint(
            x: location.x - mouseLocationOffsetInWindow.width,
            y: location.y - mouseLocationOffsetInWindow.height
        )
    }

    var movementInWindowSinceStart: CGSize {
        let location = self.location(in: nil)
        let start = startLocationInWindow
        return CGSize(width: location.x - start.x, height: location.y - start.y)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
