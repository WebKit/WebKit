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

#include <WebCore/StyleLengthWrapper.h>
#include <WebCore/StyleSingleAnimationRangeName.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/TimelineRangeValue.h>

namespace WebCore {
namespace Style {

// <single-animation-range-[start|end]> = [ normal | <length-percentage> | <timeline-range-name> <length-percentage>? ]
// https://drafts.csswg.org/scroll-animations-1/#propdef-animation-range-start
// https://drafts.csswg.org/scroll-animations-1/#propdef-animation-range-end

enum class SingleAnimationRangeType : bool { Start, End };

struct SingleAnimationRangeLength : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;

    static SingleAnimationRangeLength defaultValue(SingleAnimationRangeType);
    bool isDefault(SingleAnimationRangeType) const;
};

template<SingleAnimationRangeType type>
struct SingleAnimationRangeEdge {
    using Base = SingleAnimationRangeEdge<type>;
    using Name = SingleAnimationRangeName;
    using Offset = SingleAnimationRangeLength;

    SingleAnimationRangeEdge(Offset&& offset)
        : SingleAnimationRangeEdge { Name::Omitted, WTF::move(offset) }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::Normal)
        : SingleAnimationRangeEdge { Name::Normal, std::nullopt }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::Cover, std::optional<Offset>&& offset = std::nullopt)
        : SingleAnimationRangeEdge { Name::Cover, WTF::move(offset) }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::Contain, std::optional<Offset>&& offset = std::nullopt)
        : SingleAnimationRangeEdge { Name::Contain, WTF::move(offset) }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::Entry, std::optional<Offset>&& offset = std::nullopt)
        : SingleAnimationRangeEdge { Name::Entry, WTF::move(offset) }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::Exit, std::optional<Offset>&& offset = std::nullopt)
        : SingleAnimationRangeEdge { Name::Exit, WTF::move(offset) }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::EntryCrossing, std::optional<Offset>&& offset = std::nullopt)
        : SingleAnimationRangeEdge { Name::EntryCrossing, WTF::move(offset) }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::ExitCrossing, std::optional<Offset>&& offset = std::nullopt)
        : SingleAnimationRangeEdge { Name::ExitCrossing, WTF::move(offset) }
    {
    }
    SingleAnimationRangeEdge(CSS::Keyword::Scroll, std::optional<Offset>&& offset = std::nullopt)
        : SingleAnimationRangeEdge { Name::Scroll, WTF::move(offset) }
    {
    }

    bool isNormal() const { return m_name == Name::Normal; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        auto visitPredefinedNamedRange = [&](auto keyword) {
            if (m_offset.isDefault(type))
                return visitor(keyword);
            return visitor(SpaceSeparatedTuple { keyword, m_offset });
        };

        switch (m_name) {
        case Name::Normal:
            return visitor(CSS::Keyword::Normal { });
        case Name::Omitted:
            return visitor(m_offset);
        case Name::Cover:
            return visitPredefinedNamedRange(CSS::Keyword::Cover { });
        case Name::Contain:
            return visitPredefinedNamedRange(CSS::Keyword::Contain { });
        case Name::Entry:
            return visitPredefinedNamedRange(CSS::Keyword::Entry { });
        case Name::Exit:
            return visitPredefinedNamedRange(CSS::Keyword::Exit { });
        case Name::EntryCrossing:
            return visitPredefinedNamedRange(CSS::Keyword::EntryCrossing { });
        case Name::ExitCrossing:
            return visitPredefinedNamedRange(CSS::Keyword::ExitCrossing { });
        case Name::Scroll:
            return visitPredefinedNamedRange(CSS::Keyword::Scroll { });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    Name name() const { return m_name; }
    const Offset& offset() const { return m_offset; }

    bool hasDefaultOffset() const { return m_offset.isDefault(type); }

    bool operator==(const SingleAnimationRangeEdge<type>&) const = default;

protected:
    SingleAnimationRangeEdge(Name name, Offset&& offset)
        : m_name { name }
        , m_offset { WTF::move(offset) }
    {
    }

    SingleAnimationRangeEdge(Name name, std::optional<Offset>&& offset)
        : m_name { name }
        , m_offset { offset ? *offset : Offset::defaultValue(type) }
    {
    }

    Name m_name { Name::Normal };
    Offset m_offset;
};

struct SingleAnimationRangeStart : SingleAnimationRangeEdge<SingleAnimationRangeType::Start> {
    using Base::Base;

    TimelineRangeValue toTimelineRangeValue() const;
};

struct SingleAnimationRangeEnd : SingleAnimationRangeEdge<SingleAnimationRangeType::End> {
    using Base::Base;

    TimelineRangeValue toTimelineRangeValue() const;
};

struct SingleAnimationRange {
    SingleAnimationRangeStart start;
    SingleAnimationRangeEnd end;

    bool operator==(const SingleAnimationRange&) const = default;

    static SingleAnimationRange defaultForScrollTimeline();
    static SingleAnimationRange defaultForViewTimeline();

    bool isDefault() const { return start.isNormal() && end.isNormal(); }
};

// MARK: - Conversion

template<> struct CSSValueConversion<SingleAnimationRangeStart> { auto operator()(BuilderState&, const CSSValue&) -> SingleAnimationRangeStart; };
template<> struct CSSValueConversion<SingleAnimationRangeEnd> { auto operator()(BuilderState&, const CSSValue&) -> SingleAnimationRangeEnd; };

template<> struct DeprecatedCSSValueConversion<SingleAnimationRangeStart> { auto operator()(const RefPtr<Element>&, const CSSValue&) -> std::optional<SingleAnimationRangeStart>; };
template<> struct DeprecatedCSSValueConversion<SingleAnimationRangeEnd> { auto operator()(const RefPtr<Element>&, const CSSValue&) -> std::optional<SingleAnimationRangeEnd>; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleAnimationRangeLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleAnimationRangeStart)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleAnimationRangeEnd)
