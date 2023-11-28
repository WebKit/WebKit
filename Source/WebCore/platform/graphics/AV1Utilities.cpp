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

#include "MediaCapabilitiesInfo.h"
#include "VideoConfiguration.h"
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

template<typename E>
std::optional<E> parseEnumFromStringView(StringView stringView)
{
    auto value = parseInteger<std::underlying_type_t<E>>(stringView);
    if (!value || !isValidEnum<E>(*value))
        return std::nullopt;
    return static_cast<E>(*value);
}

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
        return std::nullopt;

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
        return std::nullopt;

    // The colorPrimaries, transferCharacteristics, matrixCoefficients, and videoFullRangeFlag
    // parameter values SHALL equal the value of matching fields in the Sequence Header OBU, if
    // color_description_present_flag is set to 1, otherwise they SHOULD not be set, defaulting
    // to the values below.
    auto colorPrimaries = parseEnumFromStringView<AV1ConfigurationColorPrimaries>(*nextElement);
    if (!colorPrimaries)
        return std::nullopt;
    configuration.colorPrimaries = static_cast<uint8_t>(*colorPrimaries);

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto transferCharacteristics = parseEnumFromStringView<AV1ConfigurationTransferCharacteristics>(*nextElement);
    if (!transferCharacteristics)
        return std::nullopt;
    configuration.transferCharacteristics = static_cast<uint8_t>(*transferCharacteristics);

    if (++nextElement == codecSplit.end())
        return std::nullopt;

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
    builder.append("av01");

    auto appendOneDigit = [&](uint8_t number) {
        builder.append(static_cast<LChar>('0' + number % 10));
    };

    auto appendTwoDigits = [&](uint8_t number) {
        builder.append(static_cast<LChar>('0' + number / 10 % 10));
        builder.append(static_cast<LChar>('0' + number % 10));
    };

    auto appendThreeDigits = [&](uint8_t number) {
        builder.append(static_cast<LChar>('0' + number / 100 % 10));
        builder.append(static_cast<LChar>('0' + number / 10 % 10));
        builder.append(static_cast<LChar>('0' + number % 10));
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

bool validateAV1PerLevelConstraints(const AV1CodecConfigurationRecord& record, const VideoConfiguration& configuration)
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

std::optional<AV1CodecConfigurationRecord> parseAV1DecoderConfigurationRecord(const SharedBuffer& buffer)
    {
    // Ref: https://aomediacodec.github.io/av1-isobmff/
    // Section 2.3: AV1 Codec Configuration Box

    // AV1CodecConfigurationRecord is at least 4 bytes long
    if (buffer.size() < 4)
        return std::nullopt;

    AV1CodecConfigurationRecord record;

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

    auto arrayBuffer = buffer.tryCreateArrayBuffer();
    if (!arrayBuffer)
        return std::nullopt;

    bool status = true;
    auto view = JSC::DataView::create(WTFMove(arrayBuffer), 0, buffer.size());

    auto profileLevel = view->get<uint8_t>(1, false, &status);
    if (!status)
        return std::nullopt;

    if (!isValidEnum<AV1ConfigurationProfile>((profileLevel & 0b11100000) >> 5))
        return std::nullopt;
    record.profile = static_cast<AV1ConfigurationProfile>((profileLevel & 0b11100000) >> 5);

    if (!isValidEnum<AV1ConfigurationLevel>(profileLevel & 0b00011111))
        return std::nullopt;
    record.level = static_cast<AV1ConfigurationLevel>(profileLevel & 0b00011111);


    auto tierBitdepthAndColorFlags = view->get<uint8_t>(2, false, &status);
    if (!status)
        return std::nullopt;

    record.tier = tierBitdepthAndColorFlags & 0b10000000 ? AV1ConfigurationTier::High : AV1ConfigurationTier::Main;
    bool highBitDepth = tierBitdepthAndColorFlags & 0b01000000;
    bool twelveBit = tierBitdepthAndColorFlags & 0b00100000;
    if (!highBitDepth && twelveBit)
        return std::nullopt;

    if (highBitDepth && twelveBit)
        record.bitDepth = 12;
    else if (highBitDepth)
        record.bitDepth = 10;
    else
        record.bitDepth = 8;

    record.monochrome = tierBitdepthAndColorFlags & 0b00010000;
    uint8_t chromaSubsamplingValue = 0;
    if (tierBitdepthAndColorFlags & 0b00001000)
        chromaSubsamplingValue += 100;
    if (tierBitdepthAndColorFlags & 0b00000100)
        chromaSubsamplingValue += 10;
    chromaSubsamplingValue += tierBitdepthAndColorFlags & 0b00000010;
    record.chromaSubsampling = chromaSubsamplingValue;

    return record;
}

}
