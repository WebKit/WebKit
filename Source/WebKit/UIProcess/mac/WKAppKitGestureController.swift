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

import Foundation
import WebKit_Internal
import AppKit
import AppKit_Private.NSPanGestureRecognizer_Private
private import CxxStdlib
private import WebCore_Private

final class WKPanGestureRecognizer: NSPanGestureRecognizer {
    private weak var webView: WKWebView?

    init(webView: WKWebView, target: Any?, action: Selector?) {
        self.webView = webView
        super.init(target: target, action: action)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @objc
    public required dynamic init?(coder: NSCoder) {
        super.init(coder: coder)
    }

    #if canImport(AppKit, _version: "2759")
    // swift-format-ignore: NoLeadingUnderscores
    override func _shouldRecognize(forDelta delta: NSPoint) -> NSPanShouldRecognizeResponse {
        guard let webView, let page = webView._protectedPage().get() else {
            return .fail
        }

        guard webView.enclosingScrollView != nil else {
            return .recognize
        }

        let locationInView = location(in: webView)
        let pinnedState = page.pinnedStateIncludingAncestorsAtPoint(.init(locationInView))

        // Safety: Accessing these functions on `pinnedState` is safe because a copy is immediately made,
        // and the functions's return value's lifetime is bound to `pinnedState` anyways.
        // FIXME: (rdar://145054011) Remove `unsafe` when possible.

        if abs(delta.x) > abs(delta.y) {
            if unsafe delta.x < 0 && pinnedState.__rightUnsafe().pointee {
                return .fail
            }
            if unsafe delta.x > 0 && pinnedState.__leftUnsafe().pointee {
                return .fail
            }
        } else {
            if unsafe delta.y < 0 && pinnedState.__topUnsafe().pointee {
                return .fail
            }
            if unsafe delta.y > 0 && pinnedState.__bottomUnsafe().pointee {
                return .fail
            }
        }

        return .recognize
    }
    #endif // canImport(AppKit, _version: "2759")
}

@objc(Swift)
@implementation
extension WKAppKitGestureController {
    func setUpPanGestureRecognizer() {
        guard let webView else {
            return
        }

        let panGestureRecognizer = WKPanGestureRecognizer(webView: webView, target: self, action: #selector(panGestureRecognized))
        configure(forScrolling: panGestureRecognizer)

        panGestureRecognizer.delegate = self
        panGestureRecognizer.name = "WKPanGesture"

        self.panGestureRecognizer = panGestureRecognizer
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
