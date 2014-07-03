/*
 * Copyright (C) 2014 Yoav Weiss <yoav@yoav.ws>
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
#include "Document.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "MediaQueryEvaluator.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "RenderView.h"

namespace WebCore {

#if ENABLE(PICTURE_SIZES)
bool SourceSize::match(RenderStyle& style, Frame* frame)
{
    if (m_mediaExp->mediaFeature().isEmpty())
        return true;
    auto expList = std::make_unique<Vector<std::unique_ptr<MediaQueryExp>>>();
    expList->append(m_mediaExp.release());

    RefPtr<MediaQuerySet> mediaQuerySet = MediaQuerySet::create();
    mediaQuerySet->addMediaQuery(std::make_unique<MediaQuery>(MediaQuery::None, "all", WTF::move(expList)));

    MediaQueryEvaluator mediaQueryEvaluator("screen", frame, &style);
    return mediaQueryEvaluator.eval(mediaQuerySet.get());
}

static unsigned computeLength(CSSParserValue& value, RenderStyle& style, RenderView* view)
{
    CSSToLengthConversionData conversionData(&style, &style, view);
    if (CSSParser::isCalculation(&value)) {
        CSSParserValueList* args = value.function->args.get();
        if (args && args->size()) {
            RefPtr<CSSCalcValue> calcValue = CSSCalcValue::create(value.function->name, *args, CalculationRangeNonNegative);
            Length length(calcValue->createCalculationValue(conversionData));
            RefPtr<CSSPrimitiveValue> primitiveValue = CSSPrimitiveValue::create(length, &style);
            return primitiveValue->computeLength<unsigned>(conversionData);
        }
    }
    RefPtr<CSSValue> cssValue = value.createCSSValue();
    RefPtr<CSSPrimitiveValue> primitiveValue = toCSSPrimitiveValue(cssValue.get());
    return primitiveValue->computeLength<unsigned>(conversionData);
}

static unsigned defaultValue(RenderStyle& style, RenderView* view)
{
    const unsigned defaultSizesAttributeValueInVW = 100;

    CSSParserValue value;
    value.id = CSSValueInvalid;
    value.isInt = true;
    value.fValue = defaultSizesAttributeValueInVW;
    value.unit = CSSPrimitiveValue::CSS_VW;

    return computeLength(value, style, view);
}

unsigned SourceSize::length(RenderStyle& style, RenderView* view)
{
    return computeLength(m_length, style, view);
}

unsigned SourceSizeList::parseSizesAttribute(const String& sizesAttribute, RenderView* view, Frame* frame)
{
    if (!view)
        return 0;
    CSSParser parser(CSSStrictMode);
    std::unique_ptr<SourceSizeList> sourceSizeList = parser.parseSizesAttribute(sizesAttribute);
    if (!sourceSizeList)
        return defaultValue(view->style(), view);
    return sourceSizeList->getEffectiveSize(view->style(), view, frame);
}

unsigned SourceSizeList::getEffectiveSize(RenderStyle& style, RenderView* view, Frame* frame)
{
    for (int i = m_list.size() - 1; i >= 0; --i) {
        SourceSize* sourceSize = m_list[i].get();
        if (sourceSize->match(style, frame))
            return sourceSize->length(style, view);
    }
    return defaultValue(style, view);
}

#endif

} // namespace WebCore
