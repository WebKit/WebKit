/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
#include "CSSBorderImageSliceValue.h"
#include "CSSCalculationValue.h"
#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontValue.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "SVGColor.h"
#include "SVGPaint.h"
#include "WebKitCSSFilterValue.h"
#include "WebKitCSSTransformValue.h"

#if ENABLE(CSS_GRID_LAYOUT)
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#endif

namespace WebCore {

struct SameSizeAsCSSValue : public RefCounted<SameSizeAsCSSValue> {
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), CSS_value_should_stay_small);

class TextCloneCSSValue : public CSSValue {
public:
    static PassRef<TextCloneCSSValue> create(ClassType classType, const String& text)
    {
        return adoptRef(*new TextCloneCSSValue(classType, text));
    }

    String cssText() const { return m_cssText; }

private:
    TextCloneCSSValue(ClassType classType, const String& text) 
        : CSSValue(classType, /*isCSSOMSafe*/ true)
        , m_cssText(text)
    {
        m_isTextClone = true;
    }

    String m_cssText;
};

bool CSSValue::isImplicitInitialValue() const
{
    return m_classType == InitialClass && toCSSInitialValue(this)->isImplicit();
}

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

void CSSValue::addSubresourceStyleURLs(ListHashSet<URL>& urls, const StyleSheetContents* styleSheet) const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (isPrimitiveValue())
        toCSSPrimitiveValue(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (isValueList())
        toCSSValueList(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == FontFaceSrcClass)
        toCSSFontFaceSrcValue(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == ReflectClass)
        toCSSReflectValue(this)->addSubresourceStyleURLs(urls, styleSheet);
}

bool CSSValue::hasFailedOrCanceledSubresources() const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (isValueList())
        return toCSSValueList(this)->hasFailedOrCanceledSubresources();
    if (classType() == FontFaceSrcClass)
        return toCSSFontFaceSrcValue(this)->hasFailedOrCanceledSubresources();
    if (classType() == ImageClass)
        return toCSSImageValue(this)->hasFailedOrCanceledSubresources();
    if (classType() == CrossfadeClass)
        return toCSSCrossfadeValue(this)->hasFailedOrCanceledSubresources();
#if ENABLE(CSS_FILTERS)
    if (classType() == FilterImageClass)
        return toCSSFilterImageValue(this)->hasFailedOrCanceledSubresources();
#endif
#if ENABLE(CSS_IMAGE_SET)
    if (classType() == ImageSetClass)
        return toCSSImageSetValue(this)->hasFailedOrCanceledSubresources();
#endif
    return false;
}

