/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCustomProperty.h"

#include "CSSCalcValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSSerializationContext.h"
#include "CSSTokenizer.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CSSVariableReferenceValue.h"
#include "CalculationValue.h"
#include "RenderStyle.h"
#include "StyleExtractorConverter.h"
#include "StyleExtractorSerializer.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CustomProperty);

bool CustomProperty::operator==(const CustomProperty& other) const
{
    if (m_name != other.m_name || m_value.index() != other.m_value.index())
        return false;

    return WTF::switchOn(m_value,
        [&](const GuaranteedInvalid&) {
            return true;
        },
        [&](const Ref<CSSVariableData>& value) {
            return arePointingToEqualData(value, std::get<Ref<CSSVariableData>>(other.m_value));
        },
        [&](const Value& value) {
            return value == std::get<Value>(other.m_value);
        },
        [&](const ValueList& value) {
            return value == std::get<ValueList>(other.m_value);
        }
    );
}

Ref<CSSValue> CustomProperty::propertyValue(CSSValuePool& pool, const RenderStyle& style) const
{
    auto convertValue = [&](const Value& value) {
        return WTF::switchOn(value,
            [&](const auto& value) -> Ref<CSSValue> {
                return createCSSValue(pool, style, value);
            }
        );
    };

    return WTF::switchOn(m_value,
        [&](const GuaranteedInvalid&) -> Ref<CSSValue> {
            return CSSPrimitiveValue::create(""_s);
        },
        [&](const Ref<CSSVariableData>& variableData) -> Ref<CSSValue> {
            return CSSVariableReferenceValue::create(variableData.copyRef());
        },
        [&](const Value& value) -> Ref<CSSValue> {
            return convertValue(value);
        },
        [&](const ValueList& valueList) -> Ref<CSSValue> {
            CSSValueListBuilder builder;
            for (auto& value : valueList.values)
                builder.append(convertValue(value));
            switch (valueList.separator) {
            case CSSValue::SpaceSeparator:
                return CSSValueList::createSpaceSeparated(WTFMove(builder));
            case CSSValue::CommaSeparator:
                return CSSValueList::createCommaSeparated(WTFMove(builder));
            case CSSValue::SlashSeparator:
                return CSSValueList::createSlashSeparated(WTFMove(builder));
            }
            RELEASE_ASSERT_NOT_REACHED();
        }
    );
}

String CustomProperty::propertyValueSerialization(const CSS::SerializationContext& context, const RenderStyle& style) const
{
    StringBuilder builder;
    propertyValueSerialization(builder, context, style);
    return builder.toString();
}

void CustomProperty::propertyValueSerialization(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style) const
{
    auto serializeValue = [&](StringBuilder& builder, const Value& value) {
        WTF::switchOn(value,
            [&](const auto& value) {
                Style::serializationForCSS(builder, context, style, value);
            }
        );
    };

    WTF::switchOn(m_value,
        [&](const GuaranteedInvalid&) {
            // "The guaranteed-invalid value serializes as the empty string".
            // https://drafts.csswg.org/css-variables-2/#guaranteed-invalid
            builder.append(emptyString());
        },
        [&](const Ref<CSSVariableData>& value) {
            builder.append(value->serialize());
        },
        [&](const Value& value) {
            serializeValue(builder, value);
        },
        [&](const ValueList& valueList) {
            builder.append(interleave(valueList.values, [&](auto& builder, const auto& value) {
                serializeValue(builder, value);
            }, CSSValue::separatorCSSText(valueList.separator)));
        }
    );
}

String CustomProperty::propertyValueSerializationForTokenization(const CSS::SerializationContext& context, const RenderStyle& style) const
{
    StringBuilder builder;
    propertyValueSerializationForTokenization(builder, context, style);
    return builder.toString();
}

void CustomProperty::propertyValueSerializationForTokenization(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style) const
{
    // `propertyValueSerializationForTokenization` differs from `propertyValueSerialization` only in how it handles custom `color`
    // values:
    //
    //   `propertyValueSerialization`: serializes the used/resolved value of `color` values (needed for getComputedStyle)
    //   `propertyValueSerializationForTokenization`: serializes the computed value of `color` values (needed for style building)

    auto serializeValue = [&](StringBuilder& builder, const Value& value) {
        WTF::switchOn(value,
            [&](const Color& value) {
                Style::serializationForCSSTokenization(builder, context, value);
            },
            [&](const auto& value) {
                Style::serializationForCSS(builder, context, style, value);
            }
        );
    };

    WTF::switchOn(m_value,
        [&](const GuaranteedInvalid&) {
            // "The guaranteed-invalid value serializes as the empty string".
            // https://drafts.csswg.org/css-variables-2/#guaranteed-invalid
            builder.append(emptyString());
        },
        [&](const Ref<CSSVariableData>& value) {
            builder.append(value->serialize());
        },
        [&](const Value& value) {
            serializeValue(builder, value);
        },
        [&](const ValueList& valueList) {
            builder.append(interleave(valueList.values, [&](auto& builder, const auto& value) {
                serializeValue(builder, value);
            }, CSSValue::separatorCSSText(valueList.separator)));
        }
    );
}

const Vector<CSSParserToken>& CustomProperty::tokens() const
{
    static NeverDestroyed<Vector<CSSParserToken>> emptyTokens;

    return WTF::switchOn(m_value,
        [&](const Ref<CSSVariableData>& value) -> const Vector<CSSParserToken>& {
            return value->tokens();
        },
        [&](auto&) -> const Vector<CSSParserToken>& {
            if (!m_cachedTokens) {
                CSSTokenizer tokenizer { propertyValueSerializationForTokenization(CSS::defaultSerializationContext(), RenderStyle::defaultStyleSingleton()) };
                m_cachedTokens = CSSVariableData::create(tokenizer.tokenRange());
            }
            return m_cachedTokens->tokens();
        }
    );
}

} // namespace Style
} // namespace WebCore
