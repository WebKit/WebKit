/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023 ChangSeok Oh <changseok@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AnchorPositionEvaluator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPathValue.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSSubgridValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "FontSelectionValueInlines.h"
#include "FontSizeAdjust.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "RenderStyleInlines.h"
#include "RotateTransformOperation.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "ScaleTransformOperation.h"
#include "ScopedName.h"
#include "ScrollAxis.h"
#include "Settings.h"
#include "StyleBasicShape.h"
#include "StyleBuilderChecking.h"
#include "StyleCalculationValue.h"
#include "StyleClipPath.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleFlexBasis.h"
#include "StyleInset.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StyleOffsetAnchor.h"
#include "StyleOffsetDistance.h"
#include "StyleOffsetPath.h"
#include "StyleOffsetPosition.h"
#include "StyleOffsetRotate.h"
#include "StylePadding.h"
#include "StylePerspective.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleRayFunction.h"
#include "StyleResolveForFont.h"
#include "StyleRotate.h"
#include "StyleSVGPaint.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleTextEdge+CSSValueConversion.h"
#include "StyleTranslate.h"
#include "StyleURL.h"
#include "StyleValueTypes+CSSValueConversion.h"
#include "TextSpacing.h"
#include "ViewTimeline.h"
#include <ranges>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Style {

// FIXME: Some of those functions assume the CSS parser only allows valid CSSValue types.
// This might not be true if we pass the CSSValue from js via CSS Typed OM.

class BuilderConverter {
public:
    template<typename T, typename... Rest> static T convertStyleType(BuilderState&, const CSSValue&, Rest&&...);
};

template<typename T, typename... Rest> inline T BuilderConverter::convertStyleType(BuilderState& builderState, const CSSValue& value, Rest&&... rest)
{
    return toStyleFromCSSValue<T>(builderState, value, std::forward<Rest>(rest)...);
}

inline float zoomWithTextZoomFactor(BuilderState& builderState)
{
    if (auto* frame = builderState.document().frame()) {
        float textZoomFactor = builderState.style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        auto usedZoom = evaluationTimeZoomEnabled(builderState) ? 1.0f : builderState.style().usedZoom();
        return usedZoom * textZoomFactor;
    }
    return builderState.cssToLengthConversionData().zoom();
}

} // namespace Style
} // namespace WebCore
