/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AnimationEffect.h"

#include "AnimationEffectTiming.h"
#include "FillMode.h"
#include "JSComputedTimingProperties.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

AnimationEffect::AnimationEffect(ClassType classType)
    : m_classType(classType)
{
    m_timing = AnimationEffectTiming::create();
}

std::optional<Seconds> AnimationEffect::localTime() const
{
    if (m_animation)
        return m_animation->currentTime();
    return std::nullopt;
}

ComputedTimingProperties AnimationEffect::getComputedTiming()
{
    ComputedTimingProperties computedTiming;
    computedTiming.delay = m_timing->bindingsDelay();
    computedTiming.endDelay = m_timing->bindingsEndDelay();
    auto fillMode = m_timing->fill();
    computedTiming.fill = fillMode == FillMode::Auto ? FillMode::None : fillMode;
    computedTiming.iterationStart = m_timing->iterationStart();
    computedTiming.iterations = m_timing->iterations();
    computedTiming.duration = secondsToWebAnimationsAPITime(m_timing->iterationDuration());
    computedTiming.direction = m_timing->direction();
    computedTiming.easing = m_timing->easing();
    computedTiming.endTime = secondsToWebAnimationsAPITime(m_timing->endTime());
    computedTiming.activeDuration = secondsToWebAnimationsAPITime(m_timing->activeDuration());
    if (auto effectLocalTime = localTime())
        computedTiming.localTime = secondsToWebAnimationsAPITime(effectLocalTime.value());
    return computedTiming;
}

} // namespace WebCore
