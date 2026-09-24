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

#import "CMUtilities.h"
#import "FourCC.h"
#import "HEVCUtilities.h"
#import "Logging.h"
#import "PlatformMediaCapabilitiesInfo.h"
#import "TrackInfo.h"
#import <algorithm>
#import <wtf/FlipBytes.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringToIntegerConversion.h>

#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

std::optional<PlatformMediaCapabilitiesInfo> validateHEVCParameters(const HEVCParameters& parameters, bool hasAlphaChannel, bool hdrSupport)
{
    CMVideoCodecType codec = kCMVideoCodecType_HEVC;
    if (hasAlphaChannel) {
        if (!PAL::isAVFoundationFrameworkAvailable() || !PAL::canLoad_AVFoundation_AVVideoCodecTypeHEVCWithAlpha())
            return std::nullopt;

        auto codecCode = FourCC::fromString(String { AVVideoCodecTypeHEVCWithAlpha });
        if (!codecCode)
            return std::nullopt;

        codec = codecCode.value().value;
    }

    if (hdrSupport) {
        // Platform supports HDR playback of HEVC Main10 Profile, as defined by ITU-T H.265 v6 (06/2019).
        bool isMain10 = parameters.generalProfileSpace == 0
            && (parameters.generalProfileIDC == 2 || parameters.generalProfileCompatibilityFlags == 1);
        if (!isMain10)
            return std::nullopt;
    }

    OSStatus status = VTSelectAndCreateVideoDecoderInstance(codec, kCFAllocatorDefault, nullptr, nullptr);
    if (status != noErr)
        return std::nullopt;

    if (!canLoad_VideoToolbox_VTCopyHEVCDecoderCapabilitiesDictionary()
        || !canLoad_VideoToolbox_kVTHEVCDecoderCapability_SupportedProfiles()
        || !canLoad_VideoToolbox_kVTHEVCDecoderCapability_PerProfileSupport()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_IsHardwareAccelerated()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_MaxDecodeLevel()
        || !canLoad_VideoToolbox_kVTHEVCDecoderProfileCapability_MaxPlaybackLevel())
        return std::nullopt;

    auto capabilities = adoptCF(VTCopyHEVCDecoderCapabilitiesDictionary());
    if (!capabilities)
        return std::nullopt;

    RetainPtr supportedProfiles = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(capabilities.get(), kVTHEVCDecoderCapability_SupportedProfiles));
    if (!supportedProfiles)
        return std::nullopt;

    int16_t generalProfileIDC = parameters.generalProfileIDC;
    auto cfGeneralProfileIDC = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &generalProfileIDC));
    auto searchRange = CFRangeMake(0, CFArrayGetCount(supportedProfiles.get()));
    if (!CFArrayContainsValue(supportedProfiles.get(), searchRange, cfGeneralProfileIDC.get()))
        return std::nullopt;

    RetainPtr perProfileSupport = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(capabilities.get(), kVTHEVCDecoderCapability_PerProfileSupport));
    if (!perProfileSupport)
        return std::nullopt;

    auto generalProfileIDCString = String::number(generalProfileIDC).createCFString();
    RetainPtr profileSupport = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(perProfileSupport.get(), generalProfileIDCString.get()));
    if (!profileSupport)
        return std::nullopt;

    PlatformMediaCapabilitiesInfo info;

    info.supported = true;

    info.powerEfficient = CFDictionaryGetValue(profileSupport.get(), kVTHEVCDecoderProfileCapability_IsHardwareAccelerated) == kCFBooleanTrue;

    if (RetainPtr cfMaxDecodeLevel = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(profileSupport.get(), kVTHEVCDecoderProfileCapability_MaxDecodeLevel))) {
        int16_t maxDecodeLevel = 0;
        if (!CFNumberGetValue(cfMaxDecodeLevel.get(), kCFNumberSInt16Type, &maxDecodeLevel))
            return std::nullopt;

        if (parameters.generalLevelIDC > maxDecodeLevel)
            return std::nullopt;
    }

    if (RetainPtr cfMaxPlaybackLevel = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(profileSupport.get(), kVTHEVCDecoderProfileCapability_MaxPlaybackLevel))) {
        int16_t maxPlaybackLevel = 0;
        if (!CFNumberGetValue(cfMaxPlaybackLevel.get(), kCFNumberSInt16Type, &maxPlaybackLevel))
            return std::nullopt;

        info.smooth = parameters.generalLevelIDC <= maxPlaybackLevel;
    }

    return info;
}

