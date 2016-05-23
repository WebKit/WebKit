/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
#include "CSSVariableValue.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserValues.h"
#include "CSSValueList.h"
#include "CSSVariableDependentValue.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSVariableValue::CSSVariableValue(CSSParserVariable* variable)
    : CSSValue(VariableClass)
    , m_name(variable->name)
{
    if (variable->args)
        m_fallbackArguments = CSSValueList::createFromParserValueList(*variable->args);
}

CSSVariableValue::CSSVariableValue(const String& name, PassRefPtr<CSSValueList> fallbackArgs)
    : CSSValue(VariableClass)
    , m_name(name)
    , m_fallbackArguments(fallbackArgs)
{
}

String CSSVariableValue::customCSSText() const
{
    StringBuilder result;
    result.append("var(");
    result.append(m_name);
    if (m_fallbackArguments) {
        result.append(", ");
        result.append(m_fallbackArguments->cssText());
    }
    result.append(")");
    return result.toString();
}

bool CSSVariableValue::equals(const CSSVariableValue& other) const
{
    return m_name == other.m_name && compareCSSValuePtr(m_fallbackArguments, other.m_fallbackArguments);
}

bool CSSVariableValue::buildParserValueListSubstitutingVariables(CSSParserValueList* resultList, const CustomPropertyValueMap& customProperties) const
{
    if (RefPtr<CSSValue> value = customProperties.get(m_name)) {
        if (value->isValueList())
            return downcast<CSSValueList>(*value).buildParserValueListSubstitutingVariables(resultList, customProperties);
        if (value->isVariableDependentValue())
            return downcast<CSSVariableDependentValue>(*value).valueList().buildParserValueListSubstitutingVariables(resultList, customProperties);
    }

    // We failed to substitute the variable. If it has fallback arguments, use those instead.
    if (!fallbackArguments() || !fallbackArguments()->length())
        return false;
    return fallbackArguments()->buildParserValueListSubstitutingVariables(resultList, customProperties);
}

}
