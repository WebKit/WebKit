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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "Color.h"
#include <optional>
#include <span>
#include <wtf/Vector.h>

namespace WebCore {

// The color palettes of a font from its 'CPAL' table.
// https://learn.microsoft.com/en-us/typography/opentype/spec/cpal
class OpenTypeCPAL {
public:
    static std::optional<OpenTypeCPAL> LEGACY_NODELETE parse(std::span<const uint8_t> table LIFETIME_BOUND);

    size_t numberOfPalettes() const { return m_numberOfPalettes; }

    Vector<Color> colors(size_t paletteIndex) const;

    Vector<Color> defaultColors() const;
    Vector<Color> colorsForLightBackground() const;
    Vector<Color> colorsForDarkBackground() const;

private:
    OpenTypeCPAL(std::span<const uint8_t> colorRecordIndices, std::span<const uint8_t> colorRecords, size_t numberOfPalettes, size_t numberOfEntries, std::optional<size_t> firstPaletteForLightBackground, std::optional<size_t> firstPaletteForDarkBackground);

    std::span<const uint8_t> m_colorRecordIndices;
    std::span<const uint8_t> m_colorRecords;
    size_t m_numberOfPalettes { 0 };
    size_t m_numberOfEntries { 0 };
    std::optional<size_t> m_firstPaletteForLightBackground;
    std::optional<size_t> m_firstPaletteForDarkBackground;
};

} // namespace WebCore
