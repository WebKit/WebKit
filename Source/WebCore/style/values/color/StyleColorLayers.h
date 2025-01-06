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

#include "CSSColorLayers.h"
#include "StyleColor.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

enum class BlendMode : uint8_t;
class Color;

namespace Style {

struct ColorResolutionState;

struct ColorLayers {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    BlendMode blendMode;
    CommaSeparatedVector<Color> colors;

    bool operator==(const ColorLayers&) const = default;
};

inline bool operator==(const UniqueRef<ColorLayers>& a, const UniqueRef<ColorLayers>& b)
{
    return a.get() == b.get();
}

Color toStyleColor(const CSS::ColorLayers&, ColorResolutionState&);
WebCore::Color resolveColor(const ColorLayers&, const WebCore::Color& currentColor);
bool containsCurrentColor(const ColorLayers&);

void serializationForCSS(StringBuilder&, const ColorLayers&);
String serializationForCSS(const ColorLayers&);

WTF::TextStream& operator<<(WTF::TextStream&, const ColorLayers&);

} // namespace Style
} // namespace WebCore
