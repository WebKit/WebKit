/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleDeprecatedFlexibleBoxData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(DeprecatedFlexibleBoxData);

DeprecatedFlexibleBoxData::DeprecatedFlexibleBoxData()
    : boxFlex(ComputedStyle::initialBoxFlex())
    , boxFlexGroup(ComputedStyle::initialBoxFlexGroup())
    , boxOrdinalGroup(ComputedStyle::initialBoxOrdinalGroup())
    , boxAlign(static_cast<unsigned>(ComputedStyle::initialBoxAlign()))
    , boxPack(static_cast<unsigned>(ComputedStyle::initialBoxPack()))
    , boxOrient(static_cast<unsigned>(ComputedStyle::initialBoxOrient()))
    , boxLines(static_cast<unsigned>(ComputedStyle::initialBoxLines()))
{
}

inline DeprecatedFlexibleBoxData::DeprecatedFlexibleBoxData(const DeprecatedFlexibleBoxData& other)
    : RefCounted<DeprecatedFlexibleBoxData>()
    , boxFlex(other.boxFlex)
    , boxFlexGroup(other.boxFlexGroup)
    , boxOrdinalGroup(other.boxOrdinalGroup)
    , boxAlign(other.boxAlign)
    , boxPack(other.boxPack)
    , boxOrient(other.boxOrient)
    , boxLines(other.boxLines)
{
}

Ref<DeprecatedFlexibleBoxData> DeprecatedFlexibleBoxData::copy() const
{
    return adoptRef(*new DeprecatedFlexibleBoxData(*this));
}

bool DeprecatedFlexibleBoxData::operator==(const DeprecatedFlexibleBoxData& other) const
{
    return boxFlex == other.boxFlex
        && boxFlexGroup == other.boxFlexGroup
        && boxOrdinalGroup == other.boxOrdinalGroup
        && boxAlign == other.boxAlign
        && boxPack == other.boxPack
        && boxOrient == other.boxOrient
        && boxLines == other.boxLines;
}

#if !LOG_DISABLED
void DeprecatedFlexibleBoxData::dumpDifferences(TextStream& ts, const DeprecatedFlexibleBoxData& other) const
{
    LOG_IF_DIFFERENT(boxFlex);
    LOG_IF_DIFFERENT(boxFlexGroup);
    LOG_IF_DIFFERENT(boxOrdinalGroup);

    LOG_IF_DIFFERENT_WITH_CAST(BoxAlignment, boxAlign);
    LOG_IF_DIFFERENT_WITH_CAST(BoxPack, boxPack);
    LOG_IF_DIFFERENT_WITH_CAST(BoxOrient, boxOrient);
    LOG_IF_DIFFERENT_WITH_CAST(BoxLines, boxLines);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
