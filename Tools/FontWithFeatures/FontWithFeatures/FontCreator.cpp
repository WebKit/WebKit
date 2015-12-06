/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "FontCreator.h"

#include <array>
#include <cassert>
#include <string>

static const size_t headerSize = 12;
static const size_t directoryEntrySize = 16;
static const int16_t unitsPerEm = 1024;
static const uint16_t numGlyphs = 26 * 2 + 1;

static inline uint16_t integralLog2(uint16_t x)
{
    uint16_t result = 0;
    while (x >>= 1)
        ++result;
    return result;
}

static inline uint16_t roundDownToPowerOfTwo(uint16_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    return (x >> 1) + 1;
}

static inline bool isFourByteAligned(size_t x)
{
    return !(x & 3);
}

// Assumption: T2 can hold every value that a T1 can hold.
template<typename T1, typename T2> static inline T1 clampTo(T2 x)
{
    x = std::min(x, static_cast<T2>(std::numeric_limits<T1>::max()));
    x = std::max(x, static_cast<T2>(std::numeric_limits<T1>::min()));
    return static_cast<T1>(x);
}

template <typename V>
static inline void append32(V& result, uint32_t value)
{
    result.push_back(value >> 24);
    result.push_back(value >> 16);
    result.push_back(value >> 8);
    result.push_back(value);
}

template <typename V>
static void writeCFFEncodedNumber(V& vector, float number)
{
    vector.push_back(0xFF);
    // Convert to 16.16 fixed-point
    append32(vector, clampTo<int32_t>(number * 0x10000));
}

static const char rLineTo = 0x05;
static const char rrCurveTo = 0x08;
static const char endChar = 0x0e;
static const char rMoveTo = 0x15;

class CFFBuilder {
public:
    CFFBuilder(float width, std::pair<float, float> origin)
    {
        writeCFFEncodedNumber(result, width);
        writeCFFEncodedNumber(result, origin.first);
        writeCFFEncodedNumber(result, origin.second);
        result.push_back(rMoveTo);
    }

    std::vector<uint8_t> takeResult()
    {
        result.push_back(endChar);
        return std::move(result);
    }

    void moveTo(std::pair<float, float> targetPoint, bool closed)
    {
        if (closed && !result.empty())
            closePath();

        std::pair<float, float> destination = targetPoint;

        writePoint(destination);
        result.push_back(rMoveTo);

        startingPoint = current;
    }

    void lineTo(std::pair<float, float> targetPoint)
    {
        std::pair<float, float> destination = targetPoint;

        writePoint(destination);
        result.push_back(rLineTo);
    }

    void curveToCubic(std::pair<float, float> point1, std::pair<float, float> point2, std::pair<float, float> targetPoint)
    {
        std::pair<float, float> destination1 = point1;
        std::pair<float, float> destination2 = point2;
        std::pair<float, float> destination3 = targetPoint;

        writePoint(destination1);
        writePoint(destination2);
        writePoint(destination3);
        result.push_back(rrCurveTo);
    }

    void closePath()
    {
        if (current != startingPoint)
            lineTo(startingPoint);
    }

private:
    void writePoint(std::pair<float, float> destination)
    {
        std::pair<float, float> delta = std::make_pair(destination.first - current.first, destination.second - current.second);
        writeCFFEncodedNumber(result, delta.first);
        writeCFFEncodedNumber(result, delta.second);

        current = destination;
    }

    std::vector<uint8_t> result;
    std::pair<float, float> startingPoint;
    std::pair<float, float> current;
};

std::vector<uint8_t> generateBoxCharString()
{
    CFFBuilder builder(unitsPerEm, std::make_pair(0.f, 0.f));
    builder.moveTo(std::make_pair(200.f, 200.f), false);
    builder.lineTo(std::make_pair(200.f, 800.f));
    builder.lineTo(std::make_pair(800.f, 800.f));
    builder.lineTo(std::make_pair(800.f, 200.f));
    builder.closePath();
    return builder.takeResult();
}

