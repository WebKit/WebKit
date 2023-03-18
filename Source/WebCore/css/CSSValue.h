/**
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

#include <wtf/Atomics.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

class CSSUnresolvedColor;
class CachedResource;
class Color;
class NumericQuad;
class Rect;
class RenderStyle;

struct Counter;
struct Length;

enum CSSPropertyID : uint16_t;
enum CSSValueID : uint16_t;

using WTF::opaque;

struct ComputedStyleDependencies {
    Vector<CSSPropertyID> properties;
    Vector<CSSPropertyID> rootProperties;
    bool containerDimensions { false };

    bool isEmpty() const { return properties.isEmpty() && rootProperties.isEmpty() && !containerDimensions; }
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSValue);
class CSSValue {
    WTF_MAKE_NONCOPYABLE(CSSValue);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSValue);

public:
    void ref() const;
    void deref() const;
    bool hasOneRef() const;
    unsigned refCount() const;
    bool hasAtLeastOneRef() const;

    String cssText() const;

    // FIXME: We should take the "Value" suffix out of most of these function names, and not include it in new ones.
    bool isAspectRatioValue() const { return type() == Type::AspectRatio; }
    bool isAttr() const { return type() == Type::Attr; }
    bool isBackgroundRepeatValue() const { return type() == Type::BackgroundRepeat; }
    bool isBorderImageSliceValue() const { return type() == Type::BorderImageSlice; }
    bool isBorderImageWidthValue() const { return type() == Type::BorderImageWidth; }
    bool isCalcValue() const { return type() == Type::Calculation; }
    bool isCanvasValue() const { return type() == Type::Canvas; }
    bool isCircle() const { return type() == Type::Circle; }
    bool isColor() const { return type() == Type::Color; }
    bool isConicGradientValue() const { return type() == Type::ConicGradient; }
    bool isContentDistributionValue() const { return type() == Type::ContentDistribution; }
    bool isCounter() const { return type() == Type::Counter; }
    bool isCounterName() const { return type() == Type::CounterName; }
    bool isCrossfadeValue() const { return type() == Type::Crossfade; }
    bool isCubicBezierTimingFunctionValue() const { return type() == Type::CubicBezierTimingFunction; }
    bool isCursorImageValue() const { return type() == Type::CursorImage; }
    bool isCustomIdent() const { return type() == Type::CustomIdent; }
    bool isCustomPropertyValue() const { return type() == Type::CustomProperty; }
    bool isDeprecatedLinearGradientValue() const { return type() == Type::DeprecatedLinearGradient; }
    bool isDeprecatedRadialGradientValue() const { return type() == Type::DeprecatedRadialGradient; }
    bool isEllipse() const { return type() == Type::Ellipse; }
    bool isFilterImageValue() const { return type() == Type::FilterImage; }
    bool isFontFaceSrcLocalValue() const { return type() == Type::FontFaceSrcLocal; }
    bool isFontFaceSrcResourceValue() const { return type() == Type::FontFaceSrcResource; }
    bool isFontFamily() const { return type() == Type::FontFamily; }
    bool isFontFeatureValue() const { return type() == Type::FontFeature; }
    bool isFontStyleRangeValue() const { return type() == Type::FontStyleRange; }
    bool isFontStyleWithAngleValue() const { return type() == Type::FontStyleWithAngle; }
    bool isFontValue() const { return type() == Type::Font; }
    bool isFontVariantAlternatesValue() const { return type() == Type::FontVariantAlternates; }
    bool isFontVariationValue() const { return type() == Type::FontVariation; }
    bool isFunctionValue() const { return type() == Type::Function; }
    bool isGridAutoRepeatValue() const { return type() == Type::GridAutoRepeat; }
    bool isGridIntegerRepeatValue() const { return type() == Type::GridIntegerRepeat; }
    bool isGridLineNamesValue() const { return type() == Type::GridLineNames; }
    bool isGridTemplateAreasValue() const { return type() == Type::GridTemplateAreas; }
    bool isIdent() const { return type() == Type::Ident; }
    bool isImageSetValue() const { return type() == Type::ImageSet; }
    bool isImageValue() const { return type() == Type::Image; }
    bool isInsetShape() const { return type() == Type::InsetShape; }
    bool isLineBoxContainValue() const { return type() == Type::LineBoxContain; }
    bool isLinearGradientValue() const { return type() == Type::LinearGradient; }
    bool isNamedImageValue() const { return type() == Type::NamedImage; }
    bool isOffsetRotateValue() const { return type() == Type::OffsetRotate; }
    bool isPair() const { return type() == Type::ValuePair; }
    bool isPath() const { return type() == Type::Path; }
    bool isPendingSubstitutionValue() const { return type() == Type::PendingSubstitutionValue; }
    bool isPolygon() const { return type() == Type::Polygon; }
    bool isPrefixedLinearGradientValue() const { return type() == Type::PrefixedLinearGradient; }
    bool isPrefixedRadialGradientValue() const { return type() == Type::PrefixedRadialGradient; }
    bool isPrimitiveValue() const { return type() == Type::Primitive; }
    bool isQuad() const { return type() == Type::Quad; }
    bool isRadialGradientValue() const { return type() == Type::RadialGradient; }
    bool isRayValue() const { return type() == Type::Ray; }
    bool isRect() const { return type() == Type::Rect; }
    bool isReflectValue() const { return type() == Type::Reflect; }
    bool isShadowValue() const { return type() == Type::Shadow; }
    bool isSpringTimingFunctionValue() const { return type() == Type::SpringTimingFunction; }
    bool isStepsTimingFunctionValue() const { return type() == Type::StepsTimingFunction; }
    bool isString() const { return type() == Type::String; }
    bool isSubgridValue() const { return type() == Type::Subgrid; }
    bool isTransformListValue() const { return type() == Type::TransformList; }
    bool isURL() const { return type() == Type::URL; }
    bool isUnicodeRangeValue() const { return type() == Type::UnicodeRange; }
    bool isUnresolvedColor() const { return type() == Type::UnresolvedColor; }
    bool isValueList() const { return type() == Type::ValueList; }
    bool isVariableReferenceValue() const { return type() == Type::VariableReference; }

#if ENABLE(CSS_PAINTING_API)
    bool isPaintImageValue() const { return type() == Type::PaintImage; }
#endif

    bool containsVector() const { return type() >= Type::ValueList; }
    bool hasVariableReferences() const { return isVariableReferenceValue() || isPendingSubstitutionValue(); }
    bool isImplicitInitialValue() const;
    bool isInImageFamily() const;
    bool isPropertyID() const;
    bool isValueID() const;

    CSSPropertyID propertyID() const;
    CSSValueID valueID() const;

    bool traverseSubresources(const Function<bool(const CachedResource&)>&) const;

    // What properties does this value rely on (eg, font-size for em units)
    ComputedStyleDependencies computedStyleDependencies() const;
    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    bool equals(const CSSValue&) const;
    bool operator==(const CSSValue& other) const { return equals(other); }

    // https://www.w3.org/TR/css-values-4/#local-urls
    // Empty URLs and fragment-only URLs should not be resolved relative to the base URL.
    static bool isCSSLocalURL(StringView relativeURL);

    enum ValueSeparator : uint8_t { SpaceSeparator, CommaSeparator, SlashSeparator };
    static constexpr uint8_t ValueSeparatorBits = 2;

    static Ref<CSSValue> create(const Length&);
    static Ref<CSSValue> create(const Length&, const RenderStyle&);

protected:
    enum class Type : uint8_t {
        AspectRatio,
        Attr,
        BackgroundRepeat,
        BorderImageSlice,
        BorderImageWidth,
        Calculation,
        Circle,
        Color,
        ContentDistribution,
        Counter,
        CounterName,
        CubicBezierTimingFunction,
        CursorImage,
        CustomIdent,
        CustomProperty,
        Ellipse,
        Font,
        FontFaceSrcLocal,
        FontFaceSrcResource,
        FontFamily,
        FontFeature,
        FontStyleRange,
        FontStyleWithAngle,
        FontVariantAlternates,
        FontVariation,
        GridLineNames,
        GridTemplateAreas,
        Ident,
        Image,
        ImageSet,
        InsetShape,
        LineBoxContain,
        OffsetRotate,
        Path,
        PendingSubstitutionValue,
        Primitive,
        Quad,
        Ray,
        Rect,
        Reflect,
        Shadow,
        SpringTimingFunction,
        StepsTimingFunction,
        String,
        URL,
        UnicodeRange,
        UnresolvedColor,
        ValuePair,
        VariableReference,

        // Image generator classes.
        // Update isInImageFamily if the first image generator class changes.
        Canvas,
        ConicGradient,
        Crossfade,
        DeprecatedLinearGradient,
        DeprecatedRadialGradient,
        FilterImage,
        LinearGradient,
        NamedImage,
#if ENABLE(CSS_PAINTING_API)
        PaintImage,
#endif
        PrefixedLinearGradient,
        PrefixedRadialGradient,
        RadialGradient,
        // Update isInImageFamily if the last image generator class changes.

        // Classes that contain vectors, which derive from CSSValueContainingVector.
        // Keep ValueList as the first of these.
        ValueList,
        Function,
        GridAutoRepeat,
        GridIntegerRepeat,
        Polygon,
        Subgrid,
        TransformList,
        // Do not append classes here unless they derive from CSSValueContainingVector.
    };
    static constexpr auto LastType = Type::TransformList;

    explicit CSSValue(Type);
    Type type() const;

    // This class does not use virtual functions for memory and performance reasons.
    ~CSSValue() = default;
    WEBCORE_EXPORT void operator delete(CSSValue*, std::destroying_delete_t);

    ValueSeparator separator() const;
    static ASCIILiteral separatorCSSText(ValueSeparator);
    ASCIILiteral separatorCSSText() const { return separatorCSSText(separator()); };

private:
    // For each type of CSS value, there can be "scalar values", where the value is stored
    // in the bits of what otherwise would be the pointer, and objects, where the value
    // is stored in a block on the heap. HasScalarBit, the low bit, is used to tell a pointer
    // from one of the scalars. Each of the CSSValue derived classes that takes advantage
    // of this scheme has to call opaque() on the pointer before getting at any data member.
    // Functions that use the pointer as a scalar also need to call opaque(), but for the
    // most part that's taken care of because they call uncheckedScalar().

    // The low bit is used for the flag that indicates the CSSValue is not a pointer.
    // If it's set, then the next 7 bits are used for the type. Then bits beyond those low
    // 8 bits are used by the various classes for each type to store type-specific values.

    // Encode scalar values in the pointer.
    static constexpr uintptr_t HasScalarBit = 1;
    static constexpr uint8_t TypeShift = 1;
    static constexpr uint8_t TypeBits = 7;
    static constexpr uintptr_t TypeMask = (1 << TypeBits) - 1;

protected:
    uintptr_t uncheckedScalar() const { return opaque(reinterpret_cast<uintptr_t>(this)); }
    bool hasScalarInPointer() const { return uncheckedScalar() & HasScalarBit; }
    static bool eitherHasScalarInPointer(const CSSValue&, const CSSValue&);
    uintptr_t scalar() const { ASSERT(hasScalarInPointer()); return uncheckedScalar(); }
    static constexpr uintptr_t typeScalar(Type type) { return HasScalarBit | static_cast<uintptr_t>(type) << TypeShift; }
    static constexpr uintptr_t TypeScalarMask = HasScalarBit | TypeMask;
    static constexpr uint8_t PayloadShift = TypeShift + TypeBits;

    // For CSSIdentValue, CSSValuePair, and CSSValueList. Bits to store a CSSValueID.
    static constexpr uint8_t IdentBits = 11;
    static constexpr uintptr_t IdentMask = (1 << IdentBits) - 1;

    // For CSSIdentValue. Defined here so valueID(), ==(CSSValueID) and more can all be in this header.
    static constexpr uintptr_t IsPropertyIDBit = 1 << PayloadShift;
    static constexpr uintptr_t IsImplicitInitialValueBit = 1 << (PayloadShift + 1);
    static constexpr uint8_t IdentShift = PayloadShift + 2;
    static constexpr uintptr_t valueIDScalar(CSSValueID);

private:
    friend bool operator==(const CSSValue&, CSSValueID);

    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&);
    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&) const;

    static inline bool customTraverseSubresources(const Function<bool(const CachedResource&)>&);

    mutable unsigned m_refCount { 1 };
    unsigned m_type : TypeBits { }; // Type

protected:
    // These data members are used by derived classes but here to take advantage of struct packing.

    // CSSPrimitiveValue:
    static constexpr uint8_t UnitBits = 7;
    static constexpr uintptr_t UnitMask = (1 << UnitBits) - 1;
    unsigned m_unit : UnitBits { };
    mutable unsigned m_hasCachedCSSText : 1 { };

    // CSSValueList and CSSValuePair:
    unsigned m_valueSeparator : ValueSeparatorBits { };

    // CSSValuePair:
    unsigned m_isNoncoalescing : 1 { };
};

bool operator==(const CSSValue&, CSSValueID);
bool operator==(const CSSValue*, CSSValueID);
bool operator==(const RefPtr<CSSValue>&, CSSValueID);

inline bool operator!=(const CSSValue& a, CSSValueID b) { return !(a == b); }
inline bool operator!=(const CSSValue* a, CSSValueID b) { return !(a == b); }
inline bool operator!=(const RefPtr<CSSValue>& a, CSSValueID b) { return !(a == b); }

CSSValueID valueID(const CSSValue*);

template<typename Type> bool compareCSSValue(const Type&, const Type&);
template<typename Type> bool compareCSSValuePtr(const Type&, const Type&);
template<typename Type> bool compareCSSValueVector(const Vector<Type>&, const Vector<Type>&);

inline CSSValue::CSSValue(Type type)
    : m_type(static_cast<uint8_t>(type))
{
}

inline bool CSSValue::eitherHasScalarInPointer(const CSSValue& a, const CSSValue& b)
{
    return (a.uncheckedScalar() | b.uncheckedScalar()) & HasScalarBit;
}

inline void CSSValue::ref() const
{
    if (hasScalarInPointer())
        return;
    ++opaque(this)->m_refCount;
}

inline void CSSValue::deref() const
{
    if (hasScalarInPointer())
        return;
    auto& self = *opaque(this);
    unsigned refCount = self.m_refCount - 1;
    if (!refCount) {
        delete &self;
        return;
    }
    self.m_refCount = refCount;
}

inline bool CSSValue::hasOneRef() const
{
    if (hasScalarInPointer())
        return false;
    return opaque(this)->m_refCount == 1;
}

inline unsigned CSSValue::refCount() const
{
    if (hasScalarInPointer())
        return std::numeric_limits<unsigned>::max();
    return opaque(this)->m_refCount;
}

inline bool CSSValue::hasAtLeastOneRef() const
{
    if (hasScalarInPointer())
        return true;
    return opaque(this)->m_refCount;
}

inline auto CSSValue::type() const -> Type
{
    if (hasScalarInPointer())
        return static_cast<Type>(scalar() >> TypeShift & TypeMask);
    return static_cast<Type>(opaque(this)->m_type);
}

inline bool CSSValue::isPropertyID() const
{
    return (uncheckedScalar() & (TypeScalarMask | IsPropertyIDBit)) == (typeScalar(Type::Ident) | IsPropertyIDBit);
}

inline CSSPropertyID CSSValue::propertyID() const
{
    ASSERT(isPropertyID());
    return static_cast<CSSPropertyID>(scalar() >> IdentShift & IdentMask);
}

inline bool CSSValue::isValueID() const
{
    return (uncheckedScalar() & (TypeScalarMask | IsPropertyIDBit)) == typeScalar(Type::Ident);
}

inline CSSValueID CSSValue::valueID() const
{
    return static_cast<CSSValueID>(isValueID() ? scalar() >> IdentShift & IdentMask : 0);
}

inline bool CSSValue::isImplicitInitialValue() const
{
    return (uncheckedScalar() & (TypeScalarMask | IsImplicitInitialValueBit)) == (typeScalar(Type::Ident) | IsImplicitInitialValueBit);
}

inline bool CSSValue::isInImageFamily() const
{
    // This intentionally omits CursorImage; corresponds to the CSS <image> production.
    constexpr auto firstImageGenerator = Type::Canvas;
    constexpr auto lastImageGenerator = Type::RadialGradient;
    auto type = this->type();
    return type == Type::Image || type == Type::ImageSet || (type >= firstImageGenerator && type <= lastImageGenerator);
}

constexpr uintptr_t CSSValue::valueIDScalar(CSSValueID valueID)
{
    ASSERT(valueID);
    return typeScalar(Type::Ident) | static_cast<uintptr_t>(valueID) << IdentShift;
}

inline auto CSSValue::separator() const -> ValueSeparator
{
    if (hasScalarInPointer())
        return SpaceSeparator;
    return static_cast<ValueSeparator>(opaque(this)->m_valueSeparator);
}

inline bool operator==(const CSSValue& value, CSSValueID valueID)
{
    return value.uncheckedScalar() == CSSValue::valueIDScalar(valueID);
}

inline bool operator==(const CSSValue* value, CSSValueID valueID)
{
    return value && *value == valueID;
}

inline bool operator==(const RefPtr<CSSValue>& value, CSSValueID valueID)
{
    return value && *value == valueID;
}

template<typename Type> inline bool compareCSSValuePtr(const Type& a, const Type& b)
{
    return a == b || (a && b && a->equals(*b));
}

template<typename Type> inline bool compareCSSValue(const Type& a, const Type& b)
{
    return &a == &b || a.get().equals(b);
}

template<typename Type> bool compareCSSValueVector(const Vector<Type>& a, const Vector<Type>& b)
{
    size_t size = a.size();
    if (size != b.size())
        return false;
    for (size_t i = 0; i < size; ++i) {
        if (!compareCSSValuePtr(a[i], b[i]))
            return false;
    }
    return true;
}

inline CSSValueID valueID(const CSSValue* value)
{
    return value ? value->valueID() : CSSValueID { };
}

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_VALUE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSValue& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
