/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GestureRecognizerChromium.h"

#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"

namespace WebCore {

// FIXME: Convert to methods.
static bool click(InnerGestureRecognizer*, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures);
static bool isClickOrScroll(InnerGestureRecognizer*, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures);
static bool inScroll(InnerGestureRecognizer*, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures);
static bool scrollEnd(InnerGestureRecognizer*, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures);
static bool noGesture(InnerGestureRecognizer*, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures);
static bool touchDown(InnerGestureRecognizer*, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures);

// FIXME: Make these configurable programmatically.
static const double maximumTouchDownDurationInSecondsForClick = 0.8;
static const double minimumTouchDownDurationInSecondsForClick = 0.01;
static const int maximumTouchMoveInPixelsForClick = 20;

InnerGestureRecognizer::InnerGestureRecognizer()
    : m_firstTouchTime(0.0)
    , m_state(InnerGestureRecognizer::NoGesture)
    , m_lastTouchTime(0.0)
    , m_ctrlKey(false)
    , m_altKey(false)
    , m_shiftKey(false)
    , m_metaKey(false)
{
    const unsigned FirstFinger = 0;
    const PlatformTouchPoint::State Released = PlatformTouchPoint::TouchReleased;
    const PlatformTouchPoint::State Pressed = PlatformTouchPoint::TouchPressed;
    const PlatformTouchPoint::State Moved = PlatformTouchPoint::TouchMoved;
    const PlatformTouchPoint::State Stationary = PlatformTouchPoint::TouchStationary;
    const PlatformTouchPoint::State Cancelled = PlatformTouchPoint::TouchCancelled;

    addEdgeFunction(NoGesture, FirstFinger, Pressed, false, touchDown);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Cancelled, false, noGesture);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Released, false, click);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Moved, false, isClickOrScroll);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Stationary, false, isClickOrScroll);
    addEdgeFunction(Scroll, FirstFinger, Moved, false, inScroll);
    addEdgeFunction(Scroll, FirstFinger, Released, false, scrollEnd);
    addEdgeFunction(Scroll, FirstFinger, Cancelled, false, scrollEnd);
}

void InnerGestureRecognizer::reset()
{
    m_firstTouchTime = 0.0;
    m_state = InnerGestureRecognizer::NoGesture;
    m_lastTouchTime = 0.0;
}

InnerGestureRecognizer::~InnerGestureRecognizer()
{
}

void InnerGestureRecognizer::addEdgeFunction(State state, unsigned fingerId, PlatformTouchPoint::State touchType, bool touchHandledByJavaScript, GestureTransitionFunction transitionFunction)
{
    m_edgeFunctions.add(signature(state, fingerId, touchType, touchHandledByJavaScript), transitionFunction);
}

bool InnerGestureRecognizer::isInClickTimeWindow()
{
    double duration(m_lastTouchTime - m_firstTouchTime);
    return duration >= minimumTouchDownDurationInSecondsForClick && duration < maximumTouchDownDurationInSecondsForClick;
}

bool InnerGestureRecognizer::isInsideManhattanSquare(const PlatformTouchPoint& point)
{
    int manhattanDistance = abs(point.pos().x() - m_firstTouchPosition.x()) + abs(point.pos().y() - m_firstTouchPosition.y());
    return manhattanDistance < maximumTouchMoveInPixelsForClick;
}

