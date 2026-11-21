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
private import AppKit_Private.NSTouch_Private
private import WebCore_Private

extension NSPanGestureRecognizer {
    func startLocation(in view: NSView) -> NSPoint {
        if responds(to: #selector(NSPanGestureRecognizer._startLocation(in:))) {
            return _startLocation(in: view)
        }

        return location(in: view)
    }

    func takeTranslation(in view: NSView) -> WebCore.FloatSize {
        let translation = WebCore.toFloatSize(WebCore.FloatPoint(self.translation(in: view)))
        setTranslation(.zero, in: view)
        return translation
    }

    fileprivate var prefersUnlockedScroll: Bool {
        touches.count > 1
    }

    fileprivate var refreshControllerEligibility: WebKit.RefreshControllerEligibility {
        touches.count == 1 ? .Eligible : .Ineligible
    }

    fileprivate var prefersForwardingToGestureController: Bool {
        touches.count == 1
    }

    fileprivate var supportsMomentumScroll: Bool {
        touches.count == 1
    }
}

extension WebCore.FloatSize {
    // rawPlatformDelta uses IOHIDEvent coordinate conventions, which have the opposite
    // sign from WebKit's delta field. This matches WebEventFactory.mm which negates
    // IOHIDEventFieldScrollX/Y values when extracting rawPlatformDelta from real events.
    fileprivate var rawPlatformDelta: WebCore.FloatSize {
        -self
    }
}

extension WKAppKitGestureController {
    // Momentum can start again while a caught decelerating scroll is still set, so the two are separate.
    struct ScrollGestureState {
        var isMomentumActive = false
        private(set) var caughtDeceleratingScroll = false
        var suppressNextPanScrollDelta = false
        var uidOfTouchThatCaughtDeceleratingScroll: Int?
        var fastScrollTracker = WKFastScrollTracker()
        var directionalScrollLockTracker = WKDirectionalScrollLockTracker()

        mutating func catchMomentum() {
            fastScrollTracker.didCatchMomentum()
            directionalScrollLockTracker.didCatchMomentum()
            caughtDeceleratingScroll = true
            suppressNextPanScrollDelta = true
        }

        mutating func resetCaughtDeceleratingScroll() {
            caughtDeceleratingScroll = false
            suppressNextPanScrollDelta = false
        }

        mutating func reset() {
            isMomentumActive = false
            resetCaughtDeceleratingScroll()
            fastScrollTracker.reset()
            directionalScrollLockTracker.reset()
        }
    }

    // The touch actions the gestures depend on; `none` is the empty set.
    struct TouchActions: OptionSet {
        let rawValue: UInt8

        static let auto = Self(rawValue: WebCore.TouchAction.Auto.rawValue)
        static let manipulation = Self(rawValue: WebCore.TouchAction.Manipulation.rawValue)
        static let panX = Self(rawValue: WebCore.TouchAction.PanX.rawValue)
        static let panY = Self(rawValue: WebCore.TouchAction.PanY.rawValue)
        static let pinchZoom = Self(rawValue: WebCore.TouchAction.PinchZoom.rawValue)

        var allowsHorizontalPanning: Bool { !isDisjoint(with: [.auto, .manipulation, .panX]) }
        var allowsVerticalPanning: Bool { !isDisjoint(with: [.auto, .manipulation, .panY]) }
        var allowsPanning: Bool { allowsHorizontalPanning || allowsVerticalPanning }
        var allowsPinchZoom: Bool { !isDisjoint(with: [.auto, .manipulation, .pinchZoom]) }
    }
}

extension WKAppKitGestureController {
    // MARK: Gesture Recognizers

