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

#include <CoreVideo/CoreVideo.h>
#include <WebCore/PlanarImageConverter.h>
#include <wtf/Borrow.h>
#include <wtf/CanBorrow.h>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class PixelBufferLock final {
    WTF_MAKE_NONCOPYABLE(PixelBufferLock);
public:
    WEBCORE_EXPORT PixelBufferLock(CVPixelBufferRef, CVPixelBufferLockFlags);
    WEBCORE_EXPORT ~PixelBufferLock();

    explicit operator bool() const { return m_isLocked; }

private:
    const RetainPtr<CVPixelBufferRef> m_pixelBuffer;
    const CVPixelBufferLockFlags m_flags;
    const bool m_isLocked;
};

// Tri-planar formats CoreVideo doesn't define, registered by WebKit. Their values are 'W', then 'f' or 'v' for full or video range,
// '0', '2' or '4' for 4:2:0, 4:2:2 or 4:4:4, and '8' or 's' for 8-bit alpha (with 8-bit samples) or 16-bit alpha (with 10-bit samples).
// 16-bit alpha is stored like the color samples: MSB aligned, so only the top 10 or 12 bits are used and opaque is 0xFFC0 or 0xFFF0.
inline constexpr OSType pixelFormatWK420YpCbCr8FullRange8ATriPlanar = 'Wf08';
inline constexpr OSType pixelFormatWK422YpCbCr8FullRange8ATriPlanar = 'Wf28';
inline constexpr OSType pixelFormatWK444YpCbCr8FullRange8ATriPlanar = 'Wf48';
inline constexpr OSType pixelFormatWK420YpCbCr10VideoRange16ATriPlanar = 'Wv0s';
inline constexpr OSType pixelFormatWK420YpCbCr10FullRange16ATriPlanar = 'Wf0s';
inline constexpr OSType pixelFormatWK422YpCbCr10FullRange16ATriPlanar = 'Wf2s';
inline constexpr OSType pixelFormatWK444YpCbCr10FullRange16ATriPlanar = 'Wf4s';

namespace PlanarImageConverter {

// Bi-planar Y'CbCr formats, and the tri-planar ones that add a full size alpha plane. The tri-planar formats CoreVideo
// doesn't define are registered with CoreVideo, in the calling process, by the first pixelFormatDescription() or pixelFormat() call.
struct YUVPixelFormatDescription {
    OSType pixelFormat { 0 };
    ChromaSubsampling subsampling { ChromaSubsampling::HorizontalAndVertical };
    SampleFormat color { };
    bool isFullRange { false };
    std::optional<SampleFormat> alpha { };
};

WEBCORE_EXPORT std::optional<YUVPixelFormatDescription> pixelFormatDescription(OSType);
WEBCORE_EXPORT std::optional<OSType> pixelFormat(ChromaSubsampling, SampleFormat color, bool isFullRange, std::optional<SampleFormat> alpha = { });

// The color sample format of the pixel buffer formats that hold samples of the given format without loss. 12-bit samples
// use the 10-bit formats: stored MSB aligned, all 12 bits are kept.
WEBCORE_EXPORT std::optional<SampleFormat> pixelBufferSampleFormat(SampleFormat);

template<typename Byte>
struct YUVPixelBufferPlanes {
    YUVPixelFormatDescription description;
    Plane<Byte> y;
    Plane<Byte> uv;
    std::optional<Plane<Byte>> alpha;
};

// Locks a pixel buffer of a format pixelFormatDescription() knows. Its planes stay valid while it is alive; read them
// through a Borrow, so that destroying it while they are in use crashes:
//     ReadOnlyYUVPixelBufferPlanes lockedPlanes { pixelBuffer };
//     Borrow planes = lockedPlanes;
//     use(planes->y());
class ReadOnlyYUVPixelBufferPlanes final : public CanBorrow {
    WTF_MAKE_NONCOPYABLE(ReadOnlyYUVPixelBufferPlanes);
    WTF_MAKE_NONMOVABLE(ReadOnlyYUVPixelBufferPlanes);
public:
    WEBCORE_EXPORT explicit ReadOnlyYUVPixelBufferPlanes(CVPixelBufferRef);