std::vector<uint8_t> generateCheckCharString()
{
    CFFBuilder builder(unitsPerEm, std::make_pair(0.f, 0.f));
    builder.moveTo(std::make_pair(200.f, 500.f), false);
    builder.lineTo(std::make_pair(250.f, 550.f));
    builder.lineTo(std::make_pair(500.f, 300.f));
    builder.lineTo(std::make_pair(900.f, 700.f));
    builder.lineTo(std::make_pair(950.f, 650.f));
    builder.lineTo(std::make_pair(500.f, 200.f));
    builder.closePath();
    return builder.takeResult();
}

std::vector<uint8_t> generateXCharString()
{
    CFFBuilder builder(unitsPerEm, std::make_pair(0.f, 0.f));
    builder.moveTo(std::make_pair(500.0f, 550.0f), false);
    builder.lineTo(std::make_pair(900.f, 950.f));
    builder.lineTo(std::make_pair(950.f, 900.f));
    builder.lineTo(std::make_pair(550.f, 500.f));
    builder.lineTo(std::make_pair(950.f, 100.f));
    builder.lineTo(std::make_pair(900.f,  50.f));
    builder.lineTo(std::make_pair(500.f, 450.f));
    builder.lineTo(std::make_pair(100.f,  50.f));
    builder.lineTo(std::make_pair(50.f , 100.f));
    builder.lineTo(std::make_pair(450.f, 500.f));
    builder.lineTo(std::make_pair(50.f , 900.f));
    builder.lineTo(std::make_pair(100.f, 950.f));
    builder.closePath();
    return builder.takeResult();
}

std::vector<uint8_t>& charStringForGlyph(uint16_t glyph, std::vector<uint8_t>& boxCharString, std::vector<uint8_t>& checkCharString, std::vector<uint8_t>& xCharString)
{
    if (!glyph)
        return boxCharString;
    if (glyph == 1)
        return checkCharString;
    return xCharString;
}

class Generator {
public:
    std::vector<uint8_t> generate()
    {
        uint16_t numTables = 10;
        uint16_t roundedNumTables = roundDownToPowerOfTwo(numTables);
        uint16_t searchRange = roundedNumTables * 16; // searchRange: "(Maximum power of 2 <= numTables) x 16."

        result.push_back('O');
        result.push_back('T');
        result.push_back('T');
        result.push_back('O');
        append16(numTables);
        append16(searchRange);
        append16(integralLog2(roundedNumTables)); // entrySelector: "Log2(maximum power of 2 <= numTables)."
        append16(numTables * 16 - searchRange); // rangeShift: "NumTables x 16-searchRange."

        assert(result.size() == headerSize);

        // Leave space for the directory entries.
        for (size_t i = 0; i < directoryEntrySize * numTables; ++i)
            result.push_back(0);

        appendTable("CFF ", &Generator::appendCFFTable);
        appendTable("GSUB", &Generator::appendGSUBTable);
        appendTable("OS/2", &Generator::appendOS2Table);
        appendTable("cmap", &Generator::appendCMAPTable);
        auto headTableOffset = result.size();
        appendTable("head", &Generator::appendHEADTable);
        appendTable("hhea", &Generator::appendHHEATable);
        appendTable("hmtx", &Generator::appendHMTXTable);
        appendTable("maxp", &Generator::appendMAXPTable);
        appendTable("name", &Generator::appendNAMETable);
        appendTable("post", &Generator::appendPOSTTable);

        assert(numTables == m_tablesAppendedCount);

        // checksumAdjustment: "To compute: set it to 0, calculate the checksum for the 'head' table and put it in the table directory,
        // sum the entire font as uint32, then store B1B0AFBA - sum. The checksum for the 'head' table will now be wrong. That is OK."
        overwrite32(headTableOffset + 8, 0xB1B0AFBAU - calculateChecksum(0, result.size()));
        return std::move(result);
    }

private:
    class Placeholder {
    public:
        Placeholder(Generator& generator, size_t baseOfOffset)
            : generator(generator)
            , baseOfOffset(baseOfOffset)
            , location(generator.result.size())
        {
            generator.append16(0);
        }

