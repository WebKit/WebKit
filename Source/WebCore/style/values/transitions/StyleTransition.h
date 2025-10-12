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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleEasingFunction.h>
#include <WebCore/StyleSingleTransitionDelay.h>
#include <WebCore/StyleSingleTransitionDuration.h>
#include <WebCore/StyleSingleTransitionProperty.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

struct Transition {
    Transition() : m_data { Data::create() } { }
    Transition(SingleTransitionProperty&& property) : m_data { Data::create(WTFMove(property)) } { }

    static Transition clone(const Transition& other)
    {
        return Transition { Data::create(other.m_data) };
    }

    const SingleTransitionProperty& property() const { return m_data->m_property; }
    SingleTransitionDelay delay() const { return m_data->m_delay; }
    SingleTransitionDuration duration() const { return m_data->m_duration; }
    const EasingFunction& timingFunction() const { return m_data->m_timingFunction; }
    TransitionBehavior behavior() const { return static_cast<TransitionBehavior>(m_data->m_behavior); }

    void setProperty(SingleTransitionProperty&& property) { m_data->m_property = WTFMove(property); m_data->m_propertySet = true; }
    void setDelay(SingleTransitionDelay delay) { m_data->m_delay = delay; m_data->m_delaySet = true; }
    void setDuration(SingleTransitionDuration duration) { m_data->m_duration = duration; m_data->m_durationSet = true; }
    void setTimingFunction(EasingFunction&& function) { m_data->m_timingFunction = WTFMove(function); m_data->m_timingFunctionSet = true; }
    void setBehavior(TransitionBehavior behavior) { m_data->m_behavior = static_cast<bool>(behavior); m_data->m_behaviorSet = true; }

    void fillProperty(SingleTransitionProperty&& property) { setProperty(WTFMove(property)); m_data->m_propertyFilled = true; }
    void fillDelay(SingleTransitionDelay delay) { setDelay(delay); m_data->m_delayFilled = true; }
    void fillDuration(SingleTransitionDuration duration) { setDuration(duration); m_data->m_durationFilled = true; }
    void fillTimingFunction(EasingFunction&& function) { setTimingFunction(WTFMove(function)); m_data->m_timingFunctionFilled = true; }
    void fillBehavior(TransitionBehavior behavior) { setBehavior(behavior); m_data->m_behaviorFilled = true; }

    void clearProperty() { m_data->m_propertySet = false; m_data->m_propertyFilled = false; }
    void clearDelay() { m_data->m_delaySet = false; m_data->m_delayFilled = false; }
    void clearDuration() { m_data->m_durationSet = false; m_data->m_durationFilled = false; }
    void clearTimingFunction() { m_data->m_timingFunctionSet = false; m_data->m_timingFunctionFilled = false; }
    void clearBehavior() { m_data->m_behaviorSet = false; m_data->m_behaviorFilled = false; }

    bool isPropertySet() const { return m_data->m_propertySet; }
    bool isDelaySet() const { return m_data->m_delaySet; }
    bool isDurationSet() const { return m_data->m_durationSet; }
    bool isTimingFunctionSet() const { return m_data->m_timingFunctionSet; }
    bool isBehaviorSet() const { return m_data->m_behaviorSet; }

    bool isPropertyFilled() const { return m_data->m_propertyFilled; }
    bool isDelayFilled() const { return m_data->m_delayFilled; }
    bool isDurationFilled() const { return m_data->m_durationFilled; }
    bool isTimingFunctionFilled() const { return m_data->m_timingFunctionFilled; }
    bool isBehaviorFilled() const { return m_data->m_behaviorFilled; }

    static SingleTransitionProperty initialProperty() { return CSS::Keyword::All { }; }
    static SingleTransitionDelay initialDelay() { return 0; }
    static SingleTransitionDuration initialDuration() { return 0; }
    static EasingFunction initialTimingFunction() { return EasingFunction { CubicBezierTimingFunction::create() }; }
    static TransitionBehavior initialBehavior() { return TransitionBehavior::Normal; }

    // CoordinatedValueList value functions.

    bool isEmpty() const
    {
        return (!isPropertySet() || isPropertyFilled())
            && (!isDelaySet() || isDelayFilled())
            && (!isDurationSet() || isDurationFilled())
            && (!isTimingFunctionSet() || isTimingFunctionFilled())
            && (!isBehaviorSet() || isBehaviorFilled());
    }

    template<auto isSet, auto getter, auto setter, typename T>
    static void fillUnsetProperty(auto& list);
    static void fillUnsetProperties(auto& list);

    bool operator==(const Transition& other) const
    {
        return arePointingToEqualData(m_data, other.m_data);
    }

private:
    struct Data : public RefCounted<Data> {
        static Ref<Data> create() { return adoptRef(*new Data()); }
        static Ref<Data> create(const Data& other) { return adoptRef(*new Data(other)); }
        static Ref<Data> create(SingleTransitionProperty&& property) { return adoptRef(*new Data(WTFMove(property))); }

        Data();
        Data(const Data&);
        Data(SingleTransitionProperty&&);

        bool operator==(const Data&) const;

        SingleTransitionProperty m_property;
        SingleTransitionDelay m_delay;
        SingleTransitionDuration m_duration;
        EasingFunction m_timingFunction;
        PREFERRED_TYPE(TransitionBehavior) bool m_behavior : 1;

        bool m_propertySet : 1;
        bool m_delaySet : 1;
        bool m_durationSet : 1;
        bool m_timingFunctionSet : 1;
        bool m_behaviorSet : 1;

        bool m_propertyFilled : 1;
        bool m_delayFilled : 1;
        bool m_durationFilled : 1;
        bool m_timingFunctionFilled : 1;
        bool m_behaviorFilled : 1;
    };

    Transition(Ref<Data>&& data)
        : m_data { WTFMove(data) }
    {
    }

    Ref<Data> m_data;
};

template<auto isSet, auto getter, auto filler, typename T>
void Transition::fillUnsetProperty(auto& list)
{
    size_t i = 0;
    for (; i < list.size() && (list[i].*isSet)(); ++i) { }
    if (i) {
        for (size_t j = 0; i < list.size(); ++i, ++j)
            (list[i].*filler)(T { (list[j].*getter)() });
    }
}

void Transition::fillUnsetProperties(auto& list)
{
    fillUnsetProperty<&Transition::isPropertySet, &Transition::property, &Transition::fillProperty, SingleTransitionProperty>(list);
    fillUnsetProperty<&Transition::isDelaySet, &Transition::delay, &Transition::fillDelay, SingleTransitionDelay>(list);
    fillUnsetProperty<&Transition::isDurationSet, &Transition::duration, &Transition::fillDuration, SingleTransitionDuration>(list);
    fillUnsetProperty<&Transition::isTimingFunctionSet, &Transition::timingFunction, &Transition::fillTimingFunction, EasingFunction>(list);
    fillUnsetProperty<&Transition::isBehaviorSet, &Transition::behavior, &Transition::fillBehavior, TransitionBehavior>(list);
}

// MARK: - Logging

TextStream& operator<<(TextStream&, const Transition&);

} // namespace Style
} // namespace WebCore