    explicit operator bool() const { return !!m_planes; }
    const YUVPixelFormatDescription& description() const LIFETIME_BOUND { return m_planes->description; }
    const SourcePlane& y() const LIFETIME_BOUND { return m_planes->y; }
    const SourcePlane& uv() const LIFETIME_BOUND { return m_planes->uv; }
    const std::optional<SourcePlane>& alpha() const LIFETIME_BOUND { return m_planes->alpha; }

private:
    PixelBufferLock m_lock;
    std::optional<YUVPixelBufferPlanes<const uint8_t>> m_planes;
};

class WritableYUVPixelBufferPlanes final : public CanBorrow {
    WTF_MAKE_NONCOPYABLE(WritableYUVPixelBufferPlanes);
    WTF_MAKE_NONMOVABLE(WritableYUVPixelBufferPlanes);
public:
    WEBCORE_EXPORT explicit WritableYUVPixelBufferPlanes(CVPixelBufferRef);

    explicit operator bool() const { return !!m_planes; }
    const YUVPixelFormatDescription& description() const LIFETIME_BOUND { return m_planes->description; }
    const DestinationPlane& y() const LIFETIME_BOUND { return m_planes->y; }
    const DestinationPlane& uv() const LIFETIME_BOUND { return m_planes->uv; }
    const std::optional<DestinationPlane>& alpha() const LIFETIME_BOUND { return m_planes->alpha; }

private:
    PixelBufferLock m_lock;
    std::optional<YUVPixelBufferPlanes<uint8_t>> m_planes;
};

// Separate Y, U, V and optional alpha planes, as in WebCodecs' I420 to I444AP12.
template<typename Byte>
struct PlanarFrame {
    ChromaSubsampling subsampling { ChromaSubsampling::HorizontalAndVertical };
    bool isFullRange { false };
    Plane<Byte> y;
    Plane<Byte> u;
    Plane<Byte> v;
    std::optional<Plane<Byte>> alpha;
};

using SourceFrame = PlanarFrame<const uint8_t>;
using DestinationFrame = PlanarFrame<uint8_t>;

// Y and interleaved CbCr planes, as in WebCodecs' NV12.
template<typename Byte>
struct BiPlanarFrame {
    ChromaSubsampling subsampling { ChromaSubsampling::HorizontalAndVertical };
    bool isFullRange { false };
    Plane<Byte> y;
    Plane<Byte> uv;
};

using SourceBiPlanarFrame = BiPlanarFrame<const uint8_t>;
using DestinationBiPlanarFrame = BiPlanarFrame<uint8_t>;

// When the frame and the pixel buffer have different ranges, the samples are converted as copyPlane() allows.

// The pixel buffer has the frame's size and chroma subsampling.
WEBCORE_EXPORT bool copyFrame(const SourceFrame&, CVPixelBufferRef destination);

// Copies the frame's size from (left, top) of the pixel buffer. Chroma is downsampled when the frame is more subsampled than the pixel buffer.
// alpha, when set, is read instead of the pixel buffer's alpha plane; it has the pixel buffer's size.
WEBCORE_EXPORT bool copyFrame(CVPixelBufferRef source, size_t left, size_t top, const DestinationFrame&, const std::optional<SourcePlane>& alpha = { });

// The pixel buffer has the frame's chroma subsampling; the first overload also has its size.
WEBCORE_EXPORT bool copyFrame(const SourceBiPlanarFrame&, CVPixelBufferRef destination);
WEBCORE_EXPORT bool copyFrame(CVPixelBufferRef source, size_t left, size_t top, const DestinationBiPlanarFrame&);

// Between a 4-channel 8-bit plane and a 32-bit packed pixel buffer, as permuteChannels() does. The plane has the pixel buffer's size,
// or is read from (left, top) of it.
WEBCORE_EXPORT bool copyToPixelBuffer(const SourcePlane&, CVPixelBufferRef destination, std::array<uint8_t, 4> channelMap);
WEBCORE_EXPORT bool copyFromPixelBuffer(CVPixelBufferRef source, size_t left, size_t top, const DestinationPlane&, std::array<uint8_t, 4> channelMap, std::optional<uint8_t> opaqueChannel = { });

// The pixel buffers have the same size and chroma subsampling. Alpha planes are not copied.
WEBCORE_EXPORT bool copyColorPlanes(CVPixelBufferRef source, CVPixelBufferRef destination);

// The luma of alphaSource becomes the alpha. Alpha spans the whole sample range whatever range the luma is tagged with.
WEBCORE_EXPORT bool copyLumaToAlphaPlane(CVPixelBufferRef alphaSource, CVPixelBufferRef destination);
WEBCORE_EXPORT bool copyLumaToAlpha(CVPixelBufferRef alphaSource, const DestinationPlane&);

// Overwrites the alpha of a 32BGRA pixel buffer with the 8-bit alpha plane of source.
WEBCORE_EXPORT bool copyAlphaPlaneToBGRA(CVPixelBufferRef source, CVPixelBufferRef destination);

} // namespace PlanarImageConverter

} // namespace WebCore
