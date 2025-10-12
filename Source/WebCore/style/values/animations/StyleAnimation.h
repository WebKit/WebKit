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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CompositeOperation.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleEasingFunction.h>
#include <WebCore/StyleSingleAnimationDelay.h>
#include <WebCore/StyleSingleAnimationDuration.h>
#include <WebCore/StyleSingleAnimationIterationCount.h>
#include <WebCore/StyleSingleAnimationName.h>
#include <WebCore/StyleSingleAnimationRange.h>
#include <WebCore/StyleSingleAnimationTimeline.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

struct Animation {
    Animation() : m_data { Data::create() } { }
    Animation(SingleAnimationName&& name) : m_data { Data::create(WTFMove(name)) } { }

    static Animation clone(const Animation& other)
    {
        return Animation { Data::create(other.m_data) };
    }

    const SingleAnimationName& name() const { return m_data->m_name; }
    SingleAnimationDelay delay() const { return m_data->m_delay; }
    AnimationDirection direction() const { return static_cast<AnimationDirection>(m_data->m_direction); }
    SingleAnimationDuration duration() const { return m_data->m_duration; }
    AnimationFillMode fillMode() const { return static_cast<AnimationFillMode>(m_data->m_fillMode); }
    SingleAnimationIterationCount iterationCount() const { return m_data->m_iterationCount; }
    AnimationPlayState playState() const { return static_cast<AnimationPlayState>(m_data->m_playState); }
    const SingleAnimationTimeline& timeline() const { return m_data->m_timeline; }
    const EasingFunction& timingFunction() const { return m_data->m_timingFunction; }
    CompositeOperation compositeOperation() const { return static_cast<CompositeOperation>(m_data->m_compositeOperation); }
    const SingleAnimationRangeStart& rangeStart() const { return m_data->m_range.start; }
    const SingleAnimationRangeEnd& rangeEnd() const { return m_data->m_range.end; }
    const SingleAnimationRange& range() const { return m_data->m_range; }

    void setName(SingleAnimationName&& name) { m_data->m_name = WTFMove(name); m_data->m_nameSet = true; }
    void setDelay(SingleAnimationDelay delay) { m_data->m_delay = delay; m_data->m_delaySet = true; }
    void setDirection(AnimationDirection direction) { m_data->m_direction = static_cast<unsigned>(direction); m_data->m_directionSet = true; }
    void setDuration(SingleAnimationDuration duration) { m_data->m_duration = duration; m_data->m_durationSet = true; }
    void setFillMode(AnimationFillMode fillMode) { m_data->m_fillMode = static_cast<unsigned>(fillMode); m_data->m_fillModeSet = true; }
    void setIterationCount(SingleAnimationIterationCount iterationCount) { m_data->m_iterationCount = iterationCount; m_data->m_iterationCountSet = true; }
    void setPlayState(AnimationPlayState playState) { m_data->m_playState = static_cast<unsigned>(playState); m_data->m_playStateSet = true; }
    void setTimeline(SingleAnimationTimeline&& timeline) { m_data->m_timeline = WTFMove(timeline); m_data->m_timelineSet = true; }
    void setTimingFunction(EasingFunction&& function) { m_data->m_timingFunction = WTFMove(function); m_data->m_timingFunctionSet = true; }
    void setCompositeOperation(CompositeOperation compositeOperation) { m_data->m_compositeOperation = static_cast<unsigned>(compositeOperation); m_data->m_compositeOperationSet = true; }
    void setRangeStart(SingleAnimationRangeStart&& range) { m_data->m_range.start = WTFMove(range); m_data->m_rangeStartSet = true; }
    void setRangeEnd(SingleAnimationRangeEnd&& range) { m_data->m_range.end = WTFMove(range); m_data->m_rangeEndSet = true; }
    void setRange(SingleAnimationRange&& range) { setRangeStart(WTFMove(range.start)); setRangeEnd(WTFMove(range.end)); }