    func makePanGestureRecognizer(for webView: WKWebView) -> WKPanGestureRecognizer {
        makeGestureRecognizer(
            WKPanGestureRecognizer(webView: webView, target: self, action: #selector(panGestureRecognized)),
            name: "WKPanGesture",
            configure: configure(forScrolling:)
        )
    }

    func configure(forScrolling gesture: NSPanGestureRecognizer) {
        // FIXME: Strictly, this should be `2` when allowsMagnification is true and the min/max zoom
        // scales are different, c.f. UIScrollView's _updatePanGestureConfiguration for prior art.
        gesture.maximumNumberOfTouches = 2
        gesture.allowedTouchTypes = .direct
        gesture.buttonMask = 0
    }

    // MARK: Pan Gesture

    @objc
    private func panGestureRecognized(_ gesture: NSPanGestureRecognizer) {
        guard let webView = view else {
            return
        }

        logGestureAction(gesture)

        precondition(panGestureRecognizer === gesture)

        guard let impl = webView._impl(), let panGestureRecognizer else {
            return
        }

        if impl.ignoresAllEvents() {
            Logger.viewGestures.debug("[pageProxyID=\(self.pageLogIdentifier)] \(#function): Ignored gesture")
            return
        }

        if gesture.state == .began {
            cancelPointerTrackingIfNeeded()
            impl.dismissContentRelativeChildWindowsWithAnimation(false)
            scrollState.fastScrollTracker.didStartGesture(atLocation: gesture.location(in: nil))
            scrollState.directionalScrollLockTracker.didStartGesture()
        }

        #if HAVE_NSREFRESHCONTROLLER
        impl.updateRefreshControllerForPanGesture(gesture.state, panGestureRecognizer.refreshControllerEligibility)
        #endif

        sendWheelEvent(for: panGestureRecognizer)
        startMomentumIfNeeded(for: panGestureRecognizer)

        if gesture.state.isTerminal {
            // After the wheel and momentum events above, so both still see this gesture's locked axis.
            scrollState.directionalScrollLockTracker.didEndGesture()
            scrollState.resetCaughtDeceleratingScroll()
        }
    }

    func panShouldBegin(at locationInView: NSPoint) -> Bool {
        guard view != nil else {
            return false
        }

        let requestIsValid = isPositionInformationValid(
            at: locationInView,
            radius: Self.panPositionInformationToleranceRadius
        )

        let information = positionInformation.currentInformation

        // FIXME: (rdar://181964604) Because of this logic, vertically scrolling over these elements likely will not work.
        let prefersInteraction = information.prefersDirectManipulation
        let usesHoverActivatedByClickBounds = !requestIsValid && isInsideHoverActivatedByClickBounds(at: locationInView)
        let yieldToContent =
            (requestIsValid && (prefersInteraction || information.isHoverActivatedByClick)) || usesHoverActivatedByClickBounds

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Pan shouldBegin → \(!yieldToContent) \
            (hasInfo=\(self.positionInformation.hasValidCurrentInformation) \
            valid=\(requestIsValid) prefersInteraction=\(prefersInteraction) \
            hoverActivatedByClick=\(information.isHoverActivatedByClick) \
            fallback=\(usesHoverActivatedByClickBounds))
            """
        )

        return !yieldToContent
    }

    // MARK: Touch Actions

    func touchActions(at point: NSPoint) -> TouchActions {
        TouchActions(rawValue: support.touchActions(at: point))
    }

    var panGestureRecognizerCanScroll: Bool {
        guard let webView = view else {
            return false
        }

        return touchActions(at: panGestureRecognizer?.location(in: webView) ?? .zero).allowsPanning
    }

    // MARK: Wheel Event Handling

    private func panVelocity(in view: NSView?) -> NSPoint {
        guard let panGestureRecognizer else {
            return .zero
        }
        return panGestureRecognizer.wk_velocity(in: view)
    }

    func sendWheelEvent(for gesture: NSPanGestureRecognizer) {
        guard let webView = view, let impl = webView._impl() else {
            return
        }

        let timestamp = WTF.MonotonicTime.fromRawSeconds(gesture.timestamp)
        let locationInViewCoordinates = gesture.location(in: webView)
        let locationInView = WebCore.FloatPoint(locationInViewCoordinates)
        let position = WebCore.IntPoint(locationInView)
        let globalPosition = WebCore.globalPoint(gesture.location(in: nil), impl.window())
        var gestureDelta = gesture.takeTranslation(in: webView)

        if scrollState.suppressNextPanScrollDelta {
            scrollState.suppressNextPanScrollDelta = false
            gestureDelta = WebCore.FloatSize()
        }

        guard let page = webView._protectedPage().get() else {
            return
        }

        if page.delegatesScalingToUIProcess(), let gestureController = impl.gestureController(),
            gestureController.hasActiveMagnificationGesture()
        {
            gestureDelta = WebCore.FloatSize()
            gestureController.moveMagnificationOrigin(locationInView)
        }

        let pinnedState = page.pinnedStateIncludingAncestorsAtPoint(locationInView)
        let canScrollHorizontally = gesture._canPanHorizontally && !(pinnedState[.Left] && pinnedState[.Right])
        let canScrollVertically = gesture._canPanVertically && !(pinnedState[.Top] && pinnedState[.Bottom])
        gestureDelta = WebCore.FloatSize(
            scrollState.directionalScrollLockTracker.update(
                delta: CGSize(gestureDelta),
                canScrollHorizontally: canScrollHorizontally,
                canScrollVertically: canScrollVertically,
                prefersUnlocked: gesture.prefersUnlockedScroll,
                time: gesture.timestamp
            )
        )

        func makeWheelEvent(_ delta: WebCore.FloatSize, phase: WebKit.WebEventPhase) -> WebKit.WebWheelEvent {
            var event = WebKit.WebEventData()
            event.type = .Wheel
            event.timestamp = timestamp

            var wheel = WebKit.WebWheelEventData()
            wheel.position = position
            wheel.globalPosition = WebCore.IntPoint(globalPosition)
            wheel.delta = delta
            wheel.wheelTicks = delta.scaled(1 / Float(WebCore.Scrollbar.pixelsPerLineStep()))
            wheel.granularity = .ScrollByPixelWheelEvent
            wheel.phase = phase
            wheel.hasPreciseScrollingDeltas = true
            wheel.scrollCount = 1
            wheel.unacceleratedScrollingDelta = delta
            wheel.ioHIDEventTimestamp = timestamp
            wheel.inputSource = .Touch
            return WebKit.WebWheelEvent.create(consuming: WebKit.WebWheelEventInit(event: event, wheel: wheel)).get()
        }

        let phase = WebKit.WebEventPhase(gesture.state)
        if phase == .Began {
            hasEverMagnifiedDuringCurrentGesture = false
        }
        // The third argument is the C++ default, which Swift does not take from the declaration.
        if !WTF.areEssentiallyEqual(impl.magnification(), 1, Double.ulpOfOne) {
            hasEverMagnifiedDuringCurrentGesture = true
        }

        let forwardToGestureController = impl.allowsBackForwardNavigationGestures() && gesture.prefersForwardingToGestureController

        func dispatchWheelEvent(_ delta: WebCore.FloatSize, phase: WebKit.WebEventPhase) {
            interruptMomentumEventGenerator()

            lastPanWheelEventWasHandledByViewGestureController =
                forwardToGestureController
                && impl.ensureGestureController()
                    .handleScrollWheelEvent(
                        WebKit.NativeWebWheelEvent.create(makeWheelEvent(delta, phase: phase)).get()
                    )
            if lastPanWheelEventWasHandledByViewGestureController {
                Logger.viewGestures.debug(
                    "[pageProxyID=\(self.pageLogIdentifier)] \(#function): View gesture controller handled gesture"
                )
                return
            }

            page.handleNativeWheelEvent(consuming: WebKit.NativeWebWheelEvent.create(makeWheelEvent(delta, phase: phase)))
        }

        if phase == .Began {
            dispatchWheelEvent(WebCore.FloatSize(), phase: .MayBegin)
        }

        dispatchWheelEvent(gestureDelta, phase: phase)
    }

    private static let fallbackMomentumFrameRate: WebCore.FramesPerSecond = 60

    private static func momentumFrameRate(for event: WebKit.NativeWebWheelEvent) -> Float {
        if let curve = Optional(fromCxx: WebKit.ScrollingAccelerationCurve.fromNativeWheelEvent(event)) {
            return curve.frameRate()
        }
        if let curve = Optional(fromCxx: WebKit.ScrollingAccelerationCurve.fallbackCurve()) {
            return curve.frameRate()
        }
        return Float(fallbackMomentumFrameRate)
    }

    private func sendMomentumBeganEvent(
        for gesture: NSPanGestureRecognizer,
        in webView: WKWebView,
        velocity: WebCore.FloatSize,
        fastScrollMultiplier: Double
    ) {
        guard let impl = webView._impl(), let page = webView._protectedPage().get() else {
            return
        }

        let position = WebCore.IntPoint(gesture.location(in: webView))
        let globalPosition = WebCore.IntPoint(WebCore.globalPoint(gesture.location(in: nil), impl.window()))
        let timestamp = WTF.MonotonicTime.fromRawSeconds(gesture.timestamp)

        var event = WebKit.WebEventData()
        event.type = .Wheel
        event.timestamp = timestamp

        var wheel = WebKit.WebWheelEventData()
        wheel.position = position
        wheel.globalPosition = globalPosition
        wheel.granularity = .ScrollByPixelWheelEvent
        wheel.momentumPhase = .Began
        wheel.hasPreciseScrollingDeltas = true
        wheel.scrollCount = 1
        wheel.ioHIDEventTimestamp = timestamp
        wheel.inputSource = .Touch
        wheel.momentumFastScrollMultiplier = Float(fastScrollMultiplier)
        let momentumEvent = WebKit.NativeWebWheelEvent.create(
            WebKit.WebWheelEvent.create(consuming: WebKit.WebWheelEventInit(event: event, wheel: wheel)).get()
        )

        let initialMomentumDelta = velocity / Self.momentumFrameRate(for: momentumEvent.get())
        momentumEvent.get().setRawPlatformDelta(consuming: .init(initialMomentumDelta.rawPlatformDelta))

        #if HAVE_NSREFRESHCONTROLLER
        // The refresh controller should not track synthetic momentum scrolling.
        impl.clearRefreshControllerTracking()
        #endif

        #if ENABLE_MOMENTUM_EVENT_DISPATCHER
        // Started first, so that any momentum it ends is ended before this momentum begins.
        WebKit.CxxInteropSupport.ensureMomentumEventGenerator(support).start(page, momentumEvent.get())
        #endif
        page.handleNativeWheelEvent(consuming: momentumEvent)
    }

    private func interruptMomentumEventGenerator() {
        #if ENABLE_MOMENTUM_EVENT_DISPATCHER
        WebKit.CxxInteropSupport.momentumEventGenerator(support)?.interrupt()
        #endif
    }

    // MARK: Momentum Handling

    private static let minimumVelocityForSwipe: Float = 20
    private static let minimumVelocityForMomentum: Float = 250

    func startMomentumIfNeeded(for gesture: NSPanGestureRecognizer) {
        guard let webView = view else {
            return
        }

        guard gesture.state == .ended, gesture.supportsMomentumScroll else {
            return
        }

        // Momentum started now would scroll out of sight and then show up all at once.
        if isTransformGestureActiveOrEnding {
            scrollState.fastScrollTracker.reset()
            return
        }

        // A swipe navigates instead of scrolling, so it has no momentum. Momentum would scroll the page navigated to.
        if lastPanWheelEventWasHandledByViewGestureController {
            scrollState.fastScrollTracker.reset()
            return
        }

        let unfilteredVelocity = WebCore.toFloatSize(WebCore.FloatPoint(panVelocity(in: webView)))

        // Continue the scroll along the same axis the drag was locked to rather than reintroducing
        // diagonal drift; also keeps fastScrollTracker's velocity heuristics off-axis-clean.
        let velocity = WebCore.FloatSize(
            scrollState.directionalScrollLockTracker.filterVelocity(
                CGSize(unfilteredVelocity),
                prefersUnlocked: gesture.prefersUnlockedScroll
            )
        )

        func maximumComponentMagnitude(_ vector: WebCore.FloatSize) -> Float {
            max(abs(vector.width()), abs(vector.height()))
        }

        // Advance the swipe chain for every gesture that was fast enough to be a swipe, judged on the
        // unfiltered velocity: the user swiped even if the lock just zeroed the axis they swiped along, and
        // the tracker has to see it to consume its caughtMomentum and advance its endTime. Slower gestures
        // leave the tracker alone.
        let fastScrollMultiplier: Double =
            if maximumComponentMagnitude(unfilteredVelocity) >= Self.minimumVelocityForSwipe {
                scrollState.fastScrollTracker.update(
                    location: gesture.location(in: nil),
                    velocity: CGSize(velocity),
                    time: gesture.timestamp
                )
            } else {
                1
            }

        // Suppressing the gesture itself is judged on the filtered velocity, which can only be smaller, so
        // anything reaching this point has already been through the tracker above.
        let velocityMagnitude = maximumComponentMagnitude(velocity)
        if velocityMagnitude < Self.minimumVelocityForMomentum {
            return
        }

        sendMomentumBeganEvent(
            for: gesture,
            in: webView,
            velocity: velocity,
            fastScrollMultiplier: fastScrollMultiplier
        )
        scrollState.isMomentumActive = true

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Started momentum scrolling with velocity \(velocityMagnitude, format: .fixed(precision: 2)) pts/s
            """
        )
    }

