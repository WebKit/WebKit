/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2013 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleMultiColumnData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MultiColumnData);

MultiColumnData::MultiColumnData()
    : columnWidth(ComputedStyle::initialColumnWidth())
    , columnCount(ComputedStyle::initialColumnCount())
    , columnFill(static_cast<unsigned>(ComputedStyle::initialColumnFill()))
    , columnSpan(static_cast<unsigned>(ComputedStyle::initialColumnSpan()))
    , columnAxis(static_cast<unsigned>(ComputedStyle::initialColumnAxis()))
    , columnProgression(static_cast<unsigned>(ComputedStyle::initialColumnProgression()))
{
}

inline MultiColumnData::MultiColumnData(const MultiColumnData& other)
    : RefCounted<MultiColumnData>()
    , columnWidth(other.columnWidth)
    , columnCount(other.columnCount)
    , columnRule(other.columnRule)
    , visitedLinkColumnRuleColor(other.visitedLinkColumnRuleColor)
    , columnFill(other.columnFill)
    , columnSpan(other.columnSpan)
    , columnAxis(other.columnAxis)
    , columnProgression(other.columnProgression)
{
}

Ref<MultiColumnData> MultiColumnData::copy() const
{
    return adoptRef(*new MultiColumnData(*this));
}

bool MultiColumnData::operator==(const MultiColumnData& other) const
{
    return columnWidth == other.columnWidth
        && columnCount == other.columnCount
        && columnRule == other.columnRule
        && visitedLinkColumnRuleColor == other.visitedLinkColumnRuleColor
        && columnFill == other.columnFill
        && columnSpan == other.columnSpan
        && columnAxis == other.columnAxis
        && columnProgression == other.columnProgression;
}

#if !LOG_DISABLED
void MultiColumnData::dumpDifferences(TextStream& ts, const MultiColumnData& other) const
{
    LOG_IF_DIFFERENT(columnWidth);
    LOG_IF_DIFFERENT(columnCount);
    LOG_IF_DIFFERENT(columnRule);
    LOG_IF_DIFFERENT(visitedLinkColumnRuleColor);

    LOG_IF_DIFFERENT_WITH_CAST(ColumnFill, columnFill);
    LOG_IF_DIFFERENT_WITH_CAST(ColumnSpan, columnSpan);
    LOG_IF_DIFFERENT_WITH_CAST(ColumnAxis, columnAxis);
    LOG_IF_DIFFERENT_WITH_CAST(ColumnProgression, columnProgression);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
