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

#include "FourCC.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/DataView.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SortedArrayMap.h>
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
    parameters.profileIDC = (*profileFlagsAndLevel & 0xF00) >> 16;
    parameters.constraintsFlags = (*profileFlagsAndLevel & 0xF0) >> 8;
    parameters.levelIDC = *profileFlagsAndLevel & 0xF;

    return parameters;
}

String createAVCCodecParametersString(const AVCParameters& parameters)
{
    // The format of the 'avc1' codec string is specified in ISO/IEC 14496-15:2014, Annex E.2.
    return makeString("avc1."
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
    auto view = JSC::DataView::create(WTFMove(arrayBuffer), 0, buffer.size());

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
    resultBuilder.append(parameters.codec == HEVCParameters::Codec::Hev1 ? "hev1" : "hvc1", '.');
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
    if (codecCode == "hev1")
        parameters.codec = HEVCParameters::Codec::Hev1;
    else if (codecCode == "hvc1")
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
    auto view = JSC::DataView::create(WTFMove(arrayBuffer), 0, buffer.size());
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
    static constexpr std::pair<PackedLettersLiteral<uint32_t>, DoViParameters::Codec> typesArray[] = {
        { "dva1", DoViParameters::Codec::AVC1 },
        { "dvav", DoViParameters::Codec::AVC3 },
        { "dvh1", DoViParameters::Codec::HVC1 },
        { "dvhe", DoViParameters::Codec::HEV1 },
    };
    static constexpr SortedArrayMap typesMap { typesArray };
    return makeOptionalFromPointer(typesMap.tryGet(string));
}

static std::optional<uint16_t> profileIDForAlphabeticDoViProfile(StringView profile)
{
    // See Table 7 of "Dolby Vision Profiles and Levels Version 1.3.2"
    static constexpr std::pair<PackedLettersLiteral<uint64_t>, uint16_t> profilesArray[] = {
        { "dvav.se", 9 },
        { "dvhe.dtb", 7 },
        { "dvhe.dtr", 4 },
        { "dvhe.st", 8 },
        { "dvhe.stn", 5 },
    };
    static constexpr SortedArrayMap profilesMap { profilesArray };
    return makeOptionalFromPointer(profilesMap.tryGet(profile));
}

static bool isValidDoViProfileID(uint16_t profileID)
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

static std::optional<uint16_t> maximumLevelIDForDoViProfileID(uint16_t profileID)
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

static bool isValidProfileIDForCodec(uint16_t profileID, DoViParameters::Codec codec)
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
    auto view = JSC::DataView::create(WTFMove(arrayBuffer), 0, buffer.size());

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
    builder.append("dvh1.");
    if (parameters.bitstreamProfileID < 10)
        builder.append('0');
    builder.append(parameters.bitstreamProfileID);
    builder.append('.');
    if (parameters.bitstreamLevelID < 10)
        builder.append('0');
    builder.append(parameters.bitstreamLevelID);
    return builder.toString();
}

}
