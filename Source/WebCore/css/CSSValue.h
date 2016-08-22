/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionCode.h"
#include "URLHash.h"
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class CachedResource;
class StyleSheetContents;

enum CSSPropertyID : uint16_t;

// FIXME: The current CSSValue and subclasses should be turned into internal types (StyleValue).
// The few subtypes that are actually exposed in CSSOM can be seen in the cloneForCSSOM() function.
// They should be handled by separate wrapper classes.

// Please don't expose more CSSValue types to the web.
class CSSValue : public RefCounted<CSSValue> {
public:
    enum Type {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4,
        CSS_UNSET = 5,
        CSS_REVERT = 6
    };

    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref()
    {
        if (derefBase())
            destroy();
    }

    WEBCORE_EXPORT Type cssValueType() const;

    WEBCORE_EXPORT String cssText() const;

    void setCssText(const String&, ExceptionCode&) { } // FIXME: Not implemented.

    bool isPrimitiveValue() const { return m_classType == PrimitiveClass; }
    bool isValueList() const { return m_classType >= ValueListClass; }
    
    bool isBaseValueList() const { return m_classType == ValueListClass; }
        

    bool isAspectRatioValue() const { return m_classType == AspectRatioClass; }
    bool isBorderImageSliceValue() const { return m_classType == BorderImageSliceClass; }
    bool isCanvasValue() const { return m_classType == CanvasClass; }
    bool isCrossfadeValue() const { return m_classType == CrossfadeClass; }
    bool isCursorImageValue() const { return m_classType == CursorImageClass; }
    bool isCustomPropertyValue() const { return m_classType == CustomPropertyClass; }
    bool isInvalidCustomPropertyValue() const;
    bool isVariableDependentValue() const { return m_classType == VariableDependentClass; }
    bool isVariableValue() const { return m_classType == VariableClass; }
    bool isFunctionValue() const { return m_classType == FunctionClass; }
    bool isFontFeatureValue() const { return m_classType == FontFeatureClass; }
    bool isFontFaceSrcValue() const { return m_classType == FontFaceSrcClass; }
    bool isFontValue() const { return m_classType == FontClass; }
    bool isImageGeneratorValue() const { return m_classType >= CanvasClass && m_classType <= RadialGradientClass; }
    bool isGradientValue() const { return m_classType >= LinearGradientClass && m_classType <= RadialGradientClass; }
    bool isNamedImageValue() const { return m_classType == NamedImageClass; }
    bool isImageSetValue() const { return m_classType == ImageSetClass; }
    bool isImageValue() const { return m_classType == ImageClass; }
    bool isImplicitInitialValue() const;
    bool isInheritedValue() const { return m_classType == InheritedClass; }
    bool isInitialValue() const { return m_classType == InitialClass; }
    bool isUnsetValue() const { return m_classType == UnsetClass; }
    bool isRevertValue() const { return m_classType == RevertClass; }
    bool treatAsInitialValue(CSSPropertyID) const;
    bool treatAsInheritedValue(CSSPropertyID) const;
    bool isLinearGradientValue() const { return m_classType == LinearGradientClass; }
    bool isRadialGradientValue() const { return m_classType == RadialGradientClass; }
    bool isReflectValue() const { return m_classType == ReflectClass; }
    bool isShadowValue() const { return m_classType == ShadowClass; }
    bool isCubicBezierTimingFunctionValue() const { return m_classType == CubicBezierTimingFunctionClass; }
    bool isStepsTimingFunctionValue() const { return m_classType == StepsTimingFunctionClass; }
    bool isSpringTimingFunctionValue() const { return m_classType == SpringTimingFunctionClass; }
    bool isWebKitCSSTransformValue() const { return m_classType == WebKitCSSTransformClass; }
    bool isLineBoxContainValue() const { return m_classType == LineBoxContainClass; }
    bool isCalcValue() const {return m_classType == CalculationClass; }
    bool isFilterImageValue() const { return m_classType == FilterImageClass; }
    bool isWebKitCSSFilterValue() const { return m_classType == WebKitCSSFilterClass; }
    bool isContentDistributionValue() const { return m_classType == CSSContentDistributionClass; }
#if ENABLE(CSS_GRID_LAYOUT)
    bool isGridAutoRepeatValue() const { return m_classType == GridAutoRepeatClass; }
    bool isGridTemplateAreasValue() const { return m_classType == GridTemplateAreasClass; }
    bool isGridLineNamesValue() const { return m_classType == GridLineNamesClass; }
#endif
    bool isSVGColor() const { return m_classType == SVGColorClass || m_classType == SVGPaintClass; }
    bool isSVGPaint() const { return m_classType == SVGPaintClass; }
    bool isUnicodeRangeValue() const { return m_classType == UnicodeRangeClass; }

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    bool isAnimationTriggerScrollValue() const { return m_classType == AnimationTriggerScrollClass; }
#endif

