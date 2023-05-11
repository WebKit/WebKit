/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "CSSPropertyNames.h"
#include "CompositeOperation.h"
#include "RenderStyleConstants.h"
#include "StyleScopeOrdinal.h"
#include "TimingFunction.h"
#include "WebAnimationTypes.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class Animation : public RefCounted<Animation> {
public:
    WEBCORE_EXPORT ~Animation();

    static Ref<Animation> create() { return adoptRef(*new Animation); }
    static Ref<Animation> create(const Animation& other) { return adoptRef(*new Animation(other)); }

    bool isDelaySet() const { return m_setProperties.contains(SetOrFilledProperty::Delay); }
    bool isDirectionSet() const { return m_setProperties.contains(SetOrFilledProperty::Direction); }
    bool isDurationSet() const { return m_setProperties.contains(SetOrFilledProperty::Duration); }
    bool isFillModeSet() const { return m_setProperties.contains(SetOrFilledProperty::FillMode); }
    bool isIterationCountSet() const { return m_setProperties.contains(SetOrFilledProperty::IterationCount); }
    bool isNameSet() const { return m_setProperties.contains(SetOrFilledProperty::Name); }
    bool isPlayStateSet() const { return m_setProperties.contains(SetOrFilledProperty::PlayState); }
    bool isPropertySet() const { return m_setProperties.contains(SetOrFilledProperty::Property); }
    bool isTimingFunctionSet() const { return m_setProperties.contains(SetOrFilledProperty::TimingFunction); }
    bool isCompositeOperationSet() const { return m_setProperties.contains(SetOrFilledProperty::CompositeOperation); }

    // Flags this to be the special "none" animation (animation-name: none)
    bool isNoneAnimation() const { return m_isNone; }

    // We can make placeholder Animation objects to keep the comma-separated lists
    // of properties in sync. isValidAnimation means this is not a placeholder.
    bool isValidAnimation() const { return !m_isNone && !m_name.string.isEmpty(); }

    bool isEmpty() const
    {
        return !isDirectionSet() && !isDurationSet() && !isFillModeSet()
            && !isNameSet() && !isPlayStateSet() && !isIterationCountSet()
            && !isDelaySet() && !isTimingFunctionSet() && !isPropertySet()
            && !m_isNone && !isCompositeOperationSet();
    }

    bool isEmptyOrZeroDuration() const
    {
        return isEmpty() || (m_duration == 0 && m_delay <= 0);
    }

    void clearDelay() { m_setProperties.remove(SetOrFilledProperty::Delay); m_filledProperties.remove(SetOrFilledProperty::Delay); }
    void clearDirection() { m_setProperties.remove(SetOrFilledProperty::Direction); m_filledProperties.remove(SetOrFilledProperty::Direction); }
    void clearDuration() { m_setProperties.remove(SetOrFilledProperty::Duration); m_filledProperties.remove(SetOrFilledProperty::Duration); }
    void clearFillMode() { m_setProperties.remove(SetOrFilledProperty::FillMode); m_filledProperties.remove(SetOrFilledProperty::FillMode); }
    void clearIterationCount() { m_setProperties.remove(SetOrFilledProperty::IterationCount); m_filledProperties.remove(SetOrFilledProperty::IterationCount); }
    void clearName() { m_setProperties.remove(SetOrFilledProperty::Name); m_filledProperties.remove(SetOrFilledProperty::Name); }
    void clearPlayState() { m_setProperties.remove(SetOrFilledProperty::PlayState); m_filledProperties.remove(SetOrFilledProperty::PlayState); }
    void clearProperty() { m_setProperties.remove(SetOrFilledProperty::Property); m_filledProperties.remove(SetOrFilledProperty::Property); }
    void clearTimingFunction() { m_setProperties.remove(SetOrFilledProperty::TimingFunction); m_filledProperties.remove(SetOrFilledProperty::TimingFunction); }
    void clearCompositeOperation() { m_setProperties.remove(SetOrFilledProperty::CompositeOperation); m_filledProperties.remove(SetOrFilledProperty::CompositeOperation); }

    void clearAll()
    {
        m_setProperties = { };
        m_filledProperties = { };
    }

    double delay() const { return m_delay; }

    enum class TransitionMode : uint8_t {
        All,
        None,
        SingleProperty,
        UnknownProperty
    };

    struct TransitionProperty {
        TransitionMode mode;
        AnimatableProperty animatableProperty;
    };

    enum class Direction : uint8_t {
        Normal,
        Alternate,
        Reverse,
        AlternateReverse
    };

    Direction direction() const { return static_cast<Direction>(m_direction); }
    bool directionIsForwards() const { return direction() == Direction::Normal || direction() == Direction::Alternate; }

    AnimationFillMode fillMode() const { return static_cast<AnimationFillMode>(m_fillMode); }

    double duration() const { return m_duration; }
    double playbackRate() const { return m_playbackRate; }

    struct Name {
        String string;
        bool isIdentifier { false };
    };

    static constexpr double IterationCountInfinite = -1;
    double iterationCount() const { return m_iterationCount; }
    const Name& name() const { return m_name; }
    Style::ScopeOrdinal nameStyleScopeOrdinal() const { return m_nameStyleScopeOrdinal; }
    AnimationPlayState playState() const { return static_cast<AnimationPlayState>(m_playState); }
    TransitionProperty property() const { return m_property; }
    TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    TimingFunction* defaultTimingFunctionForKeyframes() const { return m_defaultTimingFunctionForKeyframes.get(); }

    void setDelay(double c) { m_delay = c; m_setProperties.add(SetOrFilledProperty::Delay); }
    void setDirection(Direction d) { m_direction = d; m_setProperties.add(SetOrFilledProperty::Direction); }
    void setDuration(double d) { ASSERT(d >= 0); m_duration = d; m_setProperties.add(SetOrFilledProperty::Duration); }
    void setPlaybackRate(double d) { m_playbackRate = d; }
    void setFillMode(AnimationFillMode f) { m_fillMode = static_cast<unsigned>(f); m_setProperties.add(SetOrFilledProperty::FillMode); }
    void setIterationCount(double c) { m_iterationCount = c; m_setProperties.add(SetOrFilledProperty::IterationCount); }
    void setName(const Name& name, Style::ScopeOrdinal scope = Style::ScopeOrdinal::Element)
    {
        m_name = name;
        m_nameStyleScopeOrdinal = scope;
        m_setProperties.add(SetOrFilledProperty::Name);
    }
    void setPlayState(AnimationPlayState d) { m_playState = static_cast<unsigned>(d); m_setProperties.add(SetOrFilledProperty::PlayState); }
    void setProperty(TransitionProperty t) { m_property = t; m_setProperties.add(SetOrFilledProperty::Property); }
    void setTimingFunction(RefPtr<TimingFunction>&& function) { m_timingFunction = WTFMove(function); m_setProperties.add(SetOrFilledProperty::TimingFunction); }
    void setDefaultTimingFunctionForKeyframes(RefPtr<TimingFunction>&& function) { m_defaultTimingFunctionForKeyframes = WTFMove(function); }

    void setIsNoneAnimation(bool n) { m_isNone = n; }

    void fillDelay(double delay) { setDelay(delay); m_filledProperties.add(SetOrFilledProperty::Delay); }
    void fillDirection(Direction direction) { setDirection(direction); m_filledProperties.add(SetOrFilledProperty::Direction); }
    void fillDuration(double duration) { setDuration(duration); m_filledProperties.add(SetOrFilledProperty::Duration); }
    void fillFillMode(AnimationFillMode fillMode) { setFillMode(fillMode); m_filledProperties.add(SetOrFilledProperty::FillMode); }
    void fillIterationCount(double iterationCount) { setIterationCount(iterationCount); m_filledProperties.add(SetOrFilledProperty::IterationCount); }
    void fillPlayState(AnimationPlayState playState) { setPlayState(playState); m_filledProperties.add(SetOrFilledProperty::PlayState); }
    void fillProperty(TransitionProperty property) { setProperty(property); m_filledProperties.add(SetOrFilledProperty::Property); }
    void fillTimingFunction(RefPtr<TimingFunction>&& timingFunction) { setTimingFunction(WTFMove(timingFunction)); m_filledProperties.add(SetOrFilledProperty::TimingFunction); }
    void fillCompositeOperation(CompositeOperation compositeOperation) { setCompositeOperation(compositeOperation); m_filledProperties.add(SetOrFilledProperty::CompositeOperation); }

    bool isDelayFilled() const { return m_filledProperties.contains(SetOrFilledProperty::Delay); }
    bool isDirectionFilled() const { return m_filledProperties.contains(SetOrFilledProperty::Direction); }
    bool isDurationFilled() const { return m_filledProperties.contains(SetOrFilledProperty::Duration); }
    bool isFillModeFilled() const { return m_filledProperties.contains(SetOrFilledProperty::FillMode); }
    bool isIterationCountFilled() const { return m_filledProperties.contains(SetOrFilledProperty::IterationCount); }
    bool isPlayStateFilled() const { return m_filledProperties.contains(SetOrFilledProperty::PlayState); }
    bool isPropertyFilled() const { return m_filledProperties.contains(SetOrFilledProperty::Property); }
    bool isTimingFunctionFilled() const { return m_filledProperties.contains(SetOrFilledProperty::TimingFunction); }
    bool isCompositeOperationFilled() const { return m_filledProperties.contains(SetOrFilledProperty::CompositeOperation); }

    // return true if all members of this class match (excluding m_next)
    bool animationsMatch(const Animation&, bool matchProperties = true) const;

    // return true every Animation in the chain (defined by m_next) match 
    bool operator==(const Animation& o) const { return animationsMatch(o); }

    bool fillsBackwards() const { return isFillModeSet() && (fillMode() == AnimationFillMode::Backwards || fillMode() == AnimationFillMode::Both); }
    bool fillsForwards() const { return isFillModeSet() && (fillMode() == AnimationFillMode::Forwards || fillMode() == AnimationFillMode::Both); }

    CompositeOperation compositeOperation() const { return static_cast<CompositeOperation>(m_compositeOperation); }
    void setCompositeOperation(CompositeOperation op) { m_compositeOperation = static_cast<unsigned>(op); m_setProperties.add(SetOrFilledProperty::CompositeOperation); }

