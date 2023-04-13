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

#include "CSSCalcValue.h"
#include "CSSFunctionValue.h"
#include "CSSMarkup.h"
#include "CSSParserIdioms.h"
#include "CSSTokenizer.h"
#include "ColorSerialization.h"
#include "ComputedStyleExtractor.h"
#include "RenderStyle.h"

namespace WebCore {

Ref<CSSCustomPropertyValue> CSSCustomPropertyValue::createEmpty(const AtomString& name)
{
    return adoptRef(*new CSSCustomPropertyValue(name, std::monostate { }));
}

Ref<CSSCustomPropertyValue> CSSCustomPropertyValue::createWithID(const AtomString& name, CSSValueID id)
{
    ASSERT(WebCore::isCSSWideKeyword(id) || id == CSSValueInvalid);
    return adoptRef(*new CSSCustomPropertyValue(name, { id }));
}

bool CSSCustomPropertyValue::equals(const CSSCustomPropertyValue& other) const
{
    if (m_name != other.m_name || m_value.index() != other.m_value.index())
        return false;
    return WTF::switchOn(m_value, [&](const std::monostate&) {
        return true;
    }, [&](const Ref<CSSVariableReferenceValue>& value) {
        return value.get() == std::get<Ref<CSSVariableReferenceValue>>(other.m_value).get();
    }, [&](const CSSValueID& value) {
        return value == std::get<CSSValueID>(other.m_value);
    }, [&](const Ref<CSSVariableData>& value) {
        return value.get() == std::get<Ref<CSSVariableData>>(other.m_value).get();
    }, [&](const SyntaxValue& value) {
        return value == std::get<SyntaxValue>(other.m_value);
    }, [&](const SyntaxValueList& value) {
        return value == std::get<SyntaxValueList>(other.m_value);
    });
}

String CSSCustomPropertyValue::customCSSText() const
{
    auto serializeSyntaxValue = [](const SyntaxValue& syntaxValue) -> String {
        return WTF::switchOn(syntaxValue, [&](const Length& value) {
            return CSSPrimitiveValue::create(value, RenderStyle::defaultStyle())->cssText();
        }, [&](const NumericSyntaxValue& value) {
            return CSSPrimitiveValue::create(value.value, value.unitType)->cssText();
        }, [&](const StyleColor& value) {
            return serializationForCSS(value);
        }, [&](const RefPtr<StyleImage>& value) {
            // FIXME: This is not right for gradients that use `currentcolor`. There should be a way preserve it.
            return value->computedStyleValue(RenderStyle::defaultStyle())->cssText();
        }, [&](const URL& value) {
            return serializeURL(value.string());
        }, [&](const String& value) {
            return value;
        }, [&](const TransformSyntaxValue& value) {
            auto cssValue = transformOperationAsCSSValue(*value.transform, RenderStyle::defaultStyle());
            if (!cssValue)
                return emptyString();
            return cssValue->cssText();
        });
    };

    auto serialize = [&] {
        return WTF::switchOn(m_value, [&](const std::monostate&) {
            return emptyString();
        }, [&](const Ref<CSSVariableReferenceValue>& value) {
            return value->cssText();
        }, [&](const CSSValueID& value) {
            return nameString(value).string();
        }, [&](const Ref<CSSVariableData>& value) {
            return value->tokenRange().serialize();
        }, [&](const SyntaxValue& syntaxValue) {
            return serializeSyntaxValue(syntaxValue);
        }, [&](const SyntaxValueList& syntaxValueList) {
            StringBuilder builder;
            auto separator = separatorCSSText(syntaxValueList.separator);
            for (auto& syntaxValue : syntaxValueList.values) {
                if (!builder.isEmpty())
                    builder.append(separator);
                builder.append(serializeSyntaxValue(syntaxValue));
            }
            return builder.toString();
        });
    };

    if (m_cachedCSSText.isNull())
        m_cachedCSSText = serialize();

    return m_cachedCSSText;
}

const Vector<CSSParserToken>& CSSCustomPropertyValue::tokens() const
{
    static NeverDestroyed<Vector<CSSParserToken>> emptyTokens;

    return WTF::switchOn(m_value, [&](const std::monostate&) -> const Vector<CSSParserToken>& {
        // Do nothing.
        return emptyTokens;
    }, [&](const Ref<CSSVariableReferenceValue>&) -> const Vector<CSSParserToken>& {
        ASSERT_NOT_REACHED();
        return emptyTokens;
    }, [&](const CSSValueID&) -> const Vector<CSSParserToken>& {
        // Do nothing.
        return emptyTokens;
    }, [&](const Ref<CSSVariableData>& value) -> const Vector<CSSParserToken>& {
        return value->tokens();
    }, [&](auto&) -> const Vector<CSSParserToken>& {
        if (!m_cachedTokens) {
            CSSTokenizer tokenizer { customCSSText() };
            m_cachedTokens = CSSVariableData::create(tokenizer.tokenRange());
        }
        return m_cachedTokens->tokens();
    });
}

bool CSSCustomPropertyValue::containsCSSWideKeyword() const
{
    return std::holds_alternative<CSSValueID>(m_value) && WebCore::isCSSWideKeyword(std::get<CSSValueID>(m_value));
}

Ref<const CSSVariableData> CSSCustomPropertyValue::asVariableData() const
{
    return WTF::switchOn(m_value, [&](const Ref<CSSVariableData>& value) -> Ref<const CSSVariableData> {
        return value.get();
    }, [&](const Ref<CSSVariableReferenceValue>& value) -> Ref<const CSSVariableData> {
        return value->data();
    }, [&](auto&) -> Ref<const CSSVariableData> {
        return CSSVariableData::create(tokens());
    });
}

bool CSSCustomPropertyValue::isCurrentColor() const
{
    // FIXME: it's surprising that setting a custom property to "currentcolor"
    // results in m_value being a CSSVariableReferenceValue rather than a
    // CSSValueID set to CSSValueCurrentcolor or a StyleValue.
    return std::holds_alternative<Ref<CSSVariableReferenceValue>>(m_value) && std::get<Ref<CSSVariableReferenceValue>>(m_value)->customCSSText() == "currentcolor"_s;
}

}
