/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "CSSPropertyParserConsumer+Position.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserState.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "RenderStyleConstants.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

using namespace CSS::Literals;

// MARK: <position>
// https://drafts.csswg.org/css-values/#position

// <position> = <position-one> | <position-two> | <position-four>

// MARK: <bg-position>
// https://drafts.csswg.org/css-backgrounds-3/#propdef-background-position

// background-position has special parsing rules, allowing a 3-value syntax:
//
// <bg-position> = <position-one> | <position-two> | <bg-position-three> | <position-four>

// Sub-productions

// <position-one> = [ left | center | right | top | bottom | x-start | x-end | y-start | y-end | block-start | block-end | inline-start | inline-end | <length-percentage> ]
//
// <position-two> = [
//   [ left | center | right | x-start | x-end ] &&
//   [ top | center | bottom | y-start | y-end ]
// |
//   [ left | center | right | x-start | x-end | <length-percentage> ]
//   [ top | center | bottom | y-start | y-end | <length-percentage> ]
// |
//   [ block-start | center | block-end ] &&
//   [ inline-start | center | inline-end ]
// |
//   [ start | center | end ]{2}
// ]
//
// <bg-position-three> = [
//   [ [        left |  right | x-start | x-end ] <length-percentage> ] &&
//   [ center |  top | bottom | y-start | y-end ]
// |
//   [ center | left |  right | x-start | x-end ] &&
//   [ [         top | bottom | y-start | y-end ] <length-percentage> ]
// |
//   [ [         block-start |  block-end ] <length-percentage> ] &&
//   [ center | inline-start | inline-end ]
// |
//   [ center |  block-start |  block-end ] &&
//   [ [        inline-start | inline-end ] <length-percentage> ]
// |
//   [ [        start | end ] <length-percentage> ]
//   [ center | start | end ]
// |
//   [ center | start | end ]
//   [ [        start | end ] <length-percentage> ]
// ]
//
// <position-four> = [
//   [ [ left | right | x-start | x-end ] <length-percentage> ] &&
//   [ [ top | bottom | y-start | y-end ] <length-percentage> ]
// |
//   [ [ block-start | block-end ] <length-percentage> ] &&
//   [ [ inline-start | inline-end ] <length-percentage> ]
// |
//   [ [ start | end ] <length-percentage> ]{2}
// ]

// MARK: Unresolved Position

using PositionUnresolvedComponent = Variant<
    // Horizontal
    CSS::Keyword::Left,
    CSS::Keyword::Right,
    CSS::Keyword::XStart,
    CSS::Keyword::XEnd,

    // Vertical
    CSS::Keyword::Top,
    CSS::Keyword::Bottom,
    CSS::Keyword::YStart,
    CSS::Keyword::YEnd,

    // Flow
    CSS::Keyword::BlockStart,
    CSS::Keyword::BlockEnd,
    CSS::Keyword::InlineStart,
    CSS::Keyword::InlineEnd,
    CSS::Keyword::Start,
    CSS::Keyword::End,

    // Any Axis
    CSS::Keyword::Center,
    CSS::LengthPercentage<>
>;

// MARK: Predicate matching concepts

template<typename T> concept IsHorizontalOnlyComponent =
       std::same_as<T, CSS::Keyword::Left>
    || std::same_as<T, CSS::Keyword::Right>
    || std::same_as<T, CSS::Keyword::XStart>
    || std::same_as<T, CSS::Keyword::XEnd>;

template<typename T> concept IsHorizontalSecondComponent =
       IsHorizontalOnlyComponent<T>
    || std::same_as<T, CSS::Keyword::Center>;

template<typename T> concept IsVerticalOnlyComponent =
       std::same_as<T, CSS::Keyword::Top>
    || std::same_as<T, CSS::Keyword::Bottom>
    || std::same_as<T, CSS::Keyword::YStart>
    || std::same_as<T, CSS::Keyword::YEnd>;

