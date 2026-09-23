/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "HEVCUtilities.h"

#include "BitReader.h"
#include "FourCC.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/DataView.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

std::optional<AVCParameters> parseAVCCodecParameters(StringView codecString)
{
    // The format of the 'avc1' codec string is specified in ISO/IEC 14496-15:2014, Annex E2.
    StringView codecView(codecString);
    auto codecSplit = codecView.split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return std::nullopt;

    AVCParameters parameters;

    // Codec identifier: legal values are specified in ISO/IEC 14496-15:2014, section 8:
    auto codecName = *nextElement;
    if (codecName != "avc1"_s)
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // First element: profile_idc
    auto firstElement = *nextElement;
    if (!firstElement.length())
        return std::nullopt;

    auto profileFlagsAndLevel = parseInteger<uint32_t>(*nextElement, 16);
    if (!profileFlagsAndLevel)
        return std::nullopt;
    parameters.profileIDC = (*profileFlagsAndLevel >> 16) & 0xFF;
    parameters.constraintsFlags = (*profileFlagsAndLevel >> 8) & 0xFF;
    parameters.levelIDC = *profileFlagsAndLevel & 0xFF;

    return parameters;
}

String createAVCCodecParametersString(const AVCParameters& parameters)
{
    // The format of the 'avc1' codec string is specified in ISO/IEC 14496-15:2014, Annex E.2.
    return makeString("avc1."_s
        , hex(parameters.profileIDC, 2)
        , hex(parameters.constraintsFlags, 2)
        , hex(parameters.levelIDC, 2));
}

std::optional<AVCParameters> parseAVCDecoderConfigurationRecord(const SharedBuffer& buffer)
{
    // ISO/IEC 14496-10:2014
    // 7.3.2.1.1 Sequence parameter set data syntax

    // AVCDecoderConfigurationRecord is at a minimum 24 bytes long
    if (buffer.size() < 24)
        return std::nullopt;

    // aligned(8) class AVCDecoderConfigurationRecord {
    //    unsigned int(8) configurationVersion = 1;
    //    unsigned int(8) AVCProfileIndication;
    //    unsigned int(8) profile_compatibility;
    //    unsigned int(8) AVCLevelIndication;
    //    ...
    AVCParameters parameters;
    auto arrayBuffer = buffer.tryCreateArrayBuffer();
    if (!arrayBuffer)
        return std::nullopt;

    bool status = true;
    auto view = JSC::DataView::create(WTF::move(arrayBuffer), 0, buffer.size());

    // Byte 0 is a version flag
    parameters.profileIDC = view->get<uint8_t>(1, false, &status);
    if (!status)
        return std::nullopt;

    parameters.constraintsFlags = view->get<uint8_t>(2, false, &status);
    if (!status)
        return std::nullopt;

    parameters.levelIDC = view->get<uint8_t>(3, false, &status);
    if (!status)
        return std::nullopt;

    return parameters;
}

std::optional<HEVCParameters> parseHEVCCodecParameters(StringView codecString)
{
    // The format of the 'hevc' codec string is specified in ISO/IEC 14496-15:2014, Annex E.3.
    StringView codecView(codecString);
    auto codecSplit = codecView.split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return std::nullopt;

    HEVCParameters parameters;

    // Codec identifier: legal values are specified in ISO/IEC 14496-15:2014, section 8:
    auto codecName = *nextElement;
    if (codecName == "hvc1"_s)
        parameters.codec = HEVCParameters::Codec::Hvc1;
    else if (codecName == "hev1"_s)
        parameters.codec = HEVCParameters::Codec::Hev1;
    else
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // First element: Optional General Profile Space parameter ['A', 'B', 'C'], mapping to [1, 2, 3]
    // and [0] for absent, then General Profile IDC as a 5-bit decimal number.
    auto profileSpace = *nextElement;
    if (!profileSpace.length())
        return std::nullopt;

    auto firstCharacter = profileSpace[0];
    bool hasProfileSpace = firstCharacter >= 'A' && firstCharacter <= 'C';
    if (hasProfileSpace) {
        parameters.generalProfileSpace = 1 + (firstCharacter - 'A');
        profileSpace = profileSpace.substring(1);
    }

    auto profileIDC = parseInteger<uint8_t>(profileSpace);
    if (!profileIDC)
        return std::nullopt;
    parameters.generalProfileIDC = *profileIDC;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Second element: 32 bit of General Profile Compatibility Flags, in reverse bit order,
    // in hex with leading zeros omitted.
    auto compatibilityFlags = parseInteger<uint32_t>(*nextElement, 16);
    if (!compatibilityFlags)
        return std::nullopt;
    parameters.generalProfileCompatibilityFlags = reverseBits32(*compatibilityFlags);

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Third element: General Tier Flag ['L', 'H'], mapping to [false, true], followed by
    // General Level IDC as a 8-bit decimal number.
    auto generalTier = *nextElement;
    firstCharacter = generalTier[0];
    if (firstCharacter != 'L' && firstCharacter != 'H')
        return std::nullopt;
    parameters.generalTierFlag = (firstCharacter == 'L' ? 0 : 1);

    auto generalLevelIDC = parseInteger<uint8_t>(generalTier.substring(1));
    if (!generalLevelIDC)
        return std::nullopt;
    parameters.generalLevelIDC = *generalLevelIDC;

    // Optional fourth and remaining elements: a sequence of 6 1-byte constraint flags, each byte encoded
    // in hex, and separated by a period, with trailing zero bytes omitted.
    for (unsigned i = 0; i < 6; ++i) {
        if (++nextElement == codecSplit.end())
            break;
        auto flag = parseInteger<uint8_t>(*nextElement, 16);
        if (!flag)
            return std::nullopt;
        parameters.generalConstraintIndicatorFlags[i] = *flag;
    }

    return parameters;
}

String createHEVCCodecParametersString(const HEVCParameters& parameters)
{
    // For the second parameter, from ISO/IEC 14496-15:2014, Annex E.3.
    // * the 32 bits of the general_profile_compatibility_flags, but in reverse bit order, i.e. with
    // general_profile_compatibility_flag[ 31 ] as the most significant bit, followed by, general_profile_compatibility_flag[ 30 ],
    // and down to general_profile_compatibility_flag[ 0 ] as the least significant bit, where general_profile_compatibility_flag[ i ]
    // for i in the range of 0 to 31, inclusive, are specified in ISO/IEC 23008‐2, encoded in hexadecimal (leading zeroes may be omitted)
    auto compatFlagParameter = hex(reverseBits32(parameters.generalProfileCompatibilityFlags));

    // * each of the 6 bytes of the constraint flags, starting from the byte containing the
    // general_progressive_source_flag, each encoded as a hexadecimal number, and the encoding
    // of each byte separated by a period; trailing bytes that are zero may be omitted.
    StringBuilder compatibilityFlags;
    auto lastFlagByte = parameters.generalConstraintIndicatorFlags.reverseFindIf([] (auto& flag) { return flag; });
    for (size_t i = 0; lastFlagByte != notFound && i <= lastFlagByte; ++i) {
        compatibilityFlags.append('.');
        compatibilityFlags.append(hex(parameters.generalConstraintIndicatorFlags[i], 2));
    }

    StringBuilder resultBuilder;
    resultBuilder.append(parameters.codec == HEVCParameters::Codec::Hev1 ? "hev1"_s : "hvc1"_s, '.');
    if (parameters.generalProfileSpace) {
        // The format of the 'hevc' codec string is specified in ISO/IEC 14496-15:2014, Annex E.3.
        char profileSpaceCharacter = 'A' + parameters.generalProfileSpace - 1;
        resultBuilder.append(profileSpaceCharacter);
    }
    resultBuilder.append(parameters.generalProfileIDC, '.', compatFlagParameter, '.', parameters.generalTierFlag ? 'H' : 'L', parameters.generalLevelIDC);
    resultBuilder.append(compatibilityFlags);
    return resultBuilder.toString();
}

std::optional<HEVCParameters> parseHEVCDecoderConfigurationRecord(FourCC codecCode, const SharedBuffer& buffer)
{
    // ISO/IEC 14496-15:2014
    // 8.3.3.1 HEVC decoder configuration record

    // HEVCDecoderConfigurationRecord is at a minimum 23 bytes long
    if (buffer.size() < 23)
        return std::nullopt;

    HEVCParameters parameters;
    if (codecCode == std::span { "hev1" })
        parameters.codec = HEVCParameters::Codec::Hev1;
    else if (codecCode == std::span { "hvc1" })
        parameters.codec = HEVCParameters::Codec::Hvc1;
    else
        return std::nullopt;

    // aligned(8) class HEVCDecoderConfigurationRecord {
    //    unsigned int(8)  configurationVersion = 1;
    //    unsigned int(2)  general_profile_space;
    //    unsigned int(1)  general_tier_flag;
    //    unsigned int(5)  general_profile_idc;
    //    unsigned int(32) general_profile_compatibility_flags;
    //    unsigned int(48) general_constraint_indicator_flags;
    //    unsigned int(8)  general_level_idc;
    //    ...
    auto arrayBuffer = buffer.tryCreateArrayBuffer();
    if (!arrayBuffer)
        return std::nullopt;

    bool status = true;
    auto view = JSC::DataView::create(WTF::move(arrayBuffer), 0, buffer.size());
    uint32_t profileSpaceTierIDC = view->get<uint8_t>(1, false, &status);
    if (!status)
        return std::nullopt;

    parameters.generalProfileSpace = (profileSpaceTierIDC & 0b11000000) >> 6;
    parameters.generalTierFlag = (profileSpaceTierIDC & 0b00100000) >> 5;
    parameters.generalProfileIDC = profileSpaceTierIDC & 0b00011111;

    parameters.generalProfileCompatibilityFlags = view->get<uint32_t>(2, false, &status);
    if (!status)
        return std::nullopt;

    for (unsigned i = 0; i < 6; ++i) {
        parameters.generalConstraintIndicatorFlags[i] = view->get<uint8_t>(6 + i, false, &status);
        if (!status)
            return std::nullopt;
    }

    parameters.generalLevelIDC = view->get<uint8_t>(12, false, &status);
    if (!status)
        return std::nullopt;

    return parameters;
}

