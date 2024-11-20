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

#include "CSSTransformProperty.h"
#include "StyleTransformList.h"

namespace WebCore {
namespace Style {

// https://drafts.csswg.org/css-transforms-1/#transform-property
// <'transform'> = none | <transform-list>
struct TransformProperty {
    using Platform = WebCore::TransformList;

    // NOTE: `none` is represented by an empty `TransformList`.
    TransformList list;

    // Special value used by `TransformProperty` to indicate `None`.
    TransformProperty(None none)
        : list { none }
    {
    }

    TransformProperty(std::initializer_list<TransformFunction> initializerList)
        : list { initializerList }
    {
    }

    TransformProperty(TransformList list)
        : list { WTFMove(list) }
    {
    }

    bool isNone() const { return list.isEmpty(); }

    auto begin() const { return list.begin(); }
    auto end() const { return list.end(); }

    template<typename... Ts> bool hasTransformOfType() const { return list.hasTransformOfType<Ts...>(); }

    bool isAffectedByTransformOrigin() const { return list.hasFunctionAffectedByTransformOrigin(); }
    bool isRepresentableIn2D() const { return list.allFunctionsAreRepresentableIn2D(); }

    bool isWidthDependent() const { return list.hasFunctionDependentOnWidth(); }
    bool isHeightDependent() const { return list.hasFunctionDependentOnHeight(); }

    // Return true if any of the operation types are 3D operation types (even if the
    // values describe affine transforms)
    bool has3DOperation() const { return list.has3DFunction(); }

    bool isInvertible(const LayoutSize& referenceBox) const { return list.isInvertible(referenceBox); }
    bool containsNonInvertibleMatrix(const LayoutSize& referenceBox) const { return list.containsNonInvertibleMatrix(referenceBox); }
    void applyTransform(TransformationMatrix& transform, const FloatSize& referenceBox, unsigned start = 0) const { list.applyTransform(transform, referenceBox, start); }

    Platform toPlatform(const FloatSize& referenceBox) const { return list.toPlatform(referenceBox); }

    bool operator==(const TransformProperty&) const = default;
};

template<size_t I> const auto& get(const TransformProperty& value)
{
    if constexpr (!I)
        return value.list;
}

DEFINE_CSS_STYLE_MAPPING(CSS::TransformProperty, TransformProperty)

// MARK: - Blending

inline bool shouldFallBackToDiscreteAnimation(const TransformProperty& a, const TransformProperty& b, const LayoutSize& referenceBox)
{
    return shouldFallBackToDiscreteAnimation(a.list, b.list, referenceBox);
}

inline TransformProperty blend(const TransformProperty& a, const TransformProperty& b, const BlendingContext& context, const LayoutSize& referenceBox, std::optional<unsigned> prefixLength = std::nullopt)
{
    return { blend(a.list, b.list, context, referenceBox, prefixLength) };
}

// MARK: - Text Stream

WTF::TextStream& operator<<(WTF::TextStream&, const TransformProperty&);

} // namespace Style
} // namespace WebCore

STYLE_TUPLE_LIKE_CONFORMANCE(TransformProperty, 1)
