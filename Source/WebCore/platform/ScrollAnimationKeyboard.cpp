/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollAnimationKeyboard.h"

#include "FloatPoint.h"
#include "GeometryUtilities.h"
#include "ScrollExtents.h"
#include "ScrollableArea.h"
#include "TimingFunction.h"
#include "platform/KeyboardScroll.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

static float scrollDistance(ScrollDirection direction, ScrollGranularity granularity) const
{
    auto scrollbar = m_scrollAnimator.scrollableArea().scrollbarForDirection(direction);
    if (!scrollbar)
        return false;

    float step = 0;
    switch (granularity) {
    case ScrollGranularity::Line:
        step = scrollbar->lineStep();
        break;
    case ScrollGranularity::Page:
        step = scrollbar->pageStep();
        break;
    case ScrollGranularity::Document:
        step = scrollbar->totalSize();
        break;
    case ScrollGranularity::Pixel:
        step = scrollbar->pixelStep();
        break;
    }

    auto axis = axisFromDirection(direction);
    if (granularity == ScrollGranularity::Page && axis == ScrollEventAxis::Vertical)
        step = m_scrollAnimator.scrollableArea().adjustVerticalPageScrollStepForFixedContent(step);

    return step;
}

static std::optional<KeyboardScroll> makeKeyboardScroll(ScrollDirection direction, ScrollGranularity granularity) const
{
    float distance = scrollDistance(direction, granularity);

    if (!distance)
        return std::nullopt;

    KeyboardScroll scroll;

    scroll.offset = unitVectorForScrollDirection(direction).scaled(distance);
    scroll.granularity = granularity;
    scroll.direction = direction;
    scroll.maximumVelocity = scroll.offset.scaled(KeyboardScrollParameters::parameters().maximumVelocityMultiplier);
    scroll.force = scroll.maximumVelocity.scaled(KeyboardScrollParameters::parameters().springMass / KeyboardScrollParameters::parameters().timeToMaximumVelocity);

    return scroll;
}

ScrollAnimationKeyboard::ScrollAnimationKeyboard(ScrollAnimationClient& client)
    : ScrollAnimation(Type::Keyboard, client)
    // , m_timingFunction(CubicBezierTimingFunction::create())
{
}

ScrollAnimationKeyboard::~ScrollAnimationKeyboard() = default;

bool ScrollAnimationKeyboard::retargetActiveAnimation(const FloatPoint&)
{
    return true;
}

String ScrollAnimationKeyboard::debugDescription() const
{
    return ""_s;
}

/*
 //    auto scroll = makeKeyboardScroll(direction, granularity);
 //    if (!scroll)
 //        return false;

 //    m_currentKeyboardScroll = scroll;

 //    auto scrollableDirections = scrollableDirectionsFromPosition(m_scrollAnimator.currentPosition());
 //    if (!scrollableDirections.at(boxSideForDirection(direction))) {
 //        stopScrollingImmediately();
 //        return false;
 //    }

 //    if (m_scrollTriggeringKeyIsPressed)
 //        return true;

 //    if (granularity == ScrollGranularity::Document) {
 //        m_velocity = { };
 //        stopKeyboardScrollAnimation();
 //        auto newPosition = IntPoint(m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset);
 //        m_scrollAnimator.scrollToPositionWithAnimation(newPosition);
 //        return true;
 //    }

 //    m_timeAtLastFrame = MonotonicTime::now();
 //    m_scrollTriggeringKeyIsPressed = true;

 //    m_idealPositionForMinimumTravel = m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset;
 //    m_scrollController.willBeginKeyboardScrolling();

 //    return true;
 */

bool ScrollAnimationKeyboard::startKeyboardScroll(ScrollDirection, ScrollGranularity)
{
//    ALWAYS_LOG_WITH_STREAM(stream << "RR_CODES ScrollAnimationKeyboard::startKeyboardScroll fromOffset = " << fromOffset);
//
//    auto extents = m_client.scrollExtentsForAnimation(*this);
//
//    m_currentOffset = m_startOffset = fromOffset;
//    m_destinationOffset = destinationOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
//
//    if (!isActive() && fromOffset == m_destinationOffset)
//        return false;
//
//    m_duration = durationFromDistance(m_destinationOffset - m_startOffset);
//    if (!m_duration)
//        return false;
//
//    downcast<CubicBezierTimingFunction>(*m_timingFunction).setTimingFunctionPreset(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);
//
//    if (!isActive())
//        didStart(MonotonicTime::now());

    return true;
}

