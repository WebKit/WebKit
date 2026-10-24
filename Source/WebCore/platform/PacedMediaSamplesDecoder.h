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
// time: VideoDecodeWindow's for video, the audio watermarks in PacedMediaSamplesDecoder.cpp for audio.
// isReadyForMoreMediaData() follows VideoDecodeWindow's sample queue watermarks applied to the blocks waiting to be
// decoded; blocks given while it is false are queued until they are needed.
// decode() and drain() promises stay pending until their input has been decoded; while paused, that waits for
// currentTimeDidChange().
// An error rejects all pending promises and every later decode() and drain().
class PacedMediaSamplesDecoder final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PacedMediaSamplesDecoder> {
    WTF_MAKE_TZONE_ALLOCATED(PacedMediaSamplesDecoder);
public:
    using Filters = MediaSamplesDecoder::Filters;
    using CreatePromise = NativePromise<Ref<PacedMediaSamplesDecoder>, PlatformMediaError>;
    using DecodePromise = MediaSamplesDecoder::DecodePromise;
    struct PlaybackPosition {
        MediaTime time;
        double rate { 0 }; // 0 when paused.
    };
    // Called on the dispatcher given to create().
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
        MediaSamplesBlock block { };
        std::optional<MediaTime> minimumUpcomingTime { };
        Vector<MediaSamplesBlock> output { };
        DecodePromise::AutoRejectProducer producer { PlatformMediaError::Cancelled };
    };

    PacedMediaSamplesDecoder(Ref<GuaranteedSerialFunctionDispatcher>&&, Ref<MediaSamplesDecoder>&&, bool isVideo, CurrentTimeProvider&&);

    Ref<DecodePromise> enqueue(Entry&&);
    void pump();
    // How long to wait before decoding the next block, nullopt to decode it now.
    std::optional<Seconds> audioWaitTime();
    std::optional<Seconds> videoWaitTime(const Entry&);
    void submit(Ref<DecodePromise>&&);
    void didSettle(uint32_t flushId, DecodePromise::Result&&);
    void recordOutput(const Vector<MediaSamplesBlock>&);
    void fail(PlatformMediaError);
    void clearPending();
    void scheduleWake(Seconds waitHint);
    void wake(uint32_t wakeId);
    void updateReadiness();

    const Ref<GuaranteedSerialFunctionDispatcher> m_dispatcher;
    const Ref<MediaSamplesDecoder> m_decoder;
    const CurrentTimeProvider m_currentTimeProvider;
    const bool m_isVideo;
    Deque<Entry> m_pending WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    bool m_isDecoding WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { false };
    uint32_t m_flushId WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { 0 };
    std::optional<PlatformMediaError> m_error WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    Deque<MediaTime> m_decodedPresentationTimes WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    MediaTime m_decodedAudioEndTime WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { MediaTime::invalidTime() };
    // Blocks given to decode() that haven't been handed to the decoder yet.
    size_t m_queuedBlockCount WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { 0 };
    bool m_idledAtHighWaterMark WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { false };
    std::optional<MonotonicTime> m_wakeTime WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    uint32_t m_wakeId WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { 0 };
    std::optional<GenericPromise::AutoRejectProducer> m_requestMediaDataProducer WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get());
    bool m_isReadyForMoreMediaData WTF_GUARDED_BY_CAPABILITY(m_dispatcher.get()) { true };
};

} // namespace WebCore
