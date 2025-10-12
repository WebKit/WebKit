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
#include "StyleFontFeatureSettings.h"

#include "CSSFontFeatureValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "StyleBuilderChecking.h"
#include "StyleFontOpentypeTag.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontFeatureSettings>::operator()(BuilderState& state, const CSSValue& value) -> FontFeatureSettings
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (auto valueID = primitiveValue->valueID(); valueID) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        default:
            if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
                return CSS::Keyword::Normal { };
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSFontFeatureValue>(state, value);
    if (!list)
        return CSS::Keyword::Normal { };

    WebCore::FontFeatureSettings platformSettings;
    for (Ref setting : *list) {
        platformSettings.insert({
            setting->tag(),
            toStyleFromCSSValue<FontFeatureSettings::Value>(state, setting->value()).value
        });
    }

    return { WTFMove(platformSettings) };
}

Ref<CSSValue> CSSValueCreation<FontFeatureSettings>::operator()(CSSValuePool& pool, const RenderStyle& style, const FontFeatureSettings& value)
{
    if (!value.platform().size())
        return createCSSValue(pool, style, CSS::Keyword::Normal { });

    CSSValueListBuilder list;
    for (auto& setting : value.platform()) {
        list.append(CSSFontFeatureValue::create(
            setting.tag(),
            createCSSValue(pool, style, FontFeatureSettings::Value { setting.value() })
        ));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

// MARK: - Serialization

void Serialize<FontFeatureSettings>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const FontFeatureSettings& value)
{
    if (value.platform().isEmpty()) {
        serializationForCSS(builder, context, style, CSS::Keyword::Normal { });
        return;
    }

    builder.append(interleave(value.platform(), [&](auto& builder, const auto& setting) {
        serializationForCSS(builder, context, style, FontOpentypeTag { setting.tag() });
        if (setting.value() != 1.0) {
            builder.append(' ');
            serializationForCSS(builder, context, style, FontFeatureSettings::Value { setting.value() });
        }
    }, ", "_s));
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const FontFeatureSettings& value)
{
    return ts << value.platform();
}

} // namespace Style
} // namespace WebCore
