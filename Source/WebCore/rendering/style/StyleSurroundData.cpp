/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "StyleSurroundData.h"

#include "RenderStyleDifference.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleSurroundData);

StyleSurroundData::StyleSurroundData()
    : hasExplicitlySetBorderBottomLeftRadius(false)
    , hasExplicitlySetBorderBottomRightRadius(false)
    , hasExplicitlySetBorderTopLeftRadius(false)
    , hasExplicitlySetBorderTopRightRadius(false)
    , margin(LengthType::Fixed)
    , padding(LengthType::Fixed)
{
}

inline StyleSurroundData::StyleSurroundData(const StyleSurroundData& o)
    : RefCounted<StyleSurroundData>()
    , hasExplicitlySetBorderBottomLeftRadius(o.hasExplicitlySetBorderBottomLeftRadius)
    , hasExplicitlySetBorderBottomRightRadius(o.hasExplicitlySetBorderBottomRightRadius)
    , hasExplicitlySetBorderTopLeftRadius(o.hasExplicitlySetBorderTopLeftRadius)
    , hasExplicitlySetBorderTopRightRadius(o.hasExplicitlySetBorderTopRightRadius)
    , offset(o.offset)
    , margin(o.margin)
    , padding(o.padding)
    , border(o.border)
{
}

Ref<StyleSurroundData> StyleSurroundData::copy() const
{
    return adoptRef(*new StyleSurroundData(*this));
}

bool StyleSurroundData::operator==(const StyleSurroundData& o) const
{
    return offset == o.offset && margin == o.margin && padding == o.padding && border == o.border
        && hasExplicitlySetBorderBottomLeftRadius == o.hasExplicitlySetBorderBottomLeftRadius
        && hasExplicitlySetBorderBottomRightRadius == o.hasExplicitlySetBorderBottomRightRadius
        && hasExplicitlySetBorderTopLeftRadius == o.hasExplicitlySetBorderTopLeftRadius
        && hasExplicitlySetBorderTopRightRadius == o.hasExplicitlySetBorderTopRightRadius;
}

#if !LOG_DISABLED
void StyleSurroundData::dumpDifferences(TextStream& ts, const StyleSurroundData& other) const
{
    LOG_IF_DIFFERENT(hasExplicitlySetBorderBottomLeftRadius);
    LOG_IF_DIFFERENT(hasExplicitlySetBorderBottomRightRadius);
    LOG_IF_DIFFERENT(hasExplicitlySetBorderTopLeftRadius);
    LOG_IF_DIFFERENT(hasExplicitlySetBorderTopRightRadius);

    LOG_IF_DIFFERENT(offset);
    LOG_IF_DIFFERENT(margin);
    LOG_IF_DIFFERENT(padding);
    LOG_IF_DIFFERENT(border);
}
#endif

} // namespace WebCore