static std::optional<DoViParameters::Codec> parseDoViCodecType(StringView string)
{
    static constexpr SortedArrayMap typesMap { WTF::toArray<std::pair<PackedLettersLiteral<uint32_t>, DoViParameters::Codec>>({
        { "dva1"_s, DoViParameters::Codec::AVC1 },
        { "dvav"_s, DoViParameters::Codec::AVC3 },
        { "dvh1"_s, DoViParameters::Codec::HVC1 },
        { "dvhe"_s, DoViParameters::Codec::HEV1 },
    }) };
    return makeOptionalFromPointer(typesMap.tryGet(string));
}

static std::optional<uint16_t> profileIDForAlphabeticDoViProfile(StringView profile)
{
    // See Table 7 of "Dolby Vision Profiles and Levels Version 1.3.2"
    static constexpr SortedArrayMap profilesMap { WTF::toArray<std::pair<PackedLettersLiteral<uint64_t>, uint16_t>>({
        { "dvav.se"_s, 9 },
        { "dvhe.dtb"_s, 7 },
        { "dvhe.dtr"_s, 4 },
        { "dvhe.st"_s, 8 },
        { "dvhe.stn"_s, 5 },
    }) };
    return makeOptionalFromPointer(profilesMap.tryGet(profile));
}

static bool NODELETE isValidDoViProfileID(uint16_t profileID)
{
    switch (profileID) {
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
        return true;
    default:
        return false;
    }
}

static std::optional<uint16_t> NODELETE maximumLevelIDForDoViProfileID(uint16_t profileID)
{
    // See Section 4.1 of "Dolby Vision Profiles and Levels Version 1.3.2"
    switch (profileID) {
    case 4: return 9;
    case 5: return 13;
    case 7: return 9;
    case 8: return 13;
    case 9: return 5;
    default: return std::nullopt;
    }
}

static bool NODELETE isValidProfileIDForCodec(uint16_t profileID, DoViParameters::Codec codec)
{
    if (profileID == 9)
        return codec == DoViParameters::Codec::AVC1 || codec == DoViParameters::Codec::AVC3;
    return codec == DoViParameters::Codec::HVC1 || codec == DoViParameters::Codec::HEV1;
}

std::optional<DoViParameters> parseDoViCodecParameters(StringView codecView)
{
    // The format of the DoVi codec string is specified in "Dolby Vision Profiles and Levels Version 1.3.2"
    auto codecSplit = codecView.split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return std::nullopt;

    DoViParameters parameters;

    auto codec = parseDoViCodecType(*nextElement);
    if (!codec)
        return std::nullopt;
    parameters.codec = *codec;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto profileID = *nextElement;
    if (!profileID.length())
        return std::nullopt;

    auto firstCharacter = profileID[0];
    // Profile definition can either be numeric or alpha:
    if (firstCharacter == '0') {
        auto bitstreamProfileID = parseInteger<uint8_t>(profileID);
        if (!bitstreamProfileID)
            return std::nullopt;
        parameters.bitstreamProfileID = *bitstreamProfileID;
    } else {
        auto bitstreamProfileID = profileIDForAlphabeticDoViProfile(codecView.left(5 + profileID.length()));
        if (!bitstreamProfileID)
            return std::nullopt;
        parameters.bitstreamProfileID = *bitstreamProfileID;
    }

    if (!isValidDoViProfileID(parameters.bitstreamProfileID))
        return std::nullopt;

    if (!isValidProfileIDForCodec(parameters.bitstreamProfileID, parameters.codec))
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto bitstreamLevelID = parseInteger<uint8_t>(*nextElement);
    if (!bitstreamLevelID)
        return std::nullopt;
    parameters.bitstreamLevelID = *bitstreamLevelID;

    auto maximumLevelID = maximumLevelIDForDoViProfileID(parameters.bitstreamProfileID);
    if (!maximumLevelID || parameters.bitstreamLevelID > *maximumLevelID)
        return std::nullopt;

    return parameters;
}

std::optional<DoViParameters> parseDoViDecoderConfigurationRecord(const SharedBuffer& buffer)
{
    // The format of the DoVi Configuration Record is contained in "Dolby Vision Streams Within
    // the ISO Base Media File Format, Version 2.0"

    // DoViDecoderConfigurationRecord is exacty 24 bytes long
    if (buffer.size() < 24)
        return std::nullopt;

    // align (8) class DOVIDecoderConfigurationRecord
    // {
    //     unsigned int (8) dv_version_major;
    //     unsigned int (8) dv_version_minor;
    //     unsigned int (7) dv_profile;
    //     unsigned int (6) dv_level;
    //     bit (1) rpu_present_flag;
    //     bit (1) el_present_flag;
    //     bit (1) bl_present_flag;
    //     ...
    DoViParameters parameters;
    auto arrayBuffer = buffer.tryCreateArrayBuffer();
    if (!arrayBuffer)
        return std::nullopt;

    bool status = true;
    auto view = JSC::DataView::create(WTF::move(arrayBuffer), 0, buffer.size());

    auto profileLevelAndFlags = view->get<uint16_t>(2, false, &status);
    if (!status)
        return std::nullopt;

    parameters.bitstreamProfileID = (profileLevelAndFlags & 0b1111111000000000) >> 9;
    parameters.bitstreamLevelID = (profileLevelAndFlags & 0b0000000111111000) >> 3;
    return parameters;
}

