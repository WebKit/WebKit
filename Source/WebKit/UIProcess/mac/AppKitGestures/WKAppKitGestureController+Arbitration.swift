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
private import AppKit_Private.NSPanGestureRecognizer_Private
private import WebCore_Private

// The two recognizers a delegate call is about, for rules that hold whichever of the two is which.
private struct RecognizerPair {
    let first: NSGestureRecognizer
    let second: NSGestureRecognizer

    init(_ first: NSGestureRecognizer, _ second: NSGestureRecognizer) {
        self.first = first
        self.second = second
    }

    func contains(_ recognizer: NSGestureRecognizer?) -> Bool {
        first === recognizer || second === recognizer
    }

    func contains(where predicate: (NSGestureRecognizer) -> Bool) -> Bool {
        predicate(first) || predicate(second)
    }

    func isPair(_ a: NSGestureRecognizer?, _ b: NSGestureRecognizer?) -> Bool {
        (first === a && second === b) || (first === b && second === a)
    }

    // The other recognizer, if `recognizer` is one of the two.
    func partner(of recognizer: NSGestureRecognizer?) -> NSGestureRecognizer? {
        if first === recognizer {
            return second
        }
        if second === recognizer {
            return first
        }
        return nil
    }
}

private let scrollViewPanGestureClass: AnyClass? = NSClassFromString("NSScrollViewPanGestureRecognizer")

extension NSGestureRecognizer {
    fileprivate var isBuiltInScrollViewPan: Bool {
        guard let scrollViewPanGestureClass else {
            return false
        }
        return isKind(of: scrollViewPanGestureClass)
    }

    fileprivate var isBuiltInScrollViewMagnification: Bool {
        self is NSMagnificationGestureRecognizer && _isScrollGestureRecognizer
    }

    fileprivate var isCatchingDeceleratingScroll: Bool {
        guard let scrollView = view as? NSScrollView else {
            return false
        }

        // FIXME: _touchCaughtDeceleration was recently renamed in rdar://185652893, so keep this selector check for a bit.
        guard scrollView.responds(to: #selector(NSScrollView._touchCaughtDeceleration)) else {
            return false
        }

        return scrollView._touchCaughtDeceleration()
    }
}

extension WKWebView {
    // The text selection manager's gestures, read as the NSArray AppKit returns: the tests below are identity
    // tests, so no element is bridged or class-checked.
    fileprivate var textSelectionGestures: NSArray? {
        textSelectionManager?.gesturesForFailureRequirements as NSArray?
    }

    fileprivate func isTextSelectionGesture(_ gesture: NSGestureRecognizer) -> Bool {
        guard let gestures = textSelectionGestures else {
            return false
        }
        return gestures.indexOfObjectIdentical(to: gesture) != NSNotFound
    }
}

@objc(NSGestureRecognizerDelegate)
@implementation
extension WKAppKitGestureController {
    @objc(gestureRecognizer:shouldReceiveTouch:)
    func gestureRecognizer(_ gestureRecognizer: NSGestureRecognizer, shouldReceive touch: NSTouch) -> Bool {
        guard let webView = view, let impl = webView._impl(), !impl.ignoresAllEvents() else {
            return false
        }

        interruptMomentumIfNeeded(for: touch)

        if let panGestureRecognizer, gestureRecognizer === panGestureRecognizer {
            let touchActions = touchActions(at: touch.location(in: webView))
            panGestureRecognizer._canPanHorizontally = touchActions.allowsHorizontalPanning
            panGestureRecognizer._canPanVertically = touchActions.allowsVerticalPanning
        }

        return true
    }

    @objc(gestureRecognizer:shouldAttemptToRecognizeWithEvent:)
    func gestureRecognizer(_ gesture: NSGestureRecognizer, shouldAttemptToRecognizeWith event: NSEvent) -> Bool {
        // Every GR we maintain is exclusively direct touch-only, so we can unconditionally return false here.
        // This delegate implementation is only necessary for the magnification and rotation GRs since they
        // can not be configured as a (buttonMask + allowedTouchTypes) pair like the other gestures.

        // FIXME: <rdar://166268811> Migrate towards API (once available) that configures magnification/rotation GRs as touch-only.
        false
    }

