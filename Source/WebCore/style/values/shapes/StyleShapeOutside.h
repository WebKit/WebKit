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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleBasicShape.h>
#include <WebCore/StyleImageWrapper.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'shape-outside'> = none | [ <basic-shape> || <shape-box> ] | <image>
// https://www.w3.org/TR/css-shapes/#propdef-shape-outside
struct ShapeOutside {
    using Shape = BasicShape;
    using ShapeBox = CSSBoxType;

    struct ShapeAndShapeBox {
        Shape shape;
        ShapeBox box;

        bool operator==(const ShapeAndShapeBox&) const = default;
    };
    struct Image {
        ImageWrapper image;

        bool isValid() const;

        bool operator==(const Image&) const = default;
    };

    ShapeOutside(CSS::Keyword::None) { }
    ShapeOutside(Shape&& value) : m_value { Value::create(WTFMove(value)) } { }
    ShapeOutside(ShapeBox&& value) : m_value { Value::create(WTFMove(value)) } { }
    ShapeOutside(ShapeAndShapeBox&& value) : m_value { Value::create(WTFMove(value)) } { }
    ShapeOutside(Image&& value) : m_value { Value::create(WTFMove(value)) } { }

    bool isNone() const { return !m_value; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });

        return WTF::switchOn(m_value->value,
            [&](const Shape& shape) {
                return visitor(shape);
            },
            [&](const ShapeBox& box) {
                return visitor(box);
            },
            [&](const ShapeAndShapeBox& shapeAndShapeBox) {
                return visitor(shapeAndShapeBox);
            },
            [&](const Image& image) {
                return visitor(image);
            }
        );
    }

    const BasicShape* shape() const { RefPtr value = m_value; return value ? value->shape() : nullptr; }
    CSSBoxType effectiveCSSBox() const { RefPtr value = m_value; return value ? value->effectiveCSSBox() : CSSBoxType::BoxMissing; }
    RefPtr<StyleImage> image() const { RefPtr value = m_value; return value ? value->image() : nullptr; }

    bool operator==(const ShapeOutside& other) const
    {
        return arePointingToEqualData(m_value, other.m_value);
    }

private:
    friend struct Blending<ShapeOutside>;

    class Value : public RefCounted<Value> {
    public:
        using Kind = Variant<Shape, ShapeBox, ShapeAndShapeBox, Image>;

        static Ref<Value> create(Kind&& value)
        {
            return adoptRef(*new Value(WTFMove(value)));
        }

        explicit Value(Kind&& value)
            : value { WTFMove(value) }
        {
        }

        inline const BasicShape* shape() const;
        inline CSSBoxType effectiveCSSBox() const;
        inline RefPtr<StyleImage> image() const;

        bool operator==(const Value& other) const
        {
            return value == other.value;
        }

        Kind value;
    };

    RefPtr<Value> m_value { };
};

DEFINE_TYPE_WRAPPER_GET(ShapeOutside::Image, image);

template<size_t I> const auto& get(const ShapeOutside::ShapeAndShapeBox& value)
{
    if constexpr (!I)
        return value.shape;
    else if constexpr (I == 1)
        return value.box;
}

inline const BasicShape* ShapeOutside::Value::shape() const
{
    return WTF::switchOn(value,
        [](const ShapeOutside::Shape& shape) -> const BasicShape* { return &shape; },
        [](const ShapeOutside::ShapeBox&) -> const BasicShape* { return nullptr; },
        [](const ShapeOutside::ShapeAndShapeBox& shapeAndShapeBox) -> const BasicShape* { return &shapeAndShapeBox.shape; },
        [](const ShapeOutside::Image&) -> const BasicShape* { return nullptr; }
    );
}

inline RefPtr<StyleImage> ShapeOutside::Value::image() const
{
    return WTF::switchOn(value,
        [](const ShapeOutside::Shape&) -> RefPtr<StyleImage> { return nullptr; },
        [](const ShapeOutside::ShapeBox&) -> RefPtr<StyleImage> { return nullptr; },
        [](const ShapeOutside::ShapeAndShapeBox&) -> RefPtr<StyleImage> { return nullptr; },
        [](const ShapeOutside::Image& image) -> RefPtr<StyleImage> { return image.image.value.ptr(); }
    );
}

CSSBoxType ShapeOutside::Value::effectiveCSSBox() const
{
    return WTF::switchOn(value,
        [](const ShapeOutside::Shape&) { return CSSBoxType::MarginBox; },
        [](const ShapeOutside::ShapeBox& box) { return box == CSSBoxType::BoxMissing ? CSSBoxType::MarginBox : box; },
        [](const ShapeOutside::ShapeAndShapeBox& shapeAndShapeBox) { return shapeAndShapeBox.box == CSSBoxType::BoxMissing ? CSSBoxType::MarginBox : shapeAndShapeBox.box; },
        [](const ShapeOutside::Image&) { return CSSBoxType::ContentBox; }
    );
}

// MARK: - Conversion

template<> struct CSSValueConversion<ShapeOutside> { auto operator()(BuilderState&, const CSSValue&) -> ShapeOutside; };

// MARK: - Blending

template<> struct Blending<ShapeOutside> {
    auto canBlend(const ShapeOutside&, const ShapeOutside&) -> bool;
    auto blend(const ShapeOutside&, const ShapeOutside&, const BlendingContext&) -> ShapeOutside;
};

} // namespace Style
} // namespace WebCore


DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::ShapeOutside::ShapeAndShapeBox, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::ShapeOutside::Image);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ShapeOutside)
