/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SVGToOTFFontConversion.h"

#if ENABLE(SVG_FONTS)

#include "CSSStyleDeclaration.h"
#include "ElementChildIterator.h"
#include "Glyph.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGGlyphElement.h"
#include "SVGHKernElement.h"
#include "SVGMissingGlyphElement.h"
#include "SVGPathParser.h"
#include "SVGPathStringSource.h"
#include "SVGVKernElement.h"
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace WebCore {

template <typename V>
static inline void append32(V& result, uint32_t value)
{
    result.append(value >> 24);
    result.append(value >> 16);
    result.append(value >> 8);
    result.append(value);
}

class SVGToOTFFontConverter {
public:
    SVGToOTFFontConverter(const SVGFontElement&);
    bool convertSVGToOTFFont();

    Vector<char> releaseResult()
    {
        return WTFMove(m_result);
    }

    bool error() const
    {
        return m_error;
    }

private:
    struct GlyphData {
        GlyphData(Vector<char>&& charString, const SVGGlyphElement* glyphElement, float horizontalAdvance, float verticalAdvance, FloatRect boundingBox, const String& codepoints)
            : boundingBox(boundingBox)
            , charString(charString)
            , codepoints(codepoints)
            , glyphElement(glyphElement)
            , horizontalAdvance(horizontalAdvance)
            , verticalAdvance(verticalAdvance)
        {
        }
        FloatRect boundingBox;
        Vector<char> charString;
        String codepoints;
        const SVGGlyphElement* glyphElement;
        float horizontalAdvance;
        float verticalAdvance;
    };

    class Placeholder {
    public:
        Placeholder(SVGToOTFFontConverter& converter, size_t baseOfOffset)
            : m_converter(converter)
            , m_baseOfOffset(baseOfOffset)
            , m_location(m_converter.m_result.size())
        {
            m_converter.append16(0);
        }

        Placeholder(Placeholder&& other)
            : m_converter(other.m_converter)
            , m_baseOfOffset(other.m_baseOfOffset)
            , m_location(other.m_location)
#if !ASSERT_DISABLED
            , m_active(other.m_active)
#endif
        {
#if !ASSERT_DISABLED
            other.m_active = false;
#endif
        }

        void populate()
        {
            ASSERT(m_active);
            size_t delta = m_converter.m_result.size() - m_baseOfOffset;
            ASSERT(delta < std::numeric_limits<uint16_t>::max());
            m_converter.overwrite16(m_location, delta);
#if !ASSERT_DISABLED
            m_active = false;
#endif
        }

        ~Placeholder()
        {
            ASSERT(!m_active);
        }

    private:
        SVGToOTFFontConverter& m_converter;
        const size_t m_baseOfOffset;
        const size_t m_location;
#if !ASSERT_DISABLED
        bool m_active = { true };
#endif
    };

    struct KerningData {
        KerningData(uint16_t glyph1, uint16_t glyph2, int16_t adjustment)
            : glyph1(glyph1)
            , glyph2(glyph2)
            , adjustment(adjustment)
        {
        }
        uint16_t glyph1;
        uint16_t glyph2;
        int16_t adjustment;
    };

    Placeholder placeholder(size_t baseOfOffset)
    {
        return Placeholder(*this, baseOfOffset);
    }

    void append32(uint32_t value)
    {
        WebCore::append32(m_result, value);
    }

    void append32BitCode(const char code[4])
    {
        m_result.append(code[0]);
        m_result.append(code[1]);
        m_result.append(code[2]);
        m_result.append(code[3]);
    }

    void append16(uint16_t value)
    {
        m_result.append(value >> 8);
        m_result.append(value);
    }

    void grow(size_t delta)
    {
        m_result.grow(m_result.size() + delta);
    }

    void overwrite32(unsigned location, uint32_t value)
    {
        ASSERT(m_result.size() >= location + 4);
        m_result[location] = value >> 24;
        m_result[location + 1] = value >> 16;
        m_result[location + 2] = value >> 8;
        m_result[location + 3] = value;
    }

    void overwrite16(unsigned location, uint16_t value)
    {
        ASSERT(m_result.size() >= location + 2);
        m_result[location] = value >> 8;
        m_result[location + 1] = value;
    }

    static const size_t headerSize = 12;
    static const size_t directoryEntrySize = 16;

    uint32_t calculateChecksum(size_t startingOffset, size_t endingOffset) const;

    void processGlyphElement(const SVGElement& glyphOrMissingGlyphElement, const SVGGlyphElement*, float defaultHorizontalAdvance, float defaultVerticalAdvance, const String& codepoints, Optional<FloatRect>& boundingBox);

    typedef void (SVGToOTFFontConverter::*FontAppendingFunction)();
    void appendTable(const char identifier[4], FontAppendingFunction);
    void appendFormat12CMAPTable(const Vector<std::pair<UChar32, Glyph>>& codepointToGlyphMappings);
    void appendFormat4CMAPTable(const Vector<std::pair<UChar32, Glyph>>& codepointToGlyphMappings);
    void appendCMAPTable();
    void appendGSUBTable();
    void appendHEADTable();
    void appendHHEATable();
    void appendHMTXTable();
    void appendVHEATable();
    void appendVMTXTable();
    void appendKERNTable();
    void appendMAXPTable();
    void appendNAMETable();
    void appendOS2Table();
    void appendPOSTTable();
    void appendCFFTable();
    void appendVORGTable();

    void appendLigatureGlyphs();
    static bool compareCodepointsLexicographically(const GlyphData&, const GlyphData&);

    void appendValidCFFString(const String&);

    Vector<char> transcodeGlyphPaths(float width, const SVGElement& glyphOrMissingGlyphElement, Optional<FloatRect>& boundingBox) const;

    void addCodepointRanges(const UnicodeRanges&, HashSet<Glyph>& glyphSet) const;
    void addCodepoints(const HashSet<String>& codepoints, HashSet<Glyph>& glyphSet) const;
    void addGlyphNames(const HashSet<String>& glyphNames, HashSet<Glyph>& glyphSet) const;
    void addKerningPair(Vector<KerningData>&, const SVGKerningPair&) const;
    template<typename T> size_t appendKERNSubtable(bool (T::*buildKerningPair)(SVGKerningPair&) const, uint16_t coverage);
    size_t finishAppendingKERNSubtable(Vector<KerningData>, uint16_t coverage);

    void appendLigatureSubtable(size_t subtableRecordLocation);
    void appendArabicReplacementSubtable(size_t subtableRecordLocation, const char arabicForm[]);
    void appendScriptSubtable(unsigned featureCount);
    Vector<Glyph, 1> glyphsForCodepoint(UChar32) const;
    Glyph firstGlyph(const Vector<Glyph, 1>&, UChar32) const;

    template<typename T> T scaleUnitsPerEm(T value) const
    {
        return value * s_outputUnitsPerEm / m_inputUnitsPerEm;
    }

    Vector<GlyphData> m_glyphs;
    HashMap<String, Glyph> m_glyphNameToIndexMap; // SVG 1.1: "It is recommended that glyph names be unique within a font."
    HashMap<String, Vector<Glyph, 1>> m_codepointsToIndicesMap;
    Vector<char> m_result;
    Vector<char, 17> m_emptyGlyphCharString;
    FloatRect m_boundingBox;
    const SVGFontElement& m_fontElement;
    const SVGFontFaceElement* m_fontFaceElement;
    const SVGMissingGlyphElement* m_missingGlyphElement;
    String m_fontFamily;
    float m_advanceWidthMax;
    float m_advanceHeightMax;
    float m_minRightSideBearing;
    static const unsigned s_outputUnitsPerEm = 1000;
    unsigned m_inputUnitsPerEm;
    int m_lineGap;
    int m_xHeight;
    int m_capHeight;
    int m_ascent;
    int m_descent;
    unsigned m_featureCountGSUB;
    unsigned m_tablesAppendedCount;
    uint8_t m_weight;
    bool m_italic;
    bool m_error { false };
};

static uint16_t roundDownToPowerOfTwo(uint16_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    return (x >> 1) + 1;
}

static uint16_t integralLog2(uint16_t x)
{
    uint16_t result = 0;
    while (x >>= 1)
        ++result;
    return result;
}

void SVGToOTFFontConverter::appendFormat12CMAPTable(const Vector<std::pair<UChar32, Glyph>>& mappings)
{
    // Braindead scheme: One segment for each character
    ASSERT(m_glyphs.size() < 0xFFFF);
    auto subtableLocation = m_result.size();
    append32(12 << 16); // Format 12
    append32(0); // Placeholder for byte length
    append32(0); // Language independent
    append32(0); // Placeholder for nGroups
    for (auto& mapping : mappings) {
        append32(mapping.first); // startCharCode
        append32(mapping.first); // endCharCode
        append32(mapping.second); // startGlyphCode
    }
    overwrite32(subtableLocation + 4, m_result.size() - subtableLocation);
    overwrite32(subtableLocation + 12, mappings.size());
}

void SVGToOTFFontConverter::appendFormat4CMAPTable(const Vector<std::pair<UChar32, Glyph>>& bmpMappings)
{
    auto subtableLocation = m_result.size();
    append16(4); // Format 4
    append16(0); // Placeholder for length in bytes
    append16(0); // Language independent
    uint16_t segCount = bmpMappings.size() + 1;
    append16(clampTo<uint16_t>(2 * segCount)); // segCountX2: "2 x segCount"
    uint16_t originalSearchRange = roundDownToPowerOfTwo(segCount);
    uint16_t searchRange = clampTo<uint16_t>(2 * originalSearchRange); // searchRange: "2 x (2**floor(log2(segCount)))"
    append16(searchRange);
    append16(integralLog2(originalSearchRange)); // entrySelector: "log2(searchRange/2)"  
    append16(clampTo<uint16_t>((2 * segCount) - searchRange)); // rangeShift: "2 x segCount - searchRange"  

    // Ending character codes
    for (auto& mapping : bmpMappings)
        append16(mapping.first); // startCharCode
    append16(0xFFFF);

    append16(0); // reserved

    // Starting character codes
    for (auto& mapping : bmpMappings)
        append16(mapping.first); // startCharCode
    append16(0xFFFF);

    // idDelta
    for (auto& mapping : bmpMappings)
        append16(static_cast<uint16_t>(mapping.second) - static_cast<uint16_t>(mapping.first)); // startCharCode
    append16(0x0001);

    // idRangeOffset
    for (size_t i = 0; i < bmpMappings.size(); ++i)
        append16(0); // startCharCode
    append16(0);

    // Fonts strive to hold 2^16 glyphs, but with the current encoding scheme, we write 8 bytes per codepoint into this subtable.
    // Because the size of this subtable must be represented as a 16-bit number, we are limiting the number of glyphs we support to 2^13.
    // FIXME: If we hit this limit in the wild, use a more compact encoding scheme for this subtable.
    overwrite16(subtableLocation + 2, clampTo<uint16_t>(m_result.size() - subtableLocation));
}

void SVGToOTFFontConverter::appendCMAPTable()
{
    auto startingOffset = m_result.size();
    append16(0);
    append16(3); // Number of subtables

    append16(0); // Unicode
    append16(3); // Unicode version 2.2+
    append32(28); // Byte offset of subtable

    append16(3); // Microsoft
    append16(1); // Unicode BMP
    auto format4OffsetLocation = m_result.size();
    append32(0); // Byte offset of subtable

    append16(3); // Microsoft
    append16(10); // Unicode
    append32(28); // Byte offset of subtable

    Vector<std::pair<UChar32, Glyph>> mappings;
    UChar32 previousCodepoint = std::numeric_limits<UChar32>::max();
    for (size_t i = 0; i < m_glyphs.size(); ++i) {
        auto& glyph = m_glyphs[i];
        UChar32 codepoint;
        auto codePoints = StringView(glyph.codepoints).codePoints();
        auto iterator = codePoints.begin();
        if (iterator == codePoints.end())
            codepoint = 0;
        else {
            codepoint = *iterator;
            ++iterator;
            // Don't map ligatures here.
            if (iterator != codePoints.end() || codepoint == previousCodepoint)
                continue;
        }

        mappings.append(std::make_pair(codepoint, Glyph(i)));
        previousCodepoint = codepoint;
    }

    appendFormat12CMAPTable(mappings);

    Vector<std::pair<UChar32, Glyph>> bmpMappings;
    for (auto& mapping : mappings) {
        if (mapping.first < 0x10000)
            bmpMappings.append(mapping);
    }
    overwrite32(format4OffsetLocation, m_result.size() - startingOffset);
    appendFormat4CMAPTable(bmpMappings);
}

void SVGToOTFFontConverter::appendHEADTable()
{
    append32(0x00010000); // Version
    append32(0x00010000); // Revision
    append32(0); // Checksum placeholder; to be overwritten by the caller.
    append32(0x5F0F3CF5); // Magic number.
    append16((1 << 9) | 1);

    append16(s_outputUnitsPerEm);
    append32(0); // First half of creation date
    append32(0); // Last half of creation date
    append32(0); // First half of modification date
    append32(0); // Last half of modification date
    append16(clampTo<int16_t>(m_boundingBox.x()));
    append16(clampTo<int16_t>(m_boundingBox.y()));
    append16(clampTo<int16_t>(m_boundingBox.maxX()));
    append16(clampTo<int16_t>(m_boundingBox.maxY()));
    append16((m_italic ? 1 << 1 : 0) | (m_weight >= 7 ? 1 : 0));
    append16(3); // Smallest readable size in pixels
    append16(0); // Might contain LTR or RTL glyphs
    append16(0); // Short offsets in the 'loca' table. However, CFF fonts don't have a 'loca' table so this is irrelevant
    append16(0); // Glyph data format
}

void SVGToOTFFontConverter::appendHHEATable()
{
    append32(0x00010000); // Version
    append16(clampTo<int16_t>(m_ascent));
    append16(clampTo<int16_t>(-m_descent));
    // WebKit SVG font rendering has hard coded the line gap to be 1/10th of the font size since 2008 (see r29719).
    append16(clampTo<int16_t>(m_lineGap));
    append16(clampTo<uint16_t>(m_advanceWidthMax));
    append16(clampTo<int16_t>(m_boundingBox.x())); // Minimum left side bearing
    append16(clampTo<int16_t>(m_minRightSideBearing)); // Minimum right side bearing
    append16(clampTo<int16_t>(m_boundingBox.maxX())); // X maximum extent
    // Since WebKit draws the caret and ignores the following values, it doesn't matter what we set them to.
    append16(1); // Vertical caret
    append16(0); // Vertical caret
    append16(0); // "Set value to 0 for non-slanted fonts"
    append32(0); // Reserved
    append32(0); // Reserved
    append16(0); // Current format
    append16(m_glyphs.size()); // Number of advance widths in HMTX table
}

void SVGToOTFFontConverter::appendHMTXTable()
{
    for (auto& glyph : m_glyphs) {
        append16(clampTo<uint16_t>(glyph.horizontalAdvance));
        append16(clampTo<int16_t>(glyph.boundingBox.x()));
    }
}

void SVGToOTFFontConverter::appendMAXPTable()
{
    append32(0x00010000); // Version
    append16(m_glyphs.size());
    append16(0xFFFF); // Maximum number of points in non-compound glyph
    append16(0xFFFF); // Maximum number of contours in non-compound glyph
    append16(0xFFFF); // Maximum number of points in compound glyph
    append16(0xFFFF); // Maximum number of contours in compound glyph
    append16(2); // Maximum number of zones
    append16(0); // Maximum number of points used in zone 0
    append16(0); // Maximum number of storage area locations
    append16(0); // Maximum number of function definitions
    append16(0); // Maximum number of instruction definitions
    append16(0); // Maximum stack depth
    append16(0); // Maximum size of instructions
    append16(m_glyphs.size()); // Maximum number of glyphs referenced at top level
    append16(0); // No compound glyphs
}

void SVGToOTFFontConverter::appendNAMETable()
{
    append16(0); // Format selector
    append16(1); // Number of name records in table
    append16(18); // Offset in bytes to the beginning of name character strings

    append16(0); // Unicode
    append16(3); // Unicode version 2.0 or later
    append16(0); // Language
    append16(1); // Name identifier. 1 = Font family
    append16(m_fontFamily.length() * 2);
    append16(0); // Offset into name data

    for (auto codeUnit : StringView(m_fontFamily).codeUnits())
        append16(codeUnit);
}

void SVGToOTFFontConverter::appendOS2Table()
{
    int16_t averageAdvance = s_outputUnitsPerEm;
    bool ok;
    int value = m_fontElement.attributeWithoutSynchronization(SVGNames::horiz_adv_xAttr).toInt(&ok);
    if (!ok && m_missingGlyphElement)
        value = m_missingGlyphElement->attributeWithoutSynchronization(SVGNames::horiz_adv_xAttr).toInt(&ok);
    value = scaleUnitsPerEm(value);
    if (ok)
        averageAdvance = clampTo<int16_t>(value);

    append16(2); // Version
    append16(clampTo<int16_t>(averageAdvance));
    append16(m_weight); // Weight class
    append16(5); // Width class
    append16(0); // Protected font
    // WebKit handles these superscripts and subscripts
    append16(0); // Subscript X Size
    append16(0); // Subscript Y Size
    append16(0); // Subscript X Offset
    append16(0); // Subscript Y Offset
    append16(0); // Superscript X Size
    append16(0); // Superscript Y Size
    append16(0); // Superscript X Offset
    append16(0); // Superscript Y Offset
    append16(0); // Strikeout width
    append16(0); // Strikeout Position
    append16(0); // No classification

    unsigned numPanoseBytes = 0;
    const unsigned panoseSize = 10;
    char panoseBytes[panoseSize];
    if (m_fontFaceElement) {
        Vector<String> segments = m_fontFaceElement->attributeWithoutSynchronization(SVGNames::panose_1Attr).string().split(' ');
        if (segments.size() == panoseSize) {
            for (auto& segment : segments) {
                bool ok;
                int value = segment.toInt(&ok);
                if (ok && value >= std::numeric_limits<uint8_t>::min() && value <= std::numeric_limits<uint8_t>::max())
                    panoseBytes[numPanoseBytes++] = value;
            }
        }
    }
    if (numPanoseBytes != panoseSize)
        memset(panoseBytes, 0, panoseSize);
    m_result.append(panoseBytes, panoseSize);

    for (int i = 0; i < 4; ++i)
        append32(0); // "Bit assignments are pending. Set to 0"
    append32(0x544B4257); // Font Vendor. "WBKT"
    append16((m_weight >= 7 ? 1 << 5 : 0) | (m_italic ? 1 : 0)); // Font Patterns.
    append16(0); // First unicode index
    append16(0xFFFF); // Last unicode index
    append16(clampTo<int16_t>(m_ascent)); // Typographical ascender
    append16(clampTo<int16_t>(-m_descent)); // Typographical descender
    append16(clampTo<int16_t>(m_lineGap)); // Typographical line gap
    append16(clampTo<uint16_t>(m_ascent)); // Windows-specific ascent
    append16(clampTo<uint16_t>(m_descent)); // Windows-specific descent
    append32(0xFF10FC07); // Bitmask for supported codepages (Part 1). Report all pages as supported.
    append32(0x0000FFFF); // Bitmask for supported codepages (Part 2). Report all pages as supported.
    append16(clampTo<int16_t>(m_xHeight)); // x-height
    append16(clampTo<int16_t>(m_capHeight)); // Cap-height
    append16(0); // Default char
    append16(' '); // Break character
    append16(3); // Maximum context needed to perform font features
    append16(3); // Smallest optical point size
    append16(0xFFFF); // Largest optical point size
}

void SVGToOTFFontConverter::appendPOSTTable()
{
    append32(0x00030000); // Format. Printing is undefined
    append32(0); // Italic angle in degrees
    append16(0); // Underline position
    append16(0); // Underline thickness
    append32(0); // Monospaced
    append32(0); // "Minimum memory usage when a TrueType font is downloaded as a Type 42 font"
    append32(0); // "Maximum memory usage when a TrueType font is downloaded as a Type 42 font"
    append32(0); // "Minimum memory usage when a TrueType font is downloaded as a Type 1 font"
    append32(0); // "Maximum memory usage when a TrueType font is downloaded as a Type 1 font"
}

static bool isValidStringForCFF(const String& string)
{
    for (auto c : StringView(string).codeUnits()) {
        if (c < 33 || c > 126)
            return false;
    }
    return true;
}

void SVGToOTFFontConverter::appendValidCFFString(const String& string)
{
    ASSERT(isValidStringForCFF(string));
    for (auto c : StringView(string).codeUnits())
        m_result.append(c);
}

void SVGToOTFFontConverter::appendCFFTable()
{
    auto startingOffset = m_result.size();

    // Header
    m_result.append(1); // Major version
    m_result.append(0); // Minor version
    m_result.append(4); // Header size
    m_result.append(4); // Offsets within CFF table are 4 bytes long

    // Name INDEX
    String fontName;
    if (m_fontFaceElement) {
        // FIXME: fontFamily() here might not be quite what we want.
        String potentialFontName = m_fontFamily;
        if (isValidStringForCFF(potentialFontName))
            fontName = potentialFontName;
    }
    append16(1); // INDEX contains 1 element
    m_result.append(4); // Offsets in this INDEX are 4 bytes long
    append32(1); // 1-index offset of name data
    append32(fontName.length() + 1); // 1-index offset just past end of name data
    appendValidCFFString(fontName);

    String weight;
    if (m_fontFaceElement) {
        auto& potentialWeight = m_fontFaceElement->attributeWithoutSynchronization(SVGNames::font_weightAttr);
        if (isValidStringForCFF(potentialWeight))
            weight = potentialWeight;
    }

    bool hasWeight = !weight.isNull();

    const char operand32Bit = 29;
    const char fullNameKey = 2;
    const char familyNameKey = 3;
    const char weightKey = 4;
    const char fontBBoxKey = 5;
    const char charsetIndexKey = 15;
    const char charstringsIndexKey = 17;
    const char privateDictIndexKey = 18;
    const uint32_t userDefinedStringStartIndex = 391;
    const unsigned sizeOfTopIndex = 56 + (hasWeight ? 6 : 0);

    // Top DICT INDEX.
    append16(1); // INDEX contains 1 element
    m_result.append(4); // Offsets in this INDEX are 4 bytes long
    append32(1); // 1-index offset of DICT data
    append32(1 + sizeOfTopIndex); // 1-index offset just past end of DICT data

    // DICT information
#if !ASSERT_DISABLED
    unsigned topDictStart = m_result.size();
#endif
    m_result.append(operand32Bit);
    append32(userDefinedStringStartIndex);
    m_result.append(fullNameKey);
    m_result.append(operand32Bit);
    append32(userDefinedStringStartIndex);
    m_result.append(familyNameKey);
    if (hasWeight) {
        m_result.append(operand32Bit);
        append32(userDefinedStringStartIndex + 2);
        m_result.append(weightKey);
    }
    m_result.append(operand32Bit);
    append32(clampTo<int32_t>(m_boundingBox.x()));
    m_result.append(operand32Bit);
    append32(clampTo<int32_t>(m_boundingBox.y()));
    m_result.append(operand32Bit);
    append32(clampTo<int32_t>(m_boundingBox.width()));
    m_result.append(operand32Bit);
    append32(clampTo<int32_t>(m_boundingBox.height()));
    m_result.append(fontBBoxKey);
    m_result.append(operand32Bit);
    unsigned charsetOffsetLocation = m_result.size();
    append32(0); // Offset of Charset info. Will be overwritten later.
    m_result.append(charsetIndexKey);
    m_result.append(operand32Bit);
    unsigned charstringsOffsetLocation = m_result.size();
    append32(0); // Offset of CharStrings INDEX. Will be overwritten later.
    m_result.append(charstringsIndexKey);
    m_result.append(operand32Bit);
    append32(0); // 0-sized private dict
    m_result.append(operand32Bit);
    append32(0); // no location for private dict
    m_result.append(privateDictIndexKey); // Private dict size and offset
    ASSERT(m_result.size() == topDictStart + sizeOfTopIndex);

    // String INDEX
    String unknownCharacter = "UnknownChar"_s;
    append16(2 + (hasWeight ? 1 : 0)); // Number of elements in INDEX
    m_result.append(4); // Offsets in this INDEX are 4 bytes long
    uint32_t offset = 1;
    append32(offset);
    offset += fontName.length();
    append32(offset);
    offset += unknownCharacter.length();
    append32(offset);
    if (hasWeight) {
        offset += weight.length();
        append32(offset);
    }
    appendValidCFFString(fontName);
    appendValidCFFString(unknownCharacter);
    appendValidCFFString(weight);

    append16(0); // Empty subroutine INDEX

    // Charset info
    overwrite32(charsetOffsetLocation, m_result.size() - startingOffset);
    m_result.append(0);
    for (Glyph i = 1; i < m_glyphs.size(); ++i)
        append16(userDefinedStringStartIndex + 1);

    // CharStrings INDEX
    overwrite32(charstringsOffsetLocation, m_result.size() - startingOffset);
    append16(m_glyphs.size());
    m_result.append(4); // Offsets in this INDEX are 4 bytes long
    offset = 1;
    append32(offset);
    for (auto& glyph : m_glyphs) {
        offset += glyph.charString.size();
        append32(offset);
    }
    for (auto& glyph : m_glyphs)
        m_result.appendVector(glyph.charString);
}

Glyph SVGToOTFFontConverter::firstGlyph(const Vector<Glyph, 1>& v, UChar32 codepoint) const
{
#if ASSERT_DISABLED
    UNUSED_PARAM(codepoint);
#endif
    ASSERT(!v.isEmpty());
    if (v.isEmpty())
        return 0;
#if !ASSERT_DISABLED
    auto codePoints = StringView(m_glyphs[v[0]].codepoints).codePoints();
    auto codePointsIterator = codePoints.begin();
    ASSERT(codePointsIterator != codePoints.end());
    ASSERT(codepoint == *codePointsIterator);
#endif
    return v[0];
}

void SVGToOTFFontConverter::appendLigatureSubtable(size_t subtableRecordLocation)
{
    typedef std::pair<Vector<Glyph, 3>, Glyph> LigaturePair;
    Vector<LigaturePair> ligaturePairs;
    for (Glyph glyphIndex = 0; glyphIndex < m_glyphs.size(); ++glyphIndex) {
        ligaturePairs.append(LigaturePair(Vector<Glyph, 3>(), glyphIndex));
        Vector<Glyph, 3>& ligatureGlyphs = ligaturePairs.last().first;
        auto codePoints = StringView(m_glyphs[glyphIndex].codepoints).codePoints();
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=138592 This needs to be done in codepoint space, not glyph space
        for (auto codePoint : codePoints)
            ligatureGlyphs.append(firstGlyph(glyphsForCodepoint(codePoint), codePoint));
        if (ligatureGlyphs.size() < 2)
            ligaturePairs.removeLast();
    }
    if (ligaturePairs.size() > std::numeric_limits<uint16_t>::max())
        ligaturePairs.clear();
    std::sort(ligaturePairs.begin(), ligaturePairs.end(), [](auto& lhs, auto& rhs) {
        return lhs.first[0] < rhs.first[0];
    });
    Vector<size_t> overlappingFirstGlyphSegmentLengths;
    if (!ligaturePairs.isEmpty()) {
        Glyph previousFirstGlyph = ligaturePairs[0].first[0];
        size_t segmentStart = 0;
        for (size_t i = 0; i < ligaturePairs.size(); ++i) {
            auto& ligaturePair = ligaturePairs[i];
            if (ligaturePair.first[0] != previousFirstGlyph) {
                overlappingFirstGlyphSegmentLengths.append(i - segmentStart);
                segmentStart = i;
                previousFirstGlyph = ligaturePairs[0].first[0];
            }
        }
        overlappingFirstGlyphSegmentLengths.append(ligaturePairs.size() - segmentStart);
    }

    overwrite16(subtableRecordLocation + 6, m_result.size() - subtableRecordLocation);
    auto subtableLocation = m_result.size();
    append16(1); // Format 1
    append16(0); // Placeholder for offset to coverage table, relative to beginning of substitution table
    append16(ligaturePairs.size()); // Number of LigatureSet tables
    grow(overlappingFirstGlyphSegmentLengths.size() * 2); // Placeholder for offset to LigatureSet table

    Vector<size_t> ligatureSetTableLocations;
    for (size_t i = 0; i < overlappingFirstGlyphSegmentLengths.size(); ++i) {
        overwrite16(subtableLocation + 6 + 2 * i, m_result.size() - subtableLocation);
        ligatureSetTableLocations.append(m_result.size());
        append16(overlappingFirstGlyphSegmentLengths[i]); // LigatureCount
        grow(overlappingFirstGlyphSegmentLengths[i] * 2); // Placeholder for offset to Ligature table
    }
    ASSERT(ligatureSetTableLocations.size() == overlappingFirstGlyphSegmentLengths.size());

    size_t ligaturePairIndex = 0;
    for (size_t i = 0; i < overlappingFirstGlyphSegmentLengths.size(); ++i) {
        for (size_t j = 0; j < overlappingFirstGlyphSegmentLengths[i]; ++j) {
            overwrite16(ligatureSetTableLocations[i] + 2 + 2 * j, m_result.size() - ligatureSetTableLocations[i]);
            auto& ligaturePair = ligaturePairs[ligaturePairIndex];
            append16(ligaturePair.second);
            append16(ligaturePair.first.size());
            for (size_t k = 1; k < ligaturePair.first.size(); ++k)
                append16(ligaturePair.first[k]);
            ++ligaturePairIndex;
        }
    }
    ASSERT(ligaturePairIndex == ligaturePairs.size());

    // Coverage table
    overwrite16(subtableLocation + 2, m_result.size() - subtableLocation);
    append16(1); // CoverageFormat
    append16(ligatureSetTableLocations.size()); // GlyphCount
    ligaturePairIndex = 0;
    for (auto segmentLength : overlappingFirstGlyphSegmentLengths) {
        auto& ligaturePair = ligaturePairs[ligaturePairIndex];
        ASSERT(ligaturePair.first.size() > 1);
        append16(ligaturePair.first[0]);
        ligaturePairIndex += segmentLength;
    }
}

void SVGToOTFFontConverter::appendArabicReplacementSubtable(size_t subtableRecordLocation, const char arabicForm[])
{
    Vector<std::pair<Glyph, Glyph>> arabicFinalReplacements;
    for (auto& pair : m_codepointsToIndicesMap) {
        for (auto glyphIndex : pair.value) {
            auto& glyph = m_glyphs[glyphIndex];
            if (glyph.glyphElement && equalIgnoringASCIICase(glyph.glyphElement->attributeWithoutSynchronization(SVGNames::arabic_formAttr), arabicForm))
                arabicFinalReplacements.append(std::make_pair(pair.value[0], glyphIndex));
        }
    }
    if (arabicFinalReplacements.size() > std::numeric_limits<uint16_t>::max())
        arabicFinalReplacements.clear();

    overwrite16(subtableRecordLocation + 6, m_result.size() - subtableRecordLocation);
    auto subtableLocation = m_result.size();
    append16(2); // Format 2
    Placeholder toCoverageTable = placeholder(subtableLocation);
    append16(arabicFinalReplacements.size()); // GlyphCount
    for (auto& pair : arabicFinalReplacements)
        append16(pair.second);

    toCoverageTable.populate();
    append16(1); // CoverageFormat
    append16(arabicFinalReplacements.size()); // GlyphCount
    for (auto& pair : arabicFinalReplacements)
        append16(pair.first);
}

void SVGToOTFFontConverter::appendScriptSubtable(unsigned featureCount)
{
    auto dfltScriptTableLocation = m_result.size();
    append16(0); // Placeholder for offset of default language system table, relative to beginning of Script table
    append16(0); // Number of following language system tables

    // LangSys table
    overwrite16(dfltScriptTableLocation, m_result.size() - dfltScriptTableLocation);
    append16(0); // LookupOrder "= NULL ... reserved"
    append16(0xFFFF); // No features are required
    append16(featureCount); // Number of FeatureIndex values
    for (uint16_t i = 0; i < featureCount; ++i)
        append16(m_featureCountGSUB++); // Features indices
}

void SVGToOTFFontConverter::appendGSUBTable()
{
    auto tableLocation = m_result.size();
    auto headerSize = 10;

    append32(0x00010000); // Version
    append16(headerSize); // Offset to ScriptList
    Placeholder toFeatureList = placeholder(tableLocation);
    Placeholder toLookupList = placeholder(tableLocation);
    ASSERT(tableLocation + headerSize == m_result.size());

    // ScriptList
    auto scriptListLocation = m_result.size();
    append16(2); // Number of ScriptRecords
    append32BitCode("DFLT");
    append16(0); // Placeholder for offset of Script table, relative to beginning of ScriptList
    append32BitCode("arab");
    append16(0); // Placeholder for offset of Script table, relative to beginning of ScriptList

    overwrite16(scriptListLocation + 6, m_result.size() - scriptListLocation);
    appendScriptSubtable(1);
    overwrite16(scriptListLocation + 12, m_result.size() - scriptListLocation);
    appendScriptSubtable(4);

    const unsigned featureCount = 5;

    // FeatureList
    toFeatureList.populate();
    auto featureListLocation = m_result.size();
    size_t featureListSize = 2 + 6 * featureCount;
    size_t featureTableSize = 6;
    append16(featureCount); // FeatureCount
    append32BitCode("liga");
    append16(featureListSize + featureTableSize * 0); // Offset of feature table, relative to beginning of FeatureList table
    append32BitCode("fina");
    append16(featureListSize + featureTableSize * 1); // Offset of feature table, relative to beginning of FeatureList table
    append32BitCode("medi");
    append16(featureListSize + featureTableSize * 2); // Offset of feature table, relative to beginning of FeatureList table
    append32BitCode("init");
    append16(featureListSize + featureTableSize * 3); // Offset of feature table, relative to beginning of FeatureList table
    append32BitCode("rlig");
    append16(featureListSize + featureTableSize * 4); // Offset of feature table, relative to beginning of FeatureList table
    ASSERT_UNUSED(featureListLocation, featureListLocation + featureListSize == m_result.size());

    for (unsigned i = 0; i < featureCount; ++i) {
        auto featureTableStart = m_result.size();
        append16(0); // FeatureParams "= NULL ... reserved"
        append16(1); // LookupCount
        append16(i); // LookupListIndex
        ASSERT_UNUSED(featureTableStart, featureTableStart + featureTableSize == m_result.size());
    }

    // LookupList
    toLookupList.populate();
    auto lookupListLocation = m_result.size();
    append16(featureCount); // LookupCount
    for (unsigned i = 0; i < featureCount; ++i)
        append16(0); // Placeholder for offset to feature table, relative to beginning of LookupList
    size_t subtableRecordLocations[featureCount];
    for (unsigned i = 0; i < featureCount; ++i) {
        subtableRecordLocations[i] = m_result.size();
        overwrite16(lookupListLocation + 2 + 2 * i, m_result.size() - lookupListLocation);
        switch (i) {
        case 4:
            append16(3); // Type 3: "Replace one glyph with one of many glyphs"
            break;
        case 0:
            append16(4); // Type 4: "Replace multiple glyphs with one glyph"
            break;
        default:
            append16(1); // Type 1: "Replace one glyph with one glyph"
            break;
        }
        append16(0); // LookupFlag
        append16(1); // SubTableCount
        append16(0); // Placeholder for offset to subtable, relative to beginning of Lookup table
    }

    appendLigatureSubtable(subtableRecordLocations[0]);
    appendArabicReplacementSubtable(subtableRecordLocations[1], "terminal");
    appendArabicReplacementSubtable(subtableRecordLocations[2], "medial");
    appendArabicReplacementSubtable(subtableRecordLocations[3], "initial");

    // Manually append empty "rlig" subtable
    overwrite16(subtableRecordLocations[4] + 6, m_result.size() - subtableRecordLocations[4]);
    append16(1); // Format 1
    append16(6); // offset to coverage table, relative to beginning of substitution table
    append16(0); // AlternateSetCount
    append16(1); // CoverageFormat
    append16(0); // GlyphCount
}

void SVGToOTFFontConverter::appendVORGTable()
{
    append16(1); // Major version
    append16(0); // Minor version

    bool ok;
    int defaultVerticalOriginY = m_fontElement.attributeWithoutSynchronization(SVGNames::vert_origin_yAttr).toInt(&ok);
    if (!ok && m_missingGlyphElement)
        defaultVerticalOriginY = m_missingGlyphElement->attributeWithoutSynchronization(SVGNames::vert_origin_yAttr).toInt();
    defaultVerticalOriginY = scaleUnitsPerEm(defaultVerticalOriginY);
    append16(clampTo<int16_t>(defaultVerticalOriginY));

    auto tableSizeOffset = m_result.size();
    append16(0); // Place to write table size.
    for (Glyph i = 0; i < m_glyphs.size(); ++i) {
        if (auto* glyph = m_glyphs[i].glyphElement) {
            if (int verticalOriginY = glyph->attributeWithoutSynchronization(SVGNames::vert_origin_yAttr).toInt()) {
                append16(i);
                append16(clampTo<int16_t>(scaleUnitsPerEm(verticalOriginY)));
            }
        }
    }
    ASSERT(!((m_result.size() - tableSizeOffset - 2) % 4));
    overwrite16(tableSizeOffset, (m_result.size() - tableSizeOffset - 2) / 4);
}

void SVGToOTFFontConverter::appendVHEATable()
{
    float height = m_ascent + m_descent;
    append32(0x00011000); // Version
    append16(clampTo<int16_t>(height / 2)); // Vertical typographic ascender (vertical baseline to the right)
    append16(clampTo<int16_t>(-static_cast<int>(height / 2))); // Vertical typographic descender
    append16(clampTo<int16_t>(s_outputUnitsPerEm / 10)); // Vertical typographic line gap
    // FIXME: m_unitsPerEm is almost certainly not correct
    append16(clampTo<int16_t>(m_advanceHeightMax));
    append16(clampTo<int16_t>(s_outputUnitsPerEm - m_boundingBox.maxY())); // Minimum top side bearing
    append16(clampTo<int16_t>(m_boundingBox.y())); // Minimum bottom side bearing
    append16(clampTo<int16_t>(s_outputUnitsPerEm - m_boundingBox.y())); // Y maximum extent
    // Since WebKit draws the caret and ignores the following values, it doesn't matter what we set them to.
    append16(1); // Vertical caret
    append16(0); // Vertical caret
    append16(0); // "Set value to 0 for non-slanted fonts"
    append32(0); // Reserved
    append32(0); // Reserved
    append16(0); // "Set to 0"
    append16(m_glyphs.size()); // Number of advance heights in VMTX table
}

void SVGToOTFFontConverter::appendVMTXTable()
{
    for (auto& glyph : m_glyphs) {
        append16(clampTo<uint16_t>(glyph.verticalAdvance));
        append16(clampTo<int16_t>(s_outputUnitsPerEm - glyph.boundingBox.maxY())); // top side bearing
    }
}

static String codepointToString(UChar32 codepoint)
{
    UChar buffer[2];
    uint8_t length = 0;
    UBool error = false;
    U16_APPEND(buffer, length, 2, codepoint, error);
    return error ? String() : String(buffer, length);
}

Vector<Glyph, 1> SVGToOTFFontConverter::glyphsForCodepoint(UChar32 codepoint) const
{
    return m_codepointsToIndicesMap.get(codepointToString(codepoint));
}

void SVGToOTFFontConverter::addCodepointRanges(const UnicodeRanges& unicodeRanges, HashSet<Glyph>& glyphSet) const
{
    for (auto& unicodeRange : unicodeRanges) {
        for (auto codepoint = unicodeRange.first; codepoint <= unicodeRange.second; ++codepoint) {
            for (auto index : glyphsForCodepoint(codepoint))
                glyphSet.add(index);
        }
    }
}

void SVGToOTFFontConverter::addCodepoints(const HashSet<String>& codepoints, HashSet<Glyph>& glyphSet) const
{
    for (auto& codepointString : codepoints) {
        for (auto index : m_codepointsToIndicesMap.get(codepointString))
            glyphSet.add(index);
    }
}

void SVGToOTFFontConverter::addGlyphNames(const HashSet<String>& glyphNames, HashSet<Glyph>& glyphSet) const
{
    for (auto& glyphName : glyphNames) {
        if (Glyph glyph = m_glyphNameToIndexMap.get(glyphName))
            glyphSet.add(glyph);
    }
}

void SVGToOTFFontConverter::addKerningPair(Vector<KerningData>& data, const SVGKerningPair& kerningPair) const
{
    HashSet<Glyph> glyphSet1;
    HashSet<Glyph> glyphSet2;

    addCodepointRanges(kerningPair.unicodeRange1, glyphSet1);
    addCodepointRanges(kerningPair.unicodeRange2, glyphSet2);
    addGlyphNames(kerningPair.glyphName1, glyphSet1);
    addGlyphNames(kerningPair.glyphName2, glyphSet2);
    addCodepoints(kerningPair.unicodeName1, glyphSet1);
    addCodepoints(kerningPair.unicodeName2, glyphSet2);

    // FIXME: Use table format 2 so we don't have to append each of these one by one.
    for (auto& glyph1 : glyphSet1) {
        for (auto& glyph2 : glyphSet2)
            data.append(KerningData(glyph1, glyph2, clampTo<int16_t>(-scaleUnitsPerEm(kerningPair.kerning))));
    }
}

template<typename T> inline size_t SVGToOTFFontConverter::appendKERNSubtable(bool (T::*buildKerningPair)(SVGKerningPair&) const, uint16_t coverage)
{
    Vector<KerningData> kerningData;
    for (auto& element : childrenOfType<T>(m_fontElement)) {
        SVGKerningPair kerningPair;
        if ((element.*buildKerningPair)(kerningPair))
            addKerningPair(kerningData, kerningPair);
    }
    return finishAppendingKERNSubtable(WTFMove(kerningData), coverage);
}

size_t SVGToOTFFontConverter::finishAppendingKERNSubtable(Vector<KerningData> kerningData, uint16_t coverage)
{
    std::sort(kerningData.begin(), kerningData.end(), [](auto& a, auto& b) {
        return a.glyph1 < b.glyph1 || (a.glyph1 == b.glyph1 && a.glyph2 < b.glyph2);
    });

    size_t sizeOfKerningDataTable = 14 + 6 * kerningData.size();
    if (sizeOfKerningDataTable > std::numeric_limits<uint16_t>::max()) {
        kerningData.clear();
        sizeOfKerningDataTable = 14;
    }

    append16(0); // Version of subtable
    append16(sizeOfKerningDataTable); // Length of this subtable
    append16(coverage); // Table coverage bitfield

    uint16_t roundedNumKerningPairs = roundDownToPowerOfTwo(kerningData.size());

    append16(kerningData.size());
    append16(roundedNumKerningPairs * 6); // searchRange: "The largest power of two less than or equal to the value of nPairs, multiplied by the size in bytes of an entry in the table."
    append16(integralLog2(roundedNumKerningPairs)); // entrySelector: "log2 of the largest power of two less than or equal to the value of nPairs."
    append16((kerningData.size() - roundedNumKerningPairs) * 6); // rangeShift: "The value of nPairs minus the largest power of two less than or equal to nPairs,
                                                                        // and then multiplied by the size in bytes of an entry in the table."

    for (auto& kerningDataElement : kerningData) {
        append16(kerningDataElement.glyph1);
        append16(kerningDataElement.glyph2);
        append16(kerningDataElement.adjustment);
    }

    return sizeOfKerningDataTable;
}

void SVGToOTFFontConverter::appendKERNTable()
{
    append16(0); // Version
    append16(2); // Number of subtables

#if !ASSERT_DISABLED
    auto subtablesOffset = m_result.size();
#endif

    size_t sizeOfHorizontalSubtable = appendKERNSubtable<SVGHKernElement>(&SVGHKernElement::buildHorizontalKerningPair, 1);
    ASSERT_UNUSED(sizeOfHorizontalSubtable, subtablesOffset + sizeOfHorizontalSubtable == m_result.size());
    size_t sizeOfVerticalSubtable = appendKERNSubtable<SVGVKernElement>(&SVGVKernElement::buildVerticalKerningPair, 0);
    ASSERT_UNUSED(sizeOfVerticalSubtable, subtablesOffset + sizeOfHorizontalSubtable + sizeOfVerticalSubtable == m_result.size());
}

template <typename V>
static void writeCFFEncodedNumber(V& vector, float number)
{
    vector.append(0xFF);
    // Convert to 16.16 fixed-point
    append32(vector, clampTo<int32_t>(number * 0x10000));
}

static const char rLineTo = 0x05;
static const char rrCurveTo = 0x08;
static const char endChar = 0x0e;
static const char rMoveTo = 0x15;

class CFFBuilder final : public SVGPathConsumer {
public:
    CFFBuilder(Vector<char>& cffData, float width, FloatPoint origin, float unitsPerEmScalar)
        : m_cffData(cffData)
        , m_unitsPerEmScalar(unitsPerEmScalar)
    {
        writeCFFEncodedNumber(m_cffData, std::floor(width)); // hmtx table can't encode fractional FUnit values, and the CFF table needs to agree with hmtx.
        writeCFFEncodedNumber(m_cffData, origin.x());
        writeCFFEncodedNumber(m_cffData, origin.y());
        m_cffData.append(rMoveTo);
    }

    Optional<FloatRect> boundingBox() const
    {
        return m_boundingBox;
    }

private:
    void updateBoundingBox(FloatPoint point)
    {
        if (!m_boundingBox) {
            m_boundingBox = FloatRect(point, FloatSize());
            return;
        }
        m_boundingBox.value().extend(point);
    }

    void writePoint(FloatPoint destination)
    {
        updateBoundingBox(destination);

        FloatSize delta = destination - m_current;
        writeCFFEncodedNumber(m_cffData, delta.width());
        writeCFFEncodedNumber(m_cffData, delta.height());

        m_current = destination;
    }

    void moveTo(const FloatPoint& targetPoint, bool closed, PathCoordinateMode mode) final
    {
        if (closed && !m_cffData.isEmpty())
            closePath();

        FloatPoint scaledTargetPoint = FloatPoint(targetPoint.x() * m_unitsPerEmScalar, targetPoint.y() * m_unitsPerEmScalar);
        FloatPoint destination = mode == AbsoluteCoordinates ? scaledTargetPoint : m_current + scaledTargetPoint;

        writePoint(destination);
        m_cffData.append(rMoveTo);

        m_startingPoint = m_current;
    }

    void unscaledLineTo(const FloatPoint& targetPoint)
    {
        writePoint(targetPoint);
        m_cffData.append(rLineTo);
    }

    void lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode) final
    {
        FloatPoint scaledTargetPoint = FloatPoint(targetPoint.x() * m_unitsPerEmScalar, targetPoint.y() * m_unitsPerEmScalar);
        FloatPoint destination = mode == AbsoluteCoordinates ? scaledTargetPoint : m_current + scaledTargetPoint;

        unscaledLineTo(destination);
    }

    void curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& point3, PathCoordinateMode mode) final
    {
        FloatPoint scaledPoint1 = FloatPoint(point1.x() * m_unitsPerEmScalar, point1.y() * m_unitsPerEmScalar);
        FloatPoint scaledPoint2 = FloatPoint(point2.x() * m_unitsPerEmScalar, point2.y() * m_unitsPerEmScalar);
        FloatPoint scaledPoint3 = FloatPoint(point3.x() * m_unitsPerEmScalar, point3.y() * m_unitsPerEmScalar);

        if (mode == RelativeCoordinates) {
            scaledPoint1 += m_current;
            scaledPoint2 += m_current;
            scaledPoint3 += m_current;
        }

        writePoint(scaledPoint1);
        writePoint(scaledPoint2);
        writePoint(scaledPoint3);
        m_cffData.append(rrCurveTo);
    }

    void closePath() final
    {
        if (m_current != m_startingPoint)
            unscaledLineTo(m_startingPoint);
    }

    void incrementPathSegmentCount() final { }
    bool continueConsuming() final { return true; }

    void lineToHorizontal(float, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void lineToVertical(float, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToCubicSmooth(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToQuadratic(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToQuadraticSmooth(const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void arcTo(float, float, float, bool, bool, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }

    Vector<char>& m_cffData;
    FloatPoint m_startingPoint;
    FloatPoint m_current;
    Optional<FloatRect> m_boundingBox;
    float m_unitsPerEmScalar;
};

Vector<char> SVGToOTFFontConverter::transcodeGlyphPaths(float width, const SVGElement& glyphOrMissingGlyphElement, Optional<FloatRect>& boundingBox) const
{
    Vector<char> result;

    auto& dAttribute = glyphOrMissingGlyphElement.attributeWithoutSynchronization(SVGNames::dAttr);
    if (dAttribute.isEmpty()) {
        writeCFFEncodedNumber(result, width);
        writeCFFEncodedNumber(result, 0);
        writeCFFEncodedNumber(result, 0);
        result.append(rMoveTo);
        result.append(endChar);
        return result;
    }

    // FIXME: If we are vertical, use vert_origin_x and vert_origin_y
    bool ok;
    float horizontalOriginX = scaleUnitsPerEm(glyphOrMissingGlyphElement.attributeWithoutSynchronization(SVGNames::horiz_origin_xAttr).toFloat(&ok));
    if (!ok && m_fontFaceElement)
        horizontalOriginX = scaleUnitsPerEm(m_fontFaceElement->horizontalOriginX());
    float horizontalOriginY = scaleUnitsPerEm(glyphOrMissingGlyphElement.attributeWithoutSynchronization(SVGNames::horiz_origin_yAttr).toFloat(&ok));
    if (!ok && m_fontFaceElement)
        horizontalOriginY = scaleUnitsPerEm(m_fontFaceElement->horizontalOriginY());

    CFFBuilder builder(result, width, FloatPoint(horizontalOriginX, horizontalOriginY), static_cast<float>(s_outputUnitsPerEm) / m_inputUnitsPerEm);
    SVGPathStringSource source(dAttribute);

    ok = SVGPathParser::parse(source, builder);
    if (!ok)
        return { };

    boundingBox = builder.boundingBox();

    result.append(endChar);
    return result;
}

void SVGToOTFFontConverter::processGlyphElement(const SVGElement& glyphOrMissingGlyphElement, const SVGGlyphElement* glyphElement, float defaultHorizontalAdvance, float defaultVerticalAdvance, const String& codepoints, Optional<FloatRect>& boundingBox)
{
    bool ok;
    float horizontalAdvance = scaleUnitsPerEm(glyphOrMissingGlyphElement.attributeWithoutSynchronization(SVGNames::horiz_adv_xAttr).toFloat(&ok));
    if (!ok)
        horizontalAdvance = defaultHorizontalAdvance;
    m_advanceWidthMax = std::max(m_advanceWidthMax, horizontalAdvance);
    float verticalAdvance = scaleUnitsPerEm(glyphOrMissingGlyphElement.attributeWithoutSynchronization(SVGNames::vert_adv_yAttr).toFloat(&ok));
    if (!ok)
        verticalAdvance = defaultVerticalAdvance;
    m_advanceHeightMax = std::max(m_advanceHeightMax, verticalAdvance);

    Optional<FloatRect> glyphBoundingBox;
    auto path = transcodeGlyphPaths(horizontalAdvance, glyphOrMissingGlyphElement, glyphBoundingBox);
    if (!path.size()) {
        // It's better to use a fallback font rather than use a font without all its glyphs.
        m_error = true;
    }
    if (!boundingBox)
        boundingBox = glyphBoundingBox;
    else if (glyphBoundingBox)
        boundingBox.value().unite(glyphBoundingBox.value());
    if (glyphBoundingBox)
        m_minRightSideBearing = std::min(m_minRightSideBearing, horizontalAdvance - glyphBoundingBox.value().maxX());

    m_glyphs.append(GlyphData(WTFMove(path), glyphElement, horizontalAdvance, verticalAdvance, glyphBoundingBox.valueOr(FloatRect()), codepoints));
}

void SVGToOTFFontConverter::appendLigatureGlyphs()
{
    HashSet<UChar32> ligatureCodepoints;
    HashSet<UChar32> nonLigatureCodepoints;
    for (auto& glyph : m_glyphs) {
        auto codePoints = StringView(glyph.codepoints).codePoints();
        auto codePointsIterator = codePoints.begin();
        if (codePointsIterator == codePoints.end())
            continue;
        UChar32 codepoint = *codePointsIterator;
        ++codePointsIterator;
        if (codePointsIterator == codePoints.end())
            nonLigatureCodepoints.add(codepoint);
        else {
            ligatureCodepoints.add(codepoint);
            for (; codePointsIterator != codePoints.end(); ++codePointsIterator)
                ligatureCodepoints.add(*codePointsIterator);
        }
    }

    for (auto codepoint : nonLigatureCodepoints)
        ligatureCodepoints.remove(codepoint);
    for (auto codepoint : ligatureCodepoints) {
        auto codepoints = codepointToString(codepoint);
        if (!codepoints.isNull())
            m_glyphs.append(GlyphData(Vector<char>(m_emptyGlyphCharString), nullptr, s_outputUnitsPerEm, s_outputUnitsPerEm, FloatRect(), codepoints));
    }
}

bool SVGToOTFFontConverter::compareCodepointsLexicographically(const GlyphData& data1, const GlyphData& data2)
{
    auto codePoints1 = StringView(data1.codepoints).codePoints();
    auto codePoints2 = StringView(data2.codepoints).codePoints();
    auto iterator1 = codePoints1.begin();
    auto iterator2 = codePoints2.begin();
    while (iterator1 != codePoints1.end() && iterator2 != codePoints2.end()) {
        UChar32 codepoint1, codepoint2;
        codepoint1 = *iterator1;
        codepoint2 = *iterator2;

        if (codepoint1 < codepoint2)
            return true;
        if (codepoint1 > codepoint2)
            return false;

        ++iterator1;
        ++iterator2;
    }

    if (iterator1 == codePoints1.end() && iterator2 == codePoints2.end()) {
        bool firstIsIsolated = data1.glyphElement && equalLettersIgnoringASCIICase(data1.glyphElement->attributeWithoutSynchronization(SVGNames::arabic_formAttr), "isolated");
        bool secondIsIsolated = data2.glyphElement && equalLettersIgnoringASCIICase(data2.glyphElement->attributeWithoutSynchronization(SVGNames::arabic_formAttr), "isolated");
        return firstIsIsolated && !secondIsIsolated;
    }
    return iterator1 == codePoints1.end();
}

static void populateEmptyGlyphCharString(Vector<char, 17>& o, unsigned unitsPerEm)
{
    writeCFFEncodedNumber(o, unitsPerEm);
    writeCFFEncodedNumber(o, 0);
    writeCFFEncodedNumber(o, 0);
    o.append(rMoveTo);
    o.append(endChar);
}

SVGToOTFFontConverter::SVGToOTFFontConverter(const SVGFontElement& fontElement)
    : m_fontElement(fontElement)
    , m_fontFaceElement(childrenOfType<SVGFontFaceElement>(m_fontElement).first())
    , m_missingGlyphElement(childrenOfType<SVGMissingGlyphElement>(m_fontElement).first())
    , m_advanceWidthMax(0)
    , m_advanceHeightMax(0)
    , m_minRightSideBearing(std::numeric_limits<float>::max())
    , m_featureCountGSUB(0)
    , m_tablesAppendedCount(0)
    , m_weight(5)
    , m_italic(false)
{
    if (!m_fontFaceElement) {
        m_inputUnitsPerEm = 1;
        m_ascent = s_outputUnitsPerEm;
        m_descent = 1;
        m_xHeight = s_outputUnitsPerEm;
        m_capHeight = m_ascent;
    } else {
        m_inputUnitsPerEm = m_fontFaceElement->unitsPerEm();
        m_ascent = scaleUnitsPerEm(m_fontFaceElement->ascent());
        m_descent = scaleUnitsPerEm(m_fontFaceElement->descent());
        m_xHeight = scaleUnitsPerEm(m_fontFaceElement->xHeight());
        m_capHeight = scaleUnitsPerEm(m_fontFaceElement->capHeight());

        // Some platforms, including OS X, use 0 ascent and descent to mean that the platform should synthesize
        // a value based on a heuristic. However, SVG fonts can legitimately have 0 for ascent or descent.
        // Specifing a single FUnit gets us as close to 0 as we can without triggering the synthesis.
        if (!m_ascent)
            m_ascent = 1;
        if (!m_descent)
            m_descent = 1;
    }

    float defaultHorizontalAdvance = m_fontFaceElement ? scaleUnitsPerEm(m_fontFaceElement->horizontalAdvanceX()) : 0;
    float defaultVerticalAdvance = m_fontFaceElement ? scaleUnitsPerEm(m_fontFaceElement->verticalAdvanceY()) : 0;

    m_lineGap = s_outputUnitsPerEm / 10;

    populateEmptyGlyphCharString(m_emptyGlyphCharString, s_outputUnitsPerEm);

    Optional<FloatRect> boundingBox;
    if (m_missingGlyphElement)
        processGlyphElement(*m_missingGlyphElement, nullptr, defaultHorizontalAdvance, defaultVerticalAdvance, String(), boundingBox);
    else {
        m_glyphs.append(GlyphData(Vector<char>(m_emptyGlyphCharString), nullptr, s_outputUnitsPerEm, s_outputUnitsPerEm, FloatRect(), String()));
        boundingBox = FloatRect(0, 0, s_outputUnitsPerEm, s_outputUnitsPerEm);
    }

    for (auto& glyphElement : childrenOfType<SVGGlyphElement>(m_fontElement)) {
        auto& unicodeAttribute = glyphElement.attributeWithoutSynchronization(SVGNames::unicodeAttr);
        if (!unicodeAttribute.isEmpty()) // If we can never actually trigger this glyph, ignore it completely
            processGlyphElement(glyphElement, &glyphElement, defaultHorizontalAdvance, defaultVerticalAdvance, unicodeAttribute, boundingBox);
    }

    m_boundingBox = boundingBox.valueOr(FloatRect());

    appendLigatureGlyphs();

    if (m_glyphs.size() > std::numeric_limits<Glyph>::max()) {
        m_glyphs.clear();
        return;
    }

    std::sort(m_glyphs.begin(), m_glyphs.end(), &compareCodepointsLexicographically);

    for (Glyph i = 0; i < m_glyphs.size(); ++i) {
        GlyphData& glyph = m_glyphs[i];
        if (glyph.glyphElement) {
            auto& glyphName = glyph.glyphElement->attributeWithoutSynchronization(SVGNames::glyph_nameAttr);
            if (!glyphName.isNull())
                m_glyphNameToIndexMap.add(glyphName, i);
        }
        if (m_codepointsToIndicesMap.isValidKey(glyph.codepoints)) {
            auto& glyphVector = m_codepointsToIndicesMap.add(glyph.codepoints, Vector<Glyph>()).iterator->value;
            // Prefer isolated arabic forms
            if (glyph.glyphElement && equalLettersIgnoringASCIICase(glyph.glyphElement->attributeWithoutSynchronization(SVGNames::arabic_formAttr), "isolated"))
                glyphVector.insert(0, i);
            else
                glyphVector.append(i);
        }
    }

    // FIXME: Handle commas.
    if (m_fontFaceElement) {
        auto& fontWeightAttribute = m_fontFaceElement->attributeWithoutSynchronization(SVGNames::font_weightAttr);
        for (auto segment : StringView(fontWeightAttribute).split(' ')) {
            if (equalLettersIgnoringASCIICase(segment, "bold")) {
                m_weight = 7;
                break;
            }
            bool ok;
            int value = segment.toInt(ok);
            if (ok && value >= 0 && value < 1000) {
                m_weight = std::max(std::min((value + 50) / 100, static_cast<int>(std::numeric_limits<uint8_t>::max())), static_cast<int>(std::numeric_limits<uint8_t>::min()));
                break;
            }
        }
        auto& fontStyleAttribute = m_fontFaceElement->attributeWithoutSynchronization(SVGNames::font_styleAttr);
        for (auto segment : StringView(fontStyleAttribute).split(' ')) {
            if (equalLettersIgnoringASCIICase(segment, "italic") || equalLettersIgnoringASCIICase(segment, "oblique")) {
                m_italic = true;
                break;
            }
        }
    }

    if (m_fontFaceElement)
        m_fontFamily = m_fontFaceElement->fontFamily();
}

static inline bool isFourByteAligned(size_t x)
{
    return !(x & 3);
}

uint32_t SVGToOTFFontConverter::calculateChecksum(size_t startingOffset, size_t endingOffset) const
{
    ASSERT(isFourByteAligned(endingOffset - startingOffset));
    uint32_t sum = 0;
    for (size_t offset = startingOffset; offset < endingOffset; offset += 4) {
        sum += static_cast<unsigned char>(m_result[offset + 3])
            | (static_cast<unsigned char>(m_result[offset + 2]) << 8)
            | (static_cast<unsigned char>(m_result[offset + 1]) << 16)
            | (static_cast<unsigned char>(m_result[offset]) << 24);
    }
    return sum;
}

void SVGToOTFFontConverter::appendTable(const char identifier[4], FontAppendingFunction appendingFunction)
{
    size_t offset = m_result.size();
    ASSERT(isFourByteAligned(offset));
    (this->*appendingFunction)();
    size_t unpaddedSize = m_result.size() - offset;
    while (!isFourByteAligned(m_result.size()))
        m_result.append(0);
    ASSERT(isFourByteAligned(m_result.size()));
    size_t directoryEntryOffset = headerSize + m_tablesAppendedCount * directoryEntrySize;
    m_result[directoryEntryOffset] = identifier[0];
    m_result[directoryEntryOffset + 1] = identifier[1];
    m_result[directoryEntryOffset + 2] = identifier[2];
    m_result[directoryEntryOffset + 3] = identifier[3];
    overwrite32(directoryEntryOffset + 4, calculateChecksum(offset, m_result.size()));
    overwrite32(directoryEntryOffset + 8, offset);
    overwrite32(directoryEntryOffset + 12, unpaddedSize);
    ++m_tablesAppendedCount;
}

bool SVGToOTFFontConverter::convertSVGToOTFFont()
{
    if (m_glyphs.isEmpty())
        return false;

    uint16_t numTables = 14;
    uint16_t roundedNumTables = roundDownToPowerOfTwo(numTables);
    uint16_t searchRange = roundedNumTables * 16; // searchRange: "(Maximum power of 2 <= numTables) x 16."

    m_result.append('O');
    m_result.append('T');
    m_result.append('T');
    m_result.append('O');
    append16(numTables);
    append16(searchRange);
    append16(integralLog2(roundedNumTables)); // entrySelector: "Log2(maximum power of 2 <= numTables)."
    append16(numTables * 16 - searchRange); // rangeShift: "NumTables x 16-searchRange."

    ASSERT(m_result.size() == headerSize);

    // Leave space for the directory entries.
    for (size_t i = 0; i < directoryEntrySize * numTables; ++i)
        m_result.append(0);

    appendTable("CFF ", &SVGToOTFFontConverter::appendCFFTable);
    appendTable("GSUB", &SVGToOTFFontConverter::appendGSUBTable);
    appendTable("OS/2", &SVGToOTFFontConverter::appendOS2Table);
    appendTable("VORG", &SVGToOTFFontConverter::appendVORGTable);
    appendTable("cmap", &SVGToOTFFontConverter::appendCMAPTable);
    auto headTableOffset = m_result.size();
    appendTable("head", &SVGToOTFFontConverter::appendHEADTable);
    appendTable("hhea", &SVGToOTFFontConverter::appendHHEATable);
    appendTable("hmtx", &SVGToOTFFontConverter::appendHMTXTable);
    appendTable("kern", &SVGToOTFFontConverter::appendKERNTable);
    appendTable("maxp", &SVGToOTFFontConverter::appendMAXPTable);
    appendTable("name", &SVGToOTFFontConverter::appendNAMETable);
    appendTable("post", &SVGToOTFFontConverter::appendPOSTTable);
    appendTable("vhea", &SVGToOTFFontConverter::appendVHEATable);
    appendTable("vmtx", &SVGToOTFFontConverter::appendVMTXTable);

    ASSERT(numTables == m_tablesAppendedCount);

    // checksumAdjustment: "To compute: set it to 0, calculate the checksum for the 'head' table and put it in the table directory,
    // sum the entire font as uint32, then store B1B0AFBA - sum. The checksum for the 'head' table will now be wrong. That is OK."
    overwrite32(headTableOffset + 8, 0xB1B0AFBAU - calculateChecksum(0, m_result.size()));
    return true;
}

Optional<Vector<char>> convertSVGToOTFFont(const SVGFontElement& element)
{
    SVGToOTFFontConverter converter(element);
    if (converter.error())
        return WTF::nullopt;
    if (!converter.convertSVGToOTFFont())
        return WTF::nullopt;
    return converter.releaseResult();
}

}

#endif // ENABLE(SVG_FONTS)
