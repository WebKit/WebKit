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

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <wtf/text/StringToIntegerConversion.h>

#import "VideoToolboxSoftLink.h"

namespace WebCore {

bool validateHEVCParameters(HEVCParameterSet& parameters, MediaCapabilitiesInfo& info, bool hasAlphaChannel, bool hdrSupport)
{
    CMVideoCodecType codec = kCMVideoCodecType_HEVC;
    if (hasAlphaChannel) {
        if (!PAL::isAVFoundationFrameworkAvailable() || !PAL::canLoad_AVFoundation_AVVideoCodecTypeHEVCWithAlpha())
            return false;

        auto codecCode = FourCC::fromString(AVVideoCodecTypeHEVCWithAlpha);
        if (!codecCode)
            return false;

        codec = codecCode.value().value;
    }

    if (hdrSupport) {
        // Platform supports HDR playback of HEVC Main10 Profile, as defined by ITU-T H.265 v6 (06/2019).
        bool isMain10 = parameters.generalProfileSpace == 0
            && (parameters.generalProfileIDC == 2 || parameters.generalProfileCompatibilityFlags == 1);
        if (!isMain10)
            return false;
    }

    OSStatus status = VTSelectAndCreateVideoDecoderInstance(codec, kCFAllocatorDefault, nullptr, nullptr);
    if (status != noErr)
        return false;

    if (!canLoad_VideoToolbox_VTCopyHEVCDecoderCapabilitiesDictionary()
        || !canLoad_VideoToolbox_kVTHEVCDecoderCapability_SupportedProfiles()
        || !canLoad_VideoToolbox_kVTHEVCDecoderCapability_PerProfileSupport()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_IsHardwareAccelerated()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_MaxDecodeLevel()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_MaxPlaybackLevel())
        return false;

    RetainPtr<CFDictionaryRef> capabilities = adoptCF(VTCopyHEVCDecoderCapabilitiesDictionary());
    if (!capabilities)
        return false;

    auto supportedProfiles = (CFArrayRef)CFDictionaryGetValue(capabilities.get(), kVTHEVCDecoderCapability_SupportedProfiles);
    if (!supportedProfiles || CFGetTypeID(supportedProfiles) != CFArrayGetTypeID())
        return false;

    int16_t generalProfileIDC = parameters.generalProfileIDC;
    auto cfGeneralProfileIDC = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &generalProfileIDC));
    auto searchRange = CFRangeMake(0, CFArrayGetCount(supportedProfiles));
    if (!CFArrayContainsValue(supportedProfiles, searchRange, cfGeneralProfileIDC.get()))
        return false;

    auto perProfileSupport = (CFDictionaryRef)CFDictionaryGetValue(capabilities.get(), kVTHEVCDecoderCapability_PerProfileSupport);
    if (!perProfileSupport || CFGetTypeID(perProfileSupport) != CFDictionaryGetTypeID())
        return false;

    auto generalProfileIDCString = String::number(generalProfileIDC).createCFString();
    auto profileSupport = (CFDictionaryRef)CFDictionaryGetValue(perProfileSupport, generalProfileIDCString.get());
    if (!profileSupport || CFGetTypeID(profileSupport) != CFDictionaryGetTypeID())
        return false;

    auto isHardwareAccelerated = (CFBooleanRef)CFDictionaryGetValue(profileSupport, kVTHEVCDecoderProfileCapability_IsHardwareAccelerated);
    if (isHardwareAccelerated && CFGetTypeID(isHardwareAccelerated) == CFBooleanGetTypeID())
        info.powerEfficient = CFBooleanGetValue(isHardwareAccelerated);

    auto cfMaxDecodeLevel = (CFNumberRef)CFDictionaryGetValue(profileSupport, kVTHEVCDecoderProfileCapability_MaxDecodeLevel);
    if (cfMaxDecodeLevel && CFGetTypeID(cfMaxDecodeLevel) == CFNumberGetTypeID()) {
        int16_t maxDecodeLevel = 0;
        if (!CFNumberGetValue(cfMaxDecodeLevel, kCFNumberSInt16Type, &maxDecodeLevel))
            return false;

        if (parameters.generalLevelIDC > maxDecodeLevel)
            return false;

        info.supported = true;
    }

    auto cfMaxPlaybackLevel = (CFNumberRef)CFDictionaryGetValue(profileSupport, kVTHEVCDecoderProfileCapability_MaxPlaybackLevel);
    if (cfMaxPlaybackLevel && CFGetTypeID(cfMaxPlaybackLevel) == CFNumberGetTypeID()) {
        int16_t maxPlaybackLevel = 0;
        if (!CFNumberGetValue(cfMaxPlaybackLevel, kCFNumberSInt16Type, &maxPlaybackLevel))
            return false;

        info.smooth = parameters.generalLevelIDC <= maxPlaybackLevel;
    }

    return true;
}

