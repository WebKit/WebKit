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

    void reset()
    {
        stopAnimationTimerIfNeeded(&m_horizontalData);
        stopAnimationTimerIfNeeded(&m_verticalData);
        m_currentPosX = 0;
        m_currentPosY = 0;
        m_horizontalData.reset();
        m_verticalData.reset();
    }

    MOCK_METHOD1(scrollToOffsetWithoutAnimation, void(const FloatPoint&));
};

TEST(ScrollAnimatorEnabled, Enabled)
{
    MockScrollableArea scrollableArea(true);
    MockScrollAnimatorNone scrollAnimatorChromium(&scrollableArea);

    EXPECT_CALL(scrollableArea, scrollSize(_)).Times(AtLeast(1)).WillRepeatedly(Return(1000));
    EXPECT_CALL(scrollableArea, setScrollOffset(_)).Times(AtLeast(1));

    scrollAnimatorChromium.scroll(HorizontalScrollbar, ScrollByLine, 100, 1);
    EXPECT_NE(100, scrollAnimatorChromium.currentX());
    EXPECT_NE(0, scrollAnimatorChromium.currentX());
    EXPECT_EQ(0, scrollAnimatorChromium.currentY());
    scrollAnimatorChromium.reset();

    scrollAnimatorChromium.scroll(HorizontalScrollbar, ScrollByPage, 100, 1);
    EXPECT_NE(100, scrollAnimatorChromium.currentX());
    EXPECT_NE(0, scrollAnimatorChromium.currentX());
    EXPECT_EQ(0, scrollAnimatorChromium.currentY());
    scrollAnimatorChromium.reset();

    scrollAnimatorChromium.scroll(HorizontalScrollbar, ScrollByPixel, 4, 25);
    EXPECT_NE(100, scrollAnimatorChromium.currentX());
    EXPECT_NE(0, scrollAnimatorChromium.currentX());
    EXPECT_EQ(0, scrollAnimatorChromium.currentY());
    scrollAnimatorChromium.reset();
}

TEST(ScrollAnimatorEnabled, Disabled)
{
    MockScrollableArea scrollableArea(false);
    MockScrollAnimatorNone scrollAnimatorChromium(&scrollableArea);

    EXPECT_CALL(scrollableArea, scrollSize(_)).Times(AtLeast(1)).WillRepeatedly(Return(1000));
    EXPECT_CALL(scrollableArea, setScrollOffset(_)).Times(AtLeast(1));

    scrollAnimatorChromium.scroll(HorizontalScrollbar, ScrollByLine, 100, 1);
    EXPECT_EQ(100, scrollAnimatorChromium.currentX());
    EXPECT_EQ(0, scrollAnimatorChromium.currentY());
    scrollAnimatorChromium.reset();

    scrollAnimatorChromium.scroll(HorizontalScrollbar, ScrollByPage, 100, 1);
    EXPECT_EQ(100, scrollAnimatorChromium.currentX());
    EXPECT_EQ(0, scrollAnimatorChromium.currentY());
    scrollAnimatorChromium.reset();

    scrollAnimatorChromium.scroll(HorizontalScrollbar, ScrollByDocument, 100, 1);
    EXPECT_EQ(100, scrollAnimatorChromium.currentX());
    EXPECT_EQ(0, scrollAnimatorChromium.currentY());
    scrollAnimatorChromium.reset();

    scrollAnimatorChromium.scroll(HorizontalScrollbar, ScrollByPixel, 100, 1);
    EXPECT_EQ(100, scrollAnimatorChromium.currentX());
    EXPECT_EQ(0, scrollAnimatorChromium.currentY());
    scrollAnimatorChromium.reset();
}

class ScrollAnimatorNoneTest : public testing::Test {
public:
    ScrollAnimatorNoneTest()
    {
    }

    virtual void SetUp()
    {
        m_currentPosition = 100;
        m_data = new ScrollAnimatorNone::PerAxisData(&m_mockScrollAnimatorNone, &m_currentPosition);
    }
    virtual void TearDown()
    {
        delete m_data;
    }

    void reset();
    bool updateDataFromParameters(ScrollbarOrientation, float step, float multiplier, float scrollableSize, double currentTime, ScrollAnimatorNone::Parameters*);
    bool animateScroll(double currentTime);

