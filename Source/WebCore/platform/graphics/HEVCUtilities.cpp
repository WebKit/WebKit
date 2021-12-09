/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include <wtf/NeverDestroyed.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

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
    if (codecName != "hvc1" && codecName != "hev1")
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
    parameters.generalProfileCompatibilityFlags = *compatibilityFlags;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Third element: General Tier Flag ['L', 'H'], mapping to [false, true], followed by
    // General Level IDC as a 8-bit decimal number.
    auto generalTier = *nextElement;
    firstCharacter = generalTier[0];
    if (firstCharacter != 'L' && firstCharacter != 'H')
        return std::nullopt;

    auto generalLevelIDC = parseInteger<uint8_t>(generalTier.substring(1));
    if (!generalLevelIDC)
        return std::nullopt;
    parameters.generalLevelIDC = *generalLevelIDC;

    // Optional fourth and remaining elements: a sequence of 6 1-byte constraint flags, each byte encoded
    // in hex, and separated by a period, with trailing zero bytes omitted.
    for (unsigned i = 0; i < 6; ++i) {
        if (++nextElement == codecSplit.end())
            break;
        if (!parseInteger<uint8_t>(*nextElement, 16))
            return std::nullopt;
    }

    return parameters;
}

template<typename ValueType> static inline std::optional<ValueType> makeOptionalFromPointer(const ValueType* pointer)
{
    if (!pointer)
        return std::nullopt;
    return *pointer;
}

static std::optional<DoViParameters::Codec> parseDoViCodecType(StringView string)
{
    static constexpr std::pair<PackedASCIILowerCodes<uint32_t>, DoViParameters::Codec> typesArray[] = {
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
    static constexpr std::pair<PackedASCIILowerCodes<uint64_t>, uint16_t> profilesArray[] = {
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

}
