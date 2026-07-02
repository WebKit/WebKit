/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSPropertyParserConsumer+Grid.h"

#include "CSSCustomIdentValue.h"
#include "CSSGridAutoFlowValue.h"
#include "CSSGridLineValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSGridTemplateListValue.h"
#include "CSSGridTrackSizesValue.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+FlexDefinitions.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "StyleGridPosition.h"
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

static std::optional<CSS::CustomIdent> consumeUnresolvedCustomIdentForGridLine(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumeUnresolvedCustomIdentExcluding(range, state, { CSSValueAuto, CSSValueSpan });
}

std::optional<CSS::GridNamedAreaMapRow> consumeUnresolvedGridTemplateAreasRow(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // https://drafts.csswg.org/css-grid/#valdef-grid-template-areas-string

    // Utilize the NRVO by having all paths return this one `row` instance to avoid unnecessary copies.
    std::optional<CSS::GridNamedAreaMapRow> row;

    if (range.peek().type() != StringToken)
        return row;

    auto rowString = range.consumeIncludingWhitespace().value();
    if (rowString.containsOnly<isCSSSpace>())
        return row;

    // Once initial checks are completed, the value can be `emplaced` into the `std::optional` to initialize it in-place.
    row.emplace();

    StringBuilder areaName;
    for (auto character : rowString.codeUnits()) {
        if (isCSSSpace(character)) {
            if (!areaName.isEmpty()) {
                row->append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (character == '.') {
            if (areaName == "."_s)
                continue;
            if (!areaName.isEmpty()) {
                row->append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (!isNameCodePoint(character)) {
                // In this error case, we simply destroy the row in-place, and return it its now `std::nullopt` state.
                row = { };
                return row;
            }
            if (areaName == "."_s) {
                row->append("."_s);
                areaName.clear();
            }
        }
        areaName.append(character);
    }
    if (!areaName.isEmpty())
        row->append(areaName.toAtomString());

    return row;
}

std::optional<CSS::GridLine> consumeUnresolvedGridLine(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <grid-line> = auto
    //             | <custom-ident>
    //             | [ [ <integer [-∞,-1]> | <integer [1,∞]> ] && <custom-ident>? ]
    //             | [ span && [ <integer [1,∞]> || <custom-ident> ] ]
    //
    // https://drafts.csswg.org/css-grid/#typedef-grid-row-start-grid-line

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueAuto:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridLine { CSS::Keyword::Auto { } };

    case CSSValueSpan: {
        range.consumeIncludingWhitespace();

        auto index = MetaConsumer<CSS::Integer<CSS::Positive>>::consume(range, state);
        auto name = consumeUnresolvedCustomIdentForGridLine(range, state);
        if (!index)
            index = MetaConsumer<CSS::Integer<CSS::Positive>>::consume(range, state);

        if (!index && !name)
            return std::nullopt;

        guard.commit();
        return CSS::GridLine { CSS::GridLineSpan {
            .index = index.value_or(CSS::Integer<CSS::Positive> { 1 }),
            .name = WTF::move(name),
        } };
    }

    default:
        break;
    }

    if (auto index = MetaConsumer<CSS::Integer<>>::consume(range, state)) {
        auto name = consumeUnresolvedCustomIdentForGridLine(range, state);

        if (consumeIdentRaw<CSSValueSpan>(range).has_value()) {
            auto rangeCastedIndex = CSS::dynamicRangeNarrowingCast<CSS::Positive>(*index);
            if (!rangeCastedIndex)
                return std::nullopt;

            guard.commit();
            return CSS::GridLine { CSS::GridLineSpan {
                .index = WTF::move(*rangeCastedIndex),
                .name = WTF::move(name),
            } };
        } else {
            if (index->isKnownZero())
                return std::nullopt;

            guard.commit();
            return CSS::GridLine { CSS::GridLineExplicit {
                .index = WTF::move(*index),
                .name = WTF::move(name),
            } };
        }
    }

    auto name = consumeUnresolvedCustomIdentForGridLine(range, state);
    if (!name)
        return std::nullopt;

    auto index = MetaConsumer<CSS::Integer<>>::consume(range, state);

    if (consumeIdentRaw<CSSValueSpan>(range).has_value()) {
        if (index) {
            auto rangeCastedIndex = CSS::dynamicRangeNarrowingCast<CSS::Positive>(*index);
            if (!rangeCastedIndex)
                return std::nullopt;

            guard.commit();
            return CSS::GridLine { CSS::GridLineSpan {
                .index = WTF::move(*rangeCastedIndex),
                .name = WTF::move(name),
            } };
        }

        guard.commit();
        return CSS::GridLine { CSS::GridLineSpan {
            .index = CSS::Integer<CSS::Positive> { 1 },
            .name = WTF::move(name),
        } };
    }

    if (index) {
        if (index->isKnownZero())
            return std::nullopt;

        guard.commit();
        return CSS::GridLine { CSS::GridLineExplicit {
            .index = WTF::move(*index),
            .name = WTF::move(name),
        } };
    }

    guard.commit();
    return CSS::GridLine { WTF::move(*name) };
}

RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <grid-line> = auto
    //             | <custom-ident>
    //             | [ [ <integer [-∞,-1]> | <integer [1,∞]> ] && <custom-ident>? ]
    //             | [ span && [ <integer [1,∞]> || <custom-ident> ] ]
    //
    // https://drafts.csswg.org/css-grid/#typedef-grid-row-start-grid-line

    if (auto unresolved = consumeUnresolvedGridLine(range, state))
        return CSSGridLineValue::create(WTF::move(*unresolved));
    return nullptr;
}

std::optional<CSS::GridLineNames> consumeUnresolvedGridLineNames(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <line-names> = '[' <custom-ident excluding=span,auto>* ']'
    // https://drafts.csswg.org/css-grid/#typedef-line-names

    CSSParserTokenRangeGuard guard { range };

    if (range.consumeIncludingWhitespace().type() != LeftBracketToken)
        return std::nullopt;

    SpaceSeparatedVector<CSS::CustomIdent> lineNames;
    while (auto lineName = consumeUnresolvedCustomIdentForGridLine(range, state))
        lineNames.value.append(WTF::move(*lineName));

    if (range.consumeIncludingWhitespace().type() != RightBracketToken)
        return std::nullopt;

    guard.commit();
    return CSS::GridLineNames { WTF::move(lineNames) };
}

std::optional<CSS::GridTrackList> consumeUnresolvedGridExplicitTrackList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <explicit-track-list> = [ <line-names>? <track-size> ]+ <line-names>?
    // https://drafts.csswg.org/css-grid-2/#typedef-explicit-track-list

    CSSParserTokenRangeGuard guard { range };

    using Track = CSS::GridTrackList::Track;
    SpaceSeparatedVector<Track> trackList;

    if (auto lineNames = consumeUnresolvedGridLineNames(range, state); lineNames && !lineNames->isEmpty())
        trackList.value.append(Track { WTF::move(*lineNames) });

    do {
        auto track = consumeUnresolvedGridTrackSize(range, state);
        if (!track)
            return std::nullopt;

        trackList.value.append(Track { WTF::move(*track) });

        if (auto lineNames = consumeUnresolvedGridLineNames(range, state); lineNames && !lineNames->isEmpty())
            trackList.value.append(Track { WTF::move(*lineNames) });
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);

    guard.commit();
    return CSS::GridTrackList { WTF::move(trackList) };
}

