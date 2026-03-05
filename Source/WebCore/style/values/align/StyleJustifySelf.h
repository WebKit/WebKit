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
#include <WebCore/StyleOverflowPosition.h>
#include <WebCore/StyleSelfAlignmentData.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'justify-self'> = auto | normal | stretch | <baseline-position> | <overflow-position>? [ <self-position> | left | right ]
// https://drafts.csswg.org/css-align/#propdef-justify-self
// Additional values, `anchor-center` added to <self-position> by CSS Anchor Positioning.
// https://drafts.csswg.org/css-anchor-position-1/#anchor-center
struct JustifySelf {
    constexpr JustifySelf(CSS::Keyword::Auto);
    constexpr JustifySelf(CSS::Keyword::Normal);
    constexpr JustifySelf(CSS::Keyword::Stretch);
    constexpr JustifySelf(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::Center, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::Start, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::End, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::SelfStart, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::SelfEnd, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::FlexStart, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::Left, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::Right, std::optional<OverflowPosition> = std::nullopt);
    constexpr JustifySelf(CSS::Keyword::AnchorCenter, std::optional<OverflowPosition> = std::nullopt);

    constexpr bool isAuto() const { return primary() == PrimaryKind::Auto; }
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

    constexpr bool isFirstBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::First; }
    constexpr bool isLastBaseline() const { return primary() == PrimaryKind::Baseline && baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::Last; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&...) const;

    constexpr bool operator==(const JustifySelf&) const = default;

    StyleSelfAlignmentData resolve(const RenderStyle* containerStyle = nullptr) const; // Resolves 'auto' against containerStyle's justify-items.

private:
    enum class PrimaryKind : uint8_t {
        Auto,
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
    };

    static constexpr bool canHaveBaselinePosition(PrimaryKind);
    static constexpr bool canHaveOverflowPosition(PrimaryKind);

    constexpr JustifySelf(PrimaryKind);
    constexpr JustifySelf(PrimaryKind, std::optional<BaselineAlignmentPreference>);
    constexpr JustifySelf(PrimaryKind, std::optional<OverflowPosition>);

    constexpr PrimaryKind primary() const;
    constexpr BaselineAlignmentPreferenceKind baselineAlignmentPreference() const;
    constexpr OverflowPositionKind overflowPosition() const;

    PREFERRED_TYPE(PrimaryKind) uint8_t m_primary : 4 { static_cast<uint8_t>(PrimaryKind::Normal) };
    uint8_t m_secondary : 2 { 0 }; // unused, BaselineAlignmentPreferenceKind or OverflowPositionKind, depending on value of m_primary
};
static_assert(sizeof(JustifySelf) == 1);

constexpr JustifySelf::JustifySelf(CSS::Keyword::Auto)
    : JustifySelf { PrimaryKind::Auto }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::Normal)
    : JustifySelf { PrimaryKind::Normal }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::Stretch)
    : JustifySelf { PrimaryKind::Stretch }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::Baseline, std::optional<BaselineAlignmentPreference> preference)
    : JustifySelf { PrimaryKind::Baseline, preference }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::Center, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::Center, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::Start, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::Start, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::End, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::End, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::SelfStart, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::SelfStart, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::SelfEnd, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::SelfEnd, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::FlexStart, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::FlexStart, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::FlexEnd, overflow }
{
}


constexpr JustifySelf::JustifySelf(CSS::Keyword::Left, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::Left, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::Right, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::Right, overflow }
{
}

constexpr JustifySelf::JustifySelf(CSS::Keyword::AnchorCenter, std::optional<OverflowPosition> overflow)
    : JustifySelf { PrimaryKind::AnchorCenter, overflow }
{
}

constexpr JustifySelf::JustifySelf(PrimaryKind primary)
    : m_primary { static_cast<uint8_t>(primary) }
{
    ASSERT(!canHaveBaselinePosition(primary));
    ASSERT(!canHaveOverflowPosition(primary));
}

constexpr JustifySelf::JustifySelf(PrimaryKind primary, std::optional<BaselineAlignmentPreference> preference)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(preference)) }
{
    ASSERT(canHaveBaselinePosition(primary));
}

constexpr JustifySelf::JustifySelf(PrimaryKind primary, std::optional<OverflowPosition> overflow)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(overflow)) }
{
    ASSERT(canHaveOverflowPosition(primary));
}

constexpr JustifySelf::PrimaryKind JustifySelf::primary() const
{
    return static_cast<PrimaryKind>(m_primary);
}

constexpr BaselineAlignmentPreferenceKind JustifySelf::baselineAlignmentPreference() const
{
    RELEASE_ASSERT(canHaveBaselinePosition(primary()));
    return static_cast<BaselineAlignmentPreferenceKind>(m_secondary);
}

constexpr OverflowPositionKind JustifySelf::overflowPosition() const
{
    RELEASE_ASSERT(canHaveOverflowPosition(primary()));
    return static_cast<OverflowPositionKind>(m_secondary);
}

constexpr bool JustifySelf::canHaveBaselinePosition(PrimaryKind primary)
{
    return primary == PrimaryKind::Baseline;
}

constexpr bool JustifySelf::canHaveOverflowPosition(PrimaryKind primary)
{
    switch (primary) {
    case PrimaryKind::Auto:
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
    case PrimaryKind::Left:
    case PrimaryKind::Right:
    case PrimaryKind::AnchorCenter:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename... F> constexpr decltype(auto) JustifySelf::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    switch (primary()) {
    case PrimaryKind::Auto:
        return visitor(CSS::Keyword::Auto { });
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
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

template<> struct CSSValueConversion<JustifySelf> { auto operator()(BuilderState&, const CSSValue&) -> JustifySelf; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::JustifySelf)
