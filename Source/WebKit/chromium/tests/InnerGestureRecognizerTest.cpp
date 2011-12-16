/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include <gtest/gtest.h>
#include <stdarg.h>

using namespace WebCore;

class InspectableGestureRecognizerChromium : public WebCore::GestureRecognizerChromium {
public:
    InspectableGestureRecognizerChromium()
        : WebCore::GestureRecognizerChromium()
    {
    }

    int signature(State gestureState, unsigned id, PlatformTouchPoint::State touchType, bool handled)
    {
        return GestureRecognizerChromium::signature(gestureState, id, touchType, handled);
    };

    IntPoint firstTouchPosition() { return m_firstTouchPosition; };

    void setFirstTouchTime(double t) { m_firstTouchTime = t; };
    double firstTouchTime() { return m_firstTouchTime; };

    void setLastTouchTime(double t) { m_lastTouchTime = t; };
    double lastTouchTime() { return m_lastTouchTime; };

    GestureRecognizerChromium::GestureTransitionFunction edgeFunction(int hash)
    {
        return m_edgeFunctions.get(hash);
    };

    virtual void updateValues(double d, const PlatformTouchPoint &p)
    {
        GestureRecognizerChromium::updateValues(d, p);
    };

    void addEdgeFunction(State state, unsigned finger, PlatformTouchPoint::State touchState, bool touchHandledByJavaScript, GestureTransitionFunction function)
    {
        GestureRecognizerChromium::addEdgeFunction(state, finger, touchState, touchHandledByJavaScript, function);
    };

    bool stubEdgeFunction(const PlatformTouchPoint&, GestureRecognizerChromium::Gestures*);

    void setStateTest(State value)
    {
        GestureRecognizerChromium::setState(value);
    };

    bool isInsideManhattanSquare(const PlatformTouchPoint& touchPoint)
    {
        return GestureRecognizerChromium::isInsideManhattanSquare(touchPoint);
    };

    bool isInClickTimeWindow()
    {
        return GestureRecognizerChromium::isInClickTimeWindow();
    };

};

bool InspectableGestureRecognizerChromium::stubEdgeFunction(const PlatformTouchPoint&, GestureRecognizerChromium::Gestures*)
{
    return false;
}

class BuildablePlatformTouchPoint : public WebCore::PlatformTouchPoint {
public:
    BuildablePlatformTouchPoint();
    BuildablePlatformTouchPoint(int x, int y);
    BuildablePlatformTouchPoint(int x, int y, PlatformTouchPoint::State);

    void setX(int x)
    {
        m_pos.setX(x);
        m_screenPos.setX(x);
    };

    void setY(int y)
    {
        m_pos.setY(y);
        m_screenPos.setY(y);
    };
};

BuildablePlatformTouchPoint::BuildablePlatformTouchPoint()
{
    m_id = 0;
    m_state = PlatformTouchPoint::TouchStationary;
    m_pos = IntPoint::zero();
    m_screenPos = IntPoint::zero();
}

BuildablePlatformTouchPoint::BuildablePlatformTouchPoint(int x, int y)
{
    m_id = 0;
    m_state = PlatformTouchPoint::TouchStationary;
    m_pos = IntPoint(x, y);
    m_screenPos = IntPoint(x, y);
}

BuildablePlatformTouchPoint::BuildablePlatformTouchPoint(int x, int y, PlatformTouchPoint::State state)
{
    m_id = 0;
    m_state = state;
    m_pos = IntPoint(x, y);
    m_screenPos = IntPoint(x, y);
}

class BuildablePlatformTouchEvent : public WebCore::PlatformTouchEvent {
public:
    BuildablePlatformTouchEvent(WebCore::TouchEventType type, PlatformTouchPoint& point, double time)
    {
        m_type = type;
        m_touchPoints.append(point);
        m_timestamp = time;
    }

    BuildablePlatformTouchEvent(WebCore::TouchEventType type, PlatformTouchPoint& point)
    {
        m_type = type;
        m_touchPoints.append(point);
        m_timestamp = 100.;
    }
};

class GestureRecognizerTest : public testing::Test {
public:
    GestureRecognizerTest() { }

protected:
    virtual void SetUp() { }
    virtual void TearDown() { }
};

