/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "VP9UtilitiesCocoa.h"

#if ENABLE(VP9) && PLATFORM(COCOA)

#import "FourCC.h"
#import "LibWebRTCProvider.h"
#import "MediaCapabilitiesInfo.h"
#import "MediaSample.h"
#import "PlatformScreen.h"
#import "ScreenProperties.h"
#import "SharedBuffer.h"
#import "SystemBattery.h"
#import "VideoConfiguration.h"
#import <JavaScriptCore/DataView.h>
#import <webm/vp9_header_parser.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"

namespace WebCore {

using namespace webm;

VP9TestingOverrides& VP9TestingOverrides::singleton()
{
    static NeverDestroyed<VP9TestingOverrides> instance;
    return instance;
}

void VP9TestingOverrides::setHardwareDecoderDisabled(std::optional<bool>&& disabled)
{
    m_hardwareDecoderDisabled = WTFMove(disabled);
    if (m_configurationChangedCallback)
        m_configurationChangedCallback();
}

void VP9TestingOverrides::setVP9DecoderDisabled(std::optional<bool>&& disabled)
{
    m_vp9DecoderDisabled = WTFMove(disabled);
    if (m_configurationChangedCallback)
        m_configurationChangedCallback();
}

void VP9TestingOverrides::setVP9ScreenSizeAndScale(std::optional<ScreenDataOverrides>&& overrides)
{
    m_screenSizeAndScale = WTFMove(overrides);
    if (m_configurationChangedCallback)
        m_configurationChangedCallback();
}

void VP9TestingOverrides::setConfigurationChangedCallback(std::function<void()>&& callback)
{
    m_configurationChangedCallback = WTFMove(callback);
}

enum class ResolutionCategory : uint8_t {
    R_480p,
    R_720p,
    R_1080p,
    R_2K,
    R_4K,
    R_8K,
    R_12K,
};

static ResolutionCategory resolutionCategory(const FloatSize& size)
{
    auto pixels = size.area();
    if (pixels > 7680 * 4320)
        return ResolutionCategory::R_12K;
    if (pixels > 4096 * 2160)
        return ResolutionCategory::R_8K;
    if (pixels > 2048 * 1080)
        return ResolutionCategory::R_4K;
    if (pixels > 1920 * 1080)
        return ResolutionCategory::R_2K;
    if (pixels > 1280 * 720)
        return ResolutionCategory::R_1080p;
    if (pixels > 640 * 480)
        return ResolutionCategory::R_720p;
    return ResolutionCategory::R_480p;
}

void registerWebKitVP9Decoder()
{
    LibWebRTCProvider::registerWebKitVP9Decoder();
}

void registerWebKitVP8Decoder()
{
    LibWebRTCProvider::registerWebKitVP8Decoder();
}

void registerSupplementalVP9Decoder()
{
    if (!VideoToolboxLibrary(true))
        return;

    if (canLoad_VideoToolbox_VTRegisterSupplementalVideoDecoderIfAvailable())
        softLink_VideoToolbox_VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);
}

bool isVP9DecoderAvailable()
{
    if (auto disabledForTesting = VP9TestingOverrides::singleton().vp9DecoderDisabled())
        return !*disabledForTesting;

#if PLATFORM(IOS)
    return canLoad_VideoToolbox_VTIsHardwareDecodeSupported() && VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9);
#else
    if (!VideoToolboxLibrary(true))
        return false;
    return noErr == VTSelectAndCreateVideoDecoderInstance(kCMVideoCodecType_VP9, kCFAllocatorDefault, nullptr, nullptr);
#endif
}

bool isVP8DecoderAvailable()
{
    if (!VideoToolboxLibrary(true))
        return false;
    return noErr == VTSelectAndCreateVideoDecoderInstance('vp08', kCFAllocatorDefault, nullptr, nullptr);
}

bool vp9HardwareDecoderAvailable()
{
    if (auto disabledForTesting = VP9TestingOverrides::singleton().hardwareDecoderDisabled())
        return !*disabledForTesting;

    return canLoad_VideoToolbox_VTIsHardwareDecodeSupported() && VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9);
}

