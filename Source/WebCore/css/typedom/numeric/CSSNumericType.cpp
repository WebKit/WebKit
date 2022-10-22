/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "CSSNumericType.h"
#include "CSSNumericValue.h"
#include "CSSUnits.h"

namespace WebCore {

std::optional<CSSNumericType> CSSNumericType::create(CSSUnitType unit, int exponent)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssnumericvalue-create-a-type
    CSSNumericType type;
    switch (unitCategory(unit)) {
    case CSSUnitCategory::Number:
        return { WTFMove(type) };
    case CSSUnitCategory::Percent:
        type.percent = exponent;
        return { WTFMove(type) };
    case CSSUnitCategory::AbsoluteLength:
    case CSSUnitCategory::FontRelativeLength:
    case CSSUnitCategory::ViewportPercentageLength:
        type.length = exponent;
        return { WTFMove(type) };
    case CSSUnitCategory::Angle:
        type.angle = exponent;
        return { WTFMove(type) };
    case CSSUnitCategory::Time:
        type.time = exponent;
        return { WTFMove(type) };
    case CSSUnitCategory::Frequency:
        type.frequency = exponent;
        return { WTFMove(type) };
    case CSSUnitCategory::Resolution:
        type.resolution = exponent;
        return { WTFMove(type) };
    case CSSUnitCategory::Flex:
        type.flex = exponent;
        return { WTFMove(type) };
    case CSSUnitCategory::Other:
        break;
    }
    
    return std::nullopt;
}

std::optional<CSSNumericType> CSSNumericType::addTypes(CSSNumericType a, CSSNumericType b)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssnumericvalue-add-two-types
    if (a.percentHint && b.percentHint && *a.percentHint != *b.percentHint)
        return std::nullopt;

    if (a.percentHint)
        b.applyPercentHint(*a.percentHint);
    if (b.percentHint)
        a.applyPercentHint(*b.percentHint);

    if (a == b)
        return { WTFMove(a) };

    for (auto type : eachBaseType()) {
        if (type == CSSNumericBaseType::Percent)
            continue;
        if (!a.valueForType(type) && !b.valueForType(type))
            continue;
        a.applyPercentHint(type);
        b.applyPercentHint(type);
        if (a.valueForType(type) != b.valueForType(type))
            return std::nullopt;
    }

    return { WTFMove(a) };
}

template<typename Argument> std::optional<CSSNumericType> typeFromVector(const Vector<Ref<CSSNumericValue>>& values, std::optional<CSSNumericType>(*function)(Argument, Argument))
{
    if (values.isEmpty())
        return std::nullopt;
    std::optional<CSSNumericType> type = values[0]->type();
    for (size_t i = 1; i < values.size(); ++i) {
        type = function(*type, values[i]->type());
        if (!type)
            return std::nullopt;
    }
    return type;
}

std::optional<CSSNumericType> CSSNumericType::addTypes(const Vector<Ref<CSSNumericValue>>& values)
{
    return typeFromVector(values, addTypes);
}

std::optional<CSSNumericType> CSSNumericType::multiplyTypes(const CSSNumericType& a, const CSSNumericType& b)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssnumericvalue-multiply-two-types
    if (a.percentHint && b.percentHint && *a.percentHint != *b.percentHint)
        return std::nullopt;

    auto add = [] (auto left, auto right) -> CSSNumericType::BaseTypeStorage {
        if (!left)
            return right;
        if (!right)
            return left;
        return *left + *right;
    };
    
    return { {
        add(a.length, b.length),
        add(a.angle, b.angle),
        add(a.time, b.time),
        add(a.frequency, b.frequency),
        add(a.resolution, b.resolution),
        add(a.flex, b.flex),
        add(a.percent, b.percent),
        a.percentHint ? a.percentHint : b.percentHint
    } };
}

std::optional<CSSNumericType> CSSNumericType::multiplyTypes(const Vector<Ref<CSSNumericValue>>& values)
{
    return typeFromVector(values, multiplyTypes);
}

String CSSNumericType::debugString() const
{
    return makeString("{",
        length ? makeString(" length:", *length) : String(),
        angle ? makeString(" angle:", *angle) : String(),
        time ? makeString(" time:", *time) : String(),
        frequency ? makeString(" frequency:", *frequency) : String(),
        resolution ? makeString(" resolution:", *resolution) : String(),
        flex ? makeString(" flex:", *flex) : String(),
        percent ? makeString(" percent:", *percent) : String(),
        percentHint ? makeString(" percentHint:", WebCore::debugString(*percentHint)) : String(),
    " }");
}

auto CSSNumericType::valueForType(CSSNumericBaseType type) -> BaseTypeStorage&
{
    switch (type) {
    case CSSNumericBaseType::Length:
        return length;
    case CSSNumericBaseType::Angle:
        return angle;
    case CSSNumericBaseType::Time:
        return time;
    case CSSNumericBaseType::Frequency:
        return frequency;
    case CSSNumericBaseType::Resolution:
        return resolution;
    case CSSNumericBaseType::Flex:
        return flex;
    case CSSNumericBaseType::Percent:
        return percent;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool CSSNumericType::operator==(const CSSNumericType& other) const
{
    return length == other.length
        && angle == other.angle
        && time == other.time
        && frequency == other.frequency
        && resolution == other.resolution
        && flex == other.flex
        && percent == other.percent
        && percentHint == other.percentHint;
}

void CSSNumericType::applyPercentHint(CSSNumericBaseType hint)
{
    // https://drafts.css-houdini.org/css-typed-om/#apply-the-percent-hint
    auto& optional = valueForType(hint);
    if (!optional)
        optional = 0;
    if (percent)
        *optional += *std::exchange(percent, 0);
    percentHint = hint;
}

size_t CSSNumericType::nonZeroEntryCount() const
{
    size_t count { 0 };
    count += length && *length;
    count += angle && *angle;
    count += time && *time;
    count += frequency && *frequency;
    count += resolution && *resolution;
    count += flex && *flex;
    count += percent && *percent;
    return count;
}

} // namespace WebCore
