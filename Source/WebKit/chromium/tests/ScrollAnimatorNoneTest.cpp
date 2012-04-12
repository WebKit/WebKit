/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the ScrollAnimatorNone class.

#include "config.h"

#if ENABLE(SMOOTH_SCROLLING)

#include "ScrollAnimatorNone.h"

#include "FloatPoint.h"
#include "IntRect.h"
#include "Logging.h"
#include "ScrollAnimator.h"
#include "ScrollableArea.h"
#include "TreeTestHelpers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std;
using namespace WebCore;

using testing::AtLeast;
using testing::Return;
using testing::_;

class MockScrollableArea : public ScrollableArea {
public:
    MockScrollableArea(bool scrollAnimatorEnabled)
        : m_scrollAnimatorEnabled(scrollAnimatorEnabled) { }

    MOCK_CONST_METHOD0(isActive, bool());
    MOCK_CONST_METHOD1(scrollSize, int(ScrollbarOrientation));
    MOCK_CONST_METHOD1(scrollPosition, int(Scrollbar*));
    MOCK_METHOD2(invalidateScrollbar, void(Scrollbar*, const IntRect&));
    MOCK_CONST_METHOD0(isScrollCornerVisible, bool());
    MOCK_CONST_METHOD0(scrollCornerRect, IntRect());
    MOCK_METHOD1(setScrollOffset, void(const IntPoint&));
    MOCK_METHOD2(invalidateScrollbarRect, void(Scrollbar*, const IntRect&));
    MOCK_METHOD1(invalidateScrollCornerRect, void(const IntRect&));
    MOCK_METHOD1(setScrollOffsetFromAnimation, void(const IntPoint&));
    MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());

    virtual int visibleHeight() const { return 768; }
    virtual int visibleWidth() const { return 1024; }

    bool scrollAnimatorEnabled() const { return m_scrollAnimatorEnabled; }
    bool m_scrollAnimatorEnabled;
};

class MockScrollAnimatorNone : public ScrollAnimatorNone {
public:
    MockScrollAnimatorNone()
        : ScrollAnimatorNone(new MockScrollableArea(true)) { }
    MockScrollAnimatorNone(ScrollableArea* scrollableArea)
        : ScrollAnimatorNone(scrollableArea) { }

    float currentX() { return m_currentPosX; }
    float currentY() { return m_currentPosY; }

    FloatPoint m_fp;
    int m_count;

    void reset()
    {
        stopAnimationTimerIfNeeded();
        m_currentPosX = 0;
        m_currentPosY = 0;
        m_horizontalData.reset();
        m_verticalData.reset();
        m_fp = FloatPoint::zero();
        m_count = 0;
    }

    virtual void fireUpAnAnimation(FloatPoint fp)
    {
        m_fp = fp;
        m_count++;
    }

    MOCK_METHOD1(scrollToOffsetWithoutAnimation, void(const FloatPoint&));
};

TEST(ScrollAnimatorEnabled, Enabled)
{
    MockScrollableArea scrollableArea(true);
    MockScrollAnimatorNone scrollAnimatorNone(&scrollableArea);

    EXPECT_CALL(scrollableArea, scrollSize(_)).Times(AtLeast(1)).WillRepeatedly(Return(1000));
    EXPECT_CALL(scrollableArea, setScrollOffset(_)).Times(3);

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByLine, 100, 1);
    EXPECT_NE(100, scrollAnimatorNone.currentX());
    EXPECT_NE(0, scrollAnimatorNone.currentX());
    EXPECT_EQ(0, scrollAnimatorNone.currentY());
    scrollAnimatorNone.reset();

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByPage, 100, 1);
    EXPECT_NE(100, scrollAnimatorNone.currentX());
    EXPECT_NE(0, scrollAnimatorNone.currentX());
    EXPECT_EQ(0, scrollAnimatorNone.currentY());
    scrollAnimatorNone.reset();

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByPixel, 4, 25);
    EXPECT_EQ(100, scrollAnimatorNone.currentX());
    EXPECT_NE(0, scrollAnimatorNone.currentX());
    EXPECT_EQ(0, scrollAnimatorNone.currentY());
    scrollAnimatorNone.reset();
}

