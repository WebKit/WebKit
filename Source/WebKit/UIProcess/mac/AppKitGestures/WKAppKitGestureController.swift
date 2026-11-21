// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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
import os
private import CxxStdlib
private import WebCore_Private

#if !ENABLE_MAC_GESTURE_EVENTS || !ENABLE_IMAGE_ANALYSIS || !ENABLE_TWO_PHASE_CLICKS
#error("WKAppKitGestureController assumes MAC_GESTURE_EVENTS, IMAGE_ANALYSIS and TWO_PHASE_CLICKS")
#endif

@objc
@implementation
extension WKAppKitGestureController {
    @nonobjc
    private(set) final weak var view: WKWebView?

    var webView: WKWebView? { view }
    @nonobjc
    final let support = WKAppKitGestureControllerCxxSupport()
    var cxxSupport: WKAppKitGestureControllerCxxSupport { support }
    @nonobjc
    final let positionInformation: PositionInformationManager

    @nonobjc
    private(set) final var panGestureRecognizer: WKPanGestureRecognizer?
    @nonobjc
    private(set) final var singleTouchMouseTrackingGestureRecognizer: WKMouseTrackingGestureRecognizer?
    @nonobjc
    private(set) final var multiTouchMouseTrackingGestureRecognizer: WKMouseTrackingGestureRecognizer?

    @nonobjc
    private(set) final var singleClickGestureRecognizer: WKPressGestureRecognizer?
    @nonobjc
    private(set) final var doubleClickGestureRecognizer: NSClickGestureRecognizer?
    @nonobjc
    private(set) final var domDoubleClickGestureRecognizer: WKDOMDoubleClickGestureRecognizer?

    // Auxiliary gesture recognizers to support context menus.
    @nonobjc
    private(set) final var secondaryClickGestureRecognizer: WKPressGestureRecognizer?
    @nonobjc
    private(set) final var secondaryClickDeferringGestureRecognizer: WKDeferringGestureRecognizer?

    // Auxiliary gesture recognizers to support drag-and-drop.
    @nonobjc
    final var textSelectionDrag: TextSelectionDrag?
    @nonobjc
    private(set) final var dragDeferringGestureRecognizer: WKDeferringGestureRecognizer?

    @nonobjc
    private(set) final var enclosingScrollViewPanDeferringGestureRecognizer: WKDeferringGestureRecognizer?

    @nonobjc
    final var scrollState = ScrollGestureState()
    @nonobjc
    final var lastPanWheelEventWasHandledByViewGestureController = false
    @nonobjc
    final var hasEverMagnifiedDuringCurrentGesture = false

    @nonobjc
    final var isSuppressingSingleClickGestureForTextSelection = false

    @nonobjc
    final var layerTreeTransactionIDAtLastInteractionStart: WebKit.TransactionID?
    @nonobjc
    final var latestTapID: UInt64?
    @nonobjc
    private(set) final var clickHighlight: WKClickHighlightController?

    // FIXME: Avoid bloat in WKAppKitGestureController by introducing a separate
    // gesture delegate that exclusively manages the mouse tracking recognizers.
    //
    // See mouseTrackingGestureRecognized(_:) where this state is used to transition
    // between tracking gestures when an active gesture's tracking conditions change.
    @nonobjc
    final weak var activeMouseTrackingGestureRecognizer: WKMouseTrackingGestureRecognizer?
    @nonobjc
    final var mouseTrackingDelivery = MouseTrackingDelivery.none
    @nonobjc
    final var mayHaveHoverActivatedByClick = false
    @nonobjc
    final var mouseTrackingStartLocationInWindow = WebCore.FloatPoint()
    @nonobjc
    final var mouseTrackingIsSuppressedForTransformGesture = false

    @nonobjc
    private(set) final var dragPressGestureRecognizer: WKPressGestureRecognizer?

    @nonobjc
    private(set) final var pointerTrackingGestureRecognizer: WKPointerTrackingGestureRecognizer?
    @nonobjc
    private(set) final var pointerDownDeferringGestureRecognizer: WKDeferringGestureRecognizer?
    @nonobjc
    final var trackedPointer: TrackedPointer?

    @nonobjc
    final var gestureDraggingSession: NSDraggingSession?
    @nonobjc
    final var dragGestureHasSentMouseDown = false
    @nonobjc
    final var dragGestureDidReceiveDragStart = false
    @nonobjc
    final var dragIdentifier: UInt64 = 0

    @nonobjc
    final var potentialTapIsInProgress = false

    @nonobjc
    private(set) final var imageAnalysisGestureRecognizer: WKPressGestureRecognizer?
    @nonobjc
    private(set) final var imageAnalysisTextSelectionDeferringGestureRecognizer: WKDeferringGestureRecognizer?
    @nonobjc
    private(set) final var imageAnalysisDragAndContextMenuDeferringGestureRecognizer: WKDeferringGestureRecognizer?

    @nonobjc
    final var lastCompletedImageAnalysis: CompletedImageAnalysis?
    @nonobjc
    final var imageAnalysisDeferralState = ImageAnalysisDeferralState.undecided

