/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "VideoFrameLibWebRTC.h"

#if PLATFORM(COCOA) && USE(LIBWEBRTC)

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {

static PlatformVideoColorSpace defaultVPXColorSpace()
{
    return { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Iec6196621, PlatformVideoMatrixCoefficients::Smpte170m, false };
}

Ref<VideoFrameLibWebRTC> VideoFrameLibWebRTC::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, std::optional<PlatformVideoColorSpace>&& colorSpace, rtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
{
    auto finalColorSpace = WTFMove(colorSpace).value_or(defaultVPXColorSpace());
    return adoptRef(*new VideoFrameLibWebRTC(presentationTime, isMirrored, rotation, WTFMove(finalColorSpace), WTFMove(buffer), WTFMove(conversionCallback)));
}

std::optional<PlatformVideoColorSpace> VideoFrameLibWebRTC::colorSpaceFromFrame(const webrtc::VideoFrame& frame)
{
    auto webrtColorSpace = frame.color_space();
    if (!webrtColorSpace || webrtColorSpace->primaries() == webrtc::ColorSpace::PrimaryID::kUnspecified)
        return { };

    std::optional<PlatformVideoColorPrimaries> primaries;
    switch (webrtColorSpace->primaries()) {
    case webrtc::ColorSpace::PrimaryID::kBT709:
        primaries = PlatformVideoColorPrimaries::Bt709;
        break;
    case webrtc::ColorSpace::PrimaryID::kBT470BG:
        primaries = PlatformVideoColorPrimaries::Bt470bg;
        break;
    case webrtc::ColorSpace::PrimaryID::kSMPTE240M:
        primaries = PlatformVideoColorPrimaries::Smpte240m;
        break;
    case webrtc::ColorSpace::PrimaryID::kFILM:
        primaries = PlatformVideoColorPrimaries::Film;
        break;
    case webrtc::ColorSpace::PrimaryID::kBT2020:
        primaries = PlatformVideoColorPrimaries::Bt2020;
        break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST428:
        primaries = PlatformVideoColorPrimaries::SmpteSt4281;
        break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST431:
        primaries = PlatformVideoColorPrimaries::SmpteRp431;
        break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST432:
        primaries = PlatformVideoColorPrimaries::SmpteEg432;
        break;
    case webrtc::ColorSpace::PrimaryID::kJEDECP22:
        primaries = PlatformVideoColorPrimaries::JedecP22Phosphors;
        break;
    case webrtc::ColorSpace::PrimaryID::kUnspecified:
    case webrtc::ColorSpace::PrimaryID::kBT470M:
    case webrtc::ColorSpace::PrimaryID::kSMPTE170M:
        break;
    };

    std::optional<PlatformVideoTransferCharacteristics> transfer;
    switch (webrtColorSpace->transfer()) {
    case webrtc::ColorSpace::TransferID::kBT709:
        transfer = PlatformVideoTransferCharacteristics::Bt709;
        break;
    case webrtc::ColorSpace::TransferID::kGAMMA22:
        transfer = PlatformVideoTransferCharacteristics::Gamma22curve;
        break;
    case webrtc::ColorSpace::TransferID::kGAMMA28:
        transfer = PlatformVideoTransferCharacteristics::Gamma28curve;
        break;
    case webrtc::ColorSpace::TransferID::kSMPTE170M:
        transfer = PlatformVideoTransferCharacteristics::Smpte170m;
        break;
    case webrtc::ColorSpace::TransferID::kSMPTE240M:
        transfer = PlatformVideoTransferCharacteristics::Smpte240m;
        break;
    case webrtc::ColorSpace::TransferID::kLINEAR:
        transfer = PlatformVideoTransferCharacteristics::Linear;
        break;
    case webrtc::ColorSpace::TransferID::kLOG:
        transfer = PlatformVideoTransferCharacteristics::Log;
        break;
    case webrtc::ColorSpace::TransferID::kLOG_SQRT:
        transfer = PlatformVideoTransferCharacteristics::LogSqrt;
        break;
    case webrtc::ColorSpace::TransferID::kIEC61966_2_4:
        transfer = PlatformVideoTransferCharacteristics::Iec6196624;
        break;
    case webrtc::ColorSpace::TransferID::kBT1361_ECG:
        transfer = PlatformVideoTransferCharacteristics::Bt1361ExtendedColourGamut;
        break;
    case webrtc::ColorSpace::TransferID::kBT2020_10:
        transfer = PlatformVideoTransferCharacteristics::Bt2020_10bit;
        break;
    case webrtc::ColorSpace::TransferID::kBT2020_12:
        transfer = PlatformVideoTransferCharacteristics::Bt2020_12bit;
        break;
    case webrtc::ColorSpace::TransferID::kSMPTEST2084:
        transfer = PlatformVideoTransferCharacteristics::SmpteSt2084;
        break;
    case webrtc::ColorSpace::TransferID::kSMPTEST428:
        transfer = PlatformVideoTransferCharacteristics::SmpteSt4281;
        break;
    case webrtc::ColorSpace::TransferID::kARIB_STD_B67:
        transfer = PlatformVideoTransferCharacteristics::AribStdB67Hlg;
        break;
    case webrtc::ColorSpace::TransferID::kIEC61966_2_1:
    case webrtc::ColorSpace::TransferID::kUnspecified:
        break;
    };

    std::optional<PlatformVideoMatrixCoefficients> matrix;
    switch (webrtColorSpace->matrix()) {
    case webrtc::ColorSpace::MatrixID::kRGB:
        matrix = PlatformVideoMatrixCoefficients::Rgb;
        break;
    case webrtc::ColorSpace::MatrixID::kBT709:
        matrix = PlatformVideoMatrixCoefficients::Bt709;
        break;
    case webrtc::ColorSpace::MatrixID::kFCC:
        matrix = PlatformVideoMatrixCoefficients::Fcc;
        break;
    case webrtc::ColorSpace::MatrixID::kBT470BG:
        matrix = PlatformVideoMatrixCoefficients::Bt470bg;
        break;
    case webrtc::ColorSpace::MatrixID::kSMPTE170M:
        matrix = PlatformVideoMatrixCoefficients::Smpte170m;
        break;
    case webrtc::ColorSpace::MatrixID::kSMPTE240M:
        matrix = PlatformVideoMatrixCoefficients::Smpte240m;
        break;
    case webrtc::ColorSpace::MatrixID::kYCOCG:
        matrix = PlatformVideoMatrixCoefficients::YCgCo;
        break;
    case webrtc::ColorSpace::MatrixID::kBT2020_NCL:
        matrix = PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance;
        break;
    case webrtc::ColorSpace::MatrixID::kBT2020_CL:
        matrix = PlatformVideoMatrixCoefficients::Bt2020ConstantLuminance;
        break;
    case webrtc::ColorSpace::MatrixID::kUnspecified:
    case webrtc::ColorSpace::MatrixID::kSMPTE2085:
    case webrtc::ColorSpace::MatrixID::kCDNCLS:
    case webrtc::ColorSpace::MatrixID::kCDCLS:
    case webrtc::ColorSpace::MatrixID::kBT2100_ICTCP:
        break;
    };

    std::optional<bool> fullRange;
    switch (webrtColorSpace->range()) {
    case webrtc::ColorSpace::RangeID::kLimited:
        fullRange = false;
        break;
    case webrtc::ColorSpace::RangeID::kFull:
        fullRange = true;
        break;
    case webrtc::ColorSpace::RangeID::kInvalid:
    case webrtc::ColorSpace::RangeID::kDerived:
        break;
    };

    return PlatformVideoColorSpace { primaries, transfer, matrix, fullRange };
}

VideoFrameLibWebRTC::VideoFrameLibWebRTC(MediaTime presentationTime, bool isMirrored, Rotation rotation, PlatformVideoColorSpace&& colorSpace, rtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
    : VideoFrame(presentationTime, isMirrored, rotation, WTFMove(colorSpace))
    , m_buffer(WTFMove(buffer))
    , m_size(m_buffer->width(),  m_buffer->height())
    , m_conversionCallback(WTFMove(conversionCallback))
{
    switch (m_buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420:
        m_videoPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI010:
        m_videoPixelFormat = kCVPixelFormatType_420YpCbCr10BiPlanarFullRange;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

CVPixelBufferRef VideoFrameLibWebRTC::pixelBuffer() const
{
    Locker locker { m_pixelBufferLock };
    if (!m_pixelBuffer && m_conversionCallback)
        m_pixelBuffer = std::exchange(m_conversionCallback, { })(*m_buffer);
    return m_pixelBuffer.get();
}

}
#endif // PLATFORM(COCOA) && USE(LIBWEBRTC)
