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

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/CompositeOperation.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleCoordinatedValueListValue.h>
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

// macro(ownerType, property, type, lowercaseName, uppercaseName)

#define FOR_EACH_ANIMATION_REFERENCE(macro) \
    macro(Animation, AnimationName, SingleAnimationName, name, Name) \
    macro(Animation, AnimationTimeline, SingleAnimationTimeline, timeline, Timeline) \
    macro(Animation, AnimationTimingFunction, EasingFunction, timingFunction, TimingFunction) \
    macro(Animation, AnimationRangeStart, SingleAnimationRangeStart, rangeStart, RangeStart) \
    macro(Animation, AnimationRangeEnd, SingleAnimationRangeEnd, rangeEnd, RangeEnd) \
\

#define FOR_EACH_ANIMATION_VALUE(macro) \
    macro(Animation, AnimationDelay, SingleAnimationDelay, delay, Delay) \
    macro(Animation, AnimationDuration, SingleAnimationDuration, duration, Duration) \
    macro(Animation, AnimationIterationCount, SingleAnimationIterationCount, iterationCount, IterationCount) \
\

#define FOR_EACH_ANIMATION_ENUM(macro) \
    macro(Animation, AnimationDirection, AnimationDirection, direction, Direction) \
    macro(Animation, AnimationFillMode, AnimationFillMode, fillMode, FillMode) \
    macro(Animation, AnimationPlayState, AnimationPlayState, playState, PlayState) \
    macro(Animation, AnimationComposition, CompositeOperation, compositeOperation, CompositeOperation) \
\

#define FOR_EACH_ANIMATION_SHORTHAND(macro) \
    macro(Animation, AnimationRange, SingleAnimationRange, range, Range) \
\

#define FOR_EACH_ANIMATION_PROPERTY(macro) \
    FOR_EACH_ANIMATION_REFERENCE(macro) \
    FOR_EACH_ANIMATION_VALUE(macro) \
    FOR_EACH_ANIMATION_ENUM(macro) \
\

struct Animation {
    Animation();
    Animation(SingleAnimationName&&);

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
    const SingleAnimationRangeStart& rangeStart() const { return m_data->m_rangeStart; }
    const SingleAnimationRangeEnd& rangeEnd() const { return m_data->m_rangeEnd; }

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

    const std::optional<EasingFunction>& defaultTimingFunctionForKeyframes() const { return m_data->m_defaultTimingFunctionForKeyframes; }
    void setDefaultTimingFunctionForKeyframes(std::optional<EasingFunction>&& function) { m_data->m_defaultTimingFunctionForKeyframes = WTF::move(function); }

    FOR_EACH_ANIMATION_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_REFERENCE)
    FOR_EACH_ANIMATION_VALUE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_VALUE)
    FOR_EACH_ANIMATION_ENUM(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_ENUM)

    // Support for the `animation-range` shorthand.
    static SingleAnimationRange initialRange() { return { initialRangeStart(), initialRangeEnd() }; }
    SingleAnimationRange range() const { return { rangeStart(), rangeEnd() }; }
    void setRange(SingleAnimationRange&& range) { setRangeStart(WTF::move(range.start)); setRangeEnd(WTF::move(range.end)); }
    void fillRange(SingleAnimationRange&& range) { fillRangeStart(WTF::move(range.start)); fillRangeEnd(WTF::move(range.end)); }
    void clearRange() { clearRangeStart(); clearRangeEnd(); }
    bool isRangeUnset() const { return isRangeStartUnset() && isRangeEndUnset(); }
    bool isRangeSet() const { return isRangeStartSet() || isRangeEndSet(); }
    bool isRangeFilled() const { return isRangeStartFilled() || isRangeEndFilled(); }

    // Used for animation composite order sorting needed when backing a CSSAnimation object.
    // https://drafts.csswg.org/css-animations-2/#animation-composite-order
    uint64_t sortingIdentity() const { return reinterpret_cast<uint64_t>(m_data.ptr()); }

    bool operator==(const Animation& other) const
    {
        return arePointingToEqualData(m_data, other.m_data);
    }

    // CoordinatedValueList interface.

    static constexpr auto computedValueUsesUsedValues = false;
    static constexpr auto baseProperty = PropertyNameConstant<CSSPropertyAnimationName> { };
    static constexpr auto properties = std::tuple { FOR_EACH_ANIMATION_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_PROPERTY) };
    static Animation clone(const Animation& other) { return Animation { Data::create(other.m_data) }; }
    bool isInitial() const { return m_data->m_name.isNone(); }

private:
    struct Data : RefCounted<Data> {
        static Ref<Data> create() { return adoptRef(*new Data()); }
        static Ref<Data> create(const Data& other) { return adoptRef(*new Data(other)); }

        Data();
        Data(const Data&);

        bool operator==(const Data&) const;

        SingleAnimationName m_name;
        SingleAnimationDelay m_delay;
        SingleAnimationDuration m_duration;
        SingleAnimationIterationCount m_iterationCount;
        SingleAnimationTimeline m_timeline;
        EasingFunction m_timingFunction;
        std::optional<EasingFunction> m_defaultTimingFunctionForKeyframes;
        SingleAnimationRangeStart m_rangeStart;
        SingleAnimationRangeEnd m_rangeEnd;
        PREFERRED_TYPE(AnimationDirection) unsigned m_direction : 2;
        PREFERRED_TYPE(AnimationFillMode) unsigned m_fillMode : 2;
        PREFERRED_TYPE(AnimationPlayState) unsigned m_playState : 2;
        PREFERRED_TYPE(CompositeOperation) unsigned m_compositeOperation : 2;

        FOR_EACH_ANIMATION_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_IS_SET_AND_IS_FILLED_MEMBERS)
    };

    // Needed by macros to access members.
    Data& data() { return m_data.get(); }
    const Data& data() const { return m_data.get(); }

    Animation(Ref<Data>&& data)
        : m_data { WTF::move(data) }
    {
    }

    Ref<Data> m_data;
};

FOR_EACH_ANIMATION_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_REFERENCE)
FOR_EACH_ANIMATION_VALUE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_VALUE)
FOR_EACH_ANIMATION_ENUM(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_ENUM)
FOR_EACH_ANIMATION_SHORTHAND(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_SHORTHAND)

// MARK: - Logging

TextStream& operator<<(TextStream&, const Animation&);

#undef FOR_EACH_ANIMATION_REFERENCE
#undef FOR_EACH_ANIMATION_VALUE
#undef FOR_EACH_ANIMATION_ENUM
#undef FOR_EACH_ANIMATION_SHORTHAND
#undef FOR_EACH_ANIMATION_PROPERTY

} // namespace Style
} // namespace WebCore
