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

#if HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)

import Foundation
import WebKit
import SwiftUI

@objc
@implementation
extension AppKitGesturesSupport {
    class func testClickingChangesSelection() async throws {
        let page = WebPage()

        let html = """
            <div id="div" contenteditable style="font-size: 30px;">Here's to the crazy ones.</div>
            """

        try await page.load(html: html).wait()

        let contentSize = NSSize(width: 800, height: 600)

        let window = NSWindow(size: contentSize) {
            WebView(page)
        }

        window.setFrameOrigin(.zero)
        window.makeKeyAndOrderFront(nil)

        let selectCrazy = """
            const textNode = document.getElementById("div").firstChild

            const range = document.createRange();
            range.setStart(textNode, 14);
            range.setEnd(textNode, 19);

            let selection = window.getSelection();
            selection.removeAllRanges();
            selection.addRange(range);
            """

        try await page.callJavaScript(selectCrazy)

        let getSelectionBounds = """
            const selection = window.getSelection();

            const range = selection.getRangeAt(0);
            return range.getBoundingClientRect().toJSON();
            """

        let crazyBoundsDictionary = try await Testing.require(page.callJavaScript(getSelectionBounds) as? [String: Int])
        let crazyBoundsInViewportCoordinates = CGRect(
            x: crazyBoundsDictionary["x", default: 0],
            y: crazyBoundsDictionary["y", default: 0],
            width: crazyBoundsDictionary["width", default: 0],
            height: crazyBoundsDictionary["height", default: 0],
        )

        let crazyBoundsInAppKitCoordinates = CGRect(
            x: crazyBoundsInViewportCoordinates.minX,
            y: contentSize.height - crazyBoundsInViewportCoordinates.maxY,
            width: crazyBoundsInViewportCoordinates.width,
            height: crazyBoundsInViewportCoordinates.height,
        )

        let middleOfCrazy = CGPoint(x: crazyBoundsInAppKitCoordinates.midX, y: crazyBoundsInAppKitCoordinates.midY)

        let moveSelectionToStart = """
            const textNode = document.getElementById("div").firstChild

            const range = document.createRange();
            range.setStart(textNode, 0);
            range.setEnd(textNode, 0);

            let selection = window.getSelection();
            selection.removeAllRanges();
            selection.addRange(range);
            """

        try await page.callJavaScript(moveSelectionToStart)

        page.click(at: middleOfCrazy)

        try await Testing.waitUntil {
            let selection = try await page.callJavaScript("return window.getSelection().focusOffset") as? Int
            return selection != 0
        }

        let selection = try await page.callJavaScript("return window.getSelection().focusOffset") as? Int
        let expected = "Here's to the cra".count
        try Testing.expect(selection, toEqual: expected)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)
