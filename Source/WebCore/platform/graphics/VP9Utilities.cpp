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
        10,
        11,
        20,
        21,
        30,
        31,
        40,
        41,
        50,
        51,
        52,
        60,
        61,
        62,
    };

    ASSERT(std::is_sorted(std::begin(validLevels), std::end(validLevels)));
    return std::binary_search(std::begin(validLevels), std::end(validLevels), level);
}

static bool isValidVPcolorPrimaries(uint8_t colorPrimaries)
{
    constexpr uint8_t validColorPrimaries[] = {
        1,
        4,
        5,
        6,
        7,
        8,
        9,
        10,
        11,
        12,
        22,
    };

    ASSERT(std::is_sorted(std::begin(validColorPrimaries), std::end(validColorPrimaries)));
    return std::binary_search(std::begin(validColorPrimaries), std::end(validColorPrimaries), colorPrimaries);
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
    if (!chromaSubsampling || *chromaSubsampling > 3)
        return WTF::nullopt;
    configuration.chromaSubsampling = *chromaSubsampling;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Fifth element: colorPrimaries. Legal values are defined by  ISO/IEC 23001-8:2016, superceded
    // by ISO/IEC 23091-2:2019.
    auto colorPrimaries = toIntegralType<uint8_t>(*nextElement);
    if (!colorPrimaries || !isValidVPcolorPrimaries(*colorPrimaries))
        return WTF::nullopt;
    configuration.colorPrimaries = *colorPrimaries;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Sixth element: transferCharacteristics. Legal values are defined by  ISO/IEC 23001-8:2016, superceded
    // by ISO/IEC 23091-2:2019.
    auto transferCharacteristics = toIntegralType<uint8_t>(*nextElement);
    if (!transferCharacteristics || (!*transferCharacteristics || *transferCharacteristics == 2 || *transferCharacteristics == 3 || *transferCharacteristics > 18))
        return WTF::nullopt;
    configuration.transferCharacteristics = *transferCharacteristics;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Seventh element: matrixCoefficients. Legal values are defined by  ISO/IEC 23001-8:2016, superceded
    // by ISO/IEC 23091-2:2019.
    auto matrixCoefficients = toIntegralType<uint8_t>(*nextElement);
    if (!matrixCoefficients || (*matrixCoefficients == 2 || *matrixCoefficients == 3 || *matrixCoefficients > 14))
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