void SimulateAndTestFirstClick(InspectableGestureRecognizerChromium& gm)
{
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000.);
    OwnPtr<Vector<WebCore::PlatformGestureEvent> > gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ((unsigned int)1, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint release(10, 16, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000. + 0.7);
    OwnPtr<Vector<WebCore::PlatformGestureEvent> > gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    ASSERT_EQ((unsigned int)1, gestureEnd->size());
    ASSERT_EQ(PlatformEvent::GestureTap, (*gestureEnd)[0].type());
}

typedef OwnPtr<Vector<WebCore::PlatformGestureEvent> > Gestures;

TEST_F(GestureRecognizerTest, hash)
{
    InspectableGestureRecognizerChromium testGm;
    const unsigned FirstFinger = 0;
    const unsigned SecondFinger = 1;

    ASSERT_EQ(1 + 0, testGm.signature(GestureRecognizerChromium::NoGesture, FirstFinger, PlatformTouchPoint::TouchReleased, false));

    ASSERT_EQ(1 + ((8 | 1) << 1), testGm.signature(GestureRecognizerChromium::NoGesture, FirstFinger, PlatformTouchPoint::TouchPressed, true));

    ASSERT_EQ(1 + ((0x10000 | 2) << 1), testGm.signature(GestureRecognizerChromium::PendingSyntheticClick, FirstFinger, PlatformTouchPoint::TouchMoved, false));

    ASSERT_EQ(1 + (0x20000 << 1), testGm.signature(GestureRecognizerChromium::Scroll, FirstFinger, PlatformTouchPoint::TouchReleased, false));

    ASSERT_EQ(1 + ((0x20000 | 1) << 1), testGm.signature(GestureRecognizerChromium::Scroll, FirstFinger, PlatformTouchPoint::TouchPressed, false));

    ASSERT_EQ(1 + ((0x20000 | 0x10 | 8 | 1) << 1), testGm.signature(GestureRecognizerChromium::Scroll, SecondFinger, PlatformTouchPoint::TouchPressed, true));
}

TEST_F(GestureRecognizerTest, state)
{
    InspectableGestureRecognizerChromium testGm;

    ASSERT_EQ(0, testGm.state());
    testGm.setStateTest(GestureRecognizerChromium::PendingSyntheticClick);
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, testGm.state());
}

TEST_F(GestureRecognizerTest, isInsideManhattanSquare)
{
    InspectableGestureRecognizerChromium gm;
    BuildablePlatformTouchPoint p;

    ASSERT_EQ(0.0, gm.firstTouchPosition().x());
    ASSERT_EQ(0.0, gm.firstTouchPosition().y());

    p.setX(0.0);
    p.setY(19.999);
    ASSERT_TRUE(gm.isInsideManhattanSquare(p));

    p.setX(19.999);
    p.setY(0.0);
    ASSERT_TRUE(gm.isInsideManhattanSquare(p));

    p.setX(20.0);
    p.setY(0.0);
    ASSERT_FALSE(gm.isInsideManhattanSquare(p));

    p.setX(0.0);
    p.setY(20.0);
    ASSERT_FALSE(gm.isInsideManhattanSquare(p));

    p.setX(-20.0);
    p.setY(0.0);
    ASSERT_FALSE(gm.isInsideManhattanSquare(p));

    p.setX(0.0);
    p.setY(-20.0);
    ASSERT_FALSE(gm.isInsideManhattanSquare(p));
}

TEST_F(GestureRecognizerTest, isInClickTimeWindow)
{
    InspectableGestureRecognizerChromium gm;

    gm.setFirstTouchTime(0.0);
    gm.setLastTouchTime(0.0);
    ASSERT_FALSE(gm.isInClickTimeWindow());

    gm.setFirstTouchTime(0.0);
    gm.setLastTouchTime(0.010001);
    ASSERT_TRUE(gm.isInClickTimeWindow());

    gm.setFirstTouchTime(0.0);
    gm.setLastTouchTime(0.8 - .00000001);
    ASSERT_TRUE(gm.isInClickTimeWindow());

    gm.setFirstTouchTime(0.0);
    gm.setLastTouchTime(0.80001);
    ASSERT_FALSE(gm.isInClickTimeWindow());
}

