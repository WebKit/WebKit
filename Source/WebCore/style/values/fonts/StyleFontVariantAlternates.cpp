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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleFontVariantAlternates.h"

#include "CSSFunctionValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontVariantAlternates>::operator()(BuilderState& state, const CSSValue& value) -> FontVariantAlternates
{
    auto processSingleItemFunction = [&](const CSSFunctionValue& function, String& parameterToSet) -> bool {
        if (function.size() != 1) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return false;
        }
        RefPtr primitiveArgument = dynamicDowncast<CSSPrimitiveValue>(function[0]);
        if (!primitiveArgument || !primitiveArgument->isCustomIdent()) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return false;
        }
        parameterToSet = primitiveArgument->customIdent();
        return true;
    };

    auto processListFunction = [&](const CSSFunctionValue& function, Vector<String>& parameterToSet) -> bool {
        if (!function.size()) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return false;
        }
        for (Ref argument : function) {
            RefPtr primitiveArgument = dynamicDowncast<CSSPrimitiveValue>(argument);
            if (!primitiveArgument || !primitiveArgument->isCustomIdent()) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return false;
            }
            parameterToSet.append(primitiveArgument->customIdent());
        }
        return true;
    };

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueHistoricalForms:
            return CSS::Keyword::HistoricalForms { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    RefPtr list = requiredDowncast<CSSValueList>(state, value);
    if (!list)
        return CSS::Keyword::Normal { };

    auto result = FontVariantAlternates::Platform::Normal();

    for (Ref item : *list) {
        if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(item)) {
            switch (primitive->valueID()) {
            case CSSValueHistoricalForms:
                result.valuesRef().historicalForms = true;
                break;
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Normal { };
            }
        } else if (RefPtr function = dynamicDowncast<CSSFunctionValue>(item.get())) {
            switch (function->name()) {
            case CSSValueSwash:
                if (!processSingleItemFunction(*function, result.valuesRef().swash))
                    return CSS::Keyword::Normal { };
                break;
            case CSSValueStylistic:
                if (!processSingleItemFunction(*function, result.valuesRef().stylistic))
                    return CSS::Keyword::Normal { };
                break;
            case CSSValueStyleset:
                if (!processListFunction(*function, result.valuesRef().styleset))
                    return CSS::Keyword::Normal { };
                break;
            case CSSValueCharacterVariant:
                if (!processListFunction(*function, result.valuesRef().characterVariant))
                    return CSS::Keyword::Normal { };
                break;
            case CSSValueOrnaments:
                if (!processSingleItemFunction(*function, result.valuesRef().ornaments))
                    return CSS::Keyword::Normal { };
                break;
            case CSSValueAnnotation:
                if (!processSingleItemFunction(*function, result.valuesRef().annotation))
                    return CSS::Keyword::Normal { };
                break;
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Normal { };
            }
        } else {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    return result;
}

Ref<CSSValue> CSSValueCreation<FontVariantAlternates>::operator()(CSSValuePool& pool, const RenderStyle& style, const FontVariantAlternates& alternates)
{
    if (alternates.isNormal())
        return createCSSValue(pool, style, CSS::Keyword::Normal { });

    CSSValueListBuilder valueList;

    auto appendKeyword = [&](auto name, const auto& value) {
        if (value)
            valueList.append(createCSSValue(pool, style, name));
    };
    auto appendSingleItemFunction = [&](auto name, const auto& value) {
        if (!value.isNull())
            valueList.append(CSSFunctionValue::create(name.value, createCSSValue(pool, style, CustomIdentifier { AtomString { value } })));
    };
    auto appendListFunction = [&](auto name, const auto& value) {
        if (!value.isEmpty()) {
            CSSValueListBuilder functionArguments;
            for (auto& argument : value)
                functionArguments.append(createCSSValue(pool, style, CustomIdentifier { AtomString { argument } }));
            valueList.append(CSSFunctionValue::create(name.value, WTFMove(functionArguments)));
        }
    };

    appendSingleItemFunction(CSS::Keyword::Stylistic { }, alternates.platform().values().stylistic);
    appendKeyword(CSS::Keyword::HistoricalForms { }, alternates.platform().values().historicalForms);
    appendListFunction(CSS::Keyword::Styleset { }, alternates.platform().values().styleset);
    appendListFunction(CSS::Keyword::CharacterVariant { }, alternates.platform().values().characterVariant);
    appendSingleItemFunction(CSS::Keyword::Swash { }, alternates.platform().values().swash);
    appendSingleItemFunction(CSS::Keyword::Ornaments { }, alternates.platform().values().ornaments);
    appendSingleItemFunction(CSS::Keyword::Annotation { }, alternates.platform().values().annotation);

    if (valueList.size() == 1)
        return WTFMove(valueList[0]);
    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

void Serialize<FontVariantAlternates>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const FontVariantAlternates& alternates)
{
    if (alternates.isNormal()) {
        serializationForCSS(builder, context, style, CSS::Keyword::Normal { });
        return;
    }

    bool needsSpace = false;

    auto appendKeyword = [&](auto name, const auto& value) {
        if (value) {
            if (needsSpace)
                builder.append(' ');
            serializationForCSS(builder, context, style, name);
            needsSpace = true;
        }
    };
    auto appendSingleItemFunction = [&](auto name, const auto& value) {
        if (!value.isNull()) {
            if (needsSpace)
                builder.append(' ');

            serializationForCSS(builder, context, style, name);
            builder.append('(');
            serializationForCSS(builder, context, style, CustomIdentifier { AtomString { value } });
            builder.append(')');

            needsSpace = true;
        }
    };
    auto appendListFunction = [&](auto name, const auto& value) {
        if (!value.isEmpty()) {
            if (needsSpace)
                builder.append(' ');

            serializationForCSS(builder, context, style, name);
            builder.append('(');
            builder.append(interleave(value, [&](auto& builder, const auto& argument) {
                serializationForCSS(builder, context, style, CustomIdentifier { AtomString { argument } });
            }, ", "_s));
            builder.append(')');

            needsSpace = true;
        }
    };

    appendSingleItemFunction(CSS::Keyword::Stylistic { }, alternates.platform().values().stylistic);
    appendKeyword(CSS::Keyword::HistoricalForms { }, alternates.platform().values().historicalForms);
    appendListFunction(CSS::Keyword::Styleset { }, alternates.platform().values().styleset);
    appendListFunction(CSS::Keyword::CharacterVariant { }, alternates.platform().values().characterVariant);
    appendSingleItemFunction(CSS::Keyword::Swash { }, alternates.platform().values().swash);
    appendSingleItemFunction(CSS::Keyword::Ornaments { }, alternates.platform().values().ornaments);
    appendSingleItemFunction(CSS::Keyword::Annotation { }, alternates.platform().values().annotation);
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const FontVariantAlternates& alternates)
{
    return ts << alternates.platform();
}

} // namespace Style
} // namespace WebCore
