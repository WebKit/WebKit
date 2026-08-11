/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleInsetFunction.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// All <basic-shape-rect> variants (inset(), rect(), xywh()) canonicalize to
// InsetFunction during style building. See StyleRectFunction.h and
// StyleXywhFunction.h for the conversions.
using BasicShapeRect = InsetFunction;

// <'object-view-box'> = none | <basic-shape-rect>
// https://drafts.csswg.org/css-images-5/#the-object-view-box
struct ObjectViewBox {
    using Value = Variant<CSS::Keyword::None, BasicShapeRect>;

    ObjectViewBox(CSS::Keyword::None none)
        : m_value(none) { }
    ObjectViewBox(BasicShapeRect&& rect)
        : m_value(WTF::move(rect)) { }

    bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    bool isRect() const { return WTF::holdsAlternative<BasicShapeRect>(m_value); }

    std::optional<BasicShapeRect> tryRect() const
    {
        if (auto* rect = std::get_if<BasicShapeRect>(&m_value))
            return *rect;
        return std::nullopt;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const ObjectViewBox&) const = default;

private:
    Value m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<ObjectViewBox> {
    auto operator()(BuilderState&, const CSSValue&) -> ObjectViewBox;
};

// MARK: - CSSValue creation

template<> struct CSSValueCreation<ObjectViewBox> {
    Ref<CSSValue> operator()(CSSValuePool&, const ComputedStyle&, const ObjectViewBox&);
};

// MARK: - Serialization

template<> struct Serialize<ObjectViewBox> {
    void operator()(StringBuilder&, const CSS::SerializationContext&, const ComputedStyle&, const ObjectViewBox&);
};

// MARK: - Blending

template<> struct Blending<ObjectViewBox> {
    auto canBlend(const ObjectViewBox& from, const ObjectViewBox& to) -> bool;
    auto blend(const ObjectViewBox& from, const ObjectViewBox& to, const BlendingContext&) -> ObjectViewBox;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ObjectViewBox)
