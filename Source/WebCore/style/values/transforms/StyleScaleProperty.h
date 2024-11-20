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

#include "CSSScaleProperty.h"
#include "StyleTransformFunctions.h"

namespace WebCore {

struct TransformList;

namespace Style {

// MARK: - ScaleProperty

// https://drafts.csswg.org/css-transforms-2/#propdef-scale
// <'scale'> = none | [ <number> | <percentage> ]{1,3}
struct ScaleProperty {
    using Platform = WebCore::TransformList;

    std::variant<None, Scale, Scale3D> value;

    bool isNone() const { return std::holds_alternative<None>(value); }

    bool isAffectedByTransformOrigin() const;
    bool isRepresentableIn2D() const;
    bool has3DOperation() const;

    void applyTransform(TransformationMatrix&, const FloatSize& referenceBox) const;

    Platform toPlatform(const FloatSize& referenceBox) const;

    bool operator==(const ScaleProperty&) const = default;
};
DEFINE_STYLE_TYPE_WRAPPER(ScaleProperty)
DEFINE_CSS_STYLE_MAPPING(CSS::ScaleProperty, ScaleProperty)

template<> struct Blending<ScaleProperty> {
    constexpr auto canBlend(const ScaleProperty&, const ScaleProperty&) -> bool { return true; }
    auto blend(const ScaleProperty&, const ScaleProperty&, const BlendingContext&) -> ScaleProperty;
};

// MARK: Text Stream

WTF::TextStream& operator<<(WTF::TextStream&, const ScaleProperty&);

inline bool ScaleProperty::isAffectedByTransformOrigin() const
{
    return Style::isAffectedByTransformOrigin(value);
}

inline bool ScaleProperty::isRepresentableIn2D() const
{
    return Style::isRepresentableIn2D(value);
}

inline bool ScaleProperty::has3DOperation() const
{
    return WTF::switchOn(value, []<typename T>(const T&) { return Is3D<T>; });
}

inline void ScaleProperty::applyTransform(TransformationMatrix& transform, const FloatSize& referenceBox) const
{
    Style::applyTransform(value, transform, referenceBox);
}

} // namespace Style
} // namespace WebCore
