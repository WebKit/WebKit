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

#include "config.h"
#include "StyleGridTemplateList.h"

#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSSubgridValue.h"
#include "CSSValueList.h"
#include "RenderStyleInlines.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

GridTemplateList::GridTemplateList(GridTrackList&& entries)
    : list { WTFMove(entries) }
{
    if (list.isEmpty())
        return;

    auto createGridLineNamesList = [](const auto& names, auto currentNamedGridLine, auto& namedGridLines, auto& orderedNamedGridLines) {
        auto orderedResult = orderedNamedGridLines.map.add(currentNamedGridLine, Vector<String>());
        for (auto& name : names) {
            auto result = namedGridLines.map.add(name, Vector<unsigned> { });
            result.iterator->value.append(currentNamedGridLine);
            orderedResult.iterator->value.append(name);
        }
    };

    unsigned currentNamedGridLine = 0;
    unsigned autoRepeatIndex = 0;

    for (const auto& entry : list) {
        WTF::switchOn(entry,
            [&](const GridTrackSize& size) {
                ++currentNamedGridLine;
                sizes.append(size);
            },
            [&](const Vector<String>& names) {
                createGridLineNamesList(names, currentNamedGridLine, namedLines, orderedNamedLines);
                // Subgrids only have line names defined, not track sizes, so we want our count
                // to be the number of lines named rather than number of sized tracks.
                if (subgrid)
                    ++currentNamedGridLine;
            },
            [&](const GridTrackEntryRepeat& repeat) {
                for (size_t i = 0; i < repeat.repeats; ++i) {
                    for (auto& repeatEntry : repeat.list) {
                        if (std::holds_alternative<Vector<String>>(repeatEntry)) {
                            createGridLineNamesList(std::get<Vector<String>>(repeatEntry), currentNamedGridLine, namedLines, orderedNamedLines);
                            // Subgrids only have line names defined, not track sizes, so we want our count
                            // to be the number of lines named rather than number of sized tracks.
                            if (subgrid)
                                ++currentNamedGridLine;
                        } else {
                            ++currentNamedGridLine;
                            sizes.append(std::get<GridTrackSize>(repeatEntry));
                        }
                    }
                }
            },
            [&](const GridTrackEntryAutoRepeat& repeat) {
                ASSERT(!autoRepeatIndex);
                autoRepeatIndex = 0;
                autoRepeatType = repeat.type;
                for (auto& autoRepeatEntry : repeat.list) {
                    if (std::holds_alternative<Vector<String>>(autoRepeatEntry)) {
                        createGridLineNamesList(std::get<Vector<String>>(autoRepeatEntry), autoRepeatIndex, autoRepeatNamedLines, autoRepeatOrderedNamedLines);
                        if (subgrid)
                            ++autoRepeatIndex;
                        continue;
                    }
                    ++autoRepeatIndex;
                    autoRepeatSizes.append(std::get<GridTrackSize>(autoRepeatEntry));
                }
                autoRepeatInsertionPoint = currentNamedGridLine;
                if (!subgrid)
                    ++currentNamedGridLine;
            },
            [&](const GridTrackEntrySubgrid&) {
                subgrid = true;
            },
            [&](const GridTrackEntryMasonry&) {
                masonry = true;
            }
        );
    }
    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!sizes.isEmpty() || !autoRepeatSizes.isEmpty() || subgrid || masonry);
}

// MARK: - Conversion

