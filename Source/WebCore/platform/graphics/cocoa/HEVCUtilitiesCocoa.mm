/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "HEVCUtilitiesCocoa.h"

#if PLATFORM(COCOA)

#import "FourCC.h"
#import "HEVCUtilities.h"
#import "MediaCapabilitiesInfo.h"

#import <wtf/RobinHoodHashMap.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#import "VideoToolboxSoftLink.h"

namespace WebCore {

Optional<MediaCapabilitiesInfo> validateHEVCParameters(const HEVCParameters& parameters, bool hasAlphaChannel, bool hdrSupport)
{
    CMVideoCodecType codec = kCMVideoCodecType_HEVC;
    if (hasAlphaChannel) {
        if (!PAL::isAVFoundationFrameworkAvailable() || !PAL::canLoad_AVFoundation_AVVideoCodecTypeHEVCWithAlpha())
            return WTF::nullopt;

        auto codecCode = FourCC::fromString(AVVideoCodecTypeHEVCWithAlpha);
        if (!codecCode)
            return WTF::nullopt;

        codec = codecCode.value().value;
    }

    if (hdrSupport) {
        // Platform supports HDR playback of HEVC Main10 Profile, as defined by ITU-T H.265 v6 (06/2019).
        bool isMain10 = parameters.generalProfileSpace == 0
            && (parameters.generalProfileIDC == 2 || parameters.generalProfileCompatibilityFlags == 1);
        if (!isMain10)
            return WTF::nullopt;
    }

    OSStatus status = VTSelectAndCreateVideoDecoderInstance(codec, kCFAllocatorDefault, nullptr, nullptr);
    if (status != noErr)
        return WTF::nullopt;

    if (!canLoad_VideoToolbox_VTCopyHEVCDecoderCapabilitiesDictionary()
        || !canLoad_VideoToolbox_kVTHEVCDecoderCapability_SupportedProfiles()
        || !canLoad_VideoToolbox_kVTHEVCDecoderCapability_PerProfileSupport()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_IsHardwareAccelerated()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_MaxDecodeLevel()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_MaxPlaybackLevel())
        return WTF::nullopt;

    auto capabilities = adoptCF(VTCopyHEVCDecoderCapabilitiesDictionary());
    if (!capabilities)
        return WTF::nullopt;

    auto supportedProfiles = (CFArrayRef)CFDictionaryGetValue(capabilities.get(), kVTHEVCDecoderCapability_SupportedProfiles);
    if (!supportedProfiles || CFGetTypeID(supportedProfiles) != CFArrayGetTypeID())
        return WTF::nullopt;

    int16_t generalProfileIDC = parameters.generalProfileIDC;
    auto cfGeneralProfileIDC = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &generalProfileIDC));
    auto searchRange = CFRangeMake(0, CFArrayGetCount(supportedProfiles));
    if (!CFArrayContainsValue(supportedProfiles, searchRange, cfGeneralProfileIDC.get()))
        return WTF::nullopt;

    auto perProfileSupport = (CFDictionaryRef)CFDictionaryGetValue(capabilities.get(), kVTHEVCDecoderCapability_PerProfileSupport);
    if (!perProfileSupport || CFGetTypeID(perProfileSupport) != CFDictionaryGetTypeID())
        return WTF::nullopt;

    auto generalProfileIDCString = String::number(generalProfileIDC).createCFString();
    auto profileSupport = (CFDictionaryRef)CFDictionaryGetValue(perProfileSupport, generalProfileIDCString.get());
    if (!profileSupport || CFGetTypeID(profileSupport) != CFDictionaryGetTypeID())
        return WTF::nullopt;

    MediaCapabilitiesInfo info;

    info.supported = true;

    auto isHardwareAccelerated = (CFBooleanRef)CFDictionaryGetValue(profileSupport, kVTHEVCDecoderProfileCapability_IsHardwareAccelerated);
    if (isHardwareAccelerated && CFGetTypeID(isHardwareAccelerated) == CFBooleanGetTypeID())
        info.powerEfficient = CFBooleanGetValue(isHardwareAccelerated);

    auto cfMaxDecodeLevel = (CFNumberRef)CFDictionaryGetValue(profileSupport, kVTHEVCDecoderProfileCapability_MaxDecodeLevel);
    if (cfMaxDecodeLevel && CFGetTypeID(cfMaxDecodeLevel) == CFNumberGetTypeID()) {
        int16_t maxDecodeLevel = 0;
        if (!CFNumberGetValue(cfMaxDecodeLevel, kCFNumberSInt16Type, &maxDecodeLevel))
            return WTF::nullopt;

        if (parameters.generalLevelIDC > maxDecodeLevel)
            return WTF::nullopt;
    }

    auto cfMaxPlaybackLevel = (CFNumberRef)CFDictionaryGetValue(profileSupport, kVTHEVCDecoderProfileCapability_MaxPlaybackLevel);
    if (cfMaxPlaybackLevel && CFGetTypeID(cfMaxPlaybackLevel) == CFNumberGetTypeID()) {
        int16_t maxPlaybackLevel = 0;
        if (!CFNumberGetValue(cfMaxPlaybackLevel, kCFNumberSInt16Type, &maxPlaybackLevel))
            return WTF::nullopt;

        info.smooth = parameters.generalLevelIDC <= maxPlaybackLevel;
    }

    return info;
}

