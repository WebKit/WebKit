/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSSerializationContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<CSSCustomPropertyValue> CSSCustomPropertyValue::createEmpty(const AtomString& name)
{
    static NeverDestroyed<Ref<CSSVariableData>> empty { CSSVariableData::create({ }) };
    return createSyntaxAll(name, Ref { empty.get() });
}

Ref<CSSCustomPropertyValue> CSSCustomPropertyValue::createUnresolved(const AtomString& name, Ref<CSSVariableReferenceValue>&& value)
{
    return adoptRef(*new CSSCustomPropertyValue(name, VariantValue { WTF::InPlaceType<Ref<CSSVariableReferenceValue>>, WTFMove(value) }));
}

Ref<CSSCustomPropertyValue> CSSCustomPropertyValue::createSyntaxAll(const AtomString& name, Ref<CSSVariableData>&& value)
{
    return adoptRef(*new CSSCustomPropertyValue(name, VariantValue { WTF::InPlaceType<Ref<CSSVariableData>>, WTFMove(value) }));
}

Ref<CSSCustomPropertyValue> CSSCustomPropertyValue::createWithCSSWideKeyword(const AtomString& name, CSSWideKeyword keyword)
{
    return adoptRef(*new CSSCustomPropertyValue(name, VariantValue { keyword }));
}

bool CSSCustomPropertyValue::isVariableReference() const
{
    return std::holds_alternative<Ref<CSSVariableReferenceValue>>(m_value);
}

bool CSSCustomPropertyValue::isVariableData() const
{
    return std::holds_alternative<Ref<CSSVariableData>>(m_value);
}

bool CSSCustomPropertyValue::isCSSWideKeyword() const
{
    return std::holds_alternative<CSSWideKeyword>(m_value);
}

std::optional<CSSWideKeyword> CSSCustomPropertyValue::tryCSSWideKeyword() const
{
    if (auto* keyword = std::get_if<CSSWideKeyword>(&m_value))
        return *keyword;
    return { };
}

bool CSSCustomPropertyValue::equals(const CSSCustomPropertyValue& other) const
{
    if (m_name != other.m_name || m_value.index() != other.m_value.index())
        return false;

    return WTF::switchOn(m_value,
        [&](const Ref<CSSVariableReferenceValue>& value) {
            return arePointingToEqualData(value, std::get<Ref<CSSVariableReferenceValue>>(other.m_value));
        },
        [&](const Ref<CSSVariableData>& value) {
            return arePointingToEqualData(value, std::get<Ref<CSSVariableData>>(other.m_value));
        },
        [&](const CSSWideKeyword& keyword) {
            return keyword == std::get<CSSWideKeyword>(other.m_value);
        }
    );
}

String CSSCustomPropertyValue::customCSSText(const CSS::SerializationContext& context) const
{
    auto serialize = [&] {
        return WTF::switchOn(m_value,
            [&](const Ref<CSSVariableReferenceValue>& value) {
                return value->cssText(context);
            },
            [&](const Ref<CSSVariableData>& value) {
                return value->serialize();
            },
            [&](const CSSWideKeyword& value) {
                return nameStringForSerialization(toValueID(value)).string();
            }
        );
    };

    if (m_cachedCSSText.isNull())
        m_cachedCSSText = serialize();

    return m_cachedCSSText;
}

const Vector<CSSParserToken>& CSSCustomPropertyValue::tokens() const
{
    static NeverDestroyed<Vector<CSSParserToken>> emptyTokens;

    return WTF::switchOn(m_value,
        [&](const Ref<CSSVariableReferenceValue>&) -> const Vector<CSSParserToken>& {
            ASSERT_NOT_REACHED();
            return emptyTokens;
        },
        [&](const Ref<CSSVariableData>& value) -> const Vector<CSSParserToken>& {
            return value->tokens();
        },
        [&](const CSSWideKeyword&) -> const Vector<CSSParserToken>& {
            // Do nothing.
            return emptyTokens;
        }
    );
}

Ref<const CSSVariableData> CSSCustomPropertyValue::asVariableData() const
{
    return WTF::switchOn(m_value,
        [&](const Ref<CSSVariableReferenceValue>& value) -> Ref<const CSSVariableData> {
            return value->data();
        },
        [&](const Ref<CSSVariableData>& value) -> Ref<const CSSVariableData> {
            return value.get();
        },
        [&](const CSSWideKeyword&) -> Ref<const CSSVariableData> {
            return CSSVariableData::create(tokens());
        }
    );
}

bool CSSCustomPropertyValue::isCurrentColor() const
{
    // FIXME: Registered properties?
    auto tokenRange = switchOn(m_value,
        [&](const Ref<CSSVariableReferenceValue>& variableReferenceValue) {
            return variableReferenceValue->data().tokenRange();
        },
        [&](const Ref<CSSVariableData>& data) {
            return data->tokenRange();
        },
        [&](const CSSWideKeyword&) {
            return CSSParserTokenRange { };
        }
    );

    if (tokenRange.atEnd())
        return false;

    auto token = tokenRange.consumeIncludingWhitespace();
    if (!tokenRange.atEnd())
        return false;

    // FIXME: This should probably check all tokens.
    return token.id() == CSSValueCurrentcolor;
}

IterationStatus CSSCustomPropertyValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    if (auto* value = std::get_if<Ref<CSSVariableReferenceValue>>(&m_value)) {
        if (func(*value) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

} // namespace WebCore
