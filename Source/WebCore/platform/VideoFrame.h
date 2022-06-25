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

#pragma once

#if ENABLE(VIDEO)

#include "FloatSize.h"
#include <JavaScriptCore/TypedArrays.h>
#include <wtf/MediaTime.h>
#include <wtf/ThreadSafeRefCounted.h>

typedef struct __CVBuffer *CVPixelBufferRef;

namespace WebCore {

class ProcessIdentity;
#if USE(AVFOUNDATION) && PLATFORM(COCOA)
class VideoFrameCV;
#endif

// A class representing a video frame from a decoder, capture source, or similar.
class VideoFrame : public ThreadSafeRefCounted<VideoFrame> {
public:
    virtual ~VideoFrame() = default;

    enum class Rotation {
        None = 0,
        UpsideDown = 180,
        Right = 90,
        Left = 270,
    };

    MediaTime presentationTime() const { return m_presentationTime; }
    Rotation rotation() const { return m_rotation; }
    bool isMirrored() const { return m_isMirrored; }

#if PLATFORM(COCOA) && USE(AVFOUNDATION)
    WEBCORE_EXPORT RefPtr<VideoFrameCV> asVideoFrameCV();
#endif
    WEBCORE_EXPORT RefPtr<JSC::Uint8ClampedArray> getRGBAImageData() const;

    virtual FloatSize presentationSize() const = 0;
    virtual uint32_t pixelFormat() const = 0;

    virtual bool isRemoteProxy() const { return false; }
    virtual bool isLibWebRTC() const { return false; }
    virtual bool isCV() const { return false; }
#if USE(GSTREAMER)
    virtual bool isGStreamer() const { return false; }
#endif
#if PLATFORM(COCOA)
    virtual CVPixelBufferRef pixelBuffer() const { return nullptr; };
#endif
    WEBCORE_EXPORT virtual void setOwnershipIdentity(const ProcessIdentity&) { }

    void initializeCharacteristics(MediaTime presentationTime, bool isMirrored, Rotation);

protected:
    WEBCORE_EXPORT VideoFrame(MediaTime presentationTime, bool isMirrored, Rotation);

private:
    const MediaTime m_presentationTime;
    const bool m_isMirrored;
    const Rotation m_rotation;
};

}

namespace WTF {

template<> struct EnumTraits<WebCore::VideoFrame::Rotation> {
    using values = EnumValues<
        WebCore::VideoFrame::Rotation,
        WebCore::VideoFrame::Rotation::None,
        WebCore::VideoFrame::Rotation::UpsideDown,
        WebCore::VideoFrame::Rotation::Right,
        WebCore::VideoFrame::Rotation::Left
    >;
};

}

#endif // ENABLE(VIDEO)
