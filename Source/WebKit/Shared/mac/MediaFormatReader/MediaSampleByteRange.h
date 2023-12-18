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

#include <WebCore/MediaSample.h>

namespace WebKit {

class MediaSampleByteRange final : public WebCore::MediaSample {
public:
    static Ref<MediaSampleByteRange> create(WebCore::MediaSamplesBlock&& sample, MTPluginByteSourceRef byteSource)
    {
        return adoptRef(*new MediaSampleByteRange(WTFMove(sample), byteSource));
    }

    MediaTime presentationTime() const final;
    MediaTime decodeTime() const final;
    MediaTime duration() const final;
    size_t sizeInBytes() const final;
    WebCore::FloatSize presentationSize() const final;
    SampleFlags flags() const final;
    std::optional<ByteRange> byteRange() const final;

    WebCore::TrackID trackID() const final;
    WebCore::PlatformSample platformSample() const final;
    WebCore::PlatformSample::Type platformSampleType() const final { return WebCore::PlatformSample::ByteRangeSampleType; }
    void offsetTimestampsBy(const MediaTime&) final;
    void setTimestamps(const MediaTime&, const MediaTime&) final;
    Ref<MediaSample> createNonDisplayingCopy() const final { return *const_cast<MediaSampleByteRange*>(this); }

    using const_iterator = WebCore::MediaSamplesBlock::SamplesVector::const_iterator;
    const_iterator begin() const { return m_block.begin(); }
    const_iterator end() const { return m_block.end(); }

private:
    MediaSampleByteRange(WebCore::MediaSamplesBlock&&, MTPluginByteSourceRef);

    const WebCore::MediaSamplesBlock m_block;
    RetainPtr<MTPluginByteSourceRef> m_byteSource;
};

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