TEST(ScrollAnimatorEnabled, flingScrollEncoding)
{
    MockScrollableArea scrollableArea(true);
    MockScrollAnimatorNone scrollAnimatorNone(&scrollableArea);

    scrollAnimatorNone.reset();

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByPixelVelocity, 111, -42);
    scrollAnimatorNone.scroll(VerticalScrollbar, ScrollByPixelVelocity, 222, 42);
    EXPECT_EQ(-42, scrollAnimatorNone.m_fp.x());
    EXPECT_EQ(42, scrollAnimatorNone.m_fp.y());
    EXPECT_EQ(1, scrollAnimatorNone.m_count);
    scrollAnimatorNone.reset();

    scrollAnimatorNone.scroll(VerticalScrollbar, ScrollByPixelVelocity, 222, 42);
    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByPixelVelocity, 111, -42);
    EXPECT_EQ(-42, scrollAnimatorNone.m_fp.x());
    EXPECT_EQ(42, scrollAnimatorNone.m_fp.y());
    EXPECT_EQ(1, scrollAnimatorNone.m_count);
    scrollAnimatorNone.reset();
}



TEST(ScrollAnimatorEnabled, Disabled)
{
    MockScrollableArea scrollableArea(false);
    MockScrollAnimatorNone scrollAnimatorNone(&scrollableArea);

    EXPECT_CALL(scrollableArea, scrollSize(_)).Times(AtLeast(1)).WillRepeatedly(Return(1000));
    EXPECT_CALL(scrollableArea, setScrollOffset(_)).Times(4);

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByLine, 100, 1);
    EXPECT_EQ(100, scrollAnimatorNone.currentX());
    EXPECT_EQ(0, scrollAnimatorNone.currentY());
    scrollAnimatorNone.reset();

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByPage, 100, 1);
    EXPECT_EQ(100, scrollAnimatorNone.currentX());
    EXPECT_EQ(0, scrollAnimatorNone.currentY());
    scrollAnimatorNone.reset();

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByDocument, 100, 1);
    EXPECT_EQ(100, scrollAnimatorNone.currentX());
    EXPECT_EQ(0, scrollAnimatorNone.currentY());
    scrollAnimatorNone.reset();

    scrollAnimatorNone.scroll(HorizontalScrollbar, ScrollByPixel, 100, 1);
    EXPECT_EQ(100, scrollAnimatorNone.currentX());
    EXPECT_EQ(0, scrollAnimatorNone.currentY());
    scrollAnimatorNone.reset();
}

class ScrollAnimatorNoneTest : public testing::Test {
public:
    struct SavePerAxisData : public ScrollAnimatorNone::PerAxisData {
        SavePerAxisData(const ScrollAnimatorNone::PerAxisData& data)
            : ScrollAnimatorNone::PerAxisData(&m_mockScrollAnimatorNone, 0, 768)
        {
            this->m_currentVelocity = data.m_currentVelocity;
            this->m_desiredPosition = data.m_desiredPosition;
            this->m_desiredVelocity = data.m_desiredVelocity;
            this->m_startPosition = data.m_startPosition;
            this->m_startTime = data.m_startTime;
            this->m_startVelocity = data.m_startVelocity;
            this->m_animationTime = data.m_animationTime;
            this->m_lastAnimationTime = data.m_lastAnimationTime;
            this->m_attackPosition = data.m_attackPosition;
            this->m_attackTime = data.m_attackTime;
            this->m_attackCurve = data.m_attackCurve;
            this->m_releasePosition = data.m_releasePosition;
            this->m_releaseTime = data.m_releaseTime;
            this->m_releaseCurve = data.m_releaseCurve;
        }

        bool operator==(const SavePerAxisData& other) const
        {
            return m_currentVelocity == other.m_currentVelocity && m_desiredPosition == other.m_desiredPosition && m_desiredVelocity == other.m_desiredVelocity && m_startPosition == other.m_startPosition && m_startTime == other.m_startTime && m_startVelocity == other.m_startVelocity && m_animationTime == other.m_animationTime && m_lastAnimationTime == other.m_lastAnimationTime && m_attackPosition == other.m_attackPosition && m_attackTime == other.m_attackTime && m_attackCurve == other.m_attackCurve && m_releasePosition == other.m_releasePosition && m_releaseTime == other.m_releaseTime && m_releaseCurve == other.m_releaseCurve;
        }
        MockScrollAnimatorNone m_mockScrollAnimatorNone;
    };

