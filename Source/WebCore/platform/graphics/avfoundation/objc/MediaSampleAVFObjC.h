/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "MediaSample.h"
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/Forward.h>

namespace WebCore {

class MediaSampleAVFObjC : public MediaSample {
public:
    static Ref<MediaSampleAVFObjC> create(CMSampleBufferRef sample, int trackID) { return adoptRef(*new MediaSampleAVFObjC(sample, trackID)); }
    static Ref<MediaSampleAVFObjC> create(CMSampleBufferRef sample, AtomString trackID) { return adoptRef(*new MediaSampleAVFObjC(sample, trackID)); }
    static Ref<MediaSampleAVFObjC> create(CMSampleBufferRef sample, VideoRotation rotation = VideoRotation::None, bool mirrored = false) { return adoptRef(*new MediaSampleAVFObjC(sample, rotation, mirrored)); }
    static RefPtr<MediaSampleAVFObjC> createImageSample(Vector<uint8_t>&&, unsigned long width, unsigned long height);

    RefPtr<JSC::Uint8ClampedArray> getRGBAImageData() const override;

    MediaTime presentationTime() const override;
    MediaTime outputPresentationTime() const override;
    MediaTime decodeTime() const override;
    MediaTime duration() const override;
    MediaTime outputDuration() const override;

    AtomString trackID() const override { return m_id; }
    void setTrackID(const String& id) override { m_id = id; }

    size_t sizeInBytes() const override;
    FloatSize presentationSize() const override;

    SampleFlags flags() const override;
    PlatformSample platformSample() override;
    void dump(PrintStream&) const override;
    void offsetTimestampsBy(const MediaTime&) override;
    void setTimestamps(const MediaTime&, const MediaTime&) override;
    bool isDivisable() const override;
    std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime& presentationTime) override;
    Ref<MediaSample> createNonDisplayingCopy() const override;

    VideoRotation videoRotation() const override { return m_rotation; }
    bool videoMirrored() const override { return m_mirrored; }
    uint32_t videoPixelFormat() const final;

    CMSampleBufferRef sampleBuffer() const { return m_sample.get(); }

    String toJSONString() const override;

protected:
    MediaSampleAVFObjC(RetainPtr<CMSampleBufferRef>&& sample)
        : m_sample(WTFMove(sample))
    {
    }
    MediaSampleAVFObjC(CMSampleBufferRef sample)
        : m_sample(sample)
    {
    }
    MediaSampleAVFObjC(CMSampleBufferRef sample, AtomString trackID)
        : m_sample(sample)
        , m_id(trackID)
    {
    }
    MediaSampleAVFObjC(CMSampleBufferRef sample, int trackID)
        : m_sample(sample)
        , m_id(AtomString::number(trackID))
    {
    }
    MediaSampleAVFObjC(CMSampleBufferRef sample, VideoRotation rotation, bool mirrored)
        : m_sample(sample)
        , m_rotation(rotation)
        , m_mirrored(mirrored)
    {
    }

    virtual ~MediaSampleAVFObjC() = default;
    RetainPtr<CMSampleBufferRef> m_sample;
    AtomString m_id;
    VideoRotation m_rotation { VideoRotation::None };
    bool m_mirrored { false };
};

} // namespace WebCore

