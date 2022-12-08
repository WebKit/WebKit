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
#include "SkewTransformOperation.h"

#include "AnimationUtilities.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<SkewTransformOperation> SkewTransformOperation::create(double angleX, double angleY, TransformOperation::Type type)
{
    return adoptRef(*new SkewTransformOperation(angleX, angleY, type));
}

SkewTransformOperation::SkewTransformOperation(double angleX, double angleY, TransformOperation::Type type)
    : TransformOperation(type)
    , m_angleX(angleX)
    , m_angleY(angleY)
{
    ASSERT(isSkewTransformOperationType());
}

bool SkewTransformOperation::operator==(const TransformOperation& other) const
{
    if (!isSameType(other))
        return false;
    const SkewTransformOperation& s = downcast<SkewTransformOperation>(other);
    return m_angleX == s.m_angleX && m_angleY == s.m_angleY;
}

Ref<TransformOperation> SkewTransformOperation::blend(const TransformOperation* from, const BlendingContext& context, bool blendToIdentity)
{
    if (blendToIdentity)
        return SkewTransformOperation::create(WebCore::blend(m_angleX, 0.0, context), WebCore::blend(m_angleY, 0.0, context), type());

    auto outputType = sharedPrimitiveType(from);
    if (!outputType)
        return *this;

    const SkewTransformOperation* fromOp = downcast<SkewTransformOperation>(from);
    double fromAngleX = fromOp ? fromOp->m_angleX : 0;
    double fromAngleY = fromOp ? fromOp->m_angleY : 0;
    return SkewTransformOperation::create(WebCore::blend(fromAngleX, m_angleX, context), WebCore::blend(fromAngleY, m_angleY, context), *outputType);
}

void SkewTransformOperation::dump(TextStream& ts) const
{
    ts << type() << "(" << TextStream::FormatNumberRespectingIntegers(m_angleX) << "deg, " << TextStream::FormatNumberRespectingIntegers(m_angleY) << "deg)";
}

} // namespace WebCore
