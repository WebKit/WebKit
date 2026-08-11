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
#include <WebCore/StyleContentAlignmentData.h>
#include <WebCore/StyleOverflowPosition.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'justify-content'> = normal | <content-distribution> | <overflow-position>? [ <content-position> | left | right ]
// https://drafts.csswg.org/css-align/#propdef-justify-content
struct JustifyContent {
     constexpr JustifyContent(CSS::Keyword::Normal);
     constexpr JustifyContent(CSS::Keyword::SpaceBetween);
     constexpr JustifyContent(CSS::Keyword::SpaceAround);
     constexpr JustifyContent(CSS::Keyword::SpaceEvenly);
     constexpr JustifyContent(CSS::Keyword::Stretch);
     constexpr JustifyContent(CSS::Keyword::Center, std::optional<OverflowPosition> = std::nullopt);
     constexpr JustifyContent(CSS::Keyword::Start, std::optional<OverflowPosition> = std::nullopt);
     constexpr JustifyContent(CSS::Keyword::End, std::optional<OverflowPosition> = std::nullopt);
     constexpr JustifyContent(CSS::Keyword::FlexStart, std::optional<OverflowPosition> = std::nullopt);
     constexpr JustifyContent(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> = std::nullopt);
     constexpr JustifyContent(CSS::Keyword::Left, std::optional<OverflowPosition> = std::nullopt);
     constexpr JustifyContent(CSS::Keyword::Right, std::optional<OverflowPosition> = std::nullopt);

    constexpr bool isNormal() const { return primary() == PrimaryKind::Normal; }
    constexpr bool isSpaceBetween() const { return primary() == PrimaryKind::SpaceBetween; }
    constexpr bool isSpaceAround() const { return primary() == PrimaryKind::SpaceAround; }
    constexpr bool isSpaceEvenly() const { return primary() == PrimaryKind::SpaceEvenly; }
    constexpr bool isStretch() const { return primary() == PrimaryKind::Stretch; }
    constexpr bool isCenter() const { return primary() == PrimaryKind::Center; }
    constexpr bool isStart() const { return primary() == PrimaryKind::Start; }
    constexpr bool isEnd() const { return primary() == PrimaryKind::End; }
    constexpr bool isFlexStart() const { return primary() == PrimaryKind::FlexStart; }
    constexpr bool isFlexEnd() const { return primary() == PrimaryKind::FlexEnd; }
    constexpr bool isLeft() const { return primary() == PrimaryKind::Left; }
    constexpr bool isRight() const { return primary() == PrimaryKind::Right; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&...) const;

    constexpr bool operator==(const JustifyContent&) const = default;

    StyleContentAlignmentData resolve(std::optional<StyleContentAlignmentData> = std::nullopt) const;

private:
    enum class PrimaryKind : uint8_t {
        Normal,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly,
        Stretch,
        Center,
        Start,
        End,
        FlexStart,
        FlexEnd,
        Left,
        Right,
    };

    static constexpr bool isContentPosition(PrimaryKind);

    constexpr JustifyContent(PrimaryKind);
    constexpr JustifyContent(PrimaryKind, std::optional<OverflowPosition>);

    constexpr PrimaryKind primary() const;
    constexpr OverflowPositionKind overflowPosition() const;

    PREFERRED_TYPE(PrimaryKind) uint8_t m_primary : 4 { static_cast<uint8_t>(PrimaryKind::Normal) };
    uint8_t m_secondary : 2 { 0 }; // unused or OverflowPositionKind, depending on value of m_primary
};
static_assert(sizeof(JustifyContent) == 1);

