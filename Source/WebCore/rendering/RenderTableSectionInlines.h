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

inline const BorderValue& RenderTableSection::borderAdjoiningTableEnd() const { return style().borderEnd(table()->style()); }
inline const BorderValue& RenderTableSection::borderAdjoiningTableStart() const { return style().borderStart(table()->style()); }

inline LayoutUnit RenderTableSection::outerBorderBottom(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isBlockTopToBottom() ? outerBorderAfter() : outerBorderBefore();
    return writingMode.isInlineTopToBottom() ? outerBorderEnd() : outerBorderStart();
}

inline LayoutUnit RenderTableSection::outerBorderLeft(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? outerBorderStart() : outerBorderEnd();
    return writingMode.isBlockLeftToRight() ? outerBorderBefore() : outerBorderAfter();
}

inline LayoutUnit RenderTableSection::outerBorderRight(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? outerBorderEnd() : outerBorderStart();
    return writingMode.isBlockLeftToRight() ? outerBorderAfter() : outerBorderBefore();
}

inline LayoutUnit RenderTableSection::outerBorderTop(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isBlockTopToBottom() ? outerBorderBefore() : outerBorderAfter();
    return writingMode.isInlineTopToBottom() ? outerBorderStart() : outerBorderEnd();
}

} // namespace WebCore
