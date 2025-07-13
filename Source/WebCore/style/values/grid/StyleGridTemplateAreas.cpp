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
#include "StyleGridTemplateAreas.h"

#include "CSSGridTemplateAreasValue.h"
#include "GridPositionsResolver.h"
#include "StyleBuilderChecking.h"
#include <algorithm>

namespace WebCore {
namespace Style {

static GridNamedLinesMap initializeImplicitNamedGridLines(const GridNamedAreaMap& namedGridAreas, GridTrackSizingDirection direction)
{
    GridNamedLinesMap namedGridLines;

    for (auto& area : namedGridAreas.map) {
        auto areaSpan = direction == GridTrackSizingDirection::ForRows ? area.value.rows : area.value.columns;
        {
            auto& startVector = namedGridLines.map.add(makeString(area.key, "-start"_s), Vector<unsigned>()).iterator->value;
            startVector.append(areaSpan.startLine());
            std::ranges::sort(startVector);
        }
        {
            auto& endVector = namedGridLines.map.add(makeString(area.key, "-end"_s), Vector<unsigned>()).iterator->value;
            endVector.append(areaSpan.endLine());
            std::ranges::sort(endVector);
        }
    }

    // FIXME: For acceptable performance, should sort once at the end, not as we add each item, or at least insert in sorted order instead of using std::sort each time.

    return namedGridLines;
}

GridTemplateAreas::GridTemplateAreas(GridNamedAreaMap&& map)
    : map { WTFMove(map) }
    , implicitNamedGridColumnLines { initializeImplicitNamedGridLines(map, GridTrackSizingDirection::ForColumns) }
    , implicitNamedGridRowLines { initializeImplicitNamedGridLines(map, GridTrackSizingDirection::ForRows) }
{
}

GridTemplateAreas::GridTemplateAreas(const GridNamedAreaMap& map)
    : map { map }
    , implicitNamedGridColumnLines { initializeImplicitNamedGridLines(map, GridTrackSizingDirection::ForColumns) }
    , implicitNamedGridRowLines { initializeImplicitNamedGridLines(map, GridTrackSizingDirection::ForRows) }
{
}

// MARK: - Conversion

auto CSSValueConversion<GridTemplateAreas>::operator()(BuilderState& state, const CSSValue& value) -> GridTemplateAreas
{
    if (isValueID(value, CSSValueNone))
        return CSS::Keyword::None { };

    RefPtr gridTemplateAreasValue = requiredDowncast<CSSGridTemplateAreasValue>(state, value);
    if (!gridTemplateAreasValue)
        return CSS::Keyword::None { };

    return GridTemplateAreas { gridTemplateAreasValue->areas().map };
}

auto CSSValueCreation<GridTemplateAreas>::operator()(CSSValuePool& pool, const RenderStyle& style, const GridTemplateAreas& value) -> Ref<CSSValue>
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::None& keyword) -> Ref<CSSValue> {
            return createCSSValue(pool, style, keyword);
        },
        [&](const GridNamedAreaMap& map) -> Ref<CSSValue> {
            return CSSGridTemplateAreasValue::create(CSS::GridTemplateAreas { map });
        }
    );
}

// MARK: - Serialization

void Serialize<GridTemplateAreas>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const GridTemplateAreas& value)
{
    WTF::switchOn(value,
        [&](const CSS::Keyword::None& keyword) {
            serializationForCSS(builder, context, style, keyword);
        },
        [&](const GridNamedAreaMap& map) {
            serializationForCSS(builder, context, style, map);
        }
    );
}

} // namespace Style
} // namespace WebCore