        Placeholder(Placeholder&& other)
            : generator(other.generator)
            , baseOfOffset(other.baseOfOffset)
            , location(other.location)
            , active(other.active)
        {
            other.active = false;
        }

        void populate()
        {
            assert(active);
            size_t delta = generator.result.size() - baseOfOffset;
            assert(delta < std::numeric_limits<uint16_t>::max());
            generator.overwrite16(location, delta);
            active = false;
        }

        ~Placeholder()
        {
            assert(!active);
        }

    private:
        Generator& generator;
        const size_t baseOfOffset;
        const size_t location;
        bool active { true };
    };

    Placeholder placeholder(size_t baseOfOffset)
    {
        return Placeholder(*this, baseOfOffset);
    }

    void append16(uint16_t value)
    {
        result.push_back(value >> 8);
        result.push_back(value);
    }

    void append32(uint32_t value)
    {
        ::append32(result, value);
    }

    void append32BitCode(const char code[4])
    {
        result.push_back(code[0]);
        result.push_back(code[1]);
        result.push_back(code[2]);
        result.push_back(code[3]);
    }

    void overwrite16(size_t location, uint16_t value)
    {
        assert(result.size() >= location + 2);
        result[location] = value >> 8;
        result[location + 1] = value;
    }

    void overwrite32(size_t location, uint32_t value)
    {
        assert(result.size() >= location + 4);
        result[location] = value >> 24;
        result[location + 1] = value >> 16;
        result[location + 2] = value >> 8;
        result[location + 3] = value;
    }
    
    void appendCFFTable()
    {
        auto startingOffset = result.size();

        // Header
        result.push_back(1); // Major version
        result.push_back(0); // Minor version
        result.push_back(4); // Header size
        result.push_back(4); // Offsets within CFF table are 4 bytes long

        // Name INDEX
        std::string fontName = "MylesFont";
        append16(1); // INDEX contains 1 element
        result.push_back(4); // Offsets in this INDEX are 4 bytes long
        append32(1); // 1-index offset of name data
        append32(static_cast<uint32_t>(fontName.length() + 1)); // 1-index offset just past end of name data
        for (char c : fontName)
            result.push_back(c);

        const char operand32Bit = 29;
        const char fullNameKey = 2;
        const char familyNameKey = 3;
        const char fontBBoxKey = 5;
        const char charsetIndexKey = 15;
        const char charstringsIndexKey = 17;
        const char privateDictIndexKey = 18;
        const uint32_t userDefinedStringStartIndex = 391;
        const unsigned sizeOfTopIndex = 56;

        // Top DICT INDEX.
        append16(1); // INDEX contains 1 element
        result.push_back(4); // Offsets in this INDEX are 4 bytes long
        append32(1); // 1-index offset of DICT data
        append32(1 + sizeOfTopIndex); // 1-index offset just past end of DICT data

        // DICT information
        size_t topDictStart = result.size();
        result.push_back(operand32Bit);
        append32(userDefinedStringStartIndex);
        result.push_back(fullNameKey);
        result.push_back(operand32Bit);
        append32(userDefinedStringStartIndex);
        result.push_back(familyNameKey);
        result.push_back(operand32Bit);
        append32(clampTo<int32_t>(0)); // Bounding box x
        result.push_back(operand32Bit);
        append32(clampTo<int32_t>(0)); // Bounding box y
        result.push_back(operand32Bit);
        append32(clampTo<int32_t>(unitsPerEm)); // Bounding box max x
        result.push_back(operand32Bit);
        append32(clampTo<int32_t>(unitsPerEm)); // Bounding box max y
        result.push_back(fontBBoxKey);
        result.push_back(operand32Bit);
        size_t charsetOffsetLocation = result.size();
        append32(0); // Offset of Charset info. Will be overwritten later.
        result.push_back(charsetIndexKey);
        result.push_back(operand32Bit);
        size_t charstringsOffsetLocation = result.size();
        append32(0); // Offset of CharStrings INDEX. Will be overwritten later.
        result.push_back(charstringsIndexKey);
        result.push_back(operand32Bit);
        append32(0); // 0-sized private dict
        result.push_back(operand32Bit);
        append32(0); // no location for private dict
        result.push_back(privateDictIndexKey); // Private dict size and offset
        assert(result.size() == topDictStart + sizeOfTopIndex);

        // String INDEX
        append16(1); // Number of elements in INDEX
        result.push_back(4); // Offsets in this INDEX are 4 bytes long
        uint32_t offset = 1;
        append32(offset);
        offset += fontName.length();
        append32(offset);
        for (char c : fontName)
            result.push_back(c);

        append16(0); // Empty subroutine INDEX
    
        // Charset info
        overwrite32(charsetOffsetLocation, static_cast<uint32_t>(result.size() - startingOffset));
        result.push_back(0);
        for (int i = 1; i < numGlyphs; ++i)
            append16(i);

        // CharStrings INDEX
        std::vector<uint8_t> boxCharString = generateBoxCharString();
        std::vector<uint8_t> checkCharString = generateCheckCharString();
        std::vector<uint8_t> xCharString = generateXCharString();
        assert(numGlyphs > 26);
        overwrite32(charstringsOffsetLocation, static_cast<uint32_t>(result.size() - startingOffset));
        append16(numGlyphs);
        result.push_back(4); // Offsets in this INDEX are 4 bytes long
        offset = 1;
        append32(offset);
        for (uint16_t glyph = 0; glyph < numGlyphs; ++glyph) {
            offset += charStringForGlyph(glyph, boxCharString, checkCharString, xCharString).size();
            append32(offset);
        }
        for (uint16_t glyph = 0; glyph < numGlyphs; ++glyph) {
            std::vector<uint8_t>& charString = charStringForGlyph(glyph, boxCharString, checkCharString, xCharString);
            result.insert(result.end(), charString.begin(), charString.end());
        }
    }

