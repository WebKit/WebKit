/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "AV1Utilities.h"

#include "BitReader.h"
#include "PlatformMediaCapabilitiesVideoConfiguration.h"
#include "TrackInfo.h"
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WTF {

template<> bool isValidEnum<WebCore::AV1ConfigurationProfile>(std::underlying_type_t<WebCore::AV1ConfigurationProfile> value)
{
    switch (value) {
    case enumToUnderlyingType(WebCore::AV1ConfigurationProfile::Main):
    case enumToUnderlyingType(WebCore::AV1ConfigurationProfile::High):
    case enumToUnderlyingType(WebCore::AV1ConfigurationProfile::Professional):
        return true;
    default:
        return false;
    }
}

template<> bool isValidEnum<WebCore::AV1ConfigurationLevel>(std::underlying_type_t<WebCore::AV1ConfigurationLevel> value)
{
    switch (value) {
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_2_0):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_2_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_2_2):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_2_3):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_3_0):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_3_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_3_2):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_3_3):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_4_0):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_4_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_4_2):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_4_3):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_5_0):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_5_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_5_2):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_5_3):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_6_0):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_6_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_6_2):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_6_3):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_7_0):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_7_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_7_2):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_7_3):
    case enumToUnderlyingType(WebCore::AV1ConfigurationLevel::Level_Maximum):
        return true;
    default:
        return false;
    }
}

template<> bool isValidEnum<WebCore::AV1ConfigurationChromaSubsampling>(std::underlying_type_t<WebCore::AV1ConfigurationChromaSubsampling> value)
{
    switch (value) {
    case enumToUnderlyingType(WebCore::AV1ConfigurationChromaSubsampling::Subsampling_444):
    case enumToUnderlyingType(WebCore::AV1ConfigurationChromaSubsampling::Subsampling_422):
    case enumToUnderlyingType(WebCore::AV1ConfigurationChromaSubsampling::Subsampling_420_Unknown):
    case enumToUnderlyingType(WebCore::AV1ConfigurationChromaSubsampling::Subsampling_420_Vertical):
    case enumToUnderlyingType(WebCore::AV1ConfigurationChromaSubsampling::Subsampling_420_Colocated):
        return true;
    default:
        return false;
    }
}

template<> bool isValidEnum<WebCore::AV1ConfigurationRange>(std::underlying_type_t<WebCore::AV1ConfigurationRange> value)
{
    switch (value) {
    case enumToUnderlyingType(WebCore::AV1ConfigurationRange::VideoRange):
    case enumToUnderlyingType(WebCore::AV1ConfigurationRange::FullRange):
        return true;
    default:
        return false;
    }
}

template<> bool isValidEnum<WebCore::AV1ConfigurationColorPrimaries>(std::underlying_type_t<WebCore::AV1ConfigurationColorPrimaries> value)
{
    switch (value) {
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::BT_709_6):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::Unspecified):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::BT_470_6_M):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::BT_470_7_BG):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::BT_601_7):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::SMPTE_ST_240):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::Film):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::SMPTE_ST_428_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::SMPTE_RP_431_2):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::SMPTE_EG_432_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationColorPrimaries::EBU_Tech_3213_E):
        return true;
    default:
        return false;
    }
}

template<> bool isValidEnum<WebCore::AV1ConfigurationTransferCharacteristics>(std::underlying_type_t<WebCore::AV1ConfigurationTransferCharacteristics> value)
{
    switch (value) {
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_709_6):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::Unspecified):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_470_6_M):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_470_7_BG):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_601_7):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::SMPTE_ST_240):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::Linear):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::Logrithmic):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::Logrithmic_Sqrt):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::IEC_61966_2_4):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_1361_0):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::IEC_61966_2_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_2020_10bit):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_2020_12bit):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::SMPTE_ST_2084):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::SMPTE_ST_428_1):
    case enumToUnderlyingType(WebCore::AV1ConfigurationTransferCharacteristics::BT_2100_HLG):
        return true;
    default:
        return false;
    }
};

template<> bool isValidEnum<WebCore::AV1ConfigurationMatrixCoefficients>(std::underlying_type_t<WebCore::AV1ConfigurationMatrixCoefficients> value)
{
    switch (value) {
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::Identity):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::BT_709_6):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::Unspecified):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::FCC):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::BT_470_7_BG):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::BT_601_7):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::SMPTE_ST_240):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::YCgCo):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::BT_2020_Constant_Luminance):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::SMPTE_ST_2085):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::Chromacity_Nonconstant_Luminance):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::Chromacity_Constant_Luminance):
    case enumToUnderlyingType(WebCore::AV1ConfigurationMatrixCoefficients::BT_2100_ICC):
        return true;
    default:
        return false;
    }
}

} // namespace WTF

