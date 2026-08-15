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

inline bool RenderTableCell::isBaselineAligned() const
{
    if (auto alignContent = style().alignContent(); !alignContent.isNormal())
        return alignContent.isFirstBaseline();

    // `vertical-align` is stored decomposed into the box alignment-baseline and
    // baseline-shift longhands. A cell participates in baseline alignment for
    // baseline / text-top / text-bottom / super / sub / <length-percentage>, but
    // not for top / bottom / middle / -webkit-baseline-middle.
    auto& baselineShift = style().baselineShift();
    if (baselineShift.isTop() || baselineShift.isBottom())
        return false;
    if (baselineShift.isSub() || baselineShift.isSuper())
        return true;
    if (baselineShift.isLengthPercentage())
        return true;
    auto alignmentBaseline = style().alignmentBaseline();
    return alignmentBaseline == AlignmentBaseline::Baseline
        || alignmentBaseline == AlignmentBaseline::TextBeforeEdge
        || alignmentBaseline == AlignmentBaseline::TextAfterEdge;
}

inline bool RenderTableCell::isOrthogonal() const
{
    if (auto* row = this->row())
        return writingMode().isOrthogonal(row->writingMode());

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