struct GridNameRepeatFunctionResult {
    CSS::GridNameRepeatFunction value;
    bool hasConsumedAutoRepeat { false };
};
static std::optional<GridNameRepeatFunctionResult> consumeUnresolvedGridNameRepeatFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <name-repeat>         = repeat( [ <integer [1,∞]> | auto-fill ], <line-names>+)
    // https://drafts.csswg.org/css-grid/#typedef-name-repeat

    ASSERT(range.peek().functionId() == CSSValueRepeat);

    CSSParserTokenRangeGuard guard { range };
    auto args = consumeFunction(range);

    bool hasConsumedAutoRepeat = false;

    using Repeated = CSS::GridNameRepeatFunctionParameters::Repeated;
    using Repetitions = CSS::GridNameRepeatFunctionParameters::Repetitions;

    std::optional<Repetitions> repetitions;

    switch (args.peek().id()) {
    case CSSValueAutoFill:
        args.consumeIncludingWhitespace();
        repetitions = Repetitions { CSS::Keyword::AutoFill { } };
        hasConsumedAutoRepeat = true;
        break;

    default: {
        auto integerRepetitions = MetaConsumer<CSS::Integer<CSS::Positive, unsigned>>::consume(args, state);
        if (!integerRepetitions)
            return std::nullopt;
        repetitions = WTF::switchOn(*integerRepetitions,
            [&](const CSS::Integer<CSS::Positive, unsigned>::Calc&) {
                return Repetitions { WTF::move(*integerRepetitions) };
            },
            [&](const CSS::Integer<CSS::Positive, unsigned>::Raw& raw) {
                // FIXME: Given this needs to be checked at style building as well to account for calc() and this is not specified anywhere, this clamping to GridPosition::max() should probably be removed.
                return Repetitions { CSS::Integer<CSS::Positive, unsigned> { std::min<double>(raw.value, Style::GridPosition::max()) } };
            }
        );
        break;
    }
    }

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    SpaceSeparatedVector<Repeated> repeated;

    do {
        auto gridLineNames = consumeUnresolvedGridLineNames(args, state);
        if (!gridLineNames)
            return std::nullopt;

        repeated.value.append(WTF::move(*gridLineNames));
    } while (!args.atEnd());

    guard.commit();
    return GridNameRepeatFunctionResult {
        .value = CSS::GridNameRepeatFunction {
            .parameters = {
                .repetitions = WTF::move(*repetitions),
                .repeated = WTF::move(repeated),
            }
        },
        .hasConsumedAutoRepeat = hasConsumedAutoRepeat,
    };
}

