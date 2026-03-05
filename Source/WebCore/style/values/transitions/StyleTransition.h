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
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleCoordinatedValueListValue.h>
#include <WebCore/StyleEasingFunction.h>
#include <WebCore/StyleSingleTransitionDelay.h>
#include <WebCore/StyleSingleTransitionDuration.h>
#include <WebCore/StyleSingleTransitionProperty.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// macro(ownerType, property, type, lowercaseName, uppercaseName)

#define FOR_EACH_TRANSITION_REFERENCE(macro) \
    macro(Transition, TransitionProperty, SingleTransitionProperty, property, Property) \
    macro(Transition, TransitionTimingFunction, EasingFunction, timingFunction, TimingFunction) \
\

#define FOR_EACH_TRANSITION_VALUE(macro) \
    macro(Transition, TransitionDelay, SingleTransitionDelay, delay, Delay) \
    macro(Transition, TransitionDuration, SingleTransitionDuration, duration, Duration) \
\

#define FOR_EACH_TRANSITION_ENUM(macro) \
    macro(Transition, TransitionBehavior, TransitionBehavior, behavior, Behavior) \
\

#define FOR_EACH_TRANSITION_PROPERTY(macro) \
    FOR_EACH_TRANSITION_REFERENCE(macro) \
    FOR_EACH_TRANSITION_VALUE(macro) \
    FOR_EACH_TRANSITION_ENUM(macro) \
\

struct Transition {
    Transition();
    Transition(SingleTransitionProperty&&);

    const SingleTransitionProperty& property() const { return m_data->m_property; }
    SingleTransitionDelay delay() const { return m_data->m_delay; }
    SingleTransitionDuration duration() const { return m_data->m_duration; }
    const EasingFunction& timingFunction() const { return m_data->m_timingFunction; }
    TransitionBehavior behavior() const { return static_cast<TransitionBehavior>(m_data->m_behavior); }

    static SingleTransitionProperty initialProperty() { return CSS::Keyword::All { }; }
    static SingleTransitionDelay initialDelay() { return 0; }
    static SingleTransitionDuration initialDuration() { return 0; }
    static EasingFunction initialTimingFunction() { return EasingFunction { CubicBezierTimingFunction::create() }; }
    static TransitionBehavior initialBehavior() { return TransitionBehavior::Normal; }

    FOR_EACH_TRANSITION_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_REFERENCE)
    FOR_EACH_TRANSITION_VALUE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_VALUE)
    FOR_EACH_TRANSITION_ENUM(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_ENUM)

    bool operator==(const Transition& other) const
    {
        return arePointingToEqualData(m_data, other.m_data);
    }

    // CoordinatedValueList interface.

    static constexpr auto computedValueUsesUsedValues = false;
    static constexpr auto baseProperty = PropertyNameConstant<CSSPropertyTransitionProperty> { };
    static constexpr auto properties = std::tuple { FOR_EACH_TRANSITION_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_PROPERTY) };
    static Transition clone(const Transition& other) { return Transition { Data::create(other.m_data) }; }
    bool isInitial() const { return m_data->m_property.isAll(); }

private:
    struct Data : public RefCounted<Data> {
        static Ref<Data> create() { return adoptRef(*new Data()); }
        static Ref<Data> create(const Data& other) { return adoptRef(*new Data(other)); }

        Data();
        Data(const Data&);

        bool operator==(const Data&) const;

        SingleTransitionProperty m_property;
        SingleTransitionDelay m_delay;
        SingleTransitionDuration m_duration;
        EasingFunction m_timingFunction;
        PREFERRED_TYPE(TransitionBehavior) unsigned m_behavior : 1;

        FOR_EACH_TRANSITION_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_IS_SET_AND_IS_FILLED_MEMBERS)
    };

    // Needed by macros to access members.
    Data& data() { return m_data.get(); }
    const Data& data() const { return m_data.get(); }

    Transition(Ref<Data>&& data)
        : m_data { WTF::move(data) }
    {
    }

    Ref<Data> m_data;
};

FOR_EACH_TRANSITION_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_REFERENCE)
FOR_EACH_TRANSITION_VALUE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_VALUE)
FOR_EACH_TRANSITION_ENUM(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_ENUM)

// MARK: - Logging

TextStream& operator<<(TextStream&, const Transition&);

#undef FOR_EACH_TRANSITION_REFERENCE
#undef FOR_EACH_TRANSITION_VALUE
#undef FOR_EACH_TRANSITION_ENUM
#undef FOR_EACH_TRANSITION_PROPERTY

} // namespace Style
} // namespace WebCore
