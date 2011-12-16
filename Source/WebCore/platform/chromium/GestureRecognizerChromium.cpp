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

namespace WebCore {

// FIXME: Make these configurable programmatically.
static const double maximumTouchDownDurationInSecondsForClick = 0.8;
static const double minimumTouchDownDurationInSecondsForClick = 0.01;
static const double maximumSecondsBetweenDoubleClick = 0.7;
static const int maximumTouchMoveInPixelsForClick = 20;
static const float minFlickSpeedSquared = 550.f * 550.f;

GestureRecognizerChromium::GestureRecognizerChromium()
    : m_firstTouchTime(0.0)
    , m_state(GestureRecognizerChromium::NoGesture)
    , m_lastTouchTime(0.0)
    , m_lastClickTime(0.0)
    , m_lastClickPosition()
    , m_lastTouchPosition()
    , m_lastTouchScreenPosition()
    , m_xVelocity(0.0)
    , m_yVelocity(0.0)
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

    addEdgeFunction(NoGesture, FirstFinger, Pressed, false, &GestureRecognizerChromium::touchDown);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Cancelled, false, &GestureRecognizerChromium::noGesture);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Released, false, &GestureRecognizerChromium::click);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Moved, false, &GestureRecognizerChromium::isClickOrScroll);
    addEdgeFunction(PendingSyntheticClick, FirstFinger, Stationary, false, &GestureRecognizerChromium::isClickOrScroll);
    addEdgeFunction(Scroll, FirstFinger, Moved, false, &GestureRecognizerChromium::inScroll);
    addEdgeFunction(Scroll, FirstFinger, Released, false, &GestureRecognizerChromium::scrollEnd);
    addEdgeFunction(Scroll, FirstFinger, Cancelled, false, &GestureRecognizerChromium::scrollEnd);
}

void GestureRecognizerChromium::reset()
{
    m_firstTouchTime = 0.0;
    m_state = GestureRecognizerChromium::NoGesture;
    m_lastTouchTime = 0.0;
    m_lastTouchPosition.setX(0);
    m_lastTouchPosition.setY(0);
    m_lastTouchScreenPosition.setX(0);
    m_lastTouchScreenPosition.setY(0);
    m_xVelocity = 0.0;
    m_yVelocity = 0.0;
}

GestureRecognizerChromium::~GestureRecognizerChromium()
{
}

void GestureRecognizerChromium::addEdgeFunction(State state, unsigned fingerId, PlatformTouchPoint::State touchType, bool touchHandledByJavaScript, GestureTransitionFunction transitionFunction)
{
    m_edgeFunctions.add(signature(state, fingerId, touchType, touchHandledByJavaScript), transitionFunction);
}

bool GestureRecognizerChromium::isInClickTimeWindow()
{
    double duration(m_lastTouchTime - m_firstTouchTime);
    return duration >= minimumTouchDownDurationInSecondsForClick && duration < maximumTouchDownDurationInSecondsForClick;
}

bool GestureRecognizerChromium::isInSecondClickTimeWindow()
{
    double duration(m_lastTouchTime - m_lastClickTime);
    return duration < maximumSecondsBetweenDoubleClick;
}

bool GestureRecognizerChromium::isInsideManhattanSquare(const PlatformTouchPoint& point)
{
    int manhattanDistance = abs(point.pos().x() - m_firstTouchPosition.x()) + abs(point.pos().y() - m_firstTouchPosition.y());
    return manhattanDistance < maximumTouchMoveInPixelsForClick;
}

bool GestureRecognizerChromium::isSecondClickInsideManhattanSquare(const PlatformTouchPoint& point)
{
    int manhattanDistance = abs(point.pos().x() - m_lastClickPosition.x()) + abs(point.pos().y() - m_lastClickPosition.y());
    return manhattanDistance < maximumTouchMoveInPixelsForClick;
}

bool GestureRecognizerChromium::isOverMinFlickSpeed()
{
    return (m_xVelocity * m_xVelocity + m_yVelocity * m_yVelocity) > minFlickSpeedSquared;
}

