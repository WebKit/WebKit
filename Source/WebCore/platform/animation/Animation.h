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

namespace WebCore {

class Animation : public RefCounted<Animation> {
public:
    WEBCORE_EXPORT ~Animation();

    static Ref<Animation> create() { return adoptRef(*new Animation); }
    static Ref<Animation> create(const Animation& other) { return adoptRef(*new Animation(other)); }

    bool isDelaySet() const { return m_delaySet; }
    bool isDirectionSet() const { return m_directionSet; }
    bool isDurationSet() const { return m_durationSet; }
    bool isFillModeSet() const { return m_fillModeSet; }
    bool isIterationCountSet() const { return m_iterationCountSet; }
    bool isNameSet() const { return m_nameSet; }
    bool isPlayStateSet() const { return m_playStateSet; }
    bool isPropertySet() const { return m_propertySet; }
    bool isTimingFunctionSet() const { return m_timingFunctionSet; }
    bool isCompositeOperationSet() const { return m_compositeOperationSet; }

    // Flags this to be the special "none" animation (animation-name: none)
    bool isNoneAnimation() const { return m_isNone; }

    // We can make placeholder Animation objects to keep the comma-separated lists
    // of properties in sync. isValidAnimation means this is not a placeholder.
    bool isValidAnimation() const { return !m_isNone && !m_name.string.isEmpty(); }

    bool isEmpty() const
    {
        return !m_directionSet && !m_durationSet && !m_fillModeSet
            && !m_nameSet && !m_playStateSet && !m_iterationCountSet
            && !m_delaySet && !m_timingFunctionSet && !m_propertySet
            && !m_isNone && !m_compositeOperationSet;
    }

    bool isEmptyOrZeroDuration() const
    {
        return isEmpty() || (m_duration == 0 && m_delay <= 0);
    }

    void clearDelay() { m_delaySet = false; m_delayFilled = false; }
    void clearDirection() { m_directionSet = false; m_directionFilled = false; }
    void clearDuration() { m_durationSet = false; m_durationFilled = false; }
    void clearFillMode() { m_fillModeSet = false; m_fillModeFilled = false; }
    void clearIterationCount() { m_iterationCountSet = false; m_iterationCountFilled = false; }
    void clearName() { m_nameSet = false; }
    void clearPlayState() { m_playStateSet = false; m_playStateFilled = false; }
    void clearProperty() { m_propertySet = false; m_propertyFilled = false; }
    void clearTimingFunction() { m_timingFunctionSet = false; m_timingFunctionFilled = false; }
    void clearCompositeOperation() { m_compositeOperationSet = false; m_compositeOperationFilled = false; }

    void clearAll()
    {
        clearDelay();
        clearDirection();
        clearDuration();
        clearFillMode();
        clearIterationCount();
        clearName();
        clearPlayState();
        clearProperty();
        clearTimingFunction();
        clearCompositeOperation();
    }

    double delay() const { return m_delay; }

    enum class TransitionMode : uint8_t {
        All,
        None,
        SingleProperty,
        CustomProperty,
        UnknownProperty
    };

    struct TransitionProperty {
        TransitionMode mode;
        CSSPropertyID id;
    };

    enum AnimationDirection {
        AnimationDirectionNormal,
        AnimationDirectionAlternate,
        AnimationDirectionReverse,
        AnimationDirectionAlternateReverse
    };

    AnimationDirection direction() const { return static_cast<AnimationDirection>(m_direction); }
    bool directionIsForwards() const { return m_direction == AnimationDirectionNormal || m_direction == AnimationDirectionAlternate; }

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
    const String& customOrUnknownProperty() const { return m_customOrUnknownProperty; }
    TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    TimingFunction* defaultTimingFunctionForKeyframes() const { return m_defaultTimingFunctionForKeyframes.get(); }

    void setDelay(double c) { m_delay = c; m_delaySet = true; }
    void setDirection(AnimationDirection d) { m_direction = d; m_directionSet = true; }
    void setDuration(double d) { ASSERT(d >= 0); m_duration = d; m_durationSet = true; }
    void setPlaybackRate(double d) { m_playbackRate = d; }
    void setFillMode(AnimationFillMode f) { m_fillMode = static_cast<unsigned>(f); m_fillModeSet = true; }
    void setIterationCount(double c) { m_iterationCount = c; m_iterationCountSet = true; }
    void setName(const Name& name, Style::ScopeOrdinal scope = Style::ScopeOrdinal::Element)
    {
        m_name = name;
        m_nameStyleScopeOrdinal = scope;
        m_nameSet = true;
    }
    void setPlayState(AnimationPlayState d) { m_playState = static_cast<unsigned>(d); m_playStateSet = true; }
    void setProperty(TransitionProperty t) { m_property = t; m_propertySet = true; }
    void setCustomOrUnknownProperty(const String& property) { m_customOrUnknownProperty = property; }
    void setTimingFunction(RefPtr<TimingFunction>&& function) { m_timingFunction = WTFMove(function); m_timingFunctionSet = true; }
    void setDefaultTimingFunctionForKeyframes(RefPtr<TimingFunction>&& function) { m_defaultTimingFunctionForKeyframes = WTFMove(function); }