void InnerGestureRecognizer::appendClickGestureEvent(const PlatformTouchPoint& touchPoint, InnerGestureRecognizer::Gestures gestures)
{
    gestures->append(PlatformGestureEvent(PlatformGestureEvent::TapType, touchPoint.pos(), touchPoint.screenPos(), m_lastTouchTime, 0.0f, 0.0f, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
}

PlatformGestureRecognizer::PassGestures InnerGestureRecognizer::processTouchEventForGestures(const PlatformTouchEvent& event, bool defaultPrevented)
{
    m_ctrlKey = event.ctrlKey();
    m_altKey = event.altKey();
    m_shiftKey = event.shiftKey();
    m_metaKey = event.metaKey();

    OwnPtr<Vector<PlatformGestureEvent> > gestures = adoptPtr(new Vector<PlatformGestureEvent>());
    const Vector<PlatformTouchPoint>& points = event.touchPoints();
    for (unsigned i = 0; i < points.size(); i++) {
        const PlatformTouchPoint& p = points[i];
        updateValues(event.timestamp(), p);

        if (GestureTransitionFunction ef = m_edgeFunctions.get(signature(m_state, p.id(), p.state(), defaultPrevented)))
            (*ef)(this, p, gestures.get());
    }
    return gestures.release();
}

void InnerGestureRecognizer::appendScrollGestureBegin(const IntPoint& touchPoint, Gestures gestures)
{
    gestures->append(PlatformGestureEvent(PlatformGestureEvent::ScrollBeginType, touchPoint, touchPoint, m_lastTouchTime, 0.0f, 0.0f, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
    m_firstTouchPosition = touchPoint;
    m_lastTouchPosition = m_firstTouchPosition;
}

void InnerGestureRecognizer::appendScrollGestureEnd(const IntPoint& touchPoint, Gestures gestures)
{
    gestures->append(PlatformGestureEvent(PlatformGestureEvent::ScrollEndType, m_firstTouchPosition, m_firstTouchPosition, m_lastTouchTime, 0.0f, 0.0f, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
}

void InnerGestureRecognizer::appendScrollGestureUpdate(const IntPoint& touchPoint, Gestures gestures)
{
    float deltaX(touchPoint.x() - m_lastTouchPosition.x());
    float deltaY(touchPoint.y() - m_lastTouchPosition.y());

    gestures->append(PlatformGestureEvent(PlatformGestureEvent::ScrollUpdateType, m_firstTouchPosition, m_firstTouchPosition, m_lastTouchTime, deltaX, deltaY, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
    m_lastTouchPosition = touchPoint;
}

void InnerGestureRecognizer::updateValues(const double touchTime, const PlatformTouchPoint& touchPoint)
{
    m_lastTouchTime = touchTime;
    if (state() == NoGesture) {
        m_firstTouchTime = touchTime;
        m_firstTouchPosition = touchPoint.pos();
    }
}

// Builds a signature. Signatures are assembled by joining together multiple bits.
// 1 LSB bit so that the computed signature is always greater than 0
// 3 bits for the |touchType|.
// 1 bit for |handled|
// 12 bits for |id|
// 15 bits for the |gestureState|.
unsigned int InnerGestureRecognizer::signature(State gestureState, unsigned id, PlatformTouchPoint::State touchType, bool handled)
{
    ASSERT((id & 0xfff) == id);
    return 1 + ((touchType & 0x7) << 1 | (handled ? 1 << 4 : 0) | ((id & 0xfff) << 5 ) | (gestureState << 17));
}

static bool touchDown(InnerGestureRecognizer* gestureRecognizer, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures gestures)
{
    gestureRecognizer->setState(InnerGestureRecognizer::PendingSyntheticClick);
    return false;
}

static bool scrollEnd(InnerGestureRecognizer* gestureRecognizer, const PlatformTouchPoint& point, InnerGestureRecognizer::Gestures gestures)
{
    gestureRecognizer->appendScrollGestureEnd(point.pos(), gestures);
    gestureRecognizer->setState(InnerGestureRecognizer::NoGesture);
    return false;
}

static bool noGesture(InnerGestureRecognizer* gestureRecognizer, const PlatformTouchPoint&, InnerGestureRecognizer::Gestures gestures)
{
    gestureRecognizer->reset();
    return false;
}

static bool click(InnerGestureRecognizer* gestureRecognizer, const PlatformTouchPoint& point, InnerGestureRecognizer::Gestures gestures)
{
    if (gestureRecognizer->isInClickTimeWindow() && gestureRecognizer->isInsideManhattanSquare(point)) {
        gestureRecognizer->setState(InnerGestureRecognizer::NoGesture);
        gestureRecognizer->appendClickGestureEvent(point, gestures);
        return true;
    }
    return noGesture(gestureRecognizer, point, gestures);
}

static bool isClickOrScroll(InnerGestureRecognizer* gestureRecognizer, const PlatformTouchPoint& point, InnerGestureRecognizer::Gestures gestures)
{
    if (gestureRecognizer->isInClickTimeWindow() && gestureRecognizer->isInsideManhattanSquare(point)) {
        gestureRecognizer->setState(InnerGestureRecognizer::PendingSyntheticClick);
        return false;
    }

    if (point.state() == PlatformTouchPoint::TouchMoved && !gestureRecognizer->isInsideManhattanSquare(point)) {
        gestureRecognizer->appendScrollGestureBegin(gestureRecognizer->firstTouchPosition(), gestures);
        gestureRecognizer->appendScrollGestureUpdate(point.pos(), gestures);
        gestureRecognizer->setState(InnerGestureRecognizer::Scroll);
        return true;
    }
    return false;
}

static bool inScroll(InnerGestureRecognizer* gestureRecognizer, const PlatformTouchPoint& point, InnerGestureRecognizer::Gestures gestures)
{
    gestureRecognizer->appendScrollGestureUpdate(point.pos(), gestures);
    return true;
}

PassOwnPtr<PlatformGestureRecognizer> PlatformGestureRecognizer::create()
{
    GestureRecognizerChromium* gestureRecognizer = new GestureRecognizerChromium();
    return adoptPtr(gestureRecognizer);
}

PlatformGestureRecognizer::PlatformGestureRecognizer()
{
}

PlatformGestureRecognizer::~PlatformGestureRecognizer()
{
}

GestureRecognizerChromium::GestureRecognizerChromium()
    : m_innerGestureRecognizer()
{
}

GestureRecognizerChromium::~GestureRecognizerChromium()
{
}

PlatformGestureRecognizer::PassGestures GestureRecognizerChromium::processTouchEventForGestures(const PlatformTouchEvent& touchEvent, bool defaultPrevented)
{
    return m_innerGestureRecognizer.processTouchEventForGestures(touchEvent, defaultPrevented);
}

void GestureRecognizerChromium::reset()
{
    m_innerGestureRecognizer.reset();
}

} // namespace WebCore