    void fillDelay(SingleAnimationDelay delay) { setDelay(delay); m_data->m_delayFilled = true; }
    void fillDirection(AnimationDirection direction) { setDirection(direction); m_data->m_directionFilled = true; }
    void fillDuration(SingleAnimationDuration duration) { setDuration(duration); m_data->m_durationFilled = true; }
    void fillFillMode(AnimationFillMode fillMode) { setFillMode(fillMode); m_data->m_fillModeFilled = true; }
    void fillIterationCount(SingleAnimationIterationCount iterationCount) { setIterationCount(iterationCount); m_data->m_iterationCountFilled = true; }
    void fillPlayState(AnimationPlayState playState) { setPlayState(playState); m_data->m_playStateFilled = true; }
    void fillTimeline(SingleAnimationTimeline&& timeline) { setTimeline(WTFMove(timeline)); m_data->m_timelineFilled = true; }
    void fillTimingFunction(EasingFunction&& function) { setTimingFunction(WTFMove(function)); m_data->m_timingFunctionFilled = true; }
    void fillCompositeOperation(CompositeOperation compositeOperation) { setCompositeOperation(compositeOperation); m_data->m_compositeOperationFilled = true; }
    void fillRangeStart(SingleAnimationRangeStart&& range) { m_data->m_range.start = WTFMove(range); m_data->m_rangeStartFilled = true; }
    void fillRangeEnd(SingleAnimationRangeEnd&& range) { m_data->m_range.end = WTFMove(range); m_data->m_rangeEndFilled = true; }
    void fillRange(SingleAnimationRange&& range) { fillRangeStart(WTFMove(range.start)); fillRangeEnd(WTFMove(range.end)); }

    void clearName() { m_data->m_nameSet = false; m_data->m_name = initialName(); }
    void clearDelay() { m_data->m_delaySet = false; m_data->m_delayFilled = false; }
    void clearDirection() { m_data->m_directionSet = false; m_data->m_directionFilled = false; }
    void clearDuration() { m_data->m_durationSet = false; m_data->m_durationFilled = false; }
    void clearFillMode() { m_data->m_fillModeSet = false; m_data->m_fillModeFilled = false; }
    void clearIterationCount() { m_data->m_iterationCountSet = false; m_data->m_iterationCountFilled = false; }
    void clearPlayState() { m_data->m_playStateSet = false; m_data->m_playStateFilled = false; }
    void clearTimeline() { m_data->m_timelineSet = false; m_data->m_timelineFilled = false; }
    void clearTimingFunction() { m_data->m_timingFunctionSet = false; m_data->m_timingFunctionFilled = false; }
    void clearCompositeOperation() { m_data->m_compositeOperationSet = false; m_data->m_compositeOperationFilled = false; }
    void clearRangeStart() { m_data->m_rangeStartSet = false; m_data->m_rangeStartFilled = false; }
    void clearRangeEnd() { m_data->m_rangeEndSet = false; m_data->m_rangeEndFilled = false; }
    void clearRange() { clearRangeStart(); clearRangeEnd(); }

    bool isNameSet() const { return m_data->m_nameSet; }
    bool isDelaySet() const { return m_data->m_delaySet; }
    bool isDirectionSet() const { return m_data->m_directionSet; }
    bool isDurationSet() const { return m_data->m_durationSet; }
    bool isFillModeSet() const { return m_data->m_fillModeSet; }
    bool isIterationCountSet() const { return m_data->m_iterationCountSet; }
    bool isPlayStateSet() const { return m_data->m_playStateSet; }
    bool isTimelineSet() const { return m_data->m_timelineSet; }
    bool isTimingFunctionSet() const { return m_data->m_timingFunctionSet; }
    bool isCompositeOperationSet() const { return m_data->m_compositeOperationSet; }
    bool isRangeStartSet() const { return m_data->m_rangeStartSet; }
    bool isRangeEndSet() const { return m_data->m_rangeEndSet; }
    bool isRangeSet() const { return isRangeStartSet() || isRangeEndSet(); }

