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
#include "StyleAnimation.h"

#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

Animation::Animation()
    : m_data { Data::create() }
{
}

Animation::Animation(SingleAnimationName&& name)
    : Animation { }
{
    setName(WTF::move(name));
}

Animation::Data::Data()
    : m_name { Animation::initialName() }
    , m_delay { Animation::initialDelay() }
    , m_duration { Animation::initialDuration() }
    , m_iterationCount { Animation::initialIterationCount() }
    , m_timeline { Animation::initialTimeline() }
    , m_timingFunction { Animation::initialTimingFunction() }
    , m_defaultTimingFunctionForKeyframes { std::nullopt }
    , m_rangeStart { Animation::initialRangeStart() }
    , m_rangeEnd { Animation::initialRangeEnd() }
    , m_direction { static_cast<unsigned>(Animation::initialDirection()) }
    , m_fillMode { static_cast<unsigned>(Animation::initialFillMode()) }
    , m_playState { static_cast<unsigned>(Animation::initialPlayState()) }
    , m_compositeOperation { static_cast<unsigned>(Animation::initialCompositeOperation()) }
{
}

Animation::Data::Data(const Data& other)
    : RefCounted<Data>()
    , m_name { other.m_name }
    , m_delay { other.m_delay }
    , m_duration { other.m_duration }
    , m_iterationCount { other.m_iterationCount }
    , m_timeline { other.m_timeline }
    , m_timingFunction { other.m_timingFunction }
    , m_defaultTimingFunctionForKeyframes { other.m_defaultTimingFunctionForKeyframes }
    , m_rangeStart { other.m_rangeStart }
    , m_rangeEnd { other.m_rangeEnd }
    , m_direction { other.m_direction }
    , m_fillMode { other.m_fillMode }
    , m_playState { other.m_playState }
    , m_compositeOperation { other.m_compositeOperation }
    , m_nameState { other.m_nameState }
    , m_timelineState { other.m_timelineState }
    , m_timingFunctionState { other.m_timingFunctionState }
    , m_rangeStartState { other.m_rangeStartState }
    , m_rangeEndState { other.m_rangeEndState }
    , m_delayState { other.m_delayState }
    , m_durationState { other.m_durationState }
    , m_iterationCountState { other.m_iterationCountState }
    , m_directionState { other.m_directionState }
    , m_fillModeState { other.m_fillModeState }
    , m_playStateState { other.m_playStateState }
    , m_compositeOperationState { other.m_compositeOperationState }
{
}

bool Animation::Data::operator==(const Data& other) const
{
    return m_name == other.m_name
        && m_delay == other.m_delay
        && m_direction == other.m_direction
        && m_duration == other.m_duration
        && m_fillMode == other.m_fillMode
        && m_iterationCount == other.m_iterationCount
        && m_playState == other.m_playState
        && m_timeline == other.m_timeline
        && m_timingFunction == other.m_timingFunction
        && m_compositeOperation == other.m_compositeOperation
        && m_rangeStart == other.m_rangeStart
        && m_rangeEnd == other.m_rangeEnd
        && m_nameState == other.m_nameState
        && m_delayState == other.m_delayState
        && m_directionState == other.m_directionState
        && m_durationState == other.m_durationState
        && m_fillModeState == other.m_fillModeState
        && m_playStateState == other.m_playStateState
        && m_iterationCountState == other.m_iterationCountState
        && m_timelineState == other.m_timelineState
        && m_timingFunctionState == other.m_timingFunctionState
        && m_compositeOperationState == other.m_compositeOperationState
        && m_rangeStartState == other.m_rangeStartState
        && m_rangeEndState == other.m_rangeEndState;
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const Animation& animation)
{
    ts.dumpProperty("name"_s, animation.name());
    ts.dumpProperty("delay"_s, animation.delay());
    ts.dumpProperty("direction"_s, animation.direction());
    ts.dumpProperty("duration"_s, animation.duration());
    ts.dumpProperty("fill-mode"_s, animation.fillMode());
    ts.dumpProperty("iteration count"_s, animation.iterationCount());
    ts.dumpProperty("play-state"_s, animation.playState());
    ts.dumpProperty("timeline"_s, animation.timeline());
    ts.dumpProperty("timing-function"_s, animation.timingFunction());
    ts.dumpProperty("composite-operation"_s, animation.compositeOperation());
    ts.dumpProperty("range-start"_s, animation.rangeStart());
    ts.dumpProperty("range-end"_s, animation.rangeEnd());

    return ts;
}

} // namespace Style
} // namespace WebCore
