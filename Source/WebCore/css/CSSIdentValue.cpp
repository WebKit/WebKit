/*
 * Copyright (C) 2026 saku (saku@email.sakupi01.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSIdentValue.h"

#include "CSSCalcValue.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSSerializationContext.h"
#include "CSSToLengthConversionData.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool IdentArgument::operator==(const IdentArgument& other) const
{
    if (value.index() != other.value.index())
        return false;

    return WTF::switchOn(value,
        [&](const String& argument) {
            return argument == std::get<String>(other.value);
        },
        [&](const CSS::Integer<>& argument) {
            return argument == std::get<CSS::Integer<>>(other.value);
        },
        [&](const CSS::CustomIdent& argument) {
            return argument == std::get<CSS::CustomIdent>(other.value);
        }
    );
}

Ref<CSSIdentValue> CSSIdentValue::create(Vector<IdentArgument>&& arguments)
{
    return adoptRef(*new CSSIdentValue(WTF::move(arguments)));
}

String CSSIdentValue::stringValue() const
{
    // Concatenate all arguments to produce a parse-time identifier text.
    StringBuilder builder;

    for (const auto& argument : m_arguments) {
        WTF::switchOn(argument.value,
            [&](const String& stringArgument) {
                builder.append(stringArgument);
            },
            [&](const CSS::Integer<>& integerArgument) {
                CSS::serializationForCSS(builder, CSS::defaultSerializationContext(), integerArgument);
            },
            [&](const CSS::CustomIdent& identArgument) {
                builder.append(identArgument.value);
            }
        );
    }

    return builder.toString();
}

std::optional<String> CSSIdentValue::computedStringValue(const CSSToLengthConversionData& conversionData) const
{
    StringBuilder builder;

    for (const auto& argument : m_arguments) {
        WTF::switchOn(argument.value,
            [&](const String& stringArgument) {
                builder.append(stringArgument);
            },
            [&](const CSS::Integer<>& integerArgument) {
                integerArgument.switchOn(
                    [&](const CSS::Integer<>::Raw& raw) {
                        builder.append(String::number(raw.value));
                    },
                    [&](const CSS::Integer<>::Calc& calc) {
                        Ref calcValue = calc.calcValue();
                        builder.append(String::number(static_cast<int>(calcValue->doubleValue(conversionData))));
                    }
                );
            },
            [&](const CSS::CustomIdent& identArgument) {
                builder.append(identArgument.value);
            }
        );
    }

    return builder.toString();
}

bool CSSIdentValue::equals(const CSSIdentValue& other) const
{
    if (m_arguments.size() != other.m_arguments.size())
        return false;

    for (size_t i = 0; i < m_arguments.size(); ++i) {
        if (m_arguments[i] != other.m_arguments[i])
            return false;
    }

    return true;
}

String CSSIdentValue::customCSSText(const CSS::SerializationContext& context) const
{
    // https://drafts.csswg.org/css-values-5/#ident
    // <ident()> = ident( <ident-arg>+ )
    // <ident-arg> = <string> | <integer> | <ident>

    StringBuilder builder;
    builder.append("ident("_s);

    bool first = true;
    for (const auto& argument : m_arguments) {
        if (!first)
            builder.append(' ');
        first = false;

        WTF::switchOn(argument.value,
            [&](const String& stringArgument) {
                serializeString(stringArgument, builder);
            },
            [&](const CSS::Integer<>& integerArgument) {
                CSS::serializationForCSS(builder, context, integerArgument);
            },
            [&](const CSS::CustomIdent& identArgument) {
                serializeIdentifier(identArgument.value, builder);
            }
        );
    }

    builder.append(')');
    return builder.toString();
}

} // namespace WebCore