static std::optional<CSS::GridSubgrid> consumeUnresolvedGridSubgrid(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <subgrid>             = subgrid <line-name-list>?
    // <line-name-list>      = [ <line-names> | <name-repeat> ]+

    ASSERT(range.peek().id() == CSSValueSubgrid);

    CSSParserTokenRangeGuard guard { range };

    range.consumeIncludingWhitespace();

    bool hasConsumedAutoRepeat = false;

    using Entry = Variant<CSS::GridLineNames, CSS::GridNameRepeatFunction>;
    SpaceSeparatedVector<Entry> entries;

    while (!range.atEnd() && range.peek().type() != DelimiterToken) {
        auto& token = range.peek();
        if (token.functionId() == CSSValueRepeat) {
            auto repeat = consumeUnresolvedGridNameRepeatFunction(range, state);
            if (!repeat)
                return std::nullopt;

            if (repeat->hasConsumedAutoRepeat) {
                // Only one <name-repeat> production is allowed.
                if (hasConsumedAutoRepeat)
                    return std::nullopt;
                hasConsumedAutoRepeat = true;
            }

            entries.value.append(Entry { WTF::move(repeat->value) });
        } else if (token.type() == LeftBracketToken) {
            auto gridLineNames = consumeUnresolvedGridLineNames(range, state);
            if (!gridLineNames)
                return std::nullopt;

            entries.value.append(Entry { WTF::move(*gridLineNames) });
        } else
            return std::nullopt;
    }

    guard.commit();
    return CSS::GridSubgrid { WTF::move(entries) };
}

static std::optional<CSS::GridTrackBreadth> consumeUnresolvedGridInflexibleBreadth(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <inflexible-breadth>  = <length-percentage [0,∞]> | min-content | max-content | auto

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTrackBreadth { CSS::Keyword::MinContent { } };

    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTrackBreadth { CSS::Keyword::MaxContent { } };

    case CSSValueAuto:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTrackBreadth { CSS::Keyword::Auto { } };

    default:
        break;
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state)) {
        guard.commit();
        return CSS::GridTrackBreadth { WTF::move(*lengthPercentage) };
    }

    return std::nullopt;
}

