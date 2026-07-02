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
#include <WebCore/StyleBaselineAlignmentPreference.h>
#include <WebCore/StyleContentAlignmentData.h>
#include <WebCore/StyleOverflowPosition.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'align-content'> = normal | <baseline-position> | <content-distribution> | <overflow-position>? <content-position>
// https://drafts.csswg.org/css-align/#propdef-align-content
struct AlignContent {
    constexpr AlignContent(CSS::Keyword::Normal);
    constexpr AlignContent(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> = std::nullopt);
    constexpr AlignContent(CSS::Keyword::SpaceBetween);
    constexpr AlignContent(CSS::Keyword::SpaceAround);
    constexpr AlignContent(CSS::Keyword::SpaceEvenly);
    constexpr AlignContent(CSS::Keyword::Stretch);
    constexpr AlignContent(CSS::Keyword::Center, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignContent(CSS::Keyword::Start, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignContent(CSS::Keyword::End, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignContent(CSS::Keyword::FlexStart, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignContent(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> = std::nullopt);

    constexpr bool isNormal() const { return primary() == PrimaryKind::Normal; }
    constexpr bool isBaseline() const { return primary() == PrimaryKind::Baseline; }
    constexpr bool isSpaceBetween() const { return primary() == PrimaryKind::SpaceBetween; }
    constexpr bool isSpaceAround() const { return primary() == PrimaryKind::SpaceAround; }
    constexpr bool isSpaceEvenly() const { return primary() == PrimaryKind::SpaceEvenly; }
    constexpr bool isStretch() const { return primary() == PrimaryKind::Stretch; }
    constexpr bool isCenter() const { return primary() == PrimaryKind::Center; }
    constexpr bool isStart() const { return primary() == PrimaryKind::Start; }
    constexpr bool isEnd() const { return primary() == PrimaryKind::End; }
    constexpr bool isFlexStart() const { return primary() == PrimaryKind::FlexStart; }
    constexpr bool isFlexEnd() const { return primary() == PrimaryKind::FlexEnd; }

    constexpr bool isFirstBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::First; }
    constexpr bool isLastBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::Last; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&...) const;

    constexpr bool operator==(const AlignContent&) const = default;

    StyleContentAlignmentData resolve(std::optional<StyleContentAlignmentData> = std::nullopt) const;

private:
    enum class PrimaryKind : uint8_t {
        Normal,
        Baseline,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly,
        Stretch,
        Center,
        Start,
        End,
        FlexStart,
        FlexEnd,
    };

    static constexpr bool canHaveBaselinePosition(PrimaryKind);
    static constexpr bool isContentPosition(PrimaryKind);

    constexpr AlignContent(PrimaryKind);
    constexpr AlignContent(PrimaryKind, std::optional<BaselineAlignmentPreference>);
    constexpr AlignContent(PrimaryKind, std::optional<OverflowPosition>);

    constexpr PrimaryKind primary() const;
    constexpr BaselineAlignmentPreferenceKind baselineAlignmentPreference() const;
    constexpr OverflowPositionKind overflowPosition() const;

    PREFERRED_TYPE(PrimaryKind) uint8_t m_primary : 4 { static_cast<uint8_t>(PrimaryKind::Normal) };
    uint8_t m_secondary : 2 { 0 }; // unused, BaselineAlignmentPreferenceKind or OverflowPositionKind, depending on value of m_primary
};
static_assert(sizeof(AlignContent) == 1);

constexpr AlignContent::AlignContent(CSS::Keyword::Normal)
    : AlignContent { PrimaryKind::Normal }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> preference)
    : AlignContent { PrimaryKind::Baseline, preference }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::SpaceBetween)
    : AlignContent { PrimaryKind::SpaceBetween }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::SpaceAround)
    : AlignContent { PrimaryKind::SpaceAround }
{

}
constexpr AlignContent::AlignContent(CSS::Keyword::SpaceEvenly)
    : AlignContent { PrimaryKind::SpaceEvenly }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::Stretch)
    : AlignContent { PrimaryKind::Stretch }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::Center, std::optional<OverflowPosition> overflow)
    : AlignContent { PrimaryKind::Center, overflow }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::Start, std::optional<OverflowPosition> overflow)
    : AlignContent { PrimaryKind::Start, overflow }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::End, std::optional<OverflowPosition> overflow)
    : AlignContent { PrimaryKind::End, overflow }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::FlexStart, std::optional<OverflowPosition> overflow)
    : AlignContent { PrimaryKind::FlexStart, overflow }
{
}

