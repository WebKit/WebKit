/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSCounterValue::CSSCounterValue(CSS::CustomIdent&& identifier, CSS::String&& separator, CSS::CounterStyle&& counterStyle)
    : CSSValue(ClassType::Counter)
    , m_identifier(WTF::move(identifier))
    , m_separator(WTF::move(separator))
    , m_counterStyle(WTF::move(counterStyle))
{
}

Ref<CSSCounterValue> CSSCounterValue::create(CSS::CustomIdent&& identifier, CSS::String&& separator, CSS::CounterStyle&& counterStyle)
{
    return adoptRef(*new CSSCounterValue(WTF::move(identifier), WTF::move(separator), WTF::move(counterStyle)));
}

bool CSSCounterValue::equals(const CSSCounterValue& other) const
{
    return m_identifier == other.m_identifier
        && m_separator == other.m_separator
        && m_counterStyle == other.m_counterStyle;
}

String CSSCounterValue::customCSSText(const CSS::SerializationContext& context) const
{
    bool hasDefaultCounterStyle = WTF::switchOn(m_counterStyle.identifier,
        [](CSSValueID predefinedKeyword) {
            return predefinedKeyword == CSSValueDecimal;
        },
        [](const CSS::CustomIdent& ident) {
            return ident.value == nameStringForSerialization(CSSValueDecimal);
        }
    );

    if (m_separator.value.isEmpty()) {
        StringBuilder builder;
        builder.append("counter("_s);
        CSS::serializationForCSS(builder, context, m_identifier);
        if (!hasDefaultCounterStyle) {
            builder.append(", "_s);
            WTF::switchOn(m_counterStyle.identifier,
                [&](CSSValueID predefinedKeyword) {
                    builder.append(nameLiteralForSerialization(predefinedKeyword));
                },
                [&](const CSS::CustomIdent& customIdent) {
                    CSS::serializationForCSS(builder, context, customIdent);
                 }
            );
        }
        builder.append(')');
        return builder.toString();
    }

    StringBuilder builder;
    builder.append("counters("_s);
    CSS::serializationForCSS(builder, context, m_identifier);
    builder.append(", "_s);
    CSS::serializationForCSS(builder, context, m_separator);
    if (!hasDefaultCounterStyle) {
        builder.append(", "_s);
        WTF::switchOn(m_counterStyle.identifier,
            [&](CSSValueID predefinedKeyword) {
                builder.append(nameLiteralForSerialization(predefinedKeyword));
            },
            [&](const CSS::CustomIdent& customIdent) {
                CSS::serializationForCSS(builder, context, customIdent);
             }
        );
    }
    builder.append(')');
    return builder.toString();
}

} // namespace WebCore
