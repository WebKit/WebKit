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
private import WebCore_Private

// Gesture-driven Live Text is arbitrated by three recognizers, plus ImageAnalysisGestureDeferralToken:
//
//   1. imageAnalysisGestureRecognizer (the "preflight") -- a settled 0.1s press that kicks off Vision
//      image analysis. It's a passive observer of the interaction it measures: it recognizes
//      simultaneously with everything, prevents nothing, and is always allowed to begin.
//   2. imageAnalysisTextSelectionDeferringGestureRecognizer -- holds the text-selection gestures over an image until
//      analysis resolves.
//   3. imageAnalysisDragAndContextMenuDeferringGestureRecognizer -- holds the drag-press and secondary-click
//      (context menu) recognizers over an image until analysis resolves.
//
// The two deferrals resolve to *opposite* booleans (see resolveImageAnalysisDeferrals(with:)):
// on "text found" we must ALLOW text selection while PREVENTING drag / context menu (Live Text wins),
// and the reverse on "no text". A single WKDeferringGestureRecognizer can't express that -- its
// endDeferralShouldPreventGestures(_:) applies one boolean (state = .ended / .failed) to everything it
// defers -- so the two opposite outcomes require two separate deferrals.
//
// iOS needs no fallback deferral because its drag / context menu are UIKit interactions gated by a
// ProceedWithTextSelectionInImage callback. macOS drag-press / secondary-click are NSGestureRecognizers
// on independent timers, so they must be *deferred* to keep them from firing before analysis resolves.

extension WKAppKitGestureController {
    // The outcome of the image-analysis preflight, used to resolve the two image-analysis deferring
    // gestures. Over an image we defer both text selection and the drag / context-menu fallback until
    // analysis finishes, then resolve them oppositely.
    enum ImageAnalysisDeferralOutcome {
        /// Not an image (or the press was abandoned): influence neither deferral.
        case notApplicable
        /// Image without selectable text: prevent text selection; allow drag / context menu.
        case noText
        /// Image with selectable text: allow text selection; prevent drag / context menu.
        case foundText

        init(foundText: Bool) {
            self = foundText ? .foundText : .noText
        }

        func preventsGestures(deferredBy deferral: ImageAnalysisDeferral) -> Bool {
            // Text found -> Live Text wins: allow the deferred text selection, prevent the drag / context-menu
            // fallback. No text -> the reverse.
            switch (self, deferral) {
            case (.noText, .textSelection), (.foundText, .dragAndContextMenu):
                true
            default:
                false
            }
        }
    }

    enum ImageAnalysisDeferral {
        case textSelection
        case dragAndContextMenu
    }

    enum ImageAnalysisDeferralState {
        case undecided
        case deferring
        case gestureFailed
    }

    struct CompletedImageAnalysis {
        let element: WebCore.ElementContext
        let outcome: ImageAnalysisDeferralOutcome

        func outcome(for hostElement: WebCore.ElementContext?) -> ImageAnalysisDeferralOutcome? {
            guard let hostElement, element.isSameElement(hostElement) else {
                return nil
            }
            return outcome
        }
    }

    // Keeps the image-analysis deferrals open for the lifetime of an in-flight analysis. When the last
    // reference is dropped (analysis finished or abandoned), it resolves both deferrals on the main run
    // loop according to the recorded outcome. Mirrors the iOS ImageAnalysisGestureDeferralToken.
    @MainActor
    final class ImageAnalysisGestureDeferralToken {
        private weak var controller: WKAppKitGestureController?
        var outcome: ImageAnalysisDeferralOutcome = .notApplicable

        init(controller: WKAppKitGestureController) {
            self.controller = controller
        }

        isolated deinit {
            controller?.resolveImageAnalysisDeferrals(with: outcome)
        }
    }
}

extension WKAppKitGestureController {
    // MARK: Gesture Recognizers

    private static let imageAnalysisMinimumPressDuration: TimeInterval = 0.1