static CMVideoCodecType NODELETE codecType(DoViParameters::Codec codec)
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

static std::optional<Vector<uint16_t>> parseStringArrayFromDictionaryToUInt16Vector(CFDictionaryRef dictionary, const void* key)
{
    RetainPtr array = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary, key));
    if (!array)
        return std::nullopt;
    bool parseFailed = false;
    auto result = makeVector(bridge_cast(array.get()), [&] (id value) {
        auto parseResult = parseInteger<uint16_t>(String(dynamic_objc_cast<NSString>(value)));
        parseFailed |= !parseResult;
        return parseResult;
    });
    if (parseFailed)
        return std::nullopt;
    return result;
}

std::optional<PlatformMediaCapabilitiesInfo> validateDoViParameters(const DoViParameters& parameters, bool hasAlphaChannel, bool hdrSupport)
{
    if (hasAlphaChannel)
        return std::nullopt;

    if (hdrSupport) {
        // Platform supports HDR playback of HEVC Main10 Profile, which is signalled by DoVi profiles 4, 5, 7, & 8.
        switch (parameters.bitstreamProfileID) {
        case 4:
        case 5:
        case 7:
        case 8:
            break;
        default:
            return std::nullopt;
        }
    }

    OSStatus status = VTSelectAndCreateVideoDecoderInstance(codecType(parameters.codec), kCFAllocatorDefault, nullptr, nullptr);
    if (status != noErr)
        return std::nullopt;

    if (!canLoad_VideoToolbox_VTCopyHEVCDecoderCapabilitiesDictionary()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_SupportedProfiles()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_SupportedLevels()
        || !canLoad_VideoToolbox_kVTDolbyVisionDecoderCapability_IsHardwareAccelerated())
        return std::nullopt;

    auto capabilities = adoptCF(VTCopyHEVCDecoderCapabilitiesDictionary());
    if (!capabilities)
        return std::nullopt;

    auto supportedProfiles = parseStringArrayFromDictionaryToUInt16Vector(capabilities.get(), kVTDolbyVisionDecoderCapability_SupportedProfiles);
    if (!supportedProfiles)
        return std::nullopt;

    auto supportedLevels = parseStringArrayFromDictionaryToUInt16Vector(capabilities.get(), kVTDolbyVisionDecoderCapability_SupportedLevels);
    if (!supportedLevels)
        return std::nullopt;

    bool isHardwareAccelerated = CFDictionaryGetValue(capabilities.get(), kVTDolbyVisionDecoderCapability_IsHardwareAccelerated) == kCFBooleanTrue;

    if (!supportedProfiles.value().contains(parameters.bitstreamProfileID) || !supportedLevels.value().contains(parameters.bitstreamLevelID))
        return std::nullopt;

    return { { true, true, isHardwareAccelerated } };
}

Vector<uint8_t> convertHEVCCMSampleBufferToAnnexB(CMSampleBufferRef hvccSampleBuffer, bool isKeyframe)
{
    return convertParameterSetsCMSampleBufferToAnnexB(hvccSampleBuffer, isKeyframe, PAL::CMVideoFormatDescriptionGetHEVCParameterSetAtIndex);
}

static RetainPtr<CMFormatDescriptionRef> createHEVCFormatDescriptionFromParameterSets(std::span<const uint8_t* const> paramSetPointers, std::span<const size_t> paramSetSizes, size_t nalUnitHeaderLength)
{
    CMFormatDescriptionRef rawDescription = nullptr;
    if (PAL::CMVideoFormatDescriptionCreateFromHEVCParameterSets(kCFAllocatorDefault, paramSetPointers.size(), paramSetPointers.data(), paramSetSizes.data(), nalUnitHeaderLength, nullptr, &rawDescription) != noErr)
        return nullptr;
    return adoptCF(rawDescription);
}

