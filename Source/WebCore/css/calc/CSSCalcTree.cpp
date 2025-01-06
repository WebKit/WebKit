/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcTree.h"

#include "CSSCalcTree+Serialization.h"
#include "CSSUnits.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSSCalc {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Abs);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Acos);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Anchor);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(AnchorSize);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Asin);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Atan);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Atan2);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Clamp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(ContainerProgress);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Cos);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Exp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Hypot);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Invert);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Log);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Max);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(MediaProgress);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Min);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Mod);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Negate);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Pow);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Product);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Progress);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Rem);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundDown);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundNearest);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundToZero);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundUp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sign);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sin);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sqrt);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sum);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Tan);

Child makeNumeric(double value, CSSUnitType unit)
{
    switch (unit) {
    // Number
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
        return makeChild(Number { .value = value });

    // Percentage
    case CSSUnitType::CSS_PERCENTAGE:
        return makeChild(Percentage { .value = value, .hint = { } });

    // Canonical Dimension
    case CSSUnitType::CSS_PX:
        return makeChild(CanonicalDimension { .value = value, .dimension = CanonicalDimension::Dimension::Length });
    case CSSUnitType::CSS_DEG:
        return makeChild(CanonicalDimension { .value = value, .dimension = CanonicalDimension::Dimension::Angle });
    case CSSUnitType::CSS_S:
        return makeChild(CanonicalDimension { .value = value, .dimension = CanonicalDimension::Dimension::Time });
    case CSSUnitType::CSS_HZ:
        return makeChild(CanonicalDimension { .value = value, .dimension = CanonicalDimension::Dimension::Frequency });
    case CSSUnitType::CSS_DPPX:
        return makeChild(CanonicalDimension { .value = value, .dimension = CanonicalDimension::Dimension::Resolution });
    case CSSUnitType::CSS_FR:
        return makeChild(CanonicalDimension { .value = value, .dimension = CanonicalDimension::Dimension::Flex });

    // <length>
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
    // <angle>
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
    // <time>
    case CSSUnitType::CSS_MS:
    // <frequency>
    case CSSUnitType::CSS_KHZ:
    // <resolution>
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
        return makeChild(NonCanonicalDimension { .value = value, .unit = unit });

    // Non-numeric types are not supported.
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CustomIdent:
        break;
    }

    ASSERT_NOT_REACHED();
    return makeChild(Number { .value = 0 });
}

Type getType(CanonicalDimension::Dimension dimension)
{
    switch (dimension) {
    case CanonicalDimension::Dimension::Length:         return Type { .length = 1 };
    case CanonicalDimension::Dimension::Angle:          return Type { .angle = 1 };
    case CanonicalDimension::Dimension::Time:           return Type { .time = 1 };
    case CanonicalDimension::Dimension::Frequency:      return Type { .frequency = 1 };
    case CanonicalDimension::Dimension::Resolution:     return Type { .resolution = 1 };
    case CanonicalDimension::Dimension::Flex:           return Type { .flex = 1 };
    }

    ASSERT_NOT_REACHED();
    return Type { };
}

Type getType(const Number&)
{
    return Type { };
}

Type getType(const Percentage& root)
{
    auto type = Type { .percent = 1 };
    if (root.hint)
        type.applyPercentHint(*root.hint);
    return type;
}

Type getType(const CanonicalDimension& root)
{
    return getType(root.dimension);
}

Type getType(const NonCanonicalDimension& root)
{
    return Type::determineType(toCSSUnit(root));
}

Type getType(const Symbol& root)
{
    return Type::determineType(root.unit);
}

Type getType(const Child& child)
{
    return WTF::switchOn(child, [&](const auto& root) { return getType(root); });
}

std::optional<Type> toType(const Sum& root)
{
    std::optional<Type> type = getType(root.children[0]);
    for (size_t i = 1; i < root.children.size(); ++i)
        type = Type::add(type, getType(root.children[i]));
    return type;
}

std::optional<Type> toType(const Product& root)
{
    std::optional<Type> type = getType(root.children[0]);
    for (size_t i = 1; i < root.children.size(); ++i)
        type = Type::multiply(type, getType(root.children[i]));
    return type;
}

