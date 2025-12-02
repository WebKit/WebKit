/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 * Copyright (C) 2015, 2016 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)
#include "MediaSourcePrivate.h"

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/LoggerHelper.h>

namespace WebCore {

class SourceBufferPrivateGStreamer;
class MediaPlayerPrivateGStreamerMSE;
class PlatformTimeRanges;

class MediaSourcePrivateGStreamer final : public MediaSourcePrivate
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<MediaSourcePrivateGStreamer> open(MediaSourcePrivateClient&, MediaPlayerPrivateGStreamerMSE&);
    virtual ~MediaSourcePrivateGStreamer();

    void setPlayer(MediaPlayerPrivateInterface*) final;
    RefPtr<MediaPlayerPrivateInterface> player() const final;

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::GStreamer; }

    AddStatus addSourceBuffer(const ContentType&, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>&) override;

    void durationChanged(const MediaTime&) override;
    void markEndOfStream(EndOfStreamStatus) override;
    void unmarkEndOfStream() override;

    void notifyActiveSourceBuffersChanged() final;

    void startPlaybackIfHasAllTracks();
    bool hasAllTracks() const { return m_hasAllTracks; }

    void detach();

    // Similar to TrackPrivateBaseGStreamer::TrackType, but with a new value (Invalid) for when the codec is
    // not supported on this system, which should result in ParsingFailed error being thrown in SourceBuffer.
    enum StreamType { Audio, Video, Text, Unknown, Invalid };

    struct RegisteredTrack {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RegisteredTrack);
    public:

        RegisteredTrack()
            : id(0)
            , index(0)
            , streamType(Invalid)
        { }

        RegisteredTrack(TrackID id, size_t index, StreamType streamType)
            : id(id)
            , index(index)
            , streamType(streamType)
        { }

        TrackID id;
        size_t index;
        StreamType streamType;
    };

    RegisteredTrack registerTrack(TrackID, StreamType);
    void unregisterTrack(TrackID);

    void willSeek();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    ASCIILiteral logClassName() const override { return "MediaSourcePrivateGStreamer"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    uint64_t nextSourceBufferLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextSourceBufferID); }
#endif

private:
    MediaSourcePrivateGStreamer(MediaSourcePrivateClient&, MediaPlayerPrivateGStreamerMSE&);
    RefPtr<MediaPlayerPrivateGStreamerMSE> platformPlayer() const;

    ThreadSafeWeakPtr<MediaPlayerPrivateGStreamerMSE> m_playerPrivate;
    bool m_hasAllTracks { false };
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

    uint64_t m_nextSourceBufferID { 0 };

    // Stores info on known tracks, so we can:
    // 1) Work around collision in track ID between multiple source buffers.
    //    The registry is placed here to enforce ID uniqueness specifically by player, not by process,
    //    since it's not an issue if multiple players use the same ID, and we want to preserve IDs as much as possible.
    // 2) Assign indices sequentially by track type.
    //    This means the first track of a type will always have index 0,
    //    the next of the same type will be assigned 1, and so on, which:
    //    - matches how MediaPlayerPrivateGStreamer assigns indices
    //    - how {Audio,Video,Text}TrackList stores its tracks
    //    - prevents a potential out-of-bounds crash in TextTrackList
    //    Just like for IDs, we store known indices here to enforce uniqueness by player.
    HashMap<TrackID, RegisteredTrack, WTF::IntHash<TrackID>, WTF::UnsignedWithZeroKeyHashTraits<TrackID>> m_trackRegistry;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaSourcePrivateGStreamer)
static bool isType(const WebCore::MediaSourcePrivate& mediaSource) { return mediaSource.platformType() == WebCore::MediaPlatformType::GStreamer; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
