/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/StyleCoordinatedValueListValue.h>
#include <WebCore/StyleSingleAnimationTimeline.h>
#include <WebCore/StyleTimelineTriggerName.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// macro(ownerType, property, type, lowercaseName, uppercaseName)

#define FOR_EACH_TIMELINE_TRIGGER_REFERENCE(macro) \
    macro(TimelineTrigger, TimelineTriggerName, TimelineTriggerName, name, Name) \
    macro(TimelineTrigger, TimelineTriggerSource, SingleAnimationTimeline, source, Source) \
\

#define FOR_EACH_TIMELINE_TRIGGER_PROPERTY(macro) \
    FOR_EACH_TIMELINE_TRIGGER_REFERENCE(macro) \
\

struct TimelineTrigger {
    TimelineTrigger();
    TimelineTrigger(TimelineTriggerName&&);

    const TimelineTriggerName& name() const { return data().m_name; }
    const SingleAnimationTimeline& source() const LIFETIME_BOUND { return data().m_source; }

    static TimelineTriggerName initialName() { return CSS::Keyword::None { }; }
    static SingleAnimationTimeline initialSource() { return CSS::Keyword::Auto { }; }

    FOR_EACH_TIMELINE_TRIGGER_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_REFERENCE)

    bool operator==(const TimelineTrigger&) const = default;

    // CoordinatedValueList interface.

    static constexpr auto baseProperty = PropertyNameConstant<CSSPropertyTimelineTriggerName> { };
    static constexpr auto properties = std::tuple { FOR_EACH_TIMELINE_TRIGGER_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_PROPERTY) };
    static TimelineTrigger clone(const TimelineTrigger& other) { return TimelineTrigger { Data { other.m_data } }; }
    bool isInitial() const { return name().isNone(); }

private:
    struct Data {
        bool operator==(const Data&) const = default;

        TimelineTriggerName m_name { TimelineTrigger::initialName() };
        SingleAnimationTimeline m_source { TimelineTrigger::initialSource() };

        FOR_EACH_TIMELINE_TRIGGER_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_IS_SET_AND_IS_FILLED_MEMBERS)
    };

    // Needed by macros to access members.
    Data& data() LIFETIME_BOUND { return m_data; }
    const Data& data() const LIFETIME_BOUND { return m_data; }

    TimelineTrigger(Data&& data)
        : m_data { WTF::move(data) }
    {
    }

    Data m_data;
};

FOR_EACH_TIMELINE_TRIGGER_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_REFERENCE)

// MARK: - Logging

TextStream& operator<<(TextStream&, const TimelineTrigger&);

#undef FOR_EACH_TIMELINE_TRIGGER_REFERENCE
#undef FOR_EACH_TIMELINE_TRIGGER_PROPERTY

} // namespace Style
} // namespace WebCore