std::optional<Type> toType(const Negate& root)
{
    return getType(root.a);
}

std::optional<Type> toType(const Invert& root)
{
    return Type::invert(getType(root.a));
}

// Utilities to deduce the right input/merge/output policies from the operation.

template<typename Op> static std::optional<Type> getValidatedTypeFor(const Op&, const Child& child)
{
    auto type = getType(child);
    if (validateType<Op::input>(type))
        return type;
    return std::nullopt;
}

template<typename Op> static std::optional<Type> mergeTypesFor(const Op&, std::optional<Type> a, std::optional<Type> b)
{
    return mergeTypes<Op::merge>(a, b);
}

template<typename Op, typename... Args> static std::optional<Type> transformTypeFor(const Op&, std::optional<Type> a)
{
    return transformType<Op::output>(a);
}

std::optional<Type> toType(const Min& root)
{
    auto type = getValidatedTypeFor(root, root.children[0]);
    for (size_t i = 1; i < root.children.size(); ++i)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, root.children[i]));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const Max& root)
{
    auto type = getValidatedTypeFor(root, root.children[0]);
    for (size_t i = 1; i < root.children.size(); ++i)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, root.children[i]));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const Clamp& root)
{
    auto type = getValidatedTypeFor(root, root.val);
    if (std::holds_alternative<Child>(root.min))
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, std::get<Child>(root.min)));
    if (std::holds_alternative<Child>(root.max))
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, std::get<Child>(root.max)));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const RoundNearest& root)
{
    auto type = getValidatedTypeFor(root, root.a);
    if (root.b)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, *root.b));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const RoundUp& root)
{
    auto type = getValidatedTypeFor(root, root.a);
    if (root.b)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, *root.b));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const RoundDown& root)
{
    auto type = getValidatedTypeFor(root, root.a);
    if (root.b)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, *root.b));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const RoundToZero& root)
{
    auto type = getValidatedTypeFor(root, root.a);
    if (root.b)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, *root.b));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const Mod& root)
{
    return transformTypeFor(root, mergeTypesFor(root, getValidatedTypeFor(root, root.a), getValidatedTypeFor(root, root.b)));
}

std::optional<Type> toType(const Rem& root)
{
    return transformTypeFor(root, mergeTypesFor(root, getValidatedTypeFor(root, root.a), getValidatedTypeFor(root, root.b)));
}

std::optional<Type> toType(const Sin& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Cos& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Tan& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Asin& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Acos& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Atan& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Atan2& root)
{
    return transformTypeFor(root, mergeTypesFor(root, getValidatedTypeFor(root, root.a), getValidatedTypeFor(root, root.b)));
}

std::optional<Type> toType(const Pow& root)
{
    return transformTypeFor(root, mergeTypesFor(root, getValidatedTypeFor(root, root.a), getValidatedTypeFor(root, root.b)));
}

std::optional<Type> toType(const Sqrt& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Hypot& root)
{
    auto type = getValidatedTypeFor(root, root.children[0]);
    for (size_t i = 1; i < root.children.size(); ++i)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, root.children[i]));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const Log& root)
{
    auto type = getValidatedTypeFor(root, root.a);
    if (root.b)
        type = mergeTypesFor(root, type, getValidatedTypeFor(root, *root.b));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const Exp& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Abs& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Sign& root)
{
    return transformTypeFor(root, getValidatedTypeFor(root, root.a));
}

std::optional<Type> toType(const Progress& root)
{
    auto type = getValidatedTypeFor(root, root.value);
    type = mergeTypesFor(root, type, getValidatedTypeFor(root, root.start));
    type = mergeTypesFor(root, type, getValidatedTypeFor(root, root.end));
    return transformTypeFor(root, type);
}

std::optional<Type> toType(const MediaProgress&)
{
    // `media-progress()` always has type `number`.
    return Type { };
}

std::optional<Type> toType(const ContainerProgress&)
{
    // `container-progress()` always has type `number`.
    return Type { };
}

TextStream& operator<<(TextStream& ts, Tree tree)
{
    return ts << "CSSCalc::Tree [ " << serializationForCSS(tree) << " ]";
}

} // namespace CSSCalc
} // namespace WebCore
