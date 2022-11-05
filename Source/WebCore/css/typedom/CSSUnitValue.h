/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSNumericValue.h"
#include <wtf/RefCounted.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class CSSUnitType : uint8_t;

class CSSUnitValue final : public CSSNumericValue {
    WTF_MAKE_ISO_ALLOCATED(CSSUnitValue);
public:
    static ExceptionOr<Ref<CSSUnitValue>> create(double value, const String& unit);
    static Ref<CSSUnitValue> create(double value, CSSUnitType unit) { return adoptRef(*new CSSUnitValue(value, unit)); }

    void serialize(StringBuilder&, OptionSet<SerializationArguments>) const final;

    double value() const { return m_value; }
    void setValue(double value) { m_value = value; }
    ASCIILiteral unit() const;
    ASCIILiteral unitSerialization() const;
    CSSUnitType unitEnum() const { return m_unit; }

    RefPtr<CSSUnitValue> convertTo(CSSUnitType) const;
    static CSSUnitType parseUnit(const String& unit);

    RefPtr<CSSValue> toCSSValue() const final;
    RefPtr<CSSCalcExpressionNode> toCalcExpressionNode() const final;

private:
    CSSUnitValue(double, CSSUnitType);

    CSSStyleValueType getType() const final { return CSSStyleValueType::CSSUnitValue; }
    std::optional<SumValue> toSumValue() const final;
    bool equals(const CSSNumericValue&) const final;

    double m_value;
    const CSSUnitType m_unit;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSUnitValue)
static bool isType(const WebCore::CSSStyleValue& styleValue) { return styleValue.getType() == WebCore::CSSStyleValueType::CSSUnitValue; }
SPECIALIZE_TYPE_TRAITS_END()
