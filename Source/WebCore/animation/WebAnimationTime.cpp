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
#include "WebAnimationTime.h"

#include "CSSNumericFactory.h"
#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

WebAnimationTime::WebAnimationTime(std::optional<Seconds> time, std::optional<double> percentage)
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

WebAnimationTime::WebAnimationTime(const Seconds& value)
    : m_type(Type::Time)
    , m_value(value.seconds())
{
}

WebAnimationTime::WebAnimationTime(Type type, double value)
    : m_type(type)
    , m_value(value)
{
}

WebAnimationTime::WebAnimationTime(const CSSNumberish& value)
{
    if (auto* doubleValue = std::get_if<double>(&value)) {
        m_type = Type::Time;
        m_value = *doubleValue / 1000;
        return;
    }

    ASSERT(std::holds_alternative<RefPtr<CSSNumericValue>>(value));
    auto numericValue = std::get<RefPtr<CSSNumericValue>>(value);
    if (RefPtr unitValue = dynamicDowncast<CSSUnitValue>(numericValue.get())) {
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

WebAnimationTime WebAnimationTime::fromMilliseconds(double milliseconds)
{
    return { Type::Time, milliseconds / 1000 };
}

WebAnimationTime WebAnimationTime::fromPercentage(double percentage)
{
    return { Type::Percentage, percentage };
}

std::optional<Seconds> WebAnimationTime::time() const
{
    if (m_type == Type::Time)
        return Seconds { m_value };
    return std::nullopt;
}

std::optional<double> WebAnimationTime::percentage() const
{
    if (m_type == Type::Percentage)
        return m_value;
    return std::nullopt;
}

bool WebAnimationTime::isValid() const
{
    return m_type != Type::Unknown;
}

bool WebAnimationTime::isInfinity() const
{
    return std::isinf(m_value);
}

bool WebAnimationTime::isZero() const
{
    return !m_value;
}

WebAnimationTime WebAnimationTime::matchingZero() const
{
    return { m_type, 0 };
}

WebAnimationTime WebAnimationTime::matchingEpsilon() const
{
    if (m_type == Type::Percentage)
        return WebAnimationTime::fromPercentage(0.000001);
    return { WebCore::timeEpsilon };
}

bool WebAnimationTime::approximatelyEqualTo(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    if (m_type == Type::Time)
        return std::abs(time()->microseconds() - other.time()->microseconds()) < timeEpsilon.microseconds();
    return m_value == other.m_value;
}

bool WebAnimationTime::approximatelyLessThan(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    if (m_type == Type::Time)
        return (*time() + timeEpsilon) < *other.time();
    return m_value < other.m_value;
}

bool WebAnimationTime::approximatelyGreaterThan(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    if (m_type == Type::Time)
        return (*time() - timeEpsilon) > *other.time();
    return m_value > other.m_value;
}

WebAnimationTime WebAnimationTime::operator+(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    return { m_type, m_value + other.m_value };
}

WebAnimationTime WebAnimationTime::operator-(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    return { m_type, m_value - other.m_value };
}

double WebAnimationTime::operator/(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value / other.m_value;
}

WebAnimationTime& WebAnimationTime::operator+=(const WebAnimationTime& other)
{
    ASSERT(m_type == other.m_type);
    m_value += other.m_value;
    return *this;
}

WebAnimationTime& WebAnimationTime::operator-=(const WebAnimationTime& other)
{
    ASSERT(m_type == other.m_type);
    m_value -= other.m_value;
    return *this;
}

bool WebAnimationTime::operator<(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value < other.m_value;
}

bool WebAnimationTime::operator<=(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value <= other.m_value;
}

bool WebAnimationTime::operator>(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value > other.m_value;
}

bool WebAnimationTime::operator>=(const WebAnimationTime& other) const
{
    ASSERT(m_type == other.m_type);
    return m_value >= other.m_value;
}

bool WebAnimationTime::operator==(const WebAnimationTime& other) const
{
    return m_type == other.m_type && m_value == other.m_value;
}

WebAnimationTime WebAnimationTime::operator+(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return { m_type, m_value + other.seconds() };
}

WebAnimationTime WebAnimationTime::operator-(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return { m_type, m_value - other.seconds() };
}

bool WebAnimationTime::operator<(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value < other.seconds();
}

bool WebAnimationTime::operator<=(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value <= other.seconds();
}

bool WebAnimationTime::operator>(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value > other.seconds();
}

bool WebAnimationTime::operator>=(const Seconds& other) const
{
    ASSERT(m_type == Type::Time);
    return m_value >= other.seconds();
}

bool WebAnimationTime::operator==(const Seconds& other) const
{
    return m_type == Type::Time && m_value == other.seconds();
}

WebAnimationTime WebAnimationTime::operator*(double scalar) const
{
    return { m_type, m_value * scalar };
}

WebAnimationTime WebAnimationTime::operator/(double scalar) const
{
    return { m_type, m_value / scalar };
}

WebAnimationTime::operator Seconds() const
{
    ASSERT(m_type == Type::Time);
    return Seconds(m_value);
}

WebAnimationTime::operator CSSNumberish() const
{
    if (m_type == Type::Time)
        return secondsToWebAnimationsAPITime(*this);
    ASSERT(m_type == Type::Percentage);
    return CSSNumericFactory::percent(m_value);
}

void WebAnimationTime::dump(TextStream& ts) const
{
    if (m_type == Type::Time) {
        ts << m_value * 1000;
        return;
    }
    ASSERT(m_type == Type::Percentage);
    ts << m_value << "%";
    return;
}

TextStream& operator<<(TextStream& ts, const WebAnimationTime& value)
{
    value.dump(ts);
    return ts;
}

} // namespace WebCore
