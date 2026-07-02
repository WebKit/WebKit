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
#include <WebCore/StyleOverflowPosition.h>
#include <WebCore/StyleSelfAlignmentData.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'align-items'> = normal | stretch | <baseline-position> | <overflow-position>? <self-position>
// https://drafts.csswg.org/css-align/#propdef-align-items
// Additional values, `anchor-center` and `dialog` added to <self-position> by CSS Anchor Positioning.
// FIXME: Add support for `dialog`.
// https://drafts.csswg.org/css-anchor-position-1/#anchor-center
struct AlignItems {
    constexpr AlignItems(CSS::Keyword::Normal);
    constexpr AlignItems(CSS::Keyword::Stretch);
    constexpr AlignItems(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::Center, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::Start, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::End, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::SelfStart, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::SelfEnd, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::FlexStart, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> = std::nullopt);
    constexpr AlignItems(CSS::Keyword::AnchorCenter, std::optional<OverflowPosition> = std::nullopt);

    constexpr bool isNormal() const { return primary() == PrimaryKind::Normal; }
    constexpr bool isStretch() const { return primary() == PrimaryKind::Stretch; }
    constexpr bool isBaseline() const { return primary() == PrimaryKind::Baseline; }
    constexpr bool isCenter() const { return primary() == PrimaryKind::Center; }
    constexpr bool isStart() const { return primary() == PrimaryKind::Start; }
    constexpr bool isEnd() const { return primary() == PrimaryKind::End; }
    constexpr bool isSelfStart() const { return primary() == PrimaryKind::SelfStart; }
    constexpr bool isSelfEnd() const { return primary() == PrimaryKind::SelfEnd; }
    constexpr bool isFlexStart() const { return primary() == PrimaryKind::FlexStart; }
    constexpr bool isFlexEnd() const { return primary() == PrimaryKind::FlexEnd; }
    constexpr bool isAnchorCenter() const { return primary() == PrimaryKind::AnchorCenter; }

    constexpr bool isFirstBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::First; }
    constexpr bool isLastBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::Last; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&...) const;

    constexpr bool operator==(const AlignItems&) const = default;

    StyleSelfAlignmentData resolve() const;

private:
    enum class PrimaryKind : uint8_t {
        Normal,
        Stretch,
        Baseline,
        Center,
        Start,
        End,
        SelfStart,
        SelfEnd,
        FlexStart,
        FlexEnd,
        AnchorCenter,
    };

    static constexpr bool canHaveBaselinePosition(PrimaryKind);
    static constexpr bool canHaveOverflowPosition(PrimaryKind);

    constexpr AlignItems(PrimaryKind);
    constexpr AlignItems(PrimaryKind, std::optional<BaselineAlignmentPreference>);
    constexpr AlignItems(PrimaryKind, std::optional<OverflowPosition>);

    constexpr PrimaryKind primary() const;
    constexpr BaselineAlignmentPreferenceKind baselineAlignmentPreference() const;
    constexpr OverflowPositionKind overflowPosition() const;

    PREFERRED_TYPE(PrimaryKind) uint8_t m_primary : 4 { static_cast<uint8_t>(PrimaryKind::Normal) };
    uint8_t m_secondary : 2 { 0 }; // unused, BaselineAlignmentPreferenceKind or OverflowPositionKind, depending on value of m_primary
};
static_assert(sizeof(AlignItems) == 1);

constexpr AlignItems::AlignItems(CSS::Keyword::Normal)
    : AlignItems { PrimaryKind::Normal }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::Stretch)
    : AlignItems { PrimaryKind::Stretch }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> preference)
    : AlignItems { PrimaryKind::Baseline, preference }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::Center, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::Center, overflow }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::Start, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::Start, overflow }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::End, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::End, overflow }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::SelfStart, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::SelfStart, overflow }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::SelfEnd, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::SelfEnd, overflow }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::FlexStart, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::FlexStart, overflow }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::FlexEnd, overflow }
{
}