constexpr JustifyContent::JustifyContent(CSS::Keyword::Normal)
    : JustifyContent { PrimaryKind::Normal }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::SpaceBetween)
    : JustifyContent { PrimaryKind::SpaceBetween }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::SpaceAround)
    : JustifyContent { PrimaryKind::SpaceAround }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::SpaceEvenly)
    : JustifyContent { PrimaryKind::SpaceEvenly }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::Stretch)
    : JustifyContent { PrimaryKind::Stretch }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::Center, std::optional<OverflowPosition> overflow)
    : JustifyContent { PrimaryKind::Center, overflow }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::Start, std::optional<OverflowPosition> overflow)
    : JustifyContent { PrimaryKind::Start, overflow }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::End, std::optional<OverflowPosition> overflow)
    : JustifyContent { PrimaryKind::End, overflow }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::FlexStart, std::optional<OverflowPosition> overflow)
    : JustifyContent { PrimaryKind::FlexStart, overflow }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::FlexEnd, std::optional<OverflowPosition> overflow)
    : JustifyContent { PrimaryKind::FlexEnd, overflow }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::Left, std::optional<OverflowPosition> overflow)
    : JustifyContent { PrimaryKind::Left, overflow }
{
}

constexpr JustifyContent::JustifyContent(CSS::Keyword::Right, std::optional<OverflowPosition> overflow)
    : JustifyContent { PrimaryKind::Right, overflow }
{
}

constexpr JustifyContent::JustifyContent(PrimaryKind primary)
    : m_primary { static_cast<uint8_t>(primary) }
{
    ASSERT(!isContentPosition(primary));
}

constexpr JustifyContent::JustifyContent(PrimaryKind primary, std::optional<OverflowPosition> overflow)
    : m_primary { static_cast<uint8_t>(primary) }
    , m_secondary { static_cast<uint8_t>(computeKind(overflow)) }
{
    ASSERT(isContentPosition(primary));
}

constexpr JustifyContent::PrimaryKind JustifyContent::primary() const
{
    return static_cast<PrimaryKind>(m_primary);
}

constexpr OverflowPositionKind JustifyContent::overflowPosition() const
{
    RELEASE_ASSERT(isContentPosition(primary()));
    return static_cast<OverflowPositionKind>(m_secondary);
}

constexpr bool JustifyContent::isContentPosition(PrimaryKind primary)
{
    switch (primary) {
    case PrimaryKind::Normal:
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
    case PrimaryKind::Left:
    case PrimaryKind::Right:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename... F> constexpr decltype(auto) JustifyContent::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    auto visitContentPosition = [&](auto primaryKeyword) {
        switch (overflowPosition()) {
        case OverflowPositionKind::None:
            return visitor(primaryKeyword);
        case OverflowPositionKind::Unsafe:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Unsafe { }, primaryKeyword });
        case OverflowPositionKind::Safe:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Safe { }, primaryKeyword });
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    switch (primary()) {
    case PrimaryKind::Normal:
        return visitor(CSS::Keyword::Normal { });
    case PrimaryKind::SpaceBetween:
        return visitor(CSS::Keyword::SpaceBetween { });
    case PrimaryKind::SpaceAround:
        return visitor(CSS::Keyword::SpaceAround { });
    case PrimaryKind::SpaceEvenly:
        return visitor(CSS::Keyword::SpaceEvenly { });
    case PrimaryKind::Stretch:
        return visitor(CSS::Keyword::Stretch { });
    case PrimaryKind::Center:
        return visitContentPosition(CSS::Keyword::Center { });
    case PrimaryKind::Start:
        return visitContentPosition(CSS::Keyword::Start { });
    case PrimaryKind::End:
        return visitContentPosition(CSS::Keyword::End { });
    case PrimaryKind::FlexStart:
        return visitContentPosition(CSS::Keyword::FlexStart { });
    case PrimaryKind::FlexEnd:
        return visitContentPosition(CSS::Keyword::FlexEnd { });
    case PrimaryKind::Left:
        return visitContentPosition(CSS::Keyword::Left { });
    case PrimaryKind::Right:
        return visitContentPosition(CSS::Keyword::Right { });
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

template<> struct CSSValueConversion<JustifyContent> { auto operator()(BuilderState&, const CSSValue&) -> JustifyContent; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::JustifyContent)
