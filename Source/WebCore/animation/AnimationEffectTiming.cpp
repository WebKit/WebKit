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
#include "AnimationEffectTiming.h"

namespace WebCore {

Ref<AnimationEffectTiming> AnimationEffectTiming::create()
{
    return adoptRef(*new AnimationEffectTiming());
}

AnimationEffectTiming::AnimationEffectTiming()
    : m_duration(0)
{
}

AnimationEffectTiming::~AnimationEffectTiming()
{
}

ExceptionOr<void> AnimationEffectTiming::setBindingsDuration(Variant<double, String>&& duration)
{
    // https://drafts.csswg.org/web-animations-1/#dom-animationeffecttimingreadonly-duration
    // The iteration duration which is a real number greater than or equal to zero (including positive infinity)
    // representing the time taken to complete a single iteration of the animation effect.
    // In this level of this specification, the string value auto is equivalent to zero. This is a forwards-compatiblity
    // measure since a future level of this specification will introduce group effects where the auto value expands to
    // include the duration of the child effects.

    if (WTF::holds_alternative<double>(duration)) {
        auto durationAsDouble = WTF::get<double>(duration);
        if (durationAsDouble < 0 || std::isnan(durationAsDouble))
            return Exception { TypeError };
        m_duration = Seconds::fromMilliseconds(durationAsDouble);
        return { };
    }

    if (WTF::get<String>(duration) != "auto")
        return Exception { TypeError };

    m_duration = 0_s;

    return { };
}


} // namespace WebCore