    @objc(gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:)
    func gestureRecognizer(
        _ gestureRecognizer: NSGestureRecognizer,
        shouldRecognizeSimultaneouslyWith otherGestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        guard let webView = view else {
            return false
        }

        logArbitration(gestureRecognizer, otherGestureRecognizer)

        // Every rule but the last only allows, so their order does not matter.
        let pair = RecognizerPair(gestureRecognizer, otherGestureRecognizer)

        // Pan, magnification and rotation manipulate the page together; a click may become a pan or a drag.
        if gestureRecognizer !== otherGestureRecognizer && isOwnManipulationGesture(gestureRecognizer)
            && isOwnManipulationGesture(otherGestureRecognizer)
        {
            return true
        }

        if pair.isPair(singleClickGestureRecognizer, panGestureRecognizer)
            || pair.isPair(dragPressGestureRecognizer, singleClickGestureRecognizer)
        {
            return true
        }

        // Deferral, pointer tracking and the DOM double click recognize alongside everything. So does the
        // Live Text preflight (see the design note in WKAppKitGestureController+ImageAnalysis.swift): it is a
        // passive observer, and must never block, or be blocked by, the interaction it measures.
        if pair.contains(where: { $0 is WKDeferringGestureRecognizer }) || pair.contains(imageAnalysisGestureRecognizer)
            || pair.contains(pointerTrackingGestureRecognizer) || pair.contains(domDoubleClickGestureRecognizer)
        {
            return true
        }

        if gestureRecognizer is WKMouseTrackingGestureRecognizer
            && shouldRecognizeSimultaneouslyWithMouseTrackingGesture(otherGestureRecognizer)
        {
            return true
        }

        if otherGestureRecognizer is WKMouseTrackingGestureRecognizer
            && shouldRecognizeSimultaneouslyWithMouseTrackingGesture(gestureRecognizer)
        {
            return true
        }

        if gestureRecognizer === singleClickGestureRecognizer
            && otherGestureRecognizer.isBuiltInScrollViewPan
            && otherGestureRecognizer.view is NSScrollView
            && !otherGestureRecognizer.isCatchingDeceleratingScroll
        {
            return true
        }

        // Allow the single click GR to be simultaneously recognized with any of those from the text
        // selection manager.
        if let partner = pair.partner(of: singleClickGestureRecognizer) {
            return webView.isTextSelectionGesture(partner)
        }

        return false
    }

    @objc(gestureRecognizer:shouldBeRequiredToFailByGestureRecognizer:)
    func gestureRecognizer(
        _ gestureRecognizer: NSGestureRecognizer,
        shouldBeRequiredToFailBy otherGestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        guard let webView = view else {
            return false
        }

        logArbitration(gestureRecognizer, otherGestureRecognizer)

        if let deferringGestureRecognizer = gestureRecognizer as? WKDeferringGestureRecognizer {
            return deferringGestureRecognizer.shouldDefer(otherGestureRecognizer)
        }

        // Fail any gestures from the text selection manager if the secondary click GR handles them.
        return gestureRecognizer === secondaryClickGestureRecognizer && webView.isTextSelectionGesture(otherGestureRecognizer)
    }

