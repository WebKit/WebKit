/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleRotateTransformFunction.h"

#include "AnimationUtilities.h"
#include "RotateTransformOperation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <algorithm>
#include <wtf/MathExtras.h>

namespace WebCore {
namespace Style {

RotateTransformFunction::RotateTransformFunction(Number<> x, Number<> y, Number<> z, Angle<> angle, TransformFunctionBase::Type type)
    : TransformFunctionBase(type)
    , m_x(x)
    , m_y(y)
    , m_z(z)
    , m_angle(angle)
{
    RELEASE_ASSERT(isRotateTransformFunctionType(type));
}

Ref<const RotateTransformFunction> RotateTransformFunction::create(Angle<> angle, TransformFunctionBase::Type type)
{
    return adoptRef(*new RotateTransformFunction(0, 0, 1, angle, type));
}

Ref<const RotateTransformFunction> RotateTransformFunction::create(Number<> x, Number<> y, Number<> z, Angle<> angle, TransformFunctionBase::Type type)
{
    return adoptRef(*new RotateTransformFunction(x, y, z, angle, type));
}

Ref<const TransformFunctionBase> RotateTransformFunction::clone() const
{
    return adoptRef(*new RotateTransformFunction(m_x, m_y, m_z, m_angle, type()));
}

Ref<TransformOperation> RotateTransformFunction::toPlatform(const FloatSize&) const
{
    return RotateTransformOperation::create(m_x.value, m_y.value, m_z.value, m_angle.value, Style::toPlatform(type()));
}

bool RotateTransformFunction::operator==(const TransformFunctionBase& other) const
{
    if (!isSameType(other))
        return false;

    auto& otherRotate = downcast<RotateTransformFunction>(other);
    return m_angle == otherRotate.m_angle && m_x == otherRotate.m_x && m_y == otherRotate.m_y && m_z == otherRotate.m_z;
}

void RotateTransformFunction::apply(TransformationMatrix& transform, const FloatSize&) const
{
    if (type() == TransformFunctionBase::Type::Rotate)
        transform.rotate(m_angle.value);
    else
        transform.rotate3d(m_x.value, m_y.value, m_z.value, m_angle.value);
}

Ref<const TransformFunctionBase> RotateTransformFunction::blend(const TransformFunctionBase* from, const BlendingContext& context, bool blendToIdentity) const
{
    if (blendToIdentity) {
        if (context.compositeOperation == CompositeOperation::Accumulate)
            return RotateTransformFunction::create(m_x, m_y, m_z, m_angle, type());
        return RotateTransformFunction::create(m_x, m_y, m_z, m_angle.value - m_angle.value * context.progress, type());
    }
    auto outputType = sharedPrimitiveType(from);
    if (!outputType)
        return *this;

    auto* fromOp = downcast<RotateTransformFunction>(from);
    auto* toOp = this;

    // Interpolation of primitives and derived transform functions
    //
    // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
    //
    // For interpolations with the primitive rotate3d(), the direction vectors of the transform functions get
    // normalized first. If the normalized vectors are not equal and both rotation angles are non-zero the transform
    // functions get converted into 4x4 matrices first and interpolated as defined in section Interpolation of Matrices
    // afterwards. Otherwise the rotation angle gets interpolated numerically and the rotation vector of the non-zero
    // angle is used or (0, 0, 1) if both angles are zero.

    auto normalizedVector = [](const RotateTransformFunction& op) -> FloatPoint3D {
        if (auto length = std::hypot(op.m_x.value, op.m_y.value, op.m_z.value))
            return { static_cast<float>(op.m_x.value / length), static_cast<float>(op.m_y.value / length), static_cast<float>(op.m_z.value / length) };
        return { };
    };

    auto fromAngle = fromOp ? fromOp->m_angle : Angle<> { 0 };
    auto toAngle = toOp->m_angle;
    auto fromNormalizedVector = fromOp ? normalizedVector(*fromOp) : FloatPoint3D(0, 0, 1);
    auto toNormalizedVector = normalizedVector(*toOp);
    if (fromAngle.isZero() || toAngle.isZero() || fromNormalizedVector == toNormalizedVector) {
        auto vector = (fromAngle.isZero() && !toAngle.isZero()) ? toNormalizedVector : fromNormalizedVector;
        return RotateTransformFunction::create(vector.x(), vector.y(), vector.z(), Style::blend(fromAngle, toAngle, context), *outputType);
    }

    // Create the 2 rotation matrices
    TransformationMatrix fromT;
    TransformationMatrix toT;
    fromT.rotate3d((fromOp ? fromOp->m_x.value : 0),
        (fromOp ? fromOp->m_y.value : 0),
        (fromOp ? fromOp->m_z.value : 1),
        (fromOp ? fromOp->m_angle.value : 0));

    toT.rotate3d((toOp ? toOp->m_x.value : 0),
        (toOp ? toOp->m_y.value : 0),
        (toOp ? toOp->m_z.value : 1),
        (toOp ? toOp->m_angle.value : 0));

    // Blend them
    toT.blend(fromT, context.progress, context.compositeOperation);

    // Extract the result as a quaternion
    TransformationMatrix::Decomposed4Type decomposed;
    if (!toT.decompose4(decomposed)) {
        RefPtr usedOperation = context.progress > 0.5 ? this : fromOp;
        return RotateTransformFunction::create(usedOperation->x(), usedOperation->y(), usedOperation->z(), usedOperation->angle(), TransformFunctionBase::Type::Rotate3D);
    }

    // Convert that to Axis/Angle form
    double x = decomposed.quaternion.x;
    double y = decomposed.quaternion.y;
    double z = decomposed.quaternion.z;
    double length = std::hypot(x, y, z);
    double angle = 0;

    if (length > 0.00001) {
        x /= length;
        y /= length;
        z /= length;
        angle = rad2deg(acos(decomposed.quaternion.w) * 2);
    } else {
        x = 0;
        y = 0;
        z = 1;
    }
    return RotateTransformFunction::create(x, y, z, angle, Type::Rotate3D);
}

void RotateTransformFunction::dump(TextStream& ts) const
{
    ts << type() << '(' << m_x << ", "_s << m_y << ", "_s << m_z << ", "_s << m_angle << ')';
}

} // namespace Style
} // namespace WebCore
