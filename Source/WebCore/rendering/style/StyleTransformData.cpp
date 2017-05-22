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
#include "StyleTransformData.h"

#include "RenderStyle.h"

namespace WebCore {

StyleTransformData::StyleTransformData()
    : operations(RenderStyle::initialTransform())
    , x(RenderStyle::initialTransformOriginX())
    , y(RenderStyle::initialTransformOriginY())
    , z(RenderStyle::initialTransformOriginZ())
    , transformBox(RenderStyle::initialTransformBox())
{
}

inline StyleTransformData::StyleTransformData(const StyleTransformData& other)
    : RefCounted<StyleTransformData>()
    , operations(other.operations)
    , x(other.x)
    , y(other.y)
    , z(other.z)
    , transformBox(other.transformBox)
{
}

Ref<StyleTransformData> StyleTransformData::copy() const
{
    return adoptRef(*new StyleTransformData(*this));
}

bool StyleTransformData::operator==(const StyleTransformData& other) const
{
    return x == other.x && y == other.y && z == other.z && transformBox == other.transformBox && operations == other.operations;
}

} // namespace WebCore