    @nonobjc
    private(set) final var magnificationGestureRecognizer: NSMagnificationGestureRecognizer?
    @nonobjc
    private(set) final var rotationGestureRecognizer: NSRotationGestureRecognizer?

    @nonobjc
    final var transformGestureTracker = TransformGestureTracker()

    init(view: WKWebView) {
        self.view = view
        positionInformation = PositionInformationManager(view: view)
        super.init()
        support.controller = self
        support.setPositionInformationDidChangeHandler { [weak self] information in
            self?.positionInformation.didChange(information)
        }
        clickHighlight = WKClickHighlightController(delegate: self)
    }

    func setUp() {
        guard view != nil else {
            return
        }

        setUpGestureRecognizers()
        addGesturesToWebView()
        updateGestureEnablement()
    }

    func preferencesDidChange() {
        updateGestureEnablement()
        if let panGestureRecognizer {
            configure(forScrolling: panGestureRecognizer)
        }
    }

    func didCommitLoadForMainFrame() {
        positionInformation.invalidate()
        resetDOMDoubleClickGestureRecognizer()
        layerTreeTransactionIDAtLastInteractionStart = nil
        lastCompletedImageAnalysis = nil
        trackedPointer = nil
    }

    func reset() {
        clearGestureDragState()

        handleTapCancelled()
        isSuppressingSingleClickGestureForTextSelection = false
        latestTapID = nil
        layerTreeTransactionIDAtLastInteractionStart = nil
        mayHaveHoverActivatedByClick = false

        cancelPointerTrackingIfNeeded()
        activeMouseTrackingGestureRecognizer = nil
        mouseTrackingDelivery = .none
        mouseTrackingIsSuppressedForTransformGesture = false
        transformGestureTracker.reset()

        support.resetMomentum()
        scrollState.reset()

        resetDOMDoubleClickGestureRecognizer()
        lastCompletedImageAnalysis = nil
        positionInformation.reset()
    }
}

extension WKAppKitGestureController {
    // MARK: Gesture Recognizers

    private func setUpGestureRecognizers() {
        guard let webView = view else {
            return
        }

        panGestureRecognizer = makePanGestureRecognizer(for: webView)
        pointerTrackingGestureRecognizer = makePointerTrackingGestureRecognizer()
        pointerDownDeferringGestureRecognizer = makeDeferringGestureRecognizer(name: "WKPointerDownDeferringGesture")
        singleTouchMouseTrackingGestureRecognizer = makeMouseTrackingGestureRecognizer(
            numberOfTouchesRequired: 1,
            name: "WKMouseTrackingGestureSingle"
        )
        multiTouchMouseTrackingGestureRecognizer = makeMouseTrackingGestureRecognizer(
            numberOfTouchesRequired: 2,
            name: "WKMouseTrackingGestureMultiple"
        )
        singleClickGestureRecognizer = makeSingleClickGestureRecognizer()
        doubleClickGestureRecognizer = makeDoubleClickGestureRecognizer()
        domDoubleClickGestureRecognizer = makeDOMDoubleClickGestureRecognizer()

        secondaryClickGestureRecognizer = makeSecondaryClickGestureRecognizer()
        secondaryClickDeferringGestureRecognizer = makeDeferringGestureRecognizer(name: "WKSecondaryClickDeferringGesture")

        dragPressGestureRecognizer = makeDragPressGestureRecognizer()
        dragDeferringGestureRecognizer = makeDeferringGestureRecognizer(name: "WKDragDeferringGesture")

        enclosingScrollViewPanDeferringGestureRecognizer = makeDeferringGestureRecognizer(name: "WKEnclosingScrollViewPanDeferringGesture")

        imageAnalysisGestureRecognizer = makeImageAnalysisGestureRecognizer()
        (imageAnalysisTextSelectionDeferringGestureRecognizer, imageAnalysisDragAndContextMenuDeferringGestureRecognizer) =
            makeImageAnalysisDeferringGestureRecognizers()

        magnificationGestureRecognizer = makeMagnificationGestureRecognizer()
        rotationGestureRecognizer = makeRotationGestureRecognizer()
    }

    func makeGestureRecognizer<Recognizer: NSGestureRecognizer>(
        _ recognizer: Recognizer,
        name: String,
        configure: (Recognizer) -> Void = { _ in }
    ) -> Recognizer {
        recognizer.allowedTouchTypes = .direct
        configure(recognizer)
        recognizer.delegate = self
        recognizer.name = name

        // The set of gestures managed by WKAppKitGestureController are configured
        // and installed freshly for every web view initialization, and as such we
        // want to avoid duplicate sets when views are decoded.
        recognizer.shouldBeArchived = false

        return recognizer
    }

    func makeDeferringGestureRecognizer(name: String) -> WKDeferringGestureRecognizer {
        makeGestureRecognizer(WKDeferringGestureRecognizer(deferringGestureDelegate: self), name: name)
    }

