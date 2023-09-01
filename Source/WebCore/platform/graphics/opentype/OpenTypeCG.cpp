/*
 * Copyright (C) 2015 Frederic Wang (fred.wang@free.fr). All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "OpenTypeCG.h"

#include "OpenTypeTypes.h"
#include <span>

namespace WebCore {
namespace OpenType {

// From https://learn.microsoft.com/en-us/typography/opentype/spec/os2
struct OS2 {
    UInt16 version;
    Int16 xAvgCharWidth;
    UInt16 usWeightClass;
    UInt16 usWidthClass;
    UInt16 fsType;
    Int16 ySubscriptXSize;
    Int16 ySubscriptYSize;
    Int16 ySubscriptXOffset;
    Int16 ySubscriptYOffset;
    Int16 ySuperscriptXSize;
    Int16 ySuperscriptYSize;
    Int16 ySuperscriptXOffset;
    Int16 ySuperscriptYOffset;
    Int16 yStrikeoutSize;
    Int16 yStrikeoutPosition;
    Int16 sFamilyClass;
    UInt8 panose[10];
    UInt32 ulUnicodeRange1;
    UInt32 ulUnicodeRange2;
    UInt32 ulUnicodeRange3;
    UInt32 ulUnicodeRange4;
    UInt8 achVendID[4];
    UInt16 fsSelection;
    UInt16 usFirstCharIndex;
    UInt16 usLastCharIndex;
    Int16 sTypoAscender;
    Int16 sTypoDescender;
    Int16 sTypoLineGap;
    UInt16 usWinAscent;
    UInt16 usWinDescent;

    // New in v1.
    UInt32 ulCodePageRange1;
    UInt32 ulCodePageRange2;

    // New in v2.
    Int16 sxHeight;
    Int16 sCapHeight;
    UInt16 usDefaultChar;
    UInt16 usBreakChar;
    UInt16 usMaxContext;

    // New in v5.
    UInt16 usLowerOpticalPointSize;
    UInt16 usUpperOpticalPointSize;
};

struct Offset16 {
    UInt16 offset;
    explicit operator size_t() const { return static_cast<size_t>(offset); }
};

struct Offset32 {
    UInt32 offset;
    explicit operator size_t() const { return static_cast<size_t>(offset); }
};

struct Tag4 {
    static constexpr size_t tagSize = 4;
    unsigned char ch1;
    unsigned char ch2;
    unsigned char ch3;
    unsigned char ch4;

    constexpr Tag4(char c1, char c2, char c3, char c4) : ch1(c1), ch2(c2), ch3(c3), ch4(c4) { }
    constexpr Tag4(const char *ch) : Tag4(ch ? ch[0] : '?', ch ? ch[1] : '?', ch ? ch[2] : '?', ch ? ch[3] : '?') { }

    constexpr bool operator==(const Tag4& other) const { return ch1 == other.ch1 && ch2 == other.ch2 && ch3 == other.ch3 && ch4 == other.ch4; }
};
static_assert(sizeof(Tag4) == Tag4::tagSize);
static_assert(alignof(Tag4) == 1);

// From https://learn.microsoft.com/en-us/typography/opentype/spec/base
struct BASE {
    UInt16 majorVersion;
    UInt16 minorVersion;
    Offset16 horizAxisOffset;
    Offset16 vertAxisOffset;
    Offset32 itemVarStoreOffset;
};

struct BaseAxis {
    Offset16 baseTagListOffset;
    Offset16 baseScriptListOffset;
};

struct BaseTagList {
    UInt16 baseTagCount;
    Tag4 firstBaselineTag; // First of effectively Tag[baseTagCount].
};

struct BaseScriptList {
    UInt16 baseScriptCount;
    // First of effectively BaseScriptRecord[baseScriptCount]:
    Tag4 firstBaseScriptTag;
    Offset16 firstBaseScriptOffset;
};

struct BaseScript {
    Offset16 baseValuesOffset;
    // Unused here:
    // Offset16 defaultMinMaxOffset;
    // UInt16 baseLangSysCount;
    // BaseLangSysRecord baseLangSysRecords;
};

struct BaseValues {
    UInt16 defaultBaselineIndex;
    UInt16 baseCoordCount;
    Offset16 firstBaseCoordOffset; // First of effectively Offset16[baseCoordCount].
};

struct BaseCoord {
    UInt16 baseCoordFormat;
    Int16 coordinate;
    // More fields below depending on format, unused here.
};

template <typename Value>
inline std::optional<Value> readFromTable(const std::span<const UInt8>& buffer, size_t offsetOfValueInBuffer)
{
    std::optional<Value> result;
    if ((sizeof(Value) > buffer.size()) || (offsetOfValueInBuffer > buffer.size() - sizeof(Value)))
        return result;

    memcpy(&result.emplace(Value { 0 }), buffer.data() + offsetOfValueInBuffer, sizeof(Value));
    return result;
}

#define READ(span_, struct_, member_) readFromTable<decltype(struct_::member_)>(span_, offsetof(struct_, member_))

#define READ_INDEXED(span_, struct_, member_, index_) readFromTable<decltype(struct_::member_)>(span_, offsetof(struct_, member_) + (index_) * sizeof(struct_::member_))

template <typename Offset>
std::span<const UInt8> followOffset(const std::span<const UInt8>& buffer, size_t offsetOfOffsetInBuffer)
{
    auto maybeOffset = readFromTable<Offset>(buffer, offsetOfOffsetInBuffer);
    if (!maybeOffset)
        return { };
    auto offset = static_cast<size_t>(*maybeOffset);
    if (!offset || offset > buffer.size())
        return { };
    return buffer.subspan(offset);
}

#define FOLLOW_OFFSET(span_, struct_, member_) followOffset<decltype(struct_::member_)>(span_, offsetof(struct_, member_))

bool tryGetTypoMetrics(CTFontRef font, short& ascent, short& descent, short& lineGap)
{
    auto os2Table = adoptCF(CTFontCopyTable(font, kCTFontTableOS2, kCTFontTableOptionNoOptions));
    if (!os2Table)
        return false;
    auto os2Span = std::span<const UInt8>(CFDataGetBytePtr(os2Table.get()), CFDataGetLength(os2Table.get()));

    auto maybeFsSelection = READ(os2Span, OS2, fsSelection);
    if (!maybeFsSelection)
        return false;
    const unsigned useTypoMetrics = 7;
    if (!(*maybeFsSelection & (1u << useTypoMetrics)))
        return false;

    auto maybeAscent = READ(os2Span, OS2, sTypoAscender);
    if (!maybeAscent)
        return false;
    auto maybedescent = READ(os2Span, OS2, sTypoAscender);
    if (!maybedescent)
        return false;
    auto maybeLineGap = READ(os2Span, OS2, sTypoLineGap);
    if (!maybeLineGap)
        return false;

    ascent = *maybeAscent;
    descent = *maybedescent;
    lineGap = *maybeLineGap;

    return true;
}

Baselines tryGetBaselineMetrics(CTFontRef font)
{
    Baselines baselines;

    auto baseTable = adoptCF(CTFontCopyTable(font, kCTFontTableBASE, kCTFontTableOptionNoOptions));
    if (!baseTable)
        return baselines;
    auto baseSpan = std::span<const UInt8>(CFDataGetBytePtr(baseTable.get()), CFDataGetLength(baseTable.get()));

    auto horizAxisSpan = FOLLOW_OFFSET(baseSpan, BASE, horizAxisOffset);
    if (horizAxisSpan.empty())
        return baselines;

    auto baseTagListSpan = FOLLOW_OFFSET(horizAxisSpan, BaseAxis, baseTagListOffset);
    if (baseTagListSpan.empty())
        return baselines;

    auto maybeBaseTagCount = READ(baseTagListSpan, BaseTagList, baseTagCount);
    if (!maybeBaseTagCount)
        return baselines;
    size_t baseTagCount = *maybeBaseTagCount;
    if (!baseTagCount)
        return baselines;

    const Tag4 ideoTag = Tag4("ideo");
    const Tag4 hangTag = Tag4("hang");
    const Tag4 romnTag = Tag4("romn");

    size_t ideoIndex = size_t(-1);
    size_t hangIndex = size_t(-1);
    size_t romnIndex = size_t(-1);

    for (size_t tagIndex = 0; tagIndex < baseTagCount; ++tagIndex) {
        auto maybeTag = READ_INDEXED(baseTagListSpan, BaseTagList, firstBaselineTag, tagIndex);
        if (!maybeTag)
            return baselines;
        if (*maybeTag == ideoTag)
            ideoIndex = tagIndex;
        else if (*maybeTag == hangTag)
            hangIndex = tagIndex;
        else if (*maybeTag == romnTag)
            romnIndex = tagIndex;
    }

    if (ideoIndex == size_t(-1) && hangIndex == size_t(-1) && romnIndex == size_t(-1))
        return baselines;

    auto baseScriptListSpan = FOLLOW_OFFSET(horizAxisSpan, BaseAxis, baseScriptListOffset);
    if (baseScriptListSpan.empty())
        return baselines;

    auto maybeBaseScriptCount = READ(baseScriptListSpan, BaseScriptList, baseScriptCount);
    if (!maybeBaseScriptCount)
        return baselines;
    size_t baseScriptCount = *maybeBaseScriptCount;
    if (!baseScriptCount)
        return baselines;

    // Only look at the first record.
    auto baseScriptSpan = FOLLOW_OFFSET(baseScriptListSpan, BaseScriptList, firstBaseScriptOffset);
    if (baseScriptSpan.empty())
        return baselines;

    auto baseValuesSpan = FOLLOW_OFFSET(baseScriptSpan, BaseScript, baseValuesOffset);
    if (baseValuesSpan.empty())
        return baselines;

    auto maybeBaseCoordCount = READ(baseValuesSpan, BaseValues, baseCoordCount);
    if (!maybeBaseCoordCount)
        return baselines;
    size_t baseCoordCount = *maybeBaseCoordCount;
    if (!baseCoordCount)
        return baselines;

    for (size_t coordIndex = 0; coordIndex < baseCoordCount; ++coordIndex) {
        using BaseCoordOffset = decltype(BaseValues::firstBaseCoordOffset);
        auto baseCoordSpan = followOffset<BaseCoordOffset>(baseValuesSpan, offsetof(BaseValues, firstBaseCoordOffset) + coordIndex * sizeof(BaseCoordOffset));
        if (baseCoordSpan.empty())
            continue;
        auto maybeCoordinate = READ(baseCoordSpan, BaseCoord, coordinate);
        if (!maybeCoordinate)
            continue;
        if (coordIndex == ideoIndex)
            baselines.ideo.emplace(*maybeCoordinate);
        else if (coordIndex == hangIndex)
            baselines.hang.emplace(*maybeCoordinate);
        else if (coordIndex == romnIndex)
            baselines.romn.emplace(*maybeCoordinate);
    }

    return baselines;
}

#undef READ

} // namespace OpenType
} // namespace WebCore