auto CSSValueConversion<GridTemplateList>::operator()(BuilderState& state, const CSSValue& value) -> GridTemplateList
{
    RefPtr<const CSSValueContainingVector> valueList;
    GridTrackList trackList;

    RefPtr subgridValue = dynamicDowncast<CSSSubgridValue>(value);

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueMasonry) {
            trackList.append(GridTrackEntryMasonry());
            return { WTFMove(trackList) };
        }
        if (primitiveValue->valueID() == CSSValueNone)
            return CSS::Keyword::None { };
    } else if (subgridValue) {
        valueList = subgridValue;
        trackList.append(GridTrackEntrySubgrid());
    } else if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        valueList = list;
    } else {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    // https://drafts.csswg.org/css-grid-2/#computed-tracks
    // The computed track list of a non-subgrid axis is a list alternating between line name sets
    // and track sections, with the first and last items being line name sets.
    auto ensureLineNames = [&](auto& list) {
        if (subgridValue)
            return;
        if (list.isEmpty() || !std::holds_alternative<Vector<String>>(list.last()))
            list.append(Vector<String>());
    };

    auto buildRepeatList = [&](const CSSValue& repeatValue, RepeatTrackList& repeatList) {
        RefPtr vectorValue = requiredDowncast<CSSValueContainingVector>(state, repeatValue);
        if (!vectorValue)
            return;

        for (Ref currentValue : *vectorValue) {
            if (RefPtr namesValue = dynamicDowncast<CSSGridLineNamesValue>(currentValue))
                repeatList.append(Vector<String>(namesValue->names()));
            else {
                ensureLineNames(repeatList);
                repeatList.append(toStyleFromCSSValue<GridTrackSize>(state, currentValue));
            }
        }

        if (!repeatList.isEmpty())
            ensureLineNames(repeatList);
    };

    auto addOne = [&](const CSSValue& currentValue) {
        if (RefPtr namesValue = dynamicDowncast<CSSGridLineNamesValue>(currentValue)) {
            trackList.append(Vector<String>(namesValue->names()));
            return;
        }

        ensureLineNames(trackList);

        if (RefPtr repeatValue = dynamicDowncast<CSSGridAutoRepeatValue>(currentValue)) {
            CSSValueID autoRepeatID = repeatValue->autoRepeatID();
            ASSERT(autoRepeatID == CSSValueAutoFill || autoRepeatID == CSSValueAutoFit);

            GridTrackEntryAutoRepeat repeat;
            repeat.type = autoRepeatID == CSSValueAutoFill ? AutoRepeatType::Fill : AutoRepeatType::Fit;

            buildRepeatList(currentValue, repeat.list);
            trackList.append(WTFMove(repeat));
        } else if (RefPtr repeatValue = dynamicDowncast<CSSGridIntegerRepeatValue>(currentValue)) {
            auto repetitions = clampTo(repeatValue->repetitions().resolveAsInteger(state.cssToLengthConversionData()), 1, GridPosition::max());

            GridTrackEntryRepeat repeat;
            repeat.repeats = repetitions;

            buildRepeatList(currentValue, repeat.list);
            trackList.append(WTFMove(repeat));
        } else
            trackList.append(toStyleFromCSSValue<GridTrackSize>(state, currentValue));
    };

    if (!valueList)
        addOne(value);
    else {
        for (Ref value : *valueList)
            addOne(value);
    }

    if (!trackList.isEmpty())
        ensureLineNames(trackList);

    return { WTFMove(trackList) };
}

// MARK: - Blending

static RepeatTrackList blendRepeatList(const RepeatTrackList& from, const RepeatTrackList& to, const BlendingContext& context)
{
    RepeatTrackList result;
    size_t i = 0;

    auto visitor = WTF::makeVisitor(
        [&](const GridTrackSize& size) {
            result.append(Style::blend(size, std::get<GridTrackSize>(to[i]), context));
        },
        [&](const Vector<String>& names) {
            if (context.progress < 0.5)
                result.append(names);
            else
                result.append(std::get<Vector<String>>(to[i]));
        }
    );

    for (i = 0; i < from.size(); ++i)
        WTF::visit(visitor, from[i]);

    return result;
}

auto Blending<GridTemplateList>::canBlend(const GridTemplateList& from, const GridTemplateList& to) -> bool
{
    if (from.list.size() != to.list.size())
        return false;

    size_t i = 0;
    auto visitor = WTF::makeVisitor(
        [&](const GridTrackSize&) {
            return std::holds_alternative<GridTrackSize>(to.list[i]);
        },
        [&](const Vector<String>&) {
            return std::holds_alternative<Vector<String>>(to.list[i]);
        },
        [&](const GridTrackEntryRepeat& repeat) {
            if (!std::holds_alternative<GridTrackEntryRepeat>(to.list[i]))
                return false;
            auto& toEntry = std::get<GridTrackEntryRepeat>(to.list[i]);
            return repeat.repeats == toEntry.repeats && repeat.list.size() == toEntry.list.size();
        },
        [](const GridTrackEntryAutoRepeat&) {
            return false;
        },
        [](const GridTrackEntrySubgrid&) {
            return false;
        },
        [](const GridTrackEntryMasonry&) {
            return false;
        }
    );

    for (i = 0; i < from.list.size(); ++i) {
        if (!WTF::visit(visitor, from.list[i]))
            return false;
    }

    return true;
}

auto Blending<GridTemplateList>::blend(const GridTemplateList& from, const GridTemplateList& to, const BlendingContext& context) -> GridTemplateList
{
    if (!canBlend(from, to))
        return context.progress < 0.5 ? from : to;

    GridTrackList result;
    size_t i = 0;

    auto visitor = WTF::makeVisitor(
        [&](const GridTrackSize& size) {
            result.append(Style::blend(size, std::get<GridTrackSize>(to.list[i]), context));
        },
        [&](const Vector<String>& names) {
            if (context.progress < 0.5)
                result.append(names);
            else
                result.append(std::get<Vector<String>>(to.list[i]));
        },
        [&](const GridTrackEntryRepeat& repeatFrom) {
            auto& repeatTo = std::get<GridTrackEntryRepeat>(to.list[i]);
            GridTrackEntryRepeat repeatResult;
            repeatResult.repeats = repeatFrom.repeats;
            repeatResult.list = blendRepeatList(repeatFrom.list, repeatTo.list, context);
            result.append(WTFMove(repeatResult));
        },
        [&](const GridTrackEntryAutoRepeat& repeatFrom) {
            auto& repeatTo = std::get<GridTrackEntryAutoRepeat>(to.list[i]);
            GridTrackEntryAutoRepeat repeatResult;
            repeatResult.type = repeatFrom.type;
            repeatResult.list = blendRepeatList(repeatFrom.list, repeatTo.list, context);
            result.append(WTFMove(repeatResult));
        },
        [](const GridTrackEntrySubgrid&) {
        },
        [](const GridTrackEntryMasonry&) {
        }
    );

    for (i = 0; i < from.list.size(); ++i)
        WTF::visit(visitor, from.list[i]);

    return GridTemplateList { WTFMove(result) };
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const GridTemplateList& list)
{
    return ts << list.list;
}

TextStream& operator<<(TextStream& ts, const RepeatEntry& entry)
{
    WTF::switchOn(entry,
        [&](const GridTrackSize& size) {
            ts << size;
        },
        [&](const Vector<String>& names) {
            ts << names;
        }
    );
    return ts;
}

TextStream& operator<<(TextStream& ts, const GridTrackEntry& entry)
{
    WTF::switchOn(entry,
        [&](const GridTrackSize& size) {
            ts << size;
        },
        [&](const Vector<String>& names) {
            ts << names;
        },
        [&](const GridTrackEntryRepeat& repeat) {
            ts << "repeat("_s << repeat.repeats << ", "_s << repeat.list << ')';
        },
        [&](const GridTrackEntryAutoRepeat& repeat) {
            ts << "repeat("_s << repeat.type << ", "_s << repeat.list << ')';
        },
        [&](const GridTrackEntrySubgrid&) {
            ts << "subgrid"_s;
        },
        [&](const GridTrackEntryMasonry&) {
            ts << "masonry"_s;
        }
    );
    return ts;
}

} // namespace Style
} // namespace WebCore
