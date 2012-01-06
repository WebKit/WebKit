/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "CSSValue.h"

#include "CSSAspectRatioValue.h"
#include "CSSBorderImageValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFlexValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageValue.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSReflectValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "FontValue.h"
#include "FontFamilyValue.h"
#include "FontFeatureValue.h"
#include "ShadowValue.h"
#include "SVGColor.h"
#include "SVGPaint.h"
#include "WebKitCSSFilterValue.h"
#include "WebKitCSSShaderValue.h"
#include "WebKitCSSTransformValue.h"

namespace WebCore {

class SameSizeAsCSSValue : public RefCounted<SameSizeAsCSSValue> {
    unsigned char bitfields[2];
};

COMPILE_ASSERT(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), CSS_value_should_stay_small);

CSSValue::Type CSSValue::cssValueType() const
{
    if (isInheritedValue())
        return CSS_INHERIT;
    if (isPrimitiveValue())
        return CSS_PRIMITIVE_VALUE;
    if (isValueList())
        return CSS_VALUE_LIST;
    if (isInitialValue())
        return CSS_INITIAL;
    return CSS_CUSTOM;
}

void CSSValue::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const CSSStyleSheet* styleSheet)
{
    if (isPrimitiveValue())
        static_cast<CSSPrimitiveValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (isValueList())
        static_cast<CSSValueList*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == BorderImageClass)
        static_cast<CSSBorderImageValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == FontFaceSrcClass)
        static_cast<CSSFontFaceSrcValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == ReflectClass)
        static_cast<CSSReflectValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
}

String CSSValue::cssText() const
{
    switch (classType()) {
    case AspectRatioClass:
        return static_cast<const CSSAspectRatioValue*>(this)->customCssText();
    case BorderImageClass:
        return static_cast<const CSSBorderImageValue*>(this)->customCssText();
    case BorderImageSliceClass:
        return static_cast<const CSSBorderImageSliceValue*>(this)->customCssText();
    case CanvasClass:
        return static_cast<const CSSCanvasValue*>(this)->customCssText();
    case CursorImageClass:
        return static_cast<const CSSCursorImageValue*>(this)->customCssText();
    case FontClass:
        return static_cast<const FontValue*>(this)->customCssText();
    case FontFaceSrcClass:
        return static_cast<const CSSFontFaceSrcValue*>(this)->customCssText();
    case FontFamilyClass:
        return static_cast<const FontFamilyValue*>(this)->customCssText();
    case FontFeatureClass:
        return static_cast<const FontFeatureValue*>(this)->customCssText();
    case FunctionClass:
        return static_cast<const CSSFunctionValue*>(this)->customCssText();
    case LinearGradientClass:
        return static_cast<const CSSLinearGradientValue*>(this)->customCssText();
    case RadialGradientClass:
        return static_cast<const CSSRadialGradientValue*>(this)->customCssText();
    case CrossfadeClass:
        return static_cast<const CSSCrossfadeValue*>(this)->customCssText();
    case ImageClass:
        return static_cast<const CSSImageValue*>(this)->customCssText();
    case InheritedClass:
        return static_cast<const CSSInheritedValue*>(this)->customCssText();
    case InitialClass:
        return static_cast<const CSSInitialValue*>(this)->customCssText();
    case PrimitiveClass:
        return static_cast<const CSSPrimitiveValue*>(this)->customCssText();
    case ReflectClass:
        return static_cast<const CSSReflectValue*>(this)->customCssText();
    case ShadowClass:
        return static_cast<const ShadowValue*>(this)->customCssText();
    case LinearTimingFunctionClass:
        return static_cast<const CSSLinearTimingFunctionValue*>(this)->customCssText();
    case CubicBezierTimingFunctionClass:
        return static_cast<const CSSCubicBezierTimingFunctionValue*>(this)->customCssText();
    case StepsTimingFunctionClass:
        return static_cast<const CSSStepsTimingFunctionValue*>(this)->customCssText();
    case UnicodeRangeClass:
        return static_cast<const CSSUnicodeRangeValue*>(this)->customCssText();
    case ValueListClass:
        return static_cast<const CSSValueList*>(this)->customCssText();
    case WebKitCSSTransformClass:
        return static_cast<const WebKitCSSTransformValue*>(this)->customCssText();
    case LineBoxContainClass:
        return static_cast<const CSSLineBoxContainValue*>(this)->customCssText();
    case FlexClass:
        return static_cast<const CSSFlexValue*>(this)->customCssText();
#if ENABLE(CSS_FILTERS)
    case WebKitCSSFilterClass:
        return static_cast<const WebKitCSSFilterValue*>(this)->customCssText();
#if ENABLE(CSS_SHADERS)
    case WebKitCSSShaderClass:
        return static_cast<const WebKitCSSShaderValue*>(this)->customCssText();
#endif
#endif
#if ENABLE(SVG)
    case SVGColorClass:
        return static_cast<const SVGColor*>(this)->customCssText();
    case SVGPaintClass:
        return static_cast<const SVGPaint*>(this)->customCssText();
#endif
    }
    ASSERT_NOT_REACHED();
    return String();
}

