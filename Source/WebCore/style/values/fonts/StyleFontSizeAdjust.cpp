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
#include "StyleFontSizeAdjust.h"

#include "AnimationUtilities.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

static constexpr auto defaultMetric = FontSizeAdjust::Metric::ExHeight;

std::optional<float> FontSizeAdjust::resolvedMetricValue(const RenderStyle& style) const
{
    if (m_platform.shouldResolveFromFont())
        return m_platform.resolve(style.computedFontSize(), style.metricsOfPrimaryFont());
    return m_platform.value;
}

// MARK: - Conversion

auto CSSValueConversion<FontSizeAdjust>::operator()(BuilderState& state, const CSSValue& value) -> FontSizeAdjust
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (auto valueID = primitiveValue->valueID(); valueID) {
        case CSSValueNone:
            return CSS::Keyword::None { };

        case CSSValueFromFont:
            // We cannot determine the primary font here, so we defer resolving the
            // aspect value for from-font to when the primary font is created.
            // See FontCascadeFonts::primaryFont().
            return { defaultMetric, CSS::Keyword::FromFont { } };

        case CSSValueInvalid:
            return { defaultMetric, toStyleFromCSSValue<FontSizeAdjust::Number>(state, *primitiveValue) };

        default:
            if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
                return CSS::Keyword::None { };

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return CSS::Keyword::None { };

    auto metric = fromCSSValueID<FontSizeAdjust::Metric>(pair->first->valueID());

    if (pair->second->valueID() == CSSValueFromFont) {
        // We cannot determine the primary font here, so we defer resolving the
        // aspect value for from-font to when the primary font is created.
        // See FontCascadeFonts::primaryFont().
        return { metric, CSS::Keyword::FromFont { } };
    }

    return { metric, toStyleFromCSSValue<FontSizeAdjust::Number>(state, pair->second) };
}

auto CSSValueCreation<FontSizeAdjust>::operator()(CSSValuePool& pool, const RenderStyle& style, const FontSizeAdjust& value) -> Ref<CSSValue>
{
    if (value.isNone())
        return createCSSValue(pool, style, CSS::Keyword::None { });

    auto metric = value.metric();
    auto metricValue = value.resolvedMetricValue(style);

    if (!metricValue)
        return createCSSValue(pool, style, CSS::Keyword::None { });

    if (metric == defaultMetric)
        return createCSSValue(pool, style, FontSizeAdjust::Number { *metricValue });

    return createCSSValue(pool, style, SpaceSeparatedTuple { metric, FontSizeAdjust::Number { *metricValue } });
}

// MARK: - Serialization

void Serialize<FontSizeAdjust>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const FontSizeAdjust& value)
{
    if (value.isNone()) {
        serializationForCSS(builder, context, style, CSS::Keyword::None { });
        return;
    }

    auto metric = value.metric();
    auto metricValue = value.resolvedMetricValue(style);

    if (!metricValue) {
        serializationForCSS(builder, context, style, CSS::Keyword::None { });
        return;
    }

    if (metric == defaultMetric) {
        serializationForCSS(builder, context, style, FontSizeAdjust::Number { *metricValue });
        return;
    }

    serializationForCSS(builder, context, style, SpaceSeparatedTuple { metric, FontSizeAdjust::Number { *metricValue } });
}

// MARK: Blending

auto Blending<FontSizeAdjust>::canBlend(const FontSizeAdjust& a, const FontSizeAdjust& b) -> bool
{
    return a.metric() == b.metric()
        && a.metricValue() && b.metricValue();
}

auto Blending<FontSizeAdjust>::blend(const FontSizeAdjust& a, const FontSizeAdjust& b, const BlendingContext& context) -> FontSizeAdjust
{
    if (context.isDiscrete)
        return !context.progress ? a : b;

    ASSERT(canBlend(a, b));
    return {
        a.metric(),
        Style::blend(FontSizeAdjust::Number { *a.metricValue() }, FontSizeAdjust::Number { *b.metricValue() }, context)
    };
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const FontSizeAdjust& value)
{
    return ts << value.platform();
}

} // namespace Style
} // namespace WebCore
