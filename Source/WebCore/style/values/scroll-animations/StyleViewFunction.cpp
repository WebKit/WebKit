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
#include "StyleViewFunction.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSViewValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ViewFunction>::operator()(BuilderState& state, const CSSValue& value) -> ViewFunction
{
    RefPtr viewValue = requiredDowncast<CSSViewValue>(state, value);
    if (!viewValue) {
        return ViewFunction {
            ViewFunctionParameters {
                .axis = ScrollAxis::Block,
                .insets = { CSS::Keyword::Auto { } },
            }
        };
    }

    return this->operator()(state, *viewValue);
}

auto CSSValueConversion<ViewFunction>::operator()(BuilderState& state, const CSSViewValue& value) -> ViewFunction
{
    auto axis = [&] {
        if (RefPtr axisValue = value.axis())
            return fromCSSValueID<ScrollAxis>(axisValue->valueID());
        return ScrollAxis::Block;
    }();

    auto startInset = [&] {
        if (RefPtr startInsetValue = value.startInset())
            return toStyleFromCSSValue<ViewTimelineInsetItem::Length>(state, *startInsetValue);
        return ViewTimelineInsetItem::Length { CSS::Keyword::Auto { } };
    }();

    auto endInset = [&] {
        if (RefPtr endInsetValue = value.endInset())
            return toStyleFromCSSValue<ViewTimelineInsetItem::Length>(state, *endInsetValue);
        return startInset;
    }();

    return ViewFunction {
        ViewFunctionParameters {
            .axis = axis,
            .insets = { WTFMove(startInset), WTFMove(endInset) },
        }
    };
}

Ref<CSSValue> CSSValueCreation<ViewFunction>::operator()(CSSValuePool& pool, const RenderStyle& style, const ViewFunction& value)
{
    return CSSViewValue::create(
        createCSSValue(pool, style, value.parameters.axis),
        createCSSValue(pool, style, value.parameters.insets.start()),
        createCSSValue(pool, style, value.parameters.insets.end())
    );
}

// MARK: - Serialization

void Serialize<ViewFunctionParameters>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const ViewFunctionParameters& value)
{
    bool needsSpace = false;
    if (value.axis != ScrollAxis::Block) {
        serializationForCSS(builder, context, style, value.axis);
        needsSpace = true;
    }

    if (value.insets.start() == value.insets.end()) {
        if (value.insets.start().isAuto())
            return;

        if (needsSpace)
            builder.append(' ');
        serializationForCSS(builder, context, style, value.insets.start());
    } else {
        if (needsSpace)
            builder.append(' ');

        serializationForCSS(builder, context, style, value.insets.start());
        builder.append(' ');
        serializationForCSS(builder, context, style, value.insets.end());
    }
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const ViewFunctionParameters& value)
{
    return ts << value.axis << " "_s << value.insets;
}

} // namespace Style
} // namespace WebCore