    double curveIntegralAt(ScrollAnimatorNone::Curve, double t);
    double attackArea(ScrollAnimatorNone::Curve, double startT, double endT);
    double releaseArea(ScrollAnimatorNone::Curve, double startT, double endT);
    double attackCurve(ScrollAnimatorNone::Curve, double deltaT, double curveT, double startPosition, double attackPosition);
    double releaseCurve(ScrollAnimatorNone::Curve, double deltaT, double curveT, double releasePosition, double desiredPosition);
    double curveDerivativeAt(ScrollAnimatorNone::Curve, double unitTime);

    void curveTestInner(ScrollAnimatorNone::Curve, double step, double time);
    void curveTest(ScrollAnimatorNone::Curve);

    static double kTickTime;
    static double kAnimationTime;
    static double kStartTime;
    static double kEndTime;
    float m_currentPosition;
    MockScrollAnimatorNone m_mockScrollAnimatorNone;
    ScrollAnimatorNone::PerAxisData* m_data;
};

double ScrollAnimatorNoneTest::kTickTime = 1 / 60.0;
double ScrollAnimatorNoneTest::kAnimationTime = 0.01;
double ScrollAnimatorNoneTest::kStartTime = 10.0;
double ScrollAnimatorNoneTest::kEndTime = 20.0;

void ScrollAnimatorNoneTest::reset()
{
    m_data->reset();
}

bool ScrollAnimatorNoneTest::updateDataFromParameters(ScrollbarOrientation orientation, float step, float multiplier, float scrollableSize, double currentTime, ScrollAnimatorNone::Parameters* parameters)
{
    double oldVelocity = m_data->m_currentVelocity;
    double oldDesiredVelocity = m_data->m_desiredVelocity;
    bool result = m_data->updateDataFromParameters(orientation, step, multiplier, scrollableSize, currentTime, parameters);
    EXPECT_LE(oldVelocity, m_data->m_currentVelocity);

    double deltaTime = m_data->m_lastAnimationTime - m_data->m_startTime;
    double timeLeft = m_data->m_animationTime - deltaTime;
    double releaseTimeLeft = std::min(timeLeft, m_data->m_releaseTime);
    double attackTimeLeft = std::max(0., m_data->m_attackTime - deltaTime);
    double sustainTimeLeft = std::max(0., timeLeft - releaseTimeLeft - attackTimeLeft);

    EXPECT_LE(oldDesiredVelocity * 0.99, m_data->m_desiredVelocity);

    double expectedReleasePosition = m_data->m_attackPosition + sustainTimeLeft * m_data->m_desiredVelocity;
    EXPECT_DOUBLE_EQ(expectedReleasePosition, m_data->m_releasePosition);

    return result;
}

bool ScrollAnimatorNoneTest::animateScroll(double currentTime)
{
    double oldPosition = *m_data->m_currentPosition;
    bool testEstimatedMaxVelocity = m_data->m_startTime + m_data->m_animationTime - m_data->m_lastAnimationTime > m_data->m_releaseTime;

    bool result = m_data->animateScroll(currentTime);

    double deltaTime = m_data->m_lastAnimationTime - m_data->m_startTime;
    double timeLeft = m_data->m_animationTime - deltaTime;
    double releaseTimeLeft = std::min(timeLeft, m_data->m_releaseTime);
    double attackTimeLeft = std::max(0., m_data->m_attackTime - deltaTime);
    double sustainTimeLeft = std::max(0., timeLeft - releaseTimeLeft - attackTimeLeft);
    double distanceLeft = m_data->m_desiredPosition - *m_data->m_currentPosition;

    EXPECT_LE(0, m_data->m_currentVelocity);
    EXPECT_LE(oldPosition, *m_data->m_currentPosition);
    EXPECT_GE(m_data->m_desiredVelocity * 2, m_data->m_currentVelocity);
    if (testEstimatedMaxVelocity)
        EXPECT_GE((distanceLeft / sustainTimeLeft) * 1.2, m_data->m_currentVelocity);

    return result;
}

double ScrollAnimatorNoneTest::curveIntegralAt(ScrollAnimatorNone::Curve curve, double t)
{
    switch (curve) {
    case ScrollAnimatorNone::Linear:
        return t * t * t / 3;
    case ScrollAnimatorNone::Quadratic:
        return t * t * t * t / 4;
    case ScrollAnimatorNone::Cubic:
        return t * t * t * t * t / 5;
    case ScrollAnimatorNone::Bounce:
        double area;
        double t1 = std::min(t, 1 / 2.75);
        area = 2.52083 * t1 * t1 * t1;
        if (t < 1 / 2.75)
            return area;
        t1 = std::min(t - 1 / 2.75, 1 / 2.75);
        double bounceArea = t1 * (t1 * (2.52083 * t1 - 1.375) + 1);
        area += bounceArea;
        if (t < 2 / 2.75)
            return area;
        t1 = std::min(t - 2 / 2.75, 0.5 / 2.75);
        bounceArea =  t1 * (t1 * (2.52083 * t1 - 0.6875) + 1);
        area += bounceArea;
        if (t < 2.5 / 2.75)
            return area;
        t1 = t - 2.5 / 2.75;
        bounceArea = t1 * (t1 * (2.52083 * t1 - 0.34375) + 1);
        area += bounceArea;
        return area;
    }
}

