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

#include "config.h"
#include "StyleShapeOutside.h"

#include "AnimationUtilities.h"
#include "CSSBasicShapeValue.h"
#include "CSSKeywordValue.h"
#include "CSSValueList.h"
#include "CachedImage.h"
#include "StyleBuilderChecking.h"
#include "StyleKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

// MARK: - ShapeOutside

ShapeOutside::ShapeOutside(Shape&& value)
    : m_value { Value::create(WTF::move(value)) }
{
}

ShapeOutside::ShapeOutside(ShapeBox&& value)
    : m_value { Value::create(WTF::move(value)) }
{
}

ShapeOutside::ShapeOutside(ShapeAndShapeBox&& value)
    : m_value { Value::create(WTF::move(value)) }
{
}

ShapeOutside::ShapeOutside(Image&& value)
    : m_value { Value::create(WTF::move(value)) }
{
}

// MARK: - ShapeOutside::Value

Ref<ShapeOutside::Value> ShapeOutside::Value::create(Kind&& value)
{
    return adoptRef(*new Value(WTF::move(value)));
}

ShapeOutside::Value::Value(Kind&& value)
    : value { WTF::move(value) }
{
}

ShapeOutside::Value::~Value() = default;

bool ShapeOutside::Value::operator==(const Value& other) const
{
    return value == other.value;
}

// MARK: - ShapeOutside::ShapeAndShapeBox

ShapeOutside::ShapeAndShapeBox::ShapeAndShapeBox(Shape&& shape, ShapeBox box)
    : shape(WTF::move(shape))
    , box(box)
{
}

ShapeOutside::ShapeAndShapeBox::ShapeAndShapeBox(ShapeAndShapeBox&&) = default;
ShapeOutside::ShapeAndShapeBox::ShapeAndShapeBox(const ShapeAndShapeBox&) = default;
ShapeOutside::ShapeAndShapeBox& ShapeOutside::ShapeAndShapeBox::operator=(ShapeAndShapeBox&&) = default;
ShapeOutside::ShapeAndShapeBox& ShapeOutside::ShapeAndShapeBox::operator=(const ShapeAndShapeBox&) = default;
ShapeOutside::ShapeAndShapeBox::~ShapeAndShapeBox() = default;
bool ShapeOutside::ShapeAndShapeBox::operator==(const ShapeAndShapeBox&) const = default;

bool ShapeOutside::Image::isValid() const
{
    Ref styleImage = image.value;
    if (styleImage->hasCachedImage()) {
        RefPtr cachedImage = styleImage->cachedImage();
        return cachedImage && cachedImage->hasImage();
    }
    return styleImage->isGeneratedImage();
}

// MARK: - Conversion

auto CSSValueConversion<ShapeOutside>::operator()(BuilderState& state, const CSSValue& value) -> ShapeOutside
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    if (value.isImage())
        return ShapeOutside::Image { ImageWrapper { state.createStyleImage(value).releaseNonNull() } };

    std::optional<BasicShape> shape;
    auto referenceBox = CSSBoxType::BoxMissing;

    auto processSingleValue = [&](const CSSValue& currentValue) {
        if (RefPtr shapeValue = dynamicDowncast<CSSBasicShapeValue>(currentValue))
            shape = toStyle(shapeValue->shape(), state, 1.0f);
        else
            referenceBox = toStyleFromCSSValue<CSSBoxType>(state, currentValue);
    };

    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        for (Ref currentValue : *list)
            processSingleValue(currentValue);
    } else
        processSingleValue(value);

    if (shape) {
        // FIXME: Add support for `path()` and `shape()` functions in `shape-outside`.
        if (WTF::holdsAlternative<PathFunction>(*shape) || WTF::holdsAlternative<ShapeFunction>(*shape)) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }

        if (referenceBox != CSSBoxType::BoxMissing)
            return ShapeOutside::ShapeAndShapeBox { WTF::move(*shape), referenceBox };
        return ShapeOutside::Shape { WTF::move(*shape) };
    }

    if (referenceBox != CSSBoxType::BoxMissing)
        return referenceBox;

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::None { };
}

// MARK: - Blending

auto Blending<ShapeOutside>::canBlend(const ShapeOutside& a, const ShapeOutside& b) -> bool
{
    RefPtr aValue = a.m_value;
    RefPtr bValue = b.m_value;

    if (!aValue || !bValue)
        return false;

    return WTF::visit(WTF::makeVisitor(
        [](const ShapeOutside::ShapeAndShapeBox& a, const ShapeOutside::ShapeAndShapeBox& b) {
            return Style::canBlend(a.shape, b.shape) && a.box == b.box;
        },
        [](const ShapeOutside::Shape& a, const ShapeOutside::Shape& b) {
            return Style::canBlend(a, b);
        },
        [](const auto&, const auto&) {
            return false;
        }
    ), aValue->value, bValue->value);
}

auto Blending<ShapeOutside>::blend(const ShapeOutside& a, const ShapeOutside& b, const BlendingContext& context) -> ShapeOutside
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));

    RefPtr aValue = a.m_value;
    RefPtr bValue = b.m_value;

    return WTF::visit(WTF::makeVisitor(
        [&](const ShapeOutside::ShapeAndShapeBox& a, const ShapeOutside::ShapeAndShapeBox& b) -> ShapeOutside {
            return ShapeOutside::ShapeAndShapeBox {
                Style::blend(a.shape, b.shape, context),
                a.box
            };
        },
        [&](const ShapeOutside::Shape& a, const ShapeOutside::Shape& b) -> ShapeOutside {
            return Style::blend(a, b, context);
        },
        [](const auto&, const auto&) -> ShapeOutside {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), aValue->value, bValue->value);
}

} // namespace Style
} // namespace WebCore
