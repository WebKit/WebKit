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
import WebKit
import SwiftUI
import struct Swift.String
import struct _Concurrency.Task
import Testing
private import TestWebKitAPILibrary

@MainActor
struct AppKitGesturesTests {
    @Test
    func clickingChangesSelection() async throws {
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

        let crazyBoundsDictionary = try await #require(page.callJavaScript(getSelectionBounds) as? [String: Double])
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

        let waitForSelectionChange = """
            return await new Promise(resolve => {
                document.addEventListener("selectionchange", () => {
                    const offset = window.getSelection().focusOffset;
                    resolve(offset);
                });
            });
            """

        async let newSelection = page.callJavaScript(waitForSelectionChange) as? Int

        // Ensure the JS `selectionchange` event listener is installed before performing the click.
        await Task.yield()

        page.click(at: middleOfCrazy)

        let selection = try await newSelection
        let expected = "Here's to the cra".count
        #expect(selection == expected)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
