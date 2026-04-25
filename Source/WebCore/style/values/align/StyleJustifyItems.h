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

#include <WebCore/StyleBaselineAlignmentPreference.h>
#include <WebCore/StyleLegacyPosition.h>
#include <WebCore/StyleOverflowPosition.h>
#include <WebCore/StyleSelfAlignmentData.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'justify-items'> = normal | stretch | <baseline-position> | <overflow-position>? [ <self-position> | left | right ] | legacy | legacy && [ left | right | center ]
// https://drafts.csswg.org/css-align/#propdef-justify-items
// Additional values, `anchor-center` and `dialog` added to <self-position> by CSS Anchor Positioning.
// FIXME: Add support for `dialog`.
// https://drafts.csswg.org/css-anchor-position-1/#anchor-center
struct JustifyItems {
    constexpr JustifyItems(CSS::Keyword::Normal);
    constexpr JustifyItems(CSS::Keyword::Stretch);
    constexpr JustifyItems(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::Center, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::Start, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::End, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::SelfStart, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::SelfEnd, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::FlexStart, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::Left, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::Right, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::AnchorCenter, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifyItems(CSS::Keyword::Legacy, std::optional<LegacyPosition> = std::nullopt);

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
    constexpr bool isLeft() const { return primary() == PrimaryKind::Left; }
    constexpr bool isRight() const { return primary() == PrimaryKind::Right; }
    constexpr bool isAnchorCenter() const { return primary() == PrimaryKind::AnchorCenter; }
    constexpr bool isLegacy() const { return primary() == PrimaryKind::Legacy; }

    constexpr bool isFirstBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::First; }
    constexpr bool isLastBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::Last; }

    constexpr bool isLegacyNone() const { return primary() == PrimaryKind::Legacy && legacyPosition() == LegacyPositionKind::None; }
    constexpr bool isLegacyLeft() const { return primary() == PrimaryKind::Legacy && legacyPosition() == LegacyPositionKind::Left; }
    constexpr bool isLegacyRight() const { return primary() == PrimaryKind::Legacy && legacyPosition() == LegacyPositionKind::Right; }
    constexpr bool isLegacyCenter() const { return primary() == PrimaryKind::Legacy && legacyPosition() == LegacyPositionKind::Center; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&...) const;

    constexpr bool operator==(const JustifyItems&) const = default;

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
        Left,
        Right,
        AnchorCenter,
        Legacy,
    };

    static constexpr bool canHaveBaselinePosition(PrimaryKind);
    static constexpr bool canHaveOverflowPosition(PrimaryKind);
    static constexpr bool canHaveLegacyPosition(PrimaryKind);

    constexpr JustifyItems(PrimaryKind);
    constexpr JustifyItems(PrimaryKind, std::optional<BaselineAlignmentPreference>);
    constexpr JustifyItems(PrimaryKind, std::optional<OverflowPosition>);
    constexpr JustifyItems(PrimaryKind, std::optional<LegacyPosition>);

    constexpr PrimaryKind primary() const;
    constexpr BaselineAlignmentPreferenceKind baselineAlignmentPreference() const;
    constexpr OverflowPositionKind overflowPosition() const;
    constexpr LegacyPositionKind legacyPosition() const;

    PREFERRED_TYPE(PrimaryKind) uint8_t m_primary : 4 { static_cast<uint8_t>(PrimaryKind::Normal) };
    uint8_t m_secondary : 2 { 0 }; // unused, BaselineAlignmentPreferenceKind, OverflowPositionKind or LegacyPositionKind, depending on value of m_primary
};
static_assert(sizeof(JustifyItems) == 1);

constexpr JustifyItems::JustifyItems(CSS::Keyword::Normal)
    : JustifyItems { PrimaryKind::Normal }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::Stretch)
    : JustifyItems { PrimaryKind::Stretch }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> preference)
    : JustifyItems { PrimaryKind::Baseline, preference }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::Center, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::Center, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::Start, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::Start, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::End, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::End, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::SelfStart, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::SelfStart, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::SelfEnd, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::SelfEnd, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::FlexStart, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::FlexStart, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::FlexEnd, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::Left, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::Left, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::Right, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::Right, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::AnchorCenter, std::optional<OverflowPosition> overflow)
    : JustifyItems { PrimaryKind::AnchorCenter, overflow }
{
}

