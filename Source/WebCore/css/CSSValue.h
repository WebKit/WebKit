/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>

namespace WTF {
class Hasher;
}

namespace WebCore {

class CSSPrimitiveValue;
class CSSStyleDeclaration;
class CSSUnresolvedColor;
class CachedResource;
class Color;
class DeprecatedCSSOMValue;
class Quad;
class Rect;

struct Counter;

enum CSSPropertyID : uint16_t;
enum CSSValueID : uint16_t;

struct ComputedStyleDependencies {
    Vector<CSSPropertyID> properties;
    Vector<CSSPropertyID> rootProperties;
    bool containerDimensions { false };
    bool viewportDimensions { false };

    bool isComputationallyIndependent() const { return properties.isEmpty() && rootProperties.isEmpty() && !containerDimensions; }
};

DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSValue);
class CSSValue {
    WTF_MAKE_NONCOPYABLE(CSSValue);
    WTF_MAKE_FAST_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSValue);
public:
    static constexpr unsigned refCountFlagIsStatic = 0x1;
    static constexpr unsigned refCountIncrement = 0x2; // This allows us to ref / deref without disturbing the static CSSValue flag.
    void ref() const { m_refCount += refCountIncrement; }
    void deref() const;
    bool hasOneRef() const { return m_refCount == refCountIncrement; }
    unsigned refCount() const { return m_refCount / refCountIncrement; }
    bool hasAtLeastOneRef() const { return m_refCount; }

    String cssText() const;

    bool isAspectRatioValue() const { return m_classType == AspectRatioClass; }
    bool isBackgroundRepeatValue() const { return m_classType == BackgroundRepeatClass; }
    bool isBorderImageSliceValue() const { return m_classType == BorderImageSliceClass; }
    bool isBorderImageWidthValue() const { return m_classType == BorderImageWidthClass; }
    bool isCalcValue() const { return m_classType == CalculationClass; }
    bool isCanvasValue() const { return m_classType == CanvasClass; }
    bool isCircle() const { return m_classType == CircleClass; }
    bool isConicGradientValue() const { return m_classType == ConicGradientClass; }
    bool isContentDistributionValue() const { return m_classType == ContentDistributionClass; }
    bool isCounter() const { return m_classType == CounterClass; }
    bool isCrossfadeValue() const { return m_classType == CrossfadeClass; }
    bool isCubicBezierTimingFunctionValue() const { return m_classType == CubicBezierTimingFunctionClass; }
    bool isCursorImageValue() const { return m_classType == CursorImageClass; }
    bool isCustomPropertyValue() const { return m_classType == CustomPropertyClass; }
    bool isDeprecatedLinearGradientValue() const { return m_classType == DeprecatedLinearGradientClass; }
    bool isDeprecatedRadialGradientValue() const { return m_classType == DeprecatedRadialGradientClass; }
    bool isEllipse() const { return m_classType == EllipseClass; }
    bool isFilterImageValue() const { return m_classType == FilterImageClass; }
    bool isFontFaceSrcLocalValue() const { return m_classType == FontFaceSrcLocalClass; }
    bool isFontFaceSrcResourceValue() const { return m_classType == FontFaceSrcResourceClass; }
    bool isFontFeatureValue() const { return m_classType == FontFeatureClass; }
    bool isFontPaletteValuesOverrideColorsValue() const { return m_classType == FontPaletteValuesOverrideColorsClass; }
    bool isFontStyleRangeValue() const { return m_classType == FontStyleRangeClass; }
    bool isFontStyleWithAngleValue() const { return m_classType == FontStyleWithAngleClass; }
    bool isFontValue() const { return m_classType == FontClass; }
    bool isFontVariantAlternatesValue() const { return m_classType == FontVariantAlternatesClass; }
    bool isFontVariationValue() const { return m_classType == FontVariationClass; }
    bool isFunctionValue() const { return m_classType == FunctionClass; }
    bool isGridAutoRepeatValue() const { return m_classType == GridAutoRepeatClass; }
    bool isGridIntegerRepeatValue() const { return m_classType == GridIntegerRepeatClass; }
    bool isGridLineNamesValue() const { return m_classType == GridLineNamesClass; }
    bool isGridTemplateAreasValue() const { return m_classType == GridTemplateAreasClass; }
    bool isImageSetOptionValue() const { return m_classType == ImageSetOptionClass; }
    bool isImageSetValue() const { return m_classType == ImageSetClass; }
    bool isImageValue() const { return m_classType == ImageClass; }
    bool isInsetShape() const { return m_classType == InsetShapeClass; }
    bool isLineBoxContainValue() const { return m_classType == LineBoxContainClass; }
    bool isLinearGradientValue() const { return m_classType == LinearGradientClass; }
    bool isLinearTimingFunctionValue() const { return m_classType == LinearTimingFunctionClass; }
    bool isNamedImageValue() const { return m_classType == NamedImageClass; }
    bool isOffsetRotateValue() const { return m_classType == OffsetRotateClass; }
    bool isPair() const { return m_classType == ValuePairClass; }
    bool isPath() const { return m_classType == PathClass; }
    bool isPendingSubstitutionValue() const { return m_classType == PendingSubstitutionValueClass; }
    bool isPolygon() const { return m_classType == PolygonClass; }
    bool isPrefixedLinearGradientValue() const { return m_classType == PrefixedLinearGradientClass; }
    bool isPrefixedRadialGradientValue() const { return m_classType == PrefixedRadialGradientClass; }
    bool isPrimitiveValue() const { return m_classType == PrimitiveClass; }
    bool isQuad() const { return m_classType == QuadClass; }
    bool isRadialGradientValue() const { return m_classType == RadialGradientClass; }
    bool isRayValue() const { return m_classType == RayClass; }
    bool isRect() const { return m_classType == RectClass; }
    bool isRectShape() const { return m_classType == RectShapeClass; }
    bool isReflectValue() const { return m_classType == ReflectClass; }
    bool isScrollValue() const { return m_classType == ScrollClass; }
    bool isShadowValue() const { return m_classType == ShadowClass; }
    bool isSpringTimingFunctionValue() const { return m_classType == SpringTimingFunctionClass; }
    bool isStepsTimingFunctionValue() const { return m_classType == StepsTimingFunctionClass; }
    bool isSubgridValue() const { return m_classType == SubgridClass; }
    bool isTransformListValue() const { return m_classType == TransformListClass; }
    bool isUnicodeRangeValue() const { return m_classType == UnicodeRangeClass; }
    bool isValueList() const { return m_classType == ValueListClass; }
    bool isVariableReferenceValue() const { return m_classType == VariableReferenceClass; }
    bool isViewValue() const { return m_classType == ViewClass; }
    bool isXywhShape() const { return m_classType == XywhShapeClass; }

