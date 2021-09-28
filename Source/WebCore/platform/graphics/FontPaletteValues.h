/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "Gradient.h"
#include <wtf/HashFunctions.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class FontPaletteValues {
public:
    using PaletteIndex = Variant<int64_t, AtomString>;
    using PaletteColorIndex = Variant<AtomString, int64_t>;
    using OverriddenColor = std::pair<PaletteColorIndex, Color>;

    FontPaletteValues() = default;

    FontPaletteValues(const PaletteIndex& basePalette, Vector<OverriddenColor>&& overrideColors)
        : m_basePalette(basePalette)
        , m_overrideColors(WTFMove(overrideColors))
    {
    }

    const PaletteIndex& basePalette() const
    {
        return m_basePalette;
    }

    const Vector<OverriddenColor>& overrideColors() const
    {
        return m_overrideColors;
    }

    bool operator==(const FontPaletteValues& other) const
    {
        return m_basePalette == other.m_basePalette && m_overrideColors == other.m_overrideColors;
    }

    bool operator!=(const FontPaletteValues& other) const
    {
        return !(*this == other);
    }

private:
    PaletteIndex m_basePalette;
    Vector<OverriddenColor> m_overrideColors;
};

inline void add(Hasher& hasher, const FontPaletteValues& fontPaletteValues)
{
    add(hasher, fontPaletteValues.basePalette());
    add(hasher, fontPaletteValues.overrideColors());
}

} // namespace WebCore

namespace WTF {

template<> struct DefaultHash<WebCore::FontPaletteValues> {
    static unsigned hash(const WebCore::FontPaletteValues& key) { return computeHash(key); }
    static bool equal(const WebCore::FontPaletteValues& a, const WebCore::FontPaletteValues& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

}