constexpr JustifyItems::JustifyItems(CSS::Keyword::Legacy, std::optional<LegacyPosition> position)
    : JustifyItems { PrimaryKind::Legacy, position }
{
}

constexpr JustifyItems::JustifyItems(PrimaryKind primary)
    : m_primary { static_cast<uint8_t>(primary) }
{
    ASSERT(!canHaveBaselinePosition(primary));
    ASSERT(!canHaveOverflowPosition(primary));
    ASSERT(!canHaveLegacyPosition(primary));
}

constexpr JustifyItems::JustifyItems(PrimaryKind primary, std::optional<BaselineAlignmentPreference> preference)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(preference)) }
{
    ASSERT(canHaveBaselinePosition(primary));
}

constexpr JustifyItems::JustifyItems(PrimaryKind primary, std::optional<OverflowPosition> overflow)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(overflow)) }
{
    ASSERT(canHaveOverflowPosition(primary));
}

constexpr JustifyItems::JustifyItems(PrimaryKind primary, std::optional<LegacyPosition> position)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(position)) }
{
    ASSERT(canHaveLegacyPosition(primary));
}

constexpr JustifyItems::PrimaryKind JustifyItems::primary() const
{
    return static_cast<PrimaryKind>(m_primary);
}

constexpr BaselineAlignmentPreferenceKind JustifyItems::baselineAlignmentPreference() const
{
    RELEASE_ASSERT(canHaveBaselinePosition(primary()));
    return static_cast<BaselineAlignmentPreferenceKind>(m_secondary);
}

constexpr OverflowPositionKind JustifyItems::overflowPosition() const
{
    RELEASE_ASSERT(canHaveOverflowPosition(primary()));
    return static_cast<OverflowPositionKind>(m_secondary);
}

constexpr LegacyPositionKind JustifyItems::legacyPosition() const
{
    RELEASE_ASSERT(canHaveLegacyPosition(primary()));
    return static_cast<LegacyPositionKind>(m_secondary);
}

constexpr bool JustifyItems::canHaveBaselinePosition(PrimaryKind primary)
{
    return primary == PrimaryKind::Baseline;
}

constexpr bool JustifyItems::canHaveOverflowPosition(PrimaryKind primary)
{
    switch (primary) {
    case PrimaryKind::Normal:
    case PrimaryKind::Stretch:
    case PrimaryKind::Baseline:
    case PrimaryKind::Legacy:
        return false;
    case PrimaryKind::Center:
    case PrimaryKind::Start:
    case PrimaryKind::End:
    case PrimaryKind::SelfStart:
    case PrimaryKind::SelfEnd:
    case PrimaryKind::FlexStart:
    case PrimaryKind::FlexEnd:
    case PrimaryKind::Left:
    case PrimaryKind::Right:
    case PrimaryKind::AnchorCenter:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr bool JustifyItems::canHaveLegacyPosition(PrimaryKind primary)
{
    return primary == PrimaryKind::Legacy;
}

template<typename... F> constexpr decltype(auto) JustifyItems::switchOn(F&&... f) const
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
    case PrimaryKind::Left:
        return visitOverflowPosition(CSS::Keyword::Left { }, overflowPosition(), visitor);
    case PrimaryKind::Right:
        return visitOverflowPosition(CSS::Keyword::Right { }, overflowPosition(), visitor);
    case PrimaryKind::AnchorCenter:
        return visitOverflowPosition(CSS::Keyword::AnchorCenter { }, overflowPosition(), visitor);
    case PrimaryKind::Legacy:
        return visitLegacyPosition(CSS::Keyword::Legacy { }, legacyPosition(), visitor);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

template<> struct CSSValueConversion<JustifyItems> { auto operator()(BuilderState&, const CSSValue&) -> JustifyItems; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::JustifyItems)
