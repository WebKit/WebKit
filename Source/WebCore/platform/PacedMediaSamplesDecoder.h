/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/MediaSamplesDecoder.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>

namespace WebCore {

class TrackInfo;

// Wraps a MediaSamplesDecoder and keeps what it has decoded between a low and a high watermark ahead of the current
// time: 100ms and 166ms for video (the decode window of VideoMediaSampleRenderer), 0.5s and 1s for audio.
// isReadyForMoreMediaData() turns false once more than 30 blocks are waiting to be decoded and true again once 15 or
// fewer are; blocks given while it is false are queued until they are needed.
// decode() and drain() promises stay pending until their input has been decoded. Any error is fatal.
// All methods must be called on the dispatcher given to create().
class PacedMediaSamplesDecoder final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PacedMediaSamplesDecoder> {
    WTF_MAKE_TZONE_ALLOCATED(PacedMediaSamplesDecoder);
public:
    using Filters = MediaSamplesDecoder::Filters;
    using CreatePromise = NativePromise<Ref<PacedMediaSamplesDecoder>, PlatformMediaError>;
    using DecodePromise = MediaSamplesDecoder::DecodePromise;
    // The current time and the rate it advances at (zero when it isn't advancing).
    struct PlaybackPosition {
        MediaTime time;
        double rate { 0 };
    };
    // Called on the dispatcher given to create(), so it must be callable from any thread.
    using CurrentTimeProvider = Function<PlaybackPosition()>;

    WEBCORE_EXPORT static Ref<CreatePromise> create(const TrackInfo&, Filters, GuaranteedSerialFunctionDispatcher&, CurrentTimeProvider&&);
    WEBCORE_EXPORT ~PacedMediaSamplesDecoder();

    WEBCORE_EXPORT bool canContinueWith(const TrackInfo&) const;

    // minimumUpcomingTime is the lowest presentation time of the video frames still to come, not finite when it isn't
    // known yet; nullopt when frames arrive in presentation order.
    WEBCORE_EXPORT Ref<DecodePromise> decode(MediaSamplesBlock&&, std::optional<MediaTime> minimumUpcomingTime = std::nullopt);
    WEBCORE_EXPORT Ref<DecodePromise> drain();
    WEBCORE_EXPORT Ref<MediaPromise> flush();

    WEBCORE_EXPORT bool isReadyForMoreMediaData() const;
    WEBCORE_EXPORT Ref<GenericPromise> requestMediaDataWhenReady();
    WEBCORE_EXPORT void stopRequestingMediaData();

    // To be called when the current time jumped or its rate changed.
    WEBCORE_EXPORT void currentTimeDidChange();

private:
    struct Entry {
        bool isDrain { false };
        MediaSamplesBlock block;
        std::optional<MediaTime> minimumUpcomingTime;
        Vector<MediaSamplesBlock> output;
        DecodePromise::AutoRejectProducer producer { PlatformMediaError::Cancelled };
    };

    PacedMediaSamplesDecoder(Ref<GuaranteedSerialFunctionDispatcher>&&, Ref<MediaSamplesDecoder>&&, bool isVideo, CurrentTimeProvider&&);

    Ref<DecodePromise> enqueue(Entry&&);
    void pump();
    bool shouldDecodeAudioNow(Seconds& waitHint);
    bool shouldDecodeVideoNow(const Entry&, Seconds& waitHint);
    void submit(Ref<DecodePromise>&&);
    void didSettle(uint32_t generation, DecodePromise::Result&&);
    void recordOutput(const Vector<MediaSamplesBlock>&);
    void fail(PlatformMediaError);
    void clearPending();
    void scheduleWake(Seconds waitHint);
    void wake(uint32_t wakeID);
    void updateReadiness();

    const Ref<GuaranteedSerialFunctionDispatcher> m_dispatcher;
    const Ref<MediaSamplesDecoder> m_decoder;
    const CurrentTimeProvider m_currentTime;
    const bool m_isVideo;
    Deque<Entry> m_pending WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    bool m_isDecoding WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { false };
    uint32_t m_generation WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { 0 };
    std::optional<PlatformMediaError> m_error WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    Vector<MediaTime> m_decodedPresentationTimes WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    MediaTime m_decodedAudioEndTime WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { MediaTime::invalidTime() };
    // Blocks given to decode() that haven't been handed to the decoder yet.
    size_t m_queuedBlockCount WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { 0 };
    bool m_idledAtHighWaterMark WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { false };
    std::optional<MonotonicTime> m_wakeTime WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    uint32_t m_wakeID WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { 0 };
    std::optional<GenericPromise::AutoRejectProducer> m_requestMediaDataProducer WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    bool m_isReadyForMoreMediaData WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { true };
};

} // namespace WebCore
