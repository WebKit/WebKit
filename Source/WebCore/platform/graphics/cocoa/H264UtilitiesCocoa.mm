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

#import "BitReader.h"
#import "TrackInfo.h"

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
        paramSets.append({ avcc.subspan(reader.byteOffset() - 1, *size) });
        paramSets.last()[0] &= ~kNAL_REF_IDC_SEQ_PARAM_SET;
        if (!reader.skipBytes(*size - kNALTypeSize))
            return nullptr;
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
        paramSets.append({ avcc.subspan(reader.byteOffset() - 1, *size) });
        paramSets.last()[0] |= kNAL_REF_IDC_PIC_PARAM_SET;
        if (!reader.skipBytes(*size - kNALTypeSize))
            return nullptr;
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
    auto dimensions = PAL::CMVideoFormatDescriptionGetDimensions(rawDescription);
    auto presentationDimensions = PAL::CMVideoFormatDescriptionGetPresentationDimensions(rawDescription, true, true);

    Ref info = VideoInfo::create();
    info->codecName = kCMVideoCodecType_H264;
    info->size = { static_cast<float>(dimensions.width), static_cast<float>(dimensions.height) };
    info->displaySize = { static_cast<float>(presentationDimensions.width), static_cast<float>(presentationDimensions.height) };
    info->atomData = SharedBuffer::create(avcc);

    return info;
}

} // namespace WebCore

