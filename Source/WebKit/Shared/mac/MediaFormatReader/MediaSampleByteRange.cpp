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

#include "config.h"
#include "MediaSampleByteRange.h"

#if ENABLE(WEBM_FORMAT_READER)

namespace WebKit {

using namespace PAL;
using namespace WebCore;

MediaSampleByteRange::MediaSampleByteRange(MediaSample& sample, MTPluginByteSourceRef byteSource, uint64_t trackID)
    : MediaSampleAVFObjC(nullptr)
    , m_presentationTime(sample.presentationTime())
    , m_decodeTime(sample.decodeTime())
    , m_duration(sample.duration())
    , m_sizeInBytes(sample.sizeInBytes())
    , m_presentationSize(sample.presentationSize())
    , m_flags(sample.flags())
    , m_trackID(trackID)
    , m_byteRange(sample.byteRange())
    , m_byteSource(byteSource)
{
    ASSERT(!isMainThread());
    ASSERT(m_decodeTime == m_presentationTime || m_decodeTime == MediaTime::invalidTime());
    auto platformSample = sample.platformSample();
    switch (platformSample.type) {
    case PlatformSample::CMSampleBufferType:
        m_formatDescription = CMSampleBufferGetFormatDescription(platformSample.sample.cmSampleBuffer);
        break;
    case PlatformSample::ByteRangeSampleType:
        m_formatDescription = platformSample.sample.byteRangeSample.second;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

AtomString MediaSampleByteRange::trackID() const
{
    return AtomString::number(m_trackID);
}

PlatformSample MediaSampleByteRange::platformSample()
{
    return {
        PlatformSample::ByteRangeSampleType,
        { .byteRangeSample = std::make_pair(m_byteSource.get(), m_formatDescription.get()) },
    };
}

void MediaSampleByteRange::offsetTimestampsBy(const MediaTime& offset)
{
    setTimestamps(presentationTime() + offset, decodeTime() + offset);
}

void MediaSampleByteRange::setTimestamps(const MediaTime& presentationTime, const MediaTime& decodeTime)
{
    m_presentationTime = presentationTime;
    m_decodeTime = decodeTime;
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
