/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

class CSSStyleDeclaration;
class CachedResource;
class DeprecatedCSSOMValue;

enum CSSPropertyID : uint16_t;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSValue);
class CSSValue {
    WTF_MAKE_NONCOPYABLE(CSSValue);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSValue);
public:
    static constexpr unsigned refCountFlagIsStatic = 0x1;
    static constexpr unsigned refCountIncrement = 0x2; // This allows us to ref / deref without disturbing the static CSSValue flag.
    void ref() const { m_refCount += refCountIncrement; }
    void deref() const;
    bool hasOneRef() const { return m_refCount == refCountIncrement; }
    unsigned refCount() const { return m_refCount / refCountIncrement; }
    bool hasAtLeastOneRef() const { return m_refCount; }

    String cssText() const;

    bool isPrimitiveValue() const { return m_classType == PrimitiveClass; }
    bool isValueList() const { return m_classType >= ValueListClass; }
    bool isValuePair() const { return m_classType == ValuePairClass; }

    bool isBaseValueList() const { return m_classType == ValueListClass; }

    bool isAspectRatioValue() const { return m_classType == AspectRatioClass; }
    bool isBackgroundRepeatValue() const { return m_classType == BackgroundRepeatClass; }
    bool isBorderImageSliceValue() const { return m_classType == BorderImageSliceClass; }
    bool isBorderImageWidthValue() const { return m_classType == BorderImageWidthClass; }
    bool isCanvasValue() const { return m_classType == CanvasClass; }
    bool isCrossfadeValue() const { return m_classType == CrossfadeClass; }
    bool isCursorImageValue() const { return m_classType == CursorImageClass; }
    bool isCustomPropertyValue() const { return m_classType == CustomPropertyClass; }
    bool isFunctionValue() const { return m_classType == FunctionClass; }
    bool isFontFeatureValue() const { return m_classType == FontFeatureClass; }
    bool isFontVariationValue() const { return m_classType == FontVariationClass; }
    bool isFontVariantAlternatesValue() const { return m_classType == FontVariantAlternatesClass; }
    bool isFontFaceSrcLocalValue() const { return m_classType == FontFaceSrcLocalClass; }
    bool isFontFaceSrcResourceValue() const { return m_classType == FontFaceSrcResourceClass; }
    bool isFontPaletteValuesOverrideColorsValue() const { return m_classType == FontPaletteValuesOverrideColorsClass; }
    bool isFontValue() const { return m_classType == FontClass; }
    bool isFontStyleRangeValue() const { return m_classType == FontStyleRangeClass; }
    bool isFontStyleWithAngleValue() const { return m_classType == FontStyleWithAngleClass; }
    bool isImageGeneratorValue() const { return m_classType >= CanvasClass && m_classType <= PrefixedRadialGradientClass; }
    bool isGradientValue() const { return m_classType >= LinearGradientClass && m_classType <= PrefixedRadialGradientClass; }
    bool isNamedImageValue() const { return m_classType == NamedImageClass; }
    bool isImageSetValue() const { return m_classType == ImageSetClass; }
    bool isImageValue() const { return m_classType == ImageClass; }
    bool isImplicitInitialValue() const;
    bool isInheritValue() const;
    bool isInitialValue() const;
    bool isUnsetValue() const;
    bool isRevertValue() const;
    bool isRevertLayerValue() const;
    bool isCSSWideKeyword() const;
    bool treatAsInitialValue(CSSPropertyID) const;
    bool treatAsInheritedValue(CSSPropertyID) const;
    bool isLinearGradientValue() const { return m_classType == LinearGradientClass; }
    bool isRadialGradientValue() const { return m_classType == RadialGradientClass; }
    bool isConicGradientValue() const { return m_classType == ConicGradientClass; }
    bool isDeprecatedLinearGradientValue() const { return m_classType == DeprecatedLinearGradientClass; }
    bool isDeprecatedRadialGradientValue() const { return m_classType == DeprecatedRadialGradientClass; }
    bool isPrefixedLinearGradientValue() const { return m_classType == PrefixedLinearGradientClass; }
    bool isPrefixedRadialGradientValue() const { return m_classType == PrefixedRadialGradientClass; }
    bool isReflectValue() const { return m_classType == ReflectClass; }
    bool isShadowValue() const { return m_classType == ShadowClass; }
    bool isCubicBezierTimingFunctionValue() const { return m_classType == CubicBezierTimingFunctionClass; }
    bool isStepsTimingFunctionValue() const { return m_classType == StepsTimingFunctionClass; }
    bool isSpringTimingFunctionValue() const { return m_classType == SpringTimingFunctionClass; }
    bool isLineBoxContainValue() const { return m_classType == LineBoxContainClass; }
    bool isCalcValue() const {return m_classType == CalculationClass; }
    bool isFilterImageValue() const { return m_classType == FilterImageClass; }
#if ENABLE(CSS_PAINTING_API)
    bool isPaintImageValue() const { return m_classType == PaintImageClass; }