namespace WebCore {

std::optional<AV1CodecConfigurationRecord> parseAV1CodecParameters(StringView codecView)
{
    // Ref: https://aomediacodec.github.io/av1-isobmff/#codecsparam
    // Section 5: Codecs Parameter String
    auto codecSplit = codecView.split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return std::nullopt;

    AV1CodecConfigurationRecord configuration;

    configuration.codecName = (*nextElement).toString();

    // The codecs parameter string for the AOM AV1 codec is as follows:
    // <sample entry 4CC>.<profile>.<level><tier>.<bitDepth>.<monochrome>.<chromaSubsampling>.
    // <colorPrimaries>.<transferCharacteristics>.<matrixCoefficients>.<videoFullRangeFlag>
    //
    // All fields following the sample entry 4CC are expressed as double digit decimals,
    // unless indicated otherwise. Leading or trailing zeros cannot be omitted.
    //
    // The parameters sample entry 4CC, profile, level, tier, and bitDepth are all mandatory
    // fields. If any of these fields are empty, or not within their allowed range, the processing
    // device SHOULD treat it as an error.
    if (configuration.codecName != "av01"_s)
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // The profile parameter value, represented by a single digit decimal, SHALL
    // equal the value of seq_profile in the Sequence Header OBU.
    auto profile = parseEnumFromStringView<AV1ConfigurationProfile>(*nextElement);
    if (!profile)
        return std::nullopt;
    configuration.profile = *profile;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // The level parameter value SHALL equal the first level value indicated by
    // seq_level_idx in the Sequence Header OBU.
    auto levelTierView = *nextElement;
    auto levelTierLength = levelTierView.length();
    if (levelTierLength < 3)
        return std::nullopt;

    auto levelView = levelTierView.substring(0, levelTierLength - 1);
    auto tierView = levelTierView.substring(levelTierLength - 1, 1);

    auto level = parseEnumFromStringView<AV1ConfigurationLevel>(levelView);
    if (!level)
        return std::nullopt;
    configuration.level = *level;

    // The tier parameter value SHALL be equal to M when the first seq_tier
    // value in the Sequence Header OBU is equal to 0, and H when it is equal to 1.
    auto tierCharacter = tierView.characterAt(0);
    if (tierCharacter == 'M')
        configuration.tier = AV1ConfigurationTier::Main;
    else if (tierCharacter == 'H')
        configuration.tier = AV1ConfigurationTier::High;
    else
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // The bitDepth parameter value SHALL equal the value of BitDepth variable as
    // defined in [AV1] derived from the Sequence Header OBU.
    auto bitDepth = parseInteger<uint8_t>(*nextElement);
    if (!bitDepth || *bitDepth > 12)
        return std::nullopt;
    configuration.bitDepth = *bitDepth;

    // All the other fields (including their leading '.') are optional, mutually inclusive (all
    // or none) fields. If not specified then the values listed in the table below are assumed.
    if (++nextElement == codecSplit.end()) {
        configuration.monochrome = 0;
        configuration.chromaSubsampling = static_cast<uint8_t>(AV1ConfigurationChromaSubsampling::Subsampling_420_Unknown);
        configuration.colorPrimaries = static_cast<uint8_t>(AV1ConfigurationColorPrimaries::BT_709_6);
        configuration.transferCharacteristics = static_cast<uint8_t>(AV1ConfigurationTransferCharacteristics::BT_709_6);
        configuration.matrixCoefficients = static_cast<uint8_t>(AV1ConfigurationMatrixCoefficients::BT_709_6);
        configuration.videoFullRangeFlag = AV1ConfigurationRange::VideoRange;
        return configuration;
    }

    // The monochrome parameter value, represented by a single digit decimal, SHALL
    // equal the value of mono_chrome in the Sequence Header OBU.
    auto monochrome = parseInteger<uint8_t>(*nextElement);
    if (!monochrome || *monochrome > 1)
        return std::nullopt;
    configuration.monochrome = *monochrome;

    if (++nextElement == codecSplit.end())
        return configuration;

    // The chromaSubsampling parameter value, represented by a three-digit decimal,
    // SHALL have its first digit equal to subsampling_x and its second digit equal
    // to subsampling_y. If both subsampling_x and subsampling_y are set to 1, then
    // the third digit SHALL be equal to chroma_sample_position, otherwise it SHALL
    // be set to 0.
    auto chromaSubsampling = parseEnumFromStringView<AV1ConfigurationChromaSubsampling>(*nextElement);
    if (!chromaSubsampling)
        return std::nullopt;
    configuration.chromaSubsampling = static_cast<uint8_t>(*chromaSubsampling);

    if (++nextElement == codecSplit.end())
        return configuration;

    // The colorPrimaries, transferCharacteristics, matrixCoefficients, and videoFullRangeFlag
    // parameter values SHALL equal the value of matching fields in the Sequence Header OBU, if
    // color_description_present_flag is set to 1, otherwise they SHOULD not be set, defaulting
    // to the values below.
    auto colorPrimaries = parseEnumFromStringView<AV1ConfigurationColorPrimaries>(*nextElement);
    if (!colorPrimaries)
        return std::nullopt;
    configuration.colorPrimaries = static_cast<uint8_t>(*colorPrimaries);

    if (++nextElement == codecSplit.end())
        return configuration;

    auto transferCharacteristics = parseEnumFromStringView<AV1ConfigurationTransferCharacteristics>(*nextElement);
    if (!transferCharacteristics)
        return std::nullopt;
    configuration.transferCharacteristics = static_cast<uint8_t>(*transferCharacteristics);

    if (++nextElement == codecSplit.end())
        return configuration;

    auto matrixCoefficients = parseEnumFromStringView<AV1ConfigurationMatrixCoefficients>(*nextElement);
    if (!matrixCoefficients)
        return std::nullopt;
    configuration.matrixCoefficients = static_cast<uint8_t>(*matrixCoefficients);

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Eighth element: videoFullRangeFlag. Legal values are 0 and 1.
    auto videoFullRangeFlag = parseEnumFromStringView<AV1ConfigurationRange>(*nextElement);
    if (!videoFullRangeFlag)
        return std::nullopt;
    configuration.videoFullRangeFlag = *videoFullRangeFlag;

    if (++nextElement != codecSplit.end())
        return std::nullopt;

    return configuration;
}

String createAV1CodecParametersString(const AV1CodecConfigurationRecord& configuration)
{
    // Ref: https://aomediacodec.github.io/av1-isobmff/#codecsparam
    // Section 5: Codecs Parameter String

    // The codecs parameter string for the AOM AV1 codec is as follows:
    // <sample entry 4CC>.<profile>.<level><tier>.<bitDepth>.<monochrome>.<chromaSubsampling>.
    // <colorPrimaries>.<transferCharacteristics>.<matrixCoefficients>.<videoFullRangeFlag>
    //
    // All fields following the sample entry 4CC are expressed as double digit decimals,
    // unless indicated otherwise. Leading or trailing zeros cannot be omitted.
    //
    // The parameters sample entry 4CC, profile, level, tier, and bitDepth are all mandatory
    // fields.

    StringBuilder builder;
    builder.append("av01"_s);

    auto appendOneDigit = [&](uint8_t number) {
        builder.append(static_cast<Latin1Character>('0' + number % 10));
    };

    auto appendTwoDigits = [&](uint8_t number) {
        builder.append(static_cast<Latin1Character>('0' + number / 10 % 10));
        builder.append(static_cast<Latin1Character>('0' + number % 10));
    };

    auto appendThreeDigits = [&](uint8_t number) {
        builder.append(static_cast<Latin1Character>('0' + number / 100 % 10));
        builder.append(static_cast<Latin1Character>('0' + number / 10 % 10));
        builder.append(static_cast<Latin1Character>('0' + number % 10));
    };

    // The parameters sample entry 4CC, profile, level, tier, and bitDepth are
    // all mandatory fields.

    // The profile parameter value, represented by a single digit decimal, SHALL
    // equal the value of seq_profile in the Sequence Header OBU.
    builder.append('.');
    appendOneDigit(static_cast<uint8_t>(configuration.profile));

    // The level parameter value SHALL equal the first level value indicated by
    // seq_level_idx in the Sequence Header OBU.
    builder.append('.');
    appendTwoDigits(static_cast<uint8_t>(configuration.level));

    // The tier parameter value SHALL be equal to M when the first seq_tier value
    // in the Sequence Header OBU is equal to 0, and H when it is equal to 1.
    builder.append(configuration.tier == AV1ConfigurationTier::Main ? 'M' : 'H');

    // The bitDepth parameter value SHALL equal the value of BitDepth variable as
    // defined in [AV1] derived from the Sequence Header OBU.
    builder.append('.');
    appendTwoDigits(static_cast<uint8_t>(configuration.bitDepth));

    // All the other fields (including their leading '.') are optional, mutually inclusive
    // (all or none) fields. If not specified then the values listed in the table below are
    // assumed.
    // NOTE: if the default values for the remaining parameters are provided, just return
    // the short-form version of the codec string.
    if (configuration.monochrome == AV1CodecConfigurationRecord::defaultMonochrome
        && configuration.chromaSubsampling == AV1CodecConfigurationRecord::defaultChromaSubsampling
        && configuration.colorPrimaries == AV1CodecConfigurationRecord::defaultColorPrimaries
        && configuration.transferCharacteristics == AV1CodecConfigurationRecord::defaultTransferCharacteristics
        && configuration.matrixCoefficients == AV1CodecConfigurationRecord::defaultMatrixCoefficients
        && configuration.videoFullRangeFlag == AV1CodecConfigurationRecord::defaultVideoFullRangeFlag)
        return builder.toString();

    // The monochrome parameter value, represented by a single digit decimal, SHALL
    // equal the value of mono_chrome in the Sequence Header OBU.
    builder.append('.');
    appendOneDigit(configuration.monochrome);

    // The chromaSubsampling parameter value, represented by a three-digit decimal,
    // SHALL have its first digit equal to subsampling_x and its second digit equal
    // to subsampling_y. If both subsampling_x and subsampling_y are set to 1, then
    // the third digit SHALL be equal to chroma_sample_position, otherwise it SHALL
    // be set to 0.
    builder.append('.');
    appendThreeDigits(configuration.chromaSubsampling);

    // The colorPrimaries, transferCharacteristics, matrixCoefficients, and
    // videoFullRangeFlag parameter values SHALL equal the value of matching fields
    // in the Sequence Header OBU, if color_description_present_flag is set to 1, otherwise
    // they SHOULD not be set, defaulting to the values below.
    builder.append('.');
    appendTwoDigits(configuration.colorPrimaries);
    builder.append('.');
    appendTwoDigits(configuration.transferCharacteristics);
    builder.append('.');
    appendTwoDigits(configuration.matrixCoefficients);

    // The videoFullRangeFlag is represented by a single digit.
    builder.append('.');
    appendOneDigit(static_cast<uint8_t>(configuration.videoFullRangeFlag));

    return builder.toString();
}

struct AV1PerLevelConstraints {
    uint32_t maxPicSize;
    uint32_t maxWidth;
    uint32_t maxHeight;
    double maxFramerate;
    uint32_t mainMaxBitrate;
    uint32_t highMaxBitrate;
};

// Derived from "AV1 Bitstream & Decoding Process Specification", Version 1.0.0 with Errata 1
// Annex A: Profiles and levels
using AV1PerLevelConstraintsMap = HashMap<AV1ConfigurationLevel, AV1PerLevelConstraints, WTF::IntHash<AV1ConfigurationLevel>, WTF::StrongEnumHashTraits<AV1ConfigurationLevel>>;
static const AV1PerLevelConstraintsMap& perLevelConstraints()
{
    static NeverDestroyed<AV1PerLevelConstraintsMap> perLevelConstraints = AV1PerLevelConstraintsMap {
        { AV1ConfigurationLevel::Level_2_0, { 147456,   2048,  1152, 30,  1572864,   0 } },
        { AV1ConfigurationLevel::Level_2_1, { 278784,   2816,  1584, 30,  3145728,   0 } },
        { AV1ConfigurationLevel::Level_3_0, { 665856,   4352,  2448, 30,  6291456,   0 } },
        { AV1ConfigurationLevel::Level_3_1, { 1065024,  5504,  3096, 30,  10485760,  0 } },
        { AV1ConfigurationLevel::Level_4_0, { 2359296,  6144,  3456, 30,  12582912,  31457280 } },
        { AV1ConfigurationLevel::Level_4_1, { 2359296,  6144,  3456, 60,  20971520,  52428800 } },
        { AV1ConfigurationLevel::Level_5_0, { 8912896,  8192,  4352, 30,  31457280,  104857600 } },
        { AV1ConfigurationLevel::Level_5_1, { 8912896,  8192,  4352, 60,  41943040,  167772160 } },
        { AV1ConfigurationLevel::Level_5_2, { 8912896,  8192,  4352, 120, 62914560,  251658240 } },
        { AV1ConfigurationLevel::Level_5_3, { 8912896,  8192,  4352, 120, 62914560,  251658240 } },
        { AV1ConfigurationLevel::Level_6_0, { 35651584, 16384, 8704, 30,  62914560,  251658240 } },
        { AV1ConfigurationLevel::Level_6_1, { 35651584, 16384, 8704, 60,  104857600, 503316480 } },
        { AV1ConfigurationLevel::Level_6_2, { 35651584, 16384, 8704, 120, 167772160, 838860800 } },
        { AV1ConfigurationLevel::Level_6_3, { 35651584, 16384, 8704, 120, 167772160, 838860800 } },
    };
    return perLevelConstraints;
}

bool validateAV1ConfigurationRecord(const AV1CodecConfigurationRecord& record)
{
    // Ref: https://aomediacodec.github.io/av1-spec/av1-spec.pdf

    // 6.4.1.General sequence header OBU semantics

    if (!isValidEnum<AV1ConfigurationChromaSubsampling>(record.chromaSubsampling))
        return false;
    auto chromaSubsampling = static_cast<AV1ConfigurationChromaSubsampling>(record.chromaSubsampling);

    switch (record.profile) {
    case AV1ConfigurationProfile::Main:
        if (record.bitDepth != 8 && record.bitDepth != 10)
            return false;
        if (chromaSubsampling != AV1ConfigurationChromaSubsampling::Subsampling_420_Unknown
            && chromaSubsampling != AV1ConfigurationChromaSubsampling::Subsampling_420_Vertical
            && chromaSubsampling != AV1ConfigurationChromaSubsampling::Subsampling_420_Colocated)
            return false;
        break;
    case AV1ConfigurationProfile::High:
        if (record.bitDepth != 8 && record.bitDepth != 10)
            return false;
        if (record.monochrome)
            return false;
        if (chromaSubsampling != AV1ConfigurationChromaSubsampling::Subsampling_444)
            return false;
        break;
    case AV1ConfigurationProfile::Professional:
        if (record.bitDepth == 8 || record.bitDepth == 10) {
            if (chromaSubsampling != AV1ConfigurationChromaSubsampling::Subsampling_444)
                return false;
        } else if (record.bitDepth != 12)
            return false;
        break;
    }

    // 6.4.2. Color config semantics
    // When monochrome is set to 1, the only valid setting for subsampling_x and subsampling_y
    // is 1 and 1. Additionally, when monochrome is set to 1 in the color_config of the Sequence OBU,
    // the only valid setting for chroma_sample_position is CSP_UNKNOWN (0).
    if (record.monochrome && chromaSubsampling != AV1ConfigurationChromaSubsampling::Subsampling_420_Unknown)
        return false;

    return true;
}

bool validateAV1PerLevelConstraints(const AV1CodecConfigurationRecord& record, const PlatformMediaCapabilitiesVideoConfiguration& configuration)
{
    // Check that VideoConfiguration is within the specified profile and level from the configuration record:
    auto findIter = perLevelConstraints().find(record.level);
    if (findIter == perLevelConstraints().end())
        return false;

    auto& levelConstraints = findIter->value;
    auto maxBitrate = record.tier == AV1ConfigurationTier::Main ? levelConstraints.mainMaxBitrate : levelConstraints.highMaxBitrate;
    return configuration.width <= levelConstraints.maxWidth
        && configuration.height <= levelConstraints.maxHeight
        && configuration.width * configuration.height <= levelConstraints.maxPicSize
        && configuration.framerate <= levelConstraints.maxFramerate
        && configuration.bitrate <= maxBitrate;
}

std::optional<AV1CodecConfigurationRecord> parseAV1DecoderConfigurationRecord(std::span<const uint8_t> buffer)
{
    // Ref: https://aomediacodec.github.io/av1-isobmff/
    // Section 2.3: AV1 Codec Configuration Box

    // AV1CodecConfigurationRecord is at least 4 bytes long
    if (buffer.size() < 4)
        return std::nullopt;

    AV1CodecConfigurationRecord record;
    BitReader bitReader(buffer);
    std::optional<size_t> value;

    // aligned(8) class AV1CodecConfigurationRecord
    // {
    //   unsigned int(1) marker = 1;
    //   unsigned int(7) version = 1;
    //   unsigned int(3) seq_profile;
    //   unsigned int(5) seq_level_idx_0;
    //   unsigned int(1) seq_tier_0;
    //   unsigned int(1) high_bitdepth;
    //   unsigned int(1) twelve_bit;
    //   unsigned int(1) monochrome;
    //   unsigned int(1) chroma_subsampling_x;
    //   unsigned int(1) chroma_subsampling_y;
    //   unsigned int(2) chroma_sample_position;
    //   unsigned int(3) reserved = 0;
    //
    //   unsigned int(1) initial_presentation_delay_present;
    //   if(initial_presentation_delay_present) {
    //     unsigned int(4) initial_presentation_delay_minus_one;
    //   } else {
    //     unsigned int(4) reserved = 0;
    //   }
    //
    //   unsigned int(8) configOBUs[];
    // }

    // marker f(1) - should be 1
    value = bitReader.read(1);
    if (!value || !*value)
        return std::nullopt;

    // version f(7) - should be 1
    value = bitReader.read(7);
    if (!value || *value != 1)
        return std::nullopt;

    // seq_profile f(3)
    value = bitReader.read(3);
    if (!value || !isValidEnum<AV1ConfigurationProfile>(*value))
        return std::nullopt;
    record.profile = static_cast<AV1ConfigurationProfile>(*value);

    // seq_level_idx_0 f(5)
    value = bitReader.read(5);
    if (!value || !isValidEnum<AV1ConfigurationLevel>(*value))
        return std::nullopt;
    record.level = static_cast<AV1ConfigurationLevel>(*value);

    // seq_tier_0 f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    record.tier = *value ? AV1ConfigurationTier::High : AV1ConfigurationTier::Main;

    // high_bitdepth f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    bool highBitDepth = *value;

    // twelve_bit f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    bool twelveBit = *value;

    if (!highBitDepth && twelveBit)
        return std::nullopt;

    if (highBitDepth && twelveBit)
        record.bitDepth = 12;
    else if (highBitDepth)
        record.bitDepth = 10;
    else
        record.bitDepth = 8;

    // monochrome f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    record.monochrome = *value;

    // chroma_subsampling_x f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    uint8_t chromaSubsamplingX = *value;

    // chroma_subsampling_y f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    uint8_t chromaSubsamplingY = *value;

    // chroma_sample_position f(2)
    value = bitReader.read(2);
    if (!value)
        return std::nullopt;
    uint8_t chromaSamplePosition = static_cast<uint8_t>(*value);

    // Compute chromaSubsampling value: first digit = subsampling_x, second = subsampling_y, third = chroma_sample_position (if 4:2:0) or 0
    record.chromaSubsampling = chromaSubsamplingX * 100 + chromaSubsamplingY * 10 + ((chromaSubsamplingX && chromaSubsamplingY) ? chromaSamplePosition : 0);

    // Initialize dimension fields to default values (decoder config doesn't contain dimensions)
    record.width = AV1CodecConfigurationRecord::defaultWidth;
    record.height = AV1CodecConfigurationRecord::defaultHeight;
    record.codecName = "av01"_s;

    return record;
}

std::optional<AV1CodecConfigurationRecord> parseSequenceHeaderOBU(std::span<const uint8_t> data)
{
    AV1CodecConfigurationRecord record;
    record.codecName = "av01"_s;

    BitReader bitReader(data);
    std::optional<size_t> value;

    // seq_profile f(3)
    value = bitReader.read(3);
    if (!value)
        return std::nullopt;
    if (!isValidEnum<AV1ConfigurationProfile>(*value))
        return std::nullopt;
    record.profile = static_cast<AV1ConfigurationProfile>(*value);
    uint8_t seqProfile = static_cast<uint8_t>(*value);

    // still_picture f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    bool stillPicture = *value;

    // reduced_still_picture_header f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    bool reducedStillPictureHeader = *value;

    // Per spec: reduced_still_picture_header requires still_picture to be 1
    if (reducedStillPictureHeader && !stillPicture)
        return std::nullopt;

    bool timingInfoPresentFlag = false;
    bool decoderModelInfoPresentFlag = false;
    bool initialDisplayDelayPresentFlag = false;
    size_t operatingPointsCntMinus1 = 0;
    size_t bufferDelayLengthMinus1 = 0;

    if (reducedStillPictureHeader) {
        // When reduced_still_picture_header is set:
        // timing_info_present_flag = 0
        // decoder_model_info_present_flag = 0
        // initial_display_delay_present_flag = 0
        // operating_points_cnt_minus_1 = 0
        // operating_point_idc[0] = 0
        // seq_level_idx[0] f(5)
        value = bitReader.read(5);
        if (!value)
            return std::nullopt;
        if (!isValidEnum<AV1ConfigurationLevel>(*value))
            return std::nullopt;
        record.level = static_cast<AV1ConfigurationLevel>(*value);
        // seq_tier[0] = 0
        record.tier = AV1ConfigurationTier::Main;
        // decoder_model_present_for_this_op[0] = 0
        // initial_display_delay_present_for_this_op[0] = 0
    } else {
        // timing_info_present_flag f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        timingInfoPresentFlag = *value;

        if (timingInfoPresentFlag) {
            // timing_info()
            // num_units_in_display_tick f(32)
            value = bitReader.read(32);
            if (!value)
                return std::nullopt;
            // time_scale f(32)
            value = bitReader.read(32);
            if (!value)
                return std::nullopt;
            // equal_picture_interval f(1)
            value = bitReader.read(1);
            if (!value)
                return std::nullopt;
            bool equalPictureInterval = *value;
            if (equalPictureInterval) {
                // num_ticks_per_picture_minus_1 uvlc()
                // Parse uvlc: count leading zeros, then read that many bits
                size_t leadingZeros = 0;
                while (true) {
                    value = bitReader.read(1);
                    if (!value)
                        return std::nullopt;
                    if (*value)
                        break;
                    leadingZeros++;
                    if (leadingZeros >= 32)
                        break;
                }
                if (leadingZeros < 32 && leadingZeros > 0) {
                    value = bitReader.read(leadingZeros);
                    if (!value)
                        return std::nullopt;
                }
            }

            // decoder_model_info_present_flag f(1)
            value = bitReader.read(1);
            if (!value)
                return std::nullopt;
            decoderModelInfoPresentFlag = *value;

            if (decoderModelInfoPresentFlag) {
                // decoder_model_info()
                // buffer_delay_length_minus_1 f(5)
                value = bitReader.read(5);
                if (!value)
                    return std::nullopt;
                bufferDelayLengthMinus1 = *value;
                // num_units_in_decoding_tick f(32)
                value = bitReader.read(32);
                if (!value)
                    return std::nullopt;
                // buffer_removal_time_length_minus_1 f(5)
                value = bitReader.read(5);
                if (!value)
                    return std::nullopt;
                // frame_presentation_time_length_minus_1 f(5)
                value = bitReader.read(5);
                if (!value)
                    return std::nullopt;
            }
        } else
            decoderModelInfoPresentFlag = false;

        // initial_display_delay_present_flag f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        initialDisplayDelayPresentFlag = *value;

        // operating_points_cnt_minus_1 f(5)
        value = bitReader.read(5);
        if (!value)
            return std::nullopt;
        operatingPointsCntMinus1 = *value;

        // For each operating point
        for (size_t i = 0; i <= operatingPointsCntMinus1; i++) {
            // operating_point_idc[i] f(12)
            value = bitReader.read(12);
            if (!value)
                return std::nullopt;

            // seq_level_idx[i] f(5)
            value = bitReader.read(5);
            if (!value)
                return std::nullopt;
            // Use the first operating point's level
            if (!i) {
                if (!isValidEnum<AV1ConfigurationLevel>(*value))
                    return std::nullopt;
                record.level = static_cast<AV1ConfigurationLevel>(*value);
            }
            size_t seqLevelIdx = *value;

            // seq_tier[i] - only present if seq_level_idx[i] > 7 (i.e., level > 3.3)
            if (seqLevelIdx > 7) {
                value = bitReader.read(1);
                if (!value)
                    return std::nullopt;
                if (!i)
                    record.tier = *value ? AV1ConfigurationTier::High : AV1ConfigurationTier::Main;
            } else if (!i)
                record.tier = AV1ConfigurationTier::Main;

            if (decoderModelInfoPresentFlag) {
                // decoder_model_present_for_this_op[i] f(1)
                value = bitReader.read(1);
                if (!value)
                    return std::nullopt;
                bool decoderModelPresentForThisOp = *value;
                if (decoderModelPresentForThisOp) {
                    // operating_parameters_info(i)
                    // n = buffer_delay_length_minus_1 + 1
                    size_t n = bufferDelayLengthMinus1 + 1;
                    // decoder_buffer_delay[op] f(n)
                    value = bitReader.read(n);
                    if (!value)
                        return std::nullopt;
                    // encoder_buffer_delay[op] f(n)
                    value = bitReader.read(n);
                    if (!value)
                        return std::nullopt;
                    // low_delay_mode_flag[op] f(1)
                    value = bitReader.read(1);
                    if (!value)
                        return std::nullopt;
                }
            }

            if (initialDisplayDelayPresentFlag) {
                // initial_display_delay_present_for_this_op[i] f(1)
                value = bitReader.read(1);
                if (!value)
                    return std::nullopt;
                bool initialDisplayDelayPresentForThisOp = *value;
                if (initialDisplayDelayPresentForThisOp) {
                    // initial_display_delay_minus_1[i] f(4)
                    value = bitReader.read(4);
                    if (!value)
                        return std::nullopt;
                }
            }
        }
    }

    // frame_width_bits_minus_1 f(4)
    value = bitReader.read(4);
    if (!value)
        return std::nullopt;
    size_t frameWidthBitsMinus1 = *value;

    // frame_height_bits_minus_1 f(4)
    value = bitReader.read(4);
    if (!value)
        return std::nullopt;
    size_t frameHeightBitsMinus1 = *value;

    // max_frame_width_minus_1 f(n) where n = frame_width_bits_minus_1 + 1
    value = bitReader.read(frameWidthBitsMinus1 + 1);
    if (!value)
        return std::nullopt;
    record.width = static_cast<uint32_t>(*value + 1);

    // max_frame_height_minus_1 f(n) where n = frame_height_bits_minus_1 + 1
    value = bitReader.read(frameHeightBitsMinus1 + 1);
    if (!value)
        return std::nullopt;
    record.height = static_cast<uint32_t>(*value + 1);

    bool frameIdNumbersPresentFlag = false;
    if (!reducedStillPictureHeader) {
        // frame_id_numbers_present_flag f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        frameIdNumbersPresentFlag = *value;
    }

    if (frameIdNumbersPresentFlag) {
        // delta_frame_id_length_minus_2 f(4)
        value = bitReader.read(4);
        if (!value)
            return std::nullopt;
        // additional_frame_id_length_minus_1 f(3)
        value = bitReader.read(3);
        if (!value)
            return std::nullopt;
    }

    // use_128x128_superblock f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;

    // enable_filter_intra f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;

    // enable_intra_edge_filter f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;

    if (!reducedStillPictureHeader) {
        // enable_interintra_compound f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;

        // enable_masked_compound f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;

        // enable_warped_motion f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;

        // enable_dual_filter f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;

        // enable_order_hint f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        bool enableOrderHint = *value;

        if (enableOrderHint) {
            // enable_jnt_comp f(1)
            value = bitReader.read(1);
            if (!value)
                return std::nullopt;

            // enable_ref_frame_mvs f(1)
            value = bitReader.read(1);
            if (!value)
                return std::nullopt;
        }

        // seq_choose_screen_content_tools f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        bool seqChooseScreenContentTools = *value;

        size_t seqForceScreenContentTools = 2; // SELECT_SCREEN_CONTENT_TOOLS
        if (!seqChooseScreenContentTools) {
            // seq_force_screen_content_tools f(1)
            value = bitReader.read(1);
            if (!value)
                return std::nullopt;
            seqForceScreenContentTools = *value;
        }

        if (seqForceScreenContentTools > 0) {
            // seq_choose_integer_mv f(1)
            value = bitReader.read(1);
            if (!value)
                return std::nullopt;
            bool seqChooseIntegerMv = *value;

            if (!seqChooseIntegerMv) {
                // seq_force_integer_mv f(1)
                value = bitReader.read(1);
                if (!value)
                    return std::nullopt;
            }
        }

        if (enableOrderHint) {
            // order_hint_bits_minus_1 f(3)
            value = bitReader.read(3);
            if (!value)
                return std::nullopt;
        }
    }

    // enable_superres f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;

    // enable_cdef f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;

    // enable_restoration f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;

    // color_config()
    // high_bitdepth f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    bool highBitdepth = *value;

    bool twelveBit = false;
    if (seqProfile == 2 && highBitdepth) {
        // twelve_bit f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        twelveBit = *value;
        record.bitDepth = twelveBit ? 12 : 10;
    } else if (seqProfile <= 2)
        record.bitDepth = highBitdepth ? 10 : 8;

    uint8_t monochrome = 0;
    if (seqProfile != 1) {
        // mono_chrome f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        monochrome = *value;
    }
    record.monochrome = monochrome;

    // color_description_present_flag f(1)
    value = bitReader.read(1);
    if (!value)
        return std::nullopt;
    bool colorDescriptionPresentFlag = *value;

    uint8_t colorPrimaries = static_cast<uint8_t>(AV1ConfigurationColorPrimaries::Unspecified);
    uint8_t transferCharacteristics = static_cast<uint8_t>(AV1ConfigurationTransferCharacteristics::Unspecified);
    uint8_t matrixCoefficients = static_cast<uint8_t>(AV1ConfigurationMatrixCoefficients::Unspecified);

    if (colorDescriptionPresentFlag) {
        // color_primaries f(8)
        value = bitReader.read(8);
        if (!value)
            return std::nullopt;
        colorPrimaries = static_cast<uint8_t>(*value);

        // transfer_characteristics f(8)
        value = bitReader.read(8);
        if (!value)
            return std::nullopt;
        transferCharacteristics = static_cast<uint8_t>(*value);

        // matrix_coefficients f(8)
        value = bitReader.read(8);
        if (!value)
            return std::nullopt;
        matrixCoefficients = static_cast<uint8_t>(*value);
    }

    record.colorPrimaries = colorPrimaries;
    record.transferCharacteristics = transferCharacteristics;
    record.matrixCoefficients = matrixCoefficients;

    bool colorRange = false;
    uint8_t subsamplingX = 1;
    uint8_t subsamplingY = 1;
    uint8_t chromaSamplePosition = 0; // CSP_UNKNOWN

    constexpr uint8_t CP_BT_709 = static_cast<uint8_t>(AV1ConfigurationColorPrimaries::BT_709_6);
    constexpr uint8_t TC_SRGB = static_cast<uint8_t>(AV1ConfigurationTransferCharacteristics::IEC_61966_2_1);
    constexpr uint8_t MC_IDENTITY = static_cast<uint8_t>(AV1ConfigurationMatrixCoefficients::Identity);

    if (monochrome) {
        // color_range f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        colorRange = *value;
        subsamplingX = 1;
        subsamplingY = 1;
        chromaSamplePosition = 0; // CSP_UNKNOWN
    } else if (colorPrimaries == CP_BT_709 && transferCharacteristics == TC_SRGB && matrixCoefficients == MC_IDENTITY) {
        colorRange = true;
        subsamplingX = 0;
        subsamplingY = 0;
    } else {
        // color_range f(1)
        value = bitReader.read(1);
        if (!value)
            return std::nullopt;
        colorRange = *value;

        if (!seqProfile) {
            subsamplingX = 1;
            subsamplingY = 1;
        } else if (seqProfile == 1) {
            subsamplingX = 0;
            subsamplingY = 0;
        } else {
            // Profile 2
            if (record.bitDepth == 12) {
                // subsampling_x f(1)
                value = bitReader.read(1);
                if (!value)
                    return std::nullopt;
                subsamplingX = static_cast<uint8_t>(*value);

                if (subsamplingX) {
                    // subsampling_y f(1)
                    value = bitReader.read(1);
                    if (!value)
                        return std::nullopt;
                    subsamplingY = static_cast<uint8_t>(*value);
                } else
                    subsamplingY = 0;
            } else {
                subsamplingX = 1;
                subsamplingY = 0;
            }
        }

        if (subsamplingX && subsamplingY) {
            // chroma_sample_position f(2)
            value = bitReader.read(2);
            if (!value)
                return std::nullopt;
            chromaSamplePosition = static_cast<uint8_t>(*value);
        }
    }

    record.videoFullRangeFlag = colorRange ? AV1ConfigurationRange::FullRange : AV1ConfigurationRange::VideoRange;

    // Compute chromaSubsampling value as per codec string spec:
    // First digit = subsampling_x, second digit = subsampling_y, third digit = chroma_sample_position (if 4:2:0) or 0
    uint8_t chromaSubsamplingValue = subsamplingX * 100 + subsamplingY * 10;
    if (subsamplingX && subsamplingY)
        chromaSubsamplingValue += chromaSamplePosition;
    record.chromaSubsampling = chromaSubsamplingValue;

    return record;
}

PlatformVideoColorSpace createPlatformVideoColorSpaceFromAV1CodecConfigurationRecord(const AV1CodecConfigurationRecord& record)
{
    PlatformVideoColorSpace colorSpace;

    // Convert AV1 color primaries to PlatformVideoColorPrimaries
    // AV1 color primaries are defined in ISO/IEC 23091-2:2019 (same as ITU-T H.273)
    colorSpace.primaries = [](uint8_t colorPrimaries) {
        switch (static_cast<AV1ConfigurationColorPrimaries>(colorPrimaries)) {
        case AV1ConfigurationColorPrimaries::BT_709_6:
            return PlatformVideoColorPrimaries::Bt709;
        case AV1ConfigurationColorPrimaries::BT_470_6_M:
            return PlatformVideoColorPrimaries::Bt470m;
        case AV1ConfigurationColorPrimaries::BT_470_7_BG:
            return PlatformVideoColorPrimaries::Bt470bg;
        case AV1ConfigurationColorPrimaries::BT_601_7:
            return PlatformVideoColorPrimaries::Smpte170m;
        case AV1ConfigurationColorPrimaries::SMPTE_ST_240:
            return PlatformVideoColorPrimaries::Smpte240m;
        case AV1ConfigurationColorPrimaries::Film:
            return PlatformVideoColorPrimaries::Film;
        case AV1ConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance:
            return PlatformVideoColorPrimaries::Bt2020;
        case AV1ConfigurationColorPrimaries::SMPTE_ST_428_1:
            return PlatformVideoColorPrimaries::SmpteSt4281;
        case AV1ConfigurationColorPrimaries::SMPTE_RP_431_2:
            return PlatformVideoColorPrimaries::SmpteRp431;
        case AV1ConfigurationColorPrimaries::SMPTE_EG_432_1:
            return PlatformVideoColorPrimaries::SmpteEg432;
        case AV1ConfigurationColorPrimaries::EBU_Tech_3213_E:
            return PlatformVideoColorPrimaries::JedecP22Phosphors;
        case AV1ConfigurationColorPrimaries::Unspecified:
        default:
            return PlatformVideoColorPrimaries::Unspecified;
        }
    }(record.colorPrimaries);

    // Convert AV1 transfer characteristics to PlatformVideoTransferCharacteristics
    colorSpace.transfer = [](uint8_t transferCharacteristics) {
        switch (static_cast<AV1ConfigurationTransferCharacteristics>(transferCharacteristics)) {
        case AV1ConfigurationTransferCharacteristics::BT_709_6:
            return PlatformVideoTransferCharacteristics::Bt709;
        case AV1ConfigurationTransferCharacteristics::BT_470_6_M:
            return PlatformVideoTransferCharacteristics::Gamma22curve;
        case AV1ConfigurationTransferCharacteristics::BT_470_7_BG:
            return PlatformVideoTransferCharacteristics::Gamma28curve;
        case AV1ConfigurationTransferCharacteristics::BT_601_7:
            return PlatformVideoTransferCharacteristics::Smpte170m;
        case AV1ConfigurationTransferCharacteristics::SMPTE_ST_240:
            return PlatformVideoTransferCharacteristics::Smpte240m;
        case AV1ConfigurationTransferCharacteristics::Linear:
            return PlatformVideoTransferCharacteristics::Linear;
        case AV1ConfigurationTransferCharacteristics::Logrithmic:
            return PlatformVideoTransferCharacteristics::Log;
        case AV1ConfigurationTransferCharacteristics::Logrithmic_Sqrt:
            return PlatformVideoTransferCharacteristics::LogSqrt;
        case AV1ConfigurationTransferCharacteristics::IEC_61966_2_4:
            return PlatformVideoTransferCharacteristics::Iec6196624;
        case AV1ConfigurationTransferCharacteristics::BT_1361_0:
            return PlatformVideoTransferCharacteristics::Bt1361ExtendedColourGamut;
        case AV1ConfigurationTransferCharacteristics::IEC_61966_2_1:
            return PlatformVideoTransferCharacteristics::Iec6196621;
        case AV1ConfigurationTransferCharacteristics::BT_2020_10bit:
            return PlatformVideoTransferCharacteristics::Bt2020_10bit;
        case AV1ConfigurationTransferCharacteristics::BT_2020_12bit:
            return PlatformVideoTransferCharacteristics::Bt2020_12bit;
        case AV1ConfigurationTransferCharacteristics::SMPTE_ST_2084:
            return PlatformVideoTransferCharacteristics::SmpteSt2084;
        case AV1ConfigurationTransferCharacteristics::SMPTE_ST_428_1:
            return PlatformVideoTransferCharacteristics::SmpteSt4281;
        case AV1ConfigurationTransferCharacteristics::BT_2100_HLG:
            return PlatformVideoTransferCharacteristics::AribStdB67Hlg;
        case AV1ConfigurationTransferCharacteristics::Unspecified:
        default:
            return PlatformVideoTransferCharacteristics::Unspecified;
        }
    }(record.transferCharacteristics);

    // Convert AV1 matrix coefficients to PlatformVideoMatrixCoefficients
    colorSpace.matrix = [](uint8_t matrixCoefficients) {
        switch (static_cast<AV1ConfigurationMatrixCoefficients>(matrixCoefficients)) {
        case AV1ConfigurationMatrixCoefficients::Identity:
            return PlatformVideoMatrixCoefficients::Rgb;
        case AV1ConfigurationMatrixCoefficients::BT_709_6:
            return PlatformVideoMatrixCoefficients::Bt709;
        case AV1ConfigurationMatrixCoefficients::FCC:
            return PlatformVideoMatrixCoefficients::Fcc;
        case AV1ConfigurationMatrixCoefficients::BT_470_7_BG:
            return PlatformVideoMatrixCoefficients::Bt470bg;
        case AV1ConfigurationMatrixCoefficients::BT_601_7:
            return PlatformVideoMatrixCoefficients::Smpte170m;
        case AV1ConfigurationMatrixCoefficients::SMPTE_ST_240:
            return PlatformVideoMatrixCoefficients::Smpte240m;
        case AV1ConfigurationMatrixCoefficients::YCgCo:
            return PlatformVideoMatrixCoefficients::YCgCo;
        case AV1ConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance:
            return PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance;
        case AV1ConfigurationMatrixCoefficients::BT_2020_Constant_Luminance:
            return PlatformVideoMatrixCoefficients::Bt2020ConstantLuminance;
        case AV1ConfigurationMatrixCoefficients::Unspecified:
        default:
            return PlatformVideoMatrixCoefficients::Unspecified;
        }
    }(record.matrixCoefficients);

    // Convert AV1 video full range flag
    colorSpace.fullRange = record.videoFullRangeFlag == AV1ConfigurationRange::FullRange;

    return colorSpace;
}

static Ref<VideoInfo> createVideoInfoFromAV1CodecConfigurationRecord(const AV1CodecConfigurationRecord& record, std::span<const uint8_t> fullOBUHeader, std::optional<FloatSize> displaySize)
{
    // Build AV1 codec configuration record (av1C) for extensionAtoms
    // Format: marker(1) | version(7) | seq_profile(3) | seq_level_idx_0(5) |
    //         seq_tier_0(1) | high_bitdepth(1) | twelve_bit(1) | monochrome(1) |
    //         chroma_subsampling_x(1) | chroma_subsampling_y(1) | chroma_sample_position(2) |
    //         reserved(3) | initial_presentation_delay_present(1) | reserved(4)

    constexpr size_t VPCodecConfigurationContentsSize = 4;
    size_t av1CodecConfigurationRecordSize = VPCodecConfigurationContentsSize + fullOBUHeader.size();
    Vector<uint8_t> av1CBytes(av1CodecConfigurationRecordSize);

    uint8_t highBitdepth = (record.bitDepth > 8) ? 1 : 0;
    uint8_t twelveBit = (record.bitDepth == 12) ? 1 : 0;
    uint8_t chromaSubsamplingX = (record.chromaSubsampling / 100) & 1;
    uint8_t chromaSubsamplingY = ((record.chromaSubsampling / 10) % 10) & 1;
    uint8_t chromaSamplePosition = record.chromaSubsampling % 10;

    av1CBytes[0] = 0x81; // marker=1, version=1
    av1CBytes[1] = (static_cast<uint8_t>(record.profile) << 5) | static_cast<uint8_t>(record.level);
    av1CBytes[2] = (static_cast<uint8_t>(record.tier) << 7) | (highBitdepth << 6) | (twelveBit << 5) | (record.monochrome << 4) | (chromaSubsamplingX << 3) | (chromaSubsamplingY << 2) | chromaSamplePosition;
    av1CBytes[3] = 0; // reserved(3) | initial_presentation_delay_present(1) | reserved(4)

    // unsigned int(8) configOBUs[];
    memcpySpan(av1CBytes.mutableSpan().subspan(4), fullOBUHeader);

    return VideoInfo::create({
        {
            .codecName = { "av01" },
            .codecString = createAV1CodecParametersString(record)
        } , {
            .size = FloatSize(record.width, record.height),
            .displaySize = displaySize.value_or(FloatSize(record.width, record.height)),
            .bitDepth = record.bitDepth,
            .colorSpace = createPlatformVideoColorSpaceFromAV1CodecConfigurationRecord(record),
            .extensionAtoms = { 1, TrackInfo::AtomData { { "av1C" }, SharedBuffer::create(WTF::move(av1CBytes)) } }
        }
    });
}

static size_t readULEBSize(std::span<const uint8_t> data, size_t& index)
{
    size_t value = 0;
    for (size_t cptr = 0; cptr < 8; ++cptr) {
        if (index >= data.size())
            return 0;

        uint8_t dataByte = data[index++];
        uint8_t decodedByte = dataByte & 0x7f;
        value |= decodedByte << (7 * cptr);
        if (value >= std::numeric_limits<uint32_t>::max())
            return 0;
        if (!(dataByte & 0x80))
            break;
    }
    return value;
}

static std::optional<std::pair<std::span<const uint8_t>, std::span<const uint8_t>>> getSequenceHeaderOBU(std::span<const uint8_t> data)
{
    size_t index = 0;
    do {
        if (index >= data.size())
            return std::nullopt;

        auto startIndex = index;
        auto value = data[index++];
        if (value >> 7)
            return std::nullopt;
        auto headerType = value >> 3;
        bool hasPayloadSize = value & 0x02;
        if (!hasPayloadSize)
            return std::nullopt;

        bool hasExtension = value & 0x04;
        if (hasExtension)
            ++index;

        Checked<size_t> payloadSize = readULEBSize(data, index);
        if (index + payloadSize >= data.size())
            return std::nullopt;

        if (headerType == 1) {
            auto fullObu = data.subspan(startIndex, payloadSize + index - startIndex);
            auto obuData = data.subspan(index, payloadSize);
            return std::make_pair(fullObu, obuData);
        }

        index += payloadSize;
    } while (true);
    return std::nullopt;
}

RefPtr<VideoInfo> createVideoInfoFromAV1Stream(std::span<const uint8_t> data, std::optional<FloatSize> displaySize)
{
    auto sequenceHeaderData = getSequenceHeaderOBU(data);
    if (!sequenceHeaderData)
        return { };

    auto record = parseSequenceHeaderOBU(sequenceHeaderData->second);
    if (!record)
        return { };

    return createVideoInfoFromAV1CodecConfigurationRecord(*record, sequenceHeaderData->first, displaySize);
}

}
