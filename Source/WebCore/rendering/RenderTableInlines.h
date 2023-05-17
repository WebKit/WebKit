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
#include "RenderTable.h"

namespace WebCore {

inline LayoutUnit RenderTable::borderBottom() const
{
    if (style().isHorizontalWritingMode())
        return style().isFlippedBlocksWritingMode() ? borderBefore() : borderAfter();
    return style().isLeftToRightDirection() ? borderEnd() : borderStart();
}

inline LayoutUnit RenderTable::borderLeft() const
{
    if (style().isHorizontalWritingMode())
        return style().isLeftToRightDirection() ? borderStart() : borderEnd();
    return style().isFlippedBlocksWritingMode() ? borderAfter() : borderBefore();
}

inline LayoutUnit RenderTable::borderRight() const
{
    if (style().isHorizontalWritingMode())
        return style().isLeftToRightDirection() ? borderEnd() : borderStart();
    return style().isFlippedBlocksWritingMode() ? borderBefore() : borderAfter();
}

inline LayoutUnit RenderTable::borderTop() const
{
    if (style().isHorizontalWritingMode())
        return style().isFlippedBlocksWritingMode() ? borderAfter() : borderBefore();
    return style().isLeftToRightDirection() ? borderStart() : borderEnd();
}

inline LayoutUnit RenderTable::bordersPaddingAndSpacingInRowDirection() const
{
    // 'border-spacing' only applies to separate borders (see 17.6.1 The separated borders model).
    return borderStart() + borderEnd() + (collapseBorders() ? 0_lu : (paddingStart() + paddingEnd() + borderSpacingInRowDirection()));
}

inline LayoutUnit RenderTable::outerBorderBottom() const
{
    if (style().isHorizontalWritingMode())
        return style().isFlippedBlocksWritingMode() ? outerBorderBefore() : outerBorderAfter();
    return style().isLeftToRightDirection() ? outerBorderEnd() : outerBorderStart();
}

inline LayoutUnit RenderTable::outerBorderLeft() const
{
    if (style().isHorizontalWritingMode())
        return style().isLeftToRightDirection() ? outerBorderStart() : outerBorderEnd();
    return style().isFlippedBlocksWritingMode() ? outerBorderAfter() : outerBorderBefore();
}

inline LayoutUnit RenderTable::outerBorderRight() const
{
    if (style().isHorizontalWritingMode())
        return style().isLeftToRightDirection() ? outerBorderEnd() : outerBorderStart();
    return style().isFlippedBlocksWritingMode() ? outerBorderBefore() : outerBorderAfter();
}

inline LayoutUnit RenderTable::outerBorderTop() const
{
    if (style().isHorizontalWritingMode())
        return style().isFlippedBlocksWritingMode() ? outerBorderAfter() : outerBorderBefore();
    return style().isLeftToRightDirection() ? outerBorderStart() : outerBorderEnd();
}

} // namespace WebCore