    @objc(gestureRecognizer:shouldRequireFailureOfGestureRecognizer:)
    func gestureRecognizer(
        _ gestureRecognizer: NSGestureRecognizer,
        shouldRequireFailureOf otherGestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        guard view != nil else {
            return false
        }

        logArbitration(gestureRecognizer, otherGestureRecognizer)

        if gestureRecognizer === singleClickGestureRecognizer && otherGestureRecognizer === doubleClickGestureRecognizer {
            return true
        }

        if gestureRecognizer is WKMouseTrackingGestureRecognizer && otherGestureRecognizer === panGestureRecognizer {
            let panCanScroll = panGestureRecognizerCanScroll
            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                Mouse tracking requires pan to fail: \(panCanScroll)
                """
            )
            return panCanScroll
        }

        return false
    }

    @objc(gestureRecognizerShouldBegin:)
    func gestureRecognizerShouldBegin(_ gestureRecognizer: NSGestureRecognizer) -> Bool {
        guard let webView = view else {
            return false
        }

        if gestureRecognizer === pointerTrackingGestureRecognizer {
            return pointerTrackingShouldBegin(
                at: gestureRecognizer.location(in: webView),
                modifierFlags: gestureRecognizer.modifierFlags
            )
        }

        Logger.viewGestures.debug(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Gesture: \(gestureRecognizer.loggingDescription)
            """
        )

        if gestureRecognizer is WKDeferringGestureRecognizer {
            return true
        }

        let locationInViewCoordinates =
            if let panGestureRecognizer, gestureRecognizer === panGestureRecognizer {
                panGestureRecognizer.startLocation(in: webView)
            } else {
                gestureRecognizer.location(in: webView)
            }

        // While catching a decelerating scroll, only select gestures are allowed to begin:
        // - single click, so it can reset the interruption state
        // - mouse tracking or pan, so they can continue with successive scrolls (scrollbar drag for the former)
        // - magnification or rotation, so they can continue with simultaneous manipulation gestures
        if scrollState.caughtDeceleratingScroll {
            if gestureRecognizer === singleClickGestureRecognizer {
                return true
            }
            if gestureRecognizer is WKMouseTrackingGestureRecognizer && isPointInScrollbar(at: locationInViewCoordinates) {
                return true
            }
            if !isOwnManipulationGesture(gestureRecognizer) {
                return false
            }
        }

        // An event over a scrollbar is a scrollbar interaction; only a mouse-tracking gesture (which drives
        // `Scrollbar::mouseDown` -> thumb drag) should handle it. The AppKit text-selection/context-menu gestures
        // are handled separately in the deferral delegate.
        if isPointInScrollbar(at: locationInViewCoordinates) && !(gestureRecognizer is WKMouseTrackingGestureRecognizer) {
            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                Denying gesture over scrollbar: \(gestureRecognizer.loggingDescription)
                """
            )
            return false
        }

        if gestureRecognizer === doubleClickGestureRecognizer {
            return webView._impl()?.allowsMagnification() ?? false
        }

        // Live Text (see the design note in WKAppKitGestureController+ImageAnalysis.swift): the preflight is a
        // passive observer and is only enabled when Live Text is available, so it's always allowed to begin to
        // measure the press.
        if gestureRecognizer === imageAnalysisGestureRecognizer {
            return true
        }

        if gestureRecognizer === domDoubleClickGestureRecognizer {
            return true
        }

        if gestureRecognizer === secondaryClickGestureRecognizer {
            return secondaryClickShouldBegin(at: locationInViewCoordinates)
        }

        if gestureRecognizer === dragPressGestureRecognizer {
            return dragPressShouldBegin(at: locationInViewCoordinates)
        }

        if gestureRecognizer === panGestureRecognizer {
            return panShouldBegin(at: locationInViewCoordinates)
        }

        if gestureRecognizer === singleClickGestureRecognizer {
            return clickShouldBegin(at: locationInViewCoordinates, in: webView)
        }

        if let mouseTrackingGesture = gestureRecognizer as? WKMouseTrackingGestureRecognizer {
            guard canTrackMouse(with: mouseTrackingGesture) else {
                Logger.viewGestures.debug(
                    """
                    [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                    Denying mouse tracking gesture: \(gestureRecognizer.loggingDescription)
                    """
                )
                return false
            }
            return clickShouldBegin(at: locationInViewCoordinates, in: webView)
        }

        return true
    }

    @objc(_gestureRecognizer:canPreventGestureRecognizer:)
    private func gestureRecognizer(
        _ preventingGestureRecognizer: NSGestureRecognizer,
        canPrevent preventedGestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        guard let webView = view else {
            return false
        }

        Logger.viewGestures.debug(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Preventing gesture: \(preventingGestureRecognizer.loggingDescription), Prevented gesture: \
            \(preventedGestureRecognizer.loggingDescription)
            """
        )

        // None of our gesture recognizers may prevent an enclosing scroll view's pan (or any other
        // scroll/zoom) gesture, so that a scroll can always be handed off to the enclosing scroll view
        // e.g. a scroll over a draggable <img> in a non-scrollable web view.
        if isManipulationGesture(preventedGestureRecognizer) {
            return false
        }

        // Live Text (see the design note in WKAppKitGestureController+ImageAnalysis.swift): the preflight is a
        // passive observer, so it prevents nothing. Unlike pointer tracking and the DOM double click, it can
        // itself be prevented.
        if preventingGestureRecognizer === imageAnalysisGestureRecognizer {
            return false
        }

        let pair = RecognizerPair(preventingGestureRecognizer, preventedGestureRecognizer)
        if pair.contains(pointerTrackingGestureRecognizer) || pair.contains(domDoubleClickGestureRecognizer) {
            return false
        }

        // Live Text: when the fallback deferral recognizes to block drag / context menu (text found), it must
        // only affect the drag-press and secondary-click recognizers it defers -- never the text-selection
        // gestures, which Live Text needs to proceed. (Its hold over drag-press / secondary-click is enforced
        // by the deferral failure-requirement, not by this prevention hook.)
        if preventingGestureRecognizer === imageAnalysisDragAndContextMenuDeferringGestureRecognizer {
            return preventedGestureRecognizer === dragPressGestureRecognizer
                || preventedGestureRecognizer === secondaryClickGestureRecognizer
        }

        guard isOwnClickGesture(preventingGestureRecognizer) else {
            return true
        }

        // Don't let other click gestures prevent the secondary click GR; it must be allowed to fire its
        // press timer (0.72s) without being short-circuited by gestures that recognize earlier
        // (e.g. single click and mouse-tracking, which both transition to Began at mouse-down).
        if preventedGestureRecognizer === secondaryClickGestureRecognizer {
            return false
        }

        // Don't let our click gestures prevent text selection manager gestures;
        // they should be allowed to recognize simultaneously (see gestureRecognizer(_:shouldRecognizeSimultaneouslyWith:)).
        if webView.isTextSelectionGesture(preventedGestureRecognizer) {
            return false
        }

        return true
    }
}

extension WKAppKitGestureController {
    // Over selected text, a click or a drag belongs to the text selection gestures.
    private func clickShouldBegin(at locationInView: NSPoint, in webView: WKWebView) -> Bool {
        guard let impl = webView._impl() else {
            return false
        }
        return !impl.isTextSelectedAtPoint(locationInView)
    }