    ScrollAnimatorNoneTest()
    {
    }

    virtual void SetUp()
    {
        m_currentPosition = 100;
        m_data = new ScrollAnimatorNone::PerAxisData(&m_mockScrollAnimatorNone, &m_currentPosition, 768);
    }
    virtual void TearDown()
    {
        delete m_data;
    }

    void reset();
    bool updateDataFromParameters(float step, float multiplier, float scrollableSize, double currentTime, ScrollAnimatorNone::Parameters*);
    bool animateScroll(double currentTime);

    double attackArea(ScrollAnimatorNone::Curve, double startT, double endT);
    double releaseArea(ScrollAnimatorNone::Curve, double startT, double endT);
    double attackCurve(ScrollAnimatorNone::Curve, double deltaT, double curveT, double startPosition, double attackPosition);
    double releaseCurve(ScrollAnimatorNone::Curve, double deltaT, double curveT, double releasePosition, double desiredPosition);
    double coastCurve(ScrollAnimatorNone::Curve, double factor);

    void curveTestInner(ScrollAnimatorNone::Curve, double step, double time);
    void curveTest(ScrollAnimatorNone::Curve);

    void checkDesiredPosition(float expectedPosition);
    void checkSoftLanding(float expectedPosition);

    static double kTickTime;
    static double kAnimationTime;
    static double kStartTime;
    static double kEndTime;
    float m_currentPosition;
    MockScrollAnimatorNone m_mockScrollAnimatorNone;
    bool m_scrollingDown;
    ScrollAnimatorNone::PerAxisData* m_data;
};

double ScrollAnimatorNoneTest::kTickTime = 1 / 60.0;
double ScrollAnimatorNoneTest::kAnimationTime = 0.01;
double ScrollAnimatorNoneTest::kStartTime = 10.0;
double ScrollAnimatorNoneTest::kEndTime = 20.0;

void ScrollAnimatorNoneTest::reset()
{
    m_data->reset();
    m_scrollingDown = true;
}

bool ScrollAnimatorNoneTest::updateDataFromParameters(float step, float multiplier, float scrollableSize, double currentTime, ScrollAnimatorNone::Parameters* parameters)
{
    if (step * multiplier)
        m_scrollingDown = (step * multiplier > 0);

    double oldVelocity = m_data->m_currentVelocity;
    double oldDesiredVelocity = m_data->m_desiredVelocity;
    double oldTimeLeft = m_data->m_animationTime - (m_data->m_lastAnimationTime - m_data->m_startTime);
    bool result = m_data->updateDataFromParameters(step, multiplier, scrollableSize, currentTime, parameters);
    if (m_scrollingDown)
        EXPECT_LE(oldVelocity, m_data->m_currentVelocity);
    else
        EXPECT_GE(oldVelocity, m_data->m_currentVelocity);

    double deltaTime = m_data->m_lastAnimationTime - m_data->m_startTime;
    double timeLeft = m_data->m_animationTime - deltaTime;
    double releaseTimeLeft = min(timeLeft, m_data->m_releaseTime);
    double attackTimeLeft = max(0., m_data->m_attackTime - deltaTime);
    double sustainTimeLeft = max(0., timeLeft - releaseTimeLeft - attackTimeLeft);

    // If we're getting near the finish, the desired velocity can decrease since the time left gets increased.
    if (step * multiplier) {
        double allowedVelocityDecreaseFactor = 0.99 * oldTimeLeft / timeLeft;
        allowedVelocityDecreaseFactor *= allowedVelocityDecreaseFactor;
        if (m_scrollingDown)
            EXPECT_LE(oldDesiredVelocity * allowedVelocityDecreaseFactor, m_data->m_desiredVelocity);
        else
            EXPECT_GE(oldDesiredVelocity * allowedVelocityDecreaseFactor, m_data->m_desiredVelocity);

        double startPosition = attackTimeLeft ? m_data->m_attackPosition : m_currentPosition;
        double expectedReleasePosition = startPosition + sustainTimeLeft * m_data->m_desiredVelocity;
        EXPECT_NEAR(expectedReleasePosition, m_data->m_releasePosition, result ? .0001 : 1);
    }

    return result;
}

