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

#include "CSSTransformList.h"
#include "StyleTransformFunctions.h"
#include "TransformList.h"

namespace WebCore {
namespace Style {

// https://drafts.csswg.org/css-transforms-1/#typedef-transform-list
// <transform-list> = <transform-function>+
struct TransformList {
    using List = SpaceSeparatedVariantList<TransformFunction>;
    using Platform = WebCore::TransformList;

    List value;

    // Special value used by `Transform` to indicate `None`.
    explicit TransformList(None)
        : value { }
    {
    }

    TransformList(std::initializer_list<TransformFunction> initializerList)
        : value { }
    {
        for (const auto& element : initializerList)
            value.value.append(element);
    }

    TransformList(List list)
        : value { WTFMove(list) }
    {
    }

    // NOTE: This should only be true when `TransformList` is being used to
    // by `Transform`, and `Transform` is trying to represent the value `none`.
    //
    // FIXME: Find a typed method for implementing this optimization. Perhaps
    // creating a new "non-empty vector" type and using a custom Markable trait
    // to utilize the unused "empty" state.
    bool isEmpty() const { return value.isEmpty(); }

    auto begin() const { return value.begin(); }
    auto end() const { return value.end(); }

    // `forEach` provides a slightly more efficient looping construct for cases where early exit is not needed.
    template<typename...F> void forEach(F&&... f) const { value.forEach(std::forward<F>(f)...); }

    template<typename... Ts> bool hasTransformOfType() const;

    bool allFunctionsAreRepresentableIn2D() const;
    bool hasFunctionAffectedByTransformOrigin() const;

    bool hasFunctionDependentOnWidth() const;
    bool hasFunctionDependentOnHeight() const;

    // Return true if any of the operation types are 3D operation types (even if the
    // values describe affine transforms)
    bool has3DFunction() const;

    bool isInvertible(const LayoutSize& referenceBox) const;
    bool containsNonInvertibleMatrix(const LayoutSize& referenceBox) const;
    void applyTransform(TransformationMatrix&, const FloatSize& referenceBox, unsigned start = 0) const;

    Platform toPlatform(const FloatSize& referenceBox) const;

    bool operator==(const TransformList&) const = default;
};
DEFINE_STYLE_TYPE_WRAPPER(TransformList)

// MARK: - Blending

bool shouldFallBackToDiscreteAnimation(const TransformList&, const TransformList&, const LayoutSize& referenceBox);
TransformList blend(const TransformList&, const TransformList&, const BlendingContext&, const LayoutSize& referenceBox, std::optional<unsigned> prefixLength = std::nullopt);

// MARK: - Conversion

template<> struct ToCSS<TransformList> {
    auto operator()(const TransformList&, const RenderStyle&) -> CSS::TransformList;
};
template<> struct ToStyle<CSS::TransformList> {
    auto operator()(const CSS::TransformList&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> TransformList;
    auto operator()(const CSS::TransformList&, const BuilderState&, const CSSCalcSymbolTable&) -> TransformList;
};

// MARK: - Text Stream

WTF::TextStream& operator<<(WTF::TextStream&, const TransformList&);

// MARK: - Inlines

template<typename... Ts> bool TransformList::hasTransformOfType() const
{
    return std::ranges::any_of(*this, [](const auto& function) {
        return (function.template holds_alternative<Ts>() || ...);
    });
}

inline bool TransformList::allFunctionsAreRepresentableIn2D() const
{
    return std::ranges::all_of(*this, [](const auto& function) {
        return Style::isRepresentableIn2D(function);
    });
}

inline bool TransformList::hasFunctionAffectedByTransformOrigin() const
{
    return std::ranges::any_of(*this, [](const auto& function) {
        return Style::isAffectedByTransformOrigin(function);
    });
}

inline bool TransformList::hasFunctionDependentOnWidth() const
{
    return std::ranges::any_of(*this, [](const auto& function) {
        return Style::isWidthDependent(function);
    });
}

inline bool TransformList::hasFunctionDependentOnHeight() const
{
    return std::ranges::any_of(*this, [](const auto& function) {
        return Style::isHeightDependent(function);
    });
}

inline bool TransformList::has3DFunction() const
{
    return std::ranges::any_of(*this, [](const auto& function) {
        return Style::is3D(function);
    });
}

inline bool TransformList::isInvertible(const LayoutSize& referenceBox) const
{
    TransformationMatrix transformationMatrix;
    applyTransform(transformationMatrix, referenceBox);
    return transformationMatrix.isInvertible();
}

inline bool TransformList::containsNonInvertibleMatrix(const LayoutSize& referenceBox) const
{
    return hasTransformOfType<Style::MatrixFunction, Style::Matrix3DFunction>() && !isInvertible(referenceBox);
}

inline void TransformList::applyTransform(TransformationMatrix& matrix, const FloatSize& referenceBox, unsigned start) const
{
    forEach([&](const auto& function) {
        if (start != 0) {
            --start;
            return;
        }
        Style::applyTransform(function, matrix, referenceBox);
    });
}

} // namespace Style
} // namespace WebCore