static std::optional<CSS::GridTrackBreadth> consumeUnresolvedGridTrackBreadth(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <track-breadth>       = <length-percentage [0,∞]> | <flex [0,∞]> | min-content | max-content | auto
    // https://drafts.csswg.org/css-grid/#typedef-track-breadth

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTrackBreadth { CSS::Keyword::MinContent { } };

    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTrackBreadth { CSS::Keyword::MaxContent { } };

    case CSSValueAuto:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTrackBreadth { CSS::Keyword::Auto { } };

    default:
        break;
    }

    if (auto flex = MetaConsumer<CSS::Flex<CSS::Nonnegative>>::consume(range, state)) {
        guard.commit();
        return CSS::GridTrackBreadth { WTF::move(*flex) };
    }
    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state)) {
        guard.commit();
        return CSS::GridTrackBreadth { WTF::move(*lengthPercentage) };
    }

    return std::nullopt;
}

struct GridInflexibleOrFixedBreadth {
    CSS::GridTrackBreadth value;
    bool hasConsumedNonFixed { false };
};
static std::optional<GridInflexibleOrFixedBreadth> consumeUnresolvedGridInflexibleOrFixedBreadth(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <inflexible-breadth>  = <length-percentage [0,∞]> | min-content | max-content | auto
    // <fixed-breadth>       = <length-percentage [0,∞]>

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return GridInflexibleOrFixedBreadth {
            .value = CSS::GridTrackBreadth { CSS::Keyword::MinContent { } },
            .hasConsumedNonFixed = true,
        };

    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return GridInflexibleOrFixedBreadth {
            .value = CSS::GridTrackBreadth { CSS::Keyword::MaxContent { } },
            .hasConsumedNonFixed = true,
        };

    case CSSValueAuto:
        range.consumeIncludingWhitespace();
        guard.commit();
        return GridInflexibleOrFixedBreadth {
            .value = CSS::GridTrackBreadth { CSS::Keyword::Auto { } },
            .hasConsumedNonFixed = true,
        };

    default:
        break;
    }

    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state)) {
        guard.commit();
        return GridInflexibleOrFixedBreadth {
            .value = CSS::GridTrackBreadth { WTF::move(*lengthPercentage) },
            .hasConsumedNonFixed = false,
        };
    }

    return std::nullopt;
}

struct GridTrackOrFixedBreadth {
    CSS::GridTrackBreadth value;
    bool hasConsumedNonFixed { false };
};
static std::optional<GridTrackOrFixedBreadth> consumeUnresolvedGridTrackOrFixedBreadth(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <track-breadth>       = <length-percentage [0,∞]> | <flex [0,∞]> | min-content | max-content | auto
    // <fixed-breadth>       = <length-percentage [0,∞]>

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return GridTrackOrFixedBreadth {
            .value = CSS::GridTrackBreadth { CSS::Keyword::MinContent { } },
            .hasConsumedNonFixed = true,
        };

    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        range.consumeIncludingWhitespace();
        guard.commit();
        return GridTrackOrFixedBreadth {
            .value = CSS::GridTrackBreadth { CSS::Keyword::MaxContent { } },
            .hasConsumedNonFixed = true,
        };

    case CSSValueAuto:
        range.consumeIncludingWhitespace();
        guard.commit();
        return GridTrackOrFixedBreadth {
            .value = CSS::GridTrackBreadth { CSS::Keyword::Auto { } },
            .hasConsumedNonFixed = true,
        };

    default:
        break;
    }

    if (auto flex = MetaConsumer<CSS::Flex<CSS::Nonnegative>>::consume(range, state)) {
        guard.commit();
        return GridTrackOrFixedBreadth {
            .value = CSS::GridTrackBreadth { WTF::move(*flex) },
            .hasConsumedNonFixed = true,
        };
    }
    if (auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state)) {
        guard.commit();
        return GridTrackOrFixedBreadth {
            .value = CSS::GridTrackBreadth { WTF::move(*lengthPercentage) },
            .hasConsumedNonFixed = false,
        };
    }

    return std::nullopt;
}

