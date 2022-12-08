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
#include "RotateTransformOperation.h"

#include "AnimationUtilities.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<RotateTransformOperation> RotateTransformOperation::create(double x, double y, double z, double angle, TransformOperation::Type type)
{
    return adoptRef(*new RotateTransformOperation(x, y, z, angle, type));
}

RotateTransformOperation::RotateTransformOperation(double x, double y, double z, double angle, TransformOperation::Type type)
    : TransformOperation(type)
    , m_x(x)
    , m_y(y)
    , m_z(z)
    , m_angle(angle)
{
    ASSERT(isRotateTransformOperationType());
}

bool RotateTransformOperation::operator==(const TransformOperation& other) const
{
    if (!isSameType(other))
        return false;
    const RotateTransformOperation& r = downcast<RotateTransformOperation>(other);
    return m_x == r.m_x && m_y == r.m_y && m_z == r.m_z && m_angle == r.m_angle;
}

Ref<TransformOperation> RotateTransformOperation::blend(const TransformOperation* from, const BlendingContext& context, bool blendToIdentity)
{
    if (blendToIdentity) {
        if (context.compositeOperation == CompositeOperation::Accumulate)
            return RotateTransformOperation::create(m_x, m_y, m_z, m_angle, type());
        return RotateTransformOperation::create(m_x, m_y, m_z, m_angle - m_angle * context.progress, type());
    }
    auto outputType = sharedPrimitiveType(from);
    if (!outputType)
        return *this;

    const RotateTransformOperation* fromOp = downcast<RotateTransformOperation>(from);
    const RotateTransformOperation* toOp = this;

    // Interpolation of primitives and derived transform functions
    //
    // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
    //
    // For interpolations with the primitive rotate3d(), the direction vectors of the transform functions get
    // normalized first. If the normalized vectors are not equal and both rotation angles are non-zero the transform
    // functions get converted into 4x4 matrices first and interpolated as defined in section Interpolation of Matrices
    // afterwards. Otherwise the rotation angle gets interpolated numerically and the rotation vector of the non-zero
    // angle is used or (0, 0, 1) if both angles are zero.

    auto normalizedVector = [](const RotateTransformOperation& op) -> FloatPoint3D {
        auto length = std::hypot(op.m_x, op.m_y, op.m_z);
        return { static_cast<float>(op.m_x / length), static_cast<float>(op.m_y / length), static_cast<float>(op.m_z / length) };
    };

    double fromAngle = fromOp ? fromOp->m_angle : 0.0;
    double toAngle = toOp->m_angle;
    auto fromNormalizedVector = fromOp ? normalizedVector(*fromOp) : FloatPoint3D(0, 0, 1);
    auto toNormalizedVector = normalizedVector(*toOp);
    if (!fromAngle || !toAngle || fromNormalizedVector == toNormalizedVector) {
        auto vector = (!fromAngle && toAngle) ? toNormalizedVector : fromNormalizedVector;
        return RotateTransformOperation::create(vector.x(), vector.y(), vector.z(), WebCore::blend(fromAngle, toAngle, context), *outputType);
    }

    // Create the 2 rotation matrices
    TransformationMatrix fromT;
    TransformationMatrix toT;
    fromT.rotate3d((fromOp ? fromOp->m_x : 0),
        (fromOp ? fromOp->m_y : 0),
        (fromOp ? fromOp->m_z : 1),
        (fromOp ? fromOp->m_angle : 0));

    toT.rotate3d((toOp ? toOp->m_x : 0),
        (toOp ? toOp->m_y : 0),
        (toOp ? toOp->m_z : 1),
        (toOp ? toOp->m_angle : 0));
    
    // Blend them
    toT.blend(fromT, context.progress, context.compositeOperation);
    
    // Extract the result as a quaternion
    TransformationMatrix::Decomposed4Type decomp;
    toT.decompose4(decomp);
    
    // Convert that to Axis/Angle form
    double x = -decomp.quaternionX;
    double y = -decomp.quaternionY;
    double z = -decomp.quaternionZ;
    double length = std::hypot(x, y, z);
    double angle = 0;
    
    if (length > 0.00001) {
        x /= length;
        y /= length;
        z /= length;
        angle = rad2deg(acos(decomp.quaternionW) * 2);
    } else {
        x = 0;
        y = 0;
        z = 1;
    }
    return RotateTransformOperation::create(x, y, z, angle, Type::Rotate3D);
}

void RotateTransformOperation::dump(TextStream& ts) const
{
    ts << type() << "(" << TextStream::FormatNumberRespectingIntegers(m_x) << ", " << TextStream::FormatNumberRespectingIntegers(m_y) << ", " << TextStream::FormatNumberRespectingIntegers(m_z) << ", " << TextStream::FormatNumberRespectingIntegers(m_angle) << "deg)";
}

} // namespace WebCore