void CSSValue::destroy()
{
    switch (classType()) {
    case AspectRatioClass:
        delete static_cast<CSSAspectRatioValue*>(this);
        return;
    case BorderImageClass:
        delete static_cast<CSSBorderImageValue*>(this);
        return;
    case BorderImageSliceClass:
        delete static_cast<CSSBorderImageSliceValue*>(this);
        return;
    case CanvasClass:
        delete static_cast<CSSCanvasValue*>(this);
        return;
    case CursorImageClass:
        delete static_cast<CSSCursorImageValue*>(this);
        return;
    case FontClass:
        delete static_cast<FontValue*>(this);
        return;
    case FontFaceSrcClass:
        delete static_cast<CSSFontFaceSrcValue*>(this);
        return;
    case FontFamilyClass:
        delete static_cast<FontFamilyValue*>(this);
        return;
    case FontFeatureClass:
        delete static_cast<FontFeatureValue*>(this);
        return;
    case FunctionClass:
        delete static_cast<CSSFunctionValue*>(this);
        return;
    case LinearGradientClass:
        delete static_cast<CSSLinearGradientValue*>(this);
        return;
    case RadialGradientClass:
        delete static_cast<CSSRadialGradientValue*>(this);
        return;
    case CrossfadeClass:
        delete static_cast<CSSCrossfadeValue*>(this);
        return;
    case ImageClass:
        delete static_cast<CSSImageValue*>(this);
        return;
    case InheritedClass:
        delete static_cast<CSSInheritedValue*>(this);
        return;
    case InitialClass:
        delete static_cast<CSSInitialValue*>(this);
        return;
    case PrimitiveClass:
        delete static_cast<CSSPrimitiveValue*>(this);
        return;
    case ReflectClass:
        delete static_cast<CSSReflectValue*>(this);
        return;
    case ShadowClass:
        delete static_cast<ShadowValue*>(this);
        return;
    case LinearTimingFunctionClass:
        delete static_cast<CSSLinearTimingFunctionValue*>(this);
        return;
    case CubicBezierTimingFunctionClass:
        delete static_cast<CSSCubicBezierTimingFunctionValue*>(this);
        return;
    case StepsTimingFunctionClass:
        delete static_cast<CSSStepsTimingFunctionValue*>(this);
        return;
    case UnicodeRangeClass:
        delete static_cast<CSSUnicodeRangeValue*>(this);
        return;
    case ValueListClass:
        delete static_cast<CSSValueList*>(this);
        return;
    case WebKitCSSTransformClass:
        delete static_cast<WebKitCSSTransformValue*>(this);
        return;
    case LineBoxContainClass:
        delete static_cast<CSSLineBoxContainValue*>(this);
        return;
    case FlexClass:
        delete static_cast<CSSFlexValue*>(this);
        return;
#if ENABLE(CSS_FILTERS)
    case WebKitCSSFilterClass:
        delete static_cast<WebKitCSSFilterValue*>(this);
        return;
#if ENABLE(CSS_SHADERS)
    case WebKitCSSShaderClass:
        delete static_cast<WebKitCSSShaderValue*>(this);
        return;
#endif
#endif
#if ENABLE(SVG)
    case SVGColorClass:
        delete static_cast<SVGColor*>(this);
        return;
    case SVGPaintClass:
        delete static_cast<SVGPaint*>(this);
        return;
#endif
    }
    ASSERT_NOT_REACHED();
}

}
