/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "MediaSampleConverter.h"

#include <WebCore/MediaSample.h>
#include <WebCore/MediaSamplesBlock.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include <CoreMedia/CMFormatDescription.h>

#include <pal/cf/CoreMediaSoftLink.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSampleConverter);

MediaSampleConverter::MediaSampleConverter() = default;
MediaSampleConverter::~MediaSampleConverter() = default;

static bool hasSameInitSegment(const MediaSample& sampleA, const MediaSample& sampleB)
{
#if PLATFORM(COCOA)
    RetainPtr cmSampleA = sampleA.platformSample().cmSampleBuffer();
    RetainPtr cmSampleB = sampleB.platformSample().cmSampleBuffer();
    RetainPtr descriptionA = PAL::CMSampleBufferGetFormatDescription(cmSampleA.get());
    RetainPtr descriptionB = PAL::CMSampleBufferGetFormatDescription(cmSampleB.get());
    return descriptionA == descriptionB;
#else
    UNUSED_PARAM(sampleA);
    UNUSED_PARAM(sampleB);
    return false;
#endif
}

UniqueRef<MediaSamplesBlock> MediaSampleConverter::convert(const MediaSample& sample, SetTrackInfo setTrackInfo)
{
    bool canReuseLastTrackInfo = m_lastSample && hasSameInitSegment(sample, Ref { *m_lastSample });
    auto block = MediaSamplesBlock::fromMediaSample(sample, canReuseLastTrackInfo ? m_lastTrackInfo.get() : nullptr);
    if (!canReuseLastTrackInfo) {
        m_lastTrackInfo = block->info();
        m_lastSample = &sample;
    }
    if (setTrackInfo == SetTrackInfo::No)
        block->setInfo(nullptr);
    return block;
}

RefPtr<MediaSample> MediaSampleConverter::convert(MediaSamplesBlock&& block)
{
    ASSERT(m_lastTrackInfo || block.info());
    if (!block.info())
        block.setInfo(m_lastTrackInfo.get());
    RefPtr sample = block.toMediaSample(m_lastSample.get());
    if (!m_lastSample)
        m_lastSample = sample;
    return sample;
}

bool MediaSampleConverter::hasFormatChanged(const MediaSample& sample)
{
    if (RefPtr lastSample = m_lastSample)
        return !hasSameInitSegment(*lastSample, sample);
    return true;
}

RefPtr<const TrackInfo> MediaSampleConverter::currentTrackInfo() const
{
    return m_lastTrackInfo;
}

void MediaSampleConverter::setTrackInfo(Ref<const TrackInfo>&& trackInfo)
{
    if (m_lastTrackInfo && trackInfo.get() != *m_lastTrackInfo)
        m_lastSample = nullptr;
    m_lastTrackInfo = WTF::move(trackInfo);
}

}