void GestureRecognizerChromium::appendTapDownGestureEvent(const PlatformTouchPoint& touchPoint, Gestures gestures)
{
    gestures->append(PlatformGestureEvent(PlatformEvent::GestureTapDown, m_firstTouchPosition, m_firstTouchScreenPosition, m_lastTouchTime, 0.f, 0.f, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
}

void GestureRecognizerChromium::appendClickGestureEvent(const PlatformTouchPoint& touchPoint, Gestures gestures)
{
    gestures->append(PlatformGestureEvent(PlatformEvent::GestureTap, m_firstTouchPosition, m_firstTouchScreenPosition, m_lastTouchTime, 0.f, 0.f, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
}

void GestureRecognizerChromium::appendDoubleClickGestureEvent(const PlatformTouchPoint& touchPoint, Gestures gestures)
{
    gestures->append(PlatformGestureEvent(PlatformEvent::GestureDoubleTap, m_firstTouchPosition, m_firstTouchScreenPosition, m_lastTouchTime, 0.f, 0.f, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
}

PlatformGestureRecognizer::PassGestures GestureRecognizerChromium::processTouchEventForGestures(const PlatformTouchEvent& event, bool defaultPrevented)
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
            ((*this).*ef)(p, gestures.get());
    }
    return gestures.release();
}

void GestureRecognizerChromium::appendScrollGestureBegin(const PlatformTouchPoint& touchPoint, Gestures gestures)
{
    gestures->append(PlatformGestureEvent(PlatformEvent::GestureScrollBegin, touchPoint.pos(), touchPoint.screenPos(), m_lastTouchTime, 0.f, 0.f, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
}

void GestureRecognizerChromium::appendScrollGestureEnd(const PlatformTouchPoint& touchPoint, Gestures gestures, float xVelocity, float yVelocity)
{
    gestures->append(PlatformGestureEvent(PlatformEvent::GestureScrollEnd, touchPoint.pos(), touchPoint.screenPos(), m_lastTouchTime, xVelocity, yVelocity, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
}

void GestureRecognizerChromium::appendScrollGestureUpdate(const PlatformTouchPoint& touchPoint, Gestures gestures)
{
    float deltaX(touchPoint.pos().x() - m_firstTouchPosition.x());
    float deltaY(touchPoint.pos().y() - m_firstTouchPosition.y());

    gestures->append(PlatformGestureEvent(PlatformEvent::GestureScrollUpdate, touchPoint.pos(), touchPoint.screenPos(), m_lastTouchTime, deltaX, deltaY, m_shiftKey, m_ctrlKey, m_altKey, m_metaKey));
    m_firstTouchPosition = touchPoint.pos();
}

void GestureRecognizerChromium::updateValues(const double touchTime, const PlatformTouchPoint& touchPoint)
{
    if (m_state != NoGesture && touchPoint.state() == PlatformTouchPoint::TouchMoved) {
        double interval(touchTime - m_lastTouchTime);
        m_xVelocity = (touchPoint.pos().x() - m_lastTouchPosition.x()) / interval;
        m_yVelocity = (touchPoint.pos().y() - m_lastTouchPosition.y()) / interval;
    }
    m_lastTouchTime = touchTime;
    m_lastTouchPosition = touchPoint.pos();
    m_lastTouchScreenPosition = touchPoint.screenPos();
    if (state() == NoGesture) {
        m_firstTouchTime = touchTime;
        m_firstTouchPosition = touchPoint.pos();
        m_firstTouchScreenPosition = touchPoint.screenPos();
        m_xVelocity = 0.0;
        m_yVelocity = 0.0;
    }
}

// Builds a signature. Signatures are assembled by joining together multiple bits.
// 1 LSB bit so that the computed signature is always greater than 0
// 3 bits for the |touchType|.
// 1 bit for |handled|
// 12 bits for |id|
// 15 bits for the |gestureState|.
unsigned int GestureRecognizerChromium::signature(State gestureState, unsigned id, PlatformTouchPoint::State touchType, bool handled)
{
    ASSERT((id & 0xfff) == id);
    return 1 + ((touchType & 0x7) << 1 | (handled ? 1 << 4 : 0) | ((id & 0xfff) << 5 ) | (gestureState << 17));
}

bool GestureRecognizerChromium::touchDown(const PlatformTouchPoint& touchPoint, Gestures gestures)
{
    appendTapDownGestureEvent(touchPoint, gestures);
    setState(PendingSyntheticClick);
    return false;
}

bool GestureRecognizerChromium::scrollEnd(const PlatformTouchPoint& point, Gestures gestures)
{
    if (isOverMinFlickSpeed() && point.state() != PlatformTouchPoint::TouchCancelled)
        appendScrollGestureEnd(point, gestures, m_xVelocity, m_yVelocity);
    else
        appendScrollGestureEnd(point, gestures, 0.f, 0.f);
    setState(NoGesture);
    reset();
    return false;
}

bool GestureRecognizerChromium::noGesture(const PlatformTouchPoint&, Gestures)
{
    reset();
    return false;
}

bool GestureRecognizerChromium::click(const PlatformTouchPoint& point, Gestures gestures)
{
    bool gestureAdded = false;
    if (isInClickTimeWindow() && isInsideManhattanSquare(point)) {
        gestureAdded = true;
        appendClickGestureEvent(point, gestures);
        if (isInSecondClickTimeWindow() && isSecondClickInsideManhattanSquare(point))
            appendDoubleClickGestureEvent(point, gestures);
        m_lastClickTime = m_lastTouchTime;
        m_lastClickPosition = m_lastTouchPosition;
    }
    reset();
    return gestureAdded;
}

bool GestureRecognizerChromium::isClickOrScroll(const PlatformTouchPoint& point, Gestures gestures)
{
    if (isInClickTimeWindow() && isInsideManhattanSquare(point)) {
        setState(GestureRecognizerChromium::PendingSyntheticClick);
        return false;
    }

    if (point.state() == PlatformTouchPoint::TouchMoved && !isInsideManhattanSquare(point)) {
        appendScrollGestureBegin(point, gestures);
        appendScrollGestureUpdate(point, gestures);
        setState(Scroll);
        return true;
    }
    return false;
}

bool GestureRecognizerChromium::inScroll(const PlatformTouchPoint& point, Gestures gestures)
{
    appendScrollGestureUpdate(point, gestures);
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

} // namespace WebCore
