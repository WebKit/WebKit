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
#include "VP9UtilitiesCocoa.h"

#if PLATFORM(COCOA)

#import "FourCC.h"
#import "LibWebRTCProvider.h"
#import "MediaCapabilitiesInfo.h"
#import "PlatformScreen.h"
#import "ScreenProperties.h"
#import "SystemBattery.h"
#import "VideoConfiguration.h"
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import "VideoToolboxSoftLink.h"

namespace WebCore {

// FIXME: Remove this once kCMVideoCodecType_VP9 is added to CMFormatDescription.h
constexpr CMVideoCodecType kCMVideoCodecType_VP9 { 'vp09' };

static bool hardwareVP9DecoderDisabledForTesting { false };

struct OverrideScreenData {
    float width { 0 };
    float height { 0 };
    float scale { 1 };
};
static Optional<OverrideScreenData> screenSizeAndScaleForTesting;

void setOverrideVP9HardwareDecoderDisabledForTesting(bool disabled)
{
    hardwareVP9DecoderDisabledForTesting = disabled;
}

void setOverrideVP9ScreenSizeAndScaleForTesting(float width, float height, float scale)
{
    screenSizeAndScaleForTesting = makeOptional<OverrideScreenData>({ width, height, scale });
}

void resetOverrideVP9ScreenSizeAndScaleForTesting()
{
    screenSizeAndScaleForTesting = WTF::nullopt;
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

void registerSupplementalVP9Decoder()
{
    if (!VideoToolboxLibrary(true))
        return;

    if (canLoad_VideoToolbox_VTRegisterSupplementalVideoDecoderIfAvailable())
        softLink_VideoToolbox_VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);
}

bool isVP9DecoderAvailable()
{
    if (!VideoToolboxLibrary(true))
        return false;
    return noErr == VTSelectAndCreateVideoDecoderInstance(kCMVideoCodecType_VP9, kCFAllocatorDefault, nullptr, nullptr);
}

bool validateVPParameters(VPCodecConfigurationRecord& codecConfiguration, MediaCapabilitiesInfo& info, const VideoConfiguration& videoConfiguration)
{
    if (!isVP9DecoderAvailable())
        return false;

    // VideoConfiguration and VPCodecConfigurationRecord can have conflicting values for HDR properties. If so, reject.
    if (videoConfiguration.transferFunction) {
        // Note: Transfer Characteristics are defined by ISO/IEC 23091-2:2019.
        if (*videoConfiguration.transferFunction == TransferFunction::SRGB && codecConfiguration.transferCharacteristics > 15)
            return false;
        if (*videoConfiguration.transferFunction == TransferFunction::PQ && codecConfiguration.transferCharacteristics != 16)
            return false;
        if (*videoConfiguration.transferFunction == TransferFunction::HLG && codecConfiguration.transferCharacteristics != 18)
            return false;
    }

    if (videoConfiguration.colorGamut) {
        if (*videoConfiguration.colorGamut == ColorGamut::Rec2020 && codecConfiguration.colorPrimaries != 9)
            return false;
    }

    if (canLoad_VideoToolbox_VTIsHardwareDecodeSupported() && VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9) && !hardwareVP9DecoderDisabledForTesting) {
        info.powerEfficient = true;

        // HW VP9 Decoder supports Profile 0 & 2:
        if (codecConfiguration.profile && codecConfiguration.profile != 2)
            return false;

        // HW VP9 Decoder supports up to Level 6:
        if (codecConfiguration.level > VPConfigurationLevel::Level_6)
            return false;

        // HW VP9 Decoder supports 8 or 10 bit color:
        if (codecConfiguration.bitDepth > 10)
            return false;

        // HW VP9 Decoder suports only 420 chroma subsampling:
        if (codecConfiguration.chromaSubsampling > VPConfigurationChromaSubsampling::Subsampling_420_Colocated)
            return false;

        // HW VP9 Decoder does not support alpha channel:
        if (videoConfiguration.alphaChannel && *videoConfiguration.alphaChannel)
            return false;

        // HW VP9 Decoder can support up to 4K @ 120 or 8K @ 30
        auto resolution = resolutionCategory({ (float)videoConfiguration.width, (float)videoConfiguration.height });
        if (resolution > ResolutionCategory::R_8K)
            return false;
        if (resolution == ResolutionCategory::R_8K && videoConfiguration.framerate > 30)
            info.smooth = false;
        else if (resolution <= ResolutionCategory::R_4K && videoConfiguration.framerate > 120)
            info.smooth = false;
        else
            info.smooth = true;

        return true;
    }

    // SW VP9 Decoder has much more variable capabilities depending on CPU characteristics.
    // FIXME: Add a lookup table for device-to-capabilities. For now, assume that the SW VP9
    // decoder can support 4K @ 30.
    if (videoConfiguration.height <= 1080 && videoConfiguration.framerate > 60)
        info.smooth = false;
    if (videoConfiguration.height <= 2160 && videoConfiguration.framerate > 30)
        info.smooth = false;
    else
        info.smooth = true;

    // For wall-powered devices, always report VP9 as supported, even if not powerEfficient.
    if (!systemHasBattery()) {
        info.supported = true;
        return true;
    }

    // For battery-powered devices, always report VP9 as supported when running on AC power,
    // but only on battery when there is an attached screen whose resolution is large enough
    // to support 4K video.
    if (systemHasAC()) {
        info.supported = true;
        return true;
    }

    bool has4kScreen = false;

    if (screenSizeAndScaleForTesting) {
        auto screenSize = FloatSize(screenSizeAndScaleForTesting->width, screenSizeAndScaleForTesting->height).scaled(screenSizeAndScaleForTesting->scale);
        has4kScreen = resolutionCategory(screenSize) >= ResolutionCategory::R_4K;
    } else {
        for (auto& screenData : getScreenProperties().screenDataMap.values()) {
            if (resolutionCategory(screenData.screenRect.size().scaled(screenData.scaleFactor)) >= ResolutionCategory::R_4K) {
                has4kScreen = true;
                break;
            }
        }
    }

    if (!has4kScreen) {
        info.supported = false;
        return false;
    }

    return true;
}

}

#endif // PLATFORM(COCOA)
