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

#include "MediaSample.h"

namespace WebCore {

#if USE(AVFOUNDATION)
class VideoFrameCV;
#endif


// A class representing a video frame from a decoder, capture source, or similar.
// FIXME: Currently for implementation purposes inherts from MediaSample until capture code
// stops referring to MediaSample
class VideoFrame : public MediaSample {
public:
    WEBCORE_EXPORT ~VideoFrame();

    // WebCore::MediaSample overrides.
    WEBCORE_EXPORT MediaTime presentationTime() const final;
    WEBCORE_EXPORT VideoRotation videoRotation() const final;
    WEBCORE_EXPORT bool videoMirrored() const final;
    // FIXME: When VideoFrame is not MediaSample, these will not be needed.
    WEBCORE_EXPORT WebCore::PlatformSample platformSample() const final;
    WEBCORE_EXPORT PlatformSample::Type platformSampleType() const final;

    virtual bool isRemoteProxy() const { return false; }
#if USE(AVFOUNDATION)
    virtual bool isCV() const { return false; }
    virtual RefPtr<WebCore::VideoFrameCV> asVideoFrameCV() = 0;
#endif

protected:
    WEBCORE_EXPORT VideoFrame(MediaTime presentationTime, bool isMirrored, VideoRotation);
    const MediaTime m_presentationTime;
    const bool m_isMirrored;
    const VideoRotation m_rotation;

private:
    // FIXME: These are not intended to be used for these objects.
    // WebCore::MediaSample overrides.
    WEBCORE_EXPORT MediaTime decodeTime() const final;
    WEBCORE_EXPORT MediaTime duration() const final;
    WEBCORE_EXPORT AtomString trackID() const final;
    WEBCORE_EXPORT size_t sizeInBytes() const final;
    WEBCORE_EXPORT void offsetTimestampsBy(const MediaTime&) final;
    WEBCORE_EXPORT void setTimestamps(const MediaTime&, const MediaTime&) final;
    WEBCORE_EXPORT bool isDivisable() const final;
    WEBCORE_EXPORT std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime& presentationTime, UseEndTime = UseEndTime::DoNotUse) final;
    WEBCORE_EXPORT Ref<WebCore::MediaSample> createNonDisplayingCopy() const final;
    WEBCORE_EXPORT SampleFlags flags() const final;
    WEBCORE_EXPORT std::optional<ByteRange> byteRange() const final;
    WEBCORE_EXPORT void dump(PrintStream&) const final;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoFrame)
    static bool isType(const WebCore::MediaSample& mediaSample) { return mediaSample.platformSampleType() == WebCore::PlatformSample::VideoFrameType; }
SPECIALIZE_TYPE_TRAITS_END()
#endif