    private func catchMomentumIfActive() {
        guard scrollState.isMomentumActive else {
            return
        }
        scrollState.isMomentumActive = false

        guard let webView = view else {
            return
        }

        scrollState.catchMomentum()

        positionInformation.invalidate()

        interruptMomentumEventGenerator()

        guard let page = webView._protectedPage().get() else {
            return
        }

        page.interruptSyntheticMomentumScrolling()
        Logger.viewGestures.log("[pageProxyID=\(self.pageLogIdentifier)] \(#function): Interrupted momentum scrolling")
    }

    private func isOnlyTouchDown(_ touch: NSTouch) -> Bool {
        guard let webView = view else {
            return false
        }

        guard let event = NSApp.currentEvent, event.type == .directTouch else {
            return false
        }

        let touches = event.touches(matching: .touching, in: webView)
        return touches.count == 1 && touches.first?._uid == touch._uid
    }

    func interruptMomentumIfNeeded(for touch: NSTouch) {
        // Other callers of resetCaughtDeceleratingScroll() can miss the end of a gesture
        // (e.g. a pan over content that disallows scrolling, which then neither scrolls, clicks, nor tracks the mouse).
        // We can't let that leak into the next interaction, or it would keep refusing gestures.
        // We only consider this a new interaction if 1) this is the only touch down,
        // and 2) isn't the touch that caught the scroll.
        if scrollState.caughtDeceleratingScroll, scrollState.uidOfTouchThatCaughtDeceleratingScroll != touch._uid, isOnlyTouchDown(touch) {
            Logger.viewGestures.log(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                Resetting stale caught decelerating scroll state from a previous interaction
                """
            )
            scrollState.resetCaughtDeceleratingScroll()
        }

        let wasMomentumActive = scrollState.isMomentumActive
        catchMomentumIfActive()

        if wasMomentumActive && scrollState.caughtDeceleratingScroll {
            scrollState.uidOfTouchThatCaughtDeceleratingScroll = touch._uid
        }
    }
}

@objc(Scrolling)
@implementation
extension WKAppKitGestureController {
    func interruptMomentumForScrollWheelEvent() {
        interruptMomentumEventGenerator()
    }

    var everMagnifiedDuringCurrentGesture: Bool { hasEverMagnifiedDuringCurrentGesture }
}

@objc(MomentumEventGeneration)
@implementation
extension WKAppKitGestureController {
    func generatedMomentumDidEnd() {
        scrollState.isMomentumActive = false

        // Only the delta suppression is tied to momentum: once it has stopped, continuing to
        // zero deltas stops top scroll stretching from engaging.
        //
        // caughtDeceleratingScroll stays set: whether the interaction caught a decelerating scroll is a
        // property of the gesture, not of the momentum, and clearing it here would let the touch that
        // interrupted the scroll click through. The gesture's end paths reset it.
        scrollState.suppressNextPanScrollDelta = false
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