template<typename T> concept IsVerticalSecondComponent =
       IsVerticalOnlyComponent<T>
    || std::same_as<T, CSS::Keyword::Center>
    || std::same_as<T, CSS::LengthPercentage<>>;

template<typename T> concept IsBlockOnlyComponent =
       std::same_as<T, CSS::Keyword::BlockStart>
    || std::same_as<T, CSS::Keyword::BlockEnd>;

template<typename T> concept IsBlockSecondComponent =
       IsBlockOnlyComponent<T>
    || std::same_as<T, CSS::Keyword::Center>;

template<typename T> concept IsInlineOnlyComponent =
       std::same_as<T, CSS::Keyword::InlineStart>
    || std::same_as<T, CSS::Keyword::InlineEnd>;

template<typename T> concept IsInlineSecondComponent =
       IsInlineOnlyComponent<T>
    || std::same_as<T, CSS::Keyword::Center>;

template<typename T> concept IsStartEndOnlyComponent =
       std::same_as<T, CSS::Keyword::Start>
    || std::same_as<T, CSS::Keyword::End>;

template<typename T> concept IsStartEndSecondComponent =
       IsStartEndOnlyComponent<T>
    || std::same_as<T, CSS::Keyword::Center>;


static std::optional<PositionUnresolvedComponent> consumePositionUnresolvedComponent(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<AllowedPositionKeywords> allowedKeywords)
{
    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::Left { } };
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::Right { } };
        case CSSValueXStart:
            if (!allowedKeywords.contains(AllowedPositionKeywords::AxisRelative) || !state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::XStart { } };
        case CSSValueXEnd:
            if (!allowedKeywords.contains(AllowedPositionKeywords::AxisRelative) || !state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::XEnd { } };
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::Bottom { } };
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::Top { } };
        case CSSValueYStart:
            if (!allowedKeywords.contains(AllowedPositionKeywords::AxisRelative) || !state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::YStart { } };
        case CSSValueYEnd:
            if (!allowedKeywords.contains(AllowedPositionKeywords::AxisRelative) || !state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::YEnd { } };
        case CSSValueBlockStart:
            if (!allowedKeywords.contains(AllowedPositionKeywords::FlowRelative) || !state.context.cssFlowRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::BlockStart { } };
        case CSSValueBlockEnd:
            if (!allowedKeywords.contains(AllowedPositionKeywords::FlowRelative) || !state.context.cssFlowRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::BlockEnd { } };
        case CSSValueInlineStart:
            if (!allowedKeywords.contains(AllowedPositionKeywords::FlowRelative) || !state.context.cssFlowRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::InlineStart { } };
        case CSSValueInlineEnd:
            if (!allowedKeywords.contains(AllowedPositionKeywords::FlowRelative) || !state.context.cssFlowRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::InlineEnd { } };
        case CSSValueStart:
            if (!allowedKeywords.contains(AllowedPositionKeywords::FlowRelative) || !state.context.cssFlowRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::Start { } };
        case CSSValueEnd:
            if (!allowedKeywords.contains(AllowedPositionKeywords::FlowRelative) || !state.context.cssFlowRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::End { } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::Center { } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return PositionUnresolvedComponent { WTFMove(*lengthPercentage) };
    return { };
}

static std::optional<CSS::Position> positionUnresolvedFromOneComponent(PositionUnresolvedComponent&& component)
{
    // <position-one> = [ left | center | right | top | bottom | x-start | x-end | y-start | y-end | block-start | block-end | inline-start | inline-end | <length-percentage> ]

    return WTF::switchOn(WTFMove(component),
        []<IsHorizontalOnlyComponent C>(C&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component) }, { CSS::Keyword::Center { } } };
        },
        []<IsVerticalOnlyComponent C>(C&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { CSS::Keyword::Center { } }, { WTFMove(component) } };
        },
        []<IsBlockOnlyComponent C>(C&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionBlockInline { { WTFMove(component) }, { CSS::Keyword::Center { } } };
        },
        []<IsInlineOnlyComponent C>(C&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionBlockInline { { CSS::Keyword::Center { } }, { WTFMove(component) } };
        },
        []<IsStartEndOnlyComponent C>(C&&) -> std::optional<CSS::Position> {
            // `start` and `end` are invalid for single component position values.
            return { };
        },
        [](CSS::Keyword::Center&&) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { CSS::Keyword::Center { } }, { CSS::Keyword::Center { } } };
        },
        [](CSS::LengthPercentage<>&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component) }, { CSS::Keyword::Center { } } };
        }
    );
}