String createDoViCodecParametersString(const DoViParameters& parameters)
{
    // The format of the DoVi codec string is specified in "Dolby Vision Profiles and Levels Version 1.3.2"
    StringBuilder builder;
    builder.append("dvh1."_s);
    if (parameters.bitstreamProfileID < 10)
        builder.append('0');
    builder.append(parameters.bitstreamProfileID);
    builder.append('.');
    if (parameters.bitstreamLevelID < 10)
        builder.append('0');
    builder.append(parameters.bitstreamLevelID);
    return builder.toString();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(HEVCBitstreamParser);

HEVCNaluType hevcNaluType(uint8_t data)
{
    constexpr uint8_t naluTypeMask = 0x7E;
    return static_cast<HEVCNaluType>((data & naluTypeMask) >> 1);
}

size_t findHEVCAnnexBVpsIndex(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices)
{
    return naluIndices.findIf([&](auto& index) {
        return hevcNaluType(data[index.payloadStartOffset]) == HEVCNaluType::Vps;
    });
}

std::optional<uint8_t> findHEVCAnnexBMaxNumReorderPics(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices)
{
    auto vpsIndex = findHEVCAnnexBVpsIndex(data, naluIndices);
    if (vpsIndex == notFound)
        return std::nullopt;

    auto& index = naluIndices[vpsIndex];
    if (!index.payloadSize)
        return std::nullopt;

    return HEVCBitstreamParser::parseVpsMaxNumReorderPics(data.subspan(index.payloadStartOffset, index.payloadSize));
}

std::optional<HVCCParameterSets> parseHVCCParameterSets(std::span<const uint8_t> hvcc)
{
    // ISO/IEC 14496-15 8.3.3.1 HEVCDecoderConfigurationRecord is at a minimum 23 bytes long (fixed fields up to and including numOfArrays), before the NAL unit arrays begin.
    constexpr size_t fixedHeaderSize = 23;
    if (hvcc.size() < fixedHeaderSize)
        return std::nullopt;

    BitReader reader { hvcc };
    // configurationVersion: u(8) -- unused.
    reader.read(8);
    // general_profile_space(2), general_tier_flag(1), general_profile_idc(5) -- unused.
    reader.read(8);
    // general_profile_compatibility_flags: u(32) -- unused.
    reader.read(32);
    // general_constraint_indicator_flags: u(48) -- unused.
    reader.read(48);
    // general_level_idc: u(8) -- unused.
    reader.read(8);
    // reserved(4), min_spatial_segmentation_idc(12) -- unused.
    reader.read(16);
    // reserved(6), parallelismType(2) -- unused.
    reader.read(8);
    // reserved(6), chroma_format_idc(2) -- unused.
    reader.read(8);
    // reserved(5), bit_depth_luma_minus8(3) -- unused.
    reader.read(8);
    // reserved(5), bit_depth_chroma_minus8(3) -- unused.
    reader.read(8);
    // avgFrameRate: u(16) -- unused.
    reader.read(16);
    // constantFrameRate(2), numTemporalLayers(3), temporalIdNested(1), lengthSizeMinusOne(2)
    auto misc = reader.read<uint8_t>();
    if (!misc)
        return std::nullopt;

    auto numOfArrays = reader.read<uint8_t>();
    if (!numOfArrays)
        return std::nullopt;

    HVCCParameterSets result;
    result.lengthFieldSize = (*misc & 0x3) + 1;

    for (size_t i = 0; i < *numOfArrays; ++i) {
        // array_completeness(1), reserved(1), NAL_unit_type(6).
        auto arrayHeader = reader.read<uint8_t>();
        if (!arrayHeader)
            return std::nullopt;
        auto type = static_cast<HEVCNaluType>(*arrayHeader & 0x3f);
        auto numNalus = reader.read<uint16_t>();
        if (!numNalus)
            return std::nullopt;
        for (size_t j = 0; j < *numNalus; ++j) {
            auto size = reader.read<uint16_t>();
            if (!size || !*size)
                return std::nullopt;
            if (!reader.skipBytes(*size))
                return std::nullopt;
            result.paramSets.append({ type, hvcc.subspan(reader.byteOffset() - *size, *size) });
        }
    }

    return result;
}

std::optional<uint8_t> findHVCCMaxNumReorderPics(const HVCCParameterSets& parameterSets)
{
    for (auto& paramSet : parameterSets.paramSets) {
        if (paramSet.type == HEVCNaluType::Vps)
            return HEVCBitstreamParser::parseVpsMaxNumReorderPics(paramSet.data);
    }
    return std::nullopt;
}

namespace {

// When n == 0, returns UINT32_MAX (matches libwebrtc's H265::Log2Ceiling(0) == -1).
uint32_t hevcLog2Ceiling(uint32_t value)
{
    if (!value)
        return std::numeric_limits<uint32_t>::max();
    return 32 - static_cast<uint32_t>(std::countl_zero(value - 1));
}

// From Table A.8 - General tier and level limits. |generalLevelIDC| is 30x the actual level.
uint32_t hevcGetMaxLumaPs(int generalLevelIDC)
{
    if (generalLevelIDC <= 30)
        return 36864;
    if (generalLevelIDC <= 60)
        return 122880;
    if (generalLevelIDC <= 63)
        return 245760;
    if (generalLevelIDC <= 90)
        return 552960;
    if (generalLevelIDC <= 93)
        return 983040;
    if (generalLevelIDC <= 123)
        return 2228224;
    if (generalLevelIDC <= 156)
        return 8912896;
    return 35651584;
}

// From A.4.2 - Profile-specific level limits for the video profiles (kProfileIdcMain..kProfileIdcHighThroughput is 1..5).
size_t hevcGetDpbMaxPicBuf(int generalProfileIDC)
{
    return (generalProfileIDC >= 1 && generalProfileIDC <= 5) ? 6 : 7;
}

bool parsePredWeightTable(BitReader& reader, uint32_t chromaArrayType, uint32_t numRefIdxL0ActiveMinus1, bool isBSlice, uint32_t numRefIdxL1ActiveMinus1)
{
    constexpr uint32_t maxRefIdxActive = 15;
    if (numRefIdxL0ActiveMinus1 >= maxRefIdxActive || (isBSlice && numRefIdxL1ActiveMinus1 >= maxRefIdxActive))
        return false;

    // luma_log2_weight_denom: ue(v)
    uint32_t lumaLog2WeightDenom = reader.readExpGolomb();
    if (!reader.ok() || lumaLog2WeightDenom > 7)
        return false;
    int32_t chromaLog2WeightDenom = static_cast<int32_t>(lumaLog2WeightDenom);
    // wp_offset_half_range_{c,y} depend on high_precision_offsets_enable_flag; range extension is
    // unsupported so both are fixed to 128 instead of 1 << (bit_depth_{luma,chroma}_minus8 + 7).
    constexpr int32_t wpOffsetHalfRangeC = 1 << 7;
    constexpr int32_t wpOffsetHalfRangeY = 1 << 7;
    if (chromaArrayType) {
        // delta_chroma_log2_weight_denom: se(v)
        int32_t deltaChromaLog2WeightDenom = reader.readSignedExpGolomb();
        if (!reader.ok() || deltaChromaLog2WeightDenom < -7 || deltaChromaLog2WeightDenom > 7)
            return false;
        chromaLog2WeightDenom += deltaChromaLog2WeightDenom;
    }
    if (!reader.ok() || chromaLog2WeightDenom < 0 || chromaLog2WeightDenom > 7)
        return false;

    std::array<bool, maxRefIdxActive> lumaWeightFlagL0 { };
    std::array<bool, maxRefIdxActive> chromaWeightFlagL0 { };
    for (uint32_t i = 0; i <= numRefIdxL0ActiveMinus1; ++i)
        lumaWeightFlagL0[i] = reader.readFlag();
    if (chromaArrayType) {
        for (uint32_t i = 0; i <= numRefIdxL0ActiveMinus1; ++i)
            chromaWeightFlagL0[i] = reader.readFlag();
    }
    for (uint32_t i = 0; i <= numRefIdxL0ActiveMinus1; ++i) {
        if (lumaWeightFlagL0[i]) {
            int32_t deltaLumaWeight = reader.readSignedExpGolomb();
            if (!reader.ok() || deltaLumaWeight < -128 || deltaLumaWeight > 127)
                return false;
            int32_t lumaOffset = reader.readSignedExpGolomb();
            if (!reader.ok() || lumaOffset < -wpOffsetHalfRangeY || lumaOffset > wpOffsetHalfRangeY - 1)
                return false;
        }
        if (chromaWeightFlagL0[i]) {
            for (uint32_t j = 0; j < 2; ++j) {
                int32_t deltaChromaWeight = reader.readSignedExpGolomb();
                if (!reader.ok() || deltaChromaWeight < -128 || deltaChromaWeight > 127)
                    return false;
                int32_t deltaChromaOffset = reader.readSignedExpGolomb();
                if (!reader.ok() || deltaChromaOffset < -4 * wpOffsetHalfRangeC || deltaChromaOffset > 4 * wpOffsetHalfRangeC - 1)
                    return false;
            }
        }
    }

    if (isBSlice) {
        std::array<bool, maxRefIdxActive> lumaWeightFlagL1 { };
        std::array<bool, maxRefIdxActive> chromaWeightFlagL1 { };
        // Matches libwebrtc's h265_bitstream_parser.cc bit-for-bit, including its use of '<' here
        // (spec syntax calls for '<=', matching the other loops below).
        for (uint32_t i = 0; i < numRefIdxL1ActiveMinus1; ++i)
            lumaWeightFlagL1[i] = reader.readFlag();
        if (chromaArrayType) {
            for (uint32_t i = 0; i <= numRefIdxL1ActiveMinus1; ++i)
                chromaWeightFlagL1[i] = reader.readFlag();
        }
        for (uint32_t i = 0; i <= numRefIdxL1ActiveMinus1; ++i) {
            if (lumaWeightFlagL1[i]) {
                int32_t deltaLumaWeight = reader.readSignedExpGolomb();
                if (!reader.ok() || deltaLumaWeight < -128 || deltaLumaWeight > 127)
                    return false;
                int32_t lumaOffset = reader.readSignedExpGolomb();
                if (!reader.ok() || lumaOffset < -wpOffsetHalfRangeY || lumaOffset > wpOffsetHalfRangeY - 1)
                    return false;
            }
            if (chromaWeightFlagL1[i]) {
                for (uint32_t j = 0; j < 2; ++j) {
                    int32_t deltaChromaWeight = reader.readSignedExpGolomb();
                    if (!reader.ok() || deltaChromaWeight < -128 || deltaChromaWeight > 127)
                        return false;
                    int32_t deltaChromaOffset = reader.readSignedExpGolomb();
                    if (!reader.ok() || deltaChromaOffset < -4 * wpOffsetHalfRangeC || deltaChromaOffset > 4 * wpOffsetHalfRangeC - 1)
                        return false;
                }
            }
        }
    }

    return true;
}

} // anonymous namespace

std::optional<HEVCBitstreamParser::ProfileTierLevel> HEVCBitstreamParser::parseProfileTierLevel(bool profilePresent, uint32_t maxNumSubLayersMinus1, BitReader& reader)
{
    ProfileTierLevel result;
    if (profilePresent) {
        // general_profile_space: u(2), must be 0.
        if (reader.readBits(2))
            return std::nullopt;
        // general_tier_flag or reserved: u(1)
        reader.consumeBits(1);
        // general_profile_idc: u(5)
        result.generalProfileIDC = reader.readBits(5);
        if (!reader.ok() || result.generalProfileIDC > 11)
            return std::nullopt;
        // general_profile_compatibility_flag[32]: u(32) -- unused.
        reader.consumeBits(32);
        // general_progressive_source_flag, general_interlaced_source_flag: u(1) each.
        bool progressive = reader.readFlag();
        bool interlaced = reader.readFlag();
        if (!reader.ok() || (!progressive && interlaced))
            return std::nullopt; // Interlaced streams are not supported.
        // general_non_packed_constraint_flag, general_frame_only_constraint_flag: u(1) each -- unused.
        reader.consumeBits(2);
        // general_reserved_zero_7bits
        reader.consumeBits(7);
        // general_one_picture_only_constraint_flag: u(1) -- unused.
        reader.consumeBits(1);
        // general_reserved_zero_35bits
        reader.consumeBits(35);
        // general_inbld_flag
        reader.consumeBits(1);
    }
    // general_level_idc: u(8)
    result.generalLevelIDC = reader.readBits(8);

    std::array<bool, 8> subLayerProfilePresentFlag { };
    std::array<bool, 8> subLayerLevelPresentFlag { };
    for (uint32_t i = 0; i < maxNumSubLayersMinus1 && i < subLayerProfilePresentFlag.size(); ++i) {
        subLayerProfilePresentFlag[i] = reader.readFlag();
        subLayerLevelPresentFlag[i] = reader.readFlag();
    }
    if (maxNumSubLayersMinus1 > 0) {
        for (uint32_t i = maxNumSubLayersMinus1; i < 8; ++i)
            reader.consumeBits(2);
    }
    for (uint32_t i = 0; i < maxNumSubLayersMinus1 && i < subLayerProfilePresentFlag.size(); ++i) {
        if (subLayerProfilePresentFlag[i]) {
            // sub_layer_profile_space, sub_layer_tier_flag, sub_layer_profile_idc, sub_layer_profile_compatibility_flag[32],
            // sub_layer_{progressive,interlaced,non_packed_constraint,frame_only_constraint}_flag, 43 reserved bits, sub_layer_inbld_flag.
            reader.consumeBits(2 + 1 + 5 + 32 + 2 + 2 + 43 + 1);
        }
        if (subLayerLevelPresentFlag[i]) {
            // sub_layer_level_idc: u(8)
            reader.consumeBits(8);
        }
    }

    if (!reader.ok())
        return std::nullopt;

    return result;
}

bool HEVCBitstreamParser::parseScalingListData(BitReader& reader)
{
    constexpr int maxNumSizeIds = 4;
    constexpr int maxNumMatrixIds = 6;
    constexpr int maxNumCoefs = 64;
    for (int sizeId = 0; sizeId < maxNumSizeIds; ++sizeId) {
        for (int matrixId = 0; matrixId < maxNumMatrixIds; matrixId += (sizeId == 3) ? 3 : 1) {
            // scaling_list_pred_mode_flag: u(1)
            if (!reader.readFlag()) {
                // scaling_list_pred_matrix_id_delta: ue(v)
                uint32_t scalingListPredMatrixIdDelta = reader.readExpGolomb();
                uint32_t maxDelta = sizeId <= 2 ? static_cast<uint32_t>(matrixId) : static_cast<uint32_t>(matrixId / 3);
                if (!reader.ok() || scalingListPredMatrixIdDelta > maxDelta)
                    return false;
            } else {
                uint32_t coefNum = std::min(maxNumCoefs, 1 << (4 + (sizeId << 1)));
                if (sizeId > 1) {
                    // scaling_list_dc_coef_minus8: se(v)
                    int32_t scalingListDcCoefMinus8 = reader.readSignedExpGolomb();
                    if (!reader.ok() || scalingListDcCoefMinus8 < -7 || scalingListDcCoefMinus8 > 247)
                        return false;
                }
                for (uint32_t i = 0; i < coefNum; ++i) {
                    // scaling_list_delta_coef: se(v)
                    int32_t scalingListDeltaCoef = reader.readSignedExpGolomb();
                    if (!reader.ok() || scalingListDeltaCoef < -128 || scalingListDeltaCoef > 127)
                        return false;
                }
            }
        }
    }
    return reader.ok();
}

std::optional<HEVCBitstreamParser::ShortTermRefPicSet> HEVCBitstreamParser::parseShortTermRefPicSet(uint32_t stRpsIndex, uint32_t numShortTermRefPicSets, const Vector<ShortTermRefPicSet>& shortTermRefPicSets, uint32_t spsMaxDecPicBufferingMinus1, BitReader& reader)
{
    constexpr uint32_t maxShortTermRefPicSets = 64;
    ShortTermRefPicSet result;

    bool interRefPicSetPredictionFlag = false;
    if (stRpsIndex)
        interRefPicSetPredictionFlag = reader.readFlag();

    if (interRefPicSetPredictionFlag) {
        uint32_t deltaIdxMinus1 = 0;
        if (stRpsIndex == numShortTermRefPicSets) {
            // delta_idx_minus1: ue(v)
            deltaIdxMinus1 = reader.readExpGolomb();
            if (!reader.ok() || deltaIdxMinus1 > stRpsIndex - 1)
                return std::nullopt;
        }
        // delta_rps_sign: u(1)
        int deltaRpsSign = reader.readBits(1);
        // abs_delta_rps_minus1: ue(v)
        uint32_t absDeltaRpsMinus1 = reader.readExpGolomb();
        if (!reader.ok() || absDeltaRpsMinus1 > 0x7FFF)
            return std::nullopt;
        int deltaRps = (1 - 2 * deltaRpsSign) * static_cast<int>(absDeltaRpsMinus1 + 1);
        uint32_t refRpsIndex = stRpsIndex - (deltaIdxMinus1 + 1);
        if (refRpsIndex >= shortTermRefPicSets.size())
            return std::nullopt;
        auto& refSet = shortTermRefPicSets[refRpsIndex];
        if (!reader.ok() || refSet.numDeltaPocs > maxShortTermRefPicSets || refSet.numNegativePics + refSet.numPositivePics > maxShortTermRefPicSets)
            return std::nullopt;

        std::array<bool, maxShortTermRefPicSets + 1> usedByCurrPicFlag { };
        std::array<bool, maxShortTermRefPicSets + 1> useDeltaFlag;
        // 7.4.8 - use_delta_flag defaults to 1 if not present.
        useDeltaFlag.fill(true);

        for (uint32_t j = 0; j <= refSet.numDeltaPocs; ++j) {
            // used_by_curr_pic_flag: u(1)
            usedByCurrPicFlag[j] = reader.readFlag();
            if (!usedByCurrPicFlag[j]) {
                // use_delta_flag: u(1)
                useDeltaFlag[j] = reader.readFlag();
            }
        }

        // Equation 7-61
        int i = 0;
        for (int j = static_cast<int>(refSet.numPositivePics) - 1; j >= 0; --j) {
            int dPoc = refSet.deltaPocS1[j] + deltaRps;
            if (dPoc < 0 && useDeltaFlag[refSet.numNegativePics + j]) {
                result.deltaPocS0[i] = dPoc;
                result.usedByCurrPicS0[i++] = usedByCurrPicFlag[refSet.numNegativePics + j];
            }
        }
        if (deltaRps < 0 && useDeltaFlag[refSet.numDeltaPocs]) {
            result.deltaPocS0[i] = deltaRps;
            result.usedByCurrPicS0[i++] = usedByCurrPicFlag[refSet.numDeltaPocs];
        }
        for (uint32_t j = 0; j < refSet.numNegativePics; ++j) {
            int dPoc = refSet.deltaPocS0[j] + deltaRps;
            if (dPoc < 0 && useDeltaFlag[j]) {
                result.deltaPocS0[i] = dPoc;
                result.usedByCurrPicS0[i++] = usedByCurrPicFlag[j];
            }
        }
        result.numNegativePics = i;

        // Equation 7-62
        i = 0;
        for (int j = static_cast<int>(refSet.numNegativePics) - 1; j >= 0; --j) {
            int dPoc = refSet.deltaPocS0[j] + deltaRps;
            if (dPoc > 0 && useDeltaFlag[j]) {
                result.deltaPocS1[i] = dPoc;
                result.usedByCurrPicS1[i++] = usedByCurrPicFlag[j];
            }
        }
        if (deltaRps > 0 && useDeltaFlag[refSet.numDeltaPocs]) {
            result.deltaPocS1[i] = deltaRps;
            result.usedByCurrPicS1[i++] = usedByCurrPicFlag[refSet.numDeltaPocs];
        }
        for (uint32_t j = 0; j < refSet.numPositivePics; ++j) {
            int dPoc = refSet.deltaPocS1[j] + deltaRps;
            if (dPoc > 0 && useDeltaFlag[refSet.numNegativePics + j]) {
                result.deltaPocS1[i] = dPoc;
                result.usedByCurrPicS1[i++] = usedByCurrPicFlag[refSet.numNegativePics + j];
            }
        }
        result.numPositivePics = i;

        if (!reader.ok() || result.numNegativePics > spsMaxDecPicBufferingMinus1)
            return std::nullopt;
        if (!reader.ok() || result.numPositivePics > spsMaxDecPicBufferingMinus1 - result.numNegativePics)
            return std::nullopt;
    } else {
        constexpr uint32_t maxSPSPics = 16;
        // num_negative_pics, num_positive_pics: ue(v)
        result.numNegativePics = reader.readExpGolomb();
        result.numPositivePics = reader.readExpGolomb();
        if (!reader.ok() || result.numNegativePics > maxSPSPics || result.numPositivePics > maxSPSPics || result.numNegativePics + result.numPositivePics > maxSPSPics)
            return std::nullopt;
        if (result.numNegativePics > spsMaxDecPicBufferingMinus1)
            return std::nullopt;
        if (result.numPositivePics > spsMaxDecPicBufferingMinus1 - result.numNegativePics)
            return std::nullopt;

        for (uint32_t i = 0; i < result.numNegativePics; ++i) {
            // delta_poc_s0_minus1: ue(v)
            uint32_t deltaPocS0Minus1 = reader.readExpGolomb();
            if (!reader.ok() || deltaPocS0Minus1 > 0x7FFF)
                return std::nullopt;
            result.deltaPocS0[i] = i ? result.deltaPocS0[i - 1] - static_cast<int32_t>(deltaPocS0Minus1 + 1) : -static_cast<int32_t>(deltaPocS0Minus1 + 1);
            // used_by_curr_pic_s0_flag: u(1)
            result.usedByCurrPicS0[i] = reader.readFlag();
        }

        for (uint32_t i = 0; i < result.numPositivePics; ++i) {
            // delta_poc_s1_minus1: ue(v)
            uint32_t deltaPocS1Minus1 = reader.readExpGolomb();
            if (!reader.ok() || deltaPocS1Minus1 > 0x7FFF)
                return std::nullopt;
            result.deltaPocS1[i] = i ? result.deltaPocS1[i - 1] + static_cast<int32_t>(deltaPocS1Minus1 + 1) : static_cast<int32_t>(deltaPocS1Minus1 + 1);
            // used_by_curr_pic_s1_flag: u(1)
            result.usedByCurrPicS1[i] = reader.readFlag();
        }
    }

    result.numDeltaPocs = result.numNegativePics + result.numPositivePics;

    if (!reader.ok())
        return std::nullopt;

    return result;
}

std::optional<uint8_t> HEVCBitstreamParser::parseVpsMaxNumReorderPics(std::span<const uint8_t> data)
{
    if (data.size() <= hevcNaluHeaderSize)
        return std::nullopt;

    auto rbsp = parseRbsp(data.subspan(hevcNaluHeaderSize));
    BitReader reader(rbsp.span());

    // vps_video_parameter_set_id: u(4), vps_base_layer_internal_flag: u(1),
    // vps_base_layer_available_flag: u(1), vps_max_layers_minus1: u(6) -- all unused.
    reader.consumeBits(4 + 1 + 1 + 6);
    // vps_max_sub_layers_minus1: u(3)
    uint32_t vpsMaxSubLayersMinus1 = reader.readBits(3);
    if (!reader.ok() || vpsMaxSubLayersMinus1 > 6)
        return std::nullopt;
    // vps_temporal_id_nesting_flag: u(1), vps_reserved_0xffff_16bits: u(16) -- unused.
    reader.consumeBits(1 + 16);

    if (!parseProfileTierLevel(true, vpsMaxSubLayersMinus1, reader))
        return std::nullopt;

    // vps_sub_layer_ordering_info_present_flag: u(1)
    bool vpsSubLayerOrderingInfoPresentFlag = reader.readFlag();
    uint32_t maxNumReorderPics = 0;
    for (uint32_t i = vpsSubLayerOrderingInfoPresentFlag ? 0 : vpsMaxSubLayersMinus1; i <= vpsMaxSubLayersMinus1; ++i) {
        // vps_max_dec_pic_buffering_minus1: ue(v) -- unused.
        reader.readExpGolomb();
        // vps_max_num_reorder_pics: ue(v)
        uint32_t numReorderPics = reader.readExpGolomb();
        // vps_max_latency_increase_plus1: ue(v) -- unused.
        reader.readExpGolomb();
        if (!reader.ok())
            return std::nullopt;
        maxNumReorderPics = std::max(maxNumReorderPics, numReorderPics);
    }

    // Matches libwebrtc's ComputeH265ReorderSizeFromVPS, which clamps to a max of 16.
    constexpr uint32_t maxSupportedReorderPics = 16;
    return static_cast<uint8_t>(std::min(maxNumReorderPics, maxSupportedReorderPics));
}

std::optional<HEVCBitstreamParser::SpsState> HEVCBitstreamParser::parseSps(std::span<const uint8_t> data)
{
    auto rbsp = parseRbsp(data);
    BitReader reader(rbsp.span());
    SpsState sps;

    // sps_video_parameter_set_id: u(4) -- unused.
    reader.consumeBits(4);
    // sps_max_sub_layers_minus1: u(3)
    uint32_t spsMaxSubLayersMinus1 = reader.readBits(3);
    if (!reader.ok() || spsMaxSubLayersMinus1 > 6)
        return std::nullopt;
    sps.spsMaxSubLayersMinus1 = spsMaxSubLayersMinus1;
    // sps_temporal_id_nesting_flag: u(1)
    reader.consumeBits(1);

    auto profileTierLevel = parseProfileTierLevel(true, sps.spsMaxSubLayersMinus1, reader);
    if (!profileTierLevel)
        return std::nullopt;

    // sps_seq_parameter_set_id: ue(v)
    uint32_t spsId = reader.readExpGolomb();
    if (!reader.ok() || spsId > 15)
        return std::nullopt;
    sps.spsId = spsId;
    // chroma_format_idc: ue(v)
    sps.chromaFormatIDC = reader.readExpGolomb();
    if (!reader.ok() || sps.chromaFormatIDC > 3)
        return std::nullopt;
    if (sps.chromaFormatIDC == 3) {
        // separate_colour_plane_flag: u(1)
        sps.separateColourPlaneFlag = reader.readFlag();
    }

    // pic_width_in_luma_samples, pic_height_in_luma_samples: ue(v)
    uint32_t picWidthInLumaSamples = reader.readExpGolomb();
    if (!reader.ok() || !picWidthInLumaSamples)
        return std::nullopt;
    uint32_t picHeightInLumaSamples = reader.readExpGolomb();
    if (!reader.ok() || !picHeightInLumaSamples)
        return std::nullopt;

    // Equation A-2: Calculate max_dpb_size.
    uint32_t maxLumaPS = hevcGetMaxLumaPs(profileTierLevel->generalLevelIDC);
    uint64_t picSizeInSamplesY = static_cast<uint64_t>(picWidthInLumaSamples) * picHeightInLumaSamples;
    size_t maxDpbPicBuf = hevcGetDpbMaxPicBuf(profileTierLevel->generalProfileIDC);
    uint32_t maxDpbSize;
    if (picSizeInSamplesY <= (maxLumaPS >> 2))
        maxDpbSize = static_cast<uint32_t>(std::min<size_t>(4 * maxDpbPicBuf, 16));
    else if (picSizeInSamplesY <= (maxLumaPS >> 1))
        maxDpbSize = static_cast<uint32_t>(std::min<size_t>(2 * maxDpbPicBuf, 16));
    else if (picSizeInSamplesY <= ((3ull * maxLumaPS) >> 2))
        maxDpbSize = static_cast<uint32_t>(std::min<size_t>((4 * maxDpbPicBuf) / 3, 16));
    else
        maxDpbSize = static_cast<uint32_t>(maxDpbPicBuf);

    // conformance_window_flag: u(1)
    bool conformanceWindowFlag = reader.readFlag();
    int subWidthC = ((sps.chromaFormatIDC == 1 || sps.chromaFormatIDC == 2) && !sps.separateColourPlaneFlag) ? 2 : 1;
    int subHeightC = (sps.chromaFormatIDC == 1 && !sps.separateColourPlaneFlag) ? 2 : 1;
    if (conformanceWindowFlag) {
        // conf_win_{left,right,top,bottom}_offset: ue(v)
        uint32_t confWinLeftOffset = reader.readExpGolomb();
        uint32_t confWinRightOffset = reader.readExpGolomb();
        uint32_t confWinTopOffset = reader.readExpGolomb();
        uint32_t confWinBottomOffset = reader.readExpGolomb();
        uint64_t widthCrop = (static_cast<uint64_t>(confWinLeftOffset) + confWinRightOffset) * subWidthC;
        if (!reader.ok() || widthCrop >= picWidthInLumaSamples)
            return std::nullopt;
        uint64_t heightCrop = (static_cast<uint64_t>(confWinTopOffset) + confWinBottomOffset) * subHeightC;
        if (!reader.ok() || heightCrop >= picHeightInLumaSamples)
            return std::nullopt;
    }

    // bit_depth_luma_minus8: ue(v)
    sps.bitDepthLumaMinus8 = reader.readExpGolomb();
    if (!reader.ok() || sps.bitDepthLumaMinus8 > 8)
        return std::nullopt;
    // bit_depth_chroma_minus8: ue(v) -- unused.
    uint32_t bitDepthChromaMinus8 = reader.readExpGolomb();
    if (!reader.ok() || bitDepthChromaMinus8 > 8)
        return std::nullopt;
    // log2_max_pic_order_cnt_lsb_minus4: ue(v)
    sps.log2MaxPicOrderCntLsbMinus4 = reader.readExpGolomb();
    if (!reader.ok() || sps.log2MaxPicOrderCntLsbMinus4 > 12)
        return std::nullopt;

    // sps_sub_layer_ordering_info_present_flag: u(1)
    bool spsSubLayerOrderingInfoPresentFlag = reader.readFlag();
    std::array<uint32_t, 7> spsMaxNumReorderPics { };
    for (uint32_t i = spsSubLayerOrderingInfoPresentFlag ? 0 : spsMaxSubLayersMinus1; i <= spsMaxSubLayersMinus1; ++i) {
        // sps_max_dec_pic_buffering_minus1: ue(v)
        sps.spsMaxDecPicBufferingMinus1[i] = reader.readExpGolomb();
        if (!reader.ok() || sps.spsMaxDecPicBufferingMinus1[i] + 1 > maxDpbSize)
            return std::nullopt;
        // sps_max_num_reorder_pics: ue(v)
        spsMaxNumReorderPics[i] = reader.readExpGolomb();
        if (!reader.ok() || spsMaxNumReorderPics[i] > sps.spsMaxDecPicBufferingMinus1[i])
            return std::nullopt;
        if (i > 0) {
            if (sps.spsMaxDecPicBufferingMinus1[i] < sps.spsMaxDecPicBufferingMinus1[i - 1])
                return std::nullopt;
            if (spsMaxNumReorderPics[i] < spsMaxNumReorderPics[i - 1])
                return std::nullopt;
        }
        // sps_max_latency_increase_plus1: ue(v) -- unused.
        reader.readExpGolomb();
    }
    if (!spsSubLayerOrderingInfoPresentFlag) {
        for (uint32_t i = 0; i < spsMaxSubLayersMinus1; ++i)
            sps.spsMaxDecPicBufferingMinus1[i] = sps.spsMaxDecPicBufferingMinus1[spsMaxSubLayersMinus1];
    }

    // log2_min_luma_coding_block_size_minus3: ue(v)
    sps.log2MinLumaCodingBlockSizeMinus3 = reader.readExpGolomb();
    if (!reader.ok() || sps.log2MinLumaCodingBlockSizeMinus3 > 27)
        return std::nullopt;
    // log2_diff_max_min_luma_coding_block_size: ue(v)
    sps.log2DiffMaxMinLumaCodingBlockSize = reader.readExpGolomb();
    uint32_t minCbLog2SizeY = sps.log2MinLumaCodingBlockSizeMinus3 + 3;
    uint32_t ctbLog2SizeY = minCbLog2SizeY + sps.log2DiffMaxMinLumaCodingBlockSize;
    if (!reader.ok() || ctbLog2SizeY > 30)
        return std::nullopt;
    uint32_t minCbSizeY = 1u << minCbLog2SizeY;
    uint32_t ctbSizeY = 1u << ctbLog2SizeY;
    sps.picWidthInCtbsY = (picWidthInLumaSamples + ctbSizeY - 1) / ctbSizeY;
    sps.picHeightInCtbsY = (picHeightInLumaSamples + ctbSizeY - 1) / ctbSizeY;
    if ((picWidthInLumaSamples % minCbSizeY) || (picHeightInLumaSamples % minCbSizeY))
        return std::nullopt;

    // log2_min_luma_transform_block_size_minus2: ue(v)
    uint32_t log2MinLumaTransformBlockSizeMinus2 = reader.readExpGolomb();
    if (!reader.ok() || log2MinLumaTransformBlockSizeMinus2 > minCbLog2SizeY - 3)
        return std::nullopt;
    uint32_t minTbLog2SizeY = log2MinLumaTransformBlockSizeMinus2 + 2;
    // log2_diff_max_min_luma_transform_block_size: ue(v) -- unused beyond range check.
    uint32_t log2DiffMaxMinLumaTransformBlockSize = reader.readExpGolomb();
    if (!reader.ok() || log2DiffMaxMinLumaTransformBlockSize > std::min(ctbLog2SizeY, 5u) - minTbLog2SizeY)
        return std::nullopt;
    // max_transform_hierarchy_depth_inter: ue(v) -- unused beyond range check.
    uint32_t maxTransformHierarchyDepthInter = reader.readExpGolomb();
    if (!reader.ok() || maxTransformHierarchyDepthInter > ctbLog2SizeY - minTbLog2SizeY)
        return std::nullopt;
    // max_transform_hierarchy_depth_intra: ue(v) -- unused beyond range check.
    uint32_t maxTransformHierarchyDepthIntra = reader.readExpGolomb();
    if (!reader.ok() || maxTransformHierarchyDepthIntra > ctbLog2SizeY - minTbLog2SizeY)
        return std::nullopt;

    // scaling_list_enabled_flag: u(1)
    if (reader.readFlag()) {
        // sps_scaling_list_data_present_flag: u(1)
        if (reader.readFlag() && !parseScalingListData(reader))
            return std::nullopt;
    }

    // amp_enabled_flag: u(1) -- unused.
    reader.consumeBits(1);
    // sample_adaptive_offset_enabled_flag: u(1)
    sps.sampleAdaptiveOffsetEnabledFlag = reader.readFlag();
    // pcm_enabled_flag: u(1)
    if (reader.readFlag()) {
        // pcm_sample_bit_depth_luma_minus1, pcm_sample_bit_depth_chroma_minus1: u(4) each -- unused.
        reader.consumeBits(8);
        // log2_min_pcm_luma_coding_block_size_minus3: ue(v)
        uint32_t log2MinPcmLumaCodingBlockSizeMinus3 = reader.readExpGolomb();
        if (!reader.ok() || log2MinPcmLumaCodingBlockSizeMinus3 > 2)
            return std::nullopt;
        uint32_t log2MinIpcmCbSizeY = log2MinPcmLumaCodingBlockSizeMinus3 + 3;
        if (!reader.ok() || log2MinIpcmCbSizeY < std::min(minCbLog2SizeY, 5u) || log2MinIpcmCbSizeY > std::min(ctbLog2SizeY, 5u))
            return std::nullopt;
        // log2_diff_max_min_pcm_luma_coding_block_size: ue(v) -- unused beyond range check.
        uint32_t log2DiffMaxMinPcmLumaCodingBlockSize = reader.readExpGolomb();
        if (!reader.ok() || log2DiffMaxMinPcmLumaCodingBlockSize > std::min(ctbLog2SizeY, 5u) - log2MinIpcmCbSizeY)
            return std::nullopt;
        // pcm_loop_filter_disabled_flag: u(1)
        reader.consumeBits(1);
    }

    // num_short_term_ref_pic_sets: ue(v)
    constexpr uint32_t maxShortTermRefPicSets = 64;
    sps.numShortTermRefPicSets = reader.readExpGolomb();
    if (!reader.ok() || sps.numShortTermRefPicSets > maxShortTermRefPicSets)
        return std::nullopt;
    sps.shortTermRefPicSet.resize(sps.numShortTermRefPicSets);
    for (uint32_t stRpsIndex = 0; stRpsIndex < sps.numShortTermRefPicSets; ++stRpsIndex) {
        auto refPicSet = parseShortTermRefPicSet(stRpsIndex, sps.numShortTermRefPicSets, sps.shortTermRefPicSet, sps.spsMaxDecPicBufferingMinus1[sps.spsMaxSubLayersMinus1], reader);
        if (!refPicSet)
            return std::nullopt;
        sps.shortTermRefPicSet[stRpsIndex] = *refPicSet;
    }

    // long_term_ref_pics_present_flag: u(1)
    sps.longTermRefPicsPresentFlag = reader.readFlag();
    if (sps.longTermRefPicsPresentFlag) {
        constexpr uint32_t maxLongTermRefPicSets = 32;
        // num_long_term_ref_pics_sps: ue(v)
        sps.numLongTermRefPicsSps = reader.readExpGolomb();
        if (!reader.ok() || sps.numLongTermRefPicsSps > maxLongTermRefPicSets)
            return std::nullopt;
        sps.usedByCurrPicLtSpsFlag.resize(sps.numLongTermRefPicsSps);
        for (uint32_t i = 0; i < sps.numLongTermRefPicsSps; ++i) {
            // lt_ref_pic_poc_lsb_sps: u(v)
            reader.consumeBits(sps.log2MaxPicOrderCntLsbMinus4 + 4);
            // used_by_curr_pic_lt_sps_flag: u(1)
            sps.usedByCurrPicLtSpsFlag[i] = reader.readFlag();
        }
    }

    // sps_temporal_mvp_enabled_flag: u(1)
    sps.spsTemporalMvpEnabledFlag = reader.readFlag();

    // Far enough! We don't use the rest of the SPS.
    sps.picWidthInLumaSamples = picWidthInLumaSamples;
    sps.picHeightInLumaSamples = picHeightInLumaSamples;

    if (!reader.ok())
        return std::nullopt;

    return sps;
}

std::optional<HEVCBitstreamParser::PpsState> HEVCBitstreamParser::parsePps(std::span<const uint8_t> data, const SpsState* sps)
{
    if (!sps)
        return std::nullopt;

    auto rbsp = parseRbsp(data);
    BitReader reader(rbsp.span());
    PpsState pps;

    // pic_parameter_set_id: ue(v)
    uint32_t ppsId = reader.readExpGolomb();
    if (!reader.ok() || ppsId > 63)
        return std::nullopt;
    pps.ppsId = ppsId;
    // seq_parameter_set_id: ue(v)
    uint32_t spsId = reader.readExpGolomb();
    if (!reader.ok() || spsId > 15)
        return std::nullopt;
    pps.spsId = spsId;

    // dependent_slice_segments_enabled_flag: u(1)
    pps.dependentSliceSegmentsEnabledFlag = reader.readFlag();
    // output_flag_present_flag: u(1)
    pps.outputFlagPresentFlag = reader.readFlag();
    // num_extra_slice_header_bits: u(3)
    pps.numExtraSliceHeaderBits = reader.readBits(3);
    if (!reader.ok() || pps.numExtraSliceHeaderBits > 2)
        return std::nullopt;
    // sign_data_hiding_enabled_flag: u(1)
    reader.consumeBits(1);
    // cabac_init_present_flag: u(1)
    pps.cabacInitPresentFlag = reader.readFlag();

    constexpr uint32_t maxRefIdxActive = 15;
    // num_ref_idx_l0_default_active_minus1: ue(v)
    pps.numRefIdxL0DefaultActiveMinus1 = reader.readExpGolomb();
    if (!reader.ok() || pps.numRefIdxL0DefaultActiveMinus1 > maxRefIdxActive - 1)
        return std::nullopt;
    // num_ref_idx_l1_default_active_minus1: ue(v)
    pps.numRefIdxL1DefaultActiveMinus1 = reader.readExpGolomb();
    if (!reader.ok() || pps.numRefIdxL1DefaultActiveMinus1 > maxRefIdxActive - 1)
        return std::nullopt;

    // init_qp_minus26: se(v)
    pps.initQPMinus26 = reader.readSignedExpGolomb();
    pps.qpBdOffsetY = 6 * sps->bitDepthLumaMinus8;
    if (!reader.ok() || pps.initQPMinus26 < -(26 + pps.qpBdOffsetY) || pps.initQPMinus26 > 25)
        return std::nullopt;

    // constrained_intra_pred_flag, transform_skip_enabled_flag: u(1) each -- unused.
    reader.consumeBits(2);
    // cu_qp_delta_enabled_flag: u(1)
    if (reader.readFlag()) {
        // diff_cu_qp_delta_depth: ue(v) -- unused beyond range check.
        uint32_t diffCuQpDeltaDepth = reader.readExpGolomb();
        if (!reader.ok() || diffCuQpDeltaDepth > sps->log2DiffMaxMinLumaCodingBlockSize)
            return std::nullopt;
    }
    // pps_cb_qp_offset: se(v)
    int32_t ppsCbQpOffset = reader.readSignedExpGolomb();
    if (!reader.ok() || ppsCbQpOffset < -12 || ppsCbQpOffset > 12)
        return std::nullopt;
    // pps_cr_qp_offset: se(v)
    int32_t ppsCrQpOffset = reader.readSignedExpGolomb();
    if (!reader.ok() || ppsCrQpOffset < -12 || ppsCrQpOffset > 12)
        return std::nullopt;
    // pps_slice_chroma_qp_offsets_present_flag: u(1)
    reader.consumeBits(1);
    // weighted_pred_flag: u(1)
    pps.weightedPredFlag = reader.readFlag();
    // weighted_bipred_flag: u(1)
    pps.weightedBipredFlag = reader.readFlag();
    // transquant_bypass_enabled_flag: u(1)
    reader.consumeBits(1);
    // tiles_enabled_flag: u(1)
    bool tilesEnabledFlag = reader.readFlag();
    // entropy_coding_sync_enabled_flag: u(1)
    reader.consumeBits(1);
    if (tilesEnabledFlag) {
        constexpr uint32_t maxNumTileColumnWidth = 19;
        constexpr uint32_t maxNumTileRowHeight = 21;
        // num_tile_columns_minus1: ue(v)
        uint32_t numTileColumnsMinus1 = reader.readExpGolomb();
        if (!reader.ok() || !sps->picWidthInCtbsY || numTileColumnsMinus1 > sps->picWidthInCtbsY - 1 || numTileColumnsMinus1 >= maxNumTileColumnWidth)
            return std::nullopt;
        // num_tile_rows_minus1: ue(v)
        uint32_t numTileRowsMinus1 = reader.readExpGolomb();
        if (!reader.ok() || !sps->picHeightInCtbsY || numTileRowsMinus1 > sps->picHeightInCtbsY - 1)
            return std::nullopt;
        if ((!numTileColumnsMinus1 && !numTileRowsMinus1) || numTileRowsMinus1 >= maxNumTileRowHeight)
            return std::nullopt;
        // uniform_spacing_flag: u(1)
        if (!reader.readFlag()) {
            std::array<int32_t, maxNumTileColumnWidth> columnWidthMinus1 { };
            columnWidthMinus1[numTileColumnsMinus1] = static_cast<int32_t>(sps->picWidthInCtbsY) - 1;
            for (uint32_t i = 0; i < numTileColumnsMinus1; ++i) {
                // column_width_minus1: ue(v)
                columnWidthMinus1[i] = reader.readExpGolomb();
                if (!reader.ok() || columnWidthMinus1[i] < 0 || columnWidthMinus1[i] > columnWidthMinus1[numTileColumnsMinus1] - 1)
                    return std::nullopt;
                columnWidthMinus1[numTileColumnsMinus1] -= columnWidthMinus1[i] + 1;
            }
            std::array<int32_t, maxNumTileRowHeight> rowHeightMinus1 { };
            rowHeightMinus1[numTileRowsMinus1] = static_cast<int32_t>(sps->picHeightInCtbsY) - 1;
            for (uint32_t i = 0; i < numTileRowsMinus1; ++i) {
                // row_height_minus1: ue(v)
                rowHeightMinus1[i] = reader.readExpGolomb();
                if (!reader.ok() || rowHeightMinus1[i] < 0 || rowHeightMinus1[i] > rowHeightMinus1[numTileRowsMinus1] - 1)
                    return std::nullopt;
                rowHeightMinus1[numTileRowsMinus1] -= rowHeightMinus1[i] + 1;
            }
            // loop_filter_across_tiles_enabled_flag: u(1)
            reader.consumeBits(1);
        }
    }
    // pps_loop_filter_across_slices_enabled_flag: u(1)
    reader.consumeBits(1);
    // deblocking_filter_control_present_flag: u(1)
    if (reader.readFlag()) {
        // deblocking_filter_override_enabled_flag: u(1)
        reader.consumeBits(1);
        // pps_deblocking_filter_disabled_flag: u(1)
        if (!reader.readFlag()) {
            // pps_beta_offset_div2: se(v)
            int32_t ppsBetaOffsetDiv2 = reader.readSignedExpGolomb();
            if (!reader.ok() || ppsBetaOffsetDiv2 < -6 || ppsBetaOffsetDiv2 > 6)
                return std::nullopt;
            // pps_tc_offset_div2: se(v)
            int32_t ppsTcOffsetDiv2 = reader.readSignedExpGolomb();
            if (!reader.ok() || ppsTcOffsetDiv2 < -6 || ppsTcOffsetDiv2 > 6)
                return std::nullopt;
        }
    }
    // pps_scaling_list_data_present_flag: u(1)
    if (reader.readFlag() && !parseScalingListData(reader))
        return std::nullopt;
    // lists_modification_present_flag: u(1)
    pps.listsModificationPresentFlag = reader.readFlag();

    if (!reader.ok())
        return std::nullopt;

    return pps;
}

auto HEVCBitstreamParser::parseNonParameterSetNalu(std::span<const uint8_t> source, uint8_t naluType) -> ParseResult
{
    // Based on section 7.3.6.1 of the H.265 spec: http://www.itu.int/rec/T-REC-H.265
    m_lastSliceQPDelta = std::nullopt;
    m_lastSlicePpsId = std::nullopt;

    auto sliceRbsp = parseRbsp(source);
    if (sliceRbsp.size() < hevcNaluHeaderSize)
        return ParseResult::InvalidStream;

    BitReader reader(sliceRbsp.span());
    reader.consumeBits(hevcNaluHeaderSize * 8);

    // first_slice_segment_in_pic_flag: u(1)
    bool firstSliceSegmentInPicFlag = reader.readFlag();
    bool irapPic = naluType >= static_cast<uint8_t>(HEVCNaluType::BlaWLp) && naluType <= static_cast<uint8_t>(HEVCNaluType::RsvIrapVcl23);
    if (irapPic) {
        // no_output_of_prior_pics_flag: u(1)
        reader.consumeBits(1);
    }
    // slice_pic_parameter_set_id: ue(v)
    uint32_t ppsId = reader.readExpGolomb();
    if (!reader.ok() || ppsId > 63)
        return ParseResult::InvalidStream;
    const PpsState* ppsState = pps(static_cast<uint16_t>(ppsId));
    if (!ppsState)
        return ParseResult::InvalidStream;
    const SpsState* spsState = sps(ppsState->spsId);
    if (!spsState)
        return ParseResult::InvalidStream;

    bool dependentSliceSegmentFlag = false;
    if (!firstSliceSegmentInPicFlag) {
        if (ppsState->dependentSliceSegmentsEnabledFlag) {
            // dependent_slice_segment_flag: u(1)
            dependentSliceSegmentFlag = reader.readFlag();
        }

        // slice_segment_address: u(v)
        uint32_t log2CtbSizeY = spsState->log2MinLumaCodingBlockSizeMinus3 + 3 + spsState->log2DiffMaxMinLumaCodingBlockSize;
        uint32_t ctbSizeY = 1u << log2CtbSizeY;
        uint32_t picWidthInCtbsY = spsState->picWidthInLumaSamples / ctbSizeY;
        if (spsState->picWidthInLumaSamples % ctbSizeY)
            ++picWidthInCtbsY;
        uint32_t picHeightInCtbsY = spsState->picHeightInLumaSamples / ctbSizeY;
        if (spsState->picHeightInLumaSamples % ctbSizeY)
            ++picHeightInCtbsY;

        uint32_t sliceSegmentAddressBits = hevcLog2Ceiling(picHeightInCtbsY * picWidthInCtbsY);
        if (!reader.ok() || sliceSegmentAddressBits == std::numeric_limits<uint32_t>::max())
            return ParseResult::InvalidStream;
        reader.consumeBits(sliceSegmentAddressBits);
    }

    uint32_t sliceType = static_cast<uint32_t>(HEVCSliceType::I);
    bool shortTermRefPicSetSpsFlag = false;
    uint32_t shortTermRefPicSetIndex = 0;
    ShortTermRefPicSet shortTermRefPicSet;
    uint32_t numRefIdxL0ActiveMinus1 = ppsState->numRefIdxL0DefaultActiveMinus1;
    uint32_t numRefIdxL1ActiveMinus1 = ppsState->numRefIdxL1DefaultActiveMinus1;

    if (!dependentSliceSegmentFlag) {
        for (uint32_t i = 0; i < ppsState->numExtraSliceHeaderBits; ++i) {
            // slice_reserved_flag: u(1)
            reader.consumeBits(1);
        }
        // slice_type: ue(v)
        sliceType = reader.readExpGolomb();
        if (!reader.ok() || sliceType > 2)
            return ParseResult::InvalidStream;
        if (ppsState->outputFlagPresentFlag) {
            // pic_output_flag: u(1)
            reader.consumeBits(1);
        }
        if (spsState->separateColourPlaneFlag) {
            // colour_plane_id: u(2)
            reader.consumeBits(2);
        }

        uint32_t numLongTermSps = 0;
        uint32_t numLongTermPics = 0;
        constexpr uint32_t maxShortTermRefPicSets = 64;
        std::array<bool, maxShortTermRefPicSets> usedByCurrPicLtFlag { };
        bool sliceTemporalMvpEnabledFlag = false;

        if (naluType != static_cast<uint8_t>(HEVCNaluType::IdrWRadl) && naluType != static_cast<uint8_t>(HEVCNaluType::IdrNLp)) {
            // slice_pic_order_cnt_lsb: u(v)
            reader.consumeBits(spsState->log2MaxPicOrderCntLsbMinus4 + 4);
            // short_term_ref_pic_set_sps_flag: u(1)
            shortTermRefPicSetSpsFlag = reader.readFlag();
            if (!shortTermRefPicSetSpsFlag) {
                auto refPicSet = parseShortTermRefPicSet(spsState->numShortTermRefPicSets, spsState->numShortTermRefPicSets, spsState->shortTermRefPicSet, spsState->spsMaxDecPicBufferingMinus1[spsState->spsMaxSubLayersMinus1], reader);
                if (!refPicSet)
                    return ParseResult::InvalidStream;
                shortTermRefPicSet = *refPicSet;
            } else if (spsState->numShortTermRefPicSets > 1) {
                // short_term_ref_pic_set_idx: u(v)
                uint32_t shortTermRefPicSetIndexBits = hevcLog2Ceiling(spsState->numShortTermRefPicSets);
                if ((1u << shortTermRefPicSetIndexBits) < spsState->numShortTermRefPicSets)
                    ++shortTermRefPicSetIndexBits;
                if (shortTermRefPicSetIndexBits > 0) {
                    shortTermRefPicSetIndex = reader.readBits(shortTermRefPicSetIndexBits);
                    if (!reader.ok() || shortTermRefPicSetIndex > spsState->numShortTermRefPicSets - 1)
                        return ParseResult::InvalidStream;
                }
            }
            if (spsState->longTermRefPicsPresentFlag) {
                constexpr uint32_t maxLongTermRefPicSets = 32;
                if (spsState->numLongTermRefPicsSps > 0) {
                    // num_long_term_sps: ue(v)
                    numLongTermSps = reader.readExpGolomb();
                    if (!reader.ok() || numLongTermSps > spsState->numLongTermRefPicsSps)
                        return ParseResult::InvalidStream;
                }
                // num_long_term_pics: ue(v)
                numLongTermPics = reader.readExpGolomb();
                if (!reader.ok() || numLongTermPics > maxLongTermRefPicSets - numLongTermSps)
                    return ParseResult::InvalidStream;
                for (uint32_t i = 0; i < numLongTermSps + numLongTermPics; ++i) {
                    if (i < numLongTermSps) {
                        uint32_t ltIdxSps = 0;
                        if (spsState->numLongTermRefPicsSps > 1) {
                            // lt_idx_sps: u(v)
                            uint32_t ltIdxSpsBits = hevcLog2Ceiling(spsState->numLongTermRefPicsSps);
                            ltIdxSps = reader.readBits(ltIdxSpsBits);
                            if (!reader.ok() || ltIdxSps > spsState->numLongTermRefPicsSps - 1)
                                return ParseResult::InvalidStream;
                        }
                        if (ltIdxSps < spsState->usedByCurrPicLtSpsFlag.size() && i < usedByCurrPicLtFlag.size())
                            usedByCurrPicLtFlag[i] = spsState->usedByCurrPicLtSpsFlag[ltIdxSps];
                    } else {
                        // poc_lsb_lt: u(v)
                        reader.consumeBits(spsState->log2MaxPicOrderCntLsbMinus4 + 4);
                        // used_by_curr_pic_lt_flag: u(1)
                        bool usedByCurrPic = reader.readFlag();
                        if (i < usedByCurrPicLtFlag.size())
                            usedByCurrPicLtFlag[i] = usedByCurrPic;
                    }
                    // delta_poc_msb_present_flag: u(1)
                    if (reader.readFlag()) {
                        // delta_poc_msb_cycle_lt: ue(v) -- unused.
                        reader.readExpGolomb();
                    }
                }
            }
            if (spsState->spsTemporalMvpEnabledFlag) {
                // slice_temporal_mvp_enabled_flag: u(1)
                sliceTemporalMvpEnabledFlag = reader.readFlag();
            }
        }

        if (spsState->sampleAdaptiveOffsetEnabledFlag) {
            // slice_sao_luma_flag: u(1)
            reader.consumeBits(1);
            uint32_t chromaArrayType = spsState->separateColourPlaneFlag ? 0 : spsState->chromaFormatIDC;
            if (chromaArrayType) {
                // slice_sao_chroma_flag: u(1)
                reader.consumeBits(1);
            }
        }

        if (sliceType == static_cast<uint32_t>(HEVCSliceType::P) || sliceType == static_cast<uint32_t>(HEVCSliceType::B)) {
            constexpr uint32_t maxRefIdxActive = 15;
            // num_ref_idx_active_override_flag: u(1)
            if (reader.readFlag()) {
                // num_ref_idx_l0_active_minus1: ue(v)
                numRefIdxL0ActiveMinus1 = reader.readExpGolomb();
                if (!reader.ok() || numRefIdxL0ActiveMinus1 > maxRefIdxActive - 1)
                    return ParseResult::InvalidStream;
                if (sliceType == static_cast<uint32_t>(HEVCSliceType::B)) {
                    // num_ref_idx_l1_active_minus1: ue(v)
                    numRefIdxL1ActiveMinus1 = reader.readExpGolomb();
                    if (!reader.ok() || numRefIdxL1ActiveMinus1 > maxRefIdxActive - 1)
                        return ParseResult::InvalidStream;
                }
            }

            uint32_t currRpsIndex = shortTermRefPicSetSpsFlag ? shortTermRefPicSetIndex : spsState->numShortTermRefPicSets;
            const ShortTermRefPicSet* refPicSet = currRpsIndex < spsState->shortTermRefPicSet.size() ? &spsState->shortTermRefPicSet[currRpsIndex] : &shortTermRefPicSet;
            if (!reader.ok() || refPicSet->numNegativePics > maxShortTermRefPicSets || refPicSet->numPositivePics > maxShortTermRefPicSets)
                return ParseResult::InvalidStream;

            uint32_t numPicTotalCurr = 0;
            for (uint32_t i = 0; i < refPicSet->numNegativePics; ++i) {
                if (refPicSet->usedByCurrPicS0[i])
                    ++numPicTotalCurr;
            }
            for (uint32_t i = 0; i < refPicSet->numPositivePics; ++i) {
                if (refPicSet->usedByCurrPicS1[i])
                    ++numPicTotalCurr;
            }
            for (uint32_t i = 0; i < numLongTermSps + numLongTermPics && i < usedByCurrPicLtFlag.size(); ++i) {
                if (usedByCurrPicLtFlag[i])
                    ++numPicTotalCurr;
            }

            if (ppsState->listsModificationPresentFlag && numPicTotalCurr > 1) {
                // ref_pic_lists_modification()
                uint32_t listEntryBits = hevcLog2Ceiling(numPicTotalCurr);
                if ((1u << listEntryBits) < numPicTotalCurr)
                    ++listEntryBits;
                // ref_pic_list_modification_flag_l0: u(1)
                if (reader.readFlag()) {
                    for (uint32_t i = 0; i < numRefIdxL0ActiveMinus1; ++i) {
                        // list_entry_l0: u(v)
                        reader.consumeBits(listEntryBits);
                    }
                }
                if (sliceType == static_cast<uint32_t>(HEVCSliceType::B)) {
                    // ref_pic_list_modification_flag_l1: u(1)
                    if (reader.readFlag()) {
                        for (uint32_t i = 0; i < numRefIdxL1ActiveMinus1; ++i) {
                            // list_entry_l1: u(v)
                            reader.consumeBits(listEntryBits);
                        }
                    }
                }
            }
            if (sliceType == static_cast<uint32_t>(HEVCSliceType::B)) {
                // mvd_l1_zero_flag: u(1)
                reader.consumeBits(1);
            }
            if (ppsState->cabacInitPresentFlag) {
                // cabac_init_flag: u(1)
                reader.consumeBits(1);
            }
            if (sliceTemporalMvpEnabledFlag) {
                bool collocatedFromL0Flag = false;
                if (sliceType == static_cast<uint32_t>(HEVCSliceType::B)) {
                    // collocated_from_l0_flag: u(1)
                    collocatedFromL0Flag = reader.readFlag();
                }
                if ((collocatedFromL0Flag && numRefIdxL0ActiveMinus1 > 0) || (!collocatedFromL0Flag && numRefIdxL1ActiveMinus1 > 0)) {
                    // collocated_ref_idx: ue(v)
                    uint32_t collocatedRefIdx = reader.readExpGolomb();
                    if (collocatedFromL0Flag) {
                        if (!reader.ok() || collocatedRefIdx > numRefIdxL0ActiveMinus1)
                            return ParseResult::InvalidStream;
                    } else if (!reader.ok() || collocatedRefIdx > numRefIdxL1ActiveMinus1)
                        return ParseResult::InvalidStream;
                }
            }

            // pred_weight_table()
            if ((ppsState->weightedPredFlag && sliceType == static_cast<uint32_t>(HEVCSliceType::P)) || (ppsState->weightedBipredFlag && sliceType == static_cast<uint32_t>(HEVCSliceType::B))) {
                uint32_t chromaArrayType = spsState->separateColourPlaneFlag ? 0 : spsState->chromaFormatIDC;
                if (!parsePredWeightTable(reader, chromaArrayType, numRefIdxL0ActiveMinus1, sliceType == static_cast<uint32_t>(HEVCSliceType::B), numRefIdxL1ActiveMinus1))
                    return ParseResult::InvalidStream;
            }

            // five_minus_max_num_merge_cand: ue(v)
            uint32_t fiveMinusMaxNumMergeCand = reader.readExpGolomb();
            if (!reader.ok() || fiveMinusMaxNumMergeCand > 4)
                return ParseResult::InvalidStream;
        }
    }

    // slice_qp_delta: se(v)
    int32_t lastSliceQPDelta = reader.readSignedExpGolomb();
    constexpr int maxAbsQPDeltaValue = 51;
    if (!reader.ok() || std::abs(lastSliceQPDelta) > maxAbsQPDeltaValue)
        return ParseResult::InvalidStream;
    // Equation 7-54 in the H.265 spec.
    int32_t qp = 26 + ppsState->initQPMinus26 + lastSliceQPDelta;
    if (qp < -ppsState->qpBdOffsetY || qp > 51)
        return ParseResult::InvalidStream;

    m_lastSliceQPDelta = lastSliceQPDelta;
    m_lastSlicePpsId = static_cast<uint16_t>(ppsId);

    return ParseResult::Ok;
}

void HEVCBitstreamParser::parseSlice(std::span<const uint8_t> slice)
{
    if (slice.empty())
        return;

    auto naluType = hevcNaluType(slice[0]);
    switch (naluType) {
    case HEVCNaluType::Sps:
        if (slice.size() >= hevcNaluHeaderSize) {
            if (auto spsState = parseSps(slice.subspan(hevcNaluHeaderSize)))
                m_sps.set(spsState->spsId, WTF::move(*spsState));
        }
        break;
    case HEVCNaluType::Pps:
        if (slice.size() >= hevcNaluHeaderSize) {
            auto rbsp = parseRbsp(slice.subspan(hevcNaluHeaderSize));
            BitReader idReader(rbsp.span());
            uint32_t ppsId = idReader.readExpGolomb();
            uint32_t spsId = idReader.readExpGolomb();
            if (idReader.ok() && ppsId <= 63 && spsId <= 15) {
                if (auto ppsState = parsePps(slice.subspan(hevcNaluHeaderSize), sps(static_cast<uint16_t>(spsId))))
                    m_pps.set(ppsState->ppsId, WTF::move(*ppsState));
            }
        }
        break;
    case HEVCNaluType::Vps:
    case HEVCNaluType::Aud:
    case HEVCNaluType::PrefixSei:
    case HEVCNaluType::SuffixSei:
    case HEVCNaluType::Ap:
    case HEVCNaluType::Fu:
        break;
    default:
        parseNonParameterSetNalu(slice, static_cast<uint8_t>(naluType));
        break;
    }
}

const HEVCBitstreamParser::PpsState* HEVCBitstreamParser::pps(uint16_t id) const
{
    auto it = m_pps.find(id);
    return it == m_pps.end() ? nullptr : &it->value;
}

const HEVCBitstreamParser::SpsState* HEVCBitstreamParser::sps(uint16_t id) const
{
    auto it = m_sps.find(id);
    return it == m_sps.end() ? nullptr : &it->value;
}

void HEVCBitstreamParser::parseBitstream(std::span<const uint8_t> bitstream)
{
    for (auto& index : findNaluIndices(bitstream))
        parseSlice(bitstream.subspan(index.payloadStartOffset, index.payloadSize));
}

std::optional<int> HEVCBitstreamParser::lastSliceQP() const
{
    if (!m_lastSliceQPDelta || !m_lastSlicePpsId) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to get qp - missing info");
        return std::nullopt;
    }
    const PpsState* ppsState = pps(*m_lastSlicePpsId);
    if (!ppsState) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to get qp - missing pps");
        return std::nullopt;
    }
    int parsedQP = 26 + ppsState->initQPMinus26 + *m_lastSliceQPDelta;
    constexpr int minQpValue = 0;
    constexpr int maxQpValue = 51;
    if (parsedQP < minQpValue || parsedQP > maxQpValue) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to get qp - invalid parsed qp");
        return std::nullopt;
    }
    return parsedQP;
}

}
