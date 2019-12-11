/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleMultiColData.h"

#include "RenderStyle.h"

namespace WebCore {

StyleMultiColData::StyleMultiColData()
    : count(RenderStyle::initialColumnCount())
    , autoWidth(true)
    , autoCount(true)
    , fill(static_cast<unsigned>(RenderStyle::initialColumnFill()))
    , columnSpan(false)
    , axis(static_cast<unsigned>(RenderStyle::initialColumnAxis()))
    , progression(static_cast<unsigned>(RenderStyle::initialColumnProgression()))
{
}

inline StyleMultiColData::StyleMultiColData(const StyleMultiColData& other)
    : RefCounted<StyleMultiColData>()
    , width(other.width)
    , count(other.count)
    , rule(other.rule)
    , visitedLinkColumnRuleColor(other.visitedLinkColumnRuleColor)
    , autoWidth(other.autoWidth)
    , autoCount(other.autoCount)
    , fill(other.fill)
    , columnSpan(other.columnSpan)
    , axis(other.axis)
    , progression(other.progression)
{
}

Ref<StyleMultiColData> StyleMultiColData::copy() const
{
    return adoptRef(*new StyleMultiColData(*this));
}

bool StyleMultiColData::operator==(const StyleMultiColData& other) const
{
    return width == other.width && count == other.count
        && rule == other.rule && visitedLinkColumnRuleColor == other.visitedLinkColumnRuleColor
        && autoWidth == other.autoWidth && autoCount == other.autoCount
        && fill == other.fill && columnSpan == other.columnSpan
        && axis == other.axis && progression == other.progression;
}

} // namespace WebCore
