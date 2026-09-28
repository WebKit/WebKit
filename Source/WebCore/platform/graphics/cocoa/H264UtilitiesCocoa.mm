/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "H264UtilitiesCocoa.h"

#import "AnnexBUtilities.h"
#import "BitReader.h"
#import "CMUtilities.h"
#import "FormatDescriptionUtilities.h"
#import "H264Utilities.h"
#import "Logging.h"
#import "TrackInfo.h"
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

RefPtr<VideoInfo> createVideoInfoFromAVCC(std::span<const uint8_t> avcc)
{
    if (avcc.size() < 7)
        return nullptr;

    BitReader reader { avcc };

    // configurationVersion
    reader.read(8);
    // AVCProfileIndication;
    reader.read(8);
    // profile_compatibility;
    reader.read(8);
    // AVCLevelIndication;
    reader.read(8);
    // bit(6) reserved = '111111'b;
    // unsigned int(2) lengthSizeMinusOne;
    size_t lengthSize = (*reader.read<uint8_t>() & 0x3) + 1;
    // bit(3) reserved = '111'b;
    // unsigned int(5) numOfSequenceParameterSets;
    size_t numOfSequenceParameterSets = 0x1f & *reader.read<uint8_t>();
    if (!numOfSequenceParameterSets)
        return nullptr;

    constexpr size_t kNALTypeSize = 1;
    constexpr uint8_t kSPSNAL = 7;
    constexpr uint8_t kPPSNAL = 8;
    constexpr uint8_t kNAL_REF_IDC_SEQ_PARAM_SET = 0x80;
    constexpr uint8_t kNAL_REF_IDC_PIC_PARAM_SET = 0x60;

    Vector<Vector<uint8_t>> paramSets;
    paramSets.reserveInitialCapacity(numOfSequenceParameterSets + 1); // typical number of picture parameter set is 1.

    for (size_t index = 0; index < numOfSequenceParameterSets; index++) {
        auto size = reader.read<uint16_t>();
        if (!size || *size < kNALTypeSize)
            return nullptr;
        auto nalType = reader.read<uint8_t>();
        if (!nalType || (*nalType & 0x1f) != kSPSNAL)
            return nullptr;
        if (!reader.skipBytes(*size - kNALTypeSize))
            return nullptr;
        paramSets.append({ avcc.subspan(reader.byteOffset() - *size, *size) });
        paramSets.last()[0] &= ~kNAL_REF_IDC_SEQ_PARAM_SET;
    }

    // unsigned int(8) numOfPictureParameterSets;
    auto numOfPictureParameterSets = reader.read<uint8_t>();
    if (!numOfPictureParameterSets || !*numOfPictureParameterSets)
        return nullptr;
    for (size_t index = 0; index < *numOfPictureParameterSets; index++) {
        auto size = reader.read<uint16_t>();
        if (!size || *size < kNALTypeSize)
            return nullptr;
        auto nalType = reader.read<uint8_t>();
        if (!nalType || (*nalType & 0x1f) != kPPSNAL)
            return nullptr;
        if (!reader.skipBytes(*size - kNALTypeSize))
            return nullptr;
        paramSets.append({ avcc.subspan(reader.byteOffset() - *size, *size) });
        paramSets.last()[0] |= kNAL_REF_IDC_PIC_PARAM_SET;
    }

    Vector<const uint8_t*> paramSetPtrs { paramSets.size(), [&paramSets](auto index) {
        return paramSets[index].span().data();
    } };
    Vector<size_t> paramSetSizes { paramSets.size(), [&paramSets](auto index) {
        return paramSets[index].size();
    } };

    CMFormatDescriptionRef rawDescription = nullptr;
    if (PAL::CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault, paramSetPtrs.size(), paramSetPtrs.span().data(), paramSetSizes.span().data(), lengthSize, &rawDescription))
        return nullptr;
    RetainPtr description = adoptCF(rawDescription);
    return createVideoInfoFromFormatDescription(description);
}

Vector<uint8_t> convertAVCCMSampleBufferToAnnexB(CMSampleBufferRef avccSampleBuffer, bool isKeyframe)
{
    return convertParameterSetsCMSampleBufferToAnnexB(avccSampleBuffer, isKeyframe, PAL::CMVideoFormatDescriptionGetH264ParameterSetAtIndex);
}

static bool h264AnnexBSpsIsFollowedByPps(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices, size_t spsIndex)
{
    if (spsIndex + 1 >= naluIndices.size())
        return false;

    auto& ppsIndex = naluIndices[spsIndex + 1];
    return ppsIndex.payloadSize && h264NaluType(data[ppsIndex.payloadStartOffset]) == H264NaluType::Pps;
}

RefPtr<VideoInfo> createVideoInfoFromAVCAnnexBStream(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices)
{
    auto spsIndex = findH264AnnexBSpsIndex(data, naluIndices);
    if (spsIndex == notFound)
        return nullptr;

    if (!h264AnnexBSpsIsFollowedByPps(data, naluIndices, spsIndex)) {
        RELEASE_LOG_ERROR(WebRTC, "createVideoInfoFromAVCAnnexBStream NAL unit following SPS is not PPS");
        return nullptr;
    }

    std::array<std::span<const uint8_t>, 2> paramSets {
        data.subspan(naluIndices[spsIndex].payloadStartOffset, naluIndices[spsIndex].payloadSize),
        data.subspan(naluIndices[spsIndex + 1].payloadStartOffset, naluIndices[spsIndex + 1].payloadSize)
    };
    std::array<const uint8_t*, 2> paramSetPointers { paramSets[0].data(), paramSets[1].data() };
    std::array<size_t, 2> paramSetSizes { paramSets[0].size(), paramSets[1].size() };

    CMFormatDescriptionRef rawDescription = nullptr;
    if (PAL::CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault, paramSetPointers.size(), paramSetPointers.data(), paramSetSizes.data(), 4, &rawDescription) != noErr)
        return nullptr;
    RetainPtr description = adoptCF(rawDescription);
    return createVideoInfoFromFormatDescription(description);
}

Vector<uint8_t> convertAVCAnnexBToLengthPrefixed(std::span<const uint8_t> data, const Vector<NaluIndex>& naluIndices)
{
    // We skip all NAL units up to and including the SPS/PPS pair, if present, as parameter sets belong in the format description, not in the per-sample data.
    size_t startIndex = 0;
    auto spsIndex = findH264AnnexBSpsIndex(data, naluIndices);
    if (spsIndex != notFound) {
        // If we only have a Sps, we skip it and log an error.
        // FIXME: We should probably align convertAVCAnnexBToLengthPrefixed and convertHEVCAnnexBToLengthPrefixed on the exact same behaviour in case of missing sps/pps/vps.
        if (h264AnnexBSpsIsFollowedByPps(data, naluIndices, spsIndex))
            startIndex = spsIndex + 2;
        else {
            RELEASE_LOG_ERROR(WebRTC, "convertAVCAnnexBToLengthPrefixed NAL unit following SPS is not PPS");
            startIndex = spsIndex + 1;
        }
    }

    return annexBToLengthPrefixed(data, naluIndices.subspan(startIndex));
}

} // namespace WebCore