double ScrollAnimatorNoneTest::attackArea(ScrollAnimatorNone::Curve curve, double startT, double endT)
{
    double startValue = curveIntegralAt(curve, startT);
    double endValue = curveIntegralAt(curve, endT);
    return endValue - startValue;
}

double ScrollAnimatorNoneTest::releaseArea(ScrollAnimatorNone::Curve curve, double startT, double endT)
{
    double startValue = curveIntegralAt(curve, 1 - endT);
    double endValue = curveIntegralAt(curve, 1 - startT);
    return endValue - startValue;
}

double ScrollAnimatorNoneTest::attackCurve(ScrollAnimatorNone::Curve curve, double deltaT, double curveT, double startPosition, double attackPosition)
{
    return ScrollAnimatorNone::PerAxisData::attackCurve(curve, deltaT, curveT, startPosition, attackPosition);
}

double ScrollAnimatorNoneTest::releaseCurve(ScrollAnimatorNone::Curve curve, double deltaT, double curveT, double releasePosition, double desiredPosition)
{
    return ScrollAnimatorNone::PerAxisData::releaseCurve(curve, deltaT, curveT, releasePosition, desiredPosition);
}

double ScrollAnimatorNoneTest::curveDerivativeAt(ScrollAnimatorNone::Curve curve, double unitTime)
{
    return ScrollAnimatorNone::PerAxisData::curveDerivativeAt(curve, unitTime);
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
            EXPECT_LE(0, velocityDelta);
            EXPECT_LT(0, delta);
        }

        double area = attackArea(curve, 0, t / time) * kPosition;
        EXPECT_LE(0, area);
        EXPECT_NEAR(accumulate, area, 1.0);
    }

    oldPos = 0;
    oldVelocity *= 2;
    accumulate = attackArea(curve, 0, 1) * kPosition;
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

TEST_F(ScrollAnimatorNoneTest, CurveMathBounce)
{
    curveTest(ScrollAnimatorNone::Bounce);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceLinear)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, ScrollAnimatorNone::Linear, 3 * kTickTime, ScrollAnimatorNone::Linear, 3 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollLongQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 20 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollQuadraticNoSustain)
{
    ScrollAnimatorNone::Parameters parameters(true, 8 * kTickTime, ScrollAnimatorNone::Quadratic, 4 * kTickTime, ScrollAnimatorNone::Quadratic, 4 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceCubic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollOnceShort)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime, ScrollAnimatorNone::Cubic, 3 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    for (double t = kStartTime; result && t < kEndTime; t += kTickTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollTwiceQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    result = result && animateScroll(t);
    double before = m_currentPosition;
    result = result && updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, t, &parameters);
    result = result && animateScroll(t);
    double after = m_currentPosition;
    EXPECT_NEAR(before, after, 10);

    t += kAnimationTime;

    result = result && animateScroll(t);
    before = m_currentPosition;
    result = result && updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, t, &parameters);
    result = result && animateScroll(t);
    after = m_currentPosition;
    EXPECT_NEAR(before, after, 10);

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = animateScroll(t);
}

TEST_F(ScrollAnimatorNoneTest, ScrollLotsQuadratic)
{
    ScrollAnimatorNone::Parameters parameters(true, 7 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime, ScrollAnimatorNone::Quadratic, 3 * kTickTime);

    updateDataFromParameters(VerticalScrollbar, 1, 40, 1000, kStartTime, &parameters);
    bool result = true;
    double t;
    for (t = kStartTime; result && t < kStartTime + 1.5 * kTickTime; t += kAnimationTime)
        result = animateScroll(t);

    for (int i = 0; i < 20; ++i) {
        t += kAnimationTime;
        result = result && animateScroll(t);
        result = result && updateDataFromParameters(VerticalScrollbar, 3, 40, 10000, t, &parameters);
    }

    t += kAnimationTime;
    for (; result && t < kEndTime; t += kAnimationTime)
        result = result && animateScroll(t);
}

#endif // ENABLE(SMOOTH_SCROLLING)
