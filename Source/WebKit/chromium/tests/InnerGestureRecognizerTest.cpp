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
};

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
};

BuildablePlatformTouchPoint::BuildablePlatformTouchPoint(int x, int y)
{
    m_id = 0;
    m_state = PlatformTouchPoint::TouchStationary;
    m_pos = IntPoint(x, y);
    m_screenPos = IntPoint(x, y);
};

BuildablePlatformTouchPoint::BuildablePlatformTouchPoint(int x, int y, PlatformTouchPoint::State state)
{
    m_id = 0;
    m_state = state;
    m_pos = IntPoint(x, y);
    m_screenPos = IntPoint(x, y);
}

class BuildablePlatformTouchEvent : public WebCore::PlatformTouchEvent {
public:
    BuildablePlatformTouchEvent(WebCore::TouchEventType type, PlatformTouchPoint& point)
    {
        m_type = type;
        m_touchPoints.append(point);
    }
};

class GestureRecognizerTest : public testing::Test {
public:
    GestureRecognizerTest() { }

protected:
    virtual void SetUp() { }
    virtual void TearDown() { }
};

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
    gm.addEdgeFunction(GestureRecognizerChromium::Scroll, 0, PlatformTouchPoint::TouchReleased, true, (GestureRecognizerChromium::GestureTransitionFunction)&InspectableGestureRecognizerChromium::stubEdgeFunction);

    ASSERT_EQ((GestureRecognizerChromium::GestureTransitionFunction)&InspectableGestureRecognizerChromium::stubEdgeFunction, gm.edgeFunction(gm.signature(GestureRecognizerChromium::Scroll, 0, PlatformTouchPoint::TouchReleased, true)));
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

TEST_F(GestureRecognizerTest, gestureScrollEvents)
{
    InspectableGestureRecognizerChromium gm;

    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());

    BuildablePlatformTouchPoint press(10, 15, PlatformTouchPoint::TouchPressed);
    BuildablePlatformTouchEvent pressEvent(WebCore::TouchStart, press);
    gm.processTouchEventForGestures(pressEvent, false);

    ASSERT_EQ(GestureRecognizerChromium::PendingSyntheticClick, gm.state());

    BuildablePlatformTouchPoint move(10, 50, PlatformTouchPoint::TouchMoved);
    BuildablePlatformTouchEvent moveEvent(WebCore::TouchMove, move);
    OwnPtr<Vector<WebCore::PlatformGestureEvent> > gestureStart(gm.processTouchEventForGestures(moveEvent, false));
    bool scrollStarted = false, scrollUpdated = false;
    for (unsigned int i = 0; i < gestureStart->size(); i++) {
        switch ((*gestureStart)[i].type()) {
        case PlatformGestureEvent::ScrollBeginType:
            scrollStarted = true;
            break;
        case PlatformGestureEvent::ScrollUpdateType:
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
    BuildablePlatformTouchEvent releaseEvent(WebCore::TouchEnd, release);
    bool scrollEnd = false;
    OwnPtr<Vector<WebCore::PlatformGestureEvent> > gestureEnd(gm.processTouchEventForGestures(releaseEvent, false));
    for (unsigned int i = 0; i < gestureEnd->size(); i++) {
        switch ((*gestureEnd)[i].type()) {
        case PlatformGestureEvent::ScrollEndType:
            scrollEnd = true;
            break;
        default:
            ASSERT_TRUE(false);
        }
    }
    ASSERT_TRUE(scrollEnd);
    ASSERT_EQ(GestureRecognizerChromium::NoGesture, gm.state());
}
