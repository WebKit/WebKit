/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSCounterValue.h"
#include "CSSEasingFunctionValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSPathValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSQuadValue.h"
#include "CSSRatioValue.h"
#include "CSSRayValue.h"
#include "CSSRectValue.h"
#include "CSSReflectValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSScrollValue.h"
#include "CSSSerializationContext.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CSSViewValue.h"
#include "ContainerNodeInlines.h"
#include "FontCascade.h"
#include "FontSelectionValueInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "PathOperation.h"
#include "PerspectiveTransformOperation.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderStyleInlines.h"
#include "ScrollTimeline.h"
#include "SkewTransformOperation.h"
#include "StyleClipPath.h"
#include "StyleColor.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleExtractorState.h"
#include "StyleFlexBasis.h"
#include "StyleInset.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StyleOffsetPath.h"
#include "StylePadding.h"
#include "StylePerspective.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleRotate.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleTranslate.h"
#include "TransformOperationData.h"
#include "ViewTimeline.h"
#include "WebAnimationUtilities.h"
#include <wtf/IteratorRange.h>

namespace WebCore {
namespace Style {

class ExtractorConverter {
public:
    // MARK: Strong value conversions

    template<typename T, typename... Rest> static Ref<CSSValue> convertStyleType(ExtractorState&, const T&, Rest&&...);

    // MARK: Primitive conversions

    template<typename ConvertibleType>
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, const ConvertibleType&);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, double);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, float);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, unsigned);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, int);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, unsigned short);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, short);
    static Ref<CSSPrimitiveValue> convert(ExtractorState&, const ScopedName&);

    template<typename T> static Ref<CSSPrimitiveValue> convertNumberAsPixels(ExtractorState&, T);

    // MARK: Transform conversions

    static Ref<CSSValue> convertTransformationMatrix(ExtractorState&, const TransformationMatrix&);
    static Ref<CSSValue> convertTransformationMatrix(const RenderStyle&, const TransformationMatrix&);
};

// MARK: - Strong value conversions

template<typename T, typename... Rest> Ref<CSSValue> ExtractorConverter::convertStyleType(ExtractorState& state, const T& value, Rest&&... rest)
{
    return createCSSValue(state.pool, state.style, value, std::forward<Rest>(rest)...);
}

// MARK: - Primitive conversions

template<typename ConvertibleType>
Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, const ConvertibleType& value)
{
    return CSSPrimitiveValue::create(toCSSValueID(value));
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, double value)
{
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, float value)
{
    return CSSPrimitiveValue::create(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, unsigned value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, int value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, unsigned short value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, short value)
{
    return CSSPrimitiveValue::createInteger(value);
}

inline Ref<CSSPrimitiveValue> ExtractorConverter::convert(ExtractorState&, const ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        return CSSPrimitiveValue::createCustomIdent(scopedName.name);
    return CSSPrimitiveValue::create(scopedName.name);
}

template<typename T> Ref<CSSPrimitiveValue> ExtractorConverter::convertNumberAsPixels(ExtractorState& state, T number)
{
    return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(number, state.style), CSSUnitType::CSS_PX);
}

// MARK: - Transform conversions

inline Ref<CSSValue> ExtractorConverter::convertTransformationMatrix(ExtractorState& state, const TransformationMatrix& transform)
{
    return convertTransformationMatrix(state.style, transform);
}

inline Ref<CSSValue> ExtractorConverter::convertTransformationMatrix(const RenderStyle& style, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        double values[] = { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        CSSValueListBuilder arguments;
        for (auto value : values)
            arguments.append(CSSPrimitiveValue::create(value));
        return CSSFunctionValue::create(CSSValueMatrix, WTFMove(arguments));
    }

    double values[] = {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    CSSValueListBuilder arguments;
    for (auto value : values)
        arguments.append(CSSPrimitiveValue::create(value));
    return CSSFunctionValue::create(CSSValueMatrix3d, WTFMove(arguments));
}

} // namespace Style
} // namespace WebCore
