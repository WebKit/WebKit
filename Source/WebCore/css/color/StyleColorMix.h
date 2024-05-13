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

#include "CSSPropertyParserConsumer+RawTypes.h"
#include "ColorInterpolationMethod.h"
#include "StyleColor.h"
#include <optional>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Color;

struct StyleColorMix {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    struct Component {
        using Percentage = PercentRaw;

        StyleColor color;
        std::optional<Percentage> percentage;

        bool operator==(const Component&) const = default;
    };

    bool operator==(const StyleColorMix&) const = default;

    ColorInterpolationMethod colorInterpolationMethod;
    Component mixComponents1;
    Component mixComponents2;
};

inline bool operator==(const UniqueRef<StyleColorMix>& a, const UniqueRef<StyleColorMix>& b)
{
    return a.get() == b.get();
}

std::optional<Color> resolveAbsoluteComponents(const StyleColorMix&);
Color resolveColor(const StyleColorMix&, const Color& currentColor);

bool containsNonAbsoluteColor(const StyleColorMix&);
bool containsCurrentColor(const StyleColorMix&);

void serializationForCSS(StringBuilder&, const StyleColorMix&);
String serializationForCSS(const StyleColorMix&);

WTF::TextStream& operator<<(WTF::TextStream&, const StyleColorMix&);

} // namespace WebCore
