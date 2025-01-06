/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSColorDescriptors.h"
#include "CSSRelativeColor.h"
#include "CSSRelativeColorResolver.h"
#include "CSSRelativeColorSerialization.h"
#include "Color.h"
#include "ColorSerialization.h"
#include "StyleColor.h"
#include "StyleResolvedColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

template<typename D, unsigned Index> using RelativeColorComponent = GetCSSColorParseTypeWithCalcAndSymbolsComponentResult<D, Index>;

template<typename D> struct RelativeColor {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    using Descriptor = D;

    Style::Color origin;
    CSSColorParseTypeWithCalcAndSymbols<Descriptor> components;

    bool operator==(const RelativeColor<Descriptor>&) const = default;
};

template<typename D> bool operator==(const UniqueRef<RelativeColor<D>>& a, const UniqueRef<RelativeColor<D>>& b)
{
    return a.get() == b.get();
}

template<typename D> Style::Color toStyleColor(const CSS::RelativeColor<D>& unresolved, ColorResolutionState& state)
{
    ColorResolutionStateNester nester { state };

    auto origin = toStyleColor(unresolved.origin, state);
    if (!origin.isResolvedColor()) {
        // If the origin is not absolute, we cannot fully resolve the color yet.
        // Instead, we simplify the calc values using the conversion data, and
        // return a Style::RelativeColor to be resolved at use time.
        return Style::Color {
            RelativeColor<D> {
                .origin = WTFMove(origin),
                .components = simplifyUnevaluatedCalc(unresolved.components, state.conversionData, CSSCalcSymbolTable { })
            }
        };
    }

    // If the origin is absolute, we can fully resolve the entire color.
    auto color = resolve(
        CSS::RelativeColorResolver<D> {
            .origin = origin.resolvedColor(),
            .components = unresolved.components
        },
        state.conversionData
    );

    return { ResolvedColor { WTFMove(color) } };
}

template<typename D> WebCore::Color resolveColor(const RelativeColor<D>& relative, const WebCore::Color& currentColor)
{
    return resolveNoConversionDataRequired(
        CSS::RelativeColorResolver<D> {
            .origin = relative.origin.resolveColor(currentColor),
            .components = relative.components
        }
    );
}

template<typename D> bool containsCurrentColor(const RelativeColor<D>& relative)
{
    return WebCore::Style::containsCurrentColor(relative.origin);
}

template<typename D> void serializationForCSS(StringBuilder& builder, const RelativeColor<D>& relative)
{
    CSS::serializationForCSSRelativeColor(builder, relative);
}

template<typename D> String serializationForCSS(const RelativeColor<D>& relative)
{
    StringBuilder builder;
    serializationForCSS(builder, relative);
    return builder.toString();
}

template<typename D> WTF::TextStream& operator<<(WTF::TextStream& ts, const RelativeColor<D>& relative)
{
    ts << "relativeColor(" << serializationForCSS(relative) << ")";
    return ts;
}

} // namespace Style
} // namespace WebCore