    void appendSubstitutionSubtable(size_t subtableRecordLocation, uint16_t iGetReplaced, uint16_t replacedWithMe)
    {
        overwrite16(subtableRecordLocation + 6, result.size() - subtableRecordLocation);
        auto subtableLocation = result.size();
        append16(2); // Format 2
        append16(0); // Placeholder for offset to coverage table, relative to beginning of substitution table
        append16(1); // GlyphCount
        append16(replacedWithMe); // Substitute with this glyph.

        // Coverage table
        overwrite16(subtableLocation + 2, result.size() - subtableLocation);
        append16(1); // CoverageFormat
        append16(1); // GlyphCount
        append16(iGetReplaced); // This glyph is covered in the coverage.
    }

    void appendScriptSubtable(uint16_t featureCount)
    {
        auto dfltScriptTableLocation = result.size();
        append16(0); // Placeholder for offset of default language system table, relative to beginning of Script table
        append16(0); // Number of following language system tables

        // LangSys table
        overwrite16(dfltScriptTableLocation, result.size() - dfltScriptTableLocation);
        append16(0); // LookupOrder "= NULL ... reserved"
        append16(0xFFFF); // No features are required
        append16(featureCount); // Number of FeatureIndex values
        for (uint16_t i = 0; i < featureCount; ++i)
            append16(i); // Features indices
    }

