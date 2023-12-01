/*
 * Copyright (C) 2008, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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
#include "OpenTypeUtilities.h"

#include "FontMemoryResource.h"
#include "SharedBuffer.h"

namespace WebCore {

struct BigEndianUShort { 
    operator unsigned short() const { return (v & 0x00ff) << 8 | v >> 8; }
    BigEndianUShort(unsigned short u) : v((u & 0x00ff) << 8 | u >> 8) { }
    unsigned short v;
};

struct BigEndianULong { 
    operator unsigned() const { return (v & 0xff) << 24 | (v & 0xff00) << 8 | (v & 0xff0000) >> 8 | v >> 24; }
    BigEndianULong(unsigned u) : v((u & 0xff) << 24 | (u & 0xff00) << 8 | (u & 0xff0000) >> 8 | u >> 24) { }
    unsigned v;
};

#pragma pack(1)

struct EOTPrefix {
    unsigned eotSize;
    unsigned fontDataSize;
    unsigned version;
    unsigned flags;
    uint8_t fontPANOSE[10];
    uint8_t charset;
    uint8_t italic;
    unsigned weight;
    unsigned short fsType;
    unsigned short magicNumber;
    unsigned unicodeRange[4];
    unsigned codePageRange[2];
    unsigned checkSumAdjustment;
    unsigned reserved[4];
    unsigned short padding1;
};

struct TableDirectoryEntry {
    BigEndianULong tag;
    BigEndianULong checkSum;
    BigEndianULong offset;
    BigEndianULong length;
};

#if !USE(CG) || !defined(COREGRAPHICS_INCLUDES_CORESERVICES_HEADER)
// Fixed type is not defined on non-CG and Windows platforms. |version| in sfntHeader
// and headTable and |fontRevision| in headTable are of Fixed, but they're
// not actually refered to anywhere. Therefore, we just have to match
// the size (4 bytes). For the definition of Fixed type, see
// http://developer.apple.com/documentation/mac/Legacy/GXEnvironment/GXEnvironment-356.html#HEADING356-6.
typedef int32_t Fixed;
#endif

struct sfntHeader {
    Fixed version;
    BigEndianUShort numTables;
    BigEndianUShort searchRange;
    BigEndianUShort entrySelector;
    BigEndianUShort rangeShift;
    TableDirectoryEntry tables[1];
};

struct OS2Table {
    BigEndianUShort version;
    BigEndianUShort avgCharWidth;
    BigEndianUShort weightClass;
    BigEndianUShort widthClass;
    BigEndianUShort fsType;
    BigEndianUShort subscriptXSize;
    BigEndianUShort subscriptYSize;
    BigEndianUShort subscriptXOffset;
    BigEndianUShort subscriptYOffset;
    BigEndianUShort superscriptXSize;
    BigEndianUShort superscriptYSize;
    BigEndianUShort superscriptXOffset;
    BigEndianUShort superscriptYOffset;
    BigEndianUShort strikeoutSize;
    BigEndianUShort strikeoutPosition;
    BigEndianUShort familyClass;
    uint8_t panose[10];
    BigEndianULong unicodeRange[4];
    uint8_t vendID[4];
    BigEndianUShort fsSelection;
    BigEndianUShort firstCharIndex;
    BigEndianUShort lastCharIndex;
    BigEndianUShort typoAscender;
    BigEndianUShort typoDescender;
    BigEndianUShort typoLineGap;
    BigEndianUShort winAscent;
    BigEndianUShort winDescent;
    BigEndianULong codePageRange[2];
    BigEndianUShort xHeight;
    BigEndianUShort capHeight;
    BigEndianUShort defaultChar;
    BigEndianUShort breakChar;
    BigEndianUShort maxContext;
};

struct headTable {
    Fixed version;
    Fixed fontRevision;
    BigEndianULong checkSumAdjustment;
    BigEndianULong magicNumber;
    BigEndianUShort flags;
    BigEndianUShort unitsPerEm;
    long long created;
    long long modified;
    BigEndianUShort xMin;
    BigEndianUShort xMax;
    BigEndianUShort yMin;
    BigEndianUShort yMax;
    BigEndianUShort macStyle;
    BigEndianUShort lowestRectPPEM;
    BigEndianUShort fontDirectionHint;
    BigEndianUShort indexToLocFormat;
    BigEndianUShort glyphDataFormat;
};

struct nameRecord {
    BigEndianUShort platformID;
    BigEndianUShort encodingID;
    BigEndianUShort languageID;
    BigEndianUShort nameID;
    BigEndianUShort length;
    BigEndianUShort offset;
};

struct nameTable {
    BigEndianUShort format;
    BigEndianUShort count;
    BigEndianUShort stringOffset;
    nameRecord nameRecords[1];
};

#pragma pack()

EOTHeader::EOTHeader()
    : m_buffer(sizeof(EOTPrefix))
{
}

void EOTHeader::updateEOTSize(size_t fontDataSize)
{
    prefix()->eotSize = m_buffer.size() + fontDataSize;
}

void EOTHeader::appendBigEndianString(const BigEndianUShort* string, unsigned short length)
{
    size_t oldSize = m_buffer.size();
    m_buffer.grow(oldSize + length + 2 * sizeof(unsigned short));
    UChar* dst = reinterpret_cast<UChar*>(m_buffer.data() + oldSize);
    unsigned i = 0;
    dst[i++] = length;
    unsigned numCharacters = length / 2;
    for (unsigned j = 0; j < numCharacters; j++)
        dst[i++] = string[j];
    dst[i] = 0;
}

void EOTHeader::appendPaddingShort()
{
    unsigned short padding = 0;
    m_buffer.append(reinterpret_cast<uint8_t*>(&padding), sizeof(padding));
}

// adds fontName to the font table in fontData, and writes the new font table to rewrittenFontTable
// returns the size of the name table (which is used by renameAndActivateFont), or 0 on early abort
bool renameFont(const SharedBuffer& fontData, const String& fontName, Vector<uint8_t>& rewrittenFontData)
{
    size_t originalDataSize = fontData.size();
    const sfntHeader* sfnt = reinterpret_cast<const sfntHeader*>(fontData.data());

    // Abort if the data is too small to be a font header with a "tables" entry.
    if (originalDataSize < offsetof(sfntHeader, tables))
        return false;

    // Abort if the data is too small to hold all the tables specified in the header.
    if (originalDataSize < offsetof(sfntHeader, tables) + sfnt->numTables * sizeof(TableDirectoryEntry))
        return false;

    unsigned t;
    for (t = 0; t < sfnt->numTables; ++t) {
        if (sfnt->tables[t].tag == 'name')
            break;
    }
    if (t == sfnt->numTables)
        return false;

    const int nameRecordCount = 5;

    // Rounded up to a multiple of 4 to simplify the checksum calculation.
    size_t nameTableSize = ((offsetof(nameTable, nameRecords) + nameRecordCount * sizeof(nameRecord) + fontName.length() * sizeof(UChar)) & ~3) + 4;

    rewrittenFontData.resize(fontData.size() + nameTableSize);
    auto* data = rewrittenFontData.data();
    memcpy(data, fontData.data(), originalDataSize);

    // Make the table directory entry point to the new 'name' table.
    sfntHeader* rewrittenSfnt = reinterpret_cast<sfntHeader*>(data);
    rewrittenSfnt->tables[t].length = nameTableSize;
    rewrittenSfnt->tables[t].offset = originalDataSize;

    // Write the new 'name' table after the original font data.
    nameTable* name = reinterpret_cast<nameTable*>(data + originalDataSize);
    name->format = 0;
    name->count = nameRecordCount;
    name->stringOffset = offsetof(nameTable, nameRecords) + nameRecordCount * sizeof(nameRecord);
    for (unsigned i = 0; i < nameRecordCount; ++i) {
        name->nameRecords[i].platformID = 3;
        name->nameRecords[i].encodingID = 1;
        name->nameRecords[i].languageID = 0x0409;
        name->nameRecords[i].offset = 0;
        name->nameRecords[i].length = fontName.length() * sizeof(UChar);
    }

    // The required 'name' record types: Family, Style, Unique, Full and PostScript.
    name->nameRecords[0].nameID = 1;
    name->nameRecords[1].nameID = 2;
    name->nameRecords[2].nameID = 3;
    name->nameRecords[3].nameID = 4;
    name->nameRecords[4].nameID = 6;

    for (unsigned i = 0; i < fontName.length(); ++i)
        reinterpret_cast<BigEndianUShort*>(data + originalDataSize + name->stringOffset)[i] = fontName[i];

    // Update the table checksum in the directory entry.
    rewrittenSfnt->tables[t].checkSum = 0;
    for (unsigned i = 0; i * sizeof(BigEndianULong) < nameTableSize; ++i)
        rewrittenSfnt->tables[t].checkSum = rewrittenSfnt->tables[t].checkSum + reinterpret_cast<BigEndianULong*>(name)[i];

    return true;
}

// Rename the font and install the new font data into the system
RefPtr<FontMemoryResource> renameAndActivateFont(const SharedBuffer& fontData, const String& fontName)
{
    Vector<uint8_t> rewrittenFontData;
    if (!renameFont(fontData, fontName, rewrittenFontData))
        return { };

    DWORD numFonts = 0;
    HANDLE fontHandle = AddFontMemResourceEx(rewrittenFontData.data(), rewrittenFontData.size(), 0, &numFonts);
    if (!fontHandle)
        return { };
    if (numFonts < 1) {
        RemoveFontMemResourceEx(fontHandle);
        return { };
    }

    return FontMemoryResource::create(fontHandle);
}

}
