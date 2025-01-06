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

#include "BitReader.h"
#include <JavaScriptCore/DataView.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static bool isValidVPProfile(uint8_t profile)
{
    return profile <= 3;
}

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

static bool isValidBitDepth(uint8_t bitDepth)
{
    return bitDepth == 8
        || bitDepth == 10
        || bitDepth == 12;
}

static bool isValidRange(uint8_t range)
{
    return range == VPConfigurationRange::VideoRange
        || range == VPConfigurationRange::FullRange;
}

static bool isValidChromaSubsampling(uint8_t subsampling)
{
    return subsampling >= VPConfigurationChromaSubsampling::Subsampling_420_Vertical
        && subsampling <= VPConfigurationChromaSubsampling::Subsampling_444;
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

std::optional<VPCodecConfigurationRecord> parseVPCodecParameters(StringView codecView)
{
    auto codecSplit = codecView.split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return std::nullopt;

    VPCodecConfigurationRecord configuration;

    configuration.codecName = (*nextElement).toString();
    ++nextElement;

    // Support the legacy identifiers (with no parameters) for VP8 and VP9.
    if (configuration.codecName == "vp8"_s || configuration.codecName == "vp9"_s) {
        if (nextElement == codecSplit.end())
            return configuration;

        auto codecString = codecView.toStringWithoutCopying();
        if (codecString == "vp8.0"_s || codecString == "vp9.0"_s)
            return configuration;
    }

    // The format of the 'vp09' codec string is specified in the webm GitHub repo:
    // <https://github.com/webmproject/vp9-dash/blob/master/VPCodecISOMediaFileFormatBinding.md#codecs-parameter-string>

    // "sample entry 4CC, profile, level, and bitDepth are all mandatory fields. If any of these fields are empty, or not
    // within their allowed range, the processing device SHALL treat it as an error."

    // Codec identifier: legal values are 'vp08' or 'vp09'.
    if (configuration.codecName != "vp08"_s && configuration.codecName != "vp09"_s)
        return std::nullopt;

    if (nextElement == codecSplit.end())
        return std::nullopt;

    // First element: profile. Legal values are 0-3.
    auto profile = parseInteger<uint8_t>(*nextElement);
    if (!profile || !isValidVPProfile(*profile))
        return std::nullopt;
    configuration.profile = *profile;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Second element: level. Legal values are enumerated in validVPLevels above.
    auto level = parseInteger<uint8_t>(*nextElement);
    if (!level || !isValidVPLevel(*level))
        return std::nullopt;
    configuration.level = *level;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Third element: bitDepth. Legal values are 8, 10, and 12.
    auto bitDepth = parseInteger<uint8_t>(*nextElement);
    if (!bitDepth || !isValidBitDepth(*bitDepth))
        return std::nullopt;
    configuration.bitDepth = *bitDepth;

    // "colorPrimaries, transferCharacteristics, matrixCoefficients, videoFullRangeFlag, and chromaSubsampling are OPTIONAL,
    // mutually inclusive (all or none) fields. If not specified then the processing device MUST use the values listed in the
    // table below as defaults when deciding if the device is able to decode and potentially render the video."

    if (++nextElement == codecSplit.end())
        return configuration;

    // Fourth element: chromaSubsampling. Legal values are 0-3.
    auto chromaSubsampling = parseInteger<uint8_t>(*nextElement);
    if (!chromaSubsampling || !isValidChromaSubsampling(*chromaSubsampling))
        return std::nullopt;
    configuration.chromaSubsampling = *chromaSubsampling;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Fifth element: colorPrimaries. Legal values are defined by ISO/IEC 23001-8:2016, superseded
    // by ISO/IEC 23091-2:2019.
    auto colorPrimaries = parseInteger<uint8_t>(*nextElement);
    if (!colorPrimaries || !isValidVPColorPrimaries(*colorPrimaries))
        return std::nullopt;
    configuration.colorPrimaries = *colorPrimaries;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Sixth element: transferCharacteristics. Legal values are defined by ISO/IEC 23001-8:2016, superseded
    // by ISO/IEC 23091-2:2019.
    auto transferCharacteristics = parseInteger<uint8_t>(*nextElement);
    if (!transferCharacteristics || !isValidVPTransferCharacteristics(*transferCharacteristics))
        return std::nullopt;
    configuration.transferCharacteristics = *transferCharacteristics;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Seventh element: matrixCoefficients. Legal values are defined by ISO/IEC 23001-8:2016, superseded
    // by ISO/IEC 23091-2:2019.
    auto matrixCoefficients = parseInteger<uint8_t>(*nextElement);
    if (!matrixCoefficients || !isValidVPMatrixCoefficients(*matrixCoefficients))
        return std::nullopt;
    configuration.matrixCoefficients = *matrixCoefficients;

    // "If matrixCoefficients is 0 (RGB), then chroma subsampling MUST be 3 (4:4:4)."
    if (!configuration.matrixCoefficients && configuration.chromaSubsampling != 3)
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    // Eighth element: videoFullRangeFlag. Legal values are 0 and 1.
    auto videoFullRangeFlag = parseInteger<uint8_t>(*nextElement);
    if (!videoFullRangeFlag || !isValidRange(*videoFullRangeFlag))
        return std::nullopt;
    configuration.videoFullRangeFlag = *videoFullRangeFlag;

    if (++nextElement != codecSplit.end())
        return std::nullopt;

    return configuration;
}

String createVPCodecParametersString(const VPCodecConfigurationRecord& configuration)
{
    // The format of the 'vp09' codec string is specified in the webm GitHub repo:
    // <https://github.com/webmproject/vp9-dash/blob/master/VPCodecISOMediaFileFormatBinding.md#codecs-parameter-string>
    //
    // The codecs parameter string for the VP codec family is as follows:
    //   <sample entry 4CC>.<profile>.<level>.<bitDepth>.<chromaSubsampling>.
    //   <colourPrimaries>.<transferCharacteristics>.<matrixCoefficients>.
    //   <videoFullRangeFlag>
    //  All parameter values are expressed as double-digit decimals.
    //  sample entry 4CC, profile, level, and bitDepth are all mandatory fields.

    StringBuilder resultBuilder;

    resultBuilder.append(configuration.codecName);

    if (configuration.profile > 10
        || !isValidVPProfile(configuration.profile)
        || !isValidVPLevel(configuration.level)
        || !isValidBitDepth(configuration.bitDepth)
        || !isValidChromaSubsampling(configuration.chromaSubsampling)
        || !isValidVPColorPrimaries(configuration.colorPrimaries)
        || !isValidVPTransferCharacteristics(configuration.transferCharacteristics)
        || !isValidVPMatrixCoefficients(configuration.matrixCoefficients)
        || !isValidRange(configuration.videoFullRangeFlag))
        return resultBuilder.toString();

    resultBuilder.append(".0"_s, numberToStringUnsigned<String>(configuration.profile), '.', numberToStringUnsigned<String>(configuration.level), '.');
    if (configuration.transferCharacteristics < 10)
        resultBuilder.append('0');
    resultBuilder.append(numberToStringUnsigned<String>(configuration.bitDepth));

    static NeverDestroyed<VPCodecConfigurationRecord> defaultConfiguration { };

    //  colourPrimaries, transferCharacteristics, matrixCoefficients, videoFullRangeFlag, and chromaSubsampling
    //  are OPTIONAL, mutually inclusive (all or none) fields.

    if (configuration.chromaSubsampling == defaultConfiguration->chromaSubsampling
        && configuration.videoFullRangeFlag == defaultConfiguration->videoFullRangeFlag
        && configuration.colorPrimaries == defaultConfiguration->colorPrimaries
        && configuration.transferCharacteristics == defaultConfiguration->transferCharacteristics
        && configuration.matrixCoefficients == defaultConfiguration->matrixCoefficients)
        return resultBuilder.toString();

    resultBuilder.append(".0"_s, numberToStringUnsigned<String>(configuration.chromaSubsampling), '.');
    if (configuration.colorPrimaries < 10)
        resultBuilder.append('0');
    resultBuilder.append(numberToStringUnsigned<String>(configuration.colorPrimaries), '.');
    if (configuration.transferCharacteristics < 10)
        resultBuilder.append('0');
    resultBuilder.append(numberToStringUnsigned<String>(configuration.transferCharacteristics), '.');
    if (configuration.matrixCoefficients < 10)
        resultBuilder.append('0');
    resultBuilder.append(numberToStringUnsigned<String>(configuration.matrixCoefficients), ".0"_s, numberToStringUnsigned<String>(configuration.videoFullRangeFlag));

    return resultBuilder.toString();
}

std::optional<VPCodecConfigurationRecord> createVPCodecConfigurationRecordFromVPCC(std::span<const uint8_t> vpcc)
{
    if (vpcc.size() < 12)
        return { };

    VPCodecConfigurationRecord configuration;

    BitReader reader(vpcc);

    reader.read(8); // version
    reader.read(24); // flags

    if (auto profile = reader.read(8))
        configuration.profile = *profile;
    else
        return { };
    if (auto level = reader.read(8))
        configuration.level = *level;
    else
        return { };

    if (auto bitDepth = reader.read(4))
        configuration.bitDepth = *bitDepth;
    else
        return { };

    if (auto chroma = reader.read(3))
        configuration.chromaSubsampling = *chroma;
    else
        return { };

    if (auto fullRange = reader.readBit())
        configuration.videoFullRangeFlag = *fullRange;
    else
        return { };

    if (auto colorPrimaries = reader.read(8))
        configuration.colorPrimaries = *colorPrimaries;
    else
        return { };

    if (auto transferCharacteristics = reader.read(8))
        configuration.transferCharacteristics = *transferCharacteristics;
    else
        return { };

    if (auto matrixCoefficients = reader.read(8))
        configuration.matrixCoefficients = *matrixCoefficients;
    else
        return { };

    // codecInitializationDataSize must be 0 for VP8 and VP9.
    if (auto codecInitializationDataSize = reader.read(16); codecInitializationDataSize && !*codecInitializationDataSize)
        return configuration;

    return { };
}

Vector<uint8_t> vpcCFromVPCodecConfigurationRecord(const VPCodecConfigurationRecord& record)
{
    // Ref: "VP Codec ISO Media File Format Binding, v1.0, 2017-03-31"
    // <https://www.webmproject.org/vp9/mp4/>
    //
    // class VPCodecConfigurationBox extends FullBox('vpcC', version = 1, 0)
    // {
    //     VPCodecConfigurationRecord() vpcConfig;
    // }
    //
    // aligned (8) class VPCodecConfigurationRecord {
    //     unsigned int (8)     profile;
    //     unsigned int (8)     level;
    //     unsigned int (4)     bitDepth;
    //     unsigned int (3)     chromaSubsampling;
    //     unsigned int (1)     videoFullRangeFlag;
    //     unsigned int (8)     colourPrimaries;
    //     unsigned int (8)     transferCharacteristics;
    //     unsigned int (8)     matrixCoefficients;
    //     unsigned int (16)    codecIntializationDataSize;
    //     unsigned int (8)[]   codecIntializationData;
    // }
    //
    // codecIntializationDataSize​For VP8 and VP9 this field must be 0.
    // codecIntializationData​binary codec initialization data. Not used for VP8 and VP9.

    constexpr size_t VPCodecConfigurationContentsSize = 12;

    uint32_t versionAndFlags = 1 << 24;
    uint8_t bitDepthChromaAndRange = (0xF & record.bitDepth) << 4 | (0x7 & record.chromaSubsampling) << 1 | (0x1 & record.videoFullRangeFlag);
    uint16_t codecInitializationDataSize = 0;
    auto view = JSC::DataView::create(ArrayBuffer::create(VPCodecConfigurationContentsSize, 1), 0, VPCodecConfigurationContentsSize);
    view->set(0, versionAndFlags, false);
    view->set(4, record.profile, false);
    view->set(5, record.level, false);
    view->set(6, bitDepthChromaAndRange, false);
    view->set(7, record.colorPrimaries, false);
    view->set(8, record.transferCharacteristics, false);
    view->set(9, record.matrixCoefficients, false);
    view->set(10, codecInitializationDataSize, false);
    return view->span();
}

void setConfigurationColorSpaceFromVP9ColorSpace(VPCodecConfigurationRecord& record, uint8_t colorSpace)
{
    // Derive the color information from the VP9 header color_space value, as per
    // VP9 Bitstream & Decoding Process Specification 0.6, section 7.2.2. No enum exists
    // in libwebm so reproduce the values from the specification here:
    enum class Vp9ColorSpaceValues : uint8_t {
        kCS_UNKNOWN,
        kCS_BT_601,
        kCS_BT_709,
        kCS_SMPTE_170,
        kCS_SMPTE_240,
        kCS_BT_2020,
        kCS_RESERVED,
        kCS_RGB,
    };

    // Map those "Color Space" values to primaries, transfer function, and matrices, as per
    // ISO 23091-2 (2019): Coding-independent code points
    switch (static_cast<Vp9ColorSpaceValues>(colorSpace)) {
    case Vp9ColorSpaceValues::kCS_BT_601:
        record.colorPrimaries = VPConfigurationColorPrimaries::BT_601_7;
        record.transferCharacteristics = VPConfigurationTransferCharacteristics::BT_601_7;
        record.matrixCoefficients = VPConfigurationMatrixCoefficients::BT_601_7;
        break;
    case Vp9ColorSpaceValues::kCS_BT_709:
        record.colorPrimaries = VPConfigurationColorPrimaries::BT_709_6;
        record.transferCharacteristics = VPConfigurationTransferCharacteristics::BT_709_6;
        record.matrixCoefficients = VPConfigurationMatrixCoefficients::BT_709_6;
        break;
    case Vp9ColorSpaceValues::kCS_SMPTE_170:
        record.colorPrimaries = VPConfigurationColorPrimaries::BT_601_7;
        record.transferCharacteristics = VPConfigurationTransferCharacteristics::BT_601_7;
        record.matrixCoefficients = VPConfigurationMatrixCoefficients::BT_601_7;
        break;
    case Vp9ColorSpaceValues::kCS_SMPTE_240:
        record.colorPrimaries = VPConfigurationColorPrimaries::SMPTE_ST_240;
        record.transferCharacteristics = VPConfigurationTransferCharacteristics::SMPTE_ST_240;
        record.matrixCoefficients = VPConfigurationMatrixCoefficients::SMPTE_ST_240;
        break;
    case Vp9ColorSpaceValues::kCS_BT_2020:
        // From the VP9 Bitstream documentation:
        // Note that VP9 passes the color space information in the bitstream including Rec. ITU-R BT.2020-2,
        // however, VP9 does not specify if it is in the form of “constant luminance” or “nonconstant luminance”.
        // As such, application should rely on the signaling outside of the VP9 bitstream. If there is no such
        // signaling, the application may assume nonconstant luminance for Rec. ITU-R BT.2020-2.
        record.colorPrimaries = VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance;
        record.matrixCoefficients = VPConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance;
        if (record.bitDepth <= 10)
            record.transferCharacteristics = VPConfigurationTransferCharacteristics::BT_2020_10bit;
        else
            record.transferCharacteristics = VPConfigurationTransferCharacteristics::BT_2020_12bit;
        break;
    case Vp9ColorSpaceValues::kCS_RGB:
        record.colorPrimaries = VPConfigurationColorPrimaries::BT_709_6;
        record.transferCharacteristics = VPConfigurationTransferCharacteristics::IEC_61966_2_1;
        record.matrixCoefficients = VPConfigurationMatrixCoefficients::Identity;
        break;
    case Vp9ColorSpaceValues::kCS_RESERVED:
    case Vp9ColorSpaceValues::kCS_UNKNOWN:
    default:
        record.colorPrimaries = VPConfigurationColorPrimaries::Unspecified;
        record.transferCharacteristics = VPConfigurationTransferCharacteristics::Unspecified;
        record.matrixCoefficients = VPConfigurationMatrixCoefficients::Unspecified;
        break;
    }
}

std::optional<VPCodecConfigurationRecord> vPCodecConfigurationRecordFromVPXByteStream(VPXCodec codec, std::span<const uint8_t> data)
{
    if (data.size() < 11)
        return { };

    if (codec == VPXCodec::Vp8) {
        if (!(data[0] & 1))
            return { }; // not a keyframe.
        uint8_t version = (data[0] >> 1) & 0x7;
        if (version > 3)
            return { };
        uint8_t startCodeByte0 = data[3];
        uint8_t startCodeByte1 = data[4];
        uint8_t startCodeByte2 = data[5];
        if (startCodeByte0 != 0x9d || startCodeByte1 != 0x01 || startCodeByte2 != 0x2a)
            return { }; // Not VP8

        auto colorSpaceAndClampingField = data[10];
        bool colorSpace = colorSpaceAndClampingField & 0x80;

        VPCodecConfigurationRecord record;
        record.codecName = "vp08"_s;
        record.profile = 0;
        record.level = 10;
        record.bitDepth = 8;
        record.videoFullRangeFlag = VPConfigurationRange::VideoRange;
        record.chromaSubsampling = VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
        record.colorPrimaries = colorSpace ? VPConfigurationColorPrimaries::Unspecified : VPConfigurationColorPrimaries::BT_601_7;
        record.transferCharacteristics =  colorSpace ? VPConfigurationTransferCharacteristics::Unspecified : VPConfigurationTransferCharacteristics::BT_601_7;
        record.matrixCoefficients = colorSpace ? VPConfigurationMatrixCoefficients::Unspecified : VPConfigurationMatrixCoefficients::BT_601_7;
        return record;
    }

    BitReader br(data);
    if (*br.read(2) != 2)
        return { }; // Not a valid vp9 packet header.

    auto profileLowBit = br.read(1);
    auto profileHighBit = br.read(1);
    uint8_t profile = *profileLowBit | (*profileHighBit << 1);
    if (profile == 3)
        profile += *br.read(1);

    if (auto showExistingFrame = br.read(1); *showExistingFrame)
        return { };
    if (auto frameType = br.read(1); *frameType) // NON_KEY_FRAME
        return { };

    br.read(1); // show_frame
    br.read(1); // error_resilient_mode

    auto validFrameSyncCode = [&] {
        uint8_t frameSyncByte1 = *br.read(8);
        uint8_t frameSyncByte2 = *br.read(8);
        uint8_t frameSyncByte3 = *br.read(8);
        return frameSyncByte1 == 0x49 && frameSyncByte2 == 0x83 && frameSyncByte3 == 0x42;
    };
    if (!validFrameSyncCode())
        return { };

    VPCodecConfigurationRecord record;
    record.codecName = "vp09"_s;
    record.profile = profile;

    if (profile >= 2)
        record.bitDepth = *br.read(1) ? 12 : 10;
    uint8_t colorSpace = *br.read(3);
    bool subsamplingX = true;
    bool subsamplingY = true;
    if (colorSpace != 7 /* CS_RGB */) {
        record.videoFullRangeFlag = *br.read(1) ? VPConfigurationRange::FullRange : VPConfigurationRange::VideoRange;
        if (profile == 1 || profile == 3) {
            subsamplingX = *br.read(1);
            subsamplingY = *br.read(1);
            br.read(1); // reserved_zero
        }
    } else {
        record.videoFullRangeFlag = VPConfigurationRange::FullRange;
        if (profile == 1 || profile == 3) {
            subsamplingX = subsamplingY = false;
            br.read(1); // reserved_zero
        }
    }
    record.chromaSubsampling = [&] {
        if (subsamplingX & subsamplingY)
            return VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
        if (subsamplingX & !subsamplingY)
            return VPConfigurationChromaSubsampling::Subsampling_422;
        if (!subsamplingX & !subsamplingY)
            return VPConfigurationChromaSubsampling::Subsampling_444;
        // This indicates 4:4:0 subsampling, which is not expressable in the 'vpcC' box. Default to 4:2:0.
        return VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
    }();

    setConfigurationColorSpaceFromVP9ColorSpace(record, colorSpace);

    return record;
}

static PlatformVideoColorPrimaries convertToPlatformVideoColorPrimaries(uint8_t primaries)
{
    switch (primaries) {
    case VPConfigurationColorPrimaries::BT_709_6:
        return PlatformVideoColorPrimaries::Bt709;
    case VPConfigurationColorPrimaries::BT_470_6_M:
        return PlatformVideoColorPrimaries::Bt470m;
    case VPConfigurationColorPrimaries::BT_470_7_BG:
        return PlatformVideoColorPrimaries::Bt470bg;
    case VPConfigurationColorPrimaries::BT_601_7:
        return PlatformVideoColorPrimaries::Smpte170m;
    case VPConfigurationColorPrimaries::SMPTE_ST_240:
        return PlatformVideoColorPrimaries::Smpte240m;
    case VPConfigurationColorPrimaries::Film:
        return PlatformVideoColorPrimaries::Film;
    case VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance:
        return PlatformVideoColorPrimaries::Bt2020;
    case VPConfigurationColorPrimaries::SMPTE_ST_428_1:
        return PlatformVideoColorPrimaries::SmpteSt4281;
    case VPConfigurationColorPrimaries::SMPTE_RP_431_2:
        return PlatformVideoColorPrimaries::SmpteRp431;
    case VPConfigurationColorPrimaries::SMPTE_EG_432_1:
        return PlatformVideoColorPrimaries::SmpteEg432;
    case VPConfigurationColorPrimaries::EBU_Tech_3213_E:
        return PlatformVideoColorPrimaries::JedecP22Phosphors;
    case VPConfigurationColorPrimaries::Unspecified:
    default:
        return PlatformVideoColorPrimaries::Unspecified;
    }
}

static PlatformVideoTransferCharacteristics convertToPlatformVideoTransferCharacteristics(uint8_t characteristics)
{
    switch (characteristics) {
    case VPConfigurationTransferCharacteristics::BT_709_6:
        return PlatformVideoTransferCharacteristics::Bt709;
    case VPConfigurationTransferCharacteristics::BT_470_6_M:
        return PlatformVideoTransferCharacteristics::Gamma22curve;
    case VPConfigurationTransferCharacteristics::BT_470_7_BG:
        return PlatformVideoTransferCharacteristics::Gamma28curve;
    case VPConfigurationTransferCharacteristics::BT_601_7:
        return PlatformVideoTransferCharacteristics::Smpte170m;
    case VPConfigurationTransferCharacteristics::SMPTE_ST_240:
        return PlatformVideoTransferCharacteristics::Smpte240m;
    case VPConfigurationTransferCharacteristics::Linear:
        return PlatformVideoTransferCharacteristics::Linear;
    case VPConfigurationTransferCharacteristics::Logrithmic:
        return PlatformVideoTransferCharacteristics::Log;
    case VPConfigurationTransferCharacteristics::Logrithmic_Sqrt:
        return PlatformVideoTransferCharacteristics::LogSqrt;
    case VPConfigurationTransferCharacteristics::IEC_61966_2_4:
        return PlatformVideoTransferCharacteristics::Iec6196624;
    case VPConfigurationTransferCharacteristics::BT_1361_0:
        return PlatformVideoTransferCharacteristics::Bt1361ExtendedColourGamut;
    case VPConfigurationTransferCharacteristics::IEC_61966_2_1:
        return PlatformVideoTransferCharacteristics::Iec6196621;
    case VPConfigurationTransferCharacteristics::BT_2020_10bit:
        return PlatformVideoTransferCharacteristics::Bt2020_10bit;
    case VPConfigurationTransferCharacteristics::BT_2020_12bit:
        return PlatformVideoTransferCharacteristics::Bt2020_12bit;
    case VPConfigurationTransferCharacteristics::SMPTE_ST_2084:
        return PlatformVideoTransferCharacteristics::SmpteSt2084;
    case VPConfigurationTransferCharacteristics::SMPTE_ST_428_1:
        return PlatformVideoTransferCharacteristics::SmpteSt4281;
    case VPConfigurationTransferCharacteristics::BT_2100_HLG:
        return PlatformVideoTransferCharacteristics::AribStdB67Hlg;
    case VPConfigurationTransferCharacteristics::Unspecified:
    default:
        return PlatformVideoTransferCharacteristics::Unspecified;
    }
}

static PlatformVideoMatrixCoefficients convertToPlatformVideoMatrixCoefficients(uint8_t coefficients)
{
    switch (coefficients) {
    case VPConfigurationMatrixCoefficients::Identity:
        return PlatformVideoMatrixCoefficients::Rgb;
    case VPConfigurationMatrixCoefficients::BT_709_6:
        return PlatformVideoMatrixCoefficients::Bt709;
    case VPConfigurationMatrixCoefficients::FCC:
        return PlatformVideoMatrixCoefficients::Fcc;
    case VPConfigurationMatrixCoefficients::BT_470_7_BG:
        return PlatformVideoMatrixCoefficients::Bt470bg;
    case VPConfigurationMatrixCoefficients::BT_601_7:
        return PlatformVideoMatrixCoefficients::Smpte170m;
    case VPConfigurationMatrixCoefficients::SMPTE_ST_240:
        return PlatformVideoMatrixCoefficients::Smpte240m;
    case VPConfigurationMatrixCoefficients::YCgCo:
        return PlatformVideoMatrixCoefficients::YCgCo;
    case VPConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance:
        return PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance;
    case VPConfigurationMatrixCoefficients::BT_2020_Constant_Luminance:
        return PlatformVideoMatrixCoefficients::Bt2020ConstantLuminance;
    case VPConfigurationMatrixCoefficients::Unspecified:
    default:
        return PlatformVideoMatrixCoefficients::Unspecified;
    }
}

PlatformVideoColorSpace colorSpaceFromVPCodecConfigurationRecord(const VPCodecConfigurationRecord& record)
{
    return {
        .primaries = convertToPlatformVideoColorPrimaries(record.colorPrimaries),
        .transfer = convertToPlatformVideoTransferCharacteristics(record.transferCharacteristics),
        .matrix = convertToPlatformVideoMatrixCoefficients(record.matrixCoefficients),
        .fullRange = record.videoFullRangeFlag == VPConfigurationRange::FullRange
    };
}

} // namespace WebCore
