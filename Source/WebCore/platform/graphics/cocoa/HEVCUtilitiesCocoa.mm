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

#import "VideoToolboxSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

bool validateHEVCParameters(HEVCParameterSet& parameters, MediaCapabilitiesInfo& info, bool hasAlphaChannel)
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

}

#endif // PLATFORM(COCOA)