bool ScrollAnimatorNoneTest::animateScroll(double currentTime)
{
    double oldPosition = *m_data->m_currentPosition;
    bool testEstimatedMaxVelocity = m_data->m_startTime + m_data->m_animationTime - m_data->m_lastAnimationTime > m_data->m_releaseTime;

    bool result = m_data->animateScroll(currentTime);

    double deltaTime = m_data->m_lastAnimationTime - m_data->m_startTime;
    double timeLeft = m_data->m_animationTime - deltaTime;
    double releaseTimeLeft = min(timeLeft, m_data->m_releaseTime);
    double attackTimeLeft = max(0., m_data->m_attackTime - deltaTime);
    double sustainTimeLeft = max(0., timeLeft - releaseTimeLeft - attackTimeLeft);
    double distanceLeft = m_data->m_desiredPosition - *m_data->m_currentPosition;

    if (m_scrollingDown) {
        EXPECT_LE(0, m_data->m_currentVelocity);
        EXPECT_LE(oldPosition, *m_data->m_currentPosition);
    } else {
        EXPECT_GE(0, m_data->m_currentVelocity);
        EXPECT_GE(oldPosition, *m_data->m_currentPosition);
    }
    EXPECT_GE(fabs(m_data->m_desiredVelocity) * 2, fabs(m_data->m_currentVelocity));
    if (testEstimatedMaxVelocity)
        EXPECT_GE(fabs(distanceLeft / sustainTimeLeft) * 1.2, fabs(m_data->m_currentVelocity));

    return result;
}

double ScrollAnimatorNoneTest::attackArea(ScrollAnimatorNone::Curve curve, double startT, double endT)
{
    return ScrollAnimatorNone::PerAxisData::attackArea(curve, startT, endT);
}

double ScrollAnimatorNoneTest::releaseArea(ScrollAnimatorNone::Curve curve, double startT, double endT)
{
    return ScrollAnimatorNone::PerAxisData::releaseArea(curve, startT, endT);
}

double ScrollAnimatorNoneTest::attackCurve(ScrollAnimatorNone::Curve curve, double deltaT, double curveT, double startPosition, double attackPosition)
{
    return ScrollAnimatorNone::PerAxisData::attackCurve(curve, deltaT, curveT, startPosition, attackPosition);
}

double ScrollAnimatorNoneTest::releaseCurve(ScrollAnimatorNone::Curve curve, double deltaT, double curveT, double releasePosition, double desiredPosition)
{
    return ScrollAnimatorNone::PerAxisData::releaseCurve(curve, deltaT, curveT, releasePosition, desiredPosition);
}

double ScrollAnimatorNoneTest::coastCurve(ScrollAnimatorNone::Curve curve, double factor)
{
    return ScrollAnimatorNone::PerAxisData::coastCurve(curve, factor);
}

void ScrollAnimatorNoneTest::curveTestInner(ScrollAnimatorNone::Curve curve, double step, double time)
{
    const double kPosition = 1000;

    double oldPos = 0;
    double oldVelocity = 0;
    double accumulate = 0;

    for (double t = step ; t <= time ; t += step) {
        double newPos = attackCurve(curve, t, time, 0, kPosition);
        double delta = newPos - oldPos;
        double velocity = delta / step;
        double velocityDelta = velocity - oldVelocity;

        accumulate += (oldPos + newPos) / 2 * (step / time);
        oldPos = newPos;
        oldVelocity = velocity;
        if (curve != ScrollAnimatorNone::Bounce) {
            EXPECT_LE(-.0001, velocityDelta);
            EXPECT_LT(0, delta);
        }

        double area = attackArea(curve, 0, t / time) * kPosition;
        EXPECT_LE(0, area);
        EXPECT_NEAR(accumulate, area, 1.0);
    }

    oldPos = 0;
    oldVelocity *= 2;
    accumulate = releaseArea(curve, 0, 1) * kPosition;
    for (double t = step ; t <= time ; t += step) {
        double newPos = releaseCurve(curve, t, time, 0, kPosition);
        double delta = newPos - oldPos;
        double velocity = delta / step;
        double velocityDelta = velocity - oldVelocity;

        accumulate -= (kPosition - (oldPos + newPos) / 2) * (step / time);
        oldPos = newPos;
        oldVelocity = velocity;
        if (curve != ScrollAnimatorNone::Bounce) {
            EXPECT_GE(0.01, velocityDelta);
            EXPECT_LT(0, delta);
        }

        double area = releaseArea(curve, t / time, 1) * kPosition;
        EXPECT_LE(0, area);
        EXPECT_NEAR(accumulate, area, 1.0);
    }
}