#if ENABLE(CSS_PAINTING_API)
    bool isPaintImageValue() const { return m_classType == PaintImageClass; }
#endif

    bool hasVariableReferences() const { return isVariableReferenceValue() || isPendingSubstitutionValue(); }
    bool isGradientValue() const { return m_classType >= LinearGradientClass && m_classType <= PrefixedRadialGradientClass; }
    bool isImageGeneratorValue() const { return m_classType >= CanvasClass && m_classType <= PrefixedRadialGradientClass; }
    bool isImplicitInitialValue() const { return m_isImplicitInitialValue; }
    bool containsVector() const { return m_classType >= ValueListClass; }

    // NOTE: This returns true for all image-like values except CSSCursorImageValues; these are the values that correspond to the CSS <image> production.
    bool isImage() const { return isImageValue() || isImageSetValue() || isImageGeneratorValue(); }

    Ref<DeprecatedCSSOMValue> createDeprecatedCSSOMWrapper(CSSStyleDeclaration&) const;

    bool traverseSubresources(const Function<bool(const CachedResource&)>&) const;
    void setReplacementURLForSubresources(const HashMap<String, String>&);
    void clearReplacementURLForSubresources();

    // What properties does this value rely on (eg, font-size for em units)
    ComputedStyleDependencies computedStyleDependencies() const;
    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    bool equals(const CSSValue&) const;
    bool operator==(const CSSValue& other) const { return equals(other); }

    // Returns false if the hash is computed from the CSSValue pointer instead of the underlying values.
    bool addHash(Hasher&) const;

    // https://www.w3.org/TR/css-values-4/#local-urls
    // Empty URLs and fragment-only URLs should not be resolved relative to the base URL.
    static bool isCSSLocalURL(StringView relativeURL);

    enum StaticCSSValueTag { StaticCSSValue };

    static constexpr size_t ValueSeparatorBits = 2;
    enum ValueSeparator : uint8_t { SpaceSeparator, CommaSeparator, SlashSeparator };

    inline bool isColor() const;
    inline const Color& color() const;

    inline bool isCustomIdent() const;
    inline String customIdent() const;

    inline bool isInteger() const;
    inline int integer() const;

    inline const CSSValue& first() const; // CSSValuePair
    Ref<CSSValue> protectedFirst() const; // CSSValuePair
    inline const CSSValue& second() const; // CSSValuePair
    Ref<CSSValue> protectedSecond() const; // CSSValuePair
    inline const Quad& quad() const; // CSSValueQuad
    inline const Rect& rect() const; // CSSSValueRect

    // FIXME: Should these be named isIdent and ident instead?
    inline bool isValueID() const;
    inline CSSValueID valueID() const;

    void customSetReplacementURLForSubresources(const HashMap<String, String>&) { }
    void customClearReplacementURLForSubresources() { }

