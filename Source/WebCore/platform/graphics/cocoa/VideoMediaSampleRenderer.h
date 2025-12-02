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

#include "FrameRateMonitor.h"
#include "MediaPlayerEnums.h"
#include "MediaReorderQueue.h"
#include "ProcessIdentity.h"
#include "SampleMap.h"
#include "WebAVSampleBufferListener.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferVideoRenderer;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct OpaqueCMTimebase* CMTimebaseRef;
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class EffectiveRateChangedListener;
class MediaSample;
class WebCoreDecompressionSession;

class VideoMediaSampleRenderer final
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<VideoMediaSampleRenderer, WTF::DestructionThread::Main>
    , public WebAVSampleBufferListenerClient
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper 
#endif
{
public:
    static Ref<VideoMediaSampleRenderer> create(WebSampleBufferVideoRendering *renderer, const Logger& logger, uint64_t logIdentifier) { return adoptRef(*new VideoMediaSampleRenderer(renderer, logger, logIdentifier)); }
    ~VideoMediaSampleRenderer();

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    using Preferences = VideoRendererPreferences;
    bool prefersDecompressionSession() const;
    void setPreferences(Preferences);
    bool isUsingDecompressionSession() const { return m_isUsingDecompressionSession; }

    void setTimebase(RetainPtr<CMTimebaseRef>&&);
    RetainPtr<CMTimebaseRef> timebase() const;

    bool isReadyForMoreMediaData() const;
    void requestMediaDataWhenReady(Function<void()>&&);
    void enqueueSample(const MediaSample&, const MediaTime&);
    void stopRequestingMediaData();

    void notifyFirstFrameAvailable(Function<void(const MediaTime&, double)>&&);
    void notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&&);
    void notifyWhenDecodingErrorOccurred(Function<void(NSError *)>&&);
    void notifyWhenVideoRendererRequiresFlushToResumeDecoding(Function<void()>&&);

#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    Ref<GenericPromise> changeRenderer(WebSampleBufferVideoRendering *);
#endif

    void flush();
    void shutdown();

    void expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime&);

    WebSampleBufferVideoRendering *renderer() const;

    template <typename T> T* as() const;
    template <> AVSampleBufferVideoRenderer* as() const;
    template <> AVSampleBufferDisplayLayer* as() const
    {
        assertIsMainThread();
        return m_displayLayer.get();
    }

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

    static WorkQueue& queueSingleton();

    void invalidateDecompressionSession();

private:
    VideoMediaSampleRenderer(WebSampleBufferVideoRendering *, const Logger&, uint64_t);

    void clearTimebase();
    using TimebaseAndTimerSource = std::pair<RetainPtr<CMTimebaseRef>, OSObjectPtr<dispatch_source_t>>;
    TimebaseAndTimerSource timebaseAndTimerSource() const;
    MediaTime currentTime() const;

    WebSampleBufferVideoRendering *rendererOrDisplayLayer() const;
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    AVSampleBufferVideoRenderer *videoRendererFor(WebSampleBufferVideoRendering *);
#endif

    void resetReadyForMoreMediaData();
    void initializeDecompressionSession();
    void decodeNextSampleIfNeeded();
    using FlushId = int;
    void decodedFrameAvailable(Ref<const MediaSample>&&, FlushId);
    enum class DecodedFrameResult : uint8_t {
        TooEarly,
        TooLate,
        AlreadyDisplayed,
        Displayed
    };
    DecodedFrameResult maybeQueueFrameForDisplay(const MediaTime&, const MediaSample&, FlushId);
    void flushCompressedSampleQueue();
    void flushDecodedSampleQueue();
    void cancelTimer();
    void purgeDecodedSampleQueue(FlushId);
    bool purgeDecodedSampleQueueUntilTime(const MediaTime&);
    void schedulePurgeAtTime(const MediaTime&);
    void maybeReschedulePurge(FlushId);
    void enqueueDecodedSample(Ref<const MediaSample>&&);
    size_t decodedSamplesCount() const;
    RefPtr<const MediaSample> nextDecodedSample() const;
    MediaTime nextDecodedSampleEndTime() const;
    MediaTime lastDecodedSampleTime() const;
    RetainPtr<CVPixelBufferRef> imageForSample(CMSampleBufferRef) const;

    void assignResourceOwner(const MediaSample&);
    bool areSamplesQueuesReadyForMoreMediaData(size_t waterMark) const;
    size_t compressedSamplesCount() const;
    void maybeBecomeReadyForMoreMediaData();
    bool shouldDecodeSample(const MediaSample&);

    void notifyHasAvailableVideoFrame(const MediaTime&, double, FlushId);
    void notifyErrorHasOccurred(NSError *);
    void notifyVideoRendererRequiresFlushToResumeDecoding();

    Ref<GuaranteedSerialFunctionDispatcher> dispatcher() const;
    void ensureOnDispatcher(Function<void()>&&) const;
    void ensureOnDispatcherSync(Function<void()>&&) const;
    dispatch_queue_t dispatchQueue() const;
    RefPtr<WebCoreDecompressionSession> decompressionSession() const;
    bool useDecompressionSessionForProtectedFallback() const;
    bool useDecompressionSessionForProtectedContent() const;
    bool useStereoDecoding() const;

    // WebAVSampleBufferListenerClient
    void videoRendererDidReceiveError(WebSampleBufferVideoRendering *, NSError *) final;
    void videoRendererRequiresFlushToResumeDecodingChanged(WebSampleBufferVideoRendering *, bool) final;
    void videoRendererReadyForDisplayChanged(WebSampleBufferVideoRendering *, bool) final;
    void outputObscuredDueToInsufficientExternalProtectionChanged(bool) final;