    void appendGSUBTable()
    {
        std::vector<std::array<char, 5>> features {{"liga"}, {"clig"}, {"dlig"}, {"hlig"}, {"calt"}, {"subs"}, {"sups"}, {"smcp"}, {"c2sc"}, {"pcap"}, {"c2pc"}, {"unic"}, {"titl"}, {"lnum"}, {"onum"}, {"pnum"}, {"tnum"}, {"frac"}, {"afrc"}, {"ordn"}, {"zero"}, {"hist"}, {"jp78"}, {"jp83"}, {"jp90"}, {"jp04"}, {"smpl"}, {"trad"}, {"fwid"}, {"pwid"}, {"ruby"}};
        auto tableLocation = result.size();
        auto headerSize = 10;

        append32(0x00010000); // Version
        append16(headerSize); // Offset to ScriptList
        Placeholder toFeatureList = placeholder(tableLocation);
        Placeholder toLookupList = placeholder(tableLocation);
        assert(tableLocation + headerSize == result.size());

        // ScriptList
        auto scriptListLocation = result.size();
        append16(1); // Number of ScriptRecords
        append32BitCode("DFLT");
        append16(0); // Placeholder for offset of Script table, relative to beginning of ScriptList

        overwrite16(scriptListLocation + 6, result.size() - scriptListLocation);
        appendScriptSubtable(static_cast<uint16_t>(features.size()));

        // FeatureList
        toFeatureList.populate();
        auto featureListLocation = result.size();
        size_t featureListSize = 2 + 6 * features.size();
        size_t featureTableSize = 6;
        append16(features.size()); // FeatureCount
        for (unsigned i = 0; i < features.size(); ++i) {
            auto& code = features[i];
            append32BitCode(code.data()); // Feature name
            append16(featureListSize + featureTableSize * i); // Offset of feature table, relative to beginning of FeatureList table
        }
        assert(featureListLocation + featureListSize == result.size());

        for (unsigned i = 0; i < features.size(); ++i) {
            auto featureTableStart = result.size();
            append16(0); // FeatureParams "= NULL ... reserved"
            append16(1); // LookupCount
            append16(i); // LookupListIndex
            assert(featureTableStart + featureTableSize == result.size());
        }

        // LookupList
        toLookupList.populate();
        auto lookupListLocation = result.size();
        append16(features.size()); // LookupCount
        for (unsigned i = 0; i < features.size(); ++i)
            append16(0); // Placeholder for offset to lookup table, relative to beginning of LookupList
        size_t subtableRecordLocations[features.size()];
        for (unsigned i = 0; i < features.size(); ++i) {
            subtableRecordLocations[i] = result.size();
            overwrite16(lookupListLocation + 2 + 2 * i, result.size() - lookupListLocation);
            append16(1); // Type 1: "Replace one glyph with one glyph"
            append16(0); // LookupFlag
            append16(1); // SubTableCount
            append16(0); // Placeholder for offset to subtable, relative to beginning of Lookup table
        }

        for (unsigned i = 0; i < features.size(); ++i)
            appendSubstitutionSubtable(subtableRecordLocations[i], 3 + i, 1);
    }

    void appendOS2Table()
    {
        append16(2); // Version
        append16(clampTo<int16_t>(unitsPerEm)); // Average advance
        append16(clampTo<uint16_t>(500)); // Weight class
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

        for (int i = 0; i < 10; ++i)
            result.push_back(0);

        for (int i = 0; i < 4; ++i)
            append32(0); // "Bit assignments are pending. Set to 0"
        append32(0x544B4257); // Font Vendor. "WBKT"
        append16(0); // Font Patterns.
        append16(0); // First unicode index
        append16(0xFFFF); // Last unicode index
        append16(clampTo<int16_t>(unitsPerEm)); // Typographical ascender
        append16(clampTo<int16_t>(1)); // Typographical descender
        append16(clampTo<int16_t>(unitsPerEm / 10)); // Typographical line gap
        append16(clampTo<uint16_t>(unitsPerEm)); // Windows-specific ascent
        append16(clampTo<uint16_t>(1)); // Windows-specific descent
        append32(0xFF10FC07); // Bitmask for supported codepages (Part 1). Report all pages as supported.
        append32(0x0000FFFF); // Bitmask for supported codepages (Part 2). Report all pages as supported.
        append16(clampTo<int16_t>(unitsPerEm / 2)); // x-height
        append16(clampTo<int16_t>(unitsPerEm)); // Cap-height
        append16(0); // Default char
        append16(' '); // Break character
        append16(3); // Maximum context needed to perform font features
        append16(3); // Smallest optical point size
        append16(0xFFFF); // Largest optical point size
    }

