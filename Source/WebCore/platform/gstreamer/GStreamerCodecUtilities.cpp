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

#include "HEVCUtilities.h"
#include "VP9Utilities.h"
#include <gst/pbutils/codec-utils.h>
#include <gst/video/video.h>
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
    uint8_t sps[3];
    sps[0] = spsAsInteger >> 16;
    sps[1] = spsAsInteger >> 8;
    sps[2] = spsAsInteger;

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

    uint8_t profileTierLevel[11] = { 0, };
    memset(profileTierLevel, 0, 11);
    profileTierLevel[0] = parameters->generalProfileIDC;

    if (profileTierLevel[0] >= 4) {
        auto& constraints = parameters->generalConstraintIndicatorFlags;
        for (unsigned i = 5, j = 0; i < 10; i++, j++)
            profileTierLevel[i] = constraints[j];
    }

    return gst_codec_utils_h265_get_profile(profileTierLevel, sizeof(profileTierLevel));
}

static std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> vpxCapsFromCodecString(const String& codecString, unsigned width, unsigned height, int framerateNumerator, int framerateDenominator)
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
    const char* yuvFormat = "I420";
    if (parameters->chromaSubsampling == VPConfigurationChromaSubsampling::Subsampling_422)
        yuvFormat = "I422";
    else if (parameters->chromaSubsampling == VPConfigurationChromaSubsampling::Subsampling_444)
        yuvFormat = "Y444";
    StringBuilder formatBuilder;
    formatBuilder.append(yuvFormat);
    if (parameters->bitDepth > 8) {
        formatBuilder.append('_', parameters->bitDepth);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        formatBuilder.append("LE"_s);
#else
        formatBuilder.append("BE"_s);
#endif
    }

    auto formatString = formatBuilder.toString();
    auto format = gst_video_format_from_string(formatString.ascii().data());
    GstVideoInfo info;
    gst_video_info_set_format(&info, format, width, height);

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

    gst_caps_set_simple(inputCaps.get(), "framerate", GST_TYPE_FRACTION, framerateNumerator, framerateDenominator, nullptr);

    return { inputCaps, outputCaps };
}

std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> GStreamerCodecUtilities::capsFromCodecString(const String& codecString, unsigned width, unsigned height, int framerateNumerator, int framerateDenominator)
{
    if (codecString.startsWith("vp8"_s) || codecString.startsWith("vp08"_s) || codecString.startsWith("vp9"_s) || codecString.startsWith("vp09"_s))
        return vpxCapsFromCodecString(codecString, width, height, framerateNumerator, framerateDenominator);

    if (codecString.startsWith("avc1"_s))
        return { nullptr, nullptr };
    return { nullptr, nullptr };
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
