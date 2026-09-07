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

#include "ColorInterpolationMethod.h"
#include "FontPalette.h"
#include <optional>
#include <wtf/Vector.h>

namespace WebCore {

struct FontPaletteMixFunction {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(FontPaletteMixFunction);

    struct Component {
        FontPalette palette;
        std::optional<double> percentage;

        bool operator==(const Component&) const = default;
    };
    using Components = Vector<Component>;

    ColorInterpolationMethod colorInterpolationMethod;
    Components components;

    bool operator==(const FontPaletteMixFunction&) const = default;
};

// Overload of operator== for UniqueRef<FontPaletteFunction> to make FontPalette::Kind's operator== work.
inline bool operator==(const UniqueRef<FontPaletteMixFunction>& a, const UniqueRef<FontPaletteMixFunction>& b)
{
    return arePointingToEqualData(a, b);
}

void add(Hasher&, const FontPaletteMixFunction::Component&);
void add(Hasher&, const FontPaletteMixFunction&);

TextStream& operator<<(TextStream&, const FontPaletteMixFunction::Component&);
TextStream& operator<<(TextStream&, const FontPaletteMixFunction&);

} // namespace WebCore