    private func shouldRecognizeSimultaneouslyWithMouseTrackingGesture(_ gesture: NSGestureRecognizer) -> Bool {
        guard let webView = view else {
            return false
        }

        if gesture is WKMouseTrackingGestureRecognizer || isOwnManipulationGesture(gesture) {
            return true
        }

        if gesture === doubleClickGestureRecognizer || gesture === singleClickGestureRecognizer || gesture === dragPressGestureRecognizer {
            return true
        }

        if gesture.name == "NSThemeFrame.panTouchGestureRecognizer" {
            return true
        }

        if webView.isTextSelectionGesture(gesture) {
            return true
        }

        // An enclosing scroll view pan can report that it "began" even while being deferred,
        // so let mouse tracking recognize simultaneously until we have a deferral decision.
        return isEnclosingScrollViewPanHeldByDeferral(gesture)
    }

    private func isEnclosingScrollViewPanHeldByDeferral(_ gesture: NSGestureRecognizer) -> Bool {
        guard let enclosingScrollViewPanDeferringGestureRecognizer else {
            return false
        }

        let deferralState = enclosingScrollViewPanDeferringGestureRecognizer.state
        return (deferralState == .possible || deferralState == .ended)
            && deferringGestureRecognizer(enclosingScrollViewPanDeferringGestureRecognizer, shouldDeferOtherGestureRecognizer: gesture)
    }

    private func isOwnClickGesture(_ gesture: NSGestureRecognizer) -> Bool {
        gesture === singleClickGestureRecognizer
            || gesture === secondaryClickGestureRecognizer
            || gesture is WKMouseTrackingGestureRecognizer
            || gesture === dragPressGestureRecognizer
    }

    private func isOwnManipulationGesture(_ gesture: NSGestureRecognizer) -> Bool {
        gesture === panGestureRecognizer || gesture === magnificationGestureRecognizer || gesture === rotationGestureRecognizer
    }

    private func isManipulationGesture(_ gesture: NSGestureRecognizer) -> Bool {
        isOwnManipulationGesture(gesture)
            || gesture.isBuiltInScrollViewPan
            || gesture.isBuiltInScrollViewMagnification
    }

    // MARK: Deferral

    enum DeferralResolution {
        case preventGestures
        case allowGestures
        case keepDeferring

        init(preventingGestures: Bool) {
            self = preventingGestures ? .preventGestures : .allowGestures
        }
    }

