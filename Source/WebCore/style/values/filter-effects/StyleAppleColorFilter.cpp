/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleAppleColorFilter.h"

#include "CSSAppleColorFilterValue.h"
#include "ColorConversion.h"
#include "FilterOperations.h"
#include "StyleAppleInvertLightnessFunction.h"
#include "StyleBrightnessFunction.h"
#include "StyleBuilderChecking.h"
#include "StyleContrastFunction.h"
#include "StyleFilterInterpolation.h"
#include "StyleGrayscaleFunction.h"
#include "StyleHueRotateFunction.h"
#include "StyleInvertFunction.h"
#include "StyleOpacityFunction.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include "StyleSaturateFunction.h"
#include "StyleSepiaFunction.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

const AppleColorFilter& AppleColorFilter::none()
{
    static NeverDestroyed<AppleColorFilter> value { CSS::Keyword::None { } };
    return value.get();
}

bool AppleColorFilter::transformColor(WebCore::Color& color) const
{
    if (isNone() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& value : *this) {
        Ref operation = value.value;
        if (!operation->transformColor(sRGBAColor))
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

bool AppleColorFilter::inverseTransformColor(WebCore::Color& color) const
{
    if (isNone() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& value : *this) {
        Ref operation = value.value;
        if (!operation->inverseTransformColor(sRGBAColor))
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

// MARK: - Conversions

// MARK: (AppleColorFilterValue)

auto ToCSS<AppleColorFilterValue>::operator()(const AppleColorFilterValue& value, const RenderStyle& style) -> CSS::AppleColorFilterValue
{
    Ref op = value.value;
    switch (op->type()) {
    case FilterOperation::Type::AppleInvertLightness:
        return CSS::AppleColorFilterValue { CSS::AppleInvertLightnessFunction { toCSSAppleInvertLightness(downcast<InvertLightnessFilterOperation>(op), style) } };
    case FilterOperation::Type::Grayscale:
        return CSS::AppleColorFilterValue { CSS::GrayscaleFunction { toCSSGrayscale(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::Sepia:
        return CSS::AppleColorFilterValue { CSS::SepiaFunction { toCSSSepia(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::Saturate:
        return CSS::AppleColorFilterValue { CSS::SaturateFunction { toCSSSaturate(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::HueRotate:
        return CSS::AppleColorFilterValue { CSS::HueRotateFunction { toCSSHueRotate(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::Invert:
        return CSS::AppleColorFilterValue { CSS::InvertFunction { toCSSInvert(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    case FilterOperation::Type::Opacity:
        return CSS::AppleColorFilterValue { CSS::OpacityFunction { toCSSOpacity(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    case FilterOperation::Type::Brightness:
        return CSS::AppleColorFilterValue { CSS::BrightnessFunction { toCSSBrightness(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    case FilterOperation::Type::Contrast:
        return CSS::AppleColorFilterValue { CSS::ContrastFunction { toCSSContrast(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto ToStyle<CSS::AppleColorFilterValue>::operator()(const CSS::AppleColorFilterValue& value, const BuilderState& state) -> AppleColorFilterValue
{
    return WTF::switchOn(value,
        [&](const auto& function) -> AppleColorFilterValue {
            return AppleColorFilterValue { createFilterOperation(function, state) };
        }
    );
}

// MARK: (AppleColorFilter)

auto ToCSS<AppleColorFilter>::operator()(const AppleColorFilter& value, const RenderStyle& style) -> CSS::AppleColorFilter
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::None keyword) -> CSS::AppleColorFilter {
            return keyword;
        },
        [&](const AppleColorFilterValueList& list) -> CSS::AppleColorFilter {
            return CSS::AppleColorFilterValueList::map(list, [&](const AppleColorFilterValue& value) {
                return toCSS(value, style);
            });
        }
    );
}

auto ToStyle<CSS::AppleColorFilter>::operator()(const CSS::AppleColorFilter& value, const BuilderState& state) -> AppleColorFilter
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::None keyword) -> AppleColorFilter {
            return keyword;
        },
        [&](const CSS::AppleColorFilterValueList& list) -> AppleColorFilter {
            return AppleColorFilterValueList::map(list, [&](const CSS::AppleColorFilterValue& value) {
                return toStyle(value, state);
            });
        }
    );
}

auto CSSValueConversion<AppleColorFilter>::operator()(BuilderState& state, const CSSValue& value) -> AppleColorFilter
{
    if (value.valueID() == CSSValueNone)
        return CSS::Keyword::None { };

    RefPtr filter = requiredDowncast<CSSAppleColorFilterValue>(state, value);
    if (!filter)
        return CSS::Keyword::None { };

    return toStyle(filter->filter(), state);
}

Ref<CSSValue> CSSValueCreation<AppleColorFilter>::operator()(CSSValuePool&, const RenderStyle& style, const AppleColorFilter& value)
{
    return CSSAppleColorFilterValue::create(toCSS(value, style));
}

// MARK: - Serialization

void Serialize<AppleColorFilter>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const AppleColorFilter& value)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style));
}

// MARK: - Blending

auto Blending<AppleColorFilter>::canBlend(const AppleColorFilter& from, const AppleColorFilter& to, CompositeOperation compositeOperation) -> bool
{
    return canBlendFilterLists(from.m_value, to.m_value, compositeOperation);
}

auto Blending<AppleColorFilter>::blend(const AppleColorFilter& from, const AppleColorFilter& to, const BlendingContext& context) -> AppleColorFilter
{
    auto blendedFilterList = blendFilterLists(from.m_value, to.m_value, context);

    if (blendedFilterList.isEmpty())
        return CSS::Keyword::None { };

    return AppleColorFilter { WTFMove(blendedFilterList) };
}

// MARK: - Platform

auto ToPlatform<AppleColorFilterValue>::operator()(const AppleColorFilterValue& value) -> Ref<FilterOperation>
{
    return value.value;
}

auto ToPlatform<AppleColorFilter>::operator()(const AppleColorFilter& value) -> FilterOperations
{
    return FilterOperations { WTF::map(value, [](auto& filterValue) {
        return toPlatform(filterValue);
    }) };
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const AppleColorFilterValue& value)
{
    Ref platform = value.value;
    return ts << platform;
}

} // namespace Style
} // namespace WebCore
