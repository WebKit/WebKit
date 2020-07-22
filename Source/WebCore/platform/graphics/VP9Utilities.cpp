/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "VP9Utilities.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static bool isValidVPLevel(uint8_t level)
{
    constexpr uint8_t validLevels[] = {
        VPConfigurationLevel::Level_1,
        VPConfigurationLevel::Level_1_1,
        VPConfigurationLevel::Level_2,
        VPConfigurationLevel::Level_2_1,
        VPConfigurationLevel::Level_3,
        VPConfigurationLevel::Level_3_1,
        VPConfigurationLevel::Level_4,
        VPConfigurationLevel::Level_4_1,
        VPConfigurationLevel::Level_5,
        VPConfigurationLevel::Level_5_1,
        VPConfigurationLevel::Level_5_2,
        VPConfigurationLevel::Level_6,
        VPConfigurationLevel::Level_6_1,
        VPConfigurationLevel::Level_6_2,
    };

    ASSERT(std::is_sorted(std::begin(validLevels), std::end(validLevels)));
    return std::binary_search(std::begin(validLevels), std::end(validLevels), level);
}

static bool isValidVPColorPrimaries(uint8_t colorPrimaries)
{
    constexpr uint8_t validColorPrimaries[] = {
        VPConfigurationColorPrimaries::BT_709_6,
        VPConfigurationColorPrimaries::Unspecified,
        VPConfigurationColorPrimaries::BT_470_6_M,
        VPConfigurationColorPrimaries::BT_470_7_BG,
        VPConfigurationColorPrimaries::BT_601_7,
        VPConfigurationColorPrimaries::SMPTE_ST_240,
        VPConfigurationColorPrimaries::Film,
        VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance,
        VPConfigurationColorPrimaries::SMPTE_ST_428_1,
        VPConfigurationColorPrimaries::SMPTE_RP_431_2,
        VPConfigurationColorPrimaries::SMPTE_EG_432_1,
        VPConfigurationColorPrimaries::EBU_Tech_3213_E,
    };

    ASSERT(std::is_sorted(std::begin(validColorPrimaries), std::end(validColorPrimaries)));
    return std::binary_search(std::begin(validColorPrimaries), std::end(validColorPrimaries), colorPrimaries);
}

static bool isValidVPTransferCharacteristics(uint8_t transferCharacteristics)
{
    constexpr uint8_t validTransferCharacteristics[] = {
        VPConfigurationTransferCharacteristics::BT_709_6,
        VPConfigurationTransferCharacteristics::Unspecified,
        VPConfigurationTransferCharacteristics::BT_470_6_M,
        VPConfigurationTransferCharacteristics::BT_470_7_BG,
        VPConfigurationTransferCharacteristics::BT_601_7,
        VPConfigurationTransferCharacteristics::SMPTE_ST_240,
        VPConfigurationTransferCharacteristics::Linear,
        VPConfigurationTransferCharacteristics::Logrithmic,
        VPConfigurationTransferCharacteristics::Logrithmic_Sqrt,
        VPConfigurationTransferCharacteristics::IEC_61966_2_4,
        VPConfigurationTransferCharacteristics::BT_1361_0,
        VPConfigurationTransferCharacteristics::IEC_61966_2_1,
        VPConfigurationTransferCharacteristics::BT_2020_10bit,
        VPConfigurationTransferCharacteristics::BT_2020_12bit,
        VPConfigurationTransferCharacteristics::SMPTE_ST_2084,
        VPConfigurationTransferCharacteristics::SMPTE_ST_428_1,
        VPConfigurationTransferCharacteristics::BT_2100_HLG,
    };
    ASSERT(std::is_sorted(std::begin(validTransferCharacteristics), std::end(validTransferCharacteristics)));
    return std::binary_search(std::begin(validTransferCharacteristics), std::end(validTransferCharacteristics), transferCharacteristics);
}

static bool isValidVPMatrixCoefficients(uint8_t matrixCoefficients)
{
    constexpr uint8_t validMatrixCoefficients[] = {
        VPConfigurationMatrixCoefficients::Identity,
        VPConfigurationMatrixCoefficients::BT_709_6,
        VPConfigurationMatrixCoefficients::Unspecified,
        VPConfigurationMatrixCoefficients::FCC,
        VPConfigurationMatrixCoefficients::BT_470_7_BG,
        VPConfigurationMatrixCoefficients::BT_601_7,
        VPConfigurationMatrixCoefficients::SMPTE_ST_240,
        VPConfigurationMatrixCoefficients::YCgCo,
        VPConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance,
        VPConfigurationMatrixCoefficients::BT_2020_Constant_Luminance,
        VPConfigurationMatrixCoefficients::SMPTE_ST_2085,
        VPConfigurationMatrixCoefficients::Chromacity_Constant_Luminance,
        VPConfigurationMatrixCoefficients::Chromacity_Nonconstant_Luminance,
        VPConfigurationMatrixCoefficients::BT_2100_ICC,
    };
    ASSERT(std::is_sorted(std::begin(validMatrixCoefficients), std::end(validMatrixCoefficients)));
    return std::binary_search(std::begin(validMatrixCoefficients), std::end(validMatrixCoefficients), matrixCoefficients);
}