    private func resolveDeferral(
        of deferringGestureRecognizer: WKDeferringGestureRecognizer,
        info: WebKit.InteractionInformationAtPosition
    ) -> DeferralResolution {
        let overLiveTextImage = WebKit.isLiveTextAvailableAndEnabled() && info.isImage

        if let deferral = imageAnalysisDeferral(of: deferringGestureRecognizer) {
            // Image-analysis deferrals (see the design note in WKAppKitGestureController+ImageAnalysis.swift):
            // over an image with Live Text available, keep deferring until the preflight's analysis token
            // resolves both deferrals (see resolveImageAnalysisDeferrals(with:)); otherwise this deferral has no
            // opinion, so release. We only have basic hit-test info here (this request does not fetch
            // image data), so key off info.isImage; the preflight does the full analyzability check.
            if overLiveTextImage {
                if let reusableOutcome = lastCompletedImageAnalysis?.outcome(for: info.hostImageOrVideoElementContext.value) {
                    let prevent = reusableOutcome.preventsGestures(deferredBy: deferral)
                    Logger.viewGestures.debug(
                        """
                        [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                        deferral resolved: reusing completed image analysis outcome \
                        (prevent=\(prevent))
                        """
                    )
                    return DeferralResolution(preventingGestures: prevent)
                }

                if imageAnalysisDeferralState == .gestureFailed {
                    Logger.viewGestures.debug(
                        """
                        [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                        deferral resolved: image analysis preflight failed; not preventing
                        """
                    )
                    return .allowGestures
                }

                imageAnalysisDeferralState = .deferring
                return .keepDeferring
            }

            return .allowGestures
        }

        if deferringGestureRecognizer === dragDeferringGestureRecognizer {
            let isDraggable = info.representsDraggableElement

            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                deferral resolved: isDraggable=\(isDraggable) \
                (link=\(info.isLink) image=\(info.isImage) \
                attachment=\(info.isAttachment) \
                dhtml=\(info.isDHTMLDraggable) color=\(info.isColorInput) \
                prefersDrag=\(info.prefersDraggingOverTextSelection))
                """
            )

            return DeferralResolution(preventingGestures: isDraggable && !overLiveTextImage)
        }

        if deferringGestureRecognizer === secondaryClickDeferringGestureRecognizer {
            let isSelectable = info.representsSelectableContent
            let prefersManipulation = info.prefersDirectManipulation

            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                deferral resolved: isSelectable=\(isSelectable) \
                prefersDirectManipulation=\(prefersManipulation) \
                (selectability=\(info.selectability.rawValue) \
                overEditableContent=\(info.isOverEditableContent))
                """
            )

            return DeferralResolution(preventingGestures: (!isSelectable || prefersManipulation) && !overLiveTextImage)
        }

        if deferringGestureRecognizer === enclosingScrollViewPanDeferringGestureRecognizer {
            let prefersManipulation = info.prefersDirectManipulation

            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                deferral resolved: enclosing scroll view pan yields to content=\(prefersManipulation)
                """
            )

            return DeferralResolution(preventingGestures: prefersManipulation)
        }

        preconditionFailure("Unexpected deferring gesture recognizer")
    }
}

@objc(WKDeferringGestureRecognizerDelegate)
@implementation
extension WKAppKitGestureController {
    @objc(deferringGestureRecognizer:shouldDeferOtherGestureRecognizer:)
    func deferringGestureRecognizer(
        _ deferringGestureRecognizer: WKDeferringGestureRecognizer,
        shouldDeferOtherGestureRecognizer gestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        guard let webView = view else {
            return false
        }

        // Live Text (see the design note in WKAppKitGestureController+ImageAnalysis.swift): the fallback deferral
        // holds the drag-press / secondary-click recognizers -- not text selection -- until analysis resolves, so
        // a slow analysis can't let drag-press fire and drag the image before Live Text has a chance to win.
        if deferringGestureRecognizer === imageAnalysisDragAndContextMenuDeferringGestureRecognizer {
            return gestureRecognizer === dragPressGestureRecognizer || gestureRecognizer === secondaryClickGestureRecognizer
        }

        // Defer panning any scroll view that encloses the web view so that we can delegate to direct-manipulation elements if needed.
        if deferringGestureRecognizer === enclosingScrollViewPanDeferringGestureRecognizer {
            guard gestureRecognizer.isBuiltInScrollViewPan, let scrollView = gestureRecognizer.view as? NSScrollView else {
                return false
            }
            return webView.isDescendant(of: scrollView)
        }

        return webView.isTextSelectionGesture(gestureRecognizer)
    }

    @objc(deferringGestureRecognizer:shouldDeferGesturesForEventThatWillBeginTouches:)
    func deferringGestureRecognizer(
        _ deferringGestureRecognizer: WKDeferringGestureRecognizer,
        shouldDeferGesturesForEventThatWillBeginTouches event: NSEvent
    ) -> Bool {
        guard let webView = view else {
            return false
        }

        if imageAnalysisDeferral(of: deferringGestureRecognizer) != nil {
            imageAnalysisDeferralState = .undecided
        }

        if deferringGestureRecognizer === enclosingScrollViewPanDeferringGestureRecognizer {
            if webView.enclosingScrollView == nil || !(webView._protectedPage().get()?.hasRunningProcess() ?? false) {
                return false
            }

            if scrollState.caughtDeceleratingScroll {
                return false
            }
        }

        // An event that catches a decelerating scroll only stops the scroll (and may continue it as a new pan),
        // so it must never begin a text selection or any other deferred gesture, no matter how long it's held
        // before moving.
        if scrollState.caughtDeceleratingScroll {
            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                deferral resolved: event caught a decelerating scroll; preventing deferred gestures
                """
            )
            deferringGestureRecognizer.endDeferralShouldPreventGestures(true)
            return true
        }