static bool isVP9CodecConfigurationRecordSupported(const VPCodecConfigurationRecord& codecConfiguration)
{
    if (!isVP9DecoderAvailable())
        return false;

    // HW & SW VP9 Decoders support Profile 0 & 2:
    if (codecConfiguration.profile && codecConfiguration.profile != 2)
        return false;

    // HW & SW VP9 Decoders support only 420 chroma subsampling:
    if (codecConfiguration.chromaSubsampling > VPConfigurationChromaSubsampling::Subsampling_420_Colocated)
        return false;

    // HW & SW VP9 Decoders support 8 or 10 bit color:
    if (codecConfiguration.bitDepth > 10)
        return false;

    // HW & SW VP9 Decoders support up to Level 6:
    if (codecConfiguration.level > VPConfigurationLevel::Level_6)
        return false;

    // Hardware decoders are always available.
    if (vp9HardwareDecoderAvailable())
        return true;

    // For wall-powered devices, always report VP9 as supported, even if not powerEfficient.
    if (!systemHasBattery())
        return true;

    // For battery-powered devices, always report VP9 as supported when running on AC power,
    // but only on battery when there is an attached screen whose resolution is large enough
    // to support 4K video.
    if (systemHasAC())
        return true;

    bool has4kScreen = false;
    if (auto overrideForTesting = VP9TestingOverrides::singleton().vp9ScreenSizeAndScale()) {
        auto screenSize = FloatSize(overrideForTesting->width, overrideForTesting->height).scaled(overrideForTesting->scale);
        has4kScreen = resolutionCategory(screenSize) >= ResolutionCategory::R_4K;
    } else {
        for (auto& screenData : getScreenProperties().screenDataMap.values()) {
            if (resolutionCategory(screenData.screenRect.size().scaled(screenData.scaleFactor)) >= ResolutionCategory::R_4K) {
                has4kScreen = true;
                break;
            }
        }
    }

    return has4kScreen;
}

static bool isVP8CodecConfigurationRecordSupported(const VPCodecConfigurationRecord& codecConfiguration)
{
    if (!isVP8DecoderAvailable())
        return false;

    // VP8 decoder only supports Profile 0;
    if (codecConfiguration.profile)
        return false;

    // VP8 decoder only supports 420 chroma subsampling.
    if (codecConfiguration.chromaSubsampling > VPConfigurationChromaSubsampling::Subsampling_420_Colocated)
        return false;

    // VP8 decoder only supports 8-bit color:
    if (codecConfiguration.bitDepth > 8)
        return false;

    return true;
}

bool isVPCodecConfigurationRecordSupported(const VPCodecConfigurationRecord& codecConfiguration)
{
    if (codecConfiguration.codecName == "vp08"_s || codecConfiguration.codecName == "vp8"_s)
        return isVP8CodecConfigurationRecordSupported(codecConfiguration);

    if (codecConfiguration.codecName == "vp09"_s || codecConfiguration.codecName == "vp9"_s)
        return isVP9CodecConfigurationRecordSupported(codecConfiguration);

    return false;
}

std::optional<MediaCapabilitiesInfo> validateVPParameters(const VPCodecConfigurationRecord& codecConfiguration, const VideoConfiguration& videoConfiguration)
{
    if (!isVPCodecConfigurationRecordSupported(codecConfiguration))
        return std::nullopt;

    // VideoConfiguration and VPCodecConfigurationRecord can have conflicting values for HDR properties. If so, reject.
    if (videoConfiguration.transferFunction) {
        // Note: Transfer Characteristics are defined by ISO/IEC 23091-2:2019.
        if (*videoConfiguration.transferFunction == TransferFunction::SRGB && codecConfiguration.transferCharacteristics > 15)
            return std::nullopt;
        if (*videoConfiguration.transferFunction == TransferFunction::PQ && codecConfiguration.transferCharacteristics != 16)
            return std::nullopt;
        if (*videoConfiguration.transferFunction == TransferFunction::HLG && codecConfiguration.transferCharacteristics != 18)
            return std::nullopt;
    }

    if (videoConfiguration.colorGamut) {
        if (*videoConfiguration.colorGamut == ColorGamut::Rec2020 && codecConfiguration.colorPrimaries != 9)
            return std::nullopt;
    }
    return computeVPParameters(videoConfiguration);
}

bool isVPSoftwareDecoderSmooth(const VideoConfiguration& videoConfiguration)
{
    if (videoConfiguration.height <= 1080 && videoConfiguration.framerate > 60)
        return false;

    if (videoConfiguration.height <= 2160 && videoConfiguration.framerate > 30)
        return false;

    return true;
}

