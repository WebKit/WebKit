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

#pragma once

#include "AffineTransform.h"
#include "AnimationUtilities.h"
#include "TransformFunctionType.h"
#include "TransformListSharedPrimitivesPrefix.h"
#include "TransformationMatrix.h"

namespace WebCore {

// MARK: - Transform Functions

struct Matrix;
struct Matrix3D;
struct Rotate;
struct Rotate3D;
struct Skew;
struct SkewX;
struct SkewY;
struct Scale;
struct Scale3D;
struct Translate;
struct Translate3D;
struct Perspective;

struct Matrix {
    static constexpr auto operationType = TransformFunctionType::Matrix;

    AffineTransform value;

    static Matrix identity() { return { WebCore::identity }; }
    bool isIdentity() const { return value.isIdentity(); }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    void applyTransform(TransformationMatrix& transform) const
    {
        TransformationMatrix matrix = value;
        transform.multiply(matrix);
    }

    bool operator==(const Matrix&) const = default;
};
auto blend(const Matrix&, const Matrix&, const BlendingContext&) -> Matrix;

struct Matrix3D {
    static constexpr auto operationType = TransformFunctionType::Matrix3D;

    TransformationMatrix value;

    static Matrix3D identity() { return { TransformationMatrix::identity }; }
    bool isIdentity() const { return value.isIdentity(); }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return value.isAffine(); }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.multiply(value);
    }

    bool operator==(const Matrix3D&) const = default;
};
auto blend(const Matrix3D&, const Matrix3D&, const BlendingContext&) -> Matrix3D;

struct Rotate3D {
    static constexpr auto operationType = TransformFunctionType::Rotate3D;
    using Primitive3DTransform = Rotate3D;

    double x;
    double y;
    double z;
    double angle;

    static Rotate3D identity() { return { .x = 0, .y = 0, .z = 1, .angle = 0 }; }
    bool isIdentity() const { return angle == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return (x == 0 && y == 0) || angle == 0; }

    void applyTransform(TransformationMatrix& transform) const
    {
        if (x == 0 && y == 0 && z == 1)
            transform.rotate(angle);
        else
            transform.rotate3d(x, y, z, angle);
    }
    void applyUnroundedTransform(TransformationMatrix& transform) const
    {
        if (x == 0 && y == 0 && z == 1)
            transform.rotate(angle, TransformationMatrix::RotationSnapping::None);
        else
            transform.rotate3d(x, y, z, angle, TransformationMatrix::RotationSnapping::None);
    }

    Primitive3DTransform toPrimitive3DTransform() const { return *this; }

    constexpr bool operator==(const Rotate3D&) const = default;
};
auto blend(const Rotate3D&, const Rotate3D&, const BlendingContext&) -> Rotate3D;

struct Rotate {
    static constexpr auto operationType = TransformFunctionType::Rotate;
    using Primitive3DTransform = Rotate3D;

    double value;

    static Rotate identity() { return { 0 }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.rotate(value);
    }
    void applyUnroundedTransform(TransformationMatrix& transform) const
    {
        transform.rotate(value, TransformationMatrix::RotationSnapping::None);
    }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = 0, .y = 0, .z = 1, .angle = value }; }

    constexpr bool operator==(const Rotate&) const = default;
};
auto blend(const Rotate&, const Rotate&, const BlendingContext&) -> Rotate;

struct Skew {
    static constexpr auto operationType = TransformFunctionType::Skew;

    double x;
    double y;

    static Skew identity() { return { .x = 0, .y = 0 }; }
    bool isIdentity() const { return x == 0 && y == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.skew(x, y);
    }

    constexpr bool operator==(const Skew&) const = default;
};
auto blend(const Skew&, const Skew&, const BlendingContext&) -> Skew;

struct SkewX {
    static constexpr auto operationType = TransformFunctionType::SkewX;

    double value;

    static SkewX identity() { return { .value = 0 }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.skew(value, 0);
    }

    constexpr bool operator==(const SkewX&) const = default;
};
auto blend(const SkewX&, const SkewX&, const BlendingContext&) -> SkewX;