void ScrollAnimatorNoneTest::curveTest(ScrollAnimatorNone::Curve curve)
{
    curveTestInner(curve, 0.01, 0.25);
    curveTestInner(curve, 0.2, 10);
    curveTestInner(curve, 0.025, 10);
    curveTestInner(curve, 0.01, 1);
    curveTestInner(curve, 0.25, 40);
}

void ScrollAnimatorNoneTest::checkDesiredPosition(float expectedPosition)
{
    EXPECT_EQ(expectedPosition, m_data->m_desiredPosition);
}

void ScrollAnimatorNoneTest::checkSoftLanding(float expectedPosition)
{
    EXPECT_EQ(expectedPosition, m_currentPosition);
    EXPECT_LE(m_data->m_desiredVelocity / 2, m_data->m_currentVelocity);
}

TEST_F(ScrollAnimatorNoneTest, CurveMathLinear)
{
    curveTest(ScrollAnimatorNone::Linear);
}

TEST_F(ScrollAnimatorNoneTest, CurveMathQuadratic)
{
    curveTest(ScrollAnimatorNone::Quadratic);
}

TEST_F(ScrollAnimatorNoneTest, CurveMathCubic)
{
    curveTest(ScrollAnimatorNone::Cubic);
}

TEST_F(ScrollAnimatorNoneTest, CurveMathQuartic)
{
    curveTest(ScrollAnimatorNone::Quartic);
}

TEST_F(ScrollAnimatorNoneTest, CurveMathBounce)
{
    curveTest(ScrollAnimatorNone::Bounce);
}

