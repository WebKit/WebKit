/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <CoreMedia/CMTime.h>
#include <functional>
#include <wtf/Lock.h>
#include <wtf/MediaTime.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

typedef CFTypeRef CMBufferRef;
typedef const struct __CFArray * CFArrayRef;
typedef struct opaqueCMBufferQueue *CMBufferQueueRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct OpaqueCMTimebase* CMTimebaseRef;
typedef signed long CMItemCount;
typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct __CVBuffer *CVImageBufferRef;
typedef UInt32 VTDecodeInfoFlags;
typedef struct OpaqueVTDecompressionSession*  VTDecompressionSessionRef;

namespace WebCore {

class WEBCORE_EXPORT WebCoreDecompressionSession : public ThreadSafeRefCounted<WebCoreDecompressionSession> {
public:
    static Ref<WebCoreDecompressionSession> createOpenGL() { return adoptRef(*new WebCoreDecompressionSession(OpenGL)); }
    static Ref<WebCoreDecompressionSession> createRGB() { return adoptRef(*new WebCoreDecompressionSession(RGB)); }

    void invalidate();
    bool isInvalidated() const { return m_invalidated; }

    void enqueueSample(CMSampleBufferRef, bool displaying = true);
    bool isReadyForMoreMediaData() const;
    void requestMediaDataWhenReady(std::function<void()>);
    void stopRequestingMediaData();
    void notifyWhenHasAvailableVideoFrame(std::function<void()>);

    RetainPtr<CVPixelBufferRef> decodeSampleSync(CMSampleBufferRef);

    void setTimebase(CMTimebaseRef);
    CMTimebaseRef timebase() const { return m_timebase.get(); }

    enum ImageForTimeFlags { ExactTime, AllowEarlier, AllowLater };
    RetainPtr<CVPixelBufferRef> imageForTime(const MediaTime&, ImageForTimeFlags = ExactTime);
    void flush();

    unsigned totalVideoFrames() const { return m_totalVideoFrames; }
    unsigned droppedVideoFrames() const { return m_droppedVideoFrames; }
    unsigned corruptedVideoFrames() const { return m_corruptedVideoFrames; }
    MediaTime totalFrameDelay() const { return m_totalFrameDelay; }

    bool hardwareDecoderEnabled() const { return m_hardwareDecoderEnabled; }
    void setHardwareDecoderEnabled(bool enabled) { m_hardwareDecoderEnabled = enabled; }

private:
    enum Mode {
        OpenGL,
        RGB,
    };
    WebCoreDecompressionSession(Mode);

    void ensureDecompressionSessionForSample(CMSampleBufferRef);

    void decodeSample(CMSampleBufferRef, bool displaying);
    void enqueueDecodedSample(CMSampleBufferRef, bool displaying);
    void handleDecompressionOutput(bool displaying, OSStatus, VTDecodeInfoFlags, CVImageBufferRef, CMTime presentationTimeStamp, CMTime presentationDuration);
    RetainPtr<CVPixelBufferRef> getFirstVideoFrame();
    void resetAutomaticDequeueTimer();
    void automaticDequeue();
    bool shouldDecodeSample(CMSampleBufferRef, bool displaying);

    static CMTime getDecodeTime(CMBufferRef, void* refcon);
    static CMTime getPresentationTime(CMBufferRef, void* refcon);
    static CMTime getDuration(CMBufferRef, void* refcon);
    static CFComparisonResult compareBuffers(CMBufferRef buf1, CMBufferRef buf2, void* refcon);
    void maybeBecomeReadyForMoreMediaData();

    void resetQosTier();
    void increaseQosTier();
    void decreaseQosTier();
    void updateQosWithDecodeTimeStatistics(double ratio);

    static const CMItemCount kMaximumCapacity = 120;
    static const CMItemCount kHighWaterMark = 60;
    static const CMItemCount kLowWaterMark = 15;

    Mode m_mode;
    RetainPtr<VTDecompressionSessionRef> m_decompressionSession;
    RetainPtr<CMBufferQueueRef> m_producerQueue;
    RetainPtr<CMBufferQueueRef> m_consumerQueue;
    RetainPtr<CMTimebaseRef> m_timebase;
    Ref<WorkQueue> m_decompressionQueue;
    Ref<WorkQueue> m_enqueingQueue;
    OSObjectPtr<dispatch_source_t> m_timerSource;
    std::function<void()> m_notificationCallback;
    std::function<void()> m_hasAvailableFrameCallback;
    RetainPtr<CFArrayRef> m_qosTiers;
    int m_currentQosTier { 0 };
    unsigned m_framesSinceLastQosCheck { 0 };
    double m_decodeRatioMovingAverage { 0 };

    bool m_invalidated { false };
    bool m_hardwareDecoderEnabled { true };
    int m_framesBeingDecoded { 0 };
    unsigned m_totalVideoFrames { 0 };
    unsigned m_droppedVideoFrames { 0 };
    unsigned m_corruptedVideoFrames { 0 };
    MediaTime m_totalFrameDelay;
};

}