    static constexpr bool isNameFilled() { return false; } // Needed for property generation generalization.
    bool isDelayFilled() const { return m_data->m_delayFilled; }
    bool isDirectionFilled() const { return m_data->m_directionFilled; }
    bool isDurationFilled() const { return m_data->m_durationFilled; }
    bool isFillModeFilled() const { return m_data->m_fillModeFilled; }
    bool isIterationCountFilled() const { return m_data->m_iterationCountFilled; }
    bool isPlayStateFilled() const { return m_data->m_playStateFilled; }
    bool isTimelineFilled() const { return m_data->m_timelineFilled; }
    bool isTimingFunctionFilled() const { return m_data->m_timingFunctionFilled; }
    bool isCompositeOperationFilled() const { return m_data->m_compositeOperationFilled; }
    bool isRangeStartFilled() const { return m_data->m_rangeStartFilled; }
    bool isRangeEndFilled() const { return m_data->m_rangeEndFilled; }
    bool isRangeFilled() const { return isRangeStartFilled() || isRangeEndFilled(); }

    static SingleAnimationName initialName() { return CSS::Keyword::None { }; }
    static SingleAnimationDelay initialDelay() { return 0; }
    static AnimationDirection initialDirection() { return AnimationDirection::Normal; }
    static SingleAnimationDuration initialDuration() { return CSS::Keyword::Auto { }; }
    static AnimationFillMode initialFillMode() { return AnimationFillMode::None; }
    static SingleAnimationIterationCount initialIterationCount() { return 1.0; }
    static AnimationPlayState initialPlayState() { return AnimationPlayState::Running; }
    static CompositeOperation initialCompositeOperation() { return CompositeOperation::Replace; }
    static SingleAnimationTimeline initialTimeline() { return CSS::Keyword::Auto { }; }
    static EasingFunction initialTimingFunction() { return EasingFunction { CubicBezierTimingFunction::create() }; }
    static SingleAnimationRangeStart initialRangeStart() { return CSS::Keyword::Normal { }; }
    static SingleAnimationRangeEnd initialRangeEnd() { return CSS::Keyword::Normal { }; }
    static SingleAnimationRange initialRange() { return { initialRangeStart(), initialRangeEnd() }; }

    const std::optional<EasingFunction>& defaultTimingFunctionForKeyframes() const { return m_data->m_defaultTimingFunctionForKeyframes; }
    void setDefaultTimingFunctionForKeyframes(std::optional<EasingFunction>&& function) { m_data->m_defaultTimingFunctionForKeyframes = WTFMove(function); }

    // CoordinatedValueList value functions.

    bool isEmpty() const
    {
        return !isNameSet()
            && (!isDirectionSet() || isDirectionFilled())
            && (!isDurationSet() || isDurationFilled())
            && (!isFillModeSet() || isFillModeFilled())
            && (!isPlayStateSet() || isPlayStateFilled())
            && (!isIterationCountSet() || isIterationCountFilled())
            && (!isDelaySet() || isDelayFilled())
            && (!isTimingFunctionSet() || isTimingFunctionFilled())
            && (!isCompositeOperationSet() || isCompositeOperationFilled())
            && (!isTimelineSet() || isTimelineFilled())
            && (!isRangeStartSet() || isRangeStartFilled())
            && (!isRangeEndSet() || isRangeEndFilled());
    }

    template<auto isSet, auto getter, auto setter, typename T>
    static void fillUnsetProperty(auto& list);
    static void fillUnsetProperties(auto& list);

    bool operator==(const Animation& other) const
    {
        return arePointingToEqualData(m_data, other.m_data);
    }

    // Used for animation composite order sorting needed when backing a CSSAnimation object.
    // https://drafts.csswg.org/css-animations-2/#animation-composite-order
    uint64_t sortingIdentity() const { return reinterpret_cast<uint64_t>(m_data.ptr()); }

private:
    struct Data : RefCounted<Data> {
        static Ref<Data> create() { return adoptRef(*new Data()); }
        static Ref<Data> create(const Data& other) { return adoptRef(*new Data(other)); }
        static Ref<Data> create(SingleAnimationName&& name) { return adoptRef(*new Data(WTFMove(name))); }

        Data();
        Data(const Data&);
        Data(SingleAnimationName&&);

        bool operator==(const Data&) const;

