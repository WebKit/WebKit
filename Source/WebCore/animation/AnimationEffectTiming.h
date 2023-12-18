/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "AnimationEffectPhase.h"
#include "BasicEffectTiming.h"
#include "FillMode.h"
#include "PlaybackDirection.h"
#include "TimingFunction.h"
#include "WebAnimationTypes.h"
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>

namespace WebCore {

struct ResolvedEffectTiming {
    MarkableDouble currentIteration;
    AnimationEffectPhase phase { AnimationEffectPhase::Idle };
    MarkableDouble transformedProgress;
    MarkableDouble simpleIterationProgress;
};

struct AnimationEffectTiming {
    RefPtr<TimingFunction> timingFunction { LinearTimingFunction::create() };
    FillMode fill { FillMode::Auto };
    PlaybackDirection direction { PlaybackDirection::Normal };
    double iterationStart { 0 };
    double iterations { 1 };
    Seconds delay { 0_s };
    Seconds endDelay { 0_s };
    Seconds iterationDuration { 0_s };
    Seconds activeDuration { 0_s };
    Seconds endTime { 0_s };

    void updateComputedProperties();
    BasicEffectTiming getBasicTiming(std::optional<Seconds> localTime, double playbackRate) const;
    ResolvedEffectTiming resolve(std::optional<Seconds> localTime, double playbackRate) const;
};

} // namespace WebCore
