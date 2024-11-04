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
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferVideoRenderer;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);
typedef struct opaqueCMBufferQueue *CMBufferQueueRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct OpaqueCMTimebase* CMTimebaseRef;

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

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
    RetainPtr<CMTimebaseRef> timebase() const { return m_timebase; }

    bool isReadyForMoreMediaData() const;
    void requestMediaDataWhenReady(Function<void()>&&);
    void enqueueSample(const MediaSample&);
    void stopRequestingMediaData();

    void flush();

    void expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime&);
    void resetUpcomingSampleBufferPresentationTimeExpectations();

    WebSampleBufferVideoRendering *renderer() const;
    AVSampleBufferDisplayLayer *displayLayer() const;
    RetainPtr<CVPixelBufferRef> copyDisplayedPixelBuffer();
    CGRect bounds() const;

    unsigned totalVideoFrames() const;
    unsigned droppedVideoFrames() const;
    unsigned corruptedVideoFrames() const;
    MediaTime totalFrameDelay() const;

    void setResourceOwner(const ProcessIdentity&);

private:
    VideoMediaSampleRenderer(WebSampleBufferVideoRendering *);

    void setPrefersDecompressionSessionInternal(bool);
    void setTimebaseInternal(RetainPtr<CMTimebaseRef>&&);

    void resetReadyForMoreSample();
    void initializeDecompressionSession();
    void decodeNextSample();
    void decodedFrameAvailable(RetainPtr<CMSampleBufferRef>&&);
    void flushCompressedSampleQueue();
    void flushDecodedSampleQueue();
    void purgeDecodedSampleQueue();
    CMBufferQueueRef ensureCompressedSampleQueue();
    CMBufferQueueRef ensureDecodedSampleQueue();
    void assignResourceOwner(CMSampleBufferRef);
    void maybeBecomeReadyForMoreMediaData();

    Ref<WTF::WorkQueue> m_workQueue;
    RetainPtr<AVSampleBufferDisplayLayer> m_displayLayer;
    RetainPtr<AVSampleBufferVideoRenderer> m_renderer;
    RetainPtr<CMTimebaseRef> m_timebase;
    RetainPtr<CMBufferQueueRef> m_compressedSampleQueue;
    RetainPtr<CMBufferQueueRef> m_decodedSampleQueue;
    OSObjectPtr<dispatch_source_t> m_timerSource;
    RefPtr<WebCoreDecompressionSession> m_decompressionSession;
    bool m_isDecodingSample { false };
    Function<void()> m_readyForMoreSampleFunction;
    bool m_prefersDecompressionSession { false };
    std::optional<uint32_t> m_currentCodec;

    ProcessIdentity m_resourceOwner;
};

} // namespace WebCore