protected:
    static const size_t ClassTypeBits = 7;

    // FIXME: Use an enum class here so we don't have to repeat "Class" in every name.
    enum ClassType {
        PrimitiveClass,

        // Image classes.
        ImageClass,
        ImageSetOptionClass,
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
        LinearTimingFunctionClass,
        CubicBezierTimingFunctionClass,
        SpringTimingFunctionClass,
        StepsTimingFunctionClass,

        // Other non-list classes.
        AspectRatioClass,
        BackgroundRepeatClass,
        BorderImageSliceClass,
        BorderImageWidthClass,
        CalculationClass,
        CircleClass,
        ContentDistributionClass,
        CounterClass,
        CustomPropertyClass,
        EllipseClass,
        FontClass,
        FontFaceSrcLocalClass,
        FontFaceSrcResourceClass,
        FontFeatureClass,
        FontPaletteValuesOverrideColorsClass,
        FontStyleRangeClass,
        FontStyleWithAngleClass,
        FontVariantAlternatesClass,
        FontVariationClass,
        GridLineNamesClass,
        GridTemplateAreasClass,
        InsetShapeClass,
        LineBoxContainClass,
        OffsetRotateClass,
        PathClass,
        PendingSubstitutionValueClass,
        QuadClass,
        RayClass,
        RectClass,
        RectShapeClass,
        ReflectClass,
        ScrollClass,
        ShadowClass,
        UnicodeRangeClass,
        ValuePairClass,
        VariableReferenceClass,
        ViewClass,
        XywhShapeClass,

        // Classes that contain vectors, which derive from CSSValueContainingVector.
        ValueListClass,
        FunctionClass,
        GridAutoRepeatClass,
        GridIntegerRepeatClass,
        ImageSetClass,
        PolygonClass,
        SubgridClass,
        TransformListClass,
        // Do not append classes here unless they derive from CSSValueContainingVector.
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
    bool addDerivedHash(Hasher&) const;

    mutable unsigned m_refCount { refCountIncrement };

protected:
    // These data members are used by derived classes but here to maximize struct packing.

    // CSSPrimitiveValue:
    unsigned m_primitiveUnitType : 7 { 0 }; // CSSUnitType
    mutable unsigned m_hasCachedCSSText : 1 { false };
    unsigned m_isImplicitInitialValue : 1 { false };

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

void add(Hasher&, const CSSValue&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_VALUE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSValue& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
