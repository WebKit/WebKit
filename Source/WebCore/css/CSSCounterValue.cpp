/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSCounterValue.h"

#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include <wtf/PointerComparison.h> 
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSCounterValue::CSSCounterValue(AtomString identifier, AtomString separator, RefPtr<CSSValue> counterStyle)
    : CSSValue(CounterClass)
    , m_identifier(WTFMove(identifier))
    , m_separator(WTFMove(separator))
    , m_counterStyle(WTFMove(counterStyle))
{
}

Ref<CSSCounterValue> CSSCounterValue::create(AtomString identifier, AtomString separator, RefPtr<CSSValue> counterStyle)
{
    return adoptRef(*new CSSCounterValue(WTFMove(identifier), WTFMove(separator), WTFMove(counterStyle)));
}

bool CSSCounterValue::equals(const CSSCounterValue& other) const
{
    return m_identifier == other.m_identifier && m_separator == other.m_separator && arePointingToEqualData(m_counterStyle, other.m_counterStyle);
}

String CSSCounterValue::customCSSText() const
{
    bool isDecimal = m_counterStyle->valueID() == CSSValueDecimal || (m_counterStyle->isCustomIdent() && m_counterStyle->customIdent() == "decimal"_s);
    auto listStyleSeparator = isDecimal ? ""_s : ", "_s;
    auto listStyleLiteral = isDecimal ? ""_s : counterStyleCSSText();
    if (m_separator.isEmpty())
        return makeString("counter("_s, m_identifier, listStyleSeparator, listStyleLiteral, ')');
    StringBuilder result;
    result.append("counters("_s, m_identifier, ", "_s);
    serializeString(m_separator, result);
    result.append(listStyleSeparator, listStyleLiteral, ')');
    return result.toString();
}

String CSSCounterValue::counterStyleCSSText() const
{
    if (!m_counterStyle)
        return emptyString();

    if (m_counterStyle->isValueID())
        return nameString(m_counterStyle->valueID()).string();
    if (m_counterStyle->isCustomIdent())
        return m_counterStyle->customIdent();

    ASSERT_NOT_REACHED();
    return emptyString();
}
} // namespace WebCore
