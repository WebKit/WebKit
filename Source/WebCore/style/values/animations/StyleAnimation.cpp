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

Animation::Data::Data()
    : Data { SingleAnimationName { Animation::initialName() } }
{
}

Animation::Data::Data(SingleAnimationName&& name)
    : m_name { WTFMove(name) }
    , m_delay { Animation::initialDelay() }
    , m_duration { Animation::initialDuration() }
    , m_iterationCount { Animation::initialIterationCount() }
    , m_timeline { Animation::initialTimeline() }
    , m_timingFunction { Animation::initialTimingFunction() }
    , m_defaultTimingFunctionForKeyframes { std::nullopt }
    , m_range { Animation::initialRange() }
    , m_direction { static_cast<unsigned>(Animation::initialDirection()) }
    , m_fillMode { static_cast<unsigned>(Animation::initialFillMode()) }
    , m_playState { static_cast<unsigned>(Animation::initialPlayState()) }
    , m_compositeOperation { static_cast<unsigned>(Animation::initialCompositeOperation()) }
    , m_nameSet { false }
    , m_delaySet { false }
    , m_directionSet { false }
    , m_durationSet { false }
    , m_fillModeSet { false }
    , m_iterationCountSet { false }
    , m_playStateSet { false }
    , m_timelineSet { false }
    , m_timingFunctionSet { false }
    , m_compositeOperationSet { false }
    , m_rangeStartSet { false }
    , m_rangeEndSet { false }
    , m_delayFilled { false }
    , m_directionFilled { false }
    , m_durationFilled { false }
    , m_fillModeFilled { false }
    , m_iterationCountFilled { false }
    , m_playStateFilled { false }
    , m_timelineFilled { false }
    , m_timingFunctionFilled { false }
    , m_compositeOperationFilled { false }
    , m_rangeStartFilled { false }
    , m_rangeEndFilled { false }
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
    , m_range { other.m_range }
    , m_direction { other.m_direction }
    , m_fillMode { other.m_fillMode }
    , m_playState { other.m_playState }
    , m_compositeOperation { other.m_compositeOperation }
    , m_nameSet { other.m_nameSet }
    , m_delaySet { other.m_delaySet }
    , m_directionSet { other.m_directionSet }
    , m_durationSet { other.m_durationSet }
    , m_fillModeSet { other.m_fillModeSet }
    , m_iterationCountSet { other.m_iterationCountSet }
    , m_playStateSet { other.m_playStateSet }
    , m_timelineSet { other.m_timelineSet }
    , m_timingFunctionSet { other.m_timingFunctionSet }
    , m_compositeOperationSet { other.m_compositeOperationSet }
    , m_rangeStartSet { other.m_rangeStartSet }
    , m_rangeEndSet { other.m_rangeEndSet }
    , m_delayFilled { other.m_delayFilled }
    , m_directionFilled { other.m_directionFilled }
    , m_durationFilled { other.m_durationFilled }
    , m_fillModeFilled { other.m_fillModeFilled }
    , m_iterationCountFilled { other.m_iterationCountFilled }
    , m_playStateFilled { other.m_playStateFilled }
    , m_timelineFilled { other.m_timelineFilled }
    , m_timingFunctionFilled { other.m_timingFunctionFilled }
    , m_compositeOperationFilled { other.m_compositeOperationFilled }
    , m_rangeStartFilled { other.m_rangeStartFilled }
    , m_rangeEndFilled { other.m_rangeEndFilled }
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
        && m_range == other.m_range
        && m_nameSet == other.m_nameSet
        && m_delaySet == other.m_delaySet
        && m_directionSet == other.m_directionSet
        && m_durationSet == other.m_durationSet
        && m_fillModeSet == other.m_fillModeSet
        && m_playStateSet == other.m_playStateSet
        && m_iterationCountSet == other.m_iterationCountSet
        && m_timelineSet == other.m_timelineSet
        && m_timingFunctionSet == other.m_timingFunctionSet
        && m_compositeOperationSet == other.m_compositeOperationSet
        && m_rangeStartSet == other.m_rangeStartSet
        && m_rangeEndSet == other.m_rangeEndSet;
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