std::optional<MediaCapabilitiesInfo> computeVPParameters(const VideoConfiguration& videoConfiguration)
{
    MediaCapabilitiesInfo info;

    if (vp9HardwareDecoderAvailable()) {
        // HW VP9 Decoder does not support alpha channel:
        if (videoConfiguration.alphaChannel && *videoConfiguration.alphaChannel)
            return std::nullopt;

        // HW VP9 Decoder can support up to 4K @ 120 or 8K @ 30
        auto resolution = resolutionCategory({ (float)videoConfiguration.width, (float)videoConfiguration.height });
        if (resolution > ResolutionCategory::R_8K)
            return std::nullopt;
        if (resolution == ResolutionCategory::R_8K && videoConfiguration.framerate > 30)
            info.smooth = false;
        else if (resolution <= ResolutionCategory::R_4K && videoConfiguration.framerate > 120)
            info.smooth = false;
        else
            info.smooth = true;

        info.powerEfficient = true;
        info.supported = true;
        return info;
    }

    info.powerEfficient = false;

    // SW VP9 Decoder has much more variable capabilities depending on CPU characteristics.
    // FIXME: Add a lookup table for device-to-capabilities. For now, assume that the SW VP9
    // decoder can support 4K @ 30.
    info.smooth = isVPSoftwareDecoderSmooth(videoConfiguration);

    // For wall-powered devices, always report VP9 as supported, even if not powerEfficient.
    if (!systemHasBattery()) {
        info.supported = true;
        return info;
    }

    // For battery-powered devices, always report VP9 as supported when running on AC power,
    // but only on battery when there is an attached screen whose resolution is large enough
    // to support 4K video.
    if (systemHasAC()) {
        info.supported = true;
        return info;
    }

    bool has4kScreen = false;

    if (auto overrideForTesting = VP9TestingOverrides::singleton().vp9ScreenSizeAndScale()) {
        auto screenSize = FloatSize(overrideForTesting->width, overrideForTesting->height).scaled(overrideForTesting->scale);
        has4kScreen = resolutionCategory(screenSize) >= ResolutionCategory::R_4K;
    } else {
        for (auto& screenData : getScreenProperties().screenDataMap.values()) {
            if (resolutionCategory(screenData.screenRect.size().scaled(screenData.scaleFactor)) >= ResolutionCategory::R_4K) {
                has4kScreen = true;
                break;
            }
        }
    }

    if (!has4kScreen)
        return std::nullopt;

    info.supported = true;
    return info;
}