constexpr AlignContent::AlignContent(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> overflow)
    : AlignContent { PrimaryKind::FlexEnd, overflow }
{
}

constexpr AlignContent::AlignContent(PrimaryKind primary)
    : m_primary { static_cast<uint8_t>(primary) }
{
    ASSERT(!canHaveBaselinePosition(primary));
    ASSERT(!isContentPosition(primary));
}

constexpr AlignContent::AlignContent(PrimaryKind primary, std::optional<BaselineAlignmentPreference> preference)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(preference)) }
{
    ASSERT(canHaveBaselinePosition(primary));
}

constexpr AlignContent::AlignContent(PrimaryKind primary, std::optional<OverflowPosition> overflow)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(overflow)) }
{
    ASSERT(isContentPosition(primary));
}

constexpr AlignContent::PrimaryKind AlignContent::primary() const
{
    return static_cast<PrimaryKind>(m_primary);
}

constexpr BaselineAlignmentPreferenceKind AlignContent::baselineAlignmentPreference() const
{
    RELEASE_ASSERT(canHaveBaselinePosition(primary()));
    return static_cast<BaselineAlignmentPreferenceKind>(m_secondary);
}

constexpr OverflowPositionKind AlignContent::overflowPosition() const
{
    RELEASE_ASSERT(isContentPosition(primary()));
    return static_cast<OverflowPositionKind>(m_secondary);
}

constexpr bool AlignContent::canHaveBaselinePosition(PrimaryKind primary)
{
    return primary == PrimaryKind::Baseline;
}

constexpr bool AlignContent::isContentPosition(PrimaryKind primary)
{
    switch (primary) {
    case PrimaryKind::Normal:
    case PrimaryKind::Baseline:
    case PrimaryKind::SpaceBetween:
    case PrimaryKind::SpaceAround:
    case PrimaryKind::SpaceEvenly:
    case PrimaryKind::Stretch:
        return false;
    case PrimaryKind::Center:
    case PrimaryKind::Start:
    case PrimaryKind::End:
    case PrimaryKind::FlexStart:
    case PrimaryKind::FlexEnd:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename... F> constexpr decltype(auto) AlignContent::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    switch (primary()) {
    case PrimaryKind::Normal:
        return visitor(CSS::Keyword::Normal { });
    case PrimaryKind::Baseline:
        return visitBaselineAlignmentPreference(CSS::Keyword::Baseline { }, baselineAlignmentPreference(), visitor);
    case PrimaryKind::SpaceBetween:
        return visitor(CSS::Keyword::SpaceBetween { });
    case PrimaryKind::SpaceAround:
        return visitor(CSS::Keyword::SpaceAround { });
    case PrimaryKind::SpaceEvenly:
        return visitor(CSS::Keyword::SpaceEvenly { });
    case PrimaryKind::Stretch:
        return visitor(CSS::Keyword::Stretch { });
    case PrimaryKind::Center:
        return visitOverflowPosition(CSS::Keyword::Center { }, overflowPosition(), visitor);
    case PrimaryKind::Start:
        return visitOverflowPosition(CSS::Keyword::Start { }, overflowPosition(), visitor);
    case PrimaryKind::End:
        return visitOverflowPosition(CSS::Keyword::End { }, overflowPosition(), visitor);
    case PrimaryKind::FlexStart:
        return visitOverflowPosition(CSS::Keyword::FlexStart { }, overflowPosition(), visitor);
    case PrimaryKind::FlexEnd:
        return visitOverflowPosition(CSS::Keyword::FlexEnd { }, overflowPosition(), visitor);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

template<> struct CSSValueConversion<AlignContent> { auto operator()(BuilderState&, const CSSValue&) -> AlignContent; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::AlignContent)
