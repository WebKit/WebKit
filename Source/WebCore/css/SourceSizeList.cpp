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
#include "MediaQuery.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryExp.h"
#include "RenderStyle.h"
#include "RenderView.h"

namespace WebCore {

static bool match(std::unique_ptr<MediaQueryExp>&& expression, RenderStyle& style, Frame* frame)
{
    if (expression->mediaFeature().isEmpty())
        return true;

    auto expList = std::make_unique<Vector<std::unique_ptr<MediaQueryExp>>>();
    expList->append(WTFMove(expression));

    RefPtr<MediaQuerySet> mediaQuerySet = MediaQuerySet::create();
    mediaQuerySet->addMediaQuery(std::make_unique<MediaQuery>(MediaQuery::None, "all", WTFMove(expList)));

    MediaQueryEvaluator mediaQueryEvaluator("screen", frame, &style);
    return mediaQueryEvaluator.eval(mediaQuerySet.get());
}

static float defaultLength(RenderStyle& style, RenderView* view)
{
    return clampTo<float>(CSSPrimitiveValue::computeNonCalcLengthDouble(CSSToLengthConversionData(&style, &style, view), CSSPrimitiveValue::CSS_VW, 100.0));
}

static float computeLength(CSSValue* value, RenderStyle& style, RenderView* view)
{
    CSSToLengthConversionData conversionData(&style, &style, view);
    if (is<CSSPrimitiveValue>(value)) {
        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        if (!primitiveValue.isLength())
            return defaultLength(style, view);
        return primitiveValue.computeLength<float>(conversionData);
    }
    if (is<CSSCalcValue>(value))
        return downcast<CSSCalcValue>(*value).computeLengthPx(conversionData);
    return defaultLength(style, view);
}

float parseSizesAttribute(StringView sizesAttribute, RenderView* view, Frame* frame)
{
    if (!view)
        return 0;
    if (!sizesAttribute.empty())
        view->document().updateStyleIfNeeded();
    RenderStyle& style = view->style();
    for (auto& sourceSize : CSSParser(CSSStrictMode).parseSizesAttribute(sizesAttribute)) {
        if (match(WTFMove(sourceSize.expression), style, frame))
            return computeLength(sourceSize.length.get(), style, view);
    }
    return defaultLength(style, view);
}

} // namespace WebCore