template<class ChildClassType>
inline static bool compareCSSValues(const CSSValue& first, const CSSValue& second)
{
    return static_cast<const ChildClassType&>(first).equals(static_cast<const ChildClassType&>(second));
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText() == other.cssText();
    }

    if (m_classType == other.m_classType) {
        switch (m_classType) {
        case AspectRatioClass:
            return compareCSSValues<CSSAspectRatioValue>(*this, other);
        case BorderImageSliceClass:
            return compareCSSValues<CSSBorderImageSliceValue>(*this, other);
        case CanvasClass:
            return compareCSSValues<CSSCanvasValue>(*this, other);
        case CursorImageClass:
            return compareCSSValues<CSSCursorImageValue>(*this, other);
#if ENABLE(CSS_FILTERS)
        case FilterImageClass:
            return compareCSSValues<CSSFilterImageValue>(*this, other);
#endif
        case FontClass:
            return compareCSSValues<CSSFontValue>(*this, other);
        case FontFaceSrcClass:
            return compareCSSValues<CSSFontFaceSrcValue>(*this, other);
        case FontFeatureClass:
            return compareCSSValues<CSSFontFeatureValue>(*this, other);
        case FunctionClass:
            return compareCSSValues<CSSFunctionValue>(*this, other);
        case LinearGradientClass:
            return compareCSSValues<CSSLinearGradientValue>(*this, other);
        case RadialGradientClass:
            return compareCSSValues<CSSRadialGradientValue>(*this, other);
        case CrossfadeClass:
            return compareCSSValues<CSSCrossfadeValue>(*this, other);
        case ImageClass:
            return compareCSSValues<CSSImageValue>(*this, other);
        case InheritedClass:
            return compareCSSValues<CSSInheritedValue>(*this, other);
        case InitialClass:
            return compareCSSValues<CSSInitialValue>(*this, other);
#if ENABLE(CSS_GRID_LAYOUT)
        case GridLineNamesClass:
            return compareCSSValues<CSSGridLineNamesValue>(*this, other);
        case GridTemplateAreasClass:
            return compareCSSValues<CSSGridTemplateAreasValue>(*this, other);
#endif
        case PrimitiveClass:
            return compareCSSValues<CSSPrimitiveValue>(*this, other);
        case ReflectClass:
            return compareCSSValues<CSSReflectValue>(*this, other);
        case ShadowClass:
            return compareCSSValues<CSSShadowValue>(*this, other);
        case CubicBezierTimingFunctionClass:
            return compareCSSValues<CSSCubicBezierTimingFunctionValue>(*this, other);
        case StepsTimingFunctionClass:
            return compareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
        case UnicodeRangeClass:
            return compareCSSValues<CSSUnicodeRangeValue>(*this, other);
        case ValueListClass:
            return compareCSSValues<CSSValueList>(*this, other);
        case WebKitCSSTransformClass:
            return compareCSSValues<WebKitCSSTransformValue>(*this, other);
        case LineBoxContainClass:
            return compareCSSValues<CSSLineBoxContainValue>(*this, other);
        case CalculationClass:
            return compareCSSValues<CSSCalcValue>(*this, other);
#if ENABLE(CSS_IMAGE_SET)
        case ImageSetClass:
            return compareCSSValues<CSSImageSetValue>(*this, other);
#endif
#if ENABLE(CSS_FILTERS)
        case WebKitCSSFilterClass:
            return compareCSSValues<WebKitCSSFilterValue>(*this, other);
#endif
        case SVGColorClass:
            return compareCSSValues<SVGColor>(*this, other);
        case SVGPaintClass:
            return compareCSSValues<SVGPaint>(*this, other);
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    } else if (m_classType == ValueListClass && other.m_classType != ValueListClass)
        return toCSSValueList(this)->equals(other);
    else if (m_classType != ValueListClass && other.m_classType == ValueListClass)
        return static_cast<const CSSValueList&>(other).equals(*this);
    return false;
}

String CSSValue::cssText() const
{
    if (m_isTextClone) {
         ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText();
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        return toCSSAspectRatioValue(this)->customCSSText();
    case BorderImageSliceClass:
        return toCSSBorderImageSliceValue(this)->customCSSText();
    case CanvasClass:
        return toCSSCanvasValue(this)->customCSSText();
    case CursorImageClass:
        return toCSSCursorImageValue(this)->customCSSText();
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        return toCSSFilterImageValue(this)->customCSSText();
#endif
    case FontClass:
        return toCSSFontValue(this)->customCSSText();
    case FontFaceSrcClass:
        return toCSSFontFaceSrcValue(this)->customCSSText();
    case FontFeatureClass:
        return toCSSFontFeatureValue(this)->customCSSText();
    case FunctionClass:
        return toCSSFunctionValue(this)->customCSSText();
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->customCSSText();
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->customCSSText();
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->customCSSText();
    case ImageClass:
        return toCSSImageValue(this)->customCSSText();
    case InheritedClass:
        return toCSSInheritedValue(this)->customCSSText();
    case InitialClass:
        return toCSSInitialValue(this)->customCSSText();
#if ENABLE(CSS_GRID_LAYOUT)
    case GridLineNamesClass:
        return toCSSGridLineNamesValue(this)->customCSSText();
    case GridTemplateAreasClass:
        return toCSSGridTemplateAreasValue(this)->customCSSText();
#endif
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->customCSSText();
    case ReflectClass:
        return toCSSReflectValue(this)->customCSSText();
    case ShadowClass:
        return toCSSShadowValue(this)->customCSSText();
    case CubicBezierTimingFunctionClass:
        return toCSSCubicBezierTimingFunctionValue(this)->customCSSText();
    case StepsTimingFunctionClass:
        return toCSSStepsTimingFunctionValue(this)->customCSSText();
    case UnicodeRangeClass:
        return toCSSUnicodeRangeValue(this)->customCSSText();
    case ValueListClass:
        return toCSSValueList(this)->customCSSText();
    case WebKitCSSTransformClass:
        return toWebKitCSSTransformValue(this)->customCSSText();
    case LineBoxContainClass:
        return toCSSLineBoxContainValue(this)->customCSSText();
    case CalculationClass:
        return toCSSCalcValue(this)->customCSSText();
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        return toCSSImageSetValue(this)->customCSSText();
#endif
#if ENABLE(CSS_FILTERS)
    case WebKitCSSFilterClass:
        return toWebKitCSSFilterValue(this)->customCSSText();
#endif
    case SVGColorClass:
        return toSVGColor(this)->customCSSText();
    case SVGPaintClass:
        return toSVGPaint(this)->customCSSText();
    }
    ASSERT_NOT_REACHED();
    return String();
}

