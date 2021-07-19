/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "CSSCustomPropertyValue.h"

#include "CSSTokenizer.h"

namespace WebCore {

Ref<CSSCustomPropertyValue> CSSCustomPropertyValue::createEmpty(const AtomString& name)
{
    return adoptRef(*new CSSCustomPropertyValue(name, Monostate { }));
}

bool CSSCustomPropertyValue::equals(const CSSCustomPropertyValue& other) const
{
    if (m_name != other.m_name || m_value.index() != other.m_value.index())
        return false;
    return WTF::switchOn(m_value, [&](const Monostate&) {
        return true;
    }, [&](const Ref<CSSVariableReferenceValue>& value) {
        return value.get() == WTF::get<Ref<CSSVariableReferenceValue>>(other.m_value).get();
    }, [&](const CSSValueID& value) {
        return value == WTF::get<CSSValueID>(other.m_value);
    }, [&](const Ref<CSSVariableData>& value) {
        return value.get() == WTF::get<Ref<CSSVariableData>>(other.m_value).get();
    }, [&](const Length& value) {
        return value == WTF::get<Length>(other.m_value);
    }, [&](const Ref<StyleImage>& value) {
        return value.get() == WTF::get<Ref<StyleImage>>(other.m_value).get();
    });
}

String CSSCustomPropertyValue::customCSSText() const
{
    if (m_stringValue.isNull()) {
        WTF::switchOn(m_value, [&](const Monostate&) {
            m_stringValue = emptyString();
        }, [&](const Ref<CSSVariableReferenceValue>& value) {
            m_stringValue = value->cssText();
        }, [&](const CSSValueID& value) {
            m_stringValue = getValueName(value);
        }, [&](const Ref<CSSVariableData>& value) {
            m_stringValue = value->tokenRange().serialize();
        }, [&](const Length& value) {
            m_stringValue = CSSPrimitiveValue::create(value.value(), CSSUnitType::CSS_PX)->cssText();
        }, [&](const Ref<StyleImage>& value) {
            m_stringValue = value->cssValue()->cssText();
        });
    }
    return m_stringValue;
}

Vector<CSSParserToken> CSSCustomPropertyValue::tokens() const
{
    Vector<CSSParserToken> result;
    WTF::switchOn(m_value, [&](const Monostate&) {
        // Do nothing.
    }, [&](const Ref<CSSVariableReferenceValue>&) {
        ASSERT_NOT_REACHED();
    }, [&](const CSSValueID&) {
        // Do nothing.
    }, [&](const Ref<CSSVariableData>& value) {
        result.appendVector(value->tokens());
    }, [&](auto&) {
        CSSTokenizer tokenizer(customCSSText());
        auto tokenizerRange = tokenizer.tokenRange();
        while (!tokenizerRange.atEnd())
            result.append(tokenizerRange.consume());
    });
    return result;
}

}
