/**
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include "RenderStyleInlines.h"
#include "RenderTableCell.h"

namespace WebCore {

inline const BorderValue& RenderTableCell::borderAdjoiningCellAfter(const RenderTableCell& cell)
{
    ASSERT_UNUSED(cell, table()->cellBefore(&cell) == this);
    return style().borderEnd(table()->style());
}

inline const BorderValue& RenderTableCell::borderAdjoiningCellBefore(const RenderTableCell& cell)
{
    ASSERT_UNUSED(cell, table()->cellAfter(&cell) == this);
    return style().borderStart(table()->style());
}

inline const BorderValue& RenderTableCell::borderAdjoiningTableEnd() const
{
    ASSERT(isFirstOrLastCellInRow());
    return style().borderEnd(table()->style());
}

inline const BorderValue& RenderTableCell::borderAdjoiningTableStart() const
{
    ASSERT(isFirstOrLastCellInRow());
    return style().borderStart(table()->style());
}

inline LayoutUnit RenderTableCell::logicalHeightForRowSizing() const
{
    // FIXME: This function does too much work, and is very hot during table layout!
    LayoutUnit adjustedLogicalHeight = logicalHeight() - (intrinsicPaddingBefore() + intrinsicPaddingAfter());
    if (!style().logicalHeight().isSpecified())
        return adjustedLogicalHeight;
    LayoutUnit styleLogicalHeight = valueForLength(style().logicalHeight(), 0);
    // In strict mode, box-sizing: content-box do the right thing and actually add in the border and padding.
    // Call computedCSSPadding* directly to avoid including implicitPadding.
    if (!document().inQuirksMode() && style().boxSizing() != BoxSizing::BorderBox)
        styleLogicalHeight += computedCSSPaddingBefore() + computedCSSPaddingAfter() + borderBefore() + borderAfter();
    return std::max(styleLogicalHeight, adjustedLogicalHeight);
}

inline Length RenderTableCell::styleOrColLogicalWidth() const
{
    Length styleWidth = style().logicalWidth();
    if (!styleWidth.isAuto())
        return styleWidth;
    if (RenderTableCol* firstColumn = table()->colElement(col()))
        return logicalWidthFromColumns(firstColumn, styleWidth);
    return styleWidth;
}

} // namespace WebCore