constexpr AlignItems::AlignItems(CSS::Keyword::AnchorCenter, std::optional<OverflowPosition> overflow)
    : AlignItems { PrimaryKind::AnchorCenter, overflow }
{
}

constexpr AlignItems::AlignItems(PrimaryKind primary)
    : m_primary { static_cast<uint8_t>(primary) }
{
    ASSERT(!canHaveBaselinePosition(primary));
    ASSERT(!canHaveOverflowPosition(primary));
}

constexpr AlignItems::AlignItems(PrimaryKind primary, std::optional<BaselineAlignmentPreference> preference)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(preference)) }
{
    ASSERT(canHaveBaselinePosition(primary));
}

constexpr AlignItems::AlignItems(PrimaryKind primary, std::optional<OverflowPosition> overflow)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(overflow)) }
{
    ASSERT(canHaveOverflowPosition(primary));
}

constexpr AlignItems::PrimaryKind AlignItems::primary() const
{
    return static_cast<PrimaryKind>(m_primary);
}

constexpr BaselineAlignmentPreferenceKind AlignItems::baselineAlignmentPreference() const
{
    RELEASE_ASSERT(canHaveBaselinePosition(primary()));
    return static_cast<BaselineAlignmentPreferenceKind>(m_secondary);
}

constexpr OverflowPositionKind AlignItems::overflowPosition() const
{
    RELEASE_ASSERT(canHaveOverflowPosition(primary()));
    return static_cast<OverflowPositionKind>(m_secondary);
}

constexpr bool AlignItems::canHaveBaselinePosition(PrimaryKind primary)
{
    return primary == PrimaryKind::Baseline;
}

constexpr bool AlignItems::canHaveOverflowPosition(PrimaryKind primary)
{
    switch (primary) {
    case PrimaryKind::Normal:
    case PrimaryKind::Stretch:
    case PrimaryKind::Baseline:
        return false;
    case PrimaryKind::Center:
    case PrimaryKind::Start:
    case PrimaryKind::End:
    case PrimaryKind::SelfStart:
    case PrimaryKind::SelfEnd:
    case PrimaryKind::FlexStart:
    case PrimaryKind::FlexEnd:
    case PrimaryKind::AnchorCenter:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename... F> constexpr decltype(auto) AlignItems::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    switch (primary()) {
    case PrimaryKind::Normal:
        return visitor(CSS::Keyword::Normal { });
    case PrimaryKind::Stretch:
        return visitor(CSS::Keyword::Stretch { });
    case PrimaryKind::Baseline:
        return visitBaselineAlignmentPreference(CSS::Keyword::Baseline { }, baselineAlignmentPreference(), visitor);
    case PrimaryKind::Center:
        return visitOverflowPosition(CSS::Keyword::Center { }, overflowPosition(), visitor);
    case PrimaryKind::Start:
        return visitOverflowPosition(CSS::Keyword::Start { }, overflowPosition(), visitor);
    case PrimaryKind::End:
        return visitOverflowPosition(CSS::Keyword::End { }, overflowPosition(), visitor);
    case PrimaryKind::SelfStart:
        return visitOverflowPosition(CSS::Keyword::SelfStart { }, overflowPosition(), visitor);
    case PrimaryKind::SelfEnd:
        return visitOverflowPosition(CSS::Keyword::SelfEnd { }, overflowPosition(), visitor);
    case PrimaryKind::FlexStart:
        return visitOverflowPosition(CSS::Keyword::FlexStart { }, overflowPosition(), visitor);
    case PrimaryKind::FlexEnd:
        return visitOverflowPosition(CSS::Keyword::FlexEnd { }, overflowPosition(), visitor);
    case PrimaryKind::AnchorCenter:
        return visitOverflowPosition(CSS::Keyword::AnchorCenter { }, overflowPosition(), visitor);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

template<> struct CSSValueConversion<AlignItems> { auto operator()(BuilderState&, const CSSValue&) -> AlignItems; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::AlignItems)