    void appendFormat12CMAPTable()
    {
        // Braindead scheme: One segment for each character
        auto subtableLocation = result.size();
        append32(12 << 16); // Format 12
        append32(0); // Placeholder for byte length
        append32(0); // Language independent
        append32(2); //  nGroups
        append32('A'); // startCharCode
        append32('Z'); // endCharCode
        append32(1); // startGlyphCode
        append32('a'); // startCharCode
        append32('z'); // endCharCode
        append32(27); // startGlyphCode
        overwrite32(subtableLocation + 4, static_cast<uint32_t>(result.size() - subtableLocation));
    }

    void appendFormat4CMAPTable()
    {
        auto subtableLocation = result.size();
        append16(4); // Format 4
        append16(0); // Placeholder for length in bytes
        append16(0); // Language independent
        uint16_t segCount = 3;
        append16(clampTo<uint16_t>(2 * segCount)); // segCountX2: "2 x segCount"
        uint16_t originalSearchRange = roundDownToPowerOfTwo(segCount);
        uint16_t searchRange = clampTo<uint16_t>(2 * originalSearchRange); // searchRange: "2 x (2**floor(log2(segCount)))"
        append16(searchRange);
        append16(integralLog2(originalSearchRange)); // entrySelector: "log2(searchRange/2)"
        append16(clampTo<uint16_t>((2 * segCount) - searchRange)); // rangeShift: "2 x segCount - searchRange"

        // Ending character codes
        append16('Z');
        append16('z');
        append16(0xFFFF);

        append16(0); // reserved

        // Starting character codes
        append16('A');
        append16('a');
        append16(0xFFFF);

        // idDelta
        append16(static_cast<uint16_t>(27) - static_cast<uint16_t>('A'));
        append16(static_cast<uint16_t>(1) - static_cast<uint16_t>('a'));
        append16(0x0001);

        // idRangeOffset
        append16(0); // idRangeOffset
        append16(0);

        // Fonts strive to hold 2^16 glyphs, but with the current encoding scheme, we write 8 bytes per codepoint into this subtable.
        // Because the size of this subtable must be represented as a 16-bit number, we are limiting the number of glyphs we support to 2^13.
        // FIXME: If we hit this limit in the wild, use a more compact encoding scheme for this subtable.
        overwrite16(subtableLocation + 2, clampTo<uint16_t>(result.size() - subtableLocation));
    }
    
    void appendCMAPTable()
    {
        auto startingOffset = result.size();
        append16(0);
        append16(3); // Number of subtables

        append16(0); // Unicode
        append16(3); // Unicode version 2.2+
        append32(28); // Byte offset of subtable

        append16(3); // Microsoft
        append16(1); // Unicode BMP
        auto format4OffsetLocation = result.size();
        append32(0); // Byte offset of subtable

        append16(3); // Microsoft
        append16(10); // Unicode
        append32(28); // Byte offset of subtable

        appendFormat12CMAPTable();
        overwrite32(format4OffsetLocation, static_cast<uint32_t>(result.size() - startingOffset));
        appendFormat4CMAPTable();
    }
    
    void appendHEADTable()
    {
        append32(0x00010000); // Version
        append32(0x00010000); // Revision
        append32(0); // Checksum placeholder; to be overwritten by the caller.
        append32(0x5F0F3CF5); // Magic number.
        append16((1 << 9) | 1);

        append16(unitsPerEm);
        append32(0); // First half of creation date
        append32(0); // Last half of creation date
        append32(0); // First half of modification date
        append32(0); // Last half of modification date
        append16(clampTo<int16_t>(0)); // bounding box x
        append16(clampTo<int16_t>(0)); // bounding box y
        append16(clampTo<int16_t>(unitsPerEm)); // bounding box max x
        append16(clampTo<int16_t>(unitsPerEm)); // bounding box max y
        append16(0); // Traits
        append16(3); // Smallest readable size in pixels
        append16(0); // Might contain LTR or RTL glyphs
        append16(0); // Short offsets in the 'loca' table. However, OTF fonts don't have a 'loca' table so this is irrelevant
        append16(0); // Glyph data format
    }
    