TEST_F(GestureRecognizerTest, addEdgeFunction)
{
    InspectableGestureRecognizerChromium gm;
    gm.addEdgeFunction(GestureRecognizerChromium::Scroll, 0, PlatformTouchPoint::TouchReleased, true, reinterpret_cast<GestureRecognizerChromium::GestureTransitionFunction>(&InspectableGestureRecognizerChromium::stubEdgeFunction));

    ASSERT_EQ(reinterpret_cast<GestureRecognizerChromium::GestureTransitionFunction>(&InspectableGestureRecognizerChromium::stubEdgeFunction), gm.edgeFunction(gm.signature(GestureRecognizerChromium::Scroll, 0, PlatformTouchPoint::TouchReleased, true)));
}

TEST_F(GestureRecognizerTest, updateValues)
{
    InspectableGestureRecognizerChromium gm;

    ASSERT_EQ(0.0, gm.firstTouchTime());
    ASSERT_EQ(0.0, gm.lastTouchTime());
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint p1(10, 11);
    gm.updateValues(1.1, p1);

    ASSERT_EQ(10, gm.firstTouchPosition().x());
    ASSERT_EQ(11, gm.firstTouchPosition().y());
    ASSERT_EQ(1.1, gm.firstTouchTime());
    ASSERT_EQ(0.0, gm.lastTouchTime() - gm.firstTouchTime());

    BuildablePlatformTouchPoint p2(13, 14);
    gm.setStateTest(GestureRecognizerChromium::PendingSyntheticClick);
    gm.updateValues(2.0, p2);

    ASSERT_EQ(10, gm.firstTouchPosition().x());
    ASSERT_EQ(11, gm.firstTouchPosition().y());
    ASSERT_EQ(1.1, gm.firstTouchTime());
    ASSERT_EQ(2.0 - 1.1, gm.lastTouchTime() - gm.firstTouchTime());

    BuildablePlatformTouchPoint p3(23, 34);
    gm.setStateTest(GestureRecognizerChromium::NoGesture);
    gm.updateValues(3.0, p3);

    ASSERT_EQ(23, gm.firstTouchPosition().x());
    ASSERT_EQ(34, gm.firstTouchPosition().y());
    ASSERT_EQ(3.0, gm.firstTouchTime());
    ASSERT_EQ(0.0, gm.lastTouchTime() - gm.firstTouchTime());
}

#if OS(WINDOWS)
#define MAYBE_doubleTapGestureTest DISABLED_doubleTapGestureTest
#else
#define MAYBE_doubleTapGestureTest doubleTapGestureTest
#endif

TEST_F(GestureRecognizerTest, DISABLED_doubleTapGestureTest)
{
    InspectableGestureRecognizerChromium gm;
    SimulateAndTestFirstClick(gm);
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000 + .5);
    Gestures gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ(1u, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 16, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move, 1000 + .5 + .01);
    Gestures gestureMove(gm.processTouchEventForGestures(moveEvent, false));
    ASSERT_EQ(0u, gestureMove->size());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint release(10, 16, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000 + .5 + .02);
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    ASSERT_EQ(2u, gestureEnd->size());
    ASSERT_EQ(PlatformEvent::GestureTap, (*gestureEnd)[0].type());
    ASSERT_EQ(PlatformEvent::GestureDoubleTap, (*gestureEnd)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}

TEST_F(GestureRecognizerTest, doubleTapGestureIncompleteTest)
{
    InspectableGestureRecognizerChromium gm;
    SimulateAndTestFirstClick(gm);
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000. + 0.7 + 0.01);
    Gestures gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ(1u, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 50, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move, 1000. + 0.7 + 0.02);
    Gestures gestureMove(gm.processTouchEventForGestures(moveEvent, false));
    ASSERT_EQ(2u, gestureMove->size());
    ASSERT_EQ(PlatformEvent::GestureScrollBegin, (*gestureMove)[0].type());
    ASSERT_EQ(PlatformEvent::GestureScrollUpdate, (*gestureMove)[1].type());
    ASSERT_EQ(GestureRecognizerChromium::Scroll, gm.state());

    BuildablePlatformTouchPoint release(10, 50, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000. + 0.7 + 0.03);
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    ASSERT_EQ(1u, gestureEnd->size());
    ASSERT_EQ(PlatformEvent::GestureScrollEnd, (*gestureEnd)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}