        let locationInView = webView.convert(event.locationInWindow, from: nil)

        let isInScrollbar = isPointInScrollbar(at: locationInView)

        if !isInScrollbar && !clickShouldBegin(at: locationInView, in: webView) {
            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                deferral: not deferring; text already selected at \(NSStringFromPoint(locationInView))
                """
            )
            return false
        }

        // This deferral waits on the page's reply to the pointerdown and not on position
        // information, so there is only something to wait for if pointer tracking will begin.
        if deferringGestureRecognizer === pointerDownDeferringGestureRecognizer {
            return pointerTrackingShouldBegin(at: locationInView, modifierFlags: event.modifierFlags)
        }

        Logger.viewGestures.debug(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            deferral: deferring; awaiting position info at \(NSStringFromPoint(locationInView))
            """
        )

        let request = WebKit.InteractionInformationRequest(touchAt: locationInView)
        positionInformation.doAfterUpdate(request) { [weak self, weak deferringGestureRecognizer] info in
            guard let self, let deferringGestureRecognizer else {
                return
            }

            guard let info else {
                if deferringGestureRecognizer === enclosingScrollViewPanDeferringGestureRecognizer
                    && deferringGestureRecognizer.state == .possible
                {
                    deferringGestureRecognizer.endDeferralShouldPreventGestures(false)
                }
                return
            }

            guard view != nil else {
                return
            }

            let deferralState = deferringGestureRecognizer.state
            if deferralState != .possible {
                Logger.viewGestures.debug(
                    """
                    [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                    deferral: position info arrived after deferring gesture exited Possible (state=\(deferralState.rawValue)); \
                    skipping resolution
                    """
                )
                return
            }

            // Over a scrollbar: drive the scrollbar, not text selection -- prevent the deferred gestures
            // so the mouse-tracking -> `Scrollbar::mouseDown` path wins.
            if isInScrollbar {
                Logger.viewGestures.debug(
                    """
                    [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                    deferral resolved: over scrollbar; preventing text-selection gestures
                    """
                )
                deferringGestureRecognizer.endDeferralShouldPreventGestures(true)
                return
            }

            switch resolveDeferral(of: deferringGestureRecognizer, info: info) {
            case .preventGestures:
                deferringGestureRecognizer.endDeferralShouldPreventGestures(true)
            case .allowGestures:
                deferringGestureRecognizer.endDeferralShouldPreventGestures(false)
            case .keepDeferring:
                break
            }
        }

        return true
    }

    @objc(deferringGestureRecognizer:didEndTouchesWithEvent:)
    func deferringGestureRecognizer(_ deferringGestureRecognizer: WKDeferringGestureRecognizer, didEndTouchesWith event: NSEvent) {
        guard deferringGestureRecognizer.state == .possible else {
            return
        }

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            deferral: touch ended before position info arrived; unblocking text selection
            """
        )

        if deferringGestureRecognizer !== pointerDownDeferringGestureRecognizer {
            positionInformation.abandonOutstandingRequest()
        }

        deferringGestureRecognizer.endDeferralShouldPreventGestures(false)
    }

    @objc(deferringGestureRecognizer:didTransitionToState:)
    func deferringGestureRecognizer(
        _ deferringGestureRecognizer: WKDeferringGestureRecognizer,
        didTransitionTo state: NSGestureRecognizer.State
    ) {
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
