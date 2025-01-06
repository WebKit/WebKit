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
#include "StyleFilterProperty.h"

#include "CSSFilterProperty.h"
#include "Document.h"
#include "FilterOperations.h"
#include "StyleBlurFunction.h"
#include "StyleBrightnessFunction.h"
#include "StyleContrastFunction.h"
#include "StyleDropShadowFunction.h"
#include "StyleGrayscaleFunction.h"
#include "StyleHueRotateFunction.h"
#include "StyleInvertFunction.h"
#include "StyleOpacityFunction.h"
#include "StyleSaturateFunction.h"
#include "StyleSepiaFunction.h"

namespace WebCore {
namespace Style {

CSS::FilterProperty toCSSFilterProperty(const FilterOperations& filterOperations, const RenderStyle& style)
{
    if (filterOperations.isEmpty())
        return CSS::FilterProperty { CSS::Keyword::None { } };

    CSS::FilterProperty::List list;
    list.value.reserveInitialCapacity(filterOperations.size());

    for (auto& op : filterOperations) {
        switch (op->type()) {
        case FilterOperation::Type::Reference:
            list.value.append(CSS::FilterReference { downcast<ReferenceFilterOperation>(op)->url() });
            break;
        case FilterOperation::Type::Grayscale:
            list.value.append(CSS::GrayscaleFunction { toCSSGrayscale(downcast<BasicColorMatrixFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::Sepia:
            list.value.append(CSS::SepiaFunction { toCSSSepia(downcast<BasicColorMatrixFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::Saturate:
            list.value.append(CSS::SaturateFunction { toCSSSaturate(downcast<BasicColorMatrixFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::HueRotate:
            list.value.append(CSS::HueRotateFunction { toCSSHueRotate(downcast<BasicColorMatrixFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::Invert:
            list.value.append(CSS::InvertFunction { toCSSInvert(downcast<BasicComponentTransferFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::Opacity:
            list.value.append(CSS::OpacityFunction { toCSSOpacity(downcast<BasicComponentTransferFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::Brightness:
            list.value.append(CSS::BrightnessFunction { toCSSBrightness(downcast<BasicComponentTransferFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::Contrast:
            list.value.append(CSS::ContrastFunction { toCSSContrast(downcast<BasicComponentTransferFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::Blur:
            list.value.append(CSS::BlurFunction { toCSSBlur(downcast<BlurFilterOperation>(op), style) });
            break;
        case FilterOperation::Type::DropShadow:
            list.value.append(CSS::DropShadowFunction { toCSSDropShadow(downcast<DropShadowFilterOperation>(op), style) });
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return CSS::FilterProperty { WTFMove(list) };
}

static Ref<FilterOperation> createFilterFunctionReference(const String& filterURL, const Document& document)
{
    auto fragment = document.completeURL(filterURL).fragmentIdentifier().toAtomString();
    return ReferenceFilterOperation::create(filterURL, WTFMove(fragment));
}

template<typename T> static Ref<FilterOperation> createFilterPropertyOperation(const T& value, const Document& document, RenderStyle& style, const CSSToLengthConversionData& conversionData)
{
    return WTF::switchOn(value,
        [&](const CSS::FilterReference& reference) {
            return createFilterFunctionReference(reference.url, document);
        },
        [&](const auto& function) {
            return createFilterOperation(function, document, style, conversionData);
        }
    );
}

FilterOperations createFilterOperations(const CSS::FilterProperty& value, const Document& document, RenderStyle& style, const CSSToLengthConversionData& conversionData)
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::None) {
            return FilterOperations { };
        },
        [&](const CSS::FilterProperty::List& list) {
            return FilterOperations { WTF::map(list, [&](const auto& value) {
                return createFilterPropertyOperation(value, document, style, conversionData);
            }) };
        }
    );
}

} // namespace Style
} // namespace WebCore