struct SkewY {
    static constexpr auto operationType = TransformFunctionType::SkewY;

    double value;

    static SkewY identity() { return { .value = 0 }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.skew(0, value);
    }

    constexpr bool operator==(const SkewY&) const = default;
};
auto blend(const SkewY&, const SkewY&, const BlendingContext&) -> SkewY;

struct Scale3D {
    static constexpr auto operationType = TransformFunctionType::Scale3D;
    using Primitive3DTransform = Scale3D;

    double x;
    double y;
    double z;

    static Scale3D identity() { return { .x = 1, .y = 1, .z = 1 }; }
    bool isIdentity() const { return x == 1 && y == 1 && z == 1; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return z == 1; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.scale3d(x, y, z);
    }

    Primitive3DTransform toPrimitive3DTransform() const { return *this; }

    constexpr bool operator==(const Scale3D&) const = default;
};
auto blend(const Scale3D&, const Scale3D&, const BlendingContext&) -> Scale3D;

struct Scale {
    static constexpr auto operationType = TransformFunctionType::Scale;
    using Primitive3DTransform = Scale3D;

    double x;
    double y;

    static Scale identity() { return { .x = 1, .y = 1 }; }
    bool isIdentity() const { return x == 1 && y == 1; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.scale3d(x, y, 1);
    }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = x, .y = y, .z = 1 }; }

    constexpr bool operator==(const Scale&) const = default;
};
auto blend(const Scale&, const Scale&, const BlendingContext&) -> Scale;

struct Translate3D {
    static constexpr auto operationType = TransformFunctionType::Translate3D;
    using Primitive3DTransform = Translate3D;

    double x;
    double y;
    double z;

    static Translate3D identity() { return { .x = 0, .y = 0, .z = 0 }; }
    bool isIdentity() const { return x == 0 && y == 0 && z == 0; }
    constexpr bool isAffectedByTransformOrigin() const { return false; }
    bool isRepresentableIn2D() const { return z == 0; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.translate3d(x, y, z);
    }

    Primitive3DTransform toPrimitive3DTransform() const { return *this; }

    constexpr bool operator==(const Translate3D&) const = default;
};
auto blend(const Translate3D&, const Translate3D&, const BlendingContext&) -> Translate3D;

struct Translate {
    static constexpr auto operationType = TransformFunctionType::Translate;
    using Primitive3DTransform = Translate3D;

    double x;
    double y;

    static Translate identity() { return { .x = 0, .y = 0 }; }
    bool isIdentity() const { return x == 0 && y == 0; }
    constexpr bool isAffectedByTransformOrigin() const { return false; }
    constexpr bool isRepresentableIn2D() const { return true; }

    void applyTransform(TransformationMatrix& transform) const
    {
        transform.translate3d(x, y, 0);
    }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = x, .y = y, .z = 0 }; }

    constexpr bool operator==(const Translate&) const = default;
};
auto blend(const Translate&, const Translate&, const BlendingContext&) -> Translate;

struct Perspective {
    static constexpr auto operationType = TransformFunctionType::Perspective;

    std::optional<double> value;

    static Perspective identity() { return { std::nullopt }; }
    bool isIdentity() const { return !value; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return false; }

    // From https://www.w3.org/TR/css-transforms-2/#perspective-property:
    // "As very small <length> values can produce bizarre rendering results and stress the numerical accuracy of
    // transform calculations, values less than 1px must be treated as 1px for rendering purposes. (This clamping
    // does not affect the underlying value, so perspective: 0; in a stylesheet will still serialize back as 0.)"

    double inverse() const
    {
        return value ? 1.0 / std::max(1.0, *value) : 0.0;
    }

    void applyTransform(TransformationMatrix& transform) const
    {
        if (value)
            transform.applyPerspective(std::max(1.0, *value));
    }

    constexpr bool operator==(const Perspective&) const = default;
};
auto blend(const Perspective&, const Perspective&, const BlendingContext&) -> Perspective;

