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
private import AppKit_Private.NSClickGestureRecognizer_Private
private import AppKit_Private.NSPressGestureRecognizer_Private
private import WebCore_Private

extension WKAppKitGestureController {
    // MARK: Gesture Recognizers

    func makeSingleClickGestureRecognizer() -> WKPressGestureRecognizer {
        makeGestureRecognizer(
            WKPressGestureRecognizer(target: self, action: #selector(singleClickGestureRecognized)),
            name: "WKSingleClickGesture"
        ) {
            $0.minimumPressDuration = 0
            $0.cancelPastAllowableMovement = true
            $0.buttonMask = 0
            $0.numberOfTouchesRequired = 1
        }
    }

    func makeDoubleClickGestureRecognizer() -> NSClickGestureRecognizer {
        makeGestureRecognizer(
            NSClickGestureRecognizer(target: self, action: #selector(doubleClickGestureRecognized)),
            name: "WKDoubleClickGesture"
        ) {
            $0.buttonMask = 0
            $0.numberOfTouchesRequired = 2
            $0.numberOfClicksRequired = 2
        }
    }

    func makeDOMDoubleClickGestureRecognizer() -> WKDOMDoubleClickGestureRecognizer {
        makeGestureRecognizer(
            WKDOMDoubleClickGestureRecognizer(target: self, action: #selector(domDoubleClickGestureRecognized)),
            name: "WKDOMDoubleClickGesture"
        ) {
            $0.buttonMask = 0
            $0.numberOfTouchesRequired = 1
            // Intentionally set to `1` and not `2`, see WKDOMDoubleClickGestureRecognizer.swift.
            // FIXME: rdar://184563102 (An NSClickGestureRecognizer with numberOfClicksRequired=2 results in other GRs being neglected for some gestures)
            $0.numberOfClicksRequired = 1
        }
    }

    func resetDOMDoubleClickGestureRecognizer() {
        domDoubleClickGestureRecognizer?.resetClick()
    }

    private func takeCompletedDOMDoubleClick() -> Bool {
        domDoubleClickGestureRecognizer?.takeCompletedDoubleClick() ?? false
    }

    func makeSecondaryClickGestureRecognizer() -> WKPressGestureRecognizer {
        makeGestureRecognizer(
            WKPressGestureRecognizer(target: self, action: #selector(secondaryClickGestureRecognized)),
            name: "WKSecondaryClickGesture"
        ) {
            $0.buttonMask = 0
            $0.cancelPastAllowableMovement = true
        }
    }

    // MARK: Click Gestures

    @objc
    private func singleClickGestureRecognized(_ gesture: NSGestureRecognizer) {
        guard let webView = view else {
            return
        }

        logGestureAction(gesture)

        precondition(singleClickGestureRecognizer === gesture)

        guard let impl = webView._impl(), !impl.ignoresAllEvents() else {
            return
        }

        if scrollState.caughtDeceleratingScroll {
            // This gesture is interrupting a decelerating scroll; it should stop the scroll (and may
            // turn into a pan) but must never perform a click.
            if gesture.state.isTerminal {
                scrollState.resetCaughtDeceleratingScroll()
                handleTapCancelled()
            }
            return
        }

        if forwardClickToPDFHUDIfNeeded(gesture, in: webView, impl: impl) {
            return
        }

        switch gesture.state {
        case .began:
            handleTapBegan(gesture)
        case .ended:
            handleTapEnded(gesture)
        case .cancelled, .failed:
            handleTapCancelled()
        default:
            break
        }
    }

    // Clicks aren't delivered to NSButton's built-in click gesture
    // recognizer when a parent view's GR recognizes first, so we
    // forward the click manually.
    private func forwardClickToPDFHUDIfNeeded(_ gesture: NSGestureRecognizer, in webView: WKWebView, impl: WebKit.WebViewImpl) -> Bool {
        guard let page = webView._protectedPage().get(), !page.preferences().useAlternatePDFHUD(), gesture.state == .began,
            let hitView = impl.hitTestPDFHUD(WebCore.FloatPoint(gesture.location(in: webView))).get(), impl.isViewVisible(hitView)
        else {
            return false
        }

        (hitView as? NSButton)?.performClick(nil)
        gesture.state = .cancelled
        return true
    }

    @objc
    private func doubleClickGestureRecognized(_ gesture: NSGestureRecognizer) {
        guard let webView = view else {
            return
        }

        logGestureAction(gesture)

        precondition(doubleClickGestureRecognizer === gesture)

        guard let impl = webView._impl() else {
            return
        }
        impl.dismissContentRelativeChildWindowsWithAnimation(false)

        let magnificationOrigin = webView.convert(gesture.location(in: nil), from: nil)
        impl.ensureGestureController().handleSmartMagnificationGesture(WebCore.FloatPoint(magnificationOrigin))
    }

    @objc
    private func domDoubleClickGestureRecognized(_ gesture: NSGestureRecognizer) {
        guard let webView = view else {
            return
        }

        logGestureAction(gesture)

        precondition(domDoubleClickGestureRecognizer === gesture)

        // The single-click gesture may have already committed this click as a double click.
        guard takeCompletedDOMDoubleClick() else {
            return
        }

        guard let impl = webView._impl(), !impl.ignoresAllEvents() else {
            return
        }

        if layerTreeTransactionIDAtLastInteractionStart == nil {
            updateLayerTreeTransactionIDAtLastInteractionStart()
        }

        guard let layerTreeTransactionIDAtLastInteractionStart else {
            return
        }

        let location = gesture.location(in: webView)
        let modifiers = WebKit.WebEventFactory.toWebEventModifierFlags(gesture.modifierFlags)

        webView._protectedPage().get()?
            .handleDoubleTapForDoubleClickAtPoint(
                WebCore.roundedIntPoint(location),
                modifiers,
                layerTreeTransactionIDAtLastInteractionStart,
                .Touch,
                .OneFingerTap
            )
    }

    @objc
    private func secondaryClickGestureRecognized(_ gesture: NSGestureRecognizer) {
        guard let webView = view, let impl = webView._impl() else {
            return
        }

        logGestureAction(gesture)

        precondition(secondaryClickGestureRecognizer === gesture)

        guard gesture.state == .began else {
            return
        }

        handleTapCancelled()
        cancelPointerTrackingIfNeeded()

        let modifierFlags = gesture.modifierFlags
        let location = gesture.location(in: nil)
        sendSyntheticMouseEvent(.rightMouseDown, at: location, to: impl, modifierFlags: modifierFlags, pressure: 1, canInitiateDrag: .Yes)
        sendSyntheticMouseEvent(.rightMouseUp, at: location, to: impl, modifierFlags: modifierFlags, pressure: 0, canInitiateDrag: .Yes)
    }

    func secondaryClickShouldBegin(at locationInView: NSPoint) -> Bool {
        guard view != nil else {
            return false
        }

        let radius = positionInformationToleranceRadius(for: secondaryClickGestureRecognizer)

        let information = positionInformation.currentInformation

        let requestIsValid = isPositionInformationValid(at: locationInView, radius: radius)
        let isSecondaryClickable = information.representsSecondaryClickableElement
        let isOverSelectableText = information.isOverSelectableText

        // The secondary click owns points that are not over selectable text (e.g. the page background
        // or a video player). Over a run of selectable text, the text selection manager should win so
        // that a long press selects a word instead of synthesizing a context menu.
        let shouldBegin = requestIsValid && isSecondaryClickable && !isOverSelectableText

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Secondary click shouldBegin → \(shouldBegin) \
            (hasInfo=\(self.positionInformation.hasValidCurrentInformation) \
            requestIsValid=\(requestIsValid) \
            secondaryClickable=\(isSecondaryClickable) \
            overSelectableText=\(isOverSelectableText) selectability=\(information.selectability.rawValue) \
            prefersManipulation=\(information.prefersDirectManipulation) \
            image=\(information.isImage) video=\(information.isOverVideo) \
            longClickQuirk=\(information.shouldTreatLongClickAsSecondaryClickQuirk) radius=\(radius))
            """
        )

        if !requestIsValid {
            positionInformation.invalidate()
        }

        return shouldBegin
    }

    // MARK: Click Handling

    func updateLayerTreeTransactionIDAtLastInteractionStart() {
        if let transactionID = support.lastCommittedMainFrameLayerTreeTransactionID.value {
            layerTreeTransactionIDAtLastInteractionStart = transactionID
        }
    }

    func handleTapBegan(_ gesture: NSGestureRecognizer) {
        guard let webView = view else {
            return
        }

        // Equivalent to acceptsFirstMouse, but here because potential taps are asynchronous, which
        // means that the window can get focus from this tap before we complete the click.
        let commandKeyIsDown = gesture.modifierFlags.contains(.command)
        if !(webView._impl()?.window()?.isKeyWindow ?? false) && !commandKeyIsDown {
            Logger.viewGestures.log(
                "[pageProxyID=\(self.pageLogIdentifier)] \(#function): Tap began in an inactive window; suppressing"
            )
            return
        }

        guard !isSuppressingSingleClickGestureForTextSelection else {
            return
        }

        let position = WebCore.FloatPoint(gesture.location(in: webView))

        updateLayerTreeTransactionIDAtLastInteractionStart()

        let tapID = WebKit.TapIdentifier.generate()
        latestTapID = tapID.toUInt64()
        potentialTapIsInProgress = true

        clickHighlight?.beginRequest(tapID, expectsFastCommit: true)

        let shouldRequestMagnificationInformation = false
        webView._protectedPage().get()?.potentialTapAtPosition(nil, position, shouldRequestMagnificationInformation, tapID, .Touch)
    }

    func handleTapEnded(_ gesture: NSGestureRecognizer) {
        guard let webView = view, potentialTapIsInProgress else {
            return
        }

        endPotentialTapAndEnableDoubleTapGesturesIfNecessary()

        guard let layerTreeTransactionIDAtLastInteractionStart else {
            handleTapCancelled()
            return
        }

        // If this tap completes a double tap, the web process delivers it as a double click when there is a listener
        // for one, instead of the DOM double-click gesture also sending it.
        let completesDoubleClick: WebKit.CompletesDoubleClick = takeCompletedDOMDoubleClick() ? .Yes : .No

        clickHighlight?.showPendingHighlightForCommittedClick()

        let modifiers = WebKit.WebEventFactory.toWebEventModifierFlags(gesture.modifierFlags)
        webView._protectedPage().get()?
            .commitPotentialTap(
                nil,
                modifiers,
                layerTreeTransactionIDAtLastInteractionStart,
                WebCore.mousePointerID,
                completesDoubleClick
            )

        clickHighlight?.fadeUnlessAwaitingHighlight()
    }

    func handleTapCancelled() {
        clickHighlight?.cancel()

        guard potentialTapIsInProgress else {
            return
        }

        potentialTapIsInProgress = false

        view?._protectedPage().get()?.cancelPotentialTap()
    }

    private func endPotentialTapAndEnableDoubleTapGesturesIfNecessary() {
        potentialTapIsInProgress = false
        setDoubleTapGesturesEnabled(true)
    }

    func setDoubleTapGesturesEnabled(_ enabled: Bool) {
        doubleClickGestureRecognizer?.isEnabled = enabled
    }

    fileprivate var unobscuredRect: CGRect {
        guard let webView = view else {
            return .zero
        }

        var unobscuredRect = WebCore.FloatRect(webView.bounds)
        if let page = webView._protectedPage().get() {
            unobscuredRect.contract(page.obscuredContentInsets)
        }
        return CGRect(unobscuredRect)
    }
}

@objc(Clicks)
@implementation
extension WKAppKitGestureController {
    func beginSuppressingSingleClickGestureForTextSelection() {
        guard let webView = view else {
            return
        }

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Begin suppressing single-click gesture for text selection
            """
        )

        isSuppressingSingleClickGestureForTextSelection = true

        handleTapCancelled()
        webView._protectedPage().get()?.cancelPotentialTap()
    }

    func endSuppressingSingleClickGestureForTextSelection() {
        guard view != nil else {
            return
        }

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            End suppressing single-click gesture for text selection
            """
        )

        isSuppressingSingleClickGestureForTextSelection = false
    }

    func cancelClickHighlight() {
        clickHighlight?.cancel()
    }

    // MARK: Two-Phase Click Response Handlers

    @objc(potentialTapInProgress)
    var isPotentialTapInProgress: Bool {
        @objc(isPotentialTapInProgress)
        get { potentialTapIsInProgress }
    }

    var clickHighlightController: WKClickHighlightController? { clickHighlight }

    func disableDoubleTapGesturesDuringTapIfNecessary(forRequest requestID: UInt64) {
        guard latestTapID == requestID else {
            return
        }

        setDoubleTapGesturesEnabled(false)
    }

    func commitPotentialTapFailed() {
        guard view != nil else {
            return
        }

        handleTapCancelled()

        Logger.viewGestures.error("[pageProxyID=\(self.pageLogIdentifier)] \(#function): Commit potential tap failed")
    }

    func didCompleteSyntheticClick() {
        guard let webView = view else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(self.pageLogIdentifier)] \(#function): Synthetic click completed")

        let hasHoverActivatedByClick = webView._protectedPage().get()?.hoverActivatedByClickBounds().value != nil
        let hadHoverActivatedByClick = mayHaveHoverActivatedByClick
        mayHaveHoverActivatedByClick = hasHoverActivatedByClick
        if hadHoverActivatedByClick || hasHoverActivatedByClick {
            positionInformation.invalidate()
        }
    }

    func didHandleTapAsHover() {
        guard view != nil else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(self.pageLogIdentifier)] \(#function): Tap was handled as hover")

        mayHaveHoverActivatedByClick = true
        positionInformation.invalidate()
    }

    @objc(didNotHandleTapAsClickAtPoint:)
    func didNotHandleTapAsClick(at point: NSPoint) {
        guard view != nil else {
            return
        }

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Tap at (\(point.x, format: .fixed(precision: 0)), \(point.y, format: .fixed(precision: 0))) was not handled as click
            """
        )

        // FIXME: Consider smart magnification here if a double tap is pending and the point hasn't moved significantly.
    }
}

@objc(WKClickHighlightControllerDelegate)
@implementation
extension WKAppKitGestureController {
    func clickHighlightController(_ controller: WKClickHighlightController, installHighlightView highlightView: WKClickHighlightView) {
        view?._impl()?.installClickHighlightView(highlightView)
    }

    func clickHighlightControllerContentScale(_ controller: WKClickHighlightController) -> CGFloat {
        // The highlighted quads are already in view coordinates.
        1
    }

    func clickHighlightControllerCornerRadiusScale(_ controller: WKClickHighlightController) -> CGFloat {
        guard let page = view?._protectedPage().get() else {
            return 1
        }

        return page.pageScaleFactor() * page.viewScaleFactor()
    }

    func clickHighlightControllerUnobscuredRect(_ controller: WKClickHighlightController) -> CGRect {
        unobscuredRect
    }

    func clickHighlightControllerBoundaryRect(_ controller: WKClickHighlightController) -> CGRect {
        unobscuredRect
    }

    func clickHighlightControllerIsPotentialTapInProgress(_ controller: WKClickHighlightController) -> Bool {
        potentialTapIsInProgress
    }
}

extension WebKit.WebPageProxy {
    // A copy of the insets: obscuredContentInsets() returns a reference into the page's internals, which live as
    // long as the page, and the page is held for the duration of this call.
    fileprivate var obscuredContentInsets: WebCore.FloatBoxExtent {
        unsafe __obscuredContentInsetsUnsafe().pointee
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