    private var installedGestureRecognizers: [NSGestureRecognizer] {
        let recognizers: [NSGestureRecognizer?] = [
            panGestureRecognizer,
            pointerTrackingGestureRecognizer,
            pointerDownDeferringGestureRecognizer,
            singleTouchMouseTrackingGestureRecognizer,
            multiTouchMouseTrackingGestureRecognizer,
            singleClickGestureRecognizer,
            doubleClickGestureRecognizer,
            domDoubleClickGestureRecognizer,
            secondaryClickGestureRecognizer,
            secondaryClickDeferringGestureRecognizer,
            dragPressGestureRecognizer,
            dragDeferringGestureRecognizer,
            enclosingScrollViewPanDeferringGestureRecognizer,
            imageAnalysisGestureRecognizer,
            imageAnalysisTextSelectionDeferringGestureRecognizer,
            imageAnalysisDragAndContextMenuDeferringGestureRecognizer,
            magnificationGestureRecognizer,
            rotationGestureRecognizer,
        ]
        return recognizers.compactMap { $0 }
    }

    private func addGesturesToWebView() {
        guard let webView = view else {
            return
        }

        for recognizer in installedGestureRecognizers {
            webView.addGestureRecognizer(recognizer)
        }
    }

    private func updateGestureEnablement() {
        // The deferring gesture recognizers are intentionally not enabled.
        for recognizer in installedGestureRecognizers where !(recognizer is WKDeferringGestureRecognizer) {
            updateIsEnabled(recognizer)
        }
    }

    private func updateIsEnabled(_ gesture: NSGestureRecognizer) {
        guard view != nil else {
            return
        }

        let gestureEnabled =
            gesture !== imageAnalysisGestureRecognizer || WebKit.isLiveTextAvailableAndEnabled()

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            \(gesture.loggingDescription) setEnabled:\(gestureEnabled)
            """
        )
        gesture.isEnabled = gestureEnabled
    }

    // MARK: Logging

    var pageLogIdentifier: UInt64 {
        view?._protectedPage().get()?.logIdentifier() ?? 0
    }

    func logGestureAction(_ gesture: NSGestureRecognizer, function: String = #function) {
        Logger.viewGestures.debug("[pageProxyID=\(self.pageLogIdentifier)] \(function): \(gesture.loggingDescription)")
    }

    func logArbitration(
        _ gestureRecognizer: NSGestureRecognizer,
        _ otherGestureRecognizer: NSGestureRecognizer,
        function: String = #function
    ) {
        Logger.viewGestures.debug(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(function): Gesture: \(gestureRecognizer.loggingDescription), \
            Other gesture: \(otherGestureRecognizer.loggingDescription)
            """
        )
    }

    // MARK: Synthetic Mouse Events

    func syntheticMouseEvent(
        _ type: NSEvent.EventType,
        at location: NSPoint,
        modifierFlags: NSEvent.ModifierFlags,
        windowNumber: Int,
        pressure: Float,
        timestamp: TimeInterval = GetCurrentEventTime(),
        clickCount: Int = 1,
        reportingMovementFor recognizer: WKPressGestureRecognizer? = nil
    ) -> NSEvent? {
        let event = NSEvent.mouseEvent(
            with: type,
            location: location,
            modifierFlags: modifierFlags,
            timestamp: timestamp,
            windowNumber: windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: clickCount,
            pressure: pressure
        )
        guard let recognizer else {
            return event
        }
        return event.map { recognizer.eventReportingMovement($0, atWindowLocation: location) }
    }

    func sendSyntheticMouseEvent(
        _ type: NSEvent.EventType,
        at location: NSPoint,
        to impl: WebKit.WebViewImpl,
        modifierFlags: NSEvent.ModifierFlags,
        pressure: Float,
        canInitiateDrag: WebCore.PlatformMouseEvent.CanInitiateDrag,
        timestamp: TimeInterval = GetCurrentEventTime(),
        reportingMovementFor recognizer: WKPressGestureRecognizer? = nil
    ) {
        let event = syntheticMouseEvent(
            type,
            at: location,
            modifierFlags: modifierFlags,
            windowNumber: impl.windowNumber(),
            pressure: pressure,
            timestamp: timestamp,
            reportingMovementFor: recognizer
        )
        switch type {
        case .leftMouseDown, .rightMouseDown:
            impl.mouseDown(event, .Touch, canInitiateDrag)
        case .leftMouseDragged:
            impl.mouseDragged(event, .Touch, canInitiateDrag)
        case .leftMouseUp, .rightMouseUp:
            impl.mouseUp(event, .Touch, canInitiateDrag)
        default:
            assertionFailure("Not a mouse event a gesture sends")
        }
    }
}

extension NSGestureRecognizer {
    var loggingDescription: String {
        var parts: [String] = []
        parts.append("\(type(of: self)): \(UInt(bitPattern: ObjectIdentifier(self)))")

        if let name {
            parts.append("name = \(name)")
        }

        parts.append("state = \(state.name)")

        return "<\(parts.joined(separator: "; "))>"
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
