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
#include "TranslateTransformOperation.h"

#include "FloatConversion.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

bool TranslateTransformOperation::operator==(const TransformOperation& other) const
{
    if (!isSameType(other))
        return false;
    const TranslateTransformOperation& t = downcast<TranslateTransformOperation>(other);
    return m_x == t.m_x && m_y == t.m_y && m_z == t.m_z;
}

Ref<TransformOperation> TranslateTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isSameType(*this))
        return *this;

    Length zeroLength(0, Fixed);
    if (blendToIdentity)
        return TranslateTransformOperation::create(WebCore::blend(m_x, zeroLength, progress), WebCore::blend(m_y, zeroLength, progress), WebCore::blend(m_z, zeroLength, progress), type());

    const TranslateTransformOperation* fromOp = downcast<TranslateTransformOperation>(from);
    Length fromX = fromOp ? fromOp->m_x : zeroLength;
    Length fromY = fromOp ? fromOp->m_y : zeroLength;
    Length fromZ = fromOp ? fromOp->m_z : zeroLength;
    return TranslateTransformOperation::create(WebCore::blend(fromX, x(), progress), WebCore::blend(fromY, y(), progress), WebCore::blend(fromZ, z(), progress), type());
}

void TranslateTransformOperation::dump(TextStream& ts) const
{
    ts << type() << "(" << m_x << ", " << m_y << ", " << m_z << ")";
}

} // namespace WebCore