Optional<VPCodecConfigurationRecord> parseVPCodecParameters(StringView codecView)
{
    // The format of the 'vp09' codec string is specified in the webm GitHub repo:
    // <https://github.com/webmproject/vp9-dash/blob/master/VPCodecISOMediaFileFormatBinding.md#codecs-parameter-string>

    // "sample entry 4CC, profile, level, and bitDepth are all mandatory fields. If any of these fields are empty, or not
    // within their allowed range, the processing device SHALL treat it as an error."

    auto codecSplit = codecView.split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return WTF::nullopt;

    VPCodecConfigurationRecord configuration;

    // Codec identifier: legal values are 'vp08' or 'vp09'.
    configuration.codecName = (*nextElement).toString();
    if (configuration.codecName != "vp08" && configuration.codecName != "vp09")
        return WTF::nullopt;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // First element: profile. Legal values are 0-3.
    auto profile = toIntegralType<uint8_t>(*nextElement);
    if (!profile || *profile > 3)
        return WTF::nullopt;
    configuration.profile = *profile;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Second element: level. Legal values are enumerated in validVPLevels above.
    auto level = toIntegralType<uint8_t>(*nextElement);
    if (!level || !isValidVPLevel(*level))
        return WTF::nullopt;
    configuration.level = *level;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Third element: bitDepth. Legal values are 8, 10, and 12.
    auto bitDepth = toIntegralType<uint8_t>(*nextElement);
    if (!bitDepth || (*bitDepth != 8 && *bitDepth != 10 && *bitDepth != 12))
        return WTF::nullopt;
    configuration.bitDepth = *bitDepth;

    // "colorPrimaries, transferCharacteristics, matrixCoefficients, videoFullRangeFlag, and chromaSubsampling are OPTIONAL,
    // mutually inclusive (all or none) fields. If not specified then the processing device MUST use the values listed in the
    // table below as defaults when deciding if the device is able to decode and potentially render the video."

    if (++nextElement == codecSplit.end())
        return configuration;

    // Fourth element: chromaSubsampling. Legal values are 0-3.
    auto chromaSubsampling = toIntegralType<uint8_t>(*nextElement);
    if (!chromaSubsampling || *chromaSubsampling > VPConfigurationChromaSubsampling::Subsampling_444)
        return WTF::nullopt;
    configuration.chromaSubsampling = *chromaSubsampling;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Fifth element: colorPrimaries. Legal values are defined by  ISO/IEC 23001-8:2016, superceded
    // by ISO/IEC 23091-2:2019.
    auto colorPrimaries = toIntegralType<uint8_t>(*nextElement);
    if (!colorPrimaries || !isValidVPColorPrimaries(*colorPrimaries))
        return WTF::nullopt;
    configuration.colorPrimaries = *colorPrimaries;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Sixth element: transferCharacteristics. Legal values are defined by  ISO/IEC 23001-8:2016, superceded
    // by ISO/IEC 23091-2:2019.
    auto transferCharacteristics = toIntegralType<uint8_t>(*nextElement);
    if (!transferCharacteristics || !isValidVPTransferCharacteristics(*transferCharacteristics))
        return WTF::nullopt;
    configuration.transferCharacteristics = *transferCharacteristics;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Seventh element: matrixCoefficients. Legal values are defined by  ISO/IEC 23001-8:2016, superceded
    // by ISO/IEC 23091-2:2019.
    auto matrixCoefficients = toIntegralType<uint8_t>(*nextElement);
    if (!matrixCoefficients || !isValidVPMatrixCoefficients(*matrixCoefficients))
        return WTF::nullopt;
    configuration.matrixCoefficients = *matrixCoefficients;

    // "If matrixCoefficients is 0 (RGB), then chroma subsampling MUST be 3 (4:4:4)."
    if (!configuration.matrixCoefficients && configuration.chromaSubsampling != 3)
        return WTF::nullopt;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Eighth element: videoFullRangeFlag. Legal values are 0 and 1.
    auto videoFullRangeFlag = toIntegralType<uint8_t>(*nextElement);
    if (!videoFullRangeFlag || *videoFullRangeFlag > 1)
        return WTF::nullopt;
    configuration.videoFullRangeFlag = *videoFullRangeFlag;

    if (++nextElement != codecSplit.end())
        return WTF::nullopt;

    return configuration;
}

}
