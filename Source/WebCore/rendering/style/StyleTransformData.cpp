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

#include "RenderStyleInlines.h"
#include "RenderStyleDifference.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleTransformData);

StyleTransformData::StyleTransformData()
    : transform(RenderStyle::initialTransform())
    , origin(RenderStyle::initialTransformOrigin())
    , transformBox(static_cast<unsigned>(RenderStyle::initialTransformBox()))
{
}

inline StyleTransformData::StyleTransformData(const StyleTransformData& other)
    : RefCounted<StyleTransformData>()
    , transform(other.transform)
    , origin(other.origin)
    , transformBox(other.transformBox)
{
}

Ref<StyleTransformData> StyleTransformData::copy() const
{
    return adoptRef(*new StyleTransformData(*this));
}

bool StyleTransformData::operator==(const StyleTransformData& other) const
{
    return origin == other.origin
        && transformBox == other.transformBox
        && transform == other.transform;
}

#if !LOG_DISABLED
void StyleTransformData::dumpDifferences(TextStream& ts, const StyleTransformData& other) const
{
    LOG_IF_DIFFERENT(transform);
    LOG_IF_DIFFERENT(origin);
    LOG_IF_DIFFERENT_WITH_CAST(TransformBox, transformBox);
}
#endif // !LOG_DISABLED

} // namespace WebCore
