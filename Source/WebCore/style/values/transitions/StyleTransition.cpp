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

Transition::Transition()
    : m_data { Data::create() }
{
}

Transition::Transition(SingleTransitionProperty&& property)
    : Transition { }
{
    setProperty(WTF::move(property));
}

Transition::Data::Data()
    : m_property { Transition::initialProperty() }
    , m_delay { Transition::initialDelay() }
    , m_duration { Transition::initialDuration() }
    , m_timingFunction { Transition::initialTimingFunction() }
    , m_behavior { static_cast<bool>(Transition::initialBehavior()) }
{
}

Transition::Data::Data(const Data& other)
    : RefCounted<Data>()
    , m_property { other.m_property }
    , m_delay { other.m_delay }
    , m_duration { other.m_duration }
    , m_timingFunction { other.m_timingFunction }
    , m_behavior { other.m_behavior }
    , m_propertyState { other.m_propertyState }
    , m_timingFunctionState { other.m_timingFunctionState }
    , m_delayState { other.m_delayState }
    , m_durationState { other.m_durationState }
    , m_behaviorState { other.m_behaviorState }
{
}

bool Transition::Data::operator==(const Data& other) const
{
    return m_property == other.m_property
        && m_delay == other.m_delay
        && m_duration == other.m_duration
        && m_timingFunction == other.m_timingFunction
        && m_behavior == other.m_behavior
        && m_propertyState == other.m_propertyState
        && m_delayState == other.m_delayState
        && m_durationState == other.m_durationState
        && m_timingFunctionState == other.m_timingFunctionState
        && m_behaviorState == other.m_behaviorState;
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
