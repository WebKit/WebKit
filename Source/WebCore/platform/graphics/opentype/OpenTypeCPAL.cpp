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

#include "config.h"
#include "OpenTypeCPAL.h"

#include "OpenTypeTypes.h"

namespace WebCore {

struct CPALHeader {
    OpenType::UInt16 version;
    OpenType::UInt16 numPaletteEntries;
    OpenType::UInt16 numPalettes;
    OpenType::UInt16 numColorRecords;
    OpenType::UInt32 colorRecordsArrayOffset;
    // Followed by `numPalettes` UInt16 color record indices and, in version 1, three UInt32 offsets (palette types, palette labels, and palette entry labels).
};

struct CPALColorRecord {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t alpha;
};

OpenTypeCPAL::OpenTypeCPAL(std::span<const uint8_t> colorRecordIndices, std::span<const uint8_t> colorRecords, size_t numberOfPalettes, size_t numberOfEntries, std::optional<size_t> firstPaletteForLightBackground, std::optional<size_t> firstPaletteForDarkBackground)
    : m_colorRecordIndices { colorRecordIndices }
    , m_colorRecords { colorRecords }
    , m_numberOfPalettes { numberOfPalettes }
    , m_numberOfEntries { numberOfEntries }
    , m_firstPaletteForLightBackground { firstPaletteForLightBackground }
    , m_firstPaletteForDarkBackground { firstPaletteForDarkBackground }
{
}

std::optional<OpenTypeCPAL> OpenTypeCPAL::parse(std::span<const uint8_t> table)
{
    constexpr unsigned usableWithLightBackground = 0x0001;
    constexpr unsigned usableWithDarkBackground = 0x0002;

    if (table.size() < sizeof(CPALHeader))
        return std::nullopt;

    auto& header = reinterpretCastSpanStartTo<const CPALHeader>(table);
    size_t numberOfEntries = header.numPaletteEntries;
    size_t numberOfPalettes = header.numPalettes;
    size_t numberOfColorRecords = header.numColorRecords;
    size_t colorRecordsOffset = header.colorRecordsArrayOffset;

    if (!numberOfPalettes)
        return std::nullopt;

    auto afterHeader = table.subspan(sizeof(CPALHeader));
    if (afterHeader.size() / sizeof(OpenType::UInt16) < numberOfPalettes)
        return std::nullopt;
    auto colorRecordIndexBytes = afterHeader.first(numberOfPalettes * sizeof(OpenType::UInt16));
    auto colorRecordIndices = spanReinterpretCast<const OpenType::UInt16>(colorRecordIndexBytes);

    if (colorRecordsOffset > table.size() || (table.size() - colorRecordsOffset) / sizeof(CPALColorRecord) < numberOfColorRecords)
        return std::nullopt;
    auto colorRecordBytes = table.subspan(colorRecordsOffset).first(numberOfColorRecords * sizeof(CPALColorRecord));
    auto colorRecords = spanReinterpretCast<const CPALColorRecord>(colorRecordBytes);

    std::span<const OpenType::UInt32> paletteTypes;
    if (header.version >= 1) {
        auto afterColorRecordIndices = afterHeader.subspan(colorRecordIndexBytes.size());
        if (afterColorRecordIndices.size() >= sizeof(OpenType::UInt32)) {
            size_t paletteTypesOffset = reinterpretCastSpanStartTo<const OpenType::UInt32>(afterColorRecordIndices);
            if (paletteTypesOffset && paletteTypesOffset <= table.size() && (table.size() - paletteTypesOffset) / sizeof(OpenType::UInt32) >= numberOfPalettes)
                paletteTypes = spanReinterpretCast<const OpenType::UInt32>(table.subspan(paletteTypesOffset).first(numberOfPalettes * sizeof(OpenType::UInt32)));
        }
    }

    std::optional<size_t> firstPaletteForLightBackground;
    std::optional<size_t> firstPaletteForDarkBackground;
    for (size_t paletteIndex = 0; paletteIndex < numberOfPalettes; ++paletteIndex) {
        size_t firstColorRecordIndex = colorRecordIndices[paletteIndex];
        if (firstColorRecordIndex + numberOfEntries > colorRecords.size())
            return std::nullopt;

        if (!paletteTypes.empty()) {
            unsigned type = paletteTypes[paletteIndex];
            if ((type & usableWithLightBackground) && !firstPaletteForLightBackground)
                firstPaletteForLightBackground = paletteIndex;
            if ((type & usableWithDarkBackground) && !firstPaletteForDarkBackground)
                firstPaletteForDarkBackground = paletteIndex;
        }
    }

    return OpenTypeCPAL { colorRecordIndexBytes, colorRecordBytes, numberOfPalettes, numberOfEntries, firstPaletteForLightBackground, firstPaletteForDarkBackground };
}

Vector<Color> OpenTypeCPAL::defaultColors() const
{
    return colors(0);
}

Vector<Color> OpenTypeCPAL::colorsForLightBackground() const
{
    return colors(m_firstPaletteForLightBackground.value_or(0));
}

Vector<Color> OpenTypeCPAL::colorsForDarkBackground() const
{
    return colors(m_firstPaletteForDarkBackground.value_or(0));
}

Vector<Color> OpenTypeCPAL::colors(size_t paletteIndex) const
{
    auto colorRecordIndices = spanReinterpretCast<const OpenType::UInt16>(m_colorRecordIndices);
    auto colorRecords = spanReinterpretCast<const CPALColorRecord>(m_colorRecords);

    return Vector<Color>::map(colorRecords.subspan(colorRecordIndices[paletteIndex], m_numberOfEntries), [](auto& record) -> Color {
        return SRGBA<uint8_t> { record.red, record.green, record.blue, record.alpha };
    });
}

} // namespace WebCore
