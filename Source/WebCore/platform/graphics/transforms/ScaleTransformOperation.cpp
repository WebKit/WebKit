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
#include "ScaleTransformOperation.h"

#include "AnimationUtilities.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

bool ScaleTransformOperation::operator==(const TransformOperation& other) const
{
    if (!isSameType(other))
        return false;
    const ScaleTransformOperation& s = downcast<ScaleTransformOperation>(other);
    return m_x == s.m_x && m_y == s.m_y && m_z == s.m_z;
}

Ref<TransformOperation> ScaleTransformOperation::blend(const TransformOperation* from, const BlendingContext& context, bool blendToIdentity)
{
    if (from && !from->isSameType(*this))
        return *this;
    
    if (blendToIdentity)
        return ScaleTransformOperation::create(WebCore::blend(m_x, 1.0, context),
                                               WebCore::blend(m_y, 1.0, context),
                                               WebCore::blend(m_z, 1.0, context), type());
    
    const ScaleTransformOperation* fromOp = downcast<ScaleTransformOperation>(from);
    double fromX = fromOp ? fromOp->m_x : 1.0;
    double fromY = fromOp ? fromOp->m_y : 1.0;
    double fromZ = fromOp ? fromOp->m_z : 1.0;
    return ScaleTransformOperation::create(WebCore::blend(fromX, m_x, context),
                                           WebCore::blend(fromY, m_y, context),
                                           WebCore::blend(fromZ, m_z, context), type());
}

void ScaleTransformOperation::dump(TextStream& ts) const
{
    ts << type() << "(" << TextStream::FormatNumberRespectingIntegers(m_x) << ", " << TextStream::FormatNumberRespectingIntegers(m_y) << ", " << TextStream::FormatNumberRespectingIntegers(m_z) << ")";
}

} // namespace WebCore