//bool ScrollAnimationSmooth::startKeyboardScroll(const FloatPoint& fromOffset)
//{
////    auto extents = m_client.scrollExtentsForAnimation(*this);
////
////    m_currentOffset = m_startOffset = fromOffset;
////    m_destinationOffset = destinationOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
////
////    if (!isActive() && fromOffset == m_destinationOffset)
////        return false;
////
////    m_duration = durationFromDistance(m_destinationOffset - m_startOffset);
////    if (!m_duration)
////        return false;
////
////    downcast<CubicBezierTimingFunction>(*m_timingFunction).setTimingFunctionPreset(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);
////
////    if (!isActive())
////        didStart(MonotonicTime::now());
////
////    return true;
//
//    auto scroll = makeKeyboardScroll(direction, granularity);
//    if (!scroll)
//        return false;
//
//    m_currentKeyboardScroll = scroll;
//
//    auto scrollableDirections = scrollableDirectionsFromPosition(m_scrollAnimator.currentPosition());
//    if (!scrollableDirections.at(boxSideForDirection(direction))) {
//        stopScrollingImmediately();
//        return false;
//    }
//
//    if (m_scrollTriggeringKeyIsPressed)
//        return true;
//
//    if (granularity == ScrollGranularity::Document) {
//        m_velocity = { };
//        stopKeyboardScrollAnimation();
//        auto newPosition = IntPoint(m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset);
//        m_scrollAnimator.scrollToPositionWithAnimation(newPosition);
//        return true;
//    }
//
//    m_timeAtLastFrame = MonotonicTime::now();
//    m_scrollTriggeringKeyIsPressed = true;
//
//    m_idealPositionForMinimumTravel = m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset;
//    m_scrollController.willBeginKeyboardScrolling();
//
//    return true;
//}

static RectEdges<bool> scrollableDirectionsFromPosition(FloatPoint position, ScrollExtents extents)
{
    auto minimumScrollPosition = extents.minimumScrollOffset();
    auto maximumScrollPosition = extents.maximumScrollOffset();

    RectEdges<bool> edges;

    edges.setTop(position.y() > minimumScrollPosition.y());
    edges.setBottom(position.y() < maximumScrollPosition.y());
    edges.setLeft(position.x() > minimumScrollPosition.x());
    edges.setRight(position.x() < maximumScrollPosition.x());

    return edges;
}

static BoxSide boxSideForDirection(ScrollDirection direction)
{
    switch (direction) {
    case ScrollDirection::ScrollUp:
        return BoxSide::Top;
    case ScrollDirection::ScrollDown:
        return BoxSide::Bottom;
    case ScrollDirection::ScrollLeft:
        return BoxSide::Left;
    case ScrollDirection::ScrollRight:
        return BoxSide::Right;
    }
    ASSERT_NOT_REACHED();
    return BoxSide::Top;
}

static FloatSize perpendicularAbsoluteUnitVector(ScrollDirection direction)
{
    switch (direction) {
    case ScrollDirection::ScrollUp:
    case ScrollDirection::ScrollDown:
        return { 1, 0 };
    case ScrollDirection::ScrollLeft:
    case ScrollDirection::ScrollRight:
        return { 0, 1 };
    }
    ASSERT_NOT_REACHED();
    return { };
}

bool ScrollAnimationKeyboard::animateScroll(MonotonicTime currentTime)
{
    ALWAYS_LOG_WITH_STREAM(stream << "RR_CODES ScrollAnimationKeyboard::animateScroll");

    auto extents = m_client.scrollExtentsForAnimation(*this);

     auto force = FloatSize { };
     auto axesToApplySpring = FloatSize { 1, 1 };
     KeyboardScrollParameters params = KeyboardScrollParameters::parameters();
 
    auto scrollableDirections = scrollableDirectionsFromPosition(m_currentOffset, extents);

    if (scrollableDirections.at(boxSideForDirection(m_direction))) {
        // Apply the scrolling force. Only apply the spring in the perpendicular axis,
        // otherwise it drags against the direction of motion.
        axesToApplySpring = perpendicularAbsoluteUnitVector(m_direction);
        force = m_currentForce;
    } else {
        // The scroll view cannot scroll in this direction, and is rubber-banding.
        // Apply a constant and significant force; otherwise, the force for a
        // single-line increment is not strong enough to rubber-band perceptibly.
        force = unitVectorForScrollDirection(m_direction).scaled(params.rubberBandForce);
    }

    if (fabs(m_velocity.width()) >= fabs(m_maximumVelocity.width()))
        force.setWidth(0);

    if (fabs(m_velocity.height()) >= fabs(m_maximumVelocity.height()))
        force.setHeight(0);


    auto idealPosition = m_currentOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());

//     ScrollPosition idealPosition = m_scrollAnimator.scrollableArea().constrainedScrollPosition(IntPoint(m_currentKeyboardScroll ? m_scrollAnimator.currentPosition() : m_idealPosition));
     FloatPoint displacement = idealPosition;

     // auto springForce = -displacement.scaled(params.springStiffness) - m_velocity.scaled(params.springDamping);
     // force += springForce * axesToApplySpring;

     float frameDuration = (currentTime - m_timeAtLastFrame).value();
     m_timeAtLastFrame = currentTime;

     FloatSize acceleration = force.scaled(1. / params.springMass);
     m_velocity += acceleration.scaled(frameDuration);
     m_currentOffset = m_currentOffset + m_velocity.scaled(frameDuration);

//     m_scrollAnimator.scrollToPositionWithoutAnimation(newPosition);

    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);

     // Stop the spring if it reaches the ideal position.
     FloatSize newDisplacement = m_currentOffset - idealPosition;
     if (axesToApplySpring.width() && displacement.x() * newDisplacement.width() < 0)
         m_velocity.setWidth(0);
     if (axesToApplySpring.height() && displacement.y() * newDisplacement.height() < 0)
         m_velocity.setHeight(0);

     return true;
}

void ScrollAnimationKeyboard::serviceAnimation(MonotonicTime currentTime)
{
    bool animationActive = animateScroll(currentTime);
//    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);
    if (!animationActive)
        didEnd();
}

} // namespace WebCore