static std::optional<CSS::GridFitContentFunction> consumeUnresolvedGridFitContentFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // fit-content( <length-percentage [0,∞]> )

    ASSERT(range.peek().functionId() == CSSValueFitContent);

    CSSParserTokenRangeGuard guard { range };
    auto args = consumeFunction(range);

    auto lengthPercentage = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(args, state);
    if (!lengthPercentage || !args.atEnd())
        return std::nullopt;

    guard.commit();
    return CSS::GridFitContentFunction {
        .parameters = { WTF::move(*lengthPercentage) }
    };
}

std::optional<CSS::GridTrackSize> consumeUnresolvedGridTrackSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <track-size>          = <track-breadth> | minmax( <inflexible-breadth> , <track-breadth> ) | fit-content( <length-percentage [0,∞]> )

    CSSParserTokenRangeGuard guard { range };

    if (auto trackBreadth = consumeUnresolvedGridTrackBreadth(range, state)) {
        guard.commit();
        return CSS::GridTrackSize { WTF::move(*trackBreadth) };
    }

    switch (range.peek().functionId()) {
    case CSSValueFitContent:
        if (auto fitContentFunction = consumeUnresolvedGridFitContentFunction(range, state)) {
            guard.commit();
            return CSS::GridTrackSize { WTF::move(*fitContentFunction) };
        }
        return std::nullopt;

    case CSSValueMinmax: {
        // minmax( <inflexible-breadth> , <track-breadth> )

        CSSParserTokenRange args = consumeFunction(range);

        auto min = consumeUnresolvedGridInflexibleBreadth(args, state);
        if (!min)
            return std::nullopt;

        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;

        auto max = consumeUnresolvedGridTrackBreadth(args, state);
        if (!max)
            return std::nullopt;

        if (!args.atEnd())
            return std::nullopt;

        guard.commit();
        return CSS::GridTrackSize {
            CSS::GridMinMaxFunction {
                .parameters = {
                    .min = WTF::move(*min),
                    .max = WTF::move(*max),
                }
            }
        };
    }

    default:
        break;
    }

    return std::nullopt;
}


struct GridTrackOrFixedSize {
    CSS::GridTrackSize value;
    bool hasConsumedNonFixed { false };
};
static std::optional<GridTrackOrFixedSize> consumeUnresolvedGridTrackOrFixedSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <track-size>          = <track-breadth> | minmax( <inflexible-breadth> , <track-breadth> ) | fit-content( <length-percentage [0,∞]> )
    // <fixed-size>          = <fixed-breadth> | minmax( <fixed-breadth> , <track-breadth> ) | minmax( <inflexible-breadth> , <fixed-breadth> )

    CSSParserTokenRangeGuard guard { range };

    if (auto trackBreadth = consumeUnresolvedGridTrackOrFixedBreadth(range, state)) {
        guard.commit();
        return GridTrackOrFixedSize {
            .value = CSS::GridTrackSize { WTF::move(trackBreadth->value) },
            .hasConsumedNonFixed = trackBreadth->hasConsumedNonFixed,
        };
    }

    switch (range.peek().functionId()) {
    case CSSValueFitContent:
        if (auto fitContentFunction = consumeUnresolvedGridFitContentFunction(range, state)) {
            guard.commit();
            return GridTrackOrFixedSize {
                .value = CSS::GridTrackSize { WTF::move(*fitContentFunction) },
                .hasConsumedNonFixed = true,
            };
        }
        return std::nullopt;

    case CSSValueMinmax: {
        // Potentially one of:
        //   - minmax( <inflexible-breadth> , <track-breadth> ) (from <track-size>)
        //   - minmax( <fixed-breadth>      , <track-breadth> ) (from <fixed-size>)
        //   - minmax( <inflexible-breadth> , <fixed-breadth> ) (from <fixed-size>)

        CSSParserTokenRange args = consumeFunction(range);

        auto min = consumeUnresolvedGridInflexibleOrFixedBreadth(args, state);
        if (!min)
            return std::nullopt;

        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;

        auto max = consumeUnresolvedGridTrackOrFixedBreadth(args, state);
        if (!max)
            return std::nullopt;

        if (!args.atEnd())
            return std::nullopt;

        guard.commit();
        return GridTrackOrFixedSize {
            .value = CSS::GridTrackSize {
                CSS::GridMinMaxFunction {
                    .parameters = {
                        .min = WTF::move(min->value),
                        .max = WTF::move(max->value),
                    }
                }
            },
            .hasConsumedNonFixed = min->hasConsumedNonFixed && max->hasConsumedNonFixed,
        };
    }
    default:
        break;
    }

    return std::nullopt;
}

