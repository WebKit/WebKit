/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include <WebCore/StyleCoordinatedValueListValue.h>
#include <WebCore/StyleProgressTimelineAxis.h>
#include <WebCore/StyleProgressTimelineName.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/StyleViewTimelineInsetItem.h>

namespace WebCore {
namespace Style {

// macro(ownerType, property, type, lowercaseName, uppercaseName)

#define FOR_EACH_VIEW_TIMELINE_REFERENCE(macro) \
    macro(ViewTimeline, ViewTimelineName, ProgressTimelineName, name, Name) \
    macro(ViewTimeline, ViewTimelineInset, ViewTimelineInsetItem, inset, Inset) \
\

#define FOR_EACH_VIEW_TIMELINE_VALUE(macro) \
    macro(ViewTimeline, ViewTimelineAxis, ProgressTimelineAxis, axis, Axis) \
\

#define FOR_EACH_VIEW_TIMELINE_PROPERTY(macro) \
    FOR_EACH_VIEW_TIMELINE_REFERENCE(macro) \
    FOR_EACH_VIEW_TIMELINE_VALUE(macro) \
\

struct ViewTimeline {
    ViewTimeline();
    ViewTimeline(ProgressTimelineName&&);

    const ProgressTimelineName& name() const { return data().m_name; }
    ProgressTimelineAxis axis() const { return data().m_axis; }
    const ViewTimelineInsetItem& inset() const { return data().m_inset; }

    static ProgressTimelineName initialName() { return CSS::Keyword::None { }; }
    static ProgressTimelineAxis initialAxis() { return ProgressTimelineAxis::Block; }
    static ViewTimelineInsetItem initialInset() { return CSS::Keyword::Auto { }; }

    FOR_EACH_VIEW_TIMELINE_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_REFERENCE)
    FOR_EACH_VIEW_TIMELINE_VALUE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_VALUE)

    bool operator==(const ViewTimeline&) const = default;

    // CoordinatedValueList interface.

    static constexpr auto computedValueUsesUsedValues = false;
    static constexpr auto baseProperty = PropertyNameConstant<CSSPropertyViewTimelineName> { };
    static constexpr auto properties = std::tuple { FOR_EACH_VIEW_TIMELINE_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_PROPERTY) };
    static ViewTimeline clone(const ViewTimeline& other) { return ViewTimeline { Data { other.m_data } }; }
    bool isInitial() const { return name().isNone(); }

private:
    struct Data {
        bool operator==(const Data&) const = default;

        ProgressTimelineName m_name { ViewTimeline::initialName() };
        ProgressTimelineAxis m_axis { ViewTimeline::initialAxis() };
        ViewTimelineInsetItem m_inset { ViewTimeline::initialInset() };

        FOR_EACH_VIEW_TIMELINE_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_IS_SET_AND_IS_FILLED_MEMBERS)
    };

    // Needed by macros to access members.
    Data& data() { return m_data; }
    const Data& data() const { return m_data; }

    ViewTimeline(Data&& data)
        : m_data { WTF::move(data) }
    {
    }

    Data m_data;
};

FOR_EACH_VIEW_TIMELINE_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_REFERENCE)
FOR_EACH_VIEW_TIMELINE_VALUE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_VALUE)

// MARK: - Logging

TextStream& operator<<(TextStream&, const ViewTimeline&);

#undef FOR_EACH_VIEW_TIMELINE_REFERENCE
#undef FOR_EACH_VIEW_TIMELINE_VALUE
#undef FOR_EACH_VIEW_TIMELINE_PROPERTY

} // namespace Style
} // namespace WebCore
