/*
 * Copyright (C) 2014 Yoav Weiss <yoav@yoav.ws>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SourceSizeList.h"

#include "CSSParser.h"
#include "CSSToLengthConversionData.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "RenderStyle.h"
#include "RenderView.h"

namespace WebCore {

static bool match(const MediaQueryExpression& expression, const RenderStyle& style, Document& document)
{
    return expression.mediaFeature().isEmpty() || MediaQueryEvaluator { "screen", document, &style }.evaluate(expression);
}

static float defaultLength(const RenderStyle& style, RenderView& renderer)
{
    return clampTo<float>(CSSPrimitiveValue::computeNonCalcLengthDouble({ &style, &style, &renderer }, CSSPrimitiveValue::CSS_VW, 100.0));
}

static float computeLength(CSSValue& value, const RenderStyle& style, RenderView& renderer)
{
    CSSToLengthConversionData conversionData(&style, &style, &renderer);
    if (is<CSSPrimitiveValue>(value)) {
        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (!primitiveValue.isLength())
            return defaultLength(style, renderer);

        // Because we evaluate "sizes" at parse time (before style has been resolved), the font metrics used for these specific units
        // are not available. The font selector's internal consistency isn't guaranteed just yet, so we can just temporarily clear
        // the pointer to it for the duration of the unit evaluation. This is acceptible because the style always comes from the
        // RenderView, which has its font information hardcoded in resolveForDocument() to be -webkit-standard, whose operations
        // don't require a font selector.
        if (!primitiveValue.isCalculated() && (primitiveValue.primitiveType() == CSSPrimitiveValue::CSS_EXS || primitiveValue.primitiveType() == CSSPrimitiveValue::CSS_CHS)) {
            RefPtr<FontSelector> fontSelector = style.fontCascade().fontSelector();
            style.fontCascade().update(nullptr);
            float result = primitiveValue.computeLength<float>(conversionData);
            style.fontCascade().update(fontSelector.get());
            return result;
        }

        return primitiveValue.computeLength<float>(conversionData);
    }
    if (is<CSSCalcValue>(value))
        return downcast<CSSCalcValue>(value).computeLengthPx(conversionData);
    return defaultLength(style, renderer);
}

float parseSizesAttribute(Document& document, StringView sizesAttribute)
{
    auto* renderer = document.renderView();
    if (!renderer)
        return 0;
    auto& style = renderer->style();
    for (auto& sourceSize : CSSParser(CSSStrictMode).parseSizesAttribute(sizesAttribute)) {
        if (match(sourceSize.expression, style, document))
            return computeLength(sourceSize.length.get(), style, *renderer);
    }
    return defaultLength(style, *renderer);
}

} // namespace WebCore
