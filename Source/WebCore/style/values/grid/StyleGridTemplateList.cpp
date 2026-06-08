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

#include "CSSGridTemplateListValue.h"
#include "CSSKeywordValue.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

GridTemplateList::GridTemplateList(GridTrackList&& entries)
    : list { WTF::move(entries) }
{
    if (list.isEmpty())
        return;

    auto createGridLineNamesList = [](const auto& names, auto currentNamedGridLine, auto& namedGridLines, auto& orderedNamedGridLines) {
        auto orderedResult = orderedNamedGridLines.map.add(currentNamedGridLine, GridLineNames { });
        for (auto& name : names) {
            auto result = namedGridLines.map.add(name, Vector<unsigned> { });
            result.iterator->value.append(currentNamedGridLine);
            orderedResult.iterator->value.value.value.append(name);
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
            [&](const GridLineNames& names) {
                createGridLineNamesList(names, currentNamedGridLine, namedLines, orderedNamedLines);
                // Subgrids only have line names defined, not track sizes, so we want our count
                // to be the number of lines named rather than number of sized tracks.
                if (subgrid)
                    ++currentNamedGridLine;
            },
            [&](const GridTrackEntryRepeat& repeat) {
                for (size_t i = 0; i < repeat.repeats; ++i) {
                    for (auto& repeatEntry : repeat.list) {
                        if (std::holds_alternative<GridLineNames>(repeatEntry)) {
                            createGridLineNamesList(std::get<GridLineNames>(repeatEntry), currentNamedGridLine, namedLines, orderedNamedLines);
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
                    if (std::holds_alternative<GridLineNames>(autoRepeatEntry)) {
                        createGridLineNamesList(std::get<GridLineNames>(autoRepeatEntry), autoRepeatIndex, autoRepeatNamedLines, autoRepeatOrderedNamedLines);
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
            }
        );
    }
    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!sizes.isEmpty() || !autoRepeatSizes.isEmpty() || subgrid);
}

// MARK: - Conversion

auto ToStyle<CSS::GridTemplateList>::operator()(const CSS::GridTemplateList& value, const BuilderState& state) -> GridTemplateList
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::None&) -> GridTemplateList {
            return CSS::Keyword::None { };
        },
        [&](const CSS::GridTrackList& trackList) -> GridTemplateList {
            GridTrackList result;

            // https://drafts.csswg.org/css-grid-2/#computed-tracks
            // The computed track list of a non-subgrid axis is a list alternating between line name sets
            // and track sections, with the first and last items being line name sets.
            auto ensureLineNames = [&](auto& list) {
                if (list.isEmpty() || !std::holds_alternative<GridLineNames>(list.last()))
                    list.append(GridLineNames { });
            };

            for (auto& entry : trackList.value) {
                WTF::switchOn(entry,
                    [&](const CSS::GridLineNames& lineNames) {
                        result.append(toStyle(lineNames, state));
                    },
                    [&](const CSS::GridTrackSize& trackSize) {
                        ensureLineNames(result);
                        result.append(toStyle(trackSize, state));
                    },
                    [&](const CSS::GridTrackRepeatFunction& trackRepeatFunction) {
                        ensureLineNames(result);

                        auto buildRepeatList = [&](auto& repeated) {
                            RepeatTrackList repeatList;

                            for (auto& value : repeated) {
                                WTF::switchOn(value,
                                    [&](const CSS::GridLineNames& lineNames) {
                                        repeatList.append(toStyle(lineNames, state));
                                    },
                                    [&](const CSS::GridTrackSize& trackSize) {
                                        ensureLineNames(repeatList);
                                        repeatList.append(toStyle(trackSize, state));
                                    }
                                );
                            }
                            if (!repeatList.isEmpty())
                                ensureLineNames(repeatList);

                            return repeatList;
                        };

                        WTF::switchOn(trackRepeatFunction->repetitions,
                            [&](const CSS::Integer<CSS::Positive, unsigned>& value) {
                                result.append(GridTrackEntryRepeat {
                                    .repeats = clampTo<unsigned>(toStyle(value, state).value, 1u, GridPosition::max()),
                                    .list = buildRepeatList(trackRepeatFunction->repeated),
                                });
                            },
                            [&](const CSS::Keyword::AutoFill&) {
                                result.append(GridTrackEntryAutoRepeat {
                                    .type = AutoRepeatType::Fill,
                                    .list = buildRepeatList(trackRepeatFunction->repeated),
                                });
                            },
                            [&](const CSS::Keyword::AutoFit&) {
                                result.append(GridTrackEntryAutoRepeat {
                                    .type = AutoRepeatType::Fit,
                                    .list = buildRepeatList(trackRepeatFunction->repeated),
                                });
                            }
                        );
                    }
                );
            }
            if (!result.isEmpty())
                ensureLineNames(result);

            return result;
        },
        [&](const CSS::GridSubgrid& subgrid) -> GridTemplateList {
            GridTrackList result;
            result.append(GridTrackEntrySubgrid());

            auto buildRepeatList = [&](auto& repeated) {
                RepeatTrackList repeatList;
                for (auto& lineNames : repeated)
                    repeatList.append(toStyle(lineNames, state));
                return repeatList;
            };

            for (auto& entry : subgrid.value) {
                WTF::switchOn(entry,
                    [&](const CSS::GridLineNames& lineNames) {
                        result.append(toStyle(lineNames, state));
                    },
                    [&](const CSS::GridNameRepeatFunction& nameRepeatFunction) {
                        WTF::switchOn(nameRepeatFunction->repetitions,
                            [&](const CSS::Integer<CSS::Positive, unsigned>& value) {
                                result.append(GridTrackEntryRepeat {
                                    .repeats = clampTo<unsigned>(toStyle(value, state).value, 1u, GridPosition::max()),
                                    .list = buildRepeatList(nameRepeatFunction->repeated),
                                });
                            },
                            [&](const CSS::Keyword::AutoFill&) {
                                result.append(GridTrackEntryAutoRepeat {
                                    .type = AutoRepeatType::Fill,
                                    .list = buildRepeatList(nameRepeatFunction->repeated),
                                });
                            }
                        );
                    }
                );
            }

            return result;
        }
    );
}

