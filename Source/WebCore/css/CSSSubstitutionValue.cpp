// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2021 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSSubstitutionValue.h"

#include "CSSVariableData.h"

namespace WebCore {

CSSSubstitutionValue::CSSSubstitutionValue(Ref<CSSVariableData>&& data, const CSSNamespacePrefixMap& namespacePrefixMap)
    : CSSValue(ClassType::Substitution)
    , m_data(WTF::move(data))
    , m_namespacePrefixMap(namespacePrefixMap)
{
    cacheSimpleReference();
}

Ref<CSSSubstitutionValue> CSSSubstitutionValue::create(const CSSParserTokenRange& range, const CSSNamespacePrefixMap& namespacePrefixMap, const CSSParserContext& context)
{
    return adoptRef(*new CSSSubstitutionValue(CSSVariableData::create(range, context), namespacePrefixMap));
}

Ref<CSSSubstitutionValue> CSSSubstitutionValue::create(Ref<CSSVariableData>&& data)
{
    return adoptRef(*new CSSSubstitutionValue(WTF::move(data), CSSNamespacePrefixMap { }));
}

bool CSSSubstitutionValue::equals(const CSSSubstitutionValue& other) const
{
    return arePointingToEqualData(m_data, other.m_data);
}

String CSSSubstitutionValue::customCSSText(const CSS::SerializationContext&) const
{
    if (m_stringValue.isNull())
        m_stringValue = m_data->serialize();
    return m_stringValue;
}

const CSSParserContext& CSSSubstitutionValue::context() const
{
    return m_data->context();
}

void CSSSubstitutionValue::cacheSimpleReference()
{
    ASSERT(!m_simpleReference);

    auto range = m_data->tokenRange();

    auto functionId = range.peek().functionId();

    if (functionId == CSSValueInternalAutoBase) {
        range.consumeBlock();
        if (range.atEnd())
            m_simpleReference = SimpleReference { { }, CSSValueInternalAutoBase };
        return;
    }

    if (functionId != CSSValueVar && functionId != CSSValueEnv)
        return;

    auto variableRange = range.consumeBlock();
    if (!range.atEnd())
        return;

    variableRange.consumeWhitespace();

    auto variableName = variableRange.consumeIncludingWhitespace().value().toAtomString();

    // No fallback support on this path.
    if (!variableRange.atEnd())
        return;

    m_simpleReference = SimpleReference { variableName, functionId };
}

} // namespace WebCore
