//
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
// THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if HAVE_NSREFRESHCONTROLLER

import Foundation
#if canImport(WebKit_Internal)
internal import WebKit_Internal
#endif

@objc(NSRefreshControlHosting)
@implementation
extension WKWebView {
    // Protocol conformance.
    func apply(withVerticalInset verticalInset: CGFloat, animated: Bool, completion: (@Sendable () -> Void)?) {
        defer {
            completion?()
        }

        guard let impl = _impl() else {
            return
        }

        impl.applyRefreshControllerHeight(verticalInset, animated)
    }

    // Protocol conformance.
    func remove(withVerticalInset verticalInset: CGFloat, animated: Bool, completion: (@Sendable () -> Void)?) {
        defer {
            completion?()
        }

        guard let impl = _impl() else {
            return
        }

        impl.applyRefreshControllerHeight(0, animated)
    }

    // Protocol conformance.
    @objc(refreshControlVisibleHeight)
    var refreshControlVisibleHeight: CGFloat {
        _refreshControlVisibleHeight
    }

    // Protocol conformance.
    var refreshControlHostBounds: NSRect {
        let insets = _obscuredContentInsets
        return NSRect(
            x: bounds.origin.x + insets.left,
            y: bounds.origin.y + insets.top,
            width: bounds.width - insets.left - insets.right,
            height: bounds.height - insets.top - insets.bottom
        )
    }
}

#endif // HAVE_NSREFRESHCONTROLLER
