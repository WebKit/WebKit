// Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI

import Foundation
@_spi(Testing) @_spi(CrossImportOverlay) public import WebKit
import WebKit_Private.WKPreferencesPrivate
import WebKit_Private.WKWebViewPrivateForTesting
import WebKit_Private.WKWebViewPrivate
public import WebKit_Private._WKFrameTreeNode
public import struct Swift.String
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView

#if WTF_PLATFORM_MAC
private import Carbon
#endif

extension WebPage {
    /// An edit command that can be used to interact with the web page.
    public enum EditCommand: String {
        case deleteBackward = "DeleteBackward"
        case toggleBold = "ToggleBold"
        case toggleItalic = "ToggleItalic"
        case toggleUnderline = "ToggleUnderline"
    }

    /// Suspends execution of the current context until the next presentation update has occurred for this page.
    public func waitForNextPresentationUpdate() async {
        await withCheckedContinuation { continuation in
            backingWebView._do(afterNextPresentationUpdate: {
                continuation.resume()
            })
        }
    }

    /// Configures a specific preference for the engine to use.
    ///
    /// - Parameters:
    ///   - key: The preference key value.
    ///   - enabled: If the preference should be enabled or disabled.
    public func setWebFeature(_ key: String, enabled: Bool) {
        guard let feature = WKPreferences._features().first(where: { $0.key == key }) else {
            fatalError()
        }

        backingWebView.configuration.preferences._setEnabled(enabled, for: feature)
    }

    /// Produces a String representing the  current visual state of the web content as a tree of rendering elements.
    ///
    /// - Returns: The representative render tree for the current web content.
    public func renderTree() async throws -> String {
        try await backingWebView._getRenderTreeAsString()
    }

    /// The hyperbolic elastic coefficient the main-frame scrolling node latched for its most recent
    /// gesture, which depends on the scroll source.
    ///
    /// - Returns: The latched rubberband hyperbolic coefficient.
    public func rubberbandHyperbolicCoefficient() -> Double {
        backingWebView._rubberbandHyperbolicCoefficientForTesting
    }

    /// Inserts text into the web page.
    ///
    /// - Parameter text: The text to insert.
    public func insertText(_ text: String) async {
        #if WTF_PLATFORM_MAC
        backingWebView.insertText(text)
        #else
        backingWebView.textInputContentView.insertText(text)
        #endif
        await waitForNextPresentationUpdate()
    }

    /// The main frame tree node of this page.
    public var mainFrame: _WKFrameTreeNode? {
        get async {
            await backingWebView._frames()
        }
    }

    /// Perform the specified edit command on the webpage, optionally with an argument provided to the command.
    ///
    /// - Parameters:
    ///   - command: The command to execute.
    ///   - argument: An optional argument to send to the command.
    public func executeEditCommand(_ command: EditCommand, with argument: String? = nil) async {
        let success = await backingWebView._executeEditCommand(command.rawValue, argument: argument)
        assert(success)
    }

    #if WTF_PLATFORM_MAC

    /// Determines if the specified point is located above a visible scrollbar.
    ///
    /// - Parameter locationInView: The point in view coordinates to test.
    /// - Returns: `true` if the point is over a scrollbar; `false` otherwise.
    public func isPointInScrollbar(locationInView: NSPoint) -> Bool {
        backingWebView.isPoint(inScrollbar: locationInView)
    }

    /// Sends a left mouse-down NSEvent to the web view at the given location.
    ///
    /// - Parameter location: The location in window coordinates.
    public func mouseDown(at location: NSPoint) {
        backingWebView.mouseDown(with: mouseEvent(.leftMouseDown, at: location, clickCount: 1, pressure: 1))
    }

    /// Sends a left mouse-up NSEvent to the web view at the given location.
    ///
    /// - Parameter location: The location in window coordinates.
    public func mouseUp(at location: NSPoint) {
        backingWebView.mouseUp(with: mouseEvent(.leftMouseUp, at: location, clickCount: 1, pressure: 0))
    }

    /// Sends a mouse-moved NSEvent to the web view at the given location.
    ///
    /// - Parameters:
    ///   - location: The location in window coordinates.
    ///   - flags: Modifier flags to include in the event.
    public func mouseMove(to location: NSPoint, flags: NSEvent.ModifierFlags = []) {
        backingWebView.mouseMoved(with: mouseEvent(.mouseMoved, at: location, flags: flags, clickCount: 0, pressure: 0))
    }

    /// Sends a left mouse-dragged NSEvent to the web view at the given location.
    ///
    /// - Parameter location: The location in window coordinates.
    public func mouseDrag(to location: NSPoint) {
        backingWebView.mouseDragged(with: mouseEvent(.leftMouseDragged, at: location, clickCount: 1, pressure: 1))
    }

    /// Performs a mouse click at the given location by sending a mouse down and mouse up NSEvent.
    ///
    /// - Parameter location: The location to click at, in window coordinates.
    public func click(at location: NSPoint) {
        mouseDown(at: location)
        mouseUp(at: location)
    }

