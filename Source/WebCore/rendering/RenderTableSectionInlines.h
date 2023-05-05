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
#include "RenderTableSection.h"

namespace WebCore {

inline const BorderValue& RenderTableSection::borderAdjoiningTableEnd() const { return isDirectionSame(this, table()) ? style().borderEnd() : style().borderStart(); }
inline const BorderValue& RenderTableSection::borderAdjoiningTableStart() const { return isDirectionSame(this, table()) ? style().borderStart() : style().borderEnd(); }

inline LayoutUnit RenderTableSection::outerBorderBottom(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode())
        return styleForCellFlow->isFlippedBlocksWritingMode() ? outerBorderBefore() : outerBorderAfter();
    return styleForCellFlow->isLeftToRightDirection() ? outerBorderEnd() : outerBorderStart();
}

inline LayoutUnit RenderTableSection::outerBorderLeft(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode())
        return styleForCellFlow->isLeftToRightDirection() ? outerBorderStart() : outerBorderEnd();
    return styleForCellFlow->isFlippedBlocksWritingMode() ? outerBorderAfter() : outerBorderBefore();
}

inline LayoutUnit RenderTableSection::outerBorderRight(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode())
        return styleForCellFlow->isLeftToRightDirection() ? outerBorderEnd() : outerBorderStart();
    return styleForCellFlow->isFlippedBlocksWritingMode() ? outerBorderBefore() : outerBorderAfter();
}

inline LayoutUnit RenderTableSection::outerBorderTop(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode())
        return styleForCellFlow->isFlippedBlocksWritingMode() ? outerBorderAfter() : outerBorderBefore();
    return styleForCellFlow->isLeftToRightDirection() ? outerBorderStart() : outerBorderEnd();
}

} // namespace WebCore