TEST_F(ScrollAnimatorNoneTest, CurveMathCoast)
{
    for (double t = .25; t < 1; t += .25) {
        EXPECT_EQ(t, coastCurve(ScrollAnimatorNone::Linear, t));
        EXPECT_LT(t, coastCurve(ScrollAnimatorNone::Quadratic, t));
        EXPECT_LT(t, coastCurve(ScrollAnimatorNone::Cubic, t));
        EXPECT_LT(coastCurve(ScrollAnimatorNone::Quadratic, t), coastCurve(ScrollAnimatorNone::Cubic, t));
        EXPECT_LT(t, coastCurve(ScrollAnimatorNone::Quartic, t));
        EXPECT_LT(coastCurve(ScrollAnimatorNone::Cubic, t), coastCurve(ScrollAnimatorNone::Quartic, t));
    }
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceLinear)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Linear, 3 * kTickTime, ScrollAnimatorNone::Linear, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollLongQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 20 * kTickTime, 0, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollQuadraticNoSustain)
{
    ScrollAnimatorNone::Parameters parameters(true, 8 * kTickTime, 0, ScrollAnimatorNone::Quadratic, 4 * kTickTime, ScrollAnimatorNone::Quadratic, 4 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollQuadraticSmoothed)
{
    ScrollAnimatorNone::Parameters parameters(true, 8 * kTickTime, 8 * kTickTime, ScrollAnimatorNone::Quadratic, 4 * kTickTime, ScrollAnimatorNone::Quadratic, 4 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceCubic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceQuartic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Quartic, 3 * kTickTime, ScrollAnimatorNone::Quartic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceShort)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kTickTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollTwiceQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    result = result && animateScroll(t);
    double before = m_currentPosition;
    result = result && updateDataFromParameters(1, 40, 1000, t, &parameters);
    result = result && animateScroll(t);
    double after = m_currentPosition;
    EXPECT_NEAR(before, after, 10);

    t += kAnimationTime;

    result = result && animateScroll(t);
    before = m_currentPosition;
    result = result && updateDataFromParameters(1, 40, 1000, t, &parameters);
    result = result && animateScroll(t);
    after = m_currentPosition;
    EXPECT_NEAR(before, after, 10);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollLotsQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 10000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    for (int i = 0; i < 20; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        result = result && updateDataFromParameters(3, 40, 10000, t, &parameters);
    }

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollLotsQuadraticSmoothed)
{
    ScrollAnimatorNone::Parameters parameters(true, 10 * kTickTime, 6 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 10000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    for (int i = 0; i < 20; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        result = result && updateDataFromParameters(3, 40, 10000, t, &parameters);
    }

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollTwiceCubic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    result = result && animateScroll(t);
    double before = m_currentPosition;
    result = result && updateDataFromParameters(1, 40, 1000, t, &parameters);
    result = result && animateScroll(t);
    double after = m_currentPosition;
    EXPECT_NEAR(before, after, 10);

    t += kAnimationTime;

    result = result && animateScroll(t);
    before = m_currentPosition;
    result = result && updateDataFromParameters(1, 40, 1000, t, &parameters);
    result = result && animateScroll(t);
    after = m_currentPosition;
    EXPECT_NEAR(before, after, 10);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollLotsCubic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 10000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    for (int i = 0; i < 20; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        result = result && updateDataFromParameters(3, 40, 10000, t, &parameters);
    }

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollLotsCubicSmoothed)
{
    ScrollAnimatorNone::Parameters parameters(true, 10 * kTickTime, 6 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 10000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    for (int i = 0; i < 20; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        result = result && updateDataFromParameters(3, 40, 10000, t, &parameters);
    }

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollWheelTrace)
{
    ScrollAnimatorNone::Parameters parameters(true, 11 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    // Constructed from an actual scroll wheel trace that exhibited a glitch.
    bool result = updateDataFromParameters(1, 53.33f, 1000, 100.5781f, &parameters);
    result = animateScroll(100.5933);
    result = result && animateScroll(100.6085);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.6485f, &parameters);
    result = result && animateScroll(100.6515);
    result = result && animateScroll(100.6853);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.6863f, &parameters);
    result = result && animateScroll(100.7005);
    result = result && animateScroll(100.7157);
    result = result && animateScroll(100.7312);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.7379f, &parameters);
    result = result && animateScroll(100.7464);
    result = result && animateScroll(100.7617);
    result = result && animateScroll(100.7775);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.7779f, &parameters);
    for (double t = 100.7928; result && t < 200; t += 0.015)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollWheelTraceSmoothed)
{
    ScrollAnimatorNone::Parameters parameters(true, 11 * kTickTime, 7 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    // Constructed from an actual scroll wheel trace that exhibited a glitch.
    bool result = updateDataFromParameters(1, 53.33f, 1000, 100.5781f, &parameters);
    result = animateScroll(100.5933);
    result = result && animateScroll(100.6085);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.6485f, &parameters);
    result = result && animateScroll(100.6515);
    result = result && animateScroll(100.6853);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.6863f, &parameters);
    result = result && animateScroll(100.7005);
    result = result && animateScroll(100.7157);
    result = result && animateScroll(100.7312);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.7379f, &parameters);
    result = result && animateScroll(100.7464);
    result = result && animateScroll(100.7617);
    result = result && animateScroll(100.7775);
    result = result && updateDataFromParameters(1, 53.33f, 1000, 100.7779f, &parameters);
    for (double t = 100.7928; result && t < 200; t += 0.015)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, LinuxTrackPadTrace)
{
    ScrollAnimatorNone::Parameters parameters(true, 11 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    bool result = updateDataFromParameters(1.00, 60.00, 1000, 100.6863, &parameters);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.6897, &parameters);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7001, &parameters);
    result = result && animateScroll(100.7015);
    result = result && animateScroll(100.7169);
    result = result && updateDataFromParameters(1.00, 40.00, 1000, 100.7179, &parameters);
    result = result && animateScroll(100.7322);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7332, &parameters);
    result = result && animateScroll(100.7491);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7519, &parameters);
    result = result && animateScroll(100.7676);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7698, &parameters);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7830, &parameters);
    result = result && animateScroll(100.7834);
    result = result && animateScroll(100.7997);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8019, &parameters);
    result = result && animateScroll(100.8154);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8241, &parameters);
    result = result && animateScroll(100.8335);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8465, &parameters);
    result = result && animateScroll(100.8513);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8623, &parameters);
    for (double t = 100.8674; result && t < 200; t += 0.015)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, LinuxTrackPadTraceSmoothed)
{
    ScrollAnimatorNone::Parameters parameters(true, 11 * kTickTime, 7 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    bool result = updateDataFromParameters(1.00, 60.00, 1000, 100.6863, &parameters);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.6897, &parameters);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7001, &parameters);
    result = result && animateScroll(100.7015);
    result = result && animateScroll(100.7169);
    result = result && updateDataFromParameters(1.00, 40.00, 1000, 100.7179, &parameters);
    result = result && animateScroll(100.7322);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7332, &parameters);
    result = result && animateScroll(100.7491);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7519, &parameters);
    result = result && animateScroll(100.7676);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7698, &parameters);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.7830, &parameters);
    result = result && animateScroll(100.7834);
    result = result && animateScroll(100.7997);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8019, &parameters);
    result = result && animateScroll(100.8154);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8241, &parameters);
    result = result && animateScroll(100.8335);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8465, &parameters);
    result = result && animateScroll(100.8513);
    result = result && updateDataFromParameters(1.00, 20.00, 1000, 100.8623, &parameters);
    for (double t = 100.8674; result && t < 200; t += 0.015)
        result = result && animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollDownToBumper)
{
    ScrollAnimatorNone::Parameters parameters(true, 10 * kTickTime, 7 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 20, 200, kStartTime, &parameters));
    bool result = true;
    double t = kStartTime;
    for (int i = 0; i < 10; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        updateDataFromParameters(1, 20, 200, t, &parameters);
    }
    checkDesiredPosition(200);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
    checkSoftLanding(200);
}