TEST_F(GestureRecognizerTest, doubleTapGestureIncompleteDueToSecondClickPositionTest)
{
    InspectableGestureRecognizerChromium gm;
    SimulateAndTestFirstClick(gm);
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    IntPoint awayFromFirstClick(24, 26);

    BuildablePlatformTouchPoint press(awayFromFirstClick.x(), awayFromFirstClick.y(), PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000. + 0.7 + 0.01);
    Gestures gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ(1u, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint release(awayFromFirstClick.x(), awayFromFirstClick.y(), PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000. + 0.7 + 0.025);
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    ASSERT_EQ(1u, gestureEnd->size());
    ASSERT_EQ(PlatformEvent::GestureTap, (*gestureEnd)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}

TEST_F(GestureRecognizerTest, tapDownWithoutTapGestureTest)
{
    InspectableGestureRecognizerChromium gm;
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press);
    Gestures gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ(1u, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 50, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move);
    Gestures gestureMove(gm.processTouchEventForGestures(moveEvent, false));
    for (unsigned int i = 0; i < gestureMove->size(); i++)
        ASSERT_NE(PlatformEvent::GestureTap, (*gestureMove)[i].type());
    ASSERT_EQ(GestureRecognizerChromium::Scroll, gm.state());

    BuildablePlatformTouchPoint release(10, 50, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release);
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    for (unsigned int i = 0; i < gestureEnd->size(); i++)
        ASSERT_NE(PlatformEvent::GestureTap, (*gestureEnd)[i].type());
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}

TEST_F(GestureRecognizerTest, tapDownWithTapGestureTest)
{
    InspectableGestureRecognizerChromium gm;
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000.);
    Gestures gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ(1u, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 16, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move, 1000.);
    Gestures gestureMove(gm.processTouchEventForGestures(moveEvent, false));
    ASSERT_EQ(0u, gestureMove->size());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint release(10, 16, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000. + 0.011);
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    ASSERT_EQ(1u, gestureEnd->size());
    ASSERT_EQ(PlatformEvent::GestureTap, (*gestureEnd)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}


TEST_F(GestureRecognizerTest, noDoubleTapGestureBecauseOfInterTouchIntervalTest)
{
    InspectableGestureRecognizerChromium gm;
    SimulateAndTestFirstClick(gm);
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    // Values are from GestureRecognizerChromium.cpp
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000 + .8 + 10 + .1);
    Gestures gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ(1u, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 16, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move, 1000 + .8 + 10 + .1 + .05);
    Gestures gestureMove(gm.processTouchEventForGestures(moveEvent, false));
    ASSERT_EQ(0u, gestureMove->size());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint release(11, 17, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000 + .8 + 10 + .1 + .02);
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    ASSERT_EQ(1u, gestureEnd->size());
    ASSERT_EQ(PlatformEvent::GestureTap, (*gestureEnd)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}



TEST_F(GestureRecognizerTest, gestureScrollEvents)
{
    InspectableGestureRecognizerChromium gm;

    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000.);
    gm.processTouchEventForGestures(pressEvent, false);

    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 50, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move, 1000. + 0.2);
    Gestures gestureStart(gm.processTouchEventForGestures(moveEvent, false));
    bool scrollStarted = false, scrollUpdated = false;
    for (unsigned int i = 0; i < gestureStart->size(); i++) {
        switch ((*gestureStart)[i].type()) {
        case PlatformEvent::GestureScrollBegin:
            scrollStarted = true;
            break;
        case PlatformEvent::GestureScrollUpdate:
            scrollUpdated = true;
            break;
        default:
            ASSERT_TRUE(false);
        }
    }

    ASSERT_TRUE(scrollStarted);
    ASSERT_TRUE(scrollUpdated);
    ASSERT_EQ(GestureRecognizerChromium::Scroll, gm.state());

    BuildablePlatformTouchPoint release(10, 50, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000. + 0.2 + 0.2);
    bool scrollEnd = false;
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    for (unsigned int i = 0; i < gestureEnd->size(); i++) {
        switch ((*gestureEnd)[i].type()) {
        case PlatformEvent::GestureScrollEnd:
            scrollEnd = true;
            ASSERT_EQ((*gestureEnd)[i].deltaX(), 0);
            ASSERT_EQ((*gestureEnd)[i].deltaY(), 0);
            break;
        default:
            ASSERT_TRUE(false);
        }
    }
    ASSERT_TRUE(scrollEnd);
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}

