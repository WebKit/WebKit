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

#pragma once

#include "StyleColor.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

class Color;

namespace CSS {
struct RelativeAlphaColor;
}

namespace Style {

struct ColorResolutionState;

struct RelativeAlphaColor {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RelativeAlphaColor);

    using Descriptor = AlphaDescriptor;
    using Alpha = GetCSSColorParseTypeWithCalcAndSymbolsComponentResult<Descriptor, 0>;

    Color origin;
    std::optional<Alpha> alpha;

    bool operator==(const RelativeAlphaColor&) const = default;
};

inline bool operator==(const UniqueRef<RelativeAlphaColor>& a, const UniqueRef<RelativeAlphaColor>& b)
{
    return a.get() == b.get();
}

Color toStyleColor(const CSS::RelativeAlphaColor&, ColorResolutionState&);
WebCore::Color resolveColor(const RelativeAlphaColor&, const WebCore::Color& currentColor);
bool containsCurrentColor(const RelativeAlphaColor&);

void serializationForCSSTokenization(StringBuilder&, const CSS::SerializationContext&, const RelativeAlphaColor&);
WTF::String serializationForCSSTokenization(const CSS::SerializationContext&, const RelativeAlphaColor&);

WTF::TextStream& operator<<(WTF::TextStream&, const RelativeAlphaColor&);

} // namespace Style
} // namespace WebCore