    void appendHHEATable()
    {
        append32(0x00010000); // Version
        append16(clampTo<int16_t>(unitsPerEm)); // ascent
        append16(clampTo<int16_t>(1)); // descent
        // WebKit SVG font rendering has hard coded the line gap to be 1/10th of the font size since 2008 (see r29719).
        append16(clampTo<int16_t>(unitsPerEm / 10)); // line gap
        append16(clampTo<uint16_t>(unitsPerEm)); // advance width max
        append16(clampTo<int16_t>(0)); // Minimum left side bearing
        append16(clampTo<int16_t>(0)); // Minimum right side bearing
        append16(clampTo<int16_t>(unitsPerEm)); // X maximum extent
        // Since WebKit draws the caret and ignores the following values, it doesn't matter what we set them to.
        append16(1); // Vertical caret
        append16(0); // Vertical caret
        append16(0); // "Set value to 0 for non-slanted fonts"
        append32(0); // Reserved
        append32(0); // Reserved
        append16(0); // Current format
        append16(numGlyphs); // Number of advance widths in HMTX table
    }
    
    void appendHMTXTable()
    {
        for (unsigned i = 0; i < numGlyphs; ++i) {
            append16(clampTo<uint16_t>(unitsPerEm)); // horizontal advance
            append16(clampTo<int16_t>(0)); // left side bearing
        }
    }
    
    void appendMAXPTable()
    {
        append32(0x00010000); // Version
        append16(numGlyphs); // Number of glyphs
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
        append16(numGlyphs); // Maximum number of glyphs referenced at top level
        append16(0); // No compound glyphs
    }
    
    void appendNAMETable()
    {
        std::string fontName = "MylesFont";

        append16(0); // Format selector
        append16(1); // Number of name records in table
        append16(18); // Offset in bytes to the beginning of name character strings

        append16(0); // Unicode
        append16(3); // Unicode version 2.0 or later
        append16(0); // Language
        append16(1); // Name identifier. 1 = Font family
        append16(fontName.length());
        append16(0); // Offset into name data

        for (auto codeUnit : fontName)
            append16(codeUnit);
    }
    
    void appendPOSTTable()
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

    uint32_t calculateChecksum(size_t startingOffset, size_t endingOffset) const
    {
        assert(isFourByteAligned(endingOffset - startingOffset));
        uint32_t sum = 0;
        for (size_t offset = startingOffset; offset < endingOffset; offset += 4) {
            sum += static_cast<unsigned char>(result[offset + 3])
                | (static_cast<unsigned char>(result[offset + 2]) << 8)
                | (static_cast<unsigned char>(result[offset + 1]) << 16)
                | (static_cast<unsigned char>(result[offset]) << 24);
        }
        return sum;
    }

    typedef void (Generator::*FontAppendingFunction)();
    void appendTable(const char identifier[4], FontAppendingFunction appendingFunction)
    {
        size_t offset = result.size();
        assert(isFourByteAligned(offset));
        (this->*appendingFunction)();
        size_t unpaddedSize = result.size() - offset;
        while (!isFourByteAligned(result.size()))
            result.push_back(0);
        assert(isFourByteAligned(result.size()));
        size_t directoryEntryOffset = headerSize + m_tablesAppendedCount * directoryEntrySize;
        result[directoryEntryOffset] = identifier[0];
        result[directoryEntryOffset + 1] = identifier[1];
        result[directoryEntryOffset + 2] = identifier[2];
        result[directoryEntryOffset + 3] = identifier[3];
        overwrite32(directoryEntryOffset + 4, calculateChecksum(offset, result.size()));
        overwrite32(directoryEntryOffset + 8, static_cast<uint32_t>(offset));
        overwrite32(directoryEntryOffset + 12, static_cast<uint32_t>(unpaddedSize));
        ++m_tablesAppendedCount;
    }

    unsigned m_tablesAppendedCount { 0 };
    std::vector<uint8_t> result;
};

std::vector<uint8_t> generateFont()
{
    return Generator().generate();
}
