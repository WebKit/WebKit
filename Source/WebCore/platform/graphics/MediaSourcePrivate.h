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
#include <WebCore/MediaPromiseTypes.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/TrackInfo.h>
#include <wtf/Forward.h>
#include <wtf/NativePromise.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContentType;
class MediaPlayerPrivateInterface;
class SourceBufferPrivate;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
class LegacyCDMSession;
#endif

struct MediaSourceConfiguration;

enum class MediaSourceReadyState : uint8_t {
    Closed,
    Open,
    Ended
};

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
    void shutdown();
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
    virtual void bufferedChanged(PlatformTimeRanges&&); // Base class method must be called in overrides. Must be thread-safe.
    void trackBufferedChanged(SourceBufferPrivate&, Vector<PlatformTimeRanges>&&);

    // Implements the HTMLMediaElement.buffered cross-buffer step:
    // https://w3c.github.io/media-source/#htmlmediaelement-extensions-buffered
    // Caller passes the per-active-SourceBuffer ranges (each itself the result
    // of SourceBufferPrivate::computeBufferedRanges) and whether the
    // MediaSource readyState is "ended". Used by both MediaSource (main) on
    // readyState/dirty triggers and MediaSourcePrivate (dispatcher) when track
    // ranges change so a single algorithm produces the value.
    WEBCORE_EXPORT static PlatformTimeRanges computeBufferedRanges(const Vector<PlatformTimeRanges>& activeRanges, bool ended);

    MediaPlayer::ReadyState NODELETE mediaPlayerReadyState() const;
    void setMediaPlayerReadyState(MediaPlayer::ReadyState);
    virtual void markEndOfStream(EndOfStreamStatus);
    virtual void unmarkEndOfStream() { m_isEnded = false; }
    bool isEnded() const { return m_isEnded; }

    virtual MediaSourceReadyState readyState() const { return m_readyState; }
    void setReadyState(MediaSourceReadyState readyState) { m_readyState = readyState; }
    void setLiveSeekableRange(const PlatformTimeRanges&);
    const PlatformTimeRanges& liveSeekableRange() const;
    void clearLiveSeekableRange();

    Ref<MediaTimePromise> waitForTarget(const SeekTarget&);
    Ref<GenericPromise> reenqueueMediaForTime(const MediaTime&);
    bool isReenqueuePending() const { return m_reenqueuePending; }
    void clearReenqueuePending() { m_reenqueuePending = false; }
    void cancelPendingWaitForTarget();

    // Single source of truth for canplaythrough / gap-handling policy across
    // the MSE pipeline. All values are durations.
    //   - startGapAllowance: per MSE 2 §presentation-start-time, allow up to
    //     1s gap from time 0 to the first buffered range to count as
    //     "buffered" for HAVE_FUTURE_DATA / canplay; HTMLMediaElement.buffered
    //     reported to JS is unaffected.
    //   - midStreamGapTolerance: default tolerance for gaps mid-stream;
    //     gaps below this are skipped.
    //   - audioCoveredGapTolerance: widened tolerance applied when an audio
    //     track bridges a gap (typically a video-only gap with audio
    //     continuous through it).
    static constexpr MediaTime startGapAllowance() { return { 1, 1 }; }
    static constexpr MediaTime midStreamGapTolerance() { return { 125, 1000 }; }
    static constexpr MediaTime audioCoveredGapTolerance() { return { 250, 1000 }; }

    // Returns the gap tolerance that should apply at `time`:
    //   - startGapAllowance if `time` is in the start-of-stream window;
    //   - audioCoveredGapTolerance if some active audio track other than
    //     `excluded` has `time` inside its buffered range;
    //   - midStreamGapTolerance otherwise.
    //
    // When `excluded` is unset the implementation reads a lock-protected
    // audio-buffered cache and may be called from any thread. When
    // `excluded` is set the caller MUST be on the dispatcher (used by
    // TrackBuffer's IsCoveredByOtherTracks callback).
    MediaTime gapToleranceAtTime(const MediaTime&, std::optional<TrackID> excluded = std::nullopt) const;
    bool isWithinStartGapAllowance(const MediaTime&) const;

    MediaTime duration() const;
    PlatformTimeRanges buffered() const;
    // Compares the argument against m_buffered under m_lock without copying
    // m_buffered into the caller. Useful for short-circuit checks on the main
    // thread which would otherwise pay a full PlatformTimeRanges copy via buffered().
    bool isBufferedEqual(const PlatformTimeRanges&) const;
    bool isBuffered(const PlatformTimeRanges&) const;
    PlatformTimeRanges seekable() const;

    MediaTime nextStallTime(const MediaTime& currentTime) const;
    bool hasBufferedData() const;
    bool hasCurrentTime() const;
    bool hasFutureTime() const;
    bool hasFutureTime(const MediaTime& currentTime) const;
    static constexpr MediaTime futureDataThreshold() { return MediaTime { 1001, 24000 }; }
    bool hasFutureTime(const MediaTime& currentTime, const MediaTime& threshold) const;
    bool NODELETE hasAudio() const;
    bool NODELETE hasVideo() const;
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
    bool canCompleteWaitForTarget() const WTF_REQUIRES_CAPABILITY(m_dispatcher.get());
    void completeWaitForTarget() WTF_REQUIRES_CAPABILITY(m_dispatcher.get());
    void tryCompleteWaitForTarget() WTF_REQUIRES_CAPABILITY(m_dispatcher.get());
    bool hasBufferedTime(const MediaTime&) const;

    MediaTime m_duration WTF_GUARDED_BY_LOCK(m_lock) { MediaTime::invalidTime() };
    PlatformTimeRanges m_buffered WTF_GUARDED_BY_LOCK(m_lock);
    PlatformTimeRanges m_audioBuffered WTF_GUARDED_BY_LOCK(m_lock);
    std::optional<SeekTarget> m_pendingSeekTarget WTF_GUARDED_BY_LOCK(m_lock);
    std::optional<MediaTimePromise::AutoRejectProducer> m_waitForTargetPromise WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    HashMap<SourceBufferPrivate*, Vector<PlatformTimeRanges>> m_bufferedRanges;
    PlatformTimeRanges m_liveSeekable WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<bool> m_streaming { false };
    std::atomic<bool> m_streamingAllowed { false };
    HashMap<SourceBufferPrivate*, TracksType> m_tracksTypes WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    std::atomic<TracksType> m_tracksCombinedTypes;
    const ThreadSafeWeakPtr<MediaSourcePrivateClient> m_client;
    std::atomic<bool> m_reenqueuePending { false };
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