    bool isCSSOMSafe() const { return m_isCSSOMSafe; }
    bool isSubtypeExposedToCSSOM() const
    { 
        return isPrimitiveValue() 
            || isSVGColor()
            || isValueList();
    }

    RefPtr<CSSValue> cloneForCSSOM() const;

    void addSubresourceStyleURLs(ListHashSet<URL>&, const StyleSheetContents*) const;

    bool traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const;

    bool equals(const CSSValue&) const;
    bool operator==(const CSSValue& other) const { return equals(other); }

protected:

    static const size_t ClassTypeBits = 6;
    enum ClassType {
        PrimitiveClass,

        // Image classes.
        ImageClass,
        CursorImageClass,

        // Image generator classes.
        CanvasClass,
        NamedImageClass,
        CrossfadeClass,
        FilterImageClass,
        LinearGradientClass,
        RadialGradientClass,

        // Timing function classes.
        CubicBezierTimingFunctionClass,
        StepsTimingFunctionClass,
        SpringTimingFunctionClass,

        // Other class types.
        AspectRatioClass,
        BorderImageSliceClass,
        FontFeatureClass,
        FontClass,
        FontFaceSrcClass,
        FunctionClass,

        InheritedClass,
        InitialClass,
        UnsetClass,
        RevertClass,

        ReflectClass,
        ShadowClass,
        UnicodeRangeClass,
        LineBoxContainClass,
        CalculationClass,
#if ENABLE(CSS_GRID_LAYOUT)
        GridTemplateAreasClass,
#endif
        SVGColorClass,
        SVGPaintClass,

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
        AnimationTriggerScrollClass,
#endif

        CSSContentDistributionClass,
        CustomPropertyClass,
        VariableDependentClass,
        VariableClass,

        // List class types must appear after ValueListClass.
        ValueListClass,
        ImageSetClass,
        WebKitCSSFilterClass,
        WebKitCSSTransformClass,
#if ENABLE(CSS_GRID_LAYOUT)
        GridLineNamesClass,
        GridAutoRepeatClass,
#endif
        // Do not append non-list class types here.
    };

    static const size_t ValueListSeparatorBits = 2;
    enum ValueListSeparator {
        SpaceSeparator,
        CommaSeparator,
        SlashSeparator
    };

    ClassType classType() const { return static_cast<ClassType>(m_classType); }

    explicit CSSValue(ClassType classType, bool isCSSOMSafe = false)
        : m_isCSSOMSafe(isCSSOMSafe)
        , m_isTextClone(false)
        , m_primitiveUnitType(0)
        , m_hasCachedCSSText(false)
        , m_isQuirkValue(false)
        , m_valueListSeparator(SpaceSeparator)
        , m_classType(classType)
    {
    }

    // NOTE: This class is non-virtual for memory and performance reasons.
    // Don't go making it virtual again unless you know exactly what you're doing!

    ~CSSValue() { }

private:
    WEBCORE_EXPORT void destroy();

protected:
    unsigned m_isCSSOMSafe : 1;
    unsigned m_isTextClone : 1;
    // The bits in this section are only used by specific subclasses but kept here
    // to maximize struct packing.

    // CSSPrimitiveValue bits:
    unsigned m_primitiveUnitType : 7; // CSSPrimitiveValue::UnitTypes
    mutable unsigned m_hasCachedCSSText : 1;
    unsigned m_isQuirkValue : 1;

    unsigned m_valueListSeparator : ValueListSeparatorBits;

private:
    unsigned m_classType : ClassTypeBits; // ClassType
    
friend class CSSValueList;
};

template<typename CSSValueType>
inline bool compareCSSValueVector(const Vector<Ref<CSSValueType>>& firstVector, const Vector<Ref<CSSValueType>>& secondVector)
{
    size_t size = firstVector.size();
    if (size != secondVector.size())
        return false;

    for (size_t i = 0; i < size; ++i) {
        auto& firstPtr = firstVector[i];
        auto& secondPtr = secondVector[i];
        if (firstPtr.ptr() == secondPtr.ptr() || firstPtr->equals(secondPtr))
            continue;
        return false;
    }
    return true;
}

template<typename CSSValueType>
inline bool compareCSSValuePtr(const RefPtr<CSSValueType>& first, const RefPtr<CSSValueType>& second)
{
    return first ? second && first->equals(*second) : !second;
}

template<typename CSSValueType>
inline bool compareCSSValue(const Ref<CSSValueType>& first, const Ref<CSSValueType>& second)
{
    return first.get().equals(second);
}

typedef HashMap<AtomicString, RefPtr<CSSValue>> CustomPropertyValueMap;

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_VALUE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSValue& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
