/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleTransition.h"

#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

Transition::Data::Data()
    : Data { Transition::initialProperty() }
{
}

Transition::Data::Data(SingleTransitionProperty&& property)
    : m_property { WTFMove(property) }
    , m_delay { Transition::initialDelay() }
    , m_duration { Transition::initialDuration() }
    , m_timingFunction { Transition::initialTimingFunction() }
    , m_behavior { static_cast<bool>(Transition::initialBehavior()) }
    , m_propertySet { false }
    , m_delaySet { false }
    , m_durationSet { false }
    , m_timingFunctionSet { false }
    , m_behaviorSet { false }
    , m_propertyFilled { false }
    , m_delayFilled { false }
    , m_durationFilled { false }
    , m_timingFunctionFilled { false }
    , m_behaviorFilled { false }
{
}

Transition::Data::Data(const Data& other)
    : RefCounted<Data>()
    , m_property { other.m_property }
    , m_delay { other.m_delay }
    , m_duration { other.m_duration }
    , m_timingFunction { other.m_timingFunction }
    , m_behavior { other.m_behavior }
    , m_propertySet { other.m_propertySet }
    , m_delaySet { other.m_delaySet }
    , m_durationSet { other.m_durationSet }
    , m_timingFunctionSet { other.m_timingFunctionSet }
    , m_behaviorSet { other.m_behaviorSet }
    , m_propertyFilled { other.m_propertyFilled }
    , m_delayFilled { other.m_delayFilled }
    , m_durationFilled { other.m_durationFilled }
    , m_timingFunctionFilled { other.m_timingFunctionFilled }
    , m_behaviorFilled { other.m_behaviorFilled }
{
}

bool Transition::Data::operator==(const Data& other) const
{
    return m_property == other.m_property
        && m_delay == other.m_delay
        && m_duration == other.m_duration
        && m_timingFunction == other.m_timingFunction
        && m_behavior == other.m_behavior
        && m_propertySet == other.m_propertySet
        && m_delaySet == other.m_delaySet
        && m_durationSet == other.m_durationSet
        && m_timingFunctionSet == other.m_timingFunctionSet
        && m_behaviorSet == other.m_behaviorSet;
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const Transition& transition)
{
    ts.dumpProperty("property"_s, transition.property());
    ts.dumpProperty("delay"_s, transition.delay());
    ts.dumpProperty("duration"_s, transition.duration());
    ts.dumpProperty("timing function"_s, transition.timingFunction());
    ts.dumpProperty("behavior"_s, transition.behavior());

    return ts;
}

} // namespace Style
} // namespace WebCore
