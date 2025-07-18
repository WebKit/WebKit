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

#include "CSSGridTemplateAreas.h"
#include "StyleGridNamedAreaMap.h"
#include "StyleGridNamedLinesMap.h"
#include "StyleGridTrackSizingDirection.h"

namespace WebCore {
namespace Style {

// <'grid-template-areas'> = none | <string>+
// https://drafts.csswg.org/css-grid/#propdef-grid-template-areas
struct GridTemplateAreas {
    GridNamedAreaMap map { };
    GridNamedLinesMap implicitNamedGridColumnLines { };
    GridNamedLinesMap implicitNamedGridRowLines { };

    GridTemplateAreas(CSS::Keyword::None) { }
    GridTemplateAreas(GridNamedAreaMap&&);
    GridTemplateAreas(const GridNamedAreaMap&);

    bool isNone() const { return !map.rowCount; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(map);
    }

    size_t count(GridTrackSizingDirection direction) const
    {
        return direction == GridTrackSizingDirection::Columns ? map.columnCount : map.rowCount;
    }

    const GridNamedLinesMap& implicitNamedGridLines(GridTrackSizingDirection direction) const
    {
        return direction == GridTrackSizingDirection::Columns ? implicitNamedGridColumnLines : implicitNamedGridRowLines;
    }

    bool operator==(const GridTemplateAreas& other) const
    {
        // It is only necessary to compare the `map` member, as the `implicit*` members
        // are purely cached derived values based on the value of `map`.
        return map == other.map;
    }
};

// MARK: - Conversion

template<> struct CSSValueConversion<GridTemplateAreas> { auto operator()(BuilderState&, const CSSValue&) -> GridTemplateAreas; };
template<> struct CSSValueCreation<GridTemplateAreas> { auto operator()(CSSValuePool&, const RenderStyle&, const GridTemplateAreas&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<GridTemplateAreas> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const GridTemplateAreas&); };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridTemplateAreas);