static bool hevcAnnexBVpsIsFollowedBySpsAndPps(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices, size_t vpsIndex)
{
    if (vpsIndex + 2 >= naluIndices.size())
        return false;

    auto& spsIndex = naluIndices[vpsIndex + 1];
    auto& ppsIndex = naluIndices[vpsIndex + 2];
    return spsIndex.payloadSize && hevcNaluType(data[spsIndex.payloadStartOffset]) == HEVCNaluType::Sps
        && ppsIndex.payloadSize && hevcNaluType(data[ppsIndex.payloadStartOffset]) == HEVCNaluType::Pps;
}

RefPtr<VideoInfo> createVideoInfoFromHEVCAnnexBStream(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices)
{
    auto vpsIndex = findHEVCAnnexBVpsIndex(data, naluIndices);
    if (vpsIndex == notFound || !hevcAnnexBVpsIsFollowedBySpsAndPps(data, naluIndices, vpsIndex)) {
        RELEASE_LOG_ERROR(WebRTC, "createVideoInfoFromHEVCAnnexBStream NAL units following VPS are not SPS/PPS");
        return nullptr;
    }

    std::array<std::span<const uint8_t>, 3> paramSets;
    for (size_t i = 0; i < paramSets.size(); ++i) {
        auto& index = naluIndices[vpsIndex + i];
        paramSets[i] = data.subspan(index.payloadStartOffset, index.payloadSize);
    }
    std::array<const uint8_t*, 3> paramSetPointers { paramSets[0].data(), paramSets[1].data(), paramSets[2].data() };
    std::array<size_t, 3> paramSetSizes { paramSets[0].size(), paramSets[1].size(), paramSets[2].size() };

    RetainPtr description = createHEVCFormatDescriptionFromParameterSets(paramSetPointers, paramSetSizes, 4);
    if (!description)
        return nullptr;

    return createVideoInfoFromFormatDescription(description);
}

Vector<uint8_t> convertHEVCAnnexBToLengthPrefixed(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices)
{
    // We skip all NAL units up to and including the VPS/SPS/PPS triplet, if present, as parameter sets belong in the format description, not in the per-sample data.
    size_t startIndex = 0;
    auto vpsIndex = findHEVCAnnexBVpsIndex(data, naluIndices);
    if (vpsIndex != notFound) {
        if (hevcAnnexBVpsIsFollowedBySpsAndPps(data, naluIndices, vpsIndex))
            startIndex = vpsIndex + 3;
        else
            RELEASE_LOG_ERROR(WebRTC, "convertHEVCAnnexBToLengthPrefixed NAL units following VPS are not SPS/PPS");
    }

    size_t totalSize = 0;
    for (size_t i = startIndex; i < naluIndices.size(); ++i) {
        if (naluIndices[i].payloadSize)
            totalSize += sizeof(uint32_t) + naluIndices[i].payloadSize;
    }

    Vector<uint8_t> result;
    result.reserveInitialCapacity(totalSize);
    for (size_t i = startIndex; i < naluIndices.size(); ++i) {
        auto& index = naluIndices[i];
        if (!index.payloadSize)
            continue;
        uint32_t length = flipBytes(static_cast<uint32_t>(index.payloadSize));
        result.append(asByteSpan(length));
        result.append(data.subspan(index.payloadStartOffset, index.payloadSize));
    }

    return result;
}

RefPtr<VideoInfo> createVideoInfoFromHVCC(const HVCCParameterSets& parameterSets)
{
    if (parameterSets.paramSets.isEmpty())
        return nullptr;

    auto& paramSets = parameterSets.paramSets;
    Vector<const uint8_t*> paramSetPointers { paramSets.size(),
        [&paramSets](auto index) {
            return paramSets[index].data.data();
        }
    };
    Vector<size_t> paramSetSizes { paramSets.size(),
        [&paramSets](auto index) {
            return paramSets[index].data.size();
        }
    };

    RetainPtr description = createHEVCFormatDescriptionFromParameterSets(paramSetPointers.span(), paramSetSizes.span(), parameterSets.lengthFieldSize);
    if (!description)
        return nullptr;

    return createVideoInfoFromFormatDescription(description);
}

}

#endif // PLATFORM(COCOA)