static std::optional<CSS::PositionXY> positionXYUnresolvedFromOneComponent(PositionUnresolvedComponent&& component)
{
    // Same as <position-one>, but without the flow relative keywords.

    return WTF::switchOn(WTFMove(component),
        []<IsHorizontalOnlyComponent C>(C&& component) -> std::optional<CSS::PositionXY> {
            return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component) }, { CSS::Keyword::Center { } } };
        },
        []<IsVerticalOnlyComponent C>(C&& component) -> std::optional<CSS::PositionXY> {
            return CSS::TwoComponentPositionHorizontalVertical { { CSS::Keyword::Center { } }, { WTFMove(component) } };
        },
        []<IsBlockOnlyComponent C>(C&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        []<IsInlineOnlyComponent C>(C&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        []<IsStartEndOnlyComponent C>(C&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [](CSS::Keyword::Center&&) -> std::optional<CSS::PositionXY> {
            return CSS::TwoComponentPositionHorizontalVertical { { CSS::Keyword::Center { } }, { CSS::Keyword::Center { } } };
        },
        [](CSS::LengthPercentage<>&& component) -> std::optional<CSS::PositionXY> {
            return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component) }, { CSS::Keyword::Center { } } };
        }
    );
}

static std::optional<CSS::Position> positionUnresolvedFromTwoComponents(PositionUnresolvedComponent&& component1, PositionUnresolvedComponent&& component2)
{
    // <position-two> = [
    //   [ left | center | right | x-start | x-end ] &&
    //   [ top | center | bottom | y-start | y-end ]
    // |
    //   [ left | center | right | x-start | x-end | <length-percentage> ]
    //   [ top | center | bottom | y-start | y-end | <length-percentage> ]
    // |
    //   [ block-start | center | block-end ] &&
    //   [ inline-start | center | inline-end ]
    // |
    //   [ start | center | end ]{2}
    // ]

    return WTF::switchOn(WTFMove(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ top | center | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsVerticalSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ left | center | right | x-start | x-end ] (NOTE: <length-percentage> is NOT allowed).
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component2) }, { WTFMove(component1) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsBlockOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ inline-start | center | inline-end ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsInlineSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionBlockInline { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsInlineOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ block-start | center | block-end ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsBlockSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionBlockInline { { WTFMove(component2) }, { WTFMove(component1) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsStartEndOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ start | center | end ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsStartEndSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionStartEnd { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&](CSS::Keyword::Center&& component1) -> std::optional<CSS::Position> {
            // `component2` can be anything.
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component2) }, { WTFMove(component1) } };
                },
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [&]<IsBlockOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionBlockInline { { WTFMove(component2) }, { WTFMove(component1) } };
                },
                [&]<IsInlineOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionBlockInline { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [&]<IsStartEndOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionStartEnd { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [&](CSS::Keyword::Center&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                }
            );
        },
        [&](CSS::LengthPercentage<>&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ top | center | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsVerticalSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        }
    );
}

