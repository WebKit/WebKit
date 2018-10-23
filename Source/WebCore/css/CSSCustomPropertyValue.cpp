/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

bool CSSCustomPropertyValue::equals(const CSSCustomPropertyValue& other) const
{
    if (m_name != other.m_name || m_value.index() != other.m_value.index())
        return false;
    auto visitor = WTF::makeVisitor([&](const Ref<CSSVariableReferenceValue>& value) {
        return value.get() == WTF::get<Ref<CSSVariableReferenceValue>>(other.m_value).get();
    }, [&](const CSSValueID& value) {
        return value == WTF::get<CSSValueID>(other.m_value);
    }, [&](const Ref<CSSVariableData>& value) {
        return value.get() == WTF::get<Ref<CSSVariableData>>(other.m_value).get();
    }, [&](const Length& value) {
        return value == WTF::get<Length>(other.m_value);
    });
    return WTF::visit(visitor, m_value);
}

String CSSCustomPropertyValue::customCSSText() const
{
    if (!m_serialized) {
        m_serialized = true;

        auto visitor = WTF::makeVisitor([&](const Ref<CSSVariableReferenceValue>& value) {
            m_stringValue = value->cssText();
        }, [&](const CSSValueID& value) {
            m_stringValue = getValueName(value);
        }, [&](const Ref<CSSVariableData>& value) {
            m_stringValue = value->tokenRange().serialize();
        }, [&](const Length& value) {
            m_stringValue = CSSPrimitiveValue::create(value.value(), CSSPrimitiveValue::CSS_PX)->cssText();
        });
        WTF::visit(visitor, m_value);
    }
    return m_stringValue;
}

Vector<CSSParserToken> CSSCustomPropertyValue::tokens() const
{
    Vector<CSSParserToken> result;

    auto visitor = WTF::makeVisitor([&](const Ref<CSSVariableReferenceValue>&) {
        ASSERT_NOT_REACHED();
    }, [&](const CSSValueID&) {
        // Do nothing
    }, [&](const Ref<CSSVariableData>& value) {
        result.appendVector(value->tokens());
    }, [&](const Length&) {
        CSSTokenizer tokenizer(cssText());

        auto tokenizerRange = tokenizer.tokenRange();
        while (!tokenizerRange.atEnd())
            result.append(tokenizerRange.consume());
    });
    WTF::visit(visitor, m_value);

    return result;
}

}
