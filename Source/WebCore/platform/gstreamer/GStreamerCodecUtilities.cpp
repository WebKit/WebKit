/*
 * Copyright (C) 2022 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerCodecUtilities.h"

#if USE(GSTREAMER)

#include "AV1Utilities.h"
#include "HEVCUtilities.h"
#include "VP9Utilities.h"
#include <gst/pbutils/codec-utils.h>
#include <gst/video/video.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

GST_DEBUG_CATEGORY(webkit_gst_codec_utilities_debug);
#define GST_CAT_DEFAULT webkit_gst_codec_utilities_debug

namespace WebCore {

static void ensureDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_gst_codec_utilities_debug, "webkitcodec", 0, "WebKit Codecs Utilities");
    });
}

std::pair<const char*, const char*> GStreamerCodecUtilities::parseH264ProfileAndLevel(const String& codec)
{
    ensureDebugCategoryInitialized();

    auto components = codec.split('.');
    long int spsAsInteger = strtol(components[1].utf8().data(), nullptr, 16);

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
    uint8_t sps[3];
    sps[0] = spsAsInteger >> 16;
    sps[1] = spsAsInteger >> 8;
    sps[2] = spsAsInteger;
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    const char* profile = gst_codec_utils_h264_get_profile(sps, 3);
    const char* level = gst_codec_utils_h264_get_level(sps, 3);

    // To avoid going through a class hierarchy for such a simple
    // string conversion, we use a little trick here: See
    // https://bugs.webkit.org/show_bug.cgi?id=201870.
    char levelAsStringFallback[2] = { '\0', '\0' };
    if (!level && sps[2] > 0 && sps[2] <= 5) {
        levelAsStringFallback[0] = static_cast<char>('0' + sps[2]);
        level = levelAsStringFallback;
    }

    GST_DEBUG("Codec %s translates to H.264 profile %s and level %s", codec.utf8().data(), GST_STR_NULL(profile), GST_STR_NULL(level));
    return { profile, level };
}

static std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> h264CapsFromCodecString(const String& codecString)
{
    auto outputCaps = adoptGRef(gst_caps_new_empty_simple("video/x-h264"));
    auto [gstProfile, level] = GStreamerCodecUtilities::parseH264ProfileAndLevel(codecString);
    if (gstProfile)
        gst_caps_set_simple(outputCaps.get(), "profile", G_TYPE_STRING, gstProfile, nullptr);
    if (level)
        gst_caps_set_simple(outputCaps.get(), "level", G_TYPE_STRING, level, nullptr);

    StringBuilder formatBuilder;
    auto profile = StringView::fromLatin1(gstProfile);
    auto isY444 = profile.findIgnoringASCIICase("high-4:4:4"_s) != notFound;
    auto isY422 = profile.findIgnoringASCIICase("high-4:2:2"_s) != notFound;
    auto isY420 = profile.findIgnoringASCIICase("high-10"_s) != notFound;
    if (isY444)
        formatBuilder.append("Y444"_s);
    else if (isY422)
        formatBuilder.append("I422"_s);
    else if (isY420)
        formatBuilder.append("Y420"_s);
    else
        formatBuilder.append("I420"_s);

    if (isY444 || isY422 || isY420) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        auto endianness = "LE"_s;
#else
        auto endianness = "BE"_s;
#endif
        formatBuilder.append("_10"_s, endianness);
    }

    auto pixelFormat = formatBuilder.toString();
    GST_DEBUG("Setting pixel format %s for profile %s", pixelFormat.ascii().data(), gstProfile);
    auto inputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, pixelFormat.ascii().data(), nullptr));
    return { inputCaps, outputCaps };
}

const char* GStreamerCodecUtilities::parseHEVCProfile(const String& codec)
{
    ensureDebugCategoryInitialized();

    GST_DEBUG("Parsing HEVC codec string: %s", codec.ascii().data());
    auto parameters = parseHEVCCodecParameters(codec);
    if (!parameters) {
        GST_WARNING("Invalid HEVC codec: %s", codec.ascii().data());
        return nullptr;
    }

    if (parameters->generalProfileSpace > 3) {
        GST_WARNING("Invalid general_profile_space: %u", parameters->generalProfileSpace);
        return nullptr;
    }

    if (parameters->generalProfileIDC > 0x1F) {
        GST_WARNING("Invalid general_profile_idc: %u", parameters->generalProfileIDC);
        return nullptr;
    }

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
    uint8_t profileTierLevel[11] = { 0, };
    memset(profileTierLevel, 0, 11);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    profileTierLevel[0] = parameters->generalProfileIDC;

    if (profileTierLevel[0] >= 4) {
        auto& constraints = parameters->generalConstraintIndicatorFlags;
        for (unsigned i = 5, j = 0; i < 10; i++, j++)
            profileTierLevel[i] = constraints[j];
    }

    return gst_codec_utils_h265_get_profile(profileTierLevel, sizeof(profileTierLevel));
}

static std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> h265CapsFromCodecString(const String& codecString)
{
    auto outputCaps = adoptGRef(gst_caps_new_empty_simple("video/x-h265"));
    auto gstProfile = GStreamerCodecUtilities::parseHEVCProfile(codecString);
    if (gstProfile)
        gst_caps_set_simple(outputCaps.get(), "profile", G_TYPE_STRING, gstProfile, nullptr);

    StringBuilder formatBuilder;
    auto profile = StringView::fromLatin1(gstProfile);
    auto isY444 = profile.findIgnoringASCIICase("-444"_s) != notFound;
    auto isY422 = profile.findIgnoringASCIICase("-422"_s) != notFound;
    if (isY444)
        formatBuilder.append("Y444"_s);
    else if (isY422)
        formatBuilder.append("I422"_s);
    else
        formatBuilder.append("I420"_s);

    if (isY444 || isY422) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        auto endianness = "LE"_s;
#else
        auto endianness = "BE"_s;
#endif
        auto is12Bits = profile.findIgnoringASCIICase("-12"_s) != notFound;
        auto is10Bits = profile.findIgnoringASCIICase("-10"_s) != notFound;
        if (is10Bits)
            formatBuilder.append("_10"_s, endianness);
        else if (is12Bits)
            formatBuilder.append("_12"_s, endianness);
    }

    auto pixelFormat = formatBuilder.toString();
    GST_DEBUG("Setting pixel format %s for profile %s", pixelFormat.ascii().data(), gstProfile);
    auto inputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, pixelFormat.ascii().data(), nullptr));
    return { inputCaps, outputCaps };
}

static std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> vpxCapsFromCodecString(const String& codecString)
{
    auto parameters = parseVPCodecParameters(codecString);
    if (!parameters)
        return { nullptr, nullptr };

    auto inputCaps = adoptGRef(gst_caps_new_any());

    if (parameters->codecName.startsWith("vp8"_s) || parameters->codecName.startsWith("vp08"_s))
        return { inputCaps, adoptGRef(gst_caps_new_empty_simple("video/x-vp8")) };

    auto outputCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp9"));
    auto profile = makeString(parameters->profile);
    gst_caps_set_simple(outputCaps.get(), "profile", G_TYPE_STRING, profile.ascii().data(), nullptr);
    auto yuvFormat = "I420"_s;
    if (parameters->chromaSubsampling == VPConfigurationChromaSubsampling::Subsampling_422)
        yuvFormat = "I422"_s;
    else if (parameters->chromaSubsampling == VPConfigurationChromaSubsampling::Subsampling_444)
        yuvFormat = "Y444"_s;
    StringBuilder formatBuilder;
    formatBuilder.append(yuvFormat);
    if (parameters->bitDepth > 8 || parameters->profile == 2) {
        if (parameters->bitDepth > 8)
            formatBuilder.append('_', parameters->bitDepth);
        else
            formatBuilder.append("_10"_s);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        formatBuilder.append("LE"_s);
#else
        formatBuilder.append("BE"_s);
#endif
    }

    auto formatString = formatBuilder.toString();
    auto format = gst_video_format_from_string(formatString.ascii().data());
    GstVideoInfo info;
    // Setting a random size here, it is overridden later on.
    gst_video_info_set_format(&info, format, 320, 240);

    if (parameters->videoFullRangeFlag == VPConfigurationRange::FullRange)
        GST_VIDEO_INFO_COLORIMETRY(&info).range = GST_VIDEO_COLOR_RANGE_0_255;
    else
        GST_VIDEO_INFO_COLORIMETRY(&info).range = GST_VIDEO_COLOR_RANGE_16_235;

    auto primaries = parameters->colorPrimaries;
    if (primaries == VPConfigurationColorPrimaries::BT_709_6)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
    else if (primaries == VPConfigurationColorPrimaries::BT_470_6_M)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT470M;
    else if (primaries == VPConfigurationColorPrimaries::BT_470_7_BG || primaries == VPConfigurationColorPrimaries::BT_601_7)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT470BG;
    else if (primaries == VPConfigurationColorPrimaries::SMPTE_ST_240)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE240M;
    else if (primaries == VPConfigurationColorPrimaries::Film)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_FILM;
    else if (primaries == VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
    else if (primaries == VPConfigurationColorPrimaries::SMPTE_ST_428_1)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTEST428;
    else if (primaries == VPConfigurationColorPrimaries::SMPTE_RP_431_2)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTERP431;
    else if (primaries == VPConfigurationColorPrimaries::SMPTE_EG_432_1)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432;
    else if (primaries == VPConfigurationColorPrimaries::EBU_Tech_3213_E)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_EBU3213;
    else if (primaries == VPConfigurationColorPrimaries::Unspecified)
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;

    auto transfer = parameters->transferCharacteristics;
    if (transfer == VPConfigurationTransferCharacteristics::BT_709_6 || transfer == VPConfigurationTransferCharacteristics::BT_470_6_M || transfer == VPConfigurationTransferCharacteristics::BT_1361_0)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT709;
    else if (transfer == VPConfigurationTransferCharacteristics::BT_470_7_BG)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_GAMMA28;
    else if (transfer == VPConfigurationTransferCharacteristics::BT_601_7)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT601;
    else if (transfer == VPConfigurationTransferCharacteristics::SMPTE_ST_240)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_SMPTE240M;
    else if (transfer == VPConfigurationTransferCharacteristics::Linear)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_GAMMA10;
    else if (transfer == VPConfigurationTransferCharacteristics::Logrithmic)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_LOG100;
    else if (transfer == VPConfigurationTransferCharacteristics::Logrithmic_Sqrt)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_LOG316;
    else if (transfer == VPConfigurationTransferCharacteristics::IEC_61966_2_4)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_SRGB;
    else if (transfer == VPConfigurationTransferCharacteristics::IEC_61966_2_1) {
        GST_WARNING("VPConfigurationTransferCharacteristics::IEC_61966_2_1 not supported");
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_UNKNOWN;
    } else if (transfer == VPConfigurationTransferCharacteristics::BT_2020_10bit)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT2020_10;
    else if (transfer == VPConfigurationTransferCharacteristics::BT_2020_12bit)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT2020_12;
    else if (transfer == VPConfigurationTransferCharacteristics::SMPTE_ST_2084)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_SMPTE2084;
    else if (transfer == VPConfigurationTransferCharacteristics::SMPTE_ST_428_1) {
        GST_WARNING("VPConfigurationTransferCharacteristics::SMPTE_ST_428_1 not supported");
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_UNKNOWN;
    } else if (transfer == VPConfigurationTransferCharacteristics::BT_2100_HLG)
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;

    auto matrix = parameters->matrixCoefficients;
    if (matrix == VPConfigurationMatrixCoefficients::Identity)
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_RGB;
    else if (matrix == VPConfigurationMatrixCoefficients::BT_709_6)
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_BT709;
    else if (matrix == VPConfigurationMatrixCoefficients::Unspecified)
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;
    else if (matrix == VPConfigurationMatrixCoefficients::FCC)
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_FCC;
    else if (matrix == VPConfigurationMatrixCoefficients::BT_601_7)
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_BT601;
    else if (matrix == VPConfigurationMatrixCoefficients::SMPTE_ST_240)
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_SMPTE240M;
    else if (matrix == VPConfigurationMatrixCoefficients::BT_2020_Constant_Luminance)
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
    else {
        GST_WARNING("Color matrix not supported: %u", matrix);
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;
    }
    inputCaps = adoptGRef(gst_video_info_to_caps(&info));

    return { inputCaps, outputCaps };
}

static std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> av1CapsFromCodecString(const String& codecString)
{
    auto configurationRecord = parseAV1CodecParameters(codecString);
    if (!configurationRecord)
        return { nullptr, nullptr };

    const char* profile;
    switch (configurationRecord->profile) {
    case AV1ConfigurationProfile::Main:
        profile = "main";
        break;
    case AV1ConfigurationProfile::High:
        profile = "high";
        break;
    case AV1ConfigurationProfile::Professional:
        profile = "professional";
        break;
    }

    auto outputCaps = adoptGRef(gst_caps_new_simple("video/x-av1", "profile", G_TYPE_STRING, profile, "bit-depth-luma", G_TYPE_UINT, configurationRecord->bitDepth, "bit-depth-chroma", G_TYPE_UINT, configurationRecord->bitDepth, nullptr));

    const char* chromaFormat = nullptr;
    auto yuvFormat = "I420"_s;
    switch (static_cast<AV1ConfigurationChromaSubsampling>(configurationRecord->chromaSubsampling)) {
    case AV1ConfigurationChromaSubsampling::Subsampling_422:
        yuvFormat = "I422"_s;
        chromaFormat = "4:2:2";
        break;
    case AV1ConfigurationChromaSubsampling::Subsampling_444:
        yuvFormat = "Y444"_s;
        chromaFormat = "4:4:4";
        break;
    case AV1ConfigurationChromaSubsampling::Subsampling_420_Unknown:
    case AV1ConfigurationChromaSubsampling::Subsampling_420_Vertical:
    case AV1ConfigurationChromaSubsampling::Subsampling_420_Colocated:
        if (configurationRecord->monochrome)
            chromaFormat = "4:0:0";
        else
            chromaFormat = "4:2:0";
        break;
    };

    StringBuilder formatBuilder;
    formatBuilder.append(yuvFormat);
    if (configurationRecord->bitDepth > 8) {
        formatBuilder.append('_', configurationRecord->bitDepth);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        formatBuilder.append("LE"_s);
#else
        formatBuilder.append("BE"_s);
#endif
    }

    auto formatString = formatBuilder.toString();
    auto format = gst_video_format_from_string(formatString.ascii().data());
    GstVideoInfo info;
    // Setting a random size here, it is overridden later on.
    gst_video_info_set_format(&info, format, 320, 240);

    if (configurationRecord->videoFullRangeFlag == AV1ConfigurationRange::FullRange)
        GST_VIDEO_INFO_COLORIMETRY(&info).range = GST_VIDEO_COLOR_RANGE_0_255;
    else
        GST_VIDEO_INFO_COLORIMETRY(&info).range = GST_VIDEO_COLOR_RANGE_16_235;

    switch (static_cast<AV1ConfigurationColorPrimaries>(configurationRecord->colorPrimaries)) {
    case AV1ConfigurationColorPrimaries::BT_709_6:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
        break;
    case AV1ConfigurationColorPrimaries::Unspecified:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
        break;
    case AV1ConfigurationColorPrimaries::BT_470_6_M:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT470M;
        break;
    case AV1ConfigurationColorPrimaries::BT_470_7_BG:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT470BG;
        break;
    case AV1ConfigurationColorPrimaries::BT_601_7:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
        break;
    case AV1ConfigurationColorPrimaries::SMPTE_ST_240:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE240M;
        break;
    case AV1ConfigurationColorPrimaries::Film:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_FILM;
        break;
    case AV1ConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
        break;
    case AV1ConfigurationColorPrimaries::SMPTE_ST_428_1:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTEST428;
        break;
    case AV1ConfigurationColorPrimaries::SMPTE_RP_431_2:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTERP431;
        break;
    case AV1ConfigurationColorPrimaries::SMPTE_EG_432_1:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432;
        break;
    case AV1ConfigurationColorPrimaries::EBU_Tech_3213_E:
        GST_VIDEO_INFO_COLORIMETRY(&info).primaries = GST_VIDEO_COLOR_PRIMARIES_EBU3213;
        break;
    };

    switch (static_cast<AV1ConfigurationTransferCharacteristics>(configurationRecord->transferCharacteristics)) {
    case AV1ConfigurationTransferCharacteristics::BT_709_6:
    case AV1ConfigurationTransferCharacteristics::BT_470_6_M:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT709;
        break;
    case AV1ConfigurationTransferCharacteristics::Unspecified:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_UNKNOWN;
        break;
    case AV1ConfigurationTransferCharacteristics::BT_470_7_BG:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_GAMMA28;
        break;
    case AV1ConfigurationTransferCharacteristics::BT_601_7:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT601;
        break;
    case AV1ConfigurationTransferCharacteristics::SMPTE_ST_240:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_SMPTE240M;
        break;
    case AV1ConfigurationTransferCharacteristics::Linear:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_GAMMA10;
        break;
    case AV1ConfigurationTransferCharacteristics::Logrithmic:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_LOG100;
        break;
    case AV1ConfigurationTransferCharacteristics::Logrithmic_Sqrt:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_LOG316;
        break;
    case AV1ConfigurationTransferCharacteristics::IEC_61966_2_4:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_SRGB;
        break;
    case AV1ConfigurationTransferCharacteristics::BT_1361_0:
        break;
    case AV1ConfigurationTransferCharacteristics::IEC_61966_2_1:
        GST_WARNING("AV1ConfigurationTransferCharacteristics::IEC_61966_2_1 not supported");
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_UNKNOWN;
        break;
    case AV1ConfigurationTransferCharacteristics::BT_2020_10bit:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT2020_10;
        break;
    case AV1ConfigurationTransferCharacteristics::BT_2020_12bit:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_BT2020_12;
        break;
    case AV1ConfigurationTransferCharacteristics::SMPTE_ST_2084:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_SMPTE2084;
        break;
    case AV1ConfigurationTransferCharacteristics::SMPTE_ST_428_1:
        GST_WARNING("AV1ConfigurationTransferCharacteristics::SMPTE_ST_428_1 not supported");
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_UNKNOWN;
        break;
    case AV1ConfigurationTransferCharacteristics::BT_2100_HLG:
        GST_VIDEO_INFO_COLORIMETRY(&info).transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;
        break;
    };

    switch (static_cast<AV1ConfigurationMatrixCoefficients>(configurationRecord->matrixCoefficients)) {
    case AV1ConfigurationMatrixCoefficients::Identity:
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_RGB;
        break;
    case AV1ConfigurationMatrixCoefficients::BT_709_6:
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_BT709;
        break;
    case AV1ConfigurationMatrixCoefficients::Unspecified:
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;
        break;
    case AV1ConfigurationMatrixCoefficients::FCC:
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_FCC;
        break;
    case AV1ConfigurationMatrixCoefficients::BT_470_7_BG:
        break;
    case AV1ConfigurationMatrixCoefficients::BT_601_7:
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_BT601;
        break;
    case AV1ConfigurationMatrixCoefficients::SMPTE_ST_240:
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_SMPTE240M;
        break;
    case AV1ConfigurationMatrixCoefficients::YCgCo:
        break;
    case AV1ConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance:
    case AV1ConfigurationMatrixCoefficients::BT_2020_Constant_Luminance:
        GST_VIDEO_INFO_COLORIMETRY(&info).matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
        break;
    case AV1ConfigurationMatrixCoefficients::SMPTE_ST_2085:
        break;
    case AV1ConfigurationMatrixCoefficients::Chromacity_Nonconstant_Luminance:
        break;
    case AV1ConfigurationMatrixCoefficients::Chromacity_Constant_Luminance:
        break;
    case AV1ConfigurationMatrixCoefficients::BT_2100_ICC:
        break;
    };

    if (!gst_video_colorimetry_matches(&GST_VIDEO_INFO_COLORIMETRY(&info), GST_VIDEO_COLORIMETRY_SRGB))
        gst_caps_set_simple(outputCaps.get(), "chroma-format", G_TYPE_STRING, chromaFormat, nullptr);

    auto inputCaps = adoptGRef(gst_video_info_to_caps(&info));
    GST_DEBUG("Codec %s maps to input %" GST_PTR_FORMAT " and output %" GST_PTR_FORMAT, codecString.ascii().data(), inputCaps.get(), outputCaps.get());
    return { inputCaps, outputCaps };
}

std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> GStreamerCodecUtilities::capsFromCodecString(const String& codecString, std::optional<IntSize> size, std::optional<double> frameRate)
{
    ensureDebugCategoryInitialized();

    auto completeCaps = [&](GRefPtr<GstCaps>&& caps) -> GRefPtr<GstCaps> {
        if (!caps)
            return nullptr;

        if (frameRate) {
            int framerateNumerator, framerateDenominator;
            gst_util_double_to_fraction(*frameRate, &framerateNumerator, &framerateDenominator);
            gst_caps_set_simple(caps.get(), "framerate", GST_TYPE_FRACTION, framerateNumerator, framerateDenominator, nullptr);
        } else if (!gst_caps_is_any(caps.get()) && !gst_caps_is_empty(caps.get())) {
            auto structure = gst_caps_get_structure(caps.get(), 0);
            gst_structure_remove_field(structure, "framerate");
        }

        if (size)
            gst_caps_set_simple(caps.get(), "width", G_TYPE_INT, size->width(), "height", G_TYPE_INT, size->height(), nullptr);
        else if (!gst_caps_is_any(caps.get()) && !gst_caps_is_empty(caps.get())) {
            auto structure = gst_caps_get_structure(caps.get(), 0);
            gst_structure_remove_fields(structure, "width", "height", nullptr);
        }
        return caps;
    };

    if (codecString.startsWith("vp8"_s) || codecString.startsWith("vp08"_s) || codecString.startsWith("vp9"_s) || codecString.startsWith("vp09"_s)) {
        auto [inputCaps, outputCaps] = vpxCapsFromCodecString(codecString);
        return { completeCaps(WTFMove(inputCaps)), completeCaps(WTFMove(outputCaps)) };
    }

    if (codecString.startsWith("av01"_s)) {
        auto [inputCaps, outputCaps] = av1CapsFromCodecString(codecString);
        return { completeCaps(WTFMove(inputCaps)), completeCaps(WTFMove(outputCaps)) };
    }

    if (codecString.startsWith("avc1"_s)) {
        auto [inputCaps, outputCaps] = h264CapsFromCodecString(codecString);
        return { completeCaps(WTFMove(inputCaps)), completeCaps(WTFMove(outputCaps)) };
    }

    if (codecString.startsWith("hvc1"_s) || codecString.startsWith("hev1"_s)) {
        auto [inputCaps, outputCaps] = h265CapsFromCodecString(codecString);
        return { completeCaps(WTFMove(inputCaps)), completeCaps(WTFMove(outputCaps)) };
    }

    return { nullptr, nullptr };
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