TEST_F(ScrollAnimatorNoneTest, ScrollUpToBumper)
{
    ScrollAnimatorNone::Parameters parameters(true, 10 * kTickTime, 7 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, -20, 200, kStartTime, &parameters));
    bool result = true;
    double t = kStartTime;
    for (int i = 0; i < 10; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        updateDataFromParameters(1, -20, 200, t, &parameters);
    }
    checkDesiredPosition(0);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
    checkSoftLanding(0);
}

TEST_F(ScrollAnimatorNoneTest, ScrollUpToBumperCoast)
{
    ScrollAnimatorNone::Parameters parameters(true, 11 * kTickTime, 2 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 1);

    m_currentPosition = 40000;
    EXPECT_TRUE(updateDataFromParameters(1, -10000, 50000, kStartTime, &parameters));
    bool result = true;
    double t = kStartTime;
    for (int i = 0; i < 10; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        updateDataFromParameters(1, -10000, 50000, t, &parameters);
    }
    checkDesiredPosition(0);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
    checkSoftLanding(0);
}

TEST_F(ScrollAnimatorNoneTest, ScrollDownToBumperCoast)
{
    ScrollAnimatorNone::Parameters parameters(true, 11 * kTickTime, 2 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 1);

    m_currentPosition = 10000;
    EXPECT_TRUE(updateDataFromParameters(1, 10000, 50000, kStartTime, &parameters));
    bool result = true;
    double t = kStartTime;
    for (int i = 0; i < 10; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        updateDataFromParameters(1, 10000, 50000, t, &parameters);
    }
    checkDesiredPosition(50000);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
    checkSoftLanding(50000);
}

TEST_F(ScrollAnimatorNoneTest, VaryingInputsEquivalency)
{
    ScrollAnimatorNone::Parameters parameters(true, 15 * kTickTime, 10 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Linear, 0);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 300, 50000, kStartTime, &parameters));
    SavePerAxisData dataSingle(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 150, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 150, 50000, kStartTime, &parameters));
    SavePerAxisData dataDouble(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 100, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 100, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 100, 50000, kStartTime, &parameters));
    SavePerAxisData dataTriple(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 50, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 50, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 50, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 50, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 50, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 50, 50000, kStartTime, &parameters));
    SavePerAxisData dataMany(*m_data);

    EXPECT_EQ(dataSingle, dataDouble);
    EXPECT_EQ(dataSingle, dataTriple);
    EXPECT_EQ(dataSingle, dataMany);
}

