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

#if ENABLE_DRAG_SUPPORT && USE_BROWSERENGINEKIT

private import BrowserEngineKit
public import CoreGraphics
public import UIKit
public import WebKit

import struct Swift.String

/// An error thrown when a drag lifts nothing.
public struct DragLiftError: Error, CustomStringConvertible {
    /// Why the drag lifted nothing.
    public let description: String
}

extension WKWebView {
    /// Starts a drag at a point, takes the preview UIKit lifts for its first item, and cancels the drag.
    ///
    /// - Parameter pointInWindow: Where to start the drag, in the coordinates of the web view's window.
    /// - Returns: The lift preview of the first item.
    /// - Throws: ``DragLiftError`` if the web view has no drag interaction, or nothing can be dragged at the point.
    public func liftPreviewOfDrag(startingAt pointInWindow: CGPoint) async throws -> UITargetedDragPreview {
        guard let drag = DragLift(webView: self, startingAt: pointInWindow) else {
            throw DragLiftError(description: "The web view has no drag interaction.")
        }
        guard let preview = await drag.lift() else {
            throw DragLiftError(description: "Nothing can be dragged at \(pointInWindow).")
        }
        drag.cancel()
        return preview
    }
}

@MainActor
private final class DragLift {
    private let webView: WKWebView
    private let delegate: any BEDragInteractionDelegate
    private let interaction: BEDragInteraction
    private let session: MockDragSession

    init?(webView: WKWebView, startingAt pointInWindow: CGPoint) {
        guard
            let contentView = webView.value(forKey: "_currentContentView") as? UIView,
            let delegate = contentView as? any BEDragInteractionDelegate,
            let interaction = contentView.interactions.first(where: { $0 is BEDragInteraction }) as? BEDragInteraction
        else {
            return nil
        }

        self.webView = webView
        self.delegate = delegate
        self.interaction = interaction
        session = MockDragSession(window: webView.window, locationInWindow: pointInWindow)
    }

    // WebKit cleans up the drag if the session has no items when the prepare completion handler returns, so the handler
    // lifts the item itself instead of leaving that to the code after the await.
    func lift() async -> UITargetedDragPreview? {
        webView.becomeFirstResponder()
        return await withCheckedContinuation { continuation in
            delegate.dragInteraction?(interaction, prepare: session) {
                MainActor.assumeIsolated {
                    let preview = self.liftFirstItem()
                    continuation.resume(returning: preview)
                    return preview != nil
                }
            }
        }
    }

    func cancel() {
        delegate.dragInteraction?(interaction, session: session, didEndWith: .cancel)
    }

    private func liftFirstItem() -> UITargetedDragPreview? {
        let items = delegate.dragInteraction(interaction, itemsForBeginning: session)
        session.items = items
        guard let item = items.first else {
            return nil
        }

        let preview = delegate.dragInteraction?(interaction, previewForLifting: item, session: session)
        delegate.dragInteraction?(interaction, sessionWillBegin: session)
        return preview
    }
}

@MainActor
private final class MockDragSession: NSObject, UIDragSession {
    private let window: UIWindow?
    private let locationInWindow: CGPoint

    var items: [UIDragItem] = []
    var localContext: Any?
    let allowsMoveOperation = true
    let isRestrictedToDraggingApplication = false

    init(window: UIWindow?, locationInWindow: CGPoint) {
        self.window = window
        self.locationInWindow = locationInWindow
        super.init()
    }

    func location(in view: UIView) -> CGPoint {
        window?.convert(locationInWindow, to: view) ?? locationInWindow
    }

    func hasItemsConforming(toTypeIdentifiers typeIdentifiers: [String]) -> Bool {
        items.contains { item in
            typeIdentifiers.contains { item.itemProvider.registeredTypeIdentifiers.contains($0) }
        }
    }

    func canLoadObjects(ofClass aClass: any NSItemProviderReading.Type) -> Bool {
        items.contains { $0.itemProvider.canLoadObject(ofClass: aClass) }
    }
}

#endif // ENABLE_DRAG_SUPPORT && USE_BROWSERENGINEKIT
