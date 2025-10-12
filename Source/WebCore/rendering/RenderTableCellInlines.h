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

#include "RenderStyleInlines.h"
#include "RenderTableCell.h"
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

inline Style::PreferredSize RenderTableCell::styleOrColLogicalWidth() const
{
    auto& styleWidth = style().logicalWidth();
    if (!styleWidth.isAuto())
        return styleWidth;
    if (RenderTableCol* firstColumn = table()->colElement(col()))
        return logicalWidthFromColumns(firstColumn, styleWidth);
    return styleWidth;
}

inline bool RenderTableCell::isBaselineAligned() const
{
    if (auto alignContent = style().alignContent(); !alignContent.isNormal())
        return alignContent.position() == ContentPosition::Baseline;

    auto& verticalAlign = style().verticalAlign();
    return WTF::holdsAlternative<CSS::Keyword::Baseline>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::TextBottom>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::TextTop>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::Super>(verticalAlign)
        || WTF::holdsAlternative<CSS::Keyword::Sub>(verticalAlign)
        || WTF::holdsAlternative<Style::VerticalAlign::Length>(verticalAlign);
}

inline bool RenderTableCell::isOrthogonal() const
{
    if (auto* row = this->row())
        return writingMode().isOrthogonal(row->writingMode());

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
