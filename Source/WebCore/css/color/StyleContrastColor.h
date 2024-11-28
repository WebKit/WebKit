/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

struct StyleContrastColor {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    bool operator==(const StyleContrastColor&) const = default;

    StyleColor color { };
    bool max { false };
};

inline bool operator==(const UniqueRef<StyleContrastColor>& a, const UniqueRef<StyleContrastColor>& b)
{
    return a.get() == b.get();
}

Color resolveColor(const StyleContrastColor&, const Color& currentColor);

bool containsNonAbsoluteColor(const StyleContrastColor&);
bool containsCurrentColor(const StyleContrastColor&);

void serializationForCSS(StringBuilder&, const StyleContrastColor&);
String serializationForCSS(const StyleContrastColor&);

WTF::TextStream& operator<<(WTF::TextStream&, const StyleContrastColor&);

} // namespace WebCore
