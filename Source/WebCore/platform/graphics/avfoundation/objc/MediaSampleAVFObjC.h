/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/Forward.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/Forward.h>

using CVPixelBufferRef = struct __CVBuffer*;

namespace WebCore {

class PixelBuffer;
class SharedBuffer;

class MediaSampleAVFObjC : public MediaSample {
public:
    static Ref<MediaSampleAVFObjC> create(CMSampleBufferRef sample, uint64_t trackID) { return adoptRef(*new MediaSampleAVFObjC(sample, trackID)); }
    static Ref<MediaSampleAVFObjC> create(CMSampleBufferRef sample, AtomString trackID) { return adoptRef(*new MediaSampleAVFObjC(sample, trackID)); }
    static Ref<MediaSampleAVFObjC> create(CMSampleBufferRef sample, VideoRotation rotation = VideoRotation::None, bool mirrored = false) { return adoptRef(*new MediaSampleAVFObjC(sample, rotation, mirrored)); }
    static RefPtr<MediaSampleAVFObjC> createImageSample(PixelBuffer&&);
    WEBCORE_EXPORT static RefPtr<MediaSampleAVFObjC> createImageSample(RetainPtr<CVPixelBufferRef>&&, VideoRotation, bool mirrored);

    WEBCORE_EXPORT static void setAsDisplayImmediately(MediaSample&);
    static RetainPtr<CMSampleBufferRef> cloneSampleBufferAndSetAsDisplayImmediately(CMSampleBufferRef);

    WEBCORE_EXPORT RefPtr<JSC::Uint8ClampedArray> getRGBAImageData() const override;

    MediaTime presentationTime() const override;
    MediaTime decodeTime() const override;
    MediaTime duration() const override;

    AtomString trackID() const override { return m_id; }
    void setTrackID(const String& id) override { m_id = id; }

    size_t sizeInBytes() const override;
    FloatSize presentationSize() const override;

    SampleFlags flags() const override;
    PlatformSample platformSample() override;
    std::optional<ByteRange> byteRange() const override;
    WEBCORE_EXPORT void dump(PrintStream&) const override;
    void offsetTimestampsBy(const MediaTime&) override;
    void setTimestamps(const MediaTime&, const MediaTime&) override;
    WEBCORE_EXPORT bool isDivisable() const override;
    WEBCORE_EXPORT std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime& presentationTime, UseEndTime) override;
    WEBCORE_EXPORT Ref<MediaSample> createNonDisplayingCopy() const override;

    VideoRotation videoRotation() const override { return m_rotation; }
    bool videoMirrored() const override { return m_mirrored; }
    WEBCORE_EXPORT uint32_t videoPixelFormat() const final;
    WEBCORE_EXPORT std::optional<MediaSampleVideoFrame> videoFrame() const final;
    CMSampleBufferRef sampleBuffer() const { return m_sample.get(); }

    bool isHomogeneous() const;
    Vector<Ref<MediaSampleAVFObjC>> divideIntoHomogeneousSamples();

    void setByteRangeOffset(size_t);

protected:
    WEBCORE_EXPORT MediaSampleAVFObjC(RetainPtr<CMSampleBufferRef>&&);
    WEBCORE_EXPORT MediaSampleAVFObjC(CMSampleBufferRef);
    WEBCORE_EXPORT MediaSampleAVFObjC(CMSampleBufferRef, AtomString trackID);
    WEBCORE_EXPORT MediaSampleAVFObjC(CMSampleBufferRef, uint64_t trackID);
    WEBCORE_EXPORT MediaSampleAVFObjC(CMSampleBufferRef, VideoRotation, bool mirrored);
    WEBCORE_EXPORT virtual ~MediaSampleAVFObjC();

    std::optional<MediaSample::ByteRange> byteRangeForAttachment(CFStringRef key) const;

    RetainPtr<CMSampleBufferRef> m_sample;
    AtomString m_id;
    VideoRotation m_rotation { VideoRotation::None };
    bool m_mirrored { false };
};

} // namespace WebCore
