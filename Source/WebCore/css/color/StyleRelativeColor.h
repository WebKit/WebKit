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
#include "CSSRelativeColorResolver.h"
#include "CSSRelativeColorSerialization.h"
#include "Color.h"
#include "ColorSerialization.h"
#include "StyleColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

template<typename Descriptor, unsigned Index>
using StyleRelativeColorComponent = GetComponentResultWithCalcAndSymbolsResult<Descriptor, Index>;

template<typename D>
struct StyleRelativeColor {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    using Descriptor = D;

    StyleColor origin;
    CSSColorParseTypeWithCalcAndSymbols<Descriptor> components;

    bool operator==(const StyleRelativeColor<Descriptor>&) const = default;
};

template<typename Descriptor>
bool operator==(const UniqueRef<StyleRelativeColor<Descriptor>>& a, const UniqueRef<StyleRelativeColor<Descriptor>>& b)
{
    return a.get() == b.get();
}

template<typename Descriptor>
std::optional<Color> resolveAbsoluteComponents(const StyleRelativeColor<Descriptor>& relative)
{
    if (containsNonAbsoluteColor(relative))
        return std::nullopt;

    return resolve(
        CSSRelativeColorResolver<Descriptor> {
            .origin = relative.origin.absoluteColor(),
            .components = relative.components
        }
    );
}

template<typename Descriptor>
Color resolveColor(const StyleRelativeColor<Descriptor>& relative, const Color& currentColor)
{
    return resolve(
        CSSRelativeColorResolver<Descriptor> {
            .origin = relative.origin.resolveColor(currentColor),
            .components = relative.components
        }
    );
}

template<typename Descriptor>
bool containsNonAbsoluteColor(const StyleRelativeColor<Descriptor>& relative)
{
    return !relative.origin.isAbsoluteColor();
}

template<typename Descriptor>
bool containsCurrentColor(const StyleRelativeColor<Descriptor>& relative)
{
    return relative.origin.containsCurrentColor();
}

template<typename Descriptor>
void serializationForCSS(StringBuilder& builder, const StyleRelativeColor<Descriptor>& relative)
{
    serializationForCSSRelativeColor(builder, relative);
}

template<typename Descriptor>
String serializationForCSS(const StyleRelativeColor<Descriptor>& relative)
{
    StringBuilder builder;
    serializationForCSS(builder, relative);
    return builder.toString();
}

template<typename Descriptor>
WTF::TextStream& operator<<(WTF::TextStream& ts, const StyleRelativeColor<Descriptor>& relative)
{
    ts << "relativeColor(" << serializationForCSS(relative) << ")";
    return ts;
}

} // namespace WebCore
