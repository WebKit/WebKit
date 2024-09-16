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

CSSNumberishTime::CSSNumberishTime(double value)
    : m_type(Type::Time)
    , m_source(Source::Literal)
    , m_value(value / 1000)
{
}

CSSNumberishTime::CSSNumberishTime(Type type, Source source, double value)
    : m_type(type)
    , m_source(source)
    , m_value(value)
{
}

CSSNumberishTime::CSSNumberishTime(CSSNumberish value)
{
    if (auto* doubleValue = std::get_if<double>(&value)) {
        m_type = Type::Time;
        m_source = Source::Literal;
        m_value = *doubleValue / 1000;
        return;
    }

    ASSERT(std::holds_alternative<RefPtr<CSSNumericValue>>(value));
    auto numericValue = std::get<RefPtr<CSSNumericValue>>(value);
    if (auto* unitValue = dynamicDowncast<CSSUnitValue>(numericValue.get())) {
        if (unitValue->unitEnum() == CSSUnitType::CSS_NUMBER) {
            m_type = Type::Time;
            m_source = Source::Number;
            m_value = unitValue->value() / 1000;
        } else if (auto milliseconds = unitValue->convertTo(CSSUnitType::CSS_MS)) {
            m_type = Type::Time;
            m_source = Source::Milliseconds;
            m_value = milliseconds->value() / 1000;
        } else if (auto seconds = unitValue->convertTo(CSSUnitType::CSS_S)) {
            m_type = Type::Time;
            m_source = Source::Seconds;
            m_value = seconds->value();
        } else if (auto percentage = unitValue->convertTo(CSSUnitType::CSS_PERCENTAGE)) {
            m_type = Type::Percentage;
            m_source = Source::Percentage;
            m_value = percentage->value();
        }
    }
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
    return m_type != Type::Unknown;
}

CSSNumberishTime CSSNumberishTime::operator+(CSSNumberishTime other) const
{
    ASSERT(m_type == other.m_type);
    return { m_type, m_source, m_value + other.m_value };
}

CSSNumberishTime CSSNumberishTime::operator-(CSSNumberishTime other) const
{
    ASSERT(m_type == other.m_type);
    return { m_type, m_source, m_value - other.m_value };
}

bool CSSNumberishTime::operator<(CSSNumberishTime other) const
{
    ASSERT(m_type == other.m_type);
    return m_value < other.m_value;
}

bool CSSNumberishTime::operator<=(CSSNumberishTime other) const
{
    ASSERT(m_type == other.m_type);
    return m_value <= other.m_value;
}

bool CSSNumberishTime::operator>(CSSNumberishTime other) const
{
    ASSERT(m_type == other.m_type);
    return m_value > other.m_value;
}

bool CSSNumberishTime::operator>=(CSSNumberishTime other) const
{
    ASSERT(m_type == other.m_type);
    return m_value >= other.m_value;
}

CSSNumberishTime::operator CSSNumberish() const
{
    switch (m_source) {
    case Source::Literal:
        ASSERT(m_type == Type::Time);
        return m_value * 1000;
    case Source::Number:
        ASSERT(m_type == Type::Time);
        return CSSNumericFactory::number(m_value * 1000);
    case Source::Milliseconds:
        ASSERT(m_type == Type::Time);
        return CSSNumericFactory::ms(m_value * 1000);
    case Source::Seconds:
        ASSERT(m_type == Type::Time);
        return CSSNumericFactory::s(m_value);
    case Source::Percentage:
        ASSERT(m_type == Type::Percentage);
        return CSSNumericFactory::percent(m_value);
    }

    ASSERT_NOT_REACHED();
    return m_value;
}

} // namespace WebCore
