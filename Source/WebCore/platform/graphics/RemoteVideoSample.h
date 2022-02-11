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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "IOSurface.h"
#include "MediaSample.h"
#include "RemoteVideoSample.h"
#include <wtf/MachSendRight.h>
#include <wtf/MediaTime.h>

typedef struct __CVBuffer* CVPixelBufferRef;

namespace WebCore {

class ProcessIdentity;

class RemoteVideoSample {
public:
    RemoteVideoSample() = default;
    RemoteVideoSample(RemoteVideoSample&&) = default;
    RemoteVideoSample& operator=(RemoteVideoSample&&) = default;
    ~RemoteVideoSample() = default;

    enum class ShouldCheckForIOSurface { No, Yes };
    WEBCORE_EXPORT static std::unique_ptr<RemoteVideoSample> create(MediaSample&, ShouldCheckForIOSurface = ShouldCheckForIOSurface::Yes);
    WEBCORE_EXPORT static std::unique_ptr<RemoteVideoSample> create(RetainPtr<CVPixelBufferRef>&&, MediaTime&& presentationTime, MediaSample::VideoRotation = MediaSample::VideoRotation::None, ShouldCheckForIOSurface = ShouldCheckForIOSurface::Yes);

    WEBCORE_EXPORT IOSurfaceRef surface() const;
    CVPixelBufferRef imageBuffer() const { return m_imageBuffer.get(); }

    void setOwnershipIdentity(const ProcessIdentity&);
    void clearIOSurface() { m_ioSurface = nullptr; }

    const MediaTime& time() const { return m_time; }
    uint32_t videoFormat() const { return m_videoFormat; }
    IntSize size() const { return m_size; }
    MediaSample::VideoRotation rotation() const { return m_rotation; }
    bool mirrored() const { return m_mirrored; }

    template<class Encoder> void encode(Encoder& encoder) const
    {
        if (m_ioSurface)
            encoder << m_ioSurface->createSendRight();
        else
            encoder << MachSendRight();
        encoder << m_rotation;
        encoder << m_time;
        encoder << m_videoFormat;
        encoder << m_size;
        encoder << m_mirrored;
    }

    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder& decoder, RemoteVideoSample& sample)
    {
        MachSendRight sendRight;
        if (!decoder.decode(sendRight))
            return false;
        sample.m_sendRight = WTFMove(sendRight);

        MediaSample::VideoRotation rotation;
        if (!decoder.decode(rotation))
            return false;
        sample.m_rotation = rotation;

        MediaTime time;
        if (!decoder.decode(time))
            return false;
        sample.m_time = WTFMove(time);

        uint32_t format;
        if (!decoder.decode(format))
            return false;
        sample.m_videoFormat = format;

        IntSize size;
        if (!decoder.decode(size))
            return false;
        sample.m_size = WTFMove(size);

        bool mirrored;
        if (!decoder.decode(mirrored))
            return false;
        sample.m_mirrored = mirrored;

        return true;
    }

private:
    RemoteVideoSample(IOSurfaceRef, RetainPtr<CVPixelBufferRef>&&, const DestinationColorSpace&, MediaTime&&, MediaSample::VideoRotation, bool);

    std::unique_ptr<WebCore::IOSurface> m_ioSurface;
    RetainPtr<CVPixelBufferRef> m_imageBuffer;
    MachSendRight m_sendRight;
    MediaSample::VideoRotation m_rotation { MediaSample::VideoRotation::None };
    MediaTime m_time;
    uint32_t m_videoFormat { 0 };
    IntSize m_size;
    bool m_mirrored { false };
};

inline void RemoteVideoSample::setOwnershipIdentity(const ProcessIdentity& resourceOwner)
{
    if (m_ioSurface)
        m_ioSurface->setOwnershipIdentity(resourceOwner);
}

}

#endif // ENABLE(MEDIA_STREAM)
