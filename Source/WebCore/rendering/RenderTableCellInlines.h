/**
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleContentAlignmentData.h"

namespace WebCore {

inline const BorderValue& RenderTableCell::borderAdjoiningCellAfter(const RenderTableCell& cell)
{
    ASSERT_UNUSED(cell, table()->cellBefore(&cell) == this);
    return style().borderEnd(tableWritingMode());
}

inline const BorderValue& RenderTableCell::borderAdjoiningCellBefore(const RenderTableCell& cell)
{
    ASSERT_UNUSED(cell, table()->cellAfter(&cell) == this);
    return style().borderStart(tableWritingMode());
}

inline const BorderValue& RenderTableCell::borderAdjoiningTableEnd() const
{
    ASSERT(isFirstOrLastCellInRow());
    return style().borderEnd(tableWritingMode());
}

inline const BorderValue& RenderTableCell::borderAdjoiningTableStart() const
{
    ASSERT(isFirstOrLastCellInRow());
    return style().borderStart(tableWritingMode());
}

inline std::pair<Style::PreferredSize, Style::ZoomFactor> RenderTableCell::styleOrColLogicalWidth() const
{
    auto& style = this->style();
    auto& styleWidth = style.logicalWidth();
    if (!styleWidth.isAuto())
        return { styleWidth, style.usedZoomForLength() };
    if (RenderTableCol* firstColumn = table()->colElement(col())) {
        // logicalWidthFromColumns will return a zoomed size so we return a zoom
        // factor of 1.0 to avoid double zooming.
        return { logicalWidthFromColumns(firstColumn, styleWidth), Style::ZoomFactor { 1.0f } };
    }
    return { styleWidth, style.usedZoomForLength() };
}

// The CSS 2 'vertical-align' value a table cell acts on. With 'vertical-align' as the CSS Inline 3 shorthand,
// 'baseline-shift: top' behaves like 'top', 'bottom' like 'bottom', and otherwise 'alignment-baseline: middle'
// behaves like 'middle', and the non-standard '-webkit-baseline-middle' like the CSS 2 value of the same name;
// everything else is baseline-aligned.
// https://drafts.csswg.org/css-inline-3/#transverse-alignment
inline Style::VerticalAlign verticalAlignForTableCell(const Style::ComputedStyle& style, const Settings& settings)
{
    if (settings.cssVerticalAlignShorthandEnabled()) [[unlikely]] {
        return style.baselineShift().switchOn(
            [](const CSS::Keyword::Top& keyword) -> Style::VerticalAlign { return keyword; },
            [](const CSS::Keyword::Bottom& keyword) -> Style::VerticalAlign { return keyword; },
            [&](const auto&) -> Style::VerticalAlign {
                if (style.alignmentBaseline() == AlignmentBaseline::Middle)
                    return CSS::Keyword::Middle { };
                if (style.alignmentBaseline() == AlignmentBaseline::WebkitBaselineMiddle)
                    return CSS::Keyword::WebkitBaselineMiddle { };
                return CSS::Keyword::Baseline { };
            }
        );
    }
    return Style::effectiveVerticalAlign(style, settings);
}

inline bool RenderTableCell::isBaselineAligned() const
{
    if (auto alignContent = style().alignContent(); !alignContent.isNormal())
        return alignContent.isFirstBaseline();

    auto verticalAlign = verticalAlignForTableCell(style(), document().settings());
    return WTF::holdsAlternative<CSS::Keyword::Baseline>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::TextBottom>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::TextTop>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::Super>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::Sub>(verticalAlign)
        || WTF::holdsAlternative<Style::VerticalAlign::LengthPercentage>(verticalAlign);
}

inline bool RenderTableCell::isOrthogonal() const
{
    if (auto* row = this->row())
        return writingMode().isOrthogonal(row->writingMode());

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