using TransformFunction = std::variant<
    Matrix,
    Matrix3D,
    Rotate,
    Rotate3D,
    Skew,
    SkewX,
    SkewY,
    Scale,
    Scale3D,
    Translate,
    Translate3D,
    Perspective
>;

inline TransformFunction identityTransformFunction(const TransformFunction& function)
{
    return WTF::switchOn(function, [&]<typename T>(const T&) -> TransformFunction { return T::identity(); });
}

// MARK: - Concepts

template<typename T> concept IsTransformFunction
    = std::same_as<T, Matrix>
   || std::same_as<T, Matrix3D>
   || std::same_as<T, Rotate>
   || std::same_as<T, Rotate3D>
   || std::same_as<T, Skew>
   || std::same_as<T, SkewX>
   || std::same_as<T, SkewY>
   || std::same_as<T, Scale>
   || std::same_as<T, Scale3D>
   || std::same_as<T, Translate>
   || std::same_as<T, Translate3D>
   || std::same_as<T, Perspective>;

template<typename T> concept Is3D
    = std::same_as<T, Matrix3D>
   || std::same_as<T, Rotate3D>
   || std::same_as<T, Scale3D>
   || std::same_as<T, Translate3D>
   || std::same_as<T, Perspective>;

inline bool is3D(const TransformFunction& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return Is3D<T>; });
}

template<typename T> concept IsRotate
    = std::same_as<T, Rotate>
   || std::same_as<T, Rotate3D>;

inline bool isRotate(const TransformFunction& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return IsRotate<T>; });
}

template<typename T> concept IsScale
    = std::same_as<T, Scale>
   || std::same_as<T, Scale3D>;

inline bool isScale(const TransformFunction& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return IsScale<T>; });
}

template<typename T> concept IsTranslate
    = std::same_as<T, Translate>
   || std::same_as<T, Translate3D>;

inline bool isTranslate(const TransformFunction& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return IsTranslate<T>; });
}

template<typename T> concept HasPrimitive2DTransform = requires {
    typename T::Primitive2DTransform;
};
template<typename T> concept HasPrimitive3DTransform = requires {
    typename T::Primitive3DTransform;
};

template<typename T> using Primitive2DTransform = typename T::Primitive2DTransform;
template<typename T> using Primitive3DTransform = typename T::Primitive3DTransform;

template<typename A, typename B> consteval bool HaveSharedPrimitive2DTransform()
{
    if constexpr (HasPrimitive2DTransform<A> && HasPrimitive2DTransform<B>)
        return std::is_same_v<Primitive2DTransform<A>, Primitive2DTransform<B>>;
    else
        return false;
}

template<typename A, typename B> consteval bool HaveSharedPrimitive3DTransform()
{
    if constexpr (HasPrimitive3DTransform<A> && HasPrimitive3DTransform<B>)
        return std::is_same_v<Primitive3DTransform<A>, Primitive3DTransform<B>>;
    else
        return false;
}

// MARK: - Blending

// Returns whether the two functions share a common primitive.
// https://drafts.csswg.org/css-transforms-1/#transform-primitives
bool haveSharedPrimitiveType(const TransformFunction&, const TransformFunction&);

// Helper for blending components of `Scale` transforms.
double blendScaleComponent(double from, double to, const BlendingContext&);

// Blends two `TransformFunction`s.
auto blend(const TransformFunction&, const TransformFunction&, const BlendingContext&) -> TransformFunction;

// Blends two `std::optional<TransformFunction>`s.
template<IsTransformFunction T> auto blend(const std::optional<T>& from, const std::optional<T>& to, const BlendingContext& context) -> std::optional<T>
{
    if (from && to)
        return blend(*from, *to, context);
    if (from)
        return blend(*from, identityTransformFunction(*from), context);
    if (to)
        return blend(identityTransformFunction(*to), *to, context);
    return std::nullopt;
}

// Conformance for `PrimitiveTransformType`. Used to adapt platform TransformFunction for `TransformListSharedPrimitivesPrefix`.
template<> struct PrimitiveTransformType<TransformFunction> {
    TransformFunctionType operator()(const TransformFunction&);
};

} // namespace WebCore