struct GridTrackOrFixedOrAutoRepeatResult {
    CSS::GridTrackRepeatFunction value;
    bool hasConsumedNonFixed { false };
    bool hasConsumedAutoRepeat { false };
};
static std::optional<GridTrackOrFixedOrAutoRepeatResult> consumeGridTrackOrFixedOrAutoRepeat(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <track-repeat>        = repeat( [ <integer [1,∞]> ] , [ <line-names>? <track-size> ]+ <line-names>? )
    // <auto-repeat>         = repeat( [ auto-fill | auto-fit ] , [ <line-names>? <fixed-size> ]+ <line-names>? )
    // <fixed-repeat>        = repeat( [ <integer [1,∞]> ] , [ <line-names>? <fixed-size> ]+ <line-names>? )

    ASSERT(range.peek().functionId() == CSSValueRepeat);

    CSSParserTokenRangeGuard guard { range };
    auto args = consumeFunction(range);

    bool hasConsumedNonFixed = false;
    bool hasConsumedAutoRepeat = false;
    size_t numberOfTracks = 0;

    using Repeated = CSS::GridTrackRepeatFunctionParameters::Repeated;
    using Repetitions = CSS::GridTrackRepeatFunctionParameters::Repetitions;

    std::optional<Repetitions> repetitions;

    switch (args.peek().id()) {
    case CSSValueAutoFill:
        args.consumeIncludingWhitespace();
        repetitions = Repetitions { CSS::Keyword::AutoFill { } };
        hasConsumedAutoRepeat = true;
        break;

    case CSSValueAutoFit:
        args.consumeIncludingWhitespace();
        repetitions = Repetitions { CSS::Keyword::AutoFit { } };
        hasConsumedAutoRepeat = true;
        break;

    default: {
        auto integerRepetitions = MetaConsumer<CSS::Integer<CSS::Positive, unsigned>>::consume(args, state);
        if (!integerRepetitions)
            return std::nullopt;
        repetitions = Repetitions { *integerRepetitions };
        break;
    }
    }

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    SpaceSeparatedVector<Repeated> repeated;

    if (auto gridLineNames = consumeUnresolvedGridLineNames(args, state); gridLineNames && !gridLineNames->isEmpty())
        repeated.value.append(Repeated { WTF::move(*gridLineNames) });

    while (!args.atEnd()) {
        auto trackSize = consumeUnresolvedGridTrackOrFixedSize(args, state);
        if (!trackSize)
            return std::nullopt;

        numberOfTracks++;
        hasConsumedNonFixed |= trackSize->hasConsumedNonFixed;

        repeated.value.append(Repeated { WTF::move(trackSize->value) });

        if (auto gridLineNames = consumeUnresolvedGridLineNames(args, state); gridLineNames && !gridLineNames->isEmpty())
            repeated.value.append(Repeated { WTF::move(*gridLineNames) });
    }

    if (!numberOfTracks)
        return std::nullopt;

    if (auto* integerRepetitions = std::get_if<CSS::Integer<CSS::Positive, unsigned>>(&*repetitions)) {
        repetitions = WTF::switchOn(*integerRepetitions,
            [&](const CSS::Integer<CSS::Positive, unsigned>::Calc&) {
                return Repetitions { WTF::move(*integerRepetitions) };
            },
            [&](const CSS::Integer<CSS::Positive, unsigned>::Raw& raw) {
                // FIXME: Given this needs to be checked at style building as well to account for calc() and this is not specified anywhere, this clamping to (GridPosition::max() / numberOfTracks) should probably be removed.
                auto maxRepetitions = Style::GridPosition::max() / numberOfTracks;
                return Repetitions { CSS::Integer<CSS::Positive, unsigned> { std::min<double>(raw.value, maxRepetitions) } };
            }
        );
    }

    guard.commit();
    return GridTrackOrFixedOrAutoRepeatResult {
        .value = CSS::GridTrackRepeatFunction {
            .parameters = {
                .repetitions = WTF::move(*repetitions),
                .repeated = WTF::move(repeated),
            }
        },
        .hasConsumedNonFixed = hasConsumedNonFixed,
        .hasConsumedAutoRepeat = hasConsumedAutoRepeat,
    };
}