    /// Performs `count` sequential mouse clicks at the given location, incrementing the clickCount on each
    /// event so that AppKit interprets them as a compound gesture (double-click, triple-click, etc.).
    ///
    /// - Parameters:
    ///   - location: The location to click at, in window coordinates.
    ///   - count: The number of clicks to send.
    public func clicks(at location: NSPoint, count: Int) {
        for clickCount in 1...count {
            backingWebView.mouseDown(with: mouseEvent(.leftMouseDown, at: location, clickCount: clickCount, pressure: 1))
            backingWebView.mouseUp(with: mouseEvent(.leftMouseUp, at: location, clickCount: clickCount, pressure: 0))
        }
    }

    /// Performs a right mouse click at the given location by sending a right mouse down and right mouse up NSEvent.
    ///
    /// - Parameter location: The location to click at, in window coordinates.
    public func rightClick(at location: NSPoint) {
        backingWebView.rightMouseDown(with: mouseEvent(.rightMouseDown, at: location, clickCount: 1, pressure: 1))
        backingWebView.rightMouseUp(with: mouseEvent(.rightMouseUp, at: location, clickCount: 1, pressure: 0))
    }

    /// Synthesizes a user-driven trackpad-driven scroll gesture and delivers it to the web view.
    ///
    /// - Parameters:
    ///   - location: The location in window coordinates to scroll at.
    ///   - delta: The scroll delta in pixels (positive `y` scrolls the content up).
    public func scrollWheel(at location: NSPoint, delta: CGSize) {
        let beganPhaseDelta = CGSize(
            width: Int(delta.width.rounded(.awayFromZero)).signum(),
            height: Int(delta.height.rounded(.awayFromZero)).signum()
        )

        let changedPhaseDelta = CGSize(
            width: delta.width - beganPhaseDelta.width,
            height: delta.height - beganPhaseDelta.height
        )
        backingWebView.scrollWheel(with: scrollWheelEvent(at: location, delta: beganPhaseDelta, phase: .began, momentumPhase: .none))
        backingWebView.scrollWheel(with: scrollWheelEvent(at: location, delta: changedPhaseDelta, phase: .changed, momentumPhase: .none))
        backingWebView.scrollWheel(with: scrollWheelEvent(at: location, delta: .zero, phase: .ended, momentumPhase: .none))
    }

    /// Suspends until WebKit has processed all pending mouse events delivered to this page.
    public func waitForPendingMouseEvents() async {
        await withCheckedContinuation { continuation in
            backingWebView._do(afterProcessingAllPendingMouseEvents: {
                continuation.resume()
            })
        }
    }

    /// Copies the current selection to the system pasteboard and returns its string representation.
    public func copySelection() async -> String? {
        NSPasteboard.general.clearContents()
        NSApp.sendAction(#selector(NSText.copy(_:)), to: backingWebView, from: nil)
        await waitForNextPresentationUpdate()
        return NSPasteboard.general.string(forType: .string)
    }

    private func mouseEvent(
        _ type: NSEvent.EventType,
        at location: NSPoint,
        flags: NSEvent.ModifierFlags = [],
        clickCount: Int,
        pressure: Float
    ) -> NSEvent {
        guard let window = unsafe backingWebView.window else {
            preconditionFailure("Could not create NSEvent because there is no NSWindow.")
        }

        guard
            let event = NSEvent.mouseEvent(
                with: type,
                location: location,
                modifierFlags: flags,
                timestamp: GetCurrentEventTime(),
                windowNumber: window.windowNumber,
                context: nil,
                eventNumber: 0,
                clickCount: clickCount,
                pressure: pressure
            )
        else {
            preconditionFailure("Could not create NSEvent.")
        }

        return event
    }

    private func scrollWheelEvent(
        at location: NSPoint,
        delta: CGSize,
        phase: CGScrollPhase,
        momentumPhase: CGMomentumScrollPhase
    ) -> NSEvent {
        guard let window = unsafe backingWebView.window else {
            preconditionFailure("Could not create scroll NSEvent because there is no NSWindow.")
        }

        guard
            let cgEvent = CGEvent(
                scrollWheelEvent2Source: nil,
                units: .pixel,
                wheelCount: 2,
                wheel1: Int32(delta.height),
                wheel2: Int32(delta.width),
                wheel3: 0
            )
        else {
            preconditionFailure("Could not create CGEvent for scroll.")
        }

        cgEvent.setIntegerValueField(.scrollWheelEventScrollPhase, value: Int64(phase.rawValue))
        cgEvent.setIntegerValueField(.scrollWheelEventMomentumPhase, value: Int64(momentumPhase.rawValue))

        // CGEvent locations are in flipped, global screen coordinates.
        let locationInScreen = window.convertPoint(toScreen: location)
        let primaryScreenHeight = NSScreen.screens.first?.frame.height ?? 0
        cgEvent.location = CGPoint(x: locationInScreen.x, y: primaryScreenHeight - locationInScreen.y)

        guard let event = NSEvent(cgEvent: cgEvent) else {
            preconditionFailure("Could not create scroll NSEvent.")
        }

        return event
    }
    #endif // WTF_PLATFORM_MAC
}

#endif // ENABLE_SWIFTUI
