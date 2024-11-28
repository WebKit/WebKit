/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "ProcessIdentity.h"
#include "SampleMap.h"
#include <CoreMedia/CMTime.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferVideoRenderer;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);
typedef struct opaqueCMBufferQueue *CMBufferQueueRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct OpaqueCMTimebase* CMTimebaseRef;
typedef struct __CVBuffer* CVPixelBufferRef;

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class MediaSample;
class WebCoreDecompressionSession;

class VideoMediaSampleRenderer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<VideoMediaSampleRenderer, WTF::DestructionThread::Main> {
public:
    static Ref<VideoMediaSampleRenderer> create(WebSampleBufferVideoRendering *renderer) { return adoptRef(*new VideoMediaSampleRenderer(renderer)); }
    ~VideoMediaSampleRenderer();

    bool prefersDecompressionSession() { return m_prefersDecompressionSession; }
    void setPrefersDecompressionSession(bool);

    void setTimebase(RetainPtr<CMTimebaseRef>&&);
    RetainPtr<CMTimebaseRef> timebase() const;

    bool isReadyForMoreMediaData() const;
    void requestMediaDataWhenReady(Function<void()>&&);
    void enqueueSample(const MediaSample&);
    void stopRequestingMediaData();

    void notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&&);
    void notifyWhenDecodingErrorOccurred(Function<void(OSStatus)>&&);

    void flush();

    void expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime&);
    void resetUpcomingSampleBufferPresentationTimeExpectations();

    WebSampleBufferVideoRendering *renderer() const;
    AVSampleBufferDisplayLayer *displayLayer() const;

    struct DisplayedPixelBufferEntry {
        RetainPtr<CVPixelBufferRef> pixelBuffer;
        MediaTime presentationTimeStamp;
    };
    DisplayedPixelBufferEntry copyDisplayedPixelBuffer();

    unsigned totalDisplayedFrames() const;
    unsigned totalVideoFrames() const;
    unsigned droppedVideoFrames() const;
    unsigned corruptedVideoFrames() const;
    MediaTime totalFrameDelay() const;

    void setResourceOwner(const ProcessIdentity&);

private:
    VideoMediaSampleRenderer(WebSampleBufferVideoRendering *);

    void clearTimebase();
    using TimebaseAndTimerSource = std::pair<RetainPtr<CMTimebaseRef>, OSObjectPtr<dispatch_source_t>>;
    TimebaseAndTimerSource timebaseAndTimerSource() const;

    WebSampleBufferVideoRendering *rendererOrDisplayLayer() const;

    void resetReadyForMoreSample();
    void initializeDecompressionSession();
    void decodeNextSample();
    using FlushId = int;
    void decodedFrameAvailable(RetainPtr<CMSampleBufferRef>&&, FlushId);
    enum class DecodedFrameResult : uint8_t {
        TooEarly,
        TooLate,
        AlreadyDisplayed,
        Displayed
    };
    DecodedFrameResult maybeQueueFrameForDisplay(const CMTime&, CMSampleBufferRef, FlushId);
    void flushCompressedSampleQueue();
    void flushDecodedSampleQueue();
    void cancelTimer();
    void purgeDecodedSampleQueueAndDisplay(FlushId);
    bool purgeDecodedSampleQueue(const CMTime&);
    void schedulePurgeAndDisplayAtTime(const CMTime&);
    void maybeReschedulePurgeAndDisplay(FlushId);
    void enqueueDecodedSample(RetainPtr<CMSampleBufferRef>&&);
    size_t decodedSamplesCount() const;
    RetainPtr<CMSampleBufferRef> nextDecodedSample() const;
    CMTime nextDecodedSampleEndTime() const;

    void assignResourceOwner(CMSampleBufferRef);
    bool areSamplesQueuesReadyForMoreMediaData(size_t waterMark) const;
    void maybeBecomeReadyForMoreMediaData();
    bool shouldDecodeSample(CMSampleBufferRef, bool displaying);

    void notifyHasAvailableVideoFrame(const MediaTime&, double, FlushId);
    void notifyErrorHasOccurred(OSStatus);

    Ref<GuaranteedSerialFunctionDispatcher> dispatcher() const;
    void ensureOnDispatcher(Function<void()>&&) const;
    void ensureOnDispatcherSync(Function<void()>&&) const;
    dispatch_queue_t dispatchQueue() const;

    const RefPtr<WTF::WorkQueue> m_workQueue;
    RetainPtr<AVSampleBufferDisplayLayer> m_displayLayer;
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    RetainPtr<AVSampleBufferVideoRenderer> m_renderer;
#endif
    mutable Lock m_lock;
    TimebaseAndTimerSource m_timebaseAndTimerSource WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<ssize_t> m_framesBeingDecoded { 0 };
    std::atomic<FlushId> m_flushId { 0 };
    Deque<std::pair<RetainPtr<CMSampleBufferRef>, FlushId>> m_compressedSampleQueue WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    RetainPtr<CMBufferQueueRef> m_decodedSampleQueue; // created on the main thread, immutable after creation.
    RefPtr<WebCoreDecompressionSession> m_decompressionSession;
    bool m_isDecodingSample WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    bool m_isDisplayingSample WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    bool m_forceLateSampleToBeDisplayed WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    std::optional<CMTime> m_lastDisplayedTime WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    std::optional<CMTime> m_lastDisplayedSample WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    std::optional<CMTime> m_nextScheduledPurge WTF_GUARDED_BY_CAPABILITY(dispatcher().get());

    Function<void()> m_readyForMoreSampleFunction;
    bool m_prefersDecompressionSession { false };
    std::optional<uint32_t> m_currentCodec;
    std::atomic<bool> m_gotDecodingError { false };

    // Playback Statistics
    std::atomic<unsigned> m_totalVideoFrames { 0 };
    std::atomic<unsigned> m_droppedVideoFrames { 0 };
    std::atomic<unsigned> m_corruptedVideoFrames { 0 };
    std::atomic<unsigned> m_presentedVideoFrames { 0 };
    MediaTime m_totalFrameDelay { MediaTime::zeroTime() };

    Function<void(const MediaTime&, double)> m_hasAvailableFrameCallback WTF_GUARDED_BY_CAPABILITY(mainThread);
    Function<void(OSStatus)> m_errorOccurredFunction WTF_GUARDED_BY_CAPABILITY(mainThread);
    ProcessIdentity m_resourceOwner;
    MonotonicTime m_startupTime;
};

} // namespace WebCore