#if !RELEASE_LOG_DISABLED
    // Logger
    const Logger& logger() const final { return m_logger.get(); }
    Ref<const Logger> protectedLogger() const { return logger(); }
    ASCIILiteral logClassName() const final { return "VideoMediaSampleRenderer"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
#endif

    const bool m_rendererIsThreadSafe { false };
    RetainPtr<AVSampleBufferDisplayLayer> m_displayLayer WTF_GUARDED_BY_CAPABILITY(mainThread);
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    RetainPtr<AVSampleBufferVideoRenderer> m_renderer WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    RetainPtr<AVSampleBufferVideoRenderer> m_mainRenderer WTF_GUARDED_BY_CAPABILITY(mainThread);
#endif
    mutable Lock m_lock;
    TimebaseAndTimerSource m_timebaseAndTimerSource WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<EffectiveRateChangedListener> m_effectiveRateChangedListener;
    std::atomic<FlushId> m_flushId { 0 };
    Deque<std::tuple<Ref<const MediaSample>, MediaTime, FlushId, bool>> m_compressedSampleQueue WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    std::atomic<uint32_t> m_compressedSamplesCount { 0 };
    std::atomic<uint32_t> m_pendingSamplesCount { 0 };
    MediaSampleReorderQueue m_decodedSampleQueue WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    RefPtr<WebCoreDecompressionSession> m_decompressionSession WTF_GUARDED_BY_LOCK(m_lock);
    bool m_decompressionSessionBlocked WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_decompressionSessionWasBlocked { false };
    std::atomic<bool> m_isUsingDecompressionSession { false };
    bool m_isDecodingSample WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    bool m_isDisplayingSample WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    bool m_forceLateSampleToBeDisplayed WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    std::optional<MediaTime> m_lastDisplayedTime WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    std::optional<MediaTime> m_lastDisplayedSample WTF_GUARDED_BY_CAPABILITY(dispatcher().get());
    std::optional<MediaTime> m_nextScheduledPurge WTF_GUARDED_BY_CAPABILITY(dispatcher().get());

    bool m_notifiedFirstFrameAvailable WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    bool m_waitingForMoreMediaData WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { false };
    std::atomic<bool> m_waitingForMoreMediaDataPending { false };
    Function<void()> m_readyForMoreMediaDataFunction WTF_GUARDED_BY_CAPABILITY(mainThread);
    Preferences m_preferences;
    std::optional<uint32_t> m_currentCodec;
    std::atomic<bool> m_gotDecodingError { false };
    bool m_needsFlushing WTF_GUARDED_BY_CAPABILITY(mainThread) { false };

    MediaTime m_lastMinimumUpcomingPresentationTime WTF_GUARDED_BY_CAPABILITY(dispatcher().get()) { MediaTime::invalidTime() };

    // Playback Statistics
    std::atomic<unsigned> m_totalVideoFrames { 0 };
    std::atomic<unsigned> m_droppedVideoFrames { 0 };
    unsigned m_droppedVideoFramesOffset WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };
    std::atomic<unsigned> m_corruptedVideoFrames { 0 };
    std::atomic<unsigned> m_presentedVideoFrames { 0 };
    mutable unsigned m_sampleCount WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };
    MediaTime m_totalFrameDelay { MediaTime::zeroTime() };

    // Protected samples
    bool m_wasProtected { false };

    Function<void(const MediaTime&, double)> m_hasFirstFrameAvailableCallback WTF_GUARDED_BY_CAPABILITY(mainThread);
    Function<void(const MediaTime&, double)> m_hasAvailableFrameCallback WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::atomic<bool> m_notifyWhenHasAvailableVideoFrame { false };
    Function<void(NSError *)> m_errorOccurredFunction WTF_GUARDED_BY_CAPABILITY(mainThread);
    Function<void()> m_rendererNeedsFlushFunction WTF_GUARDED_BY_CAPABILITY(mainThread);
    ProcessIdentity m_resourceOwner;
    const Ref<WebAVSampleBufferListener> m_listener;
    MonotonicTime m_startupTime;
    MonotonicTime m_timeSinceLastDecode;
    FrameRateMonitor m_frameRateMonitor;

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore
