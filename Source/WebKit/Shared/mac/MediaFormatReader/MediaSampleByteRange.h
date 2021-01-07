/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEBM_FORMAT_READER)

#include <WebCore/MediaSampleAVFObjC.h>
#include <pal/spi/cocoa/MediaToolboxSPI.h>
#include <wtf/MediaTime.h>
#include <wtf/RetainPtr.h>

namespace WebKit {

class MediaSampleByteRange final : public WebCore::MediaSampleAVFObjC {
public:
    static Ref<MediaSampleByteRange> create(WebCore::MediaSample& sample, MTPluginByteSourceRef byteSource, uint64_t trackID)
    {
        return adoptRef(*new MediaSampleByteRange(sample, byteSource, trackID));
    }

    MediaTime presentationTime() const final { return m_presentationTime; }
    MediaTime decodeTime() const final { return m_decodeTime; }
    MediaTime duration() const final { return m_duration; }
    size_t sizeInBytes() const final { return m_sizeInBytes; }
    FloatSize presentationSize() const final { return m_presentationSize; }
    SampleFlags flags() const final { return m_flags; }
    Optional<ByteRange> byteRange() const final { return m_byteRange; }

    AtomString trackID() const final;
    PlatformSample platformSample() final;
    void offsetTimestampsBy(const MediaTime&) final;
    void setTimestamps(const MediaTime&, const MediaTime&) final;

private:
    MediaSampleByteRange(MediaSample&, MTPluginByteSourceRef, uint64_t trackID);

    MediaTime m_presentationTime;
    MediaTime m_decodeTime;
    MediaTime m_duration;
    size_t m_sizeInBytes;
    FloatSize m_presentationSize;
    SampleFlags m_flags;
    uint64_t m_trackID;
    Optional<ByteRange> m_byteRange;
    RetainPtr<MTPluginByteSourceRef> m_byteSource;
    RetainPtr<CMFormatDescriptionRef> m_formatDescription;
};

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
