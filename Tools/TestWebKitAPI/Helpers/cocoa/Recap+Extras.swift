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

extension RCPEventStreamComposer {
    /// Taps at `point`, keeping the touch down for `duration` the way a real finger would.
    ///
    /// - Parameters:
    ///   - point: Where the finger goes down.
    ///   - duration: How long the finger stays down before lifting.
    public func humanTap(at point: CGPoint, for duration: Duration) {
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

    /// Pinches open, pushing each finger `spread` points outward along the line between them.
    ///
    /// The touches stay down when this returns.
    ///
    /// - Parameters:
    ///   - start: Where the first finger goes down.
    ///   - end: Where the second finger goes down.
    ///   - spread: How far each finger moves outward, in points.
    ///   - duration: How long the spread takes. The optional translation takes the same amount of time,
    ///     and the optional preceding drag takes a third of it.
    ///   - dragOffset: If set, the first finger goes down alone and drags by this offset before
    ///     the second finger lands, at `end` shifted by the same offset.
    ///   - translation: If set, both fingers then move together by this offset.
    public func pinchOpen(
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

    /// Drags one finger from `start` to `end` over `duration`.
    ///
    /// - Parameters:
    ///   - start: Where the finger goes down.
    ///   - end: Where the finger stops moving.
    ///   - duration: How long the move from `start` to `end` takes.
    ///   - tapAndWait: How long the finger stays at `start` before it starts moving.
    ///   - lift: Whether the finger lifts at `end`.
    public func drag(
        withStart start: CGPoint,
        end: CGPoint,
        duration: Duration,
        tapAndWait: Duration = .zero,
        lift: Bool = true
    ) {
        drag(
            withStart: start,
            end: end,
            duration: duration / .seconds(1),
            tapAndWait: tapAndWait / .seconds(1),
            lift: lift
        )
    }

    /// Like ``drag(withStart:end:duration:tapAndWait:lift:)``, but first moves to `early` right after the
    /// touch begins, so that the touch moves before it settles.
    ///
    /// - Parameters:
    ///   - start: Where the finger goes down.
    ///   - end: Where the finger lifts.
    ///   - duration: How long the move from `early` to `end` takes.
    ///   - tapAndWait: How long the finger stays at `early` before moving on to `end`.
    ///   - early: Where the finger moves right after going down.
    public func drag(
        withStart start: CGPoint,
        end: CGPoint,
        duration: Duration,
        tapAndWait: Duration,
        movingFirstTo early: CGPoint
    ) {
        var startPoints = [start]
        unsafe touchDown(atPoints: &startPoints, touchCount: 1)

        var earlyPoints = [early]
        unsafe move(toPoints: &earlyPoints, touchCount: 1, duration: 0.03)

        advanceTime(tapAndWait / .seconds(1))

        var endPoints = [end]
        unsafe move(toPoints: &endPoints, touchCount: 1, duration: duration / .seconds(1))

        liftUpAtAllActivePoints()
    }

    /// Flicks from `startLocation` to `endLocation` over `duration`.
    ///
    /// - Parameters:
    ///   - startLocation: Where the flick begins.
    ///   - endLocation: Where the flick ends.
    ///   - duration: How long the flick takes.
    ///   - multiFinger: If `true`, uses a two-finger drag instead of a one-finger flick.
    public func flick(
        withStart startLocation: CGPoint,
        end endLocation: CGPoint,
        duration: Duration,
        multiFinger: Bool = false
    ) {
        if !multiFinger {
            sendFlick(withStart: startLocation, end: endLocation, duration: duration / .seconds(1))
        } else {
            var pointArray = [startLocation, endLocation]

            unsafe multifingerDrag(
                withPointArray: &pointArray,
                numPoints: UInt(pointArray.count),
                duration: duration / .seconds(1),
                numFingers: 2
            )
        }
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
