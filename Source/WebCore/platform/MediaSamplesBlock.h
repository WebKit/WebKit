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

#pragma once

#include <WebCore/MediaSample.h>
#include <WebCore/PlatformMediaCapabilitiesHdrMetadataType.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/TrackInfo.h>
#include <wtf/MediaTime.h>

namespace IPC {
template<typename> struct ArgumentCoder;
}

namespace WebCore {

class MediaSamplesBlock {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MediaSamplesBlock);
public:
    struct MediaSampleItem {
        using MediaSampleDataType = RefPtr<FragmentedSharedBuffer>;
        MediaTime presentationTime;
        MediaTime decodeTime { MediaTime::invalidTime() };
        MediaTime duration { MediaTime::zeroTime() };
        std::pair<MediaTime, MediaTime> trimInterval { MediaTime::zeroTime(), MediaTime::zeroTime() };
        MediaSampleDataType data;
        RefPtr<SharedBuffer> hdrMetadata { nullptr };
        std::optional<PlatformMediaCapabilitiesHdrMetadataType> hdrMetadataType { std::nullopt };
        uint32_t flags { };
        bool isSync() const { return flags & MediaSample::IsSync; }
#if ENABLE(ENCRYPTED_MEDIA)
        int32_t bytesOfClearDataCount { 0 };
        RefPtr<SharedBuffer> cryptorIV { };
        RefPtr<SharedBuffer> cryptorSubsampleAuxiliaryData { };
#endif
    };

    using MediaSampleDataType = MediaSampleItem::MediaSampleDataType;
    using SamplesVector = Vector<MediaSampleItem>;

    MediaSamplesBlock() = default;
    MediaSamplesBlock(const TrackInfo* info, SamplesVector&& items)
        : m_info(info)
        , m_samples(WTF::move(items))
    {
    }

    void setInfo(RefPtr<const TrackInfo>&& info) { m_info = WTF::move(info); }
    const TrackInfo* info() const { return m_info.get(); }
    RefPtr<const TrackInfo> protectedInfo() const { return m_info; }
    MediaTime presentationTime() const { return isEmpty() ? MediaTime::invalidTime() : first().presentationTime; }
    MediaTime duration() const
    {
        MediaTime duration = MediaTime::zeroTime();
        for (auto& sample : *this)
            duration += sample.duration;
        return duration;
    }
    MediaTime presentationEndTime() const { return presentationTime() + duration(); }
    bool isSync() const { return size() ? (first().flags & MediaSample::IsSync) : false; }
    TrackID trackID() const { return m_info ? m_info->trackID() : -1; }
    bool isVideo() const { return m_info && m_info->isVideo(); }
    bool isAudio() const { return m_info && m_info->isAudio(); }
    TrackInfo::TrackType type() const { return m_info ? m_info->type() : TrackInfo::TrackType::Unknown; }
    void append(MediaSampleItem&& item) { m_samples.append(WTF::move(item)); }
    void append(MediaSamplesBlock&& block) { append(std::exchange(block.m_samples, { })); }
    void append(SamplesVector&& samples) { m_samples.appendVector(WTF::move(samples)); }
    size_t size() const { return m_samples.size(); };
    bool isEmpty() const { return m_samples.isEmpty(); }
    void clear() { m_samples.clear(); }
    SamplesVector takeSamples() { return std::exchange(m_samples, { }); }

    // Indicate that this MediaSampleBlock follows a discontinuity from the previous block.
    std::optional<bool> discontinuity() const { return m_discontinuity; }
    void setDiscontinuity(bool discontinuity) { m_discontinuity = discontinuity; }

    const MediaSampleItem& operator[](size_t index) const LIFETIME_BOUND { return m_samples[index]; }
    const MediaSampleItem& first() const LIFETIME_BOUND { return m_samples.first(); }
    const MediaSampleItem& last() const LIFETIME_BOUND { return m_samples.last(); }
    SamplesVector::const_iterator begin() const LIFETIME_BOUND { return m_samples.begin(); }
    SamplesVector::const_iterator end() const LIFETIME_BOUND { return m_samples.end(); }

    WEBCORE_EXPORT RefPtr<MediaSample> toMediaSample(const MediaSample* = nullptr) const;
    WEBCORE_EXPORT static UniqueRef<MediaSamplesBlock> fromMediaSample(const MediaSample&, const TrackInfo* = nullptr);

private:
    // Used by IPC generator
    friend struct IPC::ArgumentCoder<MediaSamplesBlock>;
    MediaSamplesBlock(RefPtr<const TrackInfo>&& info, SamplesVector&& items, std::optional<bool> discontinuity)
        : m_info(WTF::move(info))
        , m_samples(WTF::move(items))
        , m_discontinuity(discontinuity)
    {
    }

    RefPtr<const TrackInfo> m_info;
    SamplesVector m_samples;
    std::optional<bool> m_discontinuity;
};

} // namespace WebCore
