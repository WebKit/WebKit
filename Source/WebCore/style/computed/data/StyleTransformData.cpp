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
#include "StyleTransformData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TransformData);

TransformData::TransformData()
    : transform(ComputedStyle::initialTransform())
    , origin({ ComputedStyle::initialTransformOriginX(), ComputedStyle::initialTransformOriginY(), ComputedStyle::initialTransformOriginZ() })
    , transformBox(static_cast<unsigned>(ComputedStyle::initialTransformBox()))
{
}

inline TransformData::TransformData(const TransformData& other)
    : RefCounted<TransformData>()
    , transform(other.transform)
    , origin(other.origin)
    , transformBox(other.transformBox)
{
}

Ref<TransformData> TransformData::copy() const
{
    return adoptRef(*new TransformData(*this));
}

bool TransformData::operator==(const TransformData& other) const
{
    return origin == other.origin
        && transformBox == other.transformBox
        && transform == other.transform;
}

#if !LOG_DISABLED
void TransformData::dumpDifferences(TextStream& ts, const TransformData& other) const
{
    LOG_IF_DIFFERENT(transform);
    LOG_IF_DIFFERENT(origin);
    LOG_IF_DIFFERENT_WITH_CAST(TransformBox, transformBox);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
