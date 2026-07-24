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

// <position-one> = [ left | center | right | top | bottom | x-start | x-end | y-start | y-end | <length-percentage> ]
//
// <position-two> = [
//   [ left | center | right | x-start | x-end ] &&
//   [ top | center | bottom | y-start | y-end ]
// |
//   [ left | center | right | x-start | x-end | <length-percentage> ]
//   [ top | center | bottom | y-start | y-end | <length-percentage> ]
// ]
//
// <bg-position-three> = [
//   [ [        left |  right | x-start | x-end ] <length-percentage> ] &&
//   [ center |  top | bottom | y-start | y-end ]
// |
//   [ center | left |  right | x-start | x-end ] &&
//   [ [         top | bottom | y-start | y-end ] <length-percentage> ]
// ]
//
// <position-four> = [
//   [ [ left | right | x-start | x-end ] <length-percentage> ] &&
//   [ [ top | bottom | y-start | y-end ] <length-percentage> ]
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

static std::optional<PositionUnresolvedComponent> consumePositionUnresolvedComponent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
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
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::XStart { } };
        case CSSValueXEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
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
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::YStart { } };
        case CSSValueYEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::YEnd { } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return PositionUnresolvedComponent { CSS::Keyword::Center { } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return PositionUnresolvedComponent { WTF::move(*lengthPercentage) };
    return { };
}

