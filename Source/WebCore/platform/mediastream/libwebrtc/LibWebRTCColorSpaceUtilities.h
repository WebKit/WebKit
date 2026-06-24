/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if USE(LIBWEBRTC)

#include <WebCore/PlatformVideoColorSpace.h>
#include <WebCore/VideoFrame.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/video/video_frame.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

static inline std::optional<webrtc::ColorSpace> toWebRTCColorSpace(const PlatformVideoColorSpace& colorSpace)
{
    if (!colorSpace.isValid())
        return { };

    auto primaryID = [](auto primaries) {
        switch (primaries) {
        case PlatformVideoColorPrimaries::Bt709:
            return webrtc::ColorSpace::PrimaryID::kBT709;
        case PlatformVideoColorPrimaries::Bt470bg:
            return webrtc::ColorSpace::PrimaryID::kBT470BG;
        case PlatformVideoColorPrimaries::Smpte170m:
            return webrtc::ColorSpace::PrimaryID::kSMPTE170M;
        case PlatformVideoColorPrimaries::Bt470m:
            return webrtc::ColorSpace::PrimaryID::kBT470M;
        case PlatformVideoColorPrimaries::Smpte240m:
            return webrtc::ColorSpace::PrimaryID::kSMPTE240M;
        case PlatformVideoColorPrimaries::Film:
            return webrtc::ColorSpace::PrimaryID::kFILM;
        case PlatformVideoColorPrimaries::Bt2020:
            return webrtc::ColorSpace::PrimaryID::kBT2020;
        case PlatformVideoColorPrimaries::SmpteSt4281:
            return webrtc::ColorSpace::PrimaryID::kSMPTEST428;
        case PlatformVideoColorPrimaries::SmpteRp431:
            return webrtc::ColorSpace::PrimaryID::kSMPTEST431;
        case PlatformVideoColorPrimaries::SmpteEg432:
            return webrtc::ColorSpace::PrimaryID::kSMPTEST432;
        case PlatformVideoColorPrimaries::JedecP22Phosphors:
            return webrtc::ColorSpace::PrimaryID::kJEDECP22;
        case PlatformVideoColorPrimaries::Unspecified:
            return webrtc::ColorSpace::PrimaryID::kUnspecified;
        }
        return webrtc::ColorSpace::PrimaryID::kUnspecified;
    }(colorSpace.primaries.value_or(PlatformVideoColorPrimaries::Unspecified));

    auto transferID = [](auto transfer) {
        switch (transfer) {
        case PlatformVideoTransferCharacteristics::Bt709:
            return webrtc::ColorSpace::TransferID::kBT709;
        case PlatformVideoTransferCharacteristics::Smpte170m:
            return webrtc::ColorSpace::TransferID::kSMPTE170M;
        case PlatformVideoTransferCharacteristics::Iec6196621:
            return webrtc::ColorSpace::TransferID::kIEC61966_2_1;
        case PlatformVideoTransferCharacteristics::Gamma22curve:
            return webrtc::ColorSpace::TransferID::kGAMMA22;
        case PlatformVideoTransferCharacteristics::Gamma28curve:
            return webrtc::ColorSpace::TransferID::kGAMMA28;
        case PlatformVideoTransferCharacteristics::Smpte240m:
            return webrtc::ColorSpace::TransferID::kSMPTE240M;
        case PlatformVideoTransferCharacteristics::Linear:
            return webrtc::ColorSpace::TransferID::kLINEAR;
        case PlatformVideoTransferCharacteristics::Log:
            return webrtc::ColorSpace::TransferID::kLOG;
        case PlatformVideoTransferCharacteristics::LogSqrt:
            return webrtc::ColorSpace::TransferID::kLOG_SQRT;
        case PlatformVideoTransferCharacteristics::Iec6196624:
            return webrtc::ColorSpace::TransferID::kIEC61966_2_4;
        case PlatformVideoTransferCharacteristics::Bt1361ExtendedColourGamut:
            return webrtc::ColorSpace::TransferID::kBT1361_ECG;
        case PlatformVideoTransferCharacteristics::Bt2020_10bit:
            return webrtc::ColorSpace::TransferID::kBT2020_10;
        case PlatformVideoTransferCharacteristics::Bt2020_12bit:
            return webrtc::ColorSpace::TransferID::kBT2020_12;
        case PlatformVideoTransferCharacteristics::SmpteSt2084:
            return webrtc::ColorSpace::TransferID::kSMPTEST2084;
        case PlatformVideoTransferCharacteristics::SmpteSt4281:
            return webrtc::ColorSpace::TransferID::kSMPTEST428;
        case PlatformVideoTransferCharacteristics::AribStdB67Hlg:
            return webrtc::ColorSpace::TransferID::kARIB_STD_B67;
        case PlatformVideoTransferCharacteristics::Unspecified:
            return webrtc::ColorSpace::TransferID::kUnspecified;
        }
        return webrtc::ColorSpace::TransferID::kUnspecified;
    }(colorSpace.transfer.value_or(PlatformVideoTransferCharacteristics::Unspecified));

    auto matrixID = [](auto matrix) {
        switch (matrix) {
        case PlatformVideoMatrixCoefficients::Rgb:
            return webrtc::ColorSpace::MatrixID::kRGB;
        case PlatformVideoMatrixCoefficients::Bt709:
            return webrtc::ColorSpace::MatrixID::kBT709;
        case PlatformVideoMatrixCoefficients::Bt470bg:
            return webrtc::ColorSpace::MatrixID::kBT470BG;
        case PlatformVideoMatrixCoefficients::Smpte170m:
            return webrtc::ColorSpace::MatrixID::kSMPTE170M;
        case PlatformVideoMatrixCoefficients::Smpte240m:
            return webrtc::ColorSpace::MatrixID::kSMPTE240M;
        case PlatformVideoMatrixCoefficients::Fcc:
            return webrtc::ColorSpace::MatrixID::kFCC;
        case PlatformVideoMatrixCoefficients::YCgCo:
            return webrtc::ColorSpace::MatrixID::kYCOCG;
        case PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance:
            return webrtc::ColorSpace::MatrixID::kBT2020_NCL;
        case PlatformVideoMatrixCoefficients::Bt2020ConstantLuminance:
            return webrtc::ColorSpace::MatrixID::kBT2020_CL;
        case PlatformVideoMatrixCoefficients::Unspecified:
            return webrtc::ColorSpace::MatrixID::kUnspecified;
        }
        return webrtc::ColorSpace::MatrixID::kUnspecified;
    }(colorSpace.matrix.value_or(PlatformVideoMatrixCoefficients::Unspecified));

    auto rangeID = [](auto fullRange) {
        if (!fullRange)
            return webrtc::ColorSpace::RangeID::kInvalid;
        return *fullRange ? webrtc::ColorSpace::RangeID::kFull : webrtc::ColorSpace::RangeID::kLimited;
    }(colorSpace.fullRange);

    return webrtc::ColorSpace { primaryID, transferID, matrixID, rangeID };
}

} // namespace WebCore

#endif
