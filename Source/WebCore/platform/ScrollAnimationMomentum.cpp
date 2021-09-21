/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollAnimationMomentum.h"

#include "Logging.h"
#include "ScrollingMomentumCalculator.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollAnimationMomentum::ScrollAnimationMomentum(ScrollAnimationClient& client)
    : ScrollAnimation(client)
{
}

ScrollAnimationMomentum::~ScrollAnimationMomentum() = default;

bool ScrollAnimationMomentum::startAnimatedScrollWithInitialVelocity(const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta, const WTF::Function<FloatPoint(const FloatPoint&)>& destinationModifier)
{
    auto extents = m_client.scrollExtentsForAnimation(*this);

    m_momentumCalculator = ScrollingMomentumCalculator::create(extents, initialOffset, initialDelta, initialVelocity);
    auto predictedScrollOffset = m_momentumCalculator->predictedDestinationOffset();

    if (destinationModifier) {
        predictedScrollOffset = destinationModifier(predictedScrollOffset);
        m_momentumCalculator->setRetargetedScrollOffset(predictedScrollOffset);
    }

    if (predictedScrollOffset == initialOffset) {
        m_momentumCalculator = nullptr;
        m_animationComplete = true;
        return false;
    }

    m_startTime = MonotonicTime::now();
    m_animationComplete = false;
    return true;
}

bool ScrollAnimationMomentum::retargetActiveAnimation(const FloatPoint& newDestination)
{
    if (m_momentumCalculator) {
        m_momentumCalculator->setRetargetedScrollOffset(newDestination);
        auto newDuration = m_momentumCalculator->animationDuration();
        m_animationComplete = newDuration > 0_s;
        return !m_animationComplete;
    }
    return false;
}

void ScrollAnimationMomentum::stop()
{
    m_momentumCalculator = nullptr;
    m_animationComplete = true;
    m_client.scrollAnimationDidEnd(*this);
}

bool ScrollAnimationMomentum::isActive() const
{
    return !m_animationComplete;
}

FloatPoint ScrollAnimationMomentum::serviceAnimation(MonotonicTime currentTime)
{
    if (!m_momentumCalculator) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto elapsedTime = currentTime - m_startTime;
    m_animationComplete = elapsedTime >= m_momentumCalculator->animationDuration();
    auto newOffset = m_momentumCalculator->scrollOffsetAfterElapsedTime(elapsedTime);

    m_client.scrollAnimationDidUpdate(*this, newOffset);

    if (m_animationComplete)
        m_client.scrollAnimationDidEnd(*this);
    
    return newOffset;
}

void ScrollAnimationMomentum::updateScrollExtents()
{
    auto extents = m_client.scrollExtentsForAnimation(*this);
    auto predictedScrollOffset = m_momentumCalculator->predictedDestinationOffset();
    auto constrainedOffset = predictedScrollOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
    if (constrainedOffset != predictedScrollOffset) {
        retargetActiveAnimation(constrainedOffset);
        if (m_animationComplete)
            m_client.scrollAnimationDidEnd(*this);
    }
}

} // namespace WebCore
