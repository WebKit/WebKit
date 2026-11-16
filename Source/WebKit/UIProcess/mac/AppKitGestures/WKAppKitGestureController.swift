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
private import CxxStdlib
private import WebCore_Private
import struct Swift.String

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

    func setUpDOMDoubleClickGestureRecognizer() {
        let recognizer = WKDOMDoubleClickGestureRecognizer(target: self, action: #selector(domDoubleClickGestureRecognized))
        configure(forDOMDoubleClick: recognizer)

        recognizer.delegate = self
        recognizer.name = "WKDOMDoubleClickGesture"

        self.domDoubleClickGestureRecognizer = recognizer
    }

    // `domDoubleClickGestureRecognizer` is always created as a `WKDOMDoubleClickGestureRecognizer`.
    @nonobjc
    private var typedDOMDoubleClickGestureRecognizer: WKDOMDoubleClickGestureRecognizer? {
        guard let recognizer = domDoubleClickGestureRecognizer else {
            return nil
        }

        guard let recognizer = recognizer as? WKDOMDoubleClickGestureRecognizer else {
            assertionFailure("DOM double click gesture recognizer was unexpectedly of type \(type(of: recognizer)).")
            return nil
        }

        return recognizer
    }

    func resetDOMDoubleClickGestureRecognizer() {
        typedDOMDoubleClickGestureRecognizer?.resetClick()
    }

    func takeCompletedDOMDoubleClick() -> Bool {
        typedDOMDoubleClickGestureRecognizer?.takeCompletedDoubleClick() ?? false
    }

    @objc(makeImageAnalysisDeferringGestureRecognizerWithName:)
    func makeImageAnalysisDeferringGestureRecognizer(withName name: String) -> WKDeferringGestureRecognizer {
        let deferringGestureRecognizer = WKDeferringGestureRecognizer(deferringGestureDelegate: self)
        configure(forImageAnalysisDeferral: deferringGestureRecognizer)
        deferringGestureRecognizer.name = name
        deferringGestureRecognizer.delegate = self
        deferringGestureRecognizer.immediatelyFailsAfterTouchEnd = true
        return deferringGestureRecognizer
    }

    @objc(panVelocityInView:)
    func panVelocity(in view: NSView?) -> NSPoint {
        guard let panGestureRecognizer = panGestureRecognizer as? WKPanGestureRecognizer else {
            return .zero
        }
        return panGestureRecognizer.wk_velocity(in: view)
    }

    @objc(loggingDescriptionForGestureRecognizer:)
    class func loggingDescription(for gestureRecognizer: NSGestureRecognizer?) -> String {
        guard let gestureRecognizer else {
            return "nil"
        }

        var parts: [String] = []
        parts.append("\(type(of: gestureRecognizer)): \(UInt(bitPattern: ObjectIdentifier(gestureRecognizer)))")

        if let name = gestureRecognizer.name {
            parts.append("name = \(name)")
        }

        parts.append("state = \(gestureRecognizer.state.name)")

        return "<\(parts.joined(separator: "; "))>"
    }
}

extension NSGestureRecognizer.State {
    fileprivate var name: String {
        switch self {
        case .began: "Began"
        case .possible: "Possible"
        case .changed: "Changed"
        case .ended: "Ended"
        case .cancelled: "Cancelled"
        case .failed: "Failed"
        @unknown default:
            fatalError()
        }
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
