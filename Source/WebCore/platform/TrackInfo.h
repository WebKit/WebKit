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
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/PlatformVideoColorSpace.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Variant.h>

namespace IPC {
template<typename T, typename> struct ArgumentCoder;
}

namespace WebCore {

using TrackID = uint64_t;
struct AudioInfo;
struct VideoInfo;

enum class TrackInfoTrackType : uint8_t {
    Unknown,
    Audio,
    Video,
    Text
};

String convertEnumerationToString(TrackInfoTrackType);

struct TrackInfo : public ThreadSafeRefCounted<TrackInfo> {
    using TrackType = TrackInfoTrackType;

    bool isAudio() const { return type() == TrackType::Audio; }
    bool isVideo() const { return type() == TrackType::Video; }

    TrackType type() const { return m_type; }

    bool operator==(const TrackInfo& other) const
    {
        if (type() != other.type() || codecName != other.codecName || trackID != other.trackID)
            return false;
        return equalTo(other);
    }

    FourCC codecName;
    String codecString;
    TrackID trackID { 0 };

    virtual ~TrackInfo() = default;

    Variant<Ref<AudioInfo>, Ref<VideoInfo>> toVariant() const
    {
        if (isAudio())
            return const_cast<AudioInfo&>(downcast<AudioInfo>(*this));
        return const_cast<VideoInfo&>(downcast<VideoInfo>(*this));
    }

protected:
    virtual bool equalTo(const TrackInfo& other) const = 0;
    TrackInfo(TrackType type)
        : m_type(type) { }
    TrackInfo(TrackType type, FourCC codecName, const String& codecString, TrackID trackID)
        : codecName(codecName)
        , codecString(codecString)
        , trackID(trackID)
        , m_type(type)
    {
    }

private:
    friend struct IPC::ArgumentCoder<TrackInfo, void>;
    friend struct IPC::ArgumentCoder<AudioInfo, void>;
    friend struct IPC::ArgumentCoder<VideoInfo, void>;
    WEBCORE_EXPORT static Ref<TrackInfo> fromVariant(Variant<Ref<AudioInfo>, Ref<VideoInfo>>);
    const TrackType m_type { TrackType::Unknown };
};

struct VideoInfo : public TrackInfo {
    static Ref<VideoInfo> create() { return adoptRef(*new VideoInfo()); }

    FloatSize size;
    // Size in pixels at which the video is rendered. This is after it has
    // been scaled by its aspect ratio.
    FloatSize displaySize;
    uint8_t bitDepth { 8 };
    PlatformVideoColorSpace colorSpace;

    String boxType;
    RefPtr<SharedBuffer> atomData;

private:
    VideoInfo()
        : TrackInfo(TrackType::Video) { }

    // Used by IPC generator
    friend struct IPC::ArgumentCoder<VideoInfo, void>;
    static Ref<VideoInfo> create(FourCC codecName, const String& codecString, WebCore::TrackID trackID, FloatSize size, FloatSize displaySize, uint8_t bitDepth, PlatformVideoColorSpace colorSpace, const String& boxType, RefPtr<SharedBuffer>&& atomData)
    {
        return adoptRef(*new VideoInfo(codecName, codecString, trackID, size, displaySize, bitDepth, colorSpace, boxType, WTFMove(atomData)));
    }

    VideoInfo(FourCC codecName, const String& codecString, WebCore::TrackID trackID, FloatSize size, FloatSize displaySize, uint8_t bitDepth, PlatformVideoColorSpace colorSpace, const String& boxType, RefPtr<SharedBuffer>&& atomData)
        : TrackInfo(TrackType::Video, codecName, codecString, trackID)
        , size(size)
        , displaySize(displaySize)
        , bitDepth(bitDepth)
        , colorSpace(colorSpace)
        , boxType(boxType)
        , atomData(WTFMove(atomData))
    {
    }

    bool equalTo(const TrackInfo& otherVideoInfo) const final
    {
        auto& other = downcast<const VideoInfo>(otherVideoInfo);
        return size == other.size && displaySize == other.displaySize && bitDepth == other.bitDepth && colorSpace == other.colorSpace && ((!atomData && !other.atomData) || (atomData && other.atomData && *atomData == *other.atomData));
    }
};

struct AudioInfo : public TrackInfo {
    static Ref<AudioInfo> create() { return adoptRef(*new AudioInfo()); }

    uint32_t rate { 0 };
    uint32_t channels { 0 };
    uint32_t framesPerPacket { 0 };
    uint8_t bitDepth { 16 };

    RefPtr<SharedBuffer> cookieData;

private:
    AudioInfo()
        : TrackInfo(TrackType::Audio) { }

    // Used by IPC generator
    friend struct IPC::ArgumentCoder<AudioInfo, void>;
    static Ref<AudioInfo> create(FourCC codecName, const String& codecString, TrackID trackID, uint32_t rate, uint32_t channels, uint32_t framesPerPacket, uint8_t bitDepth, RefPtr<SharedBuffer>&& cookieData)
    {
        return adoptRef(*new AudioInfo(codecName, codecString, trackID, rate, channels, framesPerPacket, bitDepth, WTFMove(cookieData)));
    }

    AudioInfo(FourCC codecName, const String& codecString, TrackID trackID, uint32_t rate, uint32_t channels, uint32_t framesPerPacket, uint8_t bitDepth, RefPtr<SharedBuffer>&& cookieData)
        : TrackInfo(TrackType::Audio, codecName, codecString, trackID)
        , rate(rate)
        , channels(channels)
        , framesPerPacket(framesPerPacket)
        , bitDepth(bitDepth)
        , cookieData(WTFMove(cookieData))
    {
    }

    bool equalTo(const TrackInfo& otherAudioInfo) const final
    {
        auto& other = downcast<const AudioInfo>(otherAudioInfo);
        return rate == other.rate && channels == other.channels && bitDepth == other.bitDepth && framesPerPacket == other.framesPerPacket && ((!cookieData && !other.cookieData) || (cookieData && other.cookieData && *cookieData == *other.cookieData));
    }
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