private:
    WEBCORE_EXPORT Animation();
    Animation(const Animation&);

    enum class SetOrFilledProperty : uint16_t {
        Delay = 1 << 0,
        Direction = 1 << 1,
        Duration = 1 << 2,
        FillMode = 1 << 3,
        IterationCount = 1 << 4,
        Name = 1 << 5,
        PlayState = 1 << 6,
        Property = 1 << 7,
        TimingFunction = 1 << 8,
        CompositeOperation = 1 << 9
    };

    // Packs with m_refCount from the base class.
    TransitionProperty m_property { TransitionMode::All, CSSPropertyInvalid };

    Name m_name;
    double m_iterationCount;
    double m_delay;
    double m_duration;
    double m_playbackRate { 1 };
    RefPtr<TimingFunction> m_timingFunction;
    RefPtr<TimingFunction> m_defaultTimingFunctionForKeyframes;

    Style::ScopeOrdinal m_nameStyleScopeOrdinal { Style::ScopeOrdinal::Element };

    OptionSet<SetOrFilledProperty> m_setProperties { };
    OptionSet<SetOrFilledProperty> m_filledProperties { };

    unsigned m_direction : 2; // Direction
    unsigned m_fillMode : 2; // AnimationFillMode
    unsigned m_playState : 2; // AnimationPlayState
    unsigned m_compositeOperation : 2; // CompositeOperation

    bool m_isNone : 1;

public:
    static double initialDelay() { return 0; }
    static Direction initialDirection() { return Direction::Normal; }
    static double initialDuration() { return 0; }
    static AnimationFillMode initialFillMode() { return AnimationFillMode::None; }
    static double initialIterationCount() { return 1.0; }
    static const Name& initialName();
    static AnimationPlayState initialPlayState() { return AnimationPlayState::Playing; }
    static CompositeOperation initialCompositeOperation() { return CompositeOperation::Replace; }
    static TransitionProperty initialProperty() { return { TransitionMode::All, CSSPropertyInvalid }; }
    static Ref<TimingFunction> initialTimingFunction() { return CubicBezierTimingFunction::create(); }
};

WTF::TextStream& operator<<(WTF::TextStream&, AnimationPlayState);
WTF::TextStream& operator<<(WTF::TextStream&, Animation::TransitionProperty);
WTF::TextStream& operator<<(WTF::TextStream&, Animation::Direction);
WTF::TextStream& operator<<(WTF::TextStream&, const Animation&);

} // namespace WebCore