static Optional<CMVideoCodecType> codecTypeForDoViCodecString(const String& codecString)
{
    static auto map = makeNeverDestroyed<HashMap<String, CMVideoCodecType>>({
        { "avc1", kCMVideoCodecType_H264 },
        { "avc3", kCMVideoCodecType_H264 },
        { "hvc1", kCMVideoCodecType_HEVC },
        { "hev1", kCMVideoCodecType_HEVC },
    });

    auto findResult = map.get().find(codecString);
    if (findResult == map.get().end())
        return WTF::nullopt;
    return findResult->value;
}

static Optional<Vector<unsigned short>> CFStringArrayToNumberVector(CFArrayRef arrayCF)
{
    if (!arrayCF || CFGetTypeID(arrayCF) != CFArrayGetTypeID())
        return WTF::nullopt;

    auto arrayNS = (__bridge NSArray<NSString*>*)arrayCF;
    Vector<unsigned short> values;
    values.reserveInitialCapacity(arrayNS.count);

    bool areAllValidNumbers = true;
    [arrayNS enumerateObjectsUsingBlock:[&] (NSString* value, NSUInteger, BOOL *stop) {
        if (![value isKindOfClass:NSString.class]) {
            areAllValidNumbers = false;
            if (stop)
                *stop = true;
            return;
        }

        bool isValidNumber = false;
        auto numericValue = toIntegralType<unsigned short>(String(value), &isValidNumber);
        if (!isValidNumber) {
            areAllValidNumbers = false;
            if (stop)
                *stop = true;
            return;
        }

        values.uncheckedAppend(numericValue);
    }];

    if (!areAllValidNumbers)
        return WTF::nullopt;
    return values;
}

bool validateDoViParameters(DoViParameterSet& parameters, MediaCapabilitiesInfo& info, bool hasAlphaChannel, bool hdrSupport)
{
    if (hasAlphaChannel)
        return false;

    if (hdrSupport) {
        // Platform supports HDR playback of HEVC Main10 Profile, which is signalled by DoVi profiles 4, 5, 7, & 8.
        switch (parameters.bitstreamProfileID) {
        case 4:
        case 5:
        case 7:
        case 8:
            break;
        default:
            return false;
        }
    }

    auto codecType = codecTypeForDoViCodecString(parameters.codecName);
    if (!codecType)
        return false;

    OSStatus status = VTSelectAndCreateVideoDecoderInstance(codecType.value(), kCFAllocatorDefault, nullptr, nullptr);
    if (status != noErr)
        return false;

    if (!canLoad_VideoToolbox_VTCopyHEVCDecoderCapabilitiesDictionary()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_SupportedProfiles()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_SupportedLevels()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_IsHardwareAccelerated())
        return false;

    RetainPtr<CFDictionaryRef> capabilities = adoptCF(VTCopyHEVCDecoderCapabilitiesDictionary());
    if (!capabilities)
        return false;

    auto supportedProfilesCF = (CFArrayRef)CFDictionaryGetValue(capabilities.get(), kVTDolbyVisionDecoderCapability_SupportedProfiles);
    auto supportedProfiles = CFStringArrayToNumberVector(supportedProfilesCF);
    if (!supportedProfiles)
        return false;

    auto supportedLevelsCF = (CFArrayRef)CFDictionaryGetValue(capabilities.get(), kVTDolbyVisionDecoderCapability_SupportedLevels);
    auto supportedLevels = CFStringArrayToNumberVector(supportedLevelsCF);
    if (!supportedLevels)
        return false;

    auto isHardwareAcceleratedCF = (CFBooleanRef)CFDictionaryGetValue(capabilities.get(), kVTDolbyVisionDecoderCapability_IsHardwareAccelerated);
    if (!isHardwareAcceleratedCF || CFGetTypeID(isHardwareAcceleratedCF) != CFBooleanGetTypeID())
        return false;

    if (!supportedProfiles.value().contains(parameters.bitstreamProfileID) || !supportedLevels.value().contains(parameters.bitstreamLevelID))
        return false;

    info.supported = true;
    info.smooth = true;
    info.powerEfficient = CFBooleanGetValue(isHardwareAcceleratedCF);

    return true;
}

}

#endif // PLATFORM(COCOA)