static uint8_t convertToColorPrimaries(const Primaries& coefficients)
{
    switch (coefficients) {
    case Primaries::kBt709:
        return VPConfigurationColorPrimaries::BT_709_6;
    case Primaries::kUnspecified:
        return VPConfigurationColorPrimaries::Unspecified;
    case Primaries::kBt470M:
        return VPConfigurationColorPrimaries::BT_470_6_M;
    case Primaries::kBt470Bg:
        return VPConfigurationColorPrimaries::BT_470_7_BG;
    case Primaries::kSmpte170M:
        return VPConfigurationColorPrimaries::BT_601_7;
    case Primaries::kSmpte240M:
        return VPConfigurationColorPrimaries::SMPTE_ST_240;
    case Primaries::kFilm:
        return VPConfigurationColorPrimaries::Film;
    case Primaries::kBt2020:
        return VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance;
    case Primaries::kSmpteSt4281:
        return VPConfigurationColorPrimaries::SMPTE_ST_428_1;
    case Primaries::kSmpteRp431:
        return VPConfigurationColorPrimaries::SMPTE_RP_431_2;
    case Primaries::kSmpteEg432:
        return VPConfigurationColorPrimaries::SMPTE_EG_432_1;
    case Primaries::kJedecP22Phosphors:
        return VPConfigurationColorPrimaries::EBU_Tech_3213_E;
    }
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

static uint8_t convertToTransferCharacteristics(const TransferCharacteristics& characteristics)
{
    switch (characteristics) {
    case TransferCharacteristics::kBt709:
        return VPConfigurationTransferCharacteristics::BT_709_6;
    case TransferCharacteristics::kUnspecified:
        return VPConfigurationTransferCharacteristics::Unspecified;
    case TransferCharacteristics::kGamma22curve:
        return VPConfigurationTransferCharacteristics::BT_470_6_M;
    case TransferCharacteristics::kGamma28curve:
        return VPConfigurationTransferCharacteristics::BT_470_7_BG;
    case TransferCharacteristics::kSmpte170M:
        return VPConfigurationTransferCharacteristics::BT_601_7;
    case TransferCharacteristics::kSmpte240M:
        return VPConfigurationTransferCharacteristics::SMPTE_ST_240;
    case TransferCharacteristics::kLinear:
        return VPConfigurationTransferCharacteristics::Linear;
    case TransferCharacteristics::kLog:
        return VPConfigurationTransferCharacteristics::Logrithmic;
    case TransferCharacteristics::kLogSqrt:
        return VPConfigurationTransferCharacteristics::Logrithmic_Sqrt;
    case TransferCharacteristics::kIec6196624:
        return VPConfigurationTransferCharacteristics::IEC_61966_2_4;
    case TransferCharacteristics::kBt1361ExtendedColourGamut:
        return VPConfigurationTransferCharacteristics::BT_1361_0;
    case TransferCharacteristics::kIec6196621:
        return VPConfigurationTransferCharacteristics::IEC_61966_2_1;
    case TransferCharacteristics::k10BitBt2020:
        return VPConfigurationTransferCharacteristics::BT_2020_10bit;
    case TransferCharacteristics::k12BitBt2020:
        return VPConfigurationTransferCharacteristics::BT_2020_12bit;
    case TransferCharacteristics::kSmpteSt2084:
        return VPConfigurationTransferCharacteristics::SMPTE_ST_2084;
    case TransferCharacteristics::kSmpteSt4281:
        return VPConfigurationTransferCharacteristics::SMPTE_ST_428_1;
    case TransferCharacteristics::kAribStdB67Hlg:
        return VPConfigurationTransferCharacteristics::BT_2100_HLG;
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

static uint8_t convertToMatrixCoefficients(const MatrixCoefficients& coefficients)
{
    switch (coefficients) {
    case MatrixCoefficients::kRgb:
        return VPConfigurationMatrixCoefficients::Identity;
    case MatrixCoefficients::kBt709:
        return VPConfigurationMatrixCoefficients::BT_709_6;
    case MatrixCoefficients::kUnspecified:
        return VPConfigurationMatrixCoefficients::Unspecified;
    case MatrixCoefficients::kFcc:
        return VPConfigurationMatrixCoefficients::FCC;
    case MatrixCoefficients::kBt470Bg:
        return VPConfigurationMatrixCoefficients::BT_470_7_BG;
    case MatrixCoefficients::kSmpte170M:
        return VPConfigurationMatrixCoefficients::BT_601_7;
    case MatrixCoefficients::kSmpte240M:
        return VPConfigurationMatrixCoefficients::SMPTE_ST_240;
    case MatrixCoefficients::kYCgCo:
        return VPConfigurationMatrixCoefficients::YCgCo;
    case MatrixCoefficients::kBt2020NonconstantLuminance:
        return VPConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance;
    case MatrixCoefficients::kBt2020ConstantLuminance:
        return VPConfigurationMatrixCoefficients::BT_2020_Constant_Luminance;
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

static uint8_t convertSubsamplingXYToChromaSubsampling(uint64_t x, uint64_t y)
{
    if (x & y)
        return VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
    if (x & !y)
        return VPConfigurationChromaSubsampling::Subsampling_422;
    if (!x & !y)
        return VPConfigurationChromaSubsampling::Subsampling_444;
    // This indicates 4:4:0 subsampling, which is not expressable in the 'vpcC' box. Default to 4:2:0.
    return VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
}

static Ref<VideoInfo> createVideoInfoFromVPCodecConfigurationRecord(const VPCodecConfigurationRecord& record, int32_t width, int32_t height)
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
    //
    // FIXME: Convert existing struct to an ISOBox and replace the writing code below
    // with a subclass of ISOFullBox.

    auto videoInfo = VideoInfo::create();
    videoInfo->size = videoInfo->displaySize = { float(width), float(height) };

    constexpr size_t VPCodecConfigurationContentsSize = 12;

    uint32_t versionAndFlags = 1 << 24;
    uint8_t bitDepthChromaAndRange = (0xF & record.bitDepth) << 4 | (0x7 & record.chromaSubsampling) << 1 | (0x1 & record.videoFullRangeFlag);
    uint16_t codecIntializationDataSize = 0;
    auto view = JSC::DataView::create(ArrayBuffer::create(VPCodecConfigurationContentsSize, 1), 0, VPCodecConfigurationContentsSize);
    view->set(0, versionAndFlags, false);
    view->set(4, record.profile, false);
    view->set(5, record.level, false);
    view->set(6, bitDepthChromaAndRange, false);
    view->set(7, record.colorPrimaries, false);
    view->set(8, record.transferCharacteristics, false);
    view->set(9, record.matrixCoefficients, false);
    view->set(10, codecIntializationDataSize, false);
    videoInfo->atomData = SharedBuffer::create(static_cast<uint8_t*>(view->data()), view->byteLength());
    videoInfo->colorSpace.fullRange = record.videoFullRangeFlag == VPConfigurationRange::FullRange;
    videoInfo->colorSpace.primaries = convertToPlatformVideoColorPrimaries(record.colorPrimaries);
    videoInfo->colorSpace.transfer = convertToPlatformVideoTransferCharacteristics(record.transferCharacteristics);
    videoInfo->colorSpace.matrix = convertToPlatformVideoMatrixCoefficients(record.matrixCoefficients);
    videoInfo->codecName = record.codecName == "vp09"_s ? 'vp09' : 'vp08';
    return videoInfo;
}

Ref<VideoInfo> createVideoInfoFromVP9HeaderParser(const vp9_parser::Vp9HeaderParser& parser, const webm::Element<Colour>& color)
{
    VPCodecConfigurationRecord record;

    record.codecName = "vp09"_s;
    record.profile = parser.profile();
    // CoreMedia does nat care about the VP9 codec level; hard-code to Level 1.0 here:
    record.level = 10;
    record.bitDepth = parser.bit_depth();
    record.videoFullRangeFlag = parser.color_range() ? VPConfigurationRange::FullRange : VPConfigurationRange::VideoRange;
    record.chromaSubsampling = convertSubsamplingXYToChromaSubsampling(parser.subsampling_x(), parser.subsampling_y());
    record.colorPrimaries = VPConfigurationColorPrimaries::Unspecified;
    record.transferCharacteristics = VPConfigurationTransferCharacteristics::Unspecified;
    record.matrixCoefficients = VPConfigurationMatrixCoefficients::Unspecified;

    // Container color values can override per-sample ones:
    if (color.is_present()) {
        auto& colorValue = color.value();
        if (colorValue.chroma_subsampling_x.is_present() && colorValue.chroma_subsampling_y.is_present())
            record.chromaSubsampling = convertSubsamplingXYToChromaSubsampling(colorValue.chroma_subsampling_x.value(), colorValue.chroma_subsampling_y.value());
        if (colorValue.range.is_present() && colorValue.range.value() != Range::kUnspecified)
            record.videoFullRangeFlag = colorValue.range.value() == Range::kFull ? VPConfigurationRange::FullRange : VPConfigurationRange::VideoRange;
        if (colorValue.bits_per_channel.is_present())
            record.bitDepth = colorValue.bits_per_channel.value();
        if (colorValue.transfer_characteristics.is_present())
            record.transferCharacteristics = convertToTransferCharacteristics(colorValue.transfer_characteristics.value());
        if (colorValue.matrix_coefficients.is_present())
            record.matrixCoefficients = convertToMatrixCoefficients(colorValue.matrix_coefficients.value());
        if (colorValue.primaries.is_present())
            record.colorPrimaries = convertToColorPrimaries(colorValue.primaries.value());
    }

    return createVideoInfoFromVPCodecConfigurationRecord(record, parser.width(), parser.height());
}

std::optional<VP8FrameHeader> parseVP8FrameHeader(const uint8_t* frameData, size_t frameSize)
{
    // VP8 frame headers are defined in RFC 6386: <https://tools.ietf.org/html/rfc6386>.

    // Bail if the header is below a minimum size
    if (frameSize < 11)
        return std::nullopt;

    VP8FrameHeader header;
    size_t headerSize = 11;

    auto view = JSC::DataView::create(ArrayBuffer::create(frameData, headerSize), 0, headerSize);
    bool status = true;

    auto uncompressedChunk = view->get<uint32_t>(0, true, &status);
    if (!status)
        return std::nullopt;

    header.keyframe = uncompressedChunk & 0x80000000;
    header.version = (uncompressedChunk >> 28) & 0x7;
    header.showFrame = uncompressedChunk & 0x8000000;
    header.partitionSize = (uncompressedChunk >> 8) & 0x7FFFF;

    if (!header.keyframe)
        return header;

    auto startCode0 = view->get<uint8_t>(3, true, &status);
    if (!status || startCode0 != 0x9d)
        return std::nullopt;

    auto startCode1 = view->get<uint8_t>(4, true, &status);
    if (!status || startCode1 != 0x01)
        return std::nullopt;

    auto startCode2 = view->get<uint8_t>(5, true, &status);
    if (!status || startCode2 != 0x2a)
        return std::nullopt;

    auto horizontalAndWidthField = view->get<uint16_t>(6, true, &status);
    if (!status)
        return std::nullopt;

    header.horizontalScale = static_cast<uint8_t>(horizontalAndWidthField >> 14);
    header.width = static_cast<uint16_t>(horizontalAndWidthField & 0x3FFF);

    auto verticalAndHeightField = view->get<uint16_t>(8, true, &status);
    if (!status)
        return std::nullopt;

    header.verticalScale = static_cast<uint8_t>(verticalAndHeightField >> 14);
    header.height = static_cast<uint16_t>(verticalAndHeightField & 0x3FFF);

    auto colorSpaceAndClampingField = view->get<uint8_t>(10, true, &status);
    if (!status)
        return std::nullopt;

    header.colorSpace = colorSpaceAndClampingField & 0x80;
    header.needsClamping = colorSpaceAndClampingField & 0x40;

    return header;
}

Ref<VideoInfo> createVideoInfoFromVP8Header(const VP8FrameHeader& header, const webm::Element<Colour>& color)
{
    VPCodecConfigurationRecord record;
    record.codecName = "vp08"_s;
    record.profile = 0;
    record.level = 10;
    record.bitDepth = 8;
    record.videoFullRangeFlag = VPConfigurationRange::FullRange;
    record.chromaSubsampling = VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
    record.colorPrimaries = header.colorSpace ? VPConfigurationColorPrimaries::Unspecified : VPConfigurationColorPrimaries::BT_601_7;
    record.transferCharacteristics =  header.colorSpace ? VPConfigurationTransferCharacteristics::Unspecified : VPConfigurationTransferCharacteristics::BT_601_7;
    record.matrixCoefficients = header.colorSpace ? VPConfigurationMatrixCoefficients::Unspecified : VPConfigurationMatrixCoefficients::BT_601_7;

    // Container color values can override per-sample ones:
    if (color.is_present()) {
        auto& colorValue = color.value();
        if (colorValue.chroma_subsampling_x.is_present() && colorValue.chroma_subsampling_y.is_present())
            record.chromaSubsampling = convertSubsamplingXYToChromaSubsampling(colorValue.chroma_subsampling_x.value(), colorValue.chroma_subsampling_y.value());
        if (colorValue.range.is_present() && colorValue.range.value() != Range::kUnspecified)
            record.videoFullRangeFlag = colorValue.range.value() == Range::kFull ? VPConfigurationRange::FullRange : VPConfigurationRange::VideoRange;
        if (colorValue.bits_per_channel.is_present())
            record.bitDepth = colorValue.bits_per_channel.value();
        if (colorValue.transfer_characteristics.is_present())
            record.transferCharacteristics = convertToTransferCharacteristics(colorValue.transfer_characteristics.value());
        if (colorValue.matrix_coefficients.is_present())
            record.matrixCoefficients = convertToMatrixCoefficients(colorValue.matrix_coefficients.value());
        if (colorValue.primaries.is_present())
            record.colorPrimaries = convertToColorPrimaries(colorValue.primaries.value());
    }

    return createVideoInfoFromVPCodecConfigurationRecord(record, header.width, header.height);
}

bool hasVP9ExtensionSupport()
{
#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/VP9UtilitiesCocoaAdditions.mm>
#endif

    return false;
}

}

#endif // PLATFORM(COCOA)
