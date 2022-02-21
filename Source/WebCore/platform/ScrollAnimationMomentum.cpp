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
    : ScrollAnimation(Type::Momentum, client)
{
}

ScrollAnimationMomentum::~ScrollAnimationMomentum() = default;

bool ScrollAnimationMomentum::startAnimatedScrollWithInitialVelocity(const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta, const Function<FloatPoint(const FloatPoint&)>& destinationModifier)
{
    auto extents = m_client.scrollExtentsForAnimation(*this);
    m_currentOffset = initialOffset;

    m_momentumCalculator = ScrollingMomentumCalculator::create(extents, initialOffset, initialDelta, initialVelocity);
    auto destinationScrollOffset = m_momentumCalculator->destinationScrollOffset();

    if (destinationModifier) {
        auto modifiedOffset = destinationModifier(destinationScrollOffset);
        if (modifiedOffset != destinationScrollOffset) {
            LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollAnimationMomentum " << this << " startAnimatedScrollWithInitialVelocity - predicted offset " << destinationScrollOffset << " modified to " << modifiedOffset);
            destinationScrollOffset = modifiedOffset;
            m_momentumCalculator->setRetargetedScrollOffset(destinationScrollOffset);
        }
    }

    LOG(ScrollAnimations, "ScrollAnimationMomentum::startAnimatedScrollWithInitialVelocity: velocity %.2f,%.2f from %.2f,%.2f to %.2f,%.2f",
        initialVelocity.width(), initialVelocity.height(), initialOffset.x(), initialOffset.y(), destinationScrollOffset.x(), destinationScrollOffset.y());

    if (destinationScrollOffset == initialOffset) {
        m_momentumCalculator = nullptr;
        return false;
    }

    didStart(MonotonicTime::now());
    return true;
}

bool ScrollAnimationMomentum::retargetActiveAnimation(const FloatPoint& newDestination)
{
    if (m_momentumCalculator && isActive()) {
        m_momentumCalculator->setRetargetedScrollOffset(newDestination);
        auto newDuration = m_momentumCalculator->animationDuration();
        bool animationComplete = newDuration > 0_s;
        if (animationComplete)
            didEnd();

        return !animationComplete;
    }

    return false;
}

void ScrollAnimationMomentum::stop()
{
    LOG(ScrollAnimations, "ScrollAnimationMomentum::stop: offset %.2f,%.2f", m_currentOffset.x(), m_currentOffset.y());

    m_momentumCalculator = nullptr;
    ScrollAnimation::stop();
}

void ScrollAnimationMomentum::serviceAnimation(MonotonicTime currentTime)
{
    if (!m_momentumCalculator) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto elapsedTime = timeSinceStart(currentTime);
    bool animationComplete = elapsedTime >= m_momentumCalculator->animationDuration();
    m_currentOffset = m_momentumCalculator->scrollOffsetAfterElapsedTime(elapsedTime);

    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);

    LOG(ScrollAnimations, "ScrollAnimationMomentum::serviceAnimation: offset %.2f,%.2f complete %d", m_currentOffset.x(), m_currentOffset.y(), animationComplete);

    if (animationComplete)
        didEnd();
}

void ScrollAnimationMomentum::updateScrollExtents()
{
    auto extents = m_client.scrollExtentsForAnimation(*this);
    auto predictedScrollOffset = m_momentumCalculator->destinationScrollOffset();
    auto constrainedOffset = predictedScrollOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
    if (constrainedOffset != predictedScrollOffset)
        retargetActiveAnimation(constrainedOffset);
}

String ScrollAnimationMomentum::debugDescription() const
{
    TextStream textStream;
    textStream << "ScrollAnimationMomentum " << this << " active " << isActive() << " destination " << (m_momentumCalculator ? m_momentumCalculator->destinationScrollOffset() : FloatPoint()) << " current offset " << currentOffset();
    return textStream.release();
}

} // namespace WebCore
