/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "RenderStyleInlines.h"
#include "RenderText.h"
#include "WordTrailingSpace.h"

namespace WebCore {

inline LayoutUnit RenderText::marginLeft() const { return minimumValueForLength(style().marginLeft(), 0); }
inline LayoutUnit RenderText::marginRight() const { return minimumValueForLength(style().marginRight(), 0); }

template <typename MeasureTextCallback>
float RenderText::measureTextConsideringPossibleTrailingSpace(bool currentCharacterIsSpace, unsigned startIndex, unsigned wordLength, WordTrailingSpace& wordTrailingSpace, SingleThreadWeakHashSet<const Font>& fallbackFonts, MeasureTextCallback&& callback)
{
    std::optional<float> wordTrailingSpaceWidth;
    if (currentCharacterIsSpace)
        wordTrailingSpaceWidth = wordTrailingSpace.width(fallbackFonts);
    if (wordTrailingSpaceWidth)
        return callback(startIndex, wordLength + 1) - wordTrailingSpaceWidth.value();
    return callback(startIndex, wordLength);
}

} // namespace WebCore