    void setIsNoneAnimation(bool n) { m_isNone = n; }

    void fillDelay(double delay) { setDelay(delay); m_delayFilled = true; }
    void fillDirection(AnimationDirection direction) { setDirection(direction); m_directionFilled = true; }
    void fillDuration(double duration) { setDuration(duration); m_durationFilled = true; }
    void fillFillMode(AnimationFillMode fillMode) { setFillMode(fillMode); m_fillModeFilled = true; }
    void fillIterationCount(double iterationCount) { setIterationCount(iterationCount); m_iterationCountFilled = true; }
    void fillPlayState(AnimationPlayState playState) { setPlayState(playState); m_playStateFilled = true; }
    void fillProperty(TransitionProperty property) { setProperty(property); m_propertyFilled = true; }
    void fillTimingFunction(RefPtr<TimingFunction>&& timingFunction) { setTimingFunction(WTFMove(timingFunction)); m_timingFunctionFilled = true; }
    void fillCompositeOperation(CompositeOperation compositeOperation) { setCompositeOperation(compositeOperation); m_compositeOperationFilled = true; }

    bool isDelayFilled() const { return m_delayFilled; }
    bool isDirectionFilled() const { return m_directionFilled; }
    bool isDurationFilled() const { return m_durationFilled; }
    bool isFillModeFilled() const { return m_fillModeFilled; }
    bool isIterationCountFilled() const { return m_iterationCountFilled; }
    bool isPlayStateFilled() const { return m_playStateFilled; }
    bool isPropertyFilled() const { return m_propertyFilled; }
    bool isTimingFunctionFilled() const { return m_timingFunctionFilled; }
    bool isCompositeOperationFilled() const { return m_compositeOperationFilled; }

    // return true if all members of this class match (excluding m_next)
    bool animationsMatch(const Animation&, bool matchProperties = true) const;

    // return true every Animation in the chain (defined by m_next) match 
    bool operator==(const Animation& o) const { return animationsMatch(o); }
    bool operator!=(const Animation& o) const { return !(*this == o); }

    bool fillsBackwards() const { return m_fillModeSet && (fillMode() == AnimationFillMode::Backwards || fillMode() == AnimationFillMode::Both); }
    bool fillsForwards() const { return m_fillModeSet && (fillMode() == AnimationFillMode::Forwards || fillMode() == AnimationFillMode::Both); }

    CompositeOperation compositeOperation() const { return static_cast<CompositeOperation>(m_compositeOperation); }
    void setCompositeOperation(CompositeOperation op) { m_compositeOperation = static_cast<unsigned>(op); m_compositeOperationSet = true; }

private:
    WEBCORE_EXPORT Animation();
    Animation(const Animation&);
    
    // Packs with m_refCount from the base class.
    TransitionProperty m_property { TransitionMode::All, CSSPropertyInvalid };

    Name m_name;
    String m_customOrUnknownProperty;
    double m_iterationCount;
    double m_delay;
    double m_duration;
    double m_playbackRate { 1 };
    RefPtr<TimingFunction> m_timingFunction;
    RefPtr<TimingFunction> m_defaultTimingFunctionForKeyframes;

    Style::ScopeOrdinal m_nameStyleScopeOrdinal { Style::ScopeOrdinal::Element };

    unsigned m_direction : 2; // AnimationDirection
    unsigned m_fillMode : 2; // AnimationFillMode
    unsigned m_playState : 2; // AnimationPlayState
    unsigned m_compositeOperation : 2; // CompositeOperation

    bool m_delaySet : 1;
    bool m_directionSet : 1;
    bool m_durationSet : 1;
    bool m_fillModeSet : 1;
    bool m_iterationCountSet : 1;
    bool m_nameSet : 1;
    bool m_playStateSet : 1;
    bool m_propertySet : 1;
    bool m_timingFunctionSet : 1;
    bool m_compositeOperationSet : 1;

    bool m_isNone : 1;

    bool m_delayFilled : 1;
    bool m_directionFilled : 1;
    bool m_durationFilled : 1;
    bool m_fillModeFilled : 1;
    bool m_iterationCountFilled : 1;
    bool m_playStateFilled : 1;
    bool m_propertyFilled : 1;
    bool m_timingFunctionFilled : 1;
    bool m_compositeOperationFilled : 1;

public:
    static double initialDelay() { return 0; }
    static AnimationDirection initialDirection() { return AnimationDirectionNormal; }
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
WTF::TextStream& operator<<(WTF::TextStream&, Animation::AnimationDirection);
WTF::TextStream& operator<<(WTF::TextStream&, const Animation&);

} // namespace WebCore