TEST_F(GestureRecognizerTest, flickGestureTest)
{
    InspectableGestureRecognizerChromium gm;
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press, 1000.);
    Gestures gestureStart(gm.processTouchEventForGestures(pressEvent, false));
    ASSERT_EQ((unsigned int)1, gestureStart->size());
    ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type());
    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 50, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move, 1000. + 0.02);
    Gestures gestureMove(gm.processTouchEventForGestures(moveEvent, false));
    bool scrollStarted = false, scrollUpdated = false;
    for (unsigned int i = 0; i < gestureMove->size(); i++) {
        switch ((*gestureMove)[i].type()) {
        case PlatformEvent::GestureScrollBegin:
            scrollStarted = true;
            break;
        case PlatformEvent::GestureScrollUpdate:
            scrollUpdated = true;
            break;
        default:
            ASSERT_TRUE(false);
        }
    }

    ASSERT_TRUE(scrollStarted);
    ASSERT_TRUE(scrollUpdated);
    ASSERT_EQ(GestureRecognizerChromium::Scroll, gm.state());

    BuildablePlatformTouchPoint release(10, 50, PlatformTouchPoint::TouchReleased);
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release, 1000. + 0.06);
    Gestures gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    ASSERT_EQ((unsigned int) 1, gestureEnd->size());
    ASSERT_EQ(PlatformEvent::GestureScrollEnd, (*gestureEnd)[0].type());
    ASSERT_EQ((*gestureEnd)[0].deltaX(), 0);
    ASSERT_EQ((*gestureEnd)[0].deltaY(), 1750.);
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}

struct TouchPointAndEvent {
public:
    TouchPointAndEvent(int x, int y, double timestamp, PlatformTouchPoint::State state, WebCore::TouchEventType type)
        : m_point(x, y, state)
        , m_event(type, m_point, timestamp)
    { }
    BuildablePlatformTouchPoint m_point;
    BuildablePlatformTouchEvent m_event;
};

class TouchSequence {
public:
    TouchSequence(int n, ...) : m_n(n)
    {
        va_list args;
        va_start(args, n);
        ASSERT(n > 0);
        m_data = new TouchPointAndEvent*[n];
        for (int i = 0; i < n; ++i)
            m_data[i] = va_arg(args, TouchPointAndEvent*);
        va_end(args);
    }
    ~TouchSequence()
    {
        for (int i = 0; i < m_n; ++i)
            delete m_data[i];
        delete[] m_data;
    }
    int m_n;
    TouchPointAndEvent** m_data;
};

