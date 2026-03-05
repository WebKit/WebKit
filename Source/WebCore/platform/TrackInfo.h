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

#include <WebCore/FloatSize.h>
#include <WebCore/FourCC.h>
#include <WebCore/ImmersiveVideoMetadata.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/PlatformVideoColorSpace.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Variant.h>

namespace IPC {
template<typename> struct ArgumentCoder;
}

namespace WebCore {

using TrackID = uint64_t;
class AudioInfo;
class VideoInfo;

enum class TrackInfoTrackType : uint8_t {
    Unknown,
    Audio,
    Video,
    Text
};

String convertEnumerationToString(TrackInfoTrackType);

enum class EncryptionBoxType : uint8_t {
    CommonEncryptionTrackEncryptionBox,
    TransportStreamEncryptionInitData
};

using TrackInfoAtomData = std::pair<FourCC, Ref<SharedBuffer>>;
#if ENABLE(ENCRYPTED_MEDIA)
using TrackInfoEncryptionData = std::pair<EncryptionBoxType, Ref<SharedBuffer>>;
using TrackInfoEncryptionInitData = TrackInfoAtomData;

struct EncryptionDataCollection {
    TrackInfoEncryptionData encryptionData;
    std::optional<FourCC> encryptionOriginalFormat;
    Vector<TrackInfoEncryptionInitData> encryptionInitDatas { };

    bool operator==(const EncryptionDataCollection&) const = default;
};
#endif

struct TrackInfoData {
    const FourCC codecName { };
    const String codecString { };
    TrackID trackID { 0 };

#if ENABLE(ENCRYPTED_MEDIA)
    std::optional<EncryptionDataCollection> encryptionData { };
#endif

    bool operator==(const TrackInfoData&) const = default;
};

class TrackInfo : public ThreadSafeRefCounted<TrackInfo> {
public:
    virtual ~TrackInfo() = default;
    using TrackType = TrackInfoTrackType;

    bool isAudio() const { return type() == TrackType::Audio; }
    bool isVideo() const { return type() == TrackType::Video; }

    TrackType type() const { return m_type; }

    bool operator==(const TrackInfo& other) const
    {
        if (m_data != other.m_data)
            return false;
        return equalTo(other);
    }

    FourCC codecName() const { return m_data.codecName; }
    const String& codecString() const { return m_data.codecString; }

    TrackID trackID() const { return m_data.trackID; };
    void setTrackID(TrackID trackID) { m_data.trackID = trackID; }

    using AtomData = TrackInfoAtomData;
#if ENABLE(ENCRYPTED_MEDIA)
    const std::optional<EncryptionDataCollection>& encryptionDataCollection() const { return m_data.encryptionData; }
#endif

protected:
    TrackInfo(TrackType type, TrackInfoData&& data)
        : m_data(WTF::move(data))
        , m_type(type)
    {
    }

    virtual bool equalTo(const TrackInfo& other) const = 0;
    const TrackInfoData& trackInfoData() const { return m_data; }
    TrackInfoData& trackInfoData() { return m_data; }

private:
    TrackInfoData m_data;

    friend struct IPC::ArgumentCoder<TrackInfo>;
    Variant<Ref<AudioInfo>, Ref<VideoInfo>> toVariant() const
    {
        if (isAudio())
            return const_cast<AudioInfo&>(downcast<AudioInfo>(*this));
        return const_cast<VideoInfo&>(downcast<VideoInfo>(*this));
    }

    WEBCORE_EXPORT static Ref<TrackInfo> fromVariant(Variant<Ref<AudioInfo>, Ref<VideoInfo>>);
    const TrackType m_type { TrackType::Unknown };
};

struct VideoSpecificInfoData {
    FloatSize size { };
    // Size in pixels at which the video is rendered. This is after it has
    // been scaled by its aspect ratio.
    FloatSize displaySize { };
    uint8_t bitDepth { 8 };
    PlatformVideoColorSpace colorSpace { };
    Vector<TrackInfo::AtomData> extensionAtoms { };

#if PLATFORM(VISION)
    std::optional<ImmersiveVideoMetadata> immersiveVideoMetadata { };
#endif

    bool operator==(const VideoSpecificInfoData&) const = default;
};

using VideoInfoData = std::pair<TrackInfoData, VideoSpecificInfoData>;

class VideoInfo : public TrackInfo {
public:
    static Ref<VideoInfo> create(VideoInfoData&& data) { return adoptRef(*new VideoInfo(WTF::move(data))); }

    const FloatSize& size() const { return m_data.size; }
    // Size in pixels at which the video is rendered. This is after it has
    // been scaled by its aspect ratio.
    const FloatSize& displaySize() const { return m_data.displaySize; }
    uint8_t bitDepth() const { return m_data.bitDepth; }
    const PlatformVideoColorSpace& colorSpace() const { return m_data.colorSpace; }

    const Vector<AtomData>& extensionAtoms() const { return m_data.extensionAtoms; }

#if PLATFORM(VISION)
    const std::optional<ImmersiveVideoMetadata>& immersiveVideoMetadata() const { return m_data.immersiveVideoMetadata; }
#endif

    VideoInfoData toVideoInfoData() const
    {
        return { trackInfoData(), m_data };
    }

private:
    const VideoSpecificInfoData m_data;

private:
    explicit VideoInfo(VideoInfoData&& data)
        : TrackInfo(TrackType::Video, WTF::move(data.first))
        , m_data(WTF::move(data.second))
    {
    }

    bool equalTo(const TrackInfo& otherVideoInfo) const final
    {
        auto& other = downcast<const VideoInfo>(otherVideoInfo);
        return m_data == other.m_data;
    }
};

struct AudioSpecificInfoData {
    uint32_t rate { 0 };
    uint32_t channels { 0 };
    uint32_t framesPerPacket { 0 };
    uint8_t bitDepth { 16 };

    RefPtr<SharedBuffer> cookieData { };

    bool operator==(const AudioSpecificInfoData&) const = default;
};

using AudioInfoData = std::pair<TrackInfoData, AudioSpecificInfoData>;

class AudioInfo : public TrackInfo {
public:
    static Ref<AudioInfo> create(AudioInfoData&& data) { return adoptRef(*new AudioInfo(WTF::move(data))); }

    uint32_t rate() const { return m_data.rate; }
    uint32_t channels() const { return m_data.channels; }
    uint32_t framesPerPacket() const { return m_data.framesPerPacket; }
    uint8_t bitDepth() const { return m_data.bitDepth; }

    RefPtr<SharedBuffer> cookieData() const { return m_data.cookieData; }

    AudioInfoData toAudioInfoData() const
    {
        return { trackInfoData(), m_data };
    }

private:
    explicit AudioInfo(AudioInfoData&& data)
        : TrackInfo(TrackType::Audio, WTF::move(data.first))
        , m_data(WTF::move(data.second))
    {
    }

    bool equalTo(const TrackInfo& otherAudioInfo) const final
    {
        auto& other = downcast<const AudioInfo>(otherAudioInfo);
        return m_data == other.m_data;
    }
    const AudioSpecificInfoData m_data;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoInfo)
    static bool isType(const WebCore::TrackInfo& info) { return info.isVideo(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AudioInfo)
    static bool isType(const WebCore::TrackInfo& info) { return info.isAudio(); }
SPECIALIZE_TYPE_TRAITS_END()

namespace WTF {

template <>
struct LogArgument<WebCore::TrackInfoTrackType> {
    static String toString(const WebCore::TrackInfoTrackType type)
    {
        return convertEnumerationToString(type);
    }
};

}