std::optional<CSS::GridTemplateList> consumeUnresolvedGridTemplateList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'grid-template-columns'/'grid-template-rows'> = none | <track-list> | <auto-track-list> | subgrid <line-name-list>?
    // https://drafts.csswg.org/css-grid/#propdef-grid-template-columns
    // https://drafts.csswg.org/css-grid/#propdef-grid-template-rows

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueNone:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTemplateList { CSS::Keyword::None { } };

    case CSSValueSubgrid:
        if (auto subgrid = consumeUnresolvedGridSubgrid(range, state)) {
            guard.commit();
            return CSS::GridTemplateList { WTF::move(*subgrid) };
        }
        return std::nullopt;

    default:
        break;
    }

    bool hasConsumedNonFixed = false;
    bool hasConsumedAutoRepeat = false;

    using Track = CSS::GridTrackList::Track;
    SpaceSeparatedVector<Track> trackList;

    if (auto lineNames = consumeUnresolvedGridLineNames(range, state); lineNames && !lineNames->isEmpty())
        trackList.value.append(Track { WTF::move(*lineNames) });

    do {
        if (range.peek().functionId() == CSSValueRepeat) {
            auto repeat = consumeGridTrackOrFixedOrAutoRepeat(range, state);
            if (!repeat)
                return std::nullopt;

            if (repeat->hasConsumedAutoRepeat) {
                // Only one <auto-repeat> production is allowed.
                if (hasConsumedAutoRepeat)
                    return std::nullopt;
                hasConsumedAutoRepeat = true;
            }

            hasConsumedNonFixed |= repeat->hasConsumedNonFixed;
            trackList.value.append(Track { WTF::move(repeat->value) });
        } else if (auto track = consumeUnresolvedGridTrackOrFixedSize(range, state)) {
            hasConsumedNonFixed |= track->hasConsumedNonFixed;
            trackList.value.append(Track { WTF::move(track->value) });
        } else
            return std::nullopt;

        if (auto lineNames = consumeUnresolvedGridLineNames(range, state); lineNames && !lineNames->isEmpty())
            trackList.value.append(Track { WTF::move(*lineNames) });
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);

    if (hasConsumedAutoRepeat && hasConsumedNonFixed)
        return std::nullopt;

    guard.commit();
    return CSS::GridTemplateList { CSS::GridTrackList { WTF::move(trackList) } };
}

RefPtr<CSSValue> consumeGridTemplateList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'grid-template-columns'/'grid-template-rows'> = none | <track-list> | <auto-track-list> | subgrid <line-name-list>?
    // https://drafts.csswg.org/css-grid/#propdef-grid-template-columns
    // https://drafts.csswg.org/css-grid/#propdef-grid-template-rows

    if (auto unresolved = consumeUnresolvedGridTemplateList(range, state)) {
        if (unresolved->isNone())
            return CSSKeywordValue::create(CSSValueNone);
        return CSSGridTemplateListValue::create(WTF::move(*unresolved));
    }
    return nullptr;
}

std::optional<CSS::GridTemplateAreas> consumeUnresolvedGridTemplateAreas(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'grid-template-areas'> = none | <string>+
    // https://drafts.csswg.org/css-grid/#propdef-grid-template-areas

    CSSParserTokenRangeGuard guard { range };

    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridTemplateAreas { CSS::Keyword::None { } };
    }

    CSS::GridNamedAreaMap map;
    do {
        auto row = consumeUnresolvedGridTemplateAreasRow(range, state);
        if (!row || !CSS::addRow(map, *row))
            return std::nullopt;
    } while (range.peek().type() == StringToken);

    if (!map.rowCount)
        return std::nullopt;

    guard.commit();
    return CSS::GridTemplateAreas { WTF::move(map) };
}

RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'grid-template-areas'> = none | <string>+
    // https://drafts.csswg.org/css-grid/#propdef-grid-template-areas

    if (auto unresolved = consumeUnresolvedGridTemplateAreas(range, state)) {
        if (unresolved->isNone())
            return CSSKeywordValue::create(CSSValueNone);
        return CSSGridTemplateAreasValue::create(WTF::move(*unresolved));
    }
    return nullptr;
}

std::optional<CSS::GridTrackSizes> consumeUnresolvedGridTrackSizes(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'grid-auto-columns'>/<'grid-auto-rows'> = <track-size>+
    // https://drafts.csswg.org/css-grid/#propdef-grid-auto-columns
    // https://drafts.csswg.org/css-grid/#propdef-grid-auto-rows

    CSSParserTokenRangeGuard guard { range };

    SpaceSeparatedVector<CSS::GridTrackSize> trackSizes;

    do {
        auto trackSize = consumeUnresolvedGridTrackSize(range, state);
        if (!trackSize)
            return std::nullopt;
        trackSizes.value.append(WTF::move(*trackSize));
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);

    guard.commit();
    return CSS::GridTrackSizes { WTF::move(trackSizes) };
}

RefPtr<CSSValue> consumeGridTrackSizes(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'grid-auto-columns'>/<'grid-auto-rows'> = <track-size>+
    // https://drafts.csswg.org/css-grid/#propdef-grid-auto-columns
    // https://drafts.csswg.org/css-grid/#propdef-grid-auto-rows

    if (auto unresolved = consumeUnresolvedGridTrackSizes(range, state))
        return CSSGridTrackSizesValue::create(WTF::move(*unresolved));
    return nullptr;
}

std::optional<CSS::GridAutoFlow> consumeUnresolvedGridAutoFlow(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <'grid-auto-flow'> = normal | [ [ row | column ] || dense ]
    // FIXME: `normal` is not specified in the link below. Figure out where `normal` comes from and add link.
    // https://drafts.csswg.org/css-grid/#propdef-grid-auto-flow

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueNormal:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::GridAutoFlow { CSS::Keyword::Normal { } };

    case CSSValueRow:
        range.consumeIncludingWhitespace();

        switch (range.peek().id()) {
        case CSSValueDense:
            range.consumeIncludingWhitespace();
            guard.commit();
            return CSS::GridAutoFlow { CSS::Keyword::Row { }, CSS::Keyword::Dense { } };

        default:
            break;
        }

        guard.commit();
        return CSS::GridAutoFlow { CSS::Keyword::Row { } };

    case CSSValueColumn:
        range.consumeIncludingWhitespace();

        switch (range.peek().id()) {
        case CSSValueDense:
            range.consumeIncludingWhitespace();
            guard.commit();
            return CSS::GridAutoFlow { CSS::Keyword::Column { }, CSS::Keyword::Dense { } };

        default:
            break;
        }

        guard.commit();
        return CSS::GridAutoFlow { CSS::Keyword::Column { } };

    case CSSValueDense:
        range.consumeIncludingWhitespace();

        switch (range.peek().id()) {
        case CSSValueRow:
            range.consumeIncludingWhitespace();
            guard.commit();
            return CSS::GridAutoFlow { CSS::Keyword::Row { }, CSS::Keyword::Dense { } };

        case CSSValueColumn:
            range.consumeIncludingWhitespace();
            guard.commit();
            return CSS::GridAutoFlow { CSS::Keyword::Column { }, CSS::Keyword::Dense { } };

        default:
            break;
        }

        guard.commit();
        return CSS::GridAutoFlow { CSS::Keyword::Dense { } };

    default:
        break;
    }

    return std::nullopt;
}

RefPtr<CSSValue> consumeGridAutoFlow(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'grid-auto-flow'> = normal | [ [ row | column ] || dense ]
    // FIXME: `normal` is not specified in the link below. Figure out where `normal` comes from and add link.
    // https://drafts.csswg.org/css-grid/#propdef-grid-auto-flow

    if (auto unresolved = consumeUnresolvedGridAutoFlow(range, state))
        return CSSGridAutoFlowValue::create(WTF::move(*unresolved));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