        SingleAnimationName m_name;
        SingleAnimationDelay m_delay;
        SingleAnimationDuration m_duration;
        SingleAnimationIterationCount m_iterationCount;
        SingleAnimationTimeline m_timeline;
        EasingFunction m_timingFunction;
        std::optional<EasingFunction> m_defaultTimingFunctionForKeyframes;
        SingleAnimationRange m_range;
        PREFERRED_TYPE(AnimationDirection) unsigned m_direction : 2;
        PREFERRED_TYPE(AnimationFillMode) unsigned m_fillMode : 2;
        PREFERRED_TYPE(AnimationPlayState) unsigned m_playState : 2;
        PREFERRED_TYPE(CompositeOperation) unsigned m_compositeOperation : 2;

        bool m_nameSet : 1;
        bool m_delaySet : 1;
        bool m_directionSet : 1;
        bool m_durationSet : 1;
        bool m_fillModeSet : 1;
        bool m_iterationCountSet : 1;
        bool m_playStateSet : 1;
        bool m_timelineSet : 1;
        bool m_timingFunctionSet : 1;
        bool m_compositeOperationSet : 1;
        bool m_rangeStartSet : 1;
        bool m_rangeEndSet : 1;

        bool m_delayFilled : 1;
        bool m_directionFilled : 1;
        bool m_durationFilled : 1;
        bool m_fillModeFilled : 1;
        bool m_iterationCountFilled : 1;
        bool m_playStateFilled : 1;
        bool m_timelineFilled : 1;
        bool m_timingFunctionFilled : 1;
        bool m_compositeOperationFilled : 1;
        bool m_rangeStartFilled : 1;
        bool m_rangeEndFilled : 1;
    };

    Animation(Ref<Data>&& data)
        : m_data { WTFMove(data) }
    {
    }

    Ref<Data> m_data;
};

template<auto isSet, auto getter, auto filler, typename T>
void Animation::fillUnsetProperty(auto& list)
{
    size_t i = 0;
    for (; i < list.size() && (list[i].*isSet)(); ++i) { }
    if (i) {
        for (size_t j = 0; i < list.size(); ++i, ++j)
            (list[i].*filler)(T { (list[j].*getter)() });
    }
}

void Animation::fillUnsetProperties(auto& list)
{
    fillUnsetProperty<&Animation::isDelaySet, &Animation::delay, &Animation::fillDelay, SingleAnimationDelay>(list);
    fillUnsetProperty<&Animation::isDirectionSet, &Animation::direction, &Animation::fillDirection, AnimationDirection>(list);
    fillUnsetProperty<&Animation::isDurationSet, &Animation::duration, &Animation::fillDuration, SingleAnimationDuration>(list);
    fillUnsetProperty<&Animation::isFillModeSet, &Animation::fillMode, &Animation::fillFillMode, AnimationFillMode>(list);
    fillUnsetProperty<&Animation::isIterationCountSet, &Animation::iterationCount, &Animation::fillIterationCount, SingleAnimationIterationCount>(list);
    fillUnsetProperty<&Animation::isPlayStateSet, &Animation::playState, &Animation::fillPlayState, AnimationPlayState>(list);
    fillUnsetProperty<&Animation::isTimelineSet, &Animation::timeline, &Animation::fillTimeline, SingleAnimationTimeline>(list);
    fillUnsetProperty<&Animation::isTimingFunctionSet, &Animation::timingFunction, &Animation::fillTimingFunction, EasingFunction>(list);
    fillUnsetProperty<&Animation::isCompositeOperationSet, &Animation::compositeOperation, &Animation::fillCompositeOperation, CompositeOperation>(list);
    fillUnsetProperty<&Animation::isRangeStartSet, &Animation::rangeStart, &Animation::fillRangeStart, SingleAnimationRangeStart>(list);
    fillUnsetProperty<&Animation::isRangeEndSet, &Animation::rangeEnd, &Animation::fillRangeEnd, SingleAnimationRangeEnd>(list);
}

// MARK: - Logging

TextStream& operator<<(TextStream&, const Animation&);

} // namespace Style
} // namespace WebCore