    func makeImageAnalysisGestureRecognizer() -> WKPressGestureRecognizer {
        makeGestureRecognizer(
            WKPressGestureRecognizer(target: self, action: #selector(imageAnalysisGestureRecognized)),
            name: "WKImageAnalysisGesture"
        ) {
            // Mirrors the iOS WKImageAnalysisGestureRecognizer: a short, stationary long-press that starts
            // preflighting Live Text as soon as a press settles on an image. Movement cancels it, so a
            // scroll or drag over an image does not trigger analysis.
            $0.minimumPressDuration = Self.imageAnalysisMinimumPressDuration
            $0.allowableMovement = 0
            $0.cancelPastAllowableMovement = true
            $0.buttonMask = 0
            $0.numberOfTouchesRequired = 1
            $0.refusesToBeFailureRequirement = true
            $0.pressDelegate = self
        }
    }

    func makeImageAnalysisDeferringGestureRecognizers() -> (
        textSelection: WKDeferringGestureRecognizer, dragAndContextMenu: WKDeferringGestureRecognizer
    ) {
        // One deferral holds text selection over an image until analysis resolves; the other holds the
        // drag-press / secondary-click recognizers. See the design note at the top of this file for why both
        // exist and why they resolve to opposite outcomes.
        (
            makeImageAnalysisDeferringGestureRecognizer(named: "WKImageAnalysisTextSelectionDeferringGesture"),
            makeImageAnalysisDeferringGestureRecognizer(named: "WKImageAnalysisDragAndContextMenuDeferringGesture")
        )
    }

    private func makeImageAnalysisDeferringGestureRecognizer(named name: String) -> WKDeferringGestureRecognizer {
        makeGestureRecognizer(WKDeferringGestureRecognizer(deferringGestureDelegate: self), name: name) {
            $0.immediatelyFailsAfterTouchEnd = true
        }
    }

    // MARK: Preflight and Deferrals

    @objc
    private func imageAnalysisGestureRecognized(_ gesture: NSGestureRecognizer) {
        guard let webView = view else {
            return
        }

        precondition(imageAnalysisGestureRecognizer === gesture)

        guard gesture.state == .began else {
            return
        }

        guard let impl = webView._impl(), !impl.ignoresAllEvents() else {
            return
        }

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            \(gesture.loggingDescription)
            """
        )

        var request = WebKit.InteractionInformationRequest(touchAt: gesture.location(in: webView))
        request.includeImageData = true

        // The token keeps the image-analysis deferral open until the async analysis chain finishes (or is
        // abandoned). Capturing it through each stage means the deferral resolves exactly when the last stage
        // completes; dropping it early (e.g. not an image) resolves it as "allow text selection".
        let gestureDeferralToken = ImageAnalysisGestureDeferralToken(controller: self)
        positionInformation.doAfterUpdate(request) { [weak self] info in
            guard let info, let self else {
                return
            }

            support.preflightImageAnalysis(info) { [weak self] foundText, element in
                let outcome = ImageAnalysisDeferralOutcome(foundText: foundText)

                gestureDeferralToken.outcome = outcome
                self?.lastCompletedImageAnalysis = CompletedImageAnalysis(element: element, outcome: outcome)
            }
        }
    }

    func imageAnalysisDeferral(of deferringGestureRecognizer: WKDeferringGestureRecognizer) -> ImageAnalysisDeferral? {
        if deferringGestureRecognizer === imageAnalysisTextSelectionDeferringGestureRecognizer {
            return .textSelection
        }
        if deferringGestureRecognizer === imageAnalysisDragAndContextMenuDeferringGestureRecognizer {
            return .dragAndContextMenu
        }
        return nil
    }

    func resolveImageAnalysisDeferrals(with outcome: ImageAnalysisDeferralOutcome) {
        if imageAnalysisDeferralState == .deferring {
            imageAnalysisDeferralState = .undecided
        }

        // Only resolve a deferral that's still deferring. A deferral may already be resolved by the time the
        // analysis token drops -- e.g. it failed on lift (immediatelyFailsAfterTouchEnd) before slow
        // analysis finished -- and re-setting an already-terminal gesture state would be a no-op at best.
        imageAnalysisTextSelectionDeferringGestureRecognizer?
            .endDeferralIfStillDeferring(
                preventingGestures: outcome.preventsGestures(deferredBy: .textSelection)
            )
        imageAnalysisDragAndContextMenuDeferringGestureRecognizer?
            .endDeferralIfStillDeferring(
                preventingGestures: outcome.preventsGestures(deferredBy: .dragAndContextMenu)
            )
    }
}

extension WKDeferringGestureRecognizer {
    fileprivate func endDeferralIfStillDeferring(preventingGestures: Bool) {
        guard state == .possible else {
            return
        }
        endDeferralShouldPreventGestures(preventingGestures)
    }
}

@objc(WKPressGestureRecognizerDelegate)
@implementation
extension WKAppKitGestureController {
    func pressGestureRecognizerDidFail(_ gestureRecognizer: WKPressGestureRecognizer) {
        guard gestureRecognizer === imageAnalysisGestureRecognizer else {
            return
        }

        // Similar to iOS's -imageAnalysisGestureDidFail: if the initial gesture fails, resolve our deferrals immediately.
        guard imageAnalysisDeferralState == .deferring else {
            imageAnalysisDeferralState = .gestureFailed
            return
        }

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Image analysis preflight failed; releasing image-analysis deferrals
            """
        )

        resolveImageAnalysisDeferrals(with: .notApplicable)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
