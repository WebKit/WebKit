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
import struct Foundation.URL
@_spi(WebKitAdditions_Testing) @_spi(Testing) import WebKit
import SwiftUI
import struct Swift.String
import struct _Concurrency.Task
import struct TestWebKitAPILibrary.DOMRect
import Testing
import TestWebKitAPILibrary
import Recap
private import AppKit_Private.NSMenu_Private

actor Recap {
    static let shared = Recap()

    func play(events: @Sendable (_ composer: any RCPEventStreamComposer) -> Void) async {
        let eventStream: RCPSyntheticEventStream = RCPSyntheticEventStream { composer in
            guard let composer else {
                preconditionFailure()
            }
            composer.senderProperties = ._wk_trackpadSender()
            events(composer)
        }

        await RCPInlinePlayer.play(eventStream, options: .init())
    }
}

@MainActor
protocol AppKitGestureTestSuite {
    static var text: String { get }

    static var topInset: CGFloat { get }

    var recap: Recap { get }

    var page: WebPage { get }

    var window: NSWindow { get }

    init() async throws
}

extension AppKitGestureTestSuite {
    static var topInset: CGFloat {
        0
    }
}

@Suite(.serialized, .timeLimit(.minutes(1)))
struct AppKitGesturesTests {
}

@MainActor
private func convertToCoreGraphicsScreenCoordinates(pointInWindowCoordinates: CGPoint, window: NSWindow) -> CGPoint {
    guard let screen = window.screen else {
        preconditionFailure()
    }

    let inAppKitScreenCoordinates = window.convertPoint(toScreen: pointInWindowCoordinates)

    let inCoreGraphicsScreenCoordinates = CGPoint(
        x: inAppKitScreenCoordinates.x,
        y: screen.frame.maxY - inAppKitScreenCoordinates.y
    )

    return inCoreGraphicsScreenCoordinates
}

@MainActor
private func convertToCoreGraphicsScreenCoordinates(rectInViewportCoordinates: DOMRect, window: NSWindow, topInset: CGFloat) -> CGRect {
    guard let contentViewController = window.contentViewController else {
        preconditionFailure()
    }

    guard let screen = window.screen else {
        preconditionFailure()
    }

    var inViewportCoordinates = CGRect(rectInViewportCoordinates)
    inViewportCoordinates.origin.y += topInset

    let inWindowCoordinates = CGRect(
        x: inViewportCoordinates.origin.x,
        y: contentViewController.view.frame.height - inViewportCoordinates.origin.y - inViewportCoordinates.size.height,
        width: inViewportCoordinates.size.width,
        height: inViewportCoordinates.size.height,
    )

    let inAppKitScreenCoordinates = window.convertToScreen(inWindowCoordinates)

    let inCoreGraphicsScreenCoordinates = CGRect(
        x: inAppKitScreenCoordinates.origin.x,
        y: screen.frame.maxY - inAppKitScreenCoordinates.maxY,
        width: inAppKitScreenCoordinates.width,
        height: inAppKitScreenCoordinates.height,
    )

    return inCoreGraphicsScreenCoordinates
}

extension AppKitGestureTestSuite {
    func screenBoundsOfText(_ text: String) async throws -> CGRect {
        let range = try #require(Self.text.utf16Range(of: text))

        let viewportCoordinates = try await page.callJavaScript(
            JavaScriptMessages.BoundingClientRect(in: "div", range: range)
        )

        let screenCoordinates = screenBounds(ofRectInViewportCoordinates: viewportCoordinates)
        return screenCoordinates
    }

    func screenBounds(ofElementWithID id: String) async throws -> CGRect {
        let viewportCoordinates = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: id))
        return convertToCoreGraphicsScreenCoordinates(
            rectInViewportCoordinates: viewportCoordinates,
            window: window,
            topInset: Self.topInset
        )
    }

    func screenBounds(ofPointInWindowCoordinates point: NSPoint) -> NSPoint {
        convertToCoreGraphicsScreenCoordinates(pointInWindowCoordinates: point, window: window)
    }

    func screenBounds(ofRectInViewportCoordinates point: DOMRect) -> CGRect {
        convertToCoreGraphicsScreenCoordinates(rectInViewportCoordinates: point, window: window, topInset: Self.topInset)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
