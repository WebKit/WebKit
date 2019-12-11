/*
 * Copyright (C) 2014 Frederic Wang (fred.wang@free.fr). All rights reserved.
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OpenTypeMathData.h"

#include "Font.h"
#include "FontPlatformData.h"
#if ENABLE(OPENTYPE_MATH)
#include "OpenTypeTypes.h"
#endif
#include "SharedBuffer.h"

namespace WebCore {
using namespace std;

#if ENABLE(OPENTYPE_MATH)
namespace OpenType {

#if PLATFORM(COCOA)
const FourCharCode MATHTag = 'MATH';
#else
const uint32_t MATHTag = OT_MAKE_TAG('M', 'A', 'T', 'H');
#endif

#pragma pack(1)

struct MathValueRecord {
    OpenType::Int16 value;
    OpenType::Offset deviceTableOffset;
};

struct MathConstants {
    OpenType::Int16 intConstants[OpenTypeMathData::ScriptScriptPercentScaleDown - OpenTypeMathData::ScriptPercentScaleDown + 1];
    OpenType::UInt16 uIntConstants[OpenTypeMathData::DisplayOperatorMinHeight - OpenTypeMathData::DelimitedSubFormulaMinHeight + 1];
    OpenType::MathValueRecord mathValuesConstants[OpenTypeMathData::RadicalKernAfterDegree - OpenTypeMathData::MathLeading + 1];
    OpenType::UInt16 radicalDegreeBottomRaisePercent;
};

struct MathItalicsCorrectionInfo : TableWithCoverage {
    OpenType::Offset coverageOffset;
    OpenType::UInt16 italicsCorrectionCount;
    OpenType::MathValueRecord italicsCorrection[1]; // There are italicsCorrectionCount italic correction values.

    int16_t getItalicCorrection(const SharedBuffer& buffer, Glyph glyph) const
    {
        uint16_t count = uint16_t(italicsCorrectionCount);
        if (!isValidEnd(buffer, &italicsCorrection[count]))
            return 0;

        uint16_t offset = coverageOffset;
        if (!offset)
            return 0;
        const CoverageTable* coverage = validateOffset<CoverageTable>(buffer, offset);
        if (!coverage)
            return 0;

        // We determine the index in the italicsCorrection table.
        uint32_t i;
        if (!getCoverageIndex(buffer, coverage, glyph, i) || i >= count)
            return 0;

        return int16_t(italicsCorrection[i].value);
    }
};

struct MathGlyphInfo : TableWithCoverage {
    OpenType::Offset mathItalicsCorrectionInfoOffset;
    OpenType::Offset mathTopAccentAttachmentOffset;
    OpenType::Offset extendedShapeCoverageOffset;
    OpenType::Offset mathKernInfoOffset;

    const MathItalicsCorrectionInfo* mathItalicsCorrectionInfo(const SharedBuffer& buffer) const
    {
        uint16_t offset = mathItalicsCorrectionInfoOffset;
        if (offset)
            return validateOffset<MathItalicsCorrectionInfo>(buffer, offset);
        return nullptr;
    }
};

struct GlyphAssembly : TableBase {
    OpenType::MathValueRecord italicsCorrection;
    OpenType::UInt16 partCount;
    struct GlyphPartRecord {
        OpenType::GlyphID glyph;
        OpenType::UInt16 startConnectorLength;
        OpenType::UInt16 endConnectorLength;
        OpenType::UInt16 fullAdvance;
        OpenType::UInt16 partFlags;
    } partRecords[1]; // There are partCount GlyphPartRecord's.

    // PartFlags enumeration currently uses only one bit:
    // 0x0001 If set, the part can be skipped or repeated.
    // 0xFFFE Reserved.
    enum {
        PartFlagsExtender = 0x01
    };

    void getAssemblyParts(const SharedBuffer& buffer, Vector<OpenTypeMathData::AssemblyPart>& assemblyParts) const
    {
        uint16_t count = partCount;
        if (!isValidEnd(buffer, &partRecords[count]))
            return;
        assemblyParts.resize(count);
        for (uint16_t i = 0; i < count; i++) {
            assemblyParts[i].glyph = partRecords[i].glyph;
            uint16_t flag = partRecords[i].partFlags;
            assemblyParts[i].isExtender = flag & PartFlagsExtender;
        }
    }

};

struct MathGlyphConstruction : TableBase {
    OpenType::Offset glyphAssemblyOffset;
    OpenType::UInt16 variantCount;
    struct MathGlyphVariantRecord {
        OpenType::GlyphID variantGlyph;
        OpenType::UInt16 advanceMeasurement;
    } mathGlyphVariantRecords[1]; // There are variantCount MathGlyphVariantRecord's.

    void getSizeVariants(const SharedBuffer& buffer, Vector<Glyph>& variants) const
    {
        uint16_t count = variantCount;
        if (!isValidEnd(buffer, &mathGlyphVariantRecords[count]))
            return;
        variants.resize(count);
        for (uint16_t i = 0; i < count; i++)
            variants[i] = mathGlyphVariantRecords[i].variantGlyph;
    }

    void getAssemblyParts(const SharedBuffer& buffer, Vector<OpenTypeMathData::AssemblyPart>& assemblyParts) const
    {
        uint16_t offset = glyphAssemblyOffset;
        const GlyphAssembly* glyphAssembly = validateOffset<GlyphAssembly>(buffer, offset);
        if (glyphAssembly)
            glyphAssembly->getAssemblyParts(buffer, assemblyParts);
    }
};

struct MathVariants : TableWithCoverage {
    OpenType::UInt16 minConnectorOverlap;
    OpenType::Offset verticalGlyphCoverageOffset;
    OpenType::Offset horizontalGlyphCoverageOffset;
    OpenType::UInt16 verticalGlyphCount;
    OpenType::UInt16 horizontalGlyphCount;
    OpenType::Offset mathGlyphConstructionsOffset[1]; // There are verticalGlyphCount vertical glyph contructions and horizontalGlyphCount vertical glyph contructions.

    const MathGlyphConstruction* mathGlyphConstruction(const SharedBuffer& buffer, Glyph glyph, bool isVertical) const
    {
        uint32_t count = uint16_t(verticalGlyphCount) + uint16_t(horizontalGlyphCount);
        if (!isValidEnd(buffer, &mathGlyphConstructionsOffset[count]))
            return nullptr;

        // We determine the coverage table for the specified glyph.
        uint16_t coverageOffset = isVertical ? verticalGlyphCoverageOffset : horizontalGlyphCoverageOffset;
        if (!coverageOffset)
            return nullptr;
        const CoverageTable* coverage = validateOffset<CoverageTable>(buffer, coverageOffset);
        if (!coverage)
            return nullptr;

        // We determine the index in the mathGlyphConstructionsOffset table.
        uint32_t i;
        if (!getCoverageIndex(buffer, coverage, glyph, i))
            return nullptr;
        count = isVertical ? verticalGlyphCount : horizontalGlyphCount;
        if (i >= count)
            return nullptr;
        if (!isVertical)
            i += uint16_t(verticalGlyphCount);

        return validateOffset<MathGlyphConstruction>(buffer, mathGlyphConstructionsOffset[i]);
    }
};

struct MATHTable : TableBase {
    OpenType::Fixed version;
    OpenType::Offset mathConstantsOffset;
    OpenType::Offset mathGlyphInfoOffset;
    OpenType::Offset mathVariantsOffset;

    const MathConstants* mathConstants(const SharedBuffer& buffer) const
    {
        uint16_t offset = mathConstantsOffset;
        if (offset)
            return validateOffset<MathConstants>(buffer, offset);
        return nullptr;
    }

    const MathGlyphInfo* mathGlyphInfo(const SharedBuffer& buffer) const
    {
        uint16_t offset = mathGlyphInfoOffset;
        if (offset)
            return validateOffset<MathGlyphInfo>(buffer, offset);
        return nullptr;
    }

    const MathVariants* mathVariants(const SharedBuffer& buffer) const
    {
        uint16_t offset = mathVariantsOffset;
        if (offset)
            return validateOffset<MathVariants>(buffer, offset);
        return nullptr;
    }
};

#pragma pack()

} // namespace OpenType
#endif // ENABLE(OPENTYPE_MATH)

#if ENABLE(OPENTYPE_MATH)
OpenTypeMathData::OpenTypeMathData(const FontPlatformData& font)
{
    m_mathBuffer = font.openTypeTable(OpenType::MATHTag);
    const OpenType::MATHTable* math = OpenType::validateTable<OpenType::MATHTable>(m_mathBuffer);
    if (!math) {
        m_mathBuffer = nullptr;
        return;
    }

    const OpenType::MathConstants* mathConstants = math->mathConstants(*m_mathBuffer);
    if (!mathConstants) {
        m_mathBuffer = nullptr;
        return;
    }

    const OpenType::MathVariants* mathVariants = math->mathVariants(*m_mathBuffer);
    if (!mathVariants)
        m_mathBuffer = nullptr;
}
#elif USE(HARFBUZZ)
OpenTypeMathData::OpenTypeMathData(const FontPlatformData& font)
    : m_mathFont(font.createOpenTypeMathHarfBuzzFont())
{
}
#elif USE(DIRECT2D)
OpenTypeMathData::OpenTypeMathData(const FontPlatformData& font)
{
}
#else
OpenTypeMathData::OpenTypeMathData(const FontPlatformData&) = default;
#endif

OpenTypeMathData::~OpenTypeMathData() = default;

#if ENABLE(OPENTYPE_MATH)
float OpenTypeMathData::getMathConstant(const Font& font, MathConstant constant) const
{
    int32_t value = 0;

    const OpenType::MATHTable* math = OpenType::validateTable<OpenType::MATHTable>(m_mathBuffer);
    ASSERT(math);
    const OpenType::MathConstants* mathConstants = math->mathConstants(*m_mathBuffer);
    ASSERT(mathConstants);

    if (constant >= 0 && constant <= ScriptScriptPercentScaleDown)
        value = int16_t(mathConstants->intConstants[constant]);
    else if (constant >= DelimitedSubFormulaMinHeight && constant <= DisplayOperatorMinHeight)
        value = uint16_t(mathConstants->uIntConstants[constant - DelimitedSubFormulaMinHeight]);
    else if (constant >= MathLeading && constant <= RadicalKernAfterDegree)
        value = int16_t(mathConstants->mathValuesConstants[constant - MathLeading].value);
    else if (constant == RadicalDegreeBottomRaisePercent)
        value = uint16_t(mathConstants->radicalDegreeBottomRaisePercent);

    if (constant == ScriptPercentScaleDown || constant == ScriptScriptPercentScaleDown || constant == RadicalDegreeBottomRaisePercent)
        return value / 100.0;

    return value * font.sizePerUnit();
#elif USE(HARFBUZZ)
float OpenTypeMathData::getMathConstant(const Font& font, MathConstant constant) const
{
    hb_position_t value = hb_ot_math_get_constant(m_mathFont.get(), static_cast<hb_ot_math_constant_t>(constant));
    if (constant == ScriptPercentScaleDown || constant == ScriptScriptPercentScaleDown || constant == RadicalDegreeBottomRaisePercent)
        return value / 100.0;

    return value * font.sizePerUnit();
#else
float OpenTypeMathData::getMathConstant(const Font&, MathConstant) const
{
    ASSERT_NOT_REACHED();
    return 0;
#endif
}

#if ENABLE(OPENTYPE_MATH)
float OpenTypeMathData::getItalicCorrection(const Font& font, Glyph glyph) const
{
    const OpenType::MATHTable* math = OpenType::validateTable<OpenType::MATHTable>(m_mathBuffer);
    ASSERT(math);
    const OpenType::MathGlyphInfo* mathGlyphInfo = math->mathGlyphInfo(*m_mathBuffer);
    if (!mathGlyphInfo)
        return 0;

    const OpenType::MathItalicsCorrectionInfo* mathItalicsCorrectionInfo = mathGlyphInfo->mathItalicsCorrectionInfo(*m_mathBuffer);
    if (!mathItalicsCorrectionInfo)
        return 0;

    return mathItalicsCorrectionInfo->getItalicCorrection(*m_mathBuffer, glyph) * font.sizePerUnit();
#elif USE(HARFBUZZ)
float OpenTypeMathData::getItalicCorrection(const Font& font, Glyph glyph) const
{
    return hb_ot_math_get_glyph_italics_correction(m_mathFont.get(), glyph) * font.sizePerUnit();
#else
float OpenTypeMathData::getItalicCorrection(const Font&, Glyph) const
{
    ASSERT_NOT_REACHED();
    return 0;
#endif
}

#if ENABLE(OPENTYPE_MATH)
void OpenTypeMathData::getMathVariants(Glyph glyph, bool isVertical, Vector<Glyph>& sizeVariants, Vector<AssemblyPart>& assemblyParts) const
{
    sizeVariants.clear();
    assemblyParts.clear();
    const OpenType::MATHTable* math = OpenType::validateTable<OpenType::MATHTable>(m_mathBuffer);
    ASSERT(math);
    const OpenType::MathVariants* mathVariants = math->mathVariants(*m_mathBuffer);
    ASSERT(mathVariants);

    const OpenType::MathGlyphConstruction* mathGlyphConstruction = mathVariants->mathGlyphConstruction(*m_mathBuffer, glyph, isVertical);
    if (!mathGlyphConstruction)
        return;

    mathGlyphConstruction->getSizeVariants(*m_mathBuffer, sizeVariants);
    mathGlyphConstruction->getAssemblyParts(*m_mathBuffer, assemblyParts);
#elif USE(HARFBUZZ)
void OpenTypeMathData::getMathVariants(Glyph glyph, bool isVertical, Vector<Glyph>& sizeVariants, Vector<AssemblyPart>& assemblyParts) const
{
    hb_direction_t direction = isVertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;

    sizeVariants.clear();
    hb_ot_math_glyph_variant_t variants[10];
    unsigned variantsSize = WTF_ARRAY_LENGTH(variants);
    unsigned count;
    unsigned offset = 0;
    do {
        count = variantsSize;
        hb_ot_math_get_glyph_variants(m_mathFont.get(), glyph, direction, offset, &count, variants);
        offset += count;
        for (unsigned i = 0; i < count; i++)
            sizeVariants.append(variants[i].glyph);
    } while (count == variantsSize);

    assemblyParts.clear();
    hb_ot_math_glyph_part_t parts[10];
    unsigned partsSize = WTF_ARRAY_LENGTH(parts);
    offset = 0;
    do {
        count = partsSize;
        hb_ot_math_get_glyph_assembly(m_mathFont.get(), glyph, direction, offset, &count, parts, nullptr);
        offset += count;
        for (unsigned i = 0; i < count; i++) {
            AssemblyPart assemblyPart;
            assemblyPart.glyph = parts[i].glyph;
            assemblyPart.isExtender = parts[i].flags & HB_MATH_GLYPH_PART_FLAG_EXTENDER;
            assemblyParts.append(assemblyPart);
        }
    } while (count == partsSize);
#else
void OpenTypeMathData::getMathVariants(Glyph, bool, Vector<Glyph>&, Vector<AssemblyPart>&) const
{
    ASSERT_NOT_REACHED();
#endif
}

} // namespace WebCore
