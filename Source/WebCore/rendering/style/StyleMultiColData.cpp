/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2013 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "StyleMultiColData.h"

#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleMultiColData);

StyleMultiColData::StyleMultiColData()
    : width(RenderStyle::initialColumnWidth())
    , count(RenderStyle::initialColumnCount())
    , fill(static_cast<unsigned>(RenderStyle::initialColumnFill()))
    , columnSpan(static_cast<unsigned>(RenderStyle::initialColumnSpan()))
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
    return width == other.width
        && count == other.count
        && rule == other.rule
        && visitedLinkColumnRuleColor == other.visitedLinkColumnRuleColor
        && fill == other.fill
        && columnSpan == other.columnSpan
        && axis == other.axis
        && progression == other.progression;
}

#if !LOG_DISABLED
void StyleMultiColData::dumpDifferences(TextStream& ts, const StyleMultiColData& other) const
{
    LOG_IF_DIFFERENT(width);
    LOG_IF_DIFFERENT(count);
    LOG_IF_DIFFERENT(rule);
    LOG_IF_DIFFERENT(visitedLinkColumnRuleColor);

    LOG_IF_DIFFERENT_WITH_CAST(ColumnFill, fill);
    LOG_IF_DIFFERENT_WITH_CAST(ColumnSpan, columnSpan);
    LOG_IF_DIFFERENT_WITH_CAST(ColumnAxis, axis);
    LOG_IF_DIFFERENT_WITH_CAST(ColumnProgression, progression);
}
#endif // !LOG_DISABLED

} // namespace WebCore