static std::optional<CSS::Position> positionUnresolvedFromOneComponent(PositionUnresolvedComponent&& component)
{
    // <position-one> = [ left | center | right | top | bottom | x-start | x-end | y-start | y-end | <length-percentage> ]

    return WTF::switchOn(WTF::move(component),
        []<IsHorizontalOnlyComponent C>(C&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component) }, { CSS::Keyword::Center { } } };
        },
        []<IsVerticalOnlyComponent C>(C&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { CSS::Keyword::Center { } }, { WTF::move(component) } };
        },
        [](CSS::Keyword::Center&&) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { CSS::Keyword::Center { } }, { CSS::Keyword::Center { } } };
        },
        [](CSS::LengthPercentage<>&& component) -> std::optional<CSS::Position> {
            return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component) }, { CSS::Keyword::Center { } } };
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
    // ]

    return WTF::switchOn(WTF::move(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ top | center | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTF::move(component2),
                [&]<IsVerticalSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component1) }, { WTF::move(component2) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ left | center | right | x-start | x-end ] (NOTE: <length-percentage> is NOT allowed).
            return WTF::switchOn(WTF::move(component2),
                [&]<IsHorizontalSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component2) }, { WTF::move(component1) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&](CSS::Keyword::Center&& component1) -> std::optional<CSS::Position> {
            // `component2` can be anything.
            return WTF::switchOn(WTF::move(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component2) }, { WTF::move(component1) } };
                },
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component1) }, { WTF::move(component2) } };
                },
                [&](CSS::Keyword::Center&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component1) }, { WTF::move(component2) } };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component1) }, { WTF::move(component2) } };
                }
            );
        },
        [&](CSS::LengthPercentage<>&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ top | center | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTF::move(component2),
                [&]<IsVerticalSecondComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::TwoComponentPositionHorizontalVertical { { WTF::move(component1) }, { WTF::move(component2) } };
                },
                [](auto&&) -> std::optional<CSS::Position> {
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
    // ]

    return WTF::switchOn(WTF::move(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component2` must be in the set [ top | bottom | y-start | y-end | <length-percentage> ]
            return WTF::switchOn(WTF::move(component2),
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                        { { WTF::move(component1) } },
                        { { WTF::move(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be in the set [ center | top | bottom | y-start | y-end ]
                    return WTF::switchOn(WTF::move(component3),
                        [&]<IsVerticalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                                { { WTF::move(component1), WTF::move(component2) } },
                                { { WTF::move(component3) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                                { { WTF::move(component1), WTF::move(component2) } },
                                { { WTF::move(component3) } },
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
            return WTF::switchOn(WTF::move(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be <length-percentage>
                    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component3))
                        return { };
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                        { { WTF::move(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTF::move(component1) } },
                    };
                },
                [&](CSS::LengthPercentage<>&& component2) -> std::optional<CSS::Position> {
                    // `component3` must be in the set [ center | left | right | x-start | x-end ]
                    return WTF::switchOn(WTF::move(component3),
                        [&]<IsHorizontalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                                { { WTF::move(component3) } },
                                { { WTF::move(component1), WTF::move(component2) } },
                            };
                        },
                        [&](CSS::Keyword::Center&& component3) -> std::optional<CSS::Position> {
                            return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                                { { WTF::move(component3) } },
                                { { WTF::move(component1), WTF::move(component2) } },
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

            // `component2` must be in the set [ left | right | x-start | x-end | top | bottom | y-start | y-end ]
            return WTF::switchOn(WTF::move(component2),
                [&]<IsHorizontalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthFirst {
                        { { WTF::move(component2), std::get<CSS::LengthPercentage<>>(component3) } },
                        { { WTF::move(component1) } },
                    };
                },
                [&]<IsVerticalOnlyComponent C2>(C2&& component2) -> std::optional<CSS::Position> {
                    return CSS::ThreeComponentPositionHorizontalVerticalLengthSecond {
                        { { WTF::move(component1) } },
                        { { WTF::move(component2), std::get<CSS::LengthPercentage<>>(component3) } },
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

static std::optional<CSS::Position> positionUnresolvedFromFourComponents(PositionUnresolvedComponent&& component1, PositionUnresolvedComponent&& component2, PositionUnresolvedComponent&& component3, PositionUnresolvedComponent&& component4)
{
    // <position-four> = [
    //   [ [ left | right | x-start | x-end ] <length-percentage> ] &&
    //   [ [ top | bottom | y-start | y-end ] <length-percentage> ]
    // ]

    // `component2` and `component4` must be <length-percentage>
    if (!WTF::holdsAlternative<CSS::LengthPercentage<>>(component2) || !WTF::holdsAlternative<CSS::LengthPercentage<>>(component4))
        return { };

    return WTF::switchOn(WTF::move(component1),
        [&]<IsHorizontalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component3` must be in the set [ top | bottom | y-start | y-end ]
            return WTF::switchOn(WTF::move(component3),
                [&]<IsVerticalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                    return CSS::FourComponentPositionHorizontalVertical {
                        { { WTF::move(component1), std::get<CSS::LengthPercentage<>>(component2) } },
                        { { WTF::move(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                    };
                },
                [](auto&&) -> std::optional<CSS::Position> {
                    return { };
                }
            );
        },
        [&]<IsVerticalOnlyComponent C1>(C1&& component1) -> std::optional<CSS::Position> {
            // `component3` must be in the set [ left | right | x-start | x-end ]
            return WTF::switchOn(WTF::move(component3),
                [&]<IsHorizontalOnlyComponent C3>(C3&& component3) -> std::optional<CSS::Position> {
                    return CSS::FourComponentPositionHorizontalVertical {
                        { { WTF::move(component3), std::get<CSS::LengthPercentage<>>(component4) } },
                        { { WTF::move(component1), std::get<CSS::LengthPercentage<>>(component2) } },
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

std::optional<CSS::Position> consumePositionUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component2) {
        auto position = positionUnresolvedFromOneComponent(WTF::move(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component3 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component3) {
        auto position = positionUnresolvedFromTwoComponents(WTF::move(*component1), WTF::move(*component2));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component4 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component4)
        return { };

    auto position = positionUnresolvedFromFourComponents(WTF::move(*component1), WTF::move(*component2), WTF::move(*component3), WTF::move(*component4));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}

std::optional<CSS::Position> consumeBackgroundPositionUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component2) {
        auto position = positionUnresolvedFromOneComponent(WTF::move(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component3 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component3) {
        auto position = positionUnresolvedFromTwoComponents(WTF::move(*component1), WTF::move(*component2));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto component4 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component4) {
        auto position = positionUnresolvedFromThreeComponents(WTF::move(*component1), WTF::move(*component2), WTF::move(*component3));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto position = positionUnresolvedFromFourComponents(WTF::move(*component1), WTF::move(*component2), WTF::move(*component3), WTF::move(*component4));
    if (!position)
        return { };
    range = rangeCopy;
    return position;
}

std::optional<CSS::PositionX> consumePositionXUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::Left { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::Left { } } };
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::Right { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::Right { } } };
        case CSSValueXStart:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::XStart { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::XStart { } } };
        case CSSValueXEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionX { CSS::FourComponentPositionHorizontal { { CSS::Keyword::XEnd { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::XEnd { } } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSS::PositionX { CSS::TwoComponentPositionHorizontal { CSS::Keyword::Center { } } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return CSS::PositionX { CSS::TwoComponentPositionHorizontal { WTF::move(*lengthPercentage) } };
    return { };
}

std::optional<CSS::PositionY> consumePositionYUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::Top { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::Top { } } };
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::Bottom { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::Bottom { } } };
        case CSSValueYStart:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::YStart { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::YStart { } } };
        case CSSValueYEnd:
            if (!state.context.cssAxisRelativePositionKeywordsEnabled)
                return { };
            range.consumeIncludingWhitespace();
            if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
                return CSS::PositionY { CSS::FourComponentPositionVertical { { CSS::Keyword::YEnd { }, WTF::move(*lengthPercentage) } } };
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::YEnd { } } };
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSS::PositionY { CSS::TwoComponentPositionVertical { CSS::Keyword::Center { } } };
        default:
            return { };
        }
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state))
        return CSS::PositionY { CSS::TwoComponentPositionVertical { WTF::move(*lengthPercentage) } };
    return { };
}

std::optional<CSS::Position> consumeOneOrTwoComponentPositionUnresolved(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto component1 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component1)
        return { };

    auto component2 = consumePositionUnresolvedComponent(rangeCopy, state);
    if (!component2) {
        auto position = positionUnresolvedFromOneComponent(WTF::move(*component1));
        if (!position)
            return { };
        range = rangeCopy;
        return position;
    }

    auto position = positionUnresolvedFromTwoComponents(WTF::move(*component1), WTF::move(*component2));
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
        return CSS::TwoComponentPositionHorizontal { WTF::move(*lengthPercentage) };
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
        return CSS::TwoComponentPositionVertical { WTF::move(*lengthPercentage) };
    return { };
}

// MARK: CSSValue

RefPtr<CSSValue> consumePosition(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto position = consumePositionUnresolved(range, state))
        return CSSPositionValue::create(WTF::move(*position));
    return nullptr;
}

RefPtr<CSSValue> consumePositionX(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto positionX = consumePositionXUnresolved(range, state))
        return CSSPositionXValue::create(WTF::move(*positionX));
    return nullptr;
}

RefPtr<CSSValue> consumePositionY(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto positionY = consumePositionYUnresolved(range, state))
        return CSSPositionYValue::create(WTF::move(*positionY));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
