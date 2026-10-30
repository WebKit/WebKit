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

#if ENABLE_DRAG_SUPPORT && WTF_PLATFORM_MAC

import AppKit
public import CoreGraphics
private import TestWebKitAPILibrary.Helpers.mac.TestDraggingInfo
public import WebKit

import struct Swift.String

/// A drag from outside a web view that a test moves over it, where each move returns before web content handles it.
@MainActor
public struct ExternalDrag {
    private let webView: WKWebView
    private let draggingInfo: TestDraggingInfo

    /// Creates a drag of plain text over a web view.
    public init(over webView: WKWebView, string: String) {
        let pasteboard = NSPasteboard.withUniqueName()
        pasteboard.clearContents()
        pasteboard.setString(string, forType: .string)

        self.webView = webView
        draggingInfo = TestDraggingInfo(pasteboard: pasteboard)
    }

    /// Moves the drag into the web view at a location in its coordinates.
    public func enter(at location: CGPoint) {
        draggingInfo.draggingLocation = webView.convert(location, to: nil)
        webView.draggingEntered(draggingInfo)
    }

    /// Moves the drag to another location in the web view's coordinates.
    public func move(to location: CGPoint) {
        draggingInfo.draggingLocation = webView.convert(location, to: nil)
        webView.draggingUpdated(draggingInfo)
    }

    /// Moves the drag out of the web view.
    public func exit() {
        webView.draggingExited(draggingInfo)
    }
}

#endif // ENABLE_DRAG_SUPPORT && WTF_PLATFORM_MAC