static std::optional<CSS::PositionXY> positionXYUnresolvedFromTwoComponents(PositionUnresolvedComponent&& component1, PositionUnresolvedComponent&& component2)
{
    // Same as <position-two>, but without the flow relative keywords.

    return WTF::switchOn(WTFMove(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::PositionXY> {
            // `component2` must be in the set [ top | center | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsVerticalSecondComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::PositionXY> {
            // `component2` must be in the set [ left | center | right | x-start | x-end ] (NOTE: <length-percentage> is NOT allowed).
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalSecondComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component2) }, { WTFMove(component1) } };
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        },
        [&]<IsBlockOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&]<IsInlineOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&]<IsStartEndOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&](CSS::Keyword::Center&& component1) -> std::optional<CSS::PositionXY> {
            // `component2` can be anything.
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component2) }, { WTFMove(component1) } };
                },
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [&]<IsBlockOnlyComponent C2>(C2&&) -> std::optional<CSS::PositionXY> {
                    return { };
                },
                [&]<IsInlineOnlyComponent C2>(C2&&) -> std::optional<CSS::PositionXY> {
                    return { };
                },
                [&]<IsStartEndOnlyComponent C2>(C2&&) -> std::optional<CSS::PositionXY> {
                    return { };
                },
                [&](CSS::Keyword::Center&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                }
            );
        },
        [&](CSS::LengthPercentage<>&& component1) -> std::optional<CSS::PositionXY> {
            // `component2` must be in the set [ top | center | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsVerticalSecondComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTFMove(component1) }, { WTFMove(component2) } };
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        }
    );
}

static std::optional<CSS::Position> positionUnresolvedFromThreeComponents(PositionUnresolvedComponent&& component1, PositionUnresolvedComponent&& component2, PositionUnresolvedComponent&& component3)
{
    // Special case only for <bg-position> productions.

    // <bg-position-three> = [
    //   [ [        left |  right | x-start | x-end ] <length-percentage> ] &&
    //   [ center |  top | bottom | y-start | y-end ]
    // |
    //   [ center | left |  right | x-start | x-end ] &&
    //   [ [         top | bottom | y-start | y-end ] <length-percentage> ]
    // |
    //   [ [         block-start |  block-end ] <length-percentage> ] &&
    //   [ center | inline-start | inline-end ]
    // |
    //   [ center |  block-start |  block-end ] &&
    //   [ [        inline-start | inline-end ] <length-percentage> ]
    // |
    //   [ [        start | end ] <length-percentage> ]
    //   [ center | start | end ]
    // |
    //   [ center | start | end ]
    //   [ [        start | end ] <length-percentage> ]
    // ]

    return WTF::switchOn(WTFMove(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ top | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be in the set [ center | top | bottom | y-start | y-end ]
                    return WTF::switchOn(WTFMove(component3),
                        [&]<IsVerticalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [](auto&&) -> std::optional<CSS::Position> {
                            return { };
                        }
                    );
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ left | right | x-start | x-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTFMove(component1) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be in the set [ center | left | right | x-start | x-end ]
                    return WTF::switchOn(WTFMove(component3),
                        [&]<IsHorizontalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                                { { WTFMove(component3) } },
                                { { WTFMove(component1), WTFMove(component2) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                                { { WTFMove(component3) } },
                                { { WTFMove(component1), WTFMove(component2) } },
                            };
                        },
                        [](auto&&) -> std::optional<CSS::Position> {
                            return { };
                        }
                    );
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsBlockOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ inline-start | inline-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsInlineOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionBlockInlineLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be in the set [ center | inline-start | inline-end ]
                    return WTF::switchOn(WTFMove(component3),
                        [&]<IsInlineOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionBlockInlineLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionBlockInlineLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [](auto&&) -> std::optional<CSS::Position> {
                            return { };
                        }
                    );
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsInlineOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ block-start | block-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsBlockOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionBlockInlineLengthFirst {
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTFMove(component1) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be in the set [ center | block-start | block-end ]
                    return WTF::switchOn(WTFMove(component3),
                        [&]<IsBlockOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionBlockInlineLengthSecond {
                                { { WTFMove(component3) } },
                                { { WTFMove(component1), WTFMove(component2) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionBlockInlineLengthSecond {
                                { { WTFMove(component3) } },
                                { { WTFMove(component1), WTFMove(component2) } },
                            };
                        },
                        [](auto&&) -> std::optional<CSS::Position> {
                            return { };
                        }
                    );
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsStartEndOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ start | end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsStartEndOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionStartEndLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be in the set [ center | start | end ]
                    return WTF::switchOn(WTFMove(component3),
                        [&]<IsStartEndOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionStartEndLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionStartEndLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [](auto&&) -> std::optional<CSS::Position> {
                            return { };
                        }
                    );
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&](CSS::Keyword::Center&& component1) -> std::optional<CSS::Position> {
            // `component3` must be <length-percentage>
            if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                return { };

            // `component2` must be in the set [ left | right | x-start | x-end | top | bottom | y-start | y-end | block-start | block-end inline-start | inline-end | start | end ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTFMove(component1) } },
                    };
                },
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&]<IsBlockOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::ThreeComponentPositionBlockInlineLengthFirst {
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTFMove(component1) } },
                    };
                },
                [&]<IsInlineOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::ThreeComponentPositionBlockInlineLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&]<IsStartEndOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::ThreeComponentPositionStartEndLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&](CSS::LengthPercentage<>&&) -> std::optional<CSS::Position> {
            // `<length-percentage>` is invalid for the first component of three component position values.
            return { };
        }
    );
}