void CSSValue::destroy()
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        delete static_cast<TextCloneCSSValue*>(this);
        return;
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        delete toCSSAspectRatioValue(this);
        return;
    case BorderImageSliceClass:
        delete toCSSBorderImageSliceValue(this);
        return;
    case CanvasClass:
        delete toCSSCanvasValue(this);
        return;
    case CursorImageClass:
        delete toCSSCursorImageValue(this);
        return;
    case FontClass:
        delete toCSSFontValue(this);
        return;
    case FontFaceSrcClass:
        delete toCSSFontFaceSrcValue(this);
        return;
    case FontFeatureClass:
        delete toCSSFontFeatureValue(this);
        return;
    case FunctionClass:
        delete toCSSFunctionValue(this);
        return;
    case LinearGradientClass:
        delete toCSSLinearGradientValue(this);
        return;
    case RadialGradientClass:
        delete toCSSRadialGradientValue(this);
        return;
    case CrossfadeClass:
        delete toCSSCrossfadeValue(this);
        return;
    case ImageClass:
        delete toCSSImageValue(this);
        return;
    case InheritedClass:
        delete toCSSInheritedValue(this);
        return;
    case InitialClass:
        delete toCSSInitialValue(this);
        return;
#if ENABLE(CSS_GRID_LAYOUT)
    case GridLineNamesClass:
        delete toCSSGridLineNamesValue(this);
        return;
    case GridTemplateAreasClass:
        delete toCSSGridTemplateAreasValue(this);
        return;
#endif
    case PrimitiveClass:
        delete toCSSPrimitiveValue(this);
        return;
    case ReflectClass:
        delete toCSSReflectValue(this);
        return;
    case ShadowClass:
        delete toCSSShadowValue(this);
        return;
    case CubicBezierTimingFunctionClass:
        delete toCSSCubicBezierTimingFunctionValue(this);
        return;
    case StepsTimingFunctionClass:
        delete toCSSStepsTimingFunctionValue(this);
        return;
    case UnicodeRangeClass:
        delete toCSSUnicodeRangeValue(this);
        return;
    case ValueListClass:
        delete toCSSValueList(this);
        return;
    case WebKitCSSTransformClass:
        delete toWebKitCSSTransformValue(this);
        return;
    case LineBoxContainClass:
        delete toCSSLineBoxContainValue(this);
        return;
    case CalculationClass:
        delete toCSSCalcValue(this);
        return;
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        delete toCSSImageSetValue(this);
        return;
#endif
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        delete toCSSFilterImageValue(this);
        return;
    case WebKitCSSFilterClass:
        delete toWebKitCSSFilterValue(this);
        return;
#endif
    case SVGColorClass:
        delete toSVGColor(this);
        return;
    case SVGPaintClass:
        delete toSVGPaint(this);
        return;
    }
    ASSERT_NOT_REACHED();
}

PassRefPtr<CSSValue> CSSValue::cloneForCSSOM() const
{
    switch (classType()) {
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->cloneForCSSOM();
    case ValueListClass:
        return toCSSValueList(this)->cloneForCSSOM();
    case ImageClass:
    case CursorImageClass:
        return toCSSImageValue(this)->cloneForCSSOM();
#if ENABLE(CSS_FILTERS)
    case WebKitCSSFilterClass:
        return toWebKitCSSFilterValue(this)->cloneForCSSOM();
#endif
    case WebKitCSSTransformClass:
        return toWebKitCSSTransformValue(this)->cloneForCSSOM();
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        return toCSSImageSetValue(this)->cloneForCSSOM();
#endif
    case SVGColorClass:
        return toSVGColor(this)->cloneForCSSOM();
    case SVGPaintClass:
        return toSVGPaint(this)->cloneForCSSOM();
    default:
        ASSERT(!isSubtypeExposedToCSSOM());
        return TextCloneCSSValue::create(classType(), cssText());
    }
}

}
