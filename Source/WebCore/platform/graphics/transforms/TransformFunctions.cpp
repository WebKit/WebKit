/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TransformFunctions.h"

#include "FloatConversion.h"

namespace WebCore {

// MARK: - Blending

// MARK: Matrix (blending)

Matrix blend(const Matrix& a, const Matrix& b, const BlendingContext& context)
{
    TransformationMatrix from { a.value };
    TransformationMatrix to { b.value };

    to.blend(from, context.progress, context.compositeOperation);
    return { to.toAffineTransform() };
}

// MARK: Matrix3D (blending)

Matrix3D blend(const Matrix3D& a, const Matrix3D& b, const BlendingContext& context)
{
    TransformationMatrix from = a.value;
    TransformationMatrix to = b.value;

    to.blend(from, context.progress, context.compositeOperation);
    return { to };
}

// MARK: Rotate3D (blending)

Rotate3D blend(const Rotate3D& a, const Rotate3D& b, const BlendingContext& context)
{
    // Interpolation of primitives and derived transform functions
    //
    // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
    //
    // For interpolations with the primitive rotate3d(), the direction vectors of the transform functions get
    // normalized first. If the normalized vectors are not equal and both rotation angles are non-zero the transform
    // functions get converted into 4x4 matrices first and interpolated as defined in section Interpolation of Matrices
    // afterwards. Otherwise the rotation angle gets interpolated numerically and the rotation vector of the non-zero
    // angle is used or (0, 0, 1) if both angles are zero.

    auto normalizedVector = [](const Rotate3D& op) -> FloatPoint3D {
        if (auto length = std::hypot(op.x, op.y, op.z))
            return { narrowPrecisionToFloat(op.x / length), narrowPrecisionToFloat(op.y / length), narrowPrecisionToFloat(op.z / length) };
        return { };
    };

    auto fromAngle = a.angle;
    auto toAngle = b.angle;
    auto fromNormalizedVector = normalizedVector(a);
    auto toNormalizedVector = normalizedVector(b);
    if (!fromAngle || !toAngle || fromNormalizedVector == toNormalizedVector) {
        auto vector = (!fromAngle && toAngle) ? toNormalizedVector : fromNormalizedVector;
        return { vector.x(), vector.y(), vector.z(), WebCore::blend(fromAngle, toAngle, context) };
    }

    // Create the 2 rotation matrices

    TransformationMatrix fromT;
    fromT.rotate3d(a.x, a.y, a.z, a.angle);

    TransformationMatrix toT;
    toT.rotate3d(b.x, b.y, b.z, b.angle);

    // Blend them.

    toT.blend(fromT, context.progress, context.compositeOperation);

    // Extract the result as a quaternion.

    TransformationMatrix::Decomposed4Type decomp;
    if (!toT.decompose4(decomp))
        return context.progress > 0.5 ? b : a;

    // Convert that to Axis/Angle form.

    auto x = decomp.quaternion.x;
    auto y = decomp.quaternion.y;
    auto z = decomp.quaternion.z;
    auto length = std::hypot(x, y, z);
    auto angle = 0.0;

    if (length > 0.00001) {
        x /= length;
        y /= length;
        z /= length;
        angle = rad2deg(acos(decomp.quaternion.w) * 2);
    } else {
        x = 0;
        y = 0;
        z = 1;
    }

    return { x, y, z, angle };
}

// MARK: Rotate (blending)

Rotate blend(const Rotate& a, const Rotate& b, const BlendingContext& context)
{
    return {
        WebCore::blend(a.value, b.value, context),
    };
}

// MARK: Skew (blending)

Skew blend(const Skew& a, const Skew& b, const BlendingContext& context)
{
    return {
        WebCore::blend(a.x, b.x, context),
        WebCore::blend(a.y, b.y, context),
    };
}

// MARK: SkewX (blending)

SkewX blend(const SkewX& a, const SkewX& b, const BlendingContext& context)
{
    return {
        WebCore::blend(a.value, b.value, context),
    };
}

// MARK: SkewY (blending)

SkewY blend(const SkewY& a, const SkewY& b, const BlendingContext& context)
{
    return {
        WebCore::blend(a.value, b.value, context),
    };
}

// MARK: Scale3D (blending)

double blendScaleComponent(double from, double to, const BlendingContext& context)
{
    switch (context.compositeOperation) {
    case CompositeOperation::Replace:
        return WebCore::blend(from, to, context);
    case CompositeOperation::Add:
        ASSERT(context.progress == 1.0);
        return from * to;
    case CompositeOperation::Accumulate:
        ASSERT(context.progress == 1.0);
        return from + to - 1.0;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Scale3D blend(const Scale3D& a, const Scale3D& b, const BlendingContext& context)
{
    return {
        blendScaleComponent(a.x, b.x, context),
        blendScaleComponent(a.y, b.y, context),
        blendScaleComponent(a.z, b.z, context),
    };
}

// MARK: Scale (blending)

Scale blend(const Scale& a, const Scale& b, const BlendingContext& context)
{
    return {
        blendScaleComponent(a.x, b.x, context),
        blendScaleComponent(a.y, b.y, context),
    };
}

// MARK: Translate3D (blending)

Translate3D blend(const Translate3D& a, const Translate3D& b, const BlendingContext& context)
{
    return {
        WebCore::blend(a.x, b.x, context),
        WebCore::blend(a.y, b.y, context),
        WebCore::blend(a.z, b.z, context),
    };
}

// MARK: Translate (blending)

Translate blend(const Translate& a, const Translate& b, const BlendingContext& context)
{
    return {
        WebCore::blend(a.x, b.x, context),
        WebCore::blend(a.y, b.y, context),
    };
}

// MARK: Perspective (blending)

Perspective blend(const Perspective& a, const Perspective& b, const BlendingContext& context)
{
    auto pInverse = WebCore::blend(a.inverse(), b.inverse(), context);
    if (pInverse > 0.0 && std::isnormal(pInverse))
        return Perspective { 1.0 / pInverse };
    return Perspective { std::nullopt };
}

// MARK: - TransformFunction (blending)

bool haveSharedPrimitiveType(const TransformFunction& a, const TransformFunction& b)
{
    return std::visit(WTF::makeVisitor(
        []<typename A, typename B>(const A&, const B&) -> bool {
            return std::is_same_v<A, B> || HaveSharedPrimitive3DTransform<A, B>();
        }
    ), a, b);
}

TransformFunction blend(const TransformFunction& a, const TransformFunction& b, const BlendingContext& context)
{
    return std::visit(WTF::makeVisitor(
        [&]<typename A, typename B>(const A& a, const B& b) -> TransformFunction {
            if constexpr (std::is_same_v<A, B>)
                return { WebCore::blend(a, b, context) };
            else if constexpr (HaveSharedPrimitive3DTransform<A, B>())
                return { WebCore::blend(a.toPrimitive3DTransform(), b.toPrimitive3DTransform(), context) };
            else
                return b;
        }
    ), a, b);
}

// MARK: - TransformFunction (primitive transform type)

TransformFunctionType PrimitiveTransformType<TransformFunction>::operator()(const TransformFunction& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return T::operationType; });
}

} // namespace WebCore
