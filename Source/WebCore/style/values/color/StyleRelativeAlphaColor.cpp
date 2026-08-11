/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleRelativeAlphaColor.h"

#include "CSSPrimitiveNumericTypes+Logging.h"
#include "CSSRelativeAlphaColorResolver.h"
#include "CSSSerializationContext.h"
#include "CSSUnevaluatedCalc.h"
#include "ColorSerialization.h"
#include "StyleBuilderState.h"
#include "StyleColorResolutionState.h"
#include "StyleKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StyleResolvedColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

Color toStyleColor(const CSS::RelativeAlphaColor& unresolved, ColorResolutionState& state)
{
    ColorResolutionStateNester nester { state };

    auto origin = toStyleColor(unresolved.origin, state);
    if (!origin.isResolvedColor()) {
        // If the origin is not resolved, we cannot fully resolve the color yet.
        // Instead, we simplify the calc values using the conversion data, and
        // return a Style::RelativeAlphaColor to be resolved at use time.
        return Style::Color {
            RelativeAlphaColor {
                .origin = WTF::move(origin),
                .alpha = simplifyUnevaluatedCalc(unresolved.alpha, state.conversionData, CSSCalcSymbolTable { })
            }
        };
    }

    // If the origin is resolved, we can fully resolve the entire color.
    auto color = resolve(
        CSS::RelativeAlphaColorResolver {
            .origin = origin.resolvedColor(),
            .alpha = unresolved.alpha,
        },
        state.conversionData
    );

    return { ResolvedColor { WTF::move(color) } };
}

// MARK: - Resolve

WebCore::Color resolveColor(const RelativeAlphaColor& value, const WebCore::Color& currentColor)
{
    return resolveNoConversionDataRequired(
        CSS::RelativeAlphaColorResolver {
            .origin = value.origin.resolveColor(currentColor),
            .alpha = value.alpha
        }
    );
}

// MARK: - Current Color

bool containsCurrentColor(const RelativeAlphaColor& value)
{
    return WebCore::Style::containsCurrentColor(value.origin);
}

// MARK: - Serialization

void serializationForCSSTokenization(StringBuilder& builder, const CSS::SerializationContext& context, const RelativeAlphaColor& value)
{
    builder.append("alpha(from "_s);
    serializationForCSSTokenization(builder, context, value.origin);

    if (value.alpha) {
        builder.append(" / "_s);
        CSS::serializationForCSS(builder, context, *value.alpha);
    }

    builder.append(')');
}

WTF::String serializationForCSSTokenization(const CSS::SerializationContext& context, const RelativeAlphaColor& value)
{
    StringBuilder builder;
    serializationForCSSTokenization(builder, context, value);
    return builder.toString();
}

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream& ts, const RelativeAlphaColor& value)
{
    return ts << "alpha(from "_s << value.origin;
    if (value.alpha)
        ts << " / "_s << *value.alpha;
    ts << ")"_s;
    return ts;
}

} // namespace Style
} // namespace WebCore
