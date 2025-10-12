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

#include "AnimationUtilities.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<TranslateTransformOperation> TranslateTransformOperation::create(float tx, float ty, float tz, TransformOperation::Type type)
{
    return adoptRef(*new TranslateTransformOperation(tx, ty, tz, type));
}

TranslateTransformOperation::TranslateTransformOperation(float tx, float ty, float tz, TransformOperation::Type type)
    : TransformOperation(type)
    , m_x(tx)
    , m_y(ty)
    , m_z(tz)
{
    RELEASE_ASSERT(isTranslateTransformOperationType(type));
}

bool TranslateTransformOperation::operator==(const TransformOperation& other) const
{
    if (!isSameType(other))
        return false;
    const TranslateTransformOperation& t = downcast<TranslateTransformOperation>(other);
    return m_x == t.m_x && m_y == t.m_y && m_z == t.m_z;
}

Ref<TransformOperation> TranslateTransformOperation::blend(const TransformOperation* from, const BlendingContext& context, bool blendToIdentity) const
{
    if (blendToIdentity)
        return TranslateTransformOperation::create(WebCore::blend(m_x, 0.0f, context), WebCore::blend(m_y, 0.0f, context), WebCore::blend(m_z, 0.0f, context), type());

    auto outputType = sharedPrimitiveType(from);
    if (!outputType)
        return const_cast<TranslateTransformOperation&>(*this);

    const TranslateTransformOperation* fromOp = downcast<TranslateTransformOperation>(from);
    auto fromX = fromOp ? fromOp->m_x : 0.0f;
    auto fromY = fromOp ? fromOp->m_y : 0.0f;
    auto fromZ = fromOp ? fromOp->m_z : 0.0f;
    return TranslateTransformOperation::create(WebCore::blend(fromX, x(), context), WebCore::blend(fromY, y(), context), WebCore::blend(fromZ, z(), context), *outputType);
}

void TranslateTransformOperation::dump(TextStream& ts) const
{
    ts << type() << '(' << m_x << ", "_s << m_y << ", "_s << m_z << ')';
}

} // namespace WebCore