TEST_F(ScrollAnimatorNoneTest, VaryingInputsEquivalencyCoast)
{
    ScrollAnimatorNone::Parameters parameters(true, 15 * kTickTime, 10 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Linear, 1);

    reset();
    updateDataFromParameters(1, 300, 50000, kStartTime, &parameters);
    SavePerAxisData dataSingle(*m_data);

    reset();
    updateDataFromParameters(1, 150, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 150, 50000, kStartTime, &parameters);
    SavePerAxisData dataDouble(*m_data);

    reset();
    updateDataFromParameters(1, 100, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 100, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 100, 50000, kStartTime, &parameters);
    SavePerAxisData dataTriple(*m_data);

    reset();
    updateDataFromParameters(1, 50, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 50, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 50, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 50, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 50, 50000, kStartTime, &parameters);
    updateDataFromParameters(1, 50, 50000, kStartTime, &parameters);
    SavePerAxisData dataMany(*m_data);

    EXPECT_EQ(dataSingle, dataDouble);
    EXPECT_EQ(dataSingle, dataTriple);
    EXPECT_EQ(dataSingle, dataMany);
}

TEST_F(ScrollAnimatorNoneTest, VaryingInputsEquivalencyCoastLarge)
{
    ScrollAnimatorNone::Parameters parameters(true, 15 * kTickTime, 10 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Linear, 1);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 30000, 50000, kStartTime, &parameters));
    SavePerAxisData dataSingle(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 15000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 15000, 50000, kStartTime, &parameters));
    SavePerAxisData dataDouble(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 10000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 10000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 10000, 50000, kStartTime, &parameters));
    SavePerAxisData dataTriple(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    SavePerAxisData dataMany(*m_data);

    EXPECT_EQ(dataSingle, dataDouble);
    EXPECT_EQ(dataSingle, dataTriple);
    EXPECT_EQ(dataSingle, dataMany);
}

TEST_F(ScrollAnimatorNoneTest, VaryingInputsEquivalencyCoastSteep)
{
    ScrollAnimatorNone::Parameters parameters(true, 15 * kTickTime, 10 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Cubic, 5 * kTickTime, ScrollAnimatorNone::Quadratic, 1);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 30000, 50000, kStartTime, &parameters));
    SavePerAxisData dataSingle(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 15000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 15000, 50000, kStartTime, &parameters));
    SavePerAxisData dataDouble(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 10000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 10000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 10000, 50000, kStartTime, &parameters));
    SavePerAxisData dataTriple(*m_data);

    reset();
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    EXPECT_TRUE(updateDataFromParameters(1, 5000, 50000, kStartTime, &parameters));
    SavePerAxisData dataMany(*m_data);

    EXPECT_EQ(dataSingle, dataDouble);
    EXPECT_EQ(dataSingle, dataTriple);
    EXPECT_EQ(dataSingle, dataMany);
}

TEST_F(ScrollAnimatorNoneTest, ScrollStopInMiddle)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    result = result && animateScroll(t);
    EXPECT_TRUE(result);
    double before = m_currentPosition;
    result = result && updateDataFromParameters(0, 0, 1000, t, &parameters);
    EXPECT_FALSE(result);
    result = result && animateScroll(t);
    double after = m_currentPosition;
    EXPECT_EQ(before, after);
    checkDesiredPosition(after);
}

TEST_F(ScrollAnimatorNoneTest, ReverseInMiddle)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, 0, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Linear, 0);

    EXPECT_TRUE(updateDataFromParameters(1, 40, 1000, kStartTime, &parameters));
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    result = result && animateScroll(t);
    EXPECT_TRUE(result);
    double before = m_currentPosition;
    result = result && updateDataFromParameters(1, -10, 1000, t, &parameters);
    EXPECT_TRUE(result);
    result = result && animateScroll(t);
    double after = m_currentPosition;
    EXPECT_GE(before, after);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
    EXPECT_GE(before, m_currentPosition);
}

#endif // ENABLE(SMOOTH_SCROLLING)
