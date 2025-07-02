/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "StylePositionTryFallback.h"

#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Serialization.h"

namespace WebCore {
namespace Style {

PositionTryFallback::~PositionTryFallback() = default;
bool PositionTryFallback::operator==(const PositionTryFallback&) const = default;

// MARK: - Conversion

auto CSSValueConversion<PositionTryFallback>::operator()(BuilderState& state, const CSSValue& value) -> PositionTryFallback
{
    RefPtr valueList = dynamicDowncast<CSSValueList>(value);
    if (!valueList) {
        // Turn the inlined position-area fallback into properties object that can be applied similarly to @position-try declarations.
        auto property = CSSProperty { CSSPropertyPositionArea, Ref { const_cast<CSSValue&>(value) } };
        return PositionTryFallback {
            .positionAreaProperties = ImmutableStyleProperties::create(std::span { &property, 1 }, HTMLStandardMode)
        };
    }

    if (valueList->separator() != CSSValueList::SpaceSeparator)
        return { };

    auto fallback = PositionTryFallback { };

    for (auto& item : *valueList) {
        if (item.isCustomIdent())
            fallback.positionTryRuleName = toStyleFromCSSValue<ScopedName>(state, item);
        else {
            auto tacticValue = fromCSSValueID<PositionTryFallback::Tactic>(item.valueID());
            if (fallback.tactics.contains(tacticValue)) {
                ASSERT_NOT_REACHED();
                return { };
            }

            fallback.tactics.append(tacticValue);
        }
    }
    return fallback;
}

Ref<CSSValue> CSSValueCreation<PositionTryFallback>::operator()(CSSValuePool& pool, const RenderStyle& style, const PositionTryFallback& value)
{
    if (value.positionAreaProperties) {
        if (RefPtr areaValue = value.positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea))
            return areaValue.releaseNonNull();
        return CSSPrimitiveValue::create(CSSValueNone);
    }

    CSSValueListBuilder singleFallbackList;
    if (value.positionTryRuleName)
        singleFallbackList.append(createCSSValue(pool, style, *value.positionTryRuleName));
    for (auto& tactic : value.tactics)
        singleFallbackList.append(createCSSValue(pool, style, tactic));
    return CSSValueList::createSpaceSeparated(singleFallbackList);
}

// MARK: - Serialization

void Serialize<PositionTryFallback>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const PositionTryFallback& value)
{
    if (value.positionAreaProperties) {
        if (RefPtr areaValue = value.positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea)) {
            builder.append(areaValue->cssText(context));
            return;
        }
        serializationForCSS(builder, context, style, CSS::Keyword::None { });
        return;
    }

    bool includeSpace = false;
    if (value.positionTryRuleName) {
        serializationForCSS(builder, context, style, *value.positionTryRuleName);
        includeSpace = true;
    }
    for (auto& tactic : value.tactics) {
        if (includeSpace)
            builder.append(' ');
        serializationForCSS(builder, context, style, tactic);
        includeSpace = true;
    }
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const PositionTryFallback& positionTryFallback)
{
    auto separator = ""_s;
    for (auto& tactic : positionTryFallback.tactics) {
        ts << std::exchange(separator, " "_s);
        switch (tactic) {
        case PositionTryFallback::Tactic::FlipBlock:
            ts << "flip-block"_s;
            break;
        case PositionTryFallback::Tactic::FlipInline:
            ts << "flip-inline"_s;
            break;
        case PositionTryFallback::Tactic::FlipStart:
            ts << "flip-start"_s;
            break;
        }
    }
    return ts;
}

} // namespace Style
} // namespace WebCore