auto CSSValueConversion<GridTemplateList>::operator()(BuilderState& state, const CSSValue& value) -> GridTemplateList
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        return GridTrackList {
            GridTrackEntry { GridLineNames { } },
            GridTrackEntry { GridTrackSize { toStyleFromCSSValue<GridTrackSize::Breadth>(state, *primitiveValue) } },
            GridTrackEntry { GridLineNames { } },
        };
    }

    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        if (keywordValue->valueID() == CSSValueNone)
            return CSS::Keyword::None { };

        return GridTrackList {
            GridTrackEntry { GridLineNames { } },
            GridTrackEntry { GridTrackSize { toStyleFromCSSValue<GridTrackSize::Breadth>(state, *keywordValue) } },
            GridTrackEntry { GridLineNames { } },
        };
    }

    RefPtr gridTemplateListValue = requiredDowncast<CSSGridTemplateListValue>(state, value);
    if (!gridTemplateListValue)
        return CSS::Keyword::None { };

    return toStyle(gridTemplateListValue->list(), state);
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
        [&](const GridLineNames& names) {
            if (context.progress < 0.5)
                result.append(names);
            else
                result.append(std::get<GridLineNames>(to[i]));
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
        [&](const GridLineNames&) {
            return std::holds_alternative<GridLineNames>(to.list[i]);
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
        [&](const GridLineNames& names) {
            if (context.progress < 0.5)
                result.append(names);
            else
                result.append(std::get<GridLineNames>(to.list[i]));
        },
        [&](const GridTrackEntryRepeat& repeatFrom) {
            auto& repeatTo = std::get<GridTrackEntryRepeat>(to.list[i]);
            GridTrackEntryRepeat repeatResult;
            repeatResult.repeats = repeatFrom.repeats;
            repeatResult.list = blendRepeatList(repeatFrom.list, repeatTo.list, context);
            result.append(WTF::move(repeatResult));
        },
        [&](const GridTrackEntryAutoRepeat& repeatFrom) {
            auto& repeatTo = std::get<GridTrackEntryAutoRepeat>(to.list[i]);
            GridTrackEntryAutoRepeat repeatResult;
            repeatResult.type = repeatFrom.type;
            repeatResult.list = blendRepeatList(repeatFrom.list, repeatTo.list, context);
            result.append(WTF::move(repeatResult));
        },
        [](const GridTrackEntrySubgrid&) {
        }
    );

    for (i = 0; i < from.list.size(); ++i)
        WTF::visit(visitor, from.list[i]);

    return GridTemplateList { WTF::move(result) };
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
        [&](const GridLineNames& names) {
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
        [&](const GridLineNames& names) {
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
        }
    );
    return ts;
}

} // namespace Style
} // namespace WebCore
