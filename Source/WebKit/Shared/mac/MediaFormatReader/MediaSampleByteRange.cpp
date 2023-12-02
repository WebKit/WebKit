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
#include <pal/cf/CoreMediaSoftLink.h>

#if ENABLE(WEBM_FORMAT_READER)

namespace WebKit {

using namespace WebCore;

MediaSampleByteRange::MediaSampleByteRange(MediaSamplesBlock&& sample, MTPluginByteSourceRef byteSource)
    : m_block(WTFMove(sample))
    , m_byteSource(byteSource)
{
    ASSERT(!m_block.isEmpty());
    ASSERT(std::holds_alternative<MediaSample::ByteRange>(m_block.last().data));
}

TrackID MediaSampleByteRange::trackID() const
{
    return m_block.info()->trackID;
}

PlatformSample MediaSampleByteRange::platformSample() const
{
    ASSERT(m_block.info());
    return {
        PlatformSample::ByteRangeSampleType,
        { .byteRangeSample = std::make_pair(m_byteSource.get(), std::reference_wrapper(*m_block.info())) },
    };
}

MediaTime MediaSampleByteRange::presentationTime() const
{
    return m_block.last().presentationTime;
}

MediaTime MediaSampleByteRange::decodeTime() const
{
    return m_block.last().decodeTime;
}

MediaTime MediaSampleByteRange::duration() const
{
    return m_block.last().duration;
}

size_t MediaSampleByteRange::sizeInBytes() const
{
    return std::get<MediaSample::ByteRange>(m_block.last().data).byteLength;
}

WebCore::FloatSize MediaSampleByteRange::presentationSize() const
{
    if (m_block.isVideo())
        return downcast<const VideoInfo>(m_block.info())->displaySize;
    return { };
}

MediaSampleByteRange::SampleFlags MediaSampleByteRange::flags() const
{
    return m_block.last().flags;
}

std::optional<MediaSampleByteRange::ByteRange> MediaSampleByteRange::byteRange() const
{
    return std::get<MediaSample::ByteRange>(m_block.last().data);
}

void MediaSampleByteRange::offsetTimestampsBy(const MediaTime&)
{
    ASSERT_NOT_REACHED();
}

void MediaSampleByteRange::setTimestamps(const MediaTime&, const MediaTime&)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
