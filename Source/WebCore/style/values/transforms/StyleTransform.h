/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleTransformList.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

class TransformOperations;

namespace Style {

// <'transform'> = none | <transform-list>
// https://drafts.csswg.org/css-transforms-1/#propdef-transform
struct Transform : ListOrNone<TransformList> {
    friend struct Blending<Transform>;

    using ListOrNone<TransformList>::ListOrNone;

    // Convenience constructors for creating a `transform` directly from transform functions.
    Transform(std::initializer_list<TransformFunction>);
    Transform(TransformFunction&&);

    template<TransformFunctionType operationType>
    bool hasTransformOfType() const;

    void apply(TransformationMatrix&, const FloatSize&, unsigned start = 0) const;

    // Return true if any of the operation types are 3D operation types (even if the
    // values describe affine transforms)
    bool has3DOperation() const;
    bool isRepresentableIn2D() const;
    bool affectedByTransformOrigin() const;
    bool containsNonInvertibleMatrix(const LayoutSize&) const;

    TransformFunctionSizeDependencies computeSizeDependencies() const;
};

inline Transform::Transform(std::initializer_list<TransformFunction> transformFunctions)
    : Transform { TransformList { transformFunctions } }
{
}

inline Transform::Transform(TransformFunction&& transformFunction)
    : Transform { TransformList { WTFMove(transformFunction) } }
{
}

template<TransformFunctionType operationType>
bool Transform::hasTransformOfType() const
{
    return m_value.hasTransformOfType<operationType>();
}

inline void Transform::apply(TransformationMatrix& matrix, const FloatSize& size, unsigned start) const
{
    m_value.apply(matrix, size, start);
}

inline bool Transform::has3DOperation() const
{
    return m_value.has3DOperation();
}

inline bool Transform::isRepresentableIn2D() const
{
    return m_value.isRepresentableIn2D();
}

inline bool Transform::affectedByTransformOrigin() const
{
    return m_value.affectedByTransformOrigin();
}

inline bool Transform::containsNonInvertibleMatrix(const LayoutSize& size) const
{
    return m_value.containsNonInvertibleMatrix(size);
}

inline TransformFunctionSizeDependencies Transform::computeSizeDependencies() const
{
    return m_value.computeSizeDependencies();
}

// MARK: - Conversion

template<> struct CSSValueConversion<Transform> { auto operator()(BuilderState&, const CSSValue&) -> Transform; };
template<> struct CSSValueCreation<Transform> { auto operator()(CSSValuePool&, const RenderStyle&, const Transform&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<Transform> {
    auto canBlend(const Transform&, const Transform&, CompositeOperation) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const Transform&, const Transform&) -> bool { return true; }
    auto blend(const Transform&, const Transform&, const Interpolation::Context&) -> Transform;
};

// MARK: - Platform

template<> struct ToPlatform<Transform> { auto operator()(const Transform&, const FloatSize&) -> TransformOperations; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Transform)
