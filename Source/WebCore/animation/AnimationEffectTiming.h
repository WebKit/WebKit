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

#pragma once

#include "ExceptionOr.h"
#include "FillMode.h"
#include "PlaybackDirection.h"
#include "TimingFunction.h"
#include "WebAnimationUtilities.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/Variant.h>

namespace WebCore {

class AnimationEffectTiming final : public RefCounted<AnimationEffectTiming> {
public:
    static Ref<AnimationEffectTiming> create();
    ~AnimationEffectTiming();

    double bindingsDelay() const { return secondsToWebAnimationsAPITime(m_delay); }
    void setBindingsDelay(double delay) { m_delay = Seconds::fromMilliseconds(delay); }
    Seconds delay() const { return m_delay; }
    void setDelay(Seconds& delay) { m_delay = delay; }

    double bindingsEndDelay() const { return secondsToWebAnimationsAPITime(m_endDelay); }
    void setBindingsEndDelay(double endDelay) { m_endDelay = Seconds::fromMilliseconds(endDelay); }
    Seconds endDelay() const { return m_endDelay; }
    void setEndDelay(Seconds& endDelay) { m_endDelay = endDelay; }

    FillMode fill() const { return m_fill; }
    void setFill(FillMode fill) { m_fill = fill; }

    double iterationStart() const { return m_iterationStart; }
    ExceptionOr<void> setIterationStart(double);

    double iterations() const { return m_iterations; }
    ExceptionOr<void> setIterations(double);

    Variant<double, String> bindingsDuration() const;
    ExceptionOr<void> setBindingsDuration(Variant<double, String>&&);
    Seconds iterationDuration() const { return m_iterationDuration; }
    void setIterationDuration(Seconds& duration) { m_iterationDuration = duration; }

    PlaybackDirection direction() const { return m_direction; }
    void setDirection(PlaybackDirection direction) { m_direction = direction; }

    String easing() const { return m_timingFunction->cssText(); }
    ExceptionOr<void> setEasing(const String&);
    TimingFunction* timingFunction() const { return m_timingFunction.get(); }

    Seconds endTime() const;
    Seconds activeDuration() const;

private:
    AnimationEffectTiming();
    Seconds m_delay { 0_s };
    Seconds m_endDelay { 0_s };
    FillMode m_fill { FillMode::Auto };
    double m_iterationStart { 0 };
    double m_iterations { 1 };
    Seconds m_iterationDuration { 0_s };
    PlaybackDirection m_direction { PlaybackDirection::Normal };
    RefPtr<TimingFunction> m_timingFunction;
};

} // namespace WebCore