#endif
    bool isContentDistributionValue() const { return m_classType == CSSContentDistributionClass; }
    bool isGridAutoRepeatValue() const { return m_classType == GridAutoRepeatClass; }
    bool isGridIntegerRepeatValue() const { return m_classType == GridIntegerRepeatClass; }
    bool isGridTemplateAreasValue() const { return m_classType == GridTemplateAreasClass; }
    bool isGridLineNamesValue() const { return m_classType == GridLineNamesClass; }
    bool isSubgridValue() const { return m_classType == SubgridClass; }
    bool isTransformListValue() const { return m_classType == TransformListClass; }
    bool isUnicodeRangeValue() const { return m_classType == UnicodeRangeClass; }

    bool isVariableReferenceValue() const { return m_classType == VariableReferenceClass; }
    bool isPendingSubstitutionValue() const { return m_classType == PendingSubstitutionValueClass; }

    bool isOffsetRotateValue() const { return m_classType == OffsetRotateClass; }
    bool isRayValue() const { return m_classType == RayClass; }

    // NOTE: This returns true for all image like values except CSSCursorImageValues, as these are
    //       the values that corrispond to the CSS <image> production.
    bool isImage() const { return isImageValue() || isImageSetValue() || isImageGeneratorValue(); }

    bool hasVariableReferences() const { return isVariableReferenceValue() || isPendingSubstitutionValue(); }

    Ref<DeprecatedCSSOMValue> createDeprecatedCSSOMWrapper(CSSStyleDeclaration&) const;

    bool traverseSubresources(const Function<bool(const CachedResource&)>&) const;

    // What properties does this value rely on (eg, font-size for em units)
    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>&) const;
    // What properties in the root element does this value rely on (eg. font-size for rem units)
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>&) const;

    bool equals(const CSSValue&) const;
    bool operator==(const CSSValue& other) const { return equals(other); }

    // https://www.w3.org/TR/css-values-4/#local-urls
    // Empty URLs and fragment-only URLs should not be resolved relative to the base URL.
    static bool isCSSLocalURL(StringView relativeURL);

    enum StaticCSSValueTag { StaticCSSValue };

    static constexpr size_t ValueSeparatorBits = 2;
    enum ValueSeparator : uint8_t { SpaceSeparator, CommaSeparator, SlashSeparator };

protected:
    static const size_t ClassTypeBits = 6;
    enum ClassType {
        PrimitiveClass,

        // Image classes.
        ImageClass,
        CursorImageClass,

        // Image generator classes.
        CanvasClass,
#if ENABLE(CSS_PAINTING_API)
        PaintImageClass,
#endif
        NamedImageClass,
        CrossfadeClass,
        FilterImageClass,
        LinearGradientClass,
        RadialGradientClass,
        ConicGradientClass,
        DeprecatedLinearGradientClass,
        DeprecatedRadialGradientClass,
        PrefixedLinearGradientClass,
        PrefixedRadialGradientClass,

        // Timing function classes.
        CubicBezierTimingFunctionClass,
        StepsTimingFunctionClass,
        SpringTimingFunctionClass,

        // Other class types.
        AspectRatioClass,
        BackgroundRepeatClass,
        BorderImageSliceClass,
        BorderImageWidthClass,
        FontFeatureClass,
        FontVariationClass,
        FontVariantAlternatesClass,
        FontClass,
        FontStyleRangeClass,
        FontStyleWithAngleClass,
        FontFaceSrcLocalClass,
        FontFaceSrcResourceClass,
        FontPaletteValuesOverrideColorsClass,
        FunctionClass,

        ReflectClass,
        ShadowClass,
        UnicodeRangeClass,
        LineBoxContainClass,
        CalculationClass,
        GridTemplateAreasClass,
        ValuePairClass,

        CSSContentDistributionClass,

        CustomPropertyClass,
        VariableReferenceClass,
        PendingSubstitutionValueClass,

        OffsetRotateClass,
        RayClass,

        // List class types must appear after ValueListClass. Note CSSFunctionValue
        // is deliberately excluded, since we don't want it exposed to the CSS OM
        // as a list.
        ValueListClass,
        ImageSetClass,
        GridLineNamesClass,
        GridAutoRepeatClass,
        GridIntegerRepeatClass,
        SubgridClass,
        TransformListClass,
        // Do not append non-list class types here.
    };

    constexpr ClassType classType() const { return static_cast<ClassType>(m_classType); }

    explicit CSSValue(ClassType classType)
        : m_classType(classType)
    {
    }

    void makeStatic()
    {
        m_refCount |= refCountFlagIsStatic;
    }

    // NOTE: This class is non-virtual for memory and performance reasons.
    // Don't go making it virtual again unless you know exactly what you're doing!
    ~CSSValue() = default;
    WEBCORE_EXPORT void operator delete(CSSValue*, std::destroying_delete_t);

    ValueSeparator separator() const { return static_cast<ValueSeparator>(m_valueSeparator); }
    static ASCIILiteral separatorCSSText(ValueSeparator);
    ASCIILiteral separatorCSSText() const { return separatorCSSText(separator()); };

private:
    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&);
    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&) const;

    static inline bool customTraverseSubresources(const Function<bool(const CachedResource&)>&);

    mutable unsigned m_refCount { refCountIncrement };

protected:
    // These data members are used by derived classes but here to maximize struct packing.

    // CSSPrimitiveValue:
    unsigned m_primitiveUnitType : 7 { 0 }; // CSSUnitType
    mutable unsigned m_hasCachedCSSText : 1 { false };
    unsigned m_isImplicit : 1 { false };

    // CSSValueList and CSSValuePair:
    unsigned m_valueSeparator : ValueSeparatorBits { 0 };

private:
    unsigned m_classType : ClassTypeBits; // ClassType
};

inline void CSSValue::deref() const
{
    unsigned tempRefCount = m_refCount - refCountIncrement;

    if (!tempRefCount) {
IGNORE_GCC_WARNINGS_BEGIN("free-nonheap-object")
        delete this;
IGNORE_GCC_WARNINGS_END
        return;
    }

    m_refCount = tempRefCount;
}

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

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_VALUE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSValue& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