const int numberOfFlickSamples = 11;
TouchSequence sampleFlickSequence[numberOfFlickSamples] =
{
    TouchSequence(8,
        new TouchPointAndEvent(256, 348, 1308336245.407, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(254, 345, 1308336245.470, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(252, 336, 1308336245.488, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(242, 261, 1308336245.505, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(242, 179, 1308336245.521, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(255, 100, 1308336245.533, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(262, 74, 1308336245.549, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(262, 74, 1308336245.566, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(8,
        new TouchPointAndEvent(178, 339, 1308336266.180, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(177, 335, 1308336266.212, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(172, 314, 1308336266.226, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(160, 248, 1308336266.240, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(156, 198, 1308336266.251, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(166, 99, 1308336266.266, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(179, 41, 1308336266.280, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(179, 41, 1308336266.291, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(7,
        new TouchPointAndEvent(238, 386, 1308336272.068, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(237, 383, 1308336272.121, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(236, 374, 1308336272.138, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(223, 264, 1308336272.155, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(231, 166, 1308336272.173, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(243, 107, 1308336272.190, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(243, 107, 1308336272.202, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(10,
        new TouchPointAndEvent(334, 351, 1308336313.581, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(334, 348, 1308336313.694, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(335, 346, 1308336313.714, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(334, 343, 1308336313.727, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(332, 336, 1308336313.738, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(328, 316, 1308336313.753, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(317, 277, 1308336313.770, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(306, 243, 1308336313.784, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(292, 192, 1308336313.799, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(292, 192, 1308336313.815, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(14,
        new TouchPointAndEvent(92, 112, 1308336323.955, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(92, 115, 1308336324.056, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(91, 116, 1308336324.066, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(91, 117, 1308336324.074, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(90, 122, 1308336324.089, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(90, 129, 1308336324.102, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(89, 147, 1308336324.120, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(89, 163, 1308336324.135, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(89, 188, 1308336324.151, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(89, 213, 1308336324.169, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(89, 252, 1308336324.189, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(90, 283, 1308336324.204, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(91, 308, 1308336324.218, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(91, 308, 1308336324.230, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(5,
        new TouchPointAndEvent(55, 249, 1308336349.093, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(59, 249, 1308336349.179, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(66, 248, 1308336349.191, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(128, 253, 1308336349.208, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(128, 253, 1308336349.258, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(9,
        new TouchPointAndEvent(376, 290, 1308336353.071, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(373, 288, 1308336353.127, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(372, 287, 1308336353.140, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(353, 280, 1308336353.156, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(319, 271, 1308336353.171, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(264, 258, 1308336353.188, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(215, 251, 1308336353.200, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(151, 246, 1308336353.217, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(151, 246, 1308336353.231, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(5,
        new TouchPointAndEvent(60, 166, 1308336358.898, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(63, 166, 1308336358.944, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(68, 167, 1308336358.958, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(118, 179, 1308336358.971, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(118, 179, 1308336358.984, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(5,
        new TouchPointAndEvent(66, 318, 1308336362.996, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(70, 316, 1308336363.046, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(77, 314, 1308336363.058, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(179, 295, 1308336363.082, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(179, 295, 1308336363.096, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(11,
        new TouchPointAndEvent(345, 333, 1308336366.618, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(344, 330, 1308336366.664, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(343, 329, 1308336366.681, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(339, 324, 1308336366.694, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(332, 317, 1308336366.709, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(312, 300, 1308336366.728, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(279, 275, 1308336366.741, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(246, 251, 1308336366.752, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(198, 219, 1308336366.769, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(155, 196, 1308336366.783, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(155, 196, 1308336366.794, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    ),
    TouchSequence(7,
        new TouchPointAndEvent(333, 360, 1308336369.547, PlatformTouchPoint::TouchPressed, WebCore::TouchStart),
        new TouchPointAndEvent(332, 357, 1308336369.596, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(331, 353, 1308336369.661, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(326, 345, 1308336369.713, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(310, 323, 1308336369.748, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(250, 272, 1308336369.801, PlatformTouchPoint::TouchMoved, WebCore::TouchMove),
        new TouchPointAndEvent(250, 272, 1308336369.840, PlatformTouchPoint::TouchReleased, WebCore::TouchEnd)
    )
};

TEST_F(GestureRecognizerTest, sampleFlickSequenceGestureTest)
{
    InspectableGestureRecognizerChromium gm;
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    for (int i = 0; i < numberOfFlickSamples; ++i) {
        std::ostringstream failureMessageBuilder;
        failureMessageBuilder << "Failed on sample sequence " << i;
        std::string failureMessage = failureMessageBuilder.str();

        // There should be at least 3 events (TouchStart, TouchMove, TouchEnd) in every sequence
        ASSERT_GT(sampleFlickSequence[i].m_n, 3) << failureMessage;

        // First event (TouchStart) should produce a TouchDown gesture
        Gestures gestureStart(gm.processTouchEventForGestures(sampleFlickSequence[i].m_data[0]->m_event, false));
        ASSERT_EQ((unsigned int)1, gestureStart->size()) << failureMessage;
        ASSERT_EQ(PlatformEvent::GestureTapDown, (*gestureStart)[0].type()) << failureMessage;
        ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state()) << failureMessage;

        // Then we have a bunch of TouchMove events
        for (int j = 1; j < sampleFlickSequence[i].m_n - 1; ++j)
            gm.processTouchEventForGestures(sampleFlickSequence[i].m_data[j]->m_event, false);

        // Last event (TouchEnd) should generate a Flick gesture
        Gestures gestureEnd(gm.processTouchEventForGestures(sampleFlickSequence[i].m_data[sampleFlickSequence[i].m_n - 1]->m_event, false));
        ASSERT_EQ((unsigned int) 1, gestureEnd->size()) << failureMessage;
        ASSERT_EQ(PlatformEvent::GestureScrollEnd, (*gestureEnd)[0].type()) << failureMessage;
        double xVelocity = (*gestureEnd)[0].deltaX();
        double yVelocity = (*gestureEnd)[0].deltaY();
        double velocity = sqrt(xVelocity * xVelocity + yVelocity * yVelocity);
        ASSERT_GT(velocity, 550) << failureMessage;
        ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state()) << failureMessage;
    }
}
