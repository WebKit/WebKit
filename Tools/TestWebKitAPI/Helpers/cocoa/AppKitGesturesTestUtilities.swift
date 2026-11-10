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

public import Recap

private import AppKit_Private.NSScrollView_Private
private import CoreGraphics

extension RCPEventStreamComposer {
    public func _wk_click(at point: CGPoint, for duration: Duration) {
        // This intentionally uses `drag` instead of `tap` so that the overall event lasts for `duration`
        // instead of being instantaneous, matching real-world behavior.
        //
        // This has the exact same effect as `recap -c sender touchscreen t <point> <duration>`.

        drag(
            withStart: point,
            end: point,
            duration: duration / .seconds(1)
        )
    }

    public func _wk_magnify(
        withStart start: CGPoint,
        end: CGPoint,
        spreadBy spread: CGFloat,
        duration: Duration,
        precededByDragOf dragOffset: CGSize? = nil,
        followedByTranslationOf translation: CGSize? = nil
    ) {
        let delta = CGSize(width: end.x - start.x, height: end.y - start.y)
        let length = max((delta.width * delta.width + delta.height * delta.height).squareRoot(), 1)
        let unit = CGSize(width: delta.width / length, height: delta.height / length)

        var first = start
        var second = end

        if let dragOffset {
            first = CGPoint(x: first.x + dragOffset.width, y: first.y + dragOffset.height)
            second = CGPoint(x: second.x + dragOffset.width, y: second.y + dragOffset.height)

            var origin = [start]

            unsafe touchDown(atPoints: &origin, touchCount: 1)
            advanceTime(0.1)

            var dragged = [first]
            unsafe move(toPoints: &dragged, touchCount: 1, duration: duration / .seconds(1) / 3)
            advanceTime(0.1)

            var arriving = [second]
            unsafe touchDown(atPoints: &arriving, touchCount: 1)
        } else {
            var both = [first, second]
            unsafe touchDown(atPoints: &both, touchCount: 2)
        }

        advanceTime(0.1)

        var spreadPoints = [
            CGPoint(x: first.x - unit.width * spread, y: first.y - unit.height * spread),
            CGPoint(x: second.x + unit.width * spread, y: second.y + unit.height * spread),
        ]
        unsafe move(toPoints: &spreadPoints, touchCount: 2, duration: duration / .seconds(1))

        guard let translation else {
            return
        }

        advanceTime(0.3)

        var translated = spreadPoints.map {
            CGPoint(x: $0.x + translation.width, y: $0.y + translation.height)
        }
        unsafe move(toPoints: &translated, touchCount: 2, duration: duration / .seconds(1))
    }

    public func _wk_mouseUp() {
        liftUpAtAllActivePoints()
    }

    public func _wk_drag(
        withStart start: CGPoint,
        end: CGPoint,
        duration: Duration,
        pressAndWait: Duration = .zero,
        release: Bool = true
    ) {
        drag(
            withStart: start,
            end: end,
            duration: duration / .seconds(1),
            tapAndWait: pressAndWait / .seconds(1),
            lift: release
        )
    }

    /// Like `_wk_drag(withStart:end:duration:pressAndWait:)`, but first moves to `early` right after the
    /// click begins, so that the click moves before it settles.
    public func _wk_drag(
        withStart start: CGPoint,
        end: CGPoint,
        duration: Duration,
        pressAndWait: Duration,
        movingFirstTo early: CGPoint
    ) {
        var startPoints = [start]
        unsafe touchDown(atPoints: &startPoints, touchCount: 1)

        var earlyPoints = [early]
        unsafe move(toPoints: &earlyPoints, touchCount: 1, duration: 0.03)

        advanceTime(pressAndWait / .seconds(1))

        var endPoints = [end]
        unsafe move(toPoints: &endPoints, touchCount: 1, duration: duration / .seconds(1))

        liftUpAtAllActivePoints()
    }

    public var _wk_eventFrequency: Int {
        get { touchFrequency }
        set { touchFrequency = newValue }
    }

    public func _wk_scroll(
        withStart startLocation: CGPoint,
        end endLocation: CGPoint,
        duration: Duration,
        multiFinger: Bool = false
    ) {
        if !multiFinger {
            sendFlick(withStart: startLocation, end: endLocation, duration: duration / .seconds(1))
        } else {
            var pointArray = [startLocation, endLocation]

            // Safety: This is safe because Recap accesses pointArray[0, numPoints), and
            // we always call multifingerDrag in a way that numPoints == pointArray.count.
            unsafe multifingerDrag(
                withPointArray: &pointArray,
                numPoints: UInt(pointArray.count),
                duration: duration / .seconds(1),
                numFingers: 2
            )
        }
    }
}

extension RCPEventSenderProperties {
    public static func _wk_trackpadSender() -> RCPEventSenderProperties {
        let displayUUID = unsafe CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID())?.takeRetainedValue()
        return .touchScreenDigitizerSender(forDisplayUUID: displayUUID.map { CFUUIDCreateString(nil, $0) as String })
    }
}

public func styleAdjustmentForCustomWidgetScript() -> Swift.String {
    """
    const touchActionValue = interactive ? "manipulation" : "none";
    const element = document.getElementById(elementID);
    element.style.touchAction = touchActionValue;
    """
}

public func styleAdjustmentForManipulationSurfaceScript() -> Swift.String {
    """
    document.getElementById(elementID).style.touchAction = styleValue;
    """
}

public func loadManipulationSurfaceScript() -> Swift.String {
    """
    return window.loadSurface(`touch-action: ${styleValue}`);
    """
}

public let trackpadRubberbandHyperbolicCoefficient: CGFloat = NSScrollingBehaviorHyperbolicCoefficientTrackpad

public let automationHyperbolicCoefficient: CGFloat = NSScrollingBehaviorHyperbolicCoefficientTouch

public let verticalDragOverManipulationSurfaceArguments = [
    ("none", true),
    ("pan-x pan-y", true),
    ("auto", false),
    ("pan-y", false),
    ("pan-x pan-y pinch-zoom", false),
]

public let pressDragOverStyledRangeInputStyleValues = [
    "pan-y",
    "pan-y pinch-zoom",
]

public let nonManipulableSurfaceStyle = "touch-action: none;"

public let transparentClickHighlightStyle = "-webkit-tap-highlight-color: transparent;"

#endif // HAVE_APPKIT_GESTURES_SUPPORT