static std::optional<CSS::PositionXY> positionXYUnresolvedFromThreeComponents(PositionUnresolvedComponent&& component1, PositionUnresolvedComponent&& component2, PositionUnresolvedComponent&& component3)
{
    // Same as <bg-position-three>, but without the flow relative keywords.

    return WTF::switchOn(WTFMove(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::PositionXY> {
            // `component2` must be in the set [ top | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::PositionXY> {
                    // `component3` must be in the set [ center | top | bottom | y-start | y-end ]
                    return WTF::switchOn(WTFMove(component3),
                        [&]<IsVerticalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::PositionXY> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::PositionXY> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                                { { WTFMove(component1), WTFMove(component2) } },
                                { { WTFMove(component3) } },
                            };
                        },
                        [](auto&&) -> std::optional<CSS::PositionXY> {
                            return { };
                        }
                    );
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::PositionXY> {
            // `component2` must be in the set [ left | right | x-start | x-end | <length-percentage> ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTFMove(component1) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::PositionXY> {
                    // `component3` must be in the set [ center | left | right | x-start | x-end ]
                    return WTF::switchOn(WTFMove(component3),
                        [&]<IsHorizontalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::PositionXY> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                                { { WTFMove(component3) } },
                                { { WTFMove(component1), WTFMove(component2) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::PositionXY> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                                { { WTFMove(component3) } },
                                { { WTFMove(component1), WTFMove(component2) } },
                            };
                        },
                        [](auto&&) -> std::optional<CSS::PositionXY> {
                            return { };
                        }
                    );
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        },
        [&]<IsBlockOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&]<IsInlineOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&]<IsStartEndOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&](CSS::Keyword::Center&& component1) -> std::optional<CSS::PositionXY> {
            // `component3` must be <length-percentage>
            if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                return { };

            // `component2` must be in the set [ left | right | x-start | x-end | top | bottom | y-start | y-end ]
            return WTF::switchOn(WTFMove(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTFMove(component1) } },
                    };
                },
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::PositionXY> {
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                        { { WTFMove(component1) } },
                        { { WTFMove(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&]<IsBlockOnlyComponent C2>(C2&&) -> std::optional<CSS::PositionXY> {
                    return { };
                },
                [&]<IsInlineOnlyComponent C2>(C2&&) -> std::optional<CSS::PositionXY> {
                    return { };
                },
                [&]<IsStartEndOnlyComponent C2>(C2&&) -> std::optional<CSS::PositionXY> {
                    return { };
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        },
        [&](CSS::LengthPercentage<>&&) -> std::optional<CSS::PositionXY> {
            // `<length-percentage>` is invalid for the first component of three component position values.
            return { };
        }
    );
}

static std::optional<CSS::Position> positionUnresolvedFromFourComponents(PositionUnresolvedComponent&& component1, PositionUnresolvedComponent&& component2, PositionUnresolvedComponent&& component3, PositionUnresolvedComponent&& component4)
{
    // <position-four> = [
    //   [ [ left | right | x-start | x-end ] <length-percentage> ] &&
    //   [ [ top | bottom | y-start | y-end ] <length-percentage> ]
    // |
    //   [ [ block-start | block-end ] <length-percentage> ] &&
    //   [ [ inline-start | inline-end ] <length-percentage> ]
    // |
    //   [ [ start | end ] <length-percentage> ]{2}
    // ]

    // `component2` and `component4` must be <length-percentage>
    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component2) || !WTF::holdsAlternative<CSS::LengthPercentage<>>(component4))
        return { };

    return WTF::switchOn(WTFMove(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component3` must be in the set [ top | bottom | y-start | y-end ]
            return WTF::switchOn(WTFMove(component3),
                [&]<IsVerticalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                    return CSS::FourComponentPositionHorizontalVertical {
                        { { WTFMove(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                        { { WTFMove(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component3` must be in the set [ left | right | x-start | x-end ]
            return WTF::switchOn(WTFMove(component3),
                [&]<IsHorizontalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                    return CSS::FourComponentPositionHorizontalVertical {
                        { { WTFMove(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                        { { WTFMove(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsBlockOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component3` must be in the set [ inline-start | inline-end ]
            return WTF::switchOn(WTFMove(component3),
                [&]<IsInlineOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                    return CSS::FourComponentPositionBlockInline {
                        { { WTFMove(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                        { { WTFMove(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsInlineOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component3` must be in the set [ block-start | block-end ]
            return WTF::switchOn(WTFMove(component3),
                [&]<IsBlockOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                    return CSS::FourComponentPositionBlockInline {
                        { { WTFMove(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                        { { WTFMove(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsStartEndOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component3` must be in the set [ start | end ]
            return WTF::switchOn(WTFMove(component3),
                [&]<IsStartEndOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                    return CSS::FourComponentPositionStartEnd {
                        { { WTFMove(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                        { { WTFMove(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&](CSS::Keyword::Center&&) -> std::optional<CSS::Position> {
            // `center` is invalid for the first component of four component position values.
            return { };
        },
        [&](CSS::LengthPercentage<>&&) -> std::optional<CSS::Position> {
            // `<length-percentage>` is invalid for the first component of four component position values.
            return { };
        }
    );
}

static std::optional<CSS::PositionXY> positionXYUnresolvedFromFourComponents(PositionUnresolvedComponent&& component1, PositionUnresolvedComponent&& component2, PositionUnresolvedComponent&& component3, PositionUnresolvedComponent&& component4)
{
    // <position-four> = [
    // `component2` and `component4` must be <length-percentage>
    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component2) || !WTF::holdsAlternative<CSS::LengthPercentage<>>(component4))
        return { };

    return WTF::switchOn(WTFMove(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::PositionXY> {
            // `component3` must be in the set [ top | bottom | y-start | y-end ]
            return WTF::switchOn(WTFMove(component3),
                [&]<IsVerticalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::PositionXY> {
                    return CSS::FourComponentPositionHorizontalVertical {
                        { { WTFMove(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                        { { WTFMove(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::PositionXY> {
            // `component3` must be in the set [ left | right | x-start | x-end ]
            return WTF::switchOn(WTFMove(component3),
                [&]<IsHorizontalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::PositionXY> {
                    return CSS::FourComponentPositionHorizontalVertical {
                        { { WTFMove(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                        { { WTFMove(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::PositionXY> {
                    return { };
                }
            );
        },
        [&]<IsBlockOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&]<IsInlineOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&]<IsStartEndOnlyComponent C1>(C1&&) -> std::optional<CSS::PositionXY> {
            return { };
        },
        [&](CSS::Keyword::Center&&) -> std::optional<CSS::PositionXY> {
            // `center` is invalid for the first component of four component position values.
            return { };
        },
        [&](CSS::LengthPercentage<>&&) -> std::optional<CSS::PositionXY> {
            // `<length-percentage>` is invalid for the first component of four component position values.
            return { };
        }
    );
}

std::optional<CSS::Position> consumePositionUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component2) {
        auto position = positionUnresolvedFromOneComponent(WTFMove(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component3 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component3) {
        auto position = positionUnresolvedFromTwoComponents(WTFMove(*component1), WTFMove(*component2));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component4 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component4)
        return { };

    auto position = positionUnresolvedFromFourComponents(WTFMove(*component1), WTFMove(*component2), WTFMove(*component3), WTFMove(*component4));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}

std::optional<CSS::PositionXY> consumePositionXYUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component2) {
        auto position = positionXYUnresolvedFromOneComponent(WTFMove(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component3 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component3) {
        auto position = positionXYUnresolvedFromTwoComponents(WTFMove(*component1), WTFMove(*component2));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component4 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component4)
        return { };

    auto position = positionXYUnresolvedFromFourComponents(WTFMove(*component1), WTFMove(*component2), WTFMove(*component3), WTFMove(*component4));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}


std::optional<CSS::Position> consumeBackgroundPositionUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component2) {
        auto position = positionUnresolvedFromOneComponent(WTFMove(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component3 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component3) {
        auto position = positionUnresolvedFromTwoComponents(WTFMove(*component1), WTFMove(*component2));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component4 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative, AllowedPositionKeywords::FlowRelative });
    if (!component4) {
        auto position = positionUnresolvedFromThreeComponents(WTFMove(*component1), WTFMove(*component2), WTFMove(*component3));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto position = positionUnresolvedFromFourComponents(WTFMove(*component1), WTFMove(*component2), WTFMove(*component3), WTFMove(*component4));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}

std::optional<CSS::PositionXY> consumeBackgroundPositionXYUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component2) {
        auto position = positionXYUnresolvedFromOneComponent(WTFMove(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component3 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component3) {
        auto position = positionXYUnresolvedFromTwoComponents(WTFMove(*component1), WTFMove(*component2));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component4 = consumePositionUnresolvedComponent(rangeCopy, state, { AllowedPositionKeywords::AxisRelative });
    if (!component4) {
        auto position = positionXYUnresolvedFromThreeComponents(WTFMove(*component1), WTFMove(*component2), WTFMove(*component3));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto position = positionXYUnresolvedFromFourComponents(WTFMove(*component1), WTFMove(*component2), WTFMove(*component3), WTFMove(*component4));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}

std::optional<CSS::PositionX> consumePositionXUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <position-x> = [
    //   center | left | right | x-start | x-end | <length-percentage>
    // |
    //          [ left | right | x-start | x-end ] <length-percentage>
    // ]
    // https://drafts.csswg.org/css-backgrounds-4/#propdef-background-position-x

    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::Left { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::Left { } } };
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::Right { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::Right { } } };
        case CSSValueXStart:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::XStart { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::XStart { } } };
        case CSSValueXEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::XEnd { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::XEnd { } } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::Center { } } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return CSS::PositionX { CSS::TwoComponentPositionHorizontal { WTFMove(*lengthPercentage) } };
    return { };
}

std::optional<CSS::PositionY> consumePositionYUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <position-y> = [
    //   center | top | bottom | y-start | y-end | <length-percentage>
    // |
    //          [ top | bottom | y-start | y-end ] <length-percentage>
    // ]
    // https://drafts.csswg.org/css-backgrounds-4/#propdef-background-position-y

    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::Top { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::Top { } } };
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::Bottom { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::Bottom { } } };
        case CSSValueYStart:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::YStart { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::YStart { } } };
        case CSSValueYEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::YEnd { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::YEnd { } } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::Center { } } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return CSS::PositionY { CSS::TwoComponentPositionVertical { WTFMove(*lengthPercentage) } };
    return { };
}

static std::optional<CSS::PositionLogical> consumePositionLogicalUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <position-block>/<position-inline> = [
    //   center | start | end | <length-percentage>
    // |
    //          [ start | end ] <length-percentage>
    // ]
    // https://drafts.csswg.org/css-backgrounds-4/#propdef-background-position-block

    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueStart:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionLogical { CSS::FourComponentPositionLogical { { CSS::Keyword::Start { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionLogical { CSS::TwoComponentPositionLogical { CSS::Keyword::Start { } } };
        case CSSValueEnd:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionLogical { CSS::FourComponentPositionLogical { { CSS::Keyword::End { }, WTFMove(*lengthPercentage) } } };
            return CSS::PositionLogical { CSS::TwoComponentPositionLogical { CSS::Keyword::End { } } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSS::PositionLogical { CSS::TwoComponentPositionLogical { CSS::Keyword::Center { } } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return CSS::PositionLogical { CSS::TwoComponentPositionLogical { WTFMove(*lengthPercentage) } };
    return { };
}

std::optional<CSS::PositionLogical> consumePositionBlockUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumePositionLogicalUnresolved(range, state);
}

std::optional<CSS::PositionLogical> consumePositionInlineUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumePositionLogicalUnresolved(range, state);
}

std::optional<CSS::Position> consumeLegacyBackgroundPositionUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state, { });
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state, { });
    if (!component2) {
        auto position = positionUnresolvedFromOneComponent(WTFMove(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto position = positionUnresolvedFromTwoComponents(WTFMove(*component1), WTFMove(*component2));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}

std::optional<CSS::PositionXY> consumeTransformOriginXY(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state, { });
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state, { });
    if (!component2) {
        auto position = positionXYUnresolvedFromOneComponent(WTFMove(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto position = positionXYUnresolvedFromTwoComponents(WTFMove(*component1), WTFMove(*component2));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}

std::optional<CSS::TwoComponentPositionHorizontal> consumeTwoComponentPositionHorizontalUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionHorizontal { CSS::Keyword::Left { } };
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionHorizontal { CSS::Keyword::Right { } };
        case CSSValueXStart:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionHorizontal { CSS::Keyword::XStart { } };
        case CSSValueXEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionHorizontal { CSS::Keyword::XEnd { } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionHorizontal { CSS::Keyword::Center { } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return CSS::TwoComponentPositionHorizontal { WTFMove(*lengthPercentage) };
    return { };
}

std::optional<CSS::TwoComponentPositionVertical> consumeTwoComponentPositionVerticalUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionVertical { CSS::Keyword::Bottom { } };
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionVertical { CSS::Keyword::Top { } };
        case CSSValueYStart:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionVertical { CSS::Keyword::YStart { } };
        case CSSValueYEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionVertical { CSS::Keyword::YEnd { } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSS::TwoComponentPositionVertical { CSS::Keyword::Center { } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return CSS::TwoComponentPositionVertical { WTFMove(*lengthPercentage) };
    return { };
}

// MARK: CSSValue

RefPtr<CSSValue> consumePosition(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto position = consumePositionUnresolved(range, state))
        return CSSPositionValue::create(WTFMove(*position));
    return nullptr;
}

RefPtr<CSSValue> consumePositionX(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto positionX = consumePositionXUnresolved(range, state))
        return CSSPositionXValue::create(WTFMove(*positionX));
    return nullptr;
}

RefPtr<CSSValue> consumePositionY(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto positionY = consumePositionYUnresolved(range, state))
        return CSSPositionYValue::create(WTFMove(*positionY));
    return nullptr;
}

RefPtr<CSSValue> consumePositionBlock(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto positionLogical = consumePositionBlockUnresolved(range, state))
        return CSSPositionLogicalValue::create(WTFMove(*positionLogical));
    return nullptr;
}

RefPtr<CSSValue> consumePositionInline(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto positionLogical = consumePositionInlineUnresolved(range, state))
        return CSSPositionLogicalValue::create(WTFMove(*positionLogical));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
