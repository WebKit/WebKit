/*
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSNumberishTime.h"

#include "CSSNumericFactory.h"
#include "CSSUnitValue.h"
#include "CSSUnits.h"

namespace WebCore {

CSSNumberishTime::CSSNumberishTime(std::optional<Seconds> time, std::optional<double> percentage)
{
    ASSERT(time || percentage);
    ASSERT(!!time != !!percentage);
    if (time) {
        m_type = Type::Time;
        m_value = time->seconds();
    } else {
        m_type = Type::Percentage;
        m_value = *percentage;
    }
}

CSSNumberishTime::CSSNumberishTime(const Seconds& value)
    : m_type(Type::Time)
    , m_value(value.seconds())
{
}

CSSNumberishTime::CSSNumberishTime(Type type, double value)
    : m_type(type)
    , m_value(value)
{
}

CSSNumberishTime::CSSNumberishTime(const CSSNumberish& value)
{
    if (auto* doubleValue = std::get_if<double>(&value)) {
        m_type = Type::Time;
        m_value = *doubleValue / 1000;
        return;
    }

    ASSERT(std::holds_alternative<RefPtr<CSSNumericValue>>(value));
    auto numericValue = std::get<RefPtr<CSSNumericValue>>(value);
    if (auto* unitValue = dynamicDowncast<CSSUnitValue>(numericValue.get())) {
        if (unitValue->unitEnum() == CSSUnitType::CSS_NUMBER) {
            m_type = Type::Time;
            m_value = unitValue->value() / 1000;
        } else if (auto milliseconds = unitValue->convertTo(CSSUnitType::CSS_MS)) {
            m_type = Type::Time;
            m_value = milliseconds->value() / 1000;
        } else if (auto seconds = unitValue->convertTo(CSSUnitType::CSS_S)) {
            m_type = Type::Time;
            m_value = seconds->value();
        } else if (auto percentage = unitValue->convertTo(CSSUnitType::CSS_PERCENTAGE)) {
            m_type = Type::Percentage;
            m_value = percentage->value();
        }
    }
}

CSSNumberishTime CSSNumberishTime::fromMilliseconds(double milliseconds)
{
    return { Type::Time, milliseconds / 1000 };
}

CSSNumberishTime CSSNumberishTime::fromPercentage(double percentage)
{
    return { Type::Percentage, percentage };
}

std::optional<Seconds> CSSNumberishTime::time() const
{
    if (m_type == Type::Time)
        return Seconds { m_value };
    return std::nullopt;
}

std::optional<double> CSSNumberishTime::percentage() const
{
    if (m_type == Type::Percentage)
        return m_value;
    return std::nullopt;
}

bool CSSNumberishTime::isValid() const
{
    // FIXME: We should consider a number valid as long as it's not "Unknown"
    // but currently only mark time values as valid until we can do per-timeline
    // validation when processing a value through the JS APIs that consume them.
    return m_type == Type::Time;
}

bool CSSNumberishTime::isInfinity() const
{
    return std::isinf(m_value);
}

bool CSSNumberishTime::isZero() const
{
    return !m_value;
}

CSSNumberishTime CSSNumberishTime::matchingZero() const
{
    return { m_type, 0 };
}

bool CSSNumberishTime::approximatelyEqualTo(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    if (m_type == Type::Time)
        return std::abs(time()->microseconds() - other.time()->microseconds()) < timeEpsilon.microseconds();
    return m_value == other.m_value;
}

bool CSSNumberishTime::approximatelyLessThan(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    if (m_type == Type::Time)
        return (*time() + timeEpsilon) < *other.time();
    return m_value < other.m_value;
}

bool CSSNumberishTime::approximatelyGreaterThan(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    if (m_type == Type::Time)
        return (*time() - timeEpsilon) > *other.time();
    return m_value > other.m_value;
}

CSSNumberishTime CSSNumberishTime::operator+(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    return { m_type, m_value + other.m_value };
}

CSSNumberishTime CSSNumberishTime::operator-(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    return { m_type, m_value - other.m_value };
}

double CSSNumberishTime::operator/(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value / other.m_value;
}

CSSNumberishTime& CSSNumberishTime::operator+=(const CSSNumberishTime& other)
{
    ASSERT(m_type == other.m_type);
    m_value += other.m_value;
    return *this;
}

CSSNumberishTime& CSSNumberishTime::operator-=(const CSSNumberishTime& other)
{
    ASSERT(m_type == other.m_type);
    m_value -= other.m_value;
    return *this;
}

bool CSSNumberishTime::operator<(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value < other.m_value;
}

bool CSSNumberishTime::operator<=(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value <= other.m_value;
}

bool CSSNumberishTime::operator>(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value > other.m_value;
}

bool CSSNumberishTime::operator>=(const CSSNumberishTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value >= other.m_value;
}

bool CSSNumberishTime::operator==(const CSSNumberishTime& other) const
{
    return m_type == other.m_type && m_value == other.m_value;
}

CSSNumberishTime CSSNumberishTime::operator+(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return { m_type, m_value + other.seconds() };
}

CSSNumberishTime CSSNumberishTime::operator-(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return { m_type, m_value - other.seconds() };
}

bool CSSNumberishTime::operator<(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value < other.seconds();
}

bool CSSNumberishTime::operator<=(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value <= other.seconds();
}

bool CSSNumberishTime::operator>(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value > other.seconds();
}

bool CSSNumberishTime::operator>=(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value >= other.seconds();
}

bool CSSNumberishTime::operator==(const Seconds& other) const
{
    return m_type == Type::Time && m_value == other.seconds();
}

CSSNumberishTime CSSNumberishTime::operator*(double scalar) const
{
    return { m_type, m_value * scalar };
}

CSSNumberishTime CSSNumberishTime::operator/(double scalar) const
{
    return { m_type, m_value / scalar };
}

CSSNumberishTime::operator Seconds() const
{
    ASSERT(m_type == Type::Time);
    return Seconds(m_value);
}

CSSNumberishTime::operator CSSNumberish() const
{
    if (m_type == Type::Time)
        return secondsToWebAnimationsAPITime(*this);
    ASSERT(m_type == Type::Percentage);
    return CSSNumericFactory::percent(m_value);
}

void CSSNumberishTime::dump(TextStream& ts) const
{
    if (m_type == Type::Time) {
        ts << m_value * 1000;
        return;
    }
    ASSERT(m_type == Type::Percentage);
    ts << m_value << "%";
    return;
}

TextStream& operator<<(TextStream& ts, const CSSNumberishTime& value)
{
    value.dump(ts);
    return ts;
}

} // namespace WebCore
