/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#if ENABLE(MEDIA_SOURCE)

#include <WebCore/MediaPlayer.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/TrackInfo.h>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContentType;
class MediaPlayerPrivateInterface;
class SourceBufferPrivate;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
class LegacyCDMSession;
#endif
enum class MediaSourceReadyState;
struct MediaSourceConfiguration;

enum class MediaSourcePrivateAddStatus : uint8_t {
    Ok,
    NotSupported,
    ReachedIdLimit,
    InvalidState
};

enum class MediaSourcePrivateEndOfStreamStatus : uint8_t {
    NoError,
    NetworkError,
    DecodeError
};

class WEBCORE_EXPORT MediaSourcePrivate
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaSourcePrivate> {
public:
    typedef Vector<String> CodecsArray;

    using AddStatus = MediaSourcePrivateAddStatus;
    using EndOfStreamStatus = MediaSourcePrivateEndOfStreamStatus;

    explicit MediaSourcePrivate(MediaSourcePrivateClient&);
    virtual ~MediaSourcePrivate();

    RefPtr<MediaSourcePrivateClient> client() const;
    virtual RefPtr<MediaPlayerPrivateInterface> player() const = 0;
    virtual void setPlayer(MediaPlayerPrivateInterface*) = 0;
    virtual void shutdown();
    // Implementation override must be thread-safe. For the base implementation to be thread-safe, player() must be a ThreadSafeRefCounted object.
    virtual MediaTime currentTime() const;
    virtual bool timeIsProgressing() const;

    virtual constexpr MediaPlatformType platformType() const = 0;
    virtual AddStatus addSourceBuffer(const ContentType&, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>&) = 0;
    virtual void removeSourceBuffer(SourceBufferPrivate&);
    Vector<Ref<SourceBufferPrivate>> sourceBuffers() const;
    void sourceBufferPrivateDidChangeActiveState(SourceBufferPrivate&, bool active);
    virtual void notifyActiveSourceBuffersChanged() = 0;
    virtual void durationChanged(const MediaTime&); // Base class method must be called in overrides. Must be thread-safe
    virtual void bufferedChanged(const PlatformTimeRanges&); // Base class method must be called in overrides. Must be thread-safe.
    void trackBufferedChanged(SourceBufferPrivate&, Vector<PlatformTimeRanges>&&);

    MediaPlayer::ReadyState mediaPlayerReadyState() const;
    virtual void setMediaPlayerReadyState(MediaPlayer::ReadyState);
    virtual void markEndOfStream(EndOfStreamStatus);
    virtual void unmarkEndOfStream() { m_isEnded = false; }
    bool isEnded() const { return m_isEnded; }

    virtual MediaSourceReadyState readyState() const { return m_readyState; }
    virtual void setReadyState(MediaSourceReadyState readyState) { m_readyState = readyState; }
    void setLiveSeekableRange(const PlatformTimeRanges&);
    const PlatformTimeRanges& liveSeekableRange() const;
    void clearLiveSeekableRange();

    Ref<MediaTimePromise> waitForTarget(const SeekTarget&);
    void seekToTime(const MediaTime&);

    virtual void setTimeFudgeFactor(const MediaTime& fudgeFactor) { m_timeFudgeFactor = fudgeFactor; }
    MediaTime timeFudgeFactor() const { return m_timeFudgeFactor; }

    MediaTime duration() const;
    PlatformTimeRanges buffered() const;
    PlatformTimeRanges seekable() const;

    bool hasBufferedData() const;
    bool hasFutureTime(const MediaTime& currentTime) const;
    static constexpr MediaTime futureDataThreshold() { return MediaTime { 1001, 24000 }; }
    bool hasFutureTime(const MediaTime& currentTime, const MediaTime& threshold) const;
    bool hasAudio() const;
    bool hasVideo() const;
    using TracksType = OptionSet<TrackInfoTrackType>;
    void tracksTypeChanged(SourceBufferPrivate&, TracksType);
    virtual bool supportsTracksTypeChanged() const { return false; }

    void setStreaming(bool value) { m_streaming = value; }
    bool streaming() const { return m_streaming; }
    void setStreamingAllowed(bool value) { m_streamingAllowed = value; }
    bool streamingAllowed() const { return m_streamingAllowed; }

protected:
    MediaSourcePrivate(MediaSourcePrivateClient&, WorkQueue&);
    void ensureOnDispatcher(Function<void()>&&) const;
    void ensureOnDispatcherSync(NOESCAPE Function<void()>&&) const;

    mutable Lock m_lock;
    // FIXME: This should be a Vector<Ref<SourceBufferPrivate>>
    Vector<RefPtr<SourceBufferPrivate>> m_sourceBuffers WTF_GUARDED_BY_LOCK(m_lock);
    Vector<SourceBufferPrivate*> m_activeSourceBuffers WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    std::atomic<bool> m_isEnded { false }; // Set on MediaSource's dispatcher.
    std::atomic<MediaSourceReadyState> m_readyState; // Set on MediaSource's dispatcher.
    std::atomic<WebCore::MediaPlayer::ReadyState> m_mediaPlayerReadyState { WebCore::MediaPlayer::ReadyState::HaveNothing };

    const Ref<WorkQueue> m_dispatcher; // SerialFunctionDispatcher the SourceBufferPrivate/MediaSourcePrivate is running on.

private:
    void updateBufferedRanges();
    void updateTracksType();

    MediaTime m_duration WTF_GUARDED_BY_LOCK(m_lock) { MediaTime::invalidTime() };
    PlatformTimeRanges m_buffered WTF_GUARDED_BY_LOCK(m_lock);
    HashMap<SourceBufferPrivate*, Vector<PlatformTimeRanges>> m_bufferedRanges;
    PlatformTimeRanges m_liveSeekable WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<bool> m_streaming { false };
    std::atomic<bool> m_streamingAllowed { false };
    MediaTime m_timeFudgeFactor;
    HashMap<SourceBufferPrivate*, TracksType> m_tracksTypes WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    std::atomic<TracksType> m_tracksCombinedTypes;
    const ThreadSafeWeakPtr<MediaSourcePrivateClient> m_client;
};

String convertEnumerationToString(MediaSourcePrivate::AddStatus);
String convertEnumerationToString(MediaSourcePrivate::EndOfStreamStatus);

} // namespace WebCore

namespace WTF {

template<typename Type> struct LogArgument;

template <>
struct LogArgument<WebCore::MediaSourcePrivate::AddStatus> {
    static String toString(const WebCore::MediaSourcePrivate::AddStatus status)
    {
        return convertEnumerationToString(status);
    }
};

template <>
struct LogArgument<WebCore::MediaSourcePrivate::EndOfStreamStatus> {
    static String toString(const WebCore::MediaSourcePrivate::EndOfStreamStatus status)
    {
        return convertEnumerationToString(status);
    }
};

} // namespace WTF

#endif