static CMVideoCodecType codecType(DoViParameters::Codec codec)
{
    switch (codec) {
    case DoViParameters::Codec::AVC1:
    case DoViParameters::Codec::AVC3:
        return kCMVideoCodecType_H264;
    case DoViParameters::Codec::HEV1:
    case DoViParameters::Codec::HVC1:
        return kCMVideoCodecType_HEVC;
    }
}

static Optional<Vector<uint16_t>> parseStringArrayFromDictionaryToUInt16Vector(CFDictionaryRef dictionary, const void* key)
{
    auto value = CFDictionaryGetValue(dictionary, key);
    if (!value || CFGetTypeID(value) != CFArrayGetTypeID())
        return WTF::nullopt;
    NSArray *array = (__bridge NSArray *)value;
    Vector<uint16_t> vector;
    vector.reserveInitialCapacity(array.count);
    for (id value in array) {
        if (![value isKindOfClass:NSString.class])
            return WTF::nullopt;
        bool isValidNumber = false;
        auto numericValue = toIntegralType<uint16_t>(String((NSString *)value), &isValidNumber);
        if (!isValidNumber)
            return WTF::nullopt;
        vector.uncheckedAppend(numericValue);
    }
    return vector;
}

Optional<MediaCapabilitiesInfo> validateDoViParameters(const DoViParameters& parameters, bool hasAlphaChannel, bool hdrSupport)
{
    if (hasAlphaChannel)
        return WTF::nullopt;

    if (hdrSupport) {
        // Platform supports HDR playback of HEVC Main10 Profile, which is signalled by DoVi profiles 4, 5, 7, & 8.
        switch (parameters.bitstreamProfileID) {
        case 4:
        case 5:
        case 7:
        case 8:
            break;
        default:
            return WTF::nullopt;
        }
    }

    OSStatus status = VTSelectAndCreateVideoDecoderInstance(codecType(parameters.codec), kCFAllocatorDefault, nullptr, nullptr);
    if (status != noErr)
        return WTF::nullopt;

    if (!canLoad_VideoToolbox_VTCopyHEVCDecoderCapabilitiesDictionary()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_SupportedProfiles()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_SupportedLevels()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_IsHardwareAccelerated())
        return WTF::nullopt;

    auto capabilities = adoptCF(VTCopyHEVCDecoderCapabilitiesDictionary());
    if (!capabilities)
        return WTF::nullopt;

    auto supportedProfiles = parseStringArrayFromDictionaryToUInt16Vector(capabilities.get(), kVTDolbyVisionDecoderCapability_SupportedProfiles);
    if (!supportedProfiles)
        return WTF::nullopt;

    auto supportedLevels = parseStringArrayFromDictionaryToUInt16Vector(capabilities.get(), kVTDolbyVisionDecoderCapability_SupportedLevels);
    if (!supportedLevels)
        return WTF::nullopt;

    auto isHardwareAcceleratedCF = (CFBooleanRef)CFDictionaryGetValue(capabilities.get(), kVTDolbyVisionDecoderCapability_IsHardwareAccelerated);
    if (!isHardwareAcceleratedCF || CFGetTypeID(isHardwareAcceleratedCF) != CFBooleanGetTypeID())
        return WTF::nullopt;
    bool isHardwareAccelerated = CFBooleanGetValue(isHardwareAcceleratedCF);

    if (!supportedProfiles.value().contains(parameters.bitstreamProfileID) || !supportedLevels.value().contains(parameters.bitstreamLevelID))
        return WTF::nullopt;

    return { { true, true, isHardwareAccelerated } };
}

}

#endif // PLATFORM(COCOA)
