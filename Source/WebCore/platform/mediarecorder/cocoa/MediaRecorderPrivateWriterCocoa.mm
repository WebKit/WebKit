/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MediaRecorderPrivateWriterCocoa.h"

#if ENABLE(MEDIA_RECORDER)

#include "AudioSampleBufferCompressor.h"
#include "AudioStreamDescription.h"
#include "Logging.h"
#include "MediaRecorderPrivate.h"
#include "MediaRecorderPrivateOptions.h"
#include "MediaStreamTrackPrivate.h"
#include "MediaUtilities.h"
#include "VideoFrame.h"
#include "VideoSampleBufferCompressor.h"
#include "WebAudioBufferList.h"
#include <AVFoundation/AVAssetWriter.h>
#include <AVFoundation/AVAssetWriterInput.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cocoa/AVAssetWriterSPI.h>
#include <wtf/BlockPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/cf/TypeCastsCF.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/cocoa/AVFoundationSoftLink.h>

@interface WebAVAssetWriterDelegate : NSObject <AVAssetWriterDelegate> {
    WebCore::MediaRecorderPrivateWriter* m_writer;
}

- (instancetype)initWithWriter:(WebCore::MediaRecorderPrivateWriter&)writer;
- (void)close;

@end

@implementation WebAVAssetWriterDelegate {
};

- (instancetype)initWithWriter:(WebCore::MediaRecorderPrivateWriter&)writer
{
    ASSERT(isMainThread());
    self = [super init];
    if (self)
        self->m_writer = &writer;

    return self;
}

- (void)assetWriter:(AVAssetWriter *)assetWriter didProduceFragmentedHeaderData:(NSData *)fragmentedHeaderData
{
    UNUSED_PARAM(assetWriter);
    m_writer->appendData(static_cast<const uint8_t*>([fragmentedHeaderData bytes]), [fragmentedHeaderData length]);
}

- (void)assetWriter:(AVAssetWriter *)assetWriter didProduceFragmentedMediaData:(NSData *)fragmentedMediaData fragmentedMediaDataReport:(AVFragmentedMediaDataReport *)fragmentedMediaDataReport
{
    UNUSED_PARAM(assetWriter);
    UNUSED_PARAM(fragmentedMediaDataReport);
    m_writer->appendData(static_cast<const uint8_t*>([fragmentedMediaData bytes]), [fragmentedMediaData length]);
}

- (void)close
{
    m_writer = nullptr;
}

@end

namespace WebCore {

RefPtr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriter::create(bool hasAudio, bool hasVideo, const MediaRecorderPrivateOptions& options)
{
    auto writer = adoptRef(*new MediaRecorderPrivateWriter(hasAudio, hasVideo));
    if (!writer->initialize(options))
        return nullptr;
    return writer;
}

void MediaRecorderPrivateWriter::compressedVideoOutputBufferCallback(void *mediaRecorderPrivateWriter, CMBufferQueueTriggerToken)
{
    callOnMainThread([weakWriter = ThreadSafeWeakPtr<MediaRecorderPrivateWriter> { static_cast<MediaRecorderPrivateWriter*>(mediaRecorderPrivateWriter) }] {
        if (auto strongWriter = weakWriter.get())
            strongWriter->processNewCompressedVideoSampleBuffers();
    });
}

void MediaRecorderPrivateWriter::compressedAudioOutputBufferCallback(void *mediaRecorderPrivateWriter, CMBufferQueueTriggerToken)
{
    callOnMainThread([weakWriter = ThreadSafeWeakPtr<MediaRecorderPrivateWriter> { static_cast<MediaRecorderPrivateWriter*>(mediaRecorderPrivateWriter) }] {
        if (auto strongWriter = weakWriter.get())
            strongWriter->processNewCompressedAudioSampleBuffers();
    });
}

MediaRecorderPrivateWriter::MediaRecorderPrivateWriter(bool hasAudio, bool hasVideo)
    : m_hasAudio(hasAudio)
    , m_hasVideo(hasVideo)
    , m_lastVideoPresentationTime(PAL::kCMTimeInvalid)
    , m_lastVideoDecodingTime(PAL::kCMTimeInvalid)
    , m_resumedVideoTime(PAL::kCMTimeZero)
    , m_currentVideoDuration(PAL::kCMTimeZero)
    , m_currentAudioSampleTime(PAL::kCMTimeZero)
{
}

MediaRecorderPrivateWriter::~MediaRecorderPrivateWriter()
{
    m_pendingAudioSampleQueue.clear();
    m_pendingVideoFrameQueue.clear();
    if (m_writer) {
        [m_writer cancelWriting];
        m_writer.clear();
    }

    if (m_writerDelegate)
        [m_writerDelegate close];

    if (auto completionHandler = WTFMove(m_fetchDataCompletionHandler))
        completionHandler(nullptr, 0);
}

bool MediaRecorderPrivateWriter::initialize(const MediaRecorderPrivateOptions& options)
{
    NSError *error = nil;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    m_writer = adoptNS([PAL::allocAVAssetWriterInstance() initWithFileType:AVFileTypeMPEG4 error:&error]);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "create AVAssetWriter instance failed with error code %ld", (long)error.code);
        return false;
    }

    m_writerDelegate = adoptNS([[WebAVAssetWriterDelegate alloc] initWithWriter: *this]);
    [m_writer setDelegate:m_writerDelegate.get()];

    if (m_hasAudio) {
        m_audioCompressor = AudioSampleBufferCompressor::create(compressedAudioOutputBufferCallback, this);
        if (!m_audioCompressor)
            return false;
        if (options.audioBitsPerSecond)
            m_audioCompressor->setBitsPerSecond(*options.audioBitsPerSecond);
    }
    if (m_hasVideo) {
        m_videoCompressor = VideoSampleBufferCompressor::create(options.mimeType, compressedVideoOutputBufferCallback, this);
        if (!m_videoCompressor)
            return false;
        if (options.videoBitsPerSecond)
            m_videoCompressor->setBitsPerSecond(*options.videoBitsPerSecond);
    }

    return true;
}

void MediaRecorderPrivateWriter::processNewCompressedVideoSampleBuffers()
{
    ASSERT(m_hasVideo);
    if (!m_videoFormatDescription) {
        m_videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(m_videoCompressor->getOutputSampleBuffer());

        if (m_hasAudio && !m_audioFormatDescription)
            return;

        startAssetWriter();
    }

    if (!m_hasStartedWriting)
        return;
    appendCompressedSampleBuffers();
}

void MediaRecorderPrivateWriter::processNewCompressedAudioSampleBuffers()
{
    ASSERT(m_hasAudio);
    if (!m_audioFormatDescription) {
        m_audioFormatDescription = PAL::CMSampleBufferGetFormatDescription(m_audioCompressor->getOutputSampleBuffer());
        if (m_hasVideo && !m_videoFormatDescription)
            return;

        startAssetWriter();
    }

    if (!m_hasStartedWriting)
        return;
    appendCompressedSampleBuffers();
}

void MediaRecorderPrivateWriter::startAssetWriter()
{
    if (m_hasVideo) {
        m_videoAssetWriterInput = adoptNS([PAL::allocAVAssetWriterInputInstance() initWithMediaType:AVMediaTypeVideo outputSettings:nil sourceFormatHint:m_videoFormatDescription.get()]);
        [m_videoAssetWriterInput setExpectsMediaDataInRealTime:true];
        if (m_videoTransform)
            m_videoAssetWriterInput.get().transform = *m_videoTransform;

        if (![m_writer canAddInput:m_videoAssetWriterInput.get()]) {
            RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter::startAssetWriter failed canAddInput for video");
            return;
        }
        [m_writer addInput:m_videoAssetWriterInput.get()];
    }

    if (m_hasAudio) {
        m_audioAssetWriterInput = adoptNS([PAL::allocAVAssetWriterInputInstance() initWithMediaType:AVMediaTypeAudio outputSettings:nil sourceFormatHint:m_audioFormatDescription.get()]);
        [m_audioAssetWriterInput setExpectsMediaDataInRealTime:true];
        if (![m_writer canAddInput:m_audioAssetWriterInput.get()]) {
            RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter::startAssetWriter failed canAddInput for audio");
            return;
        }
        [m_writer addInput:m_audioAssetWriterInput.get()];
    }

    if (![m_writer startWriting]) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter::startAssetWriter failed startWriting");
        return;
    }

    [m_writer startSessionAtSourceTime:PAL::kCMTimeZero];

    appendCompressedSampleBuffers();

    m_hasStartedWriting = true;
}

bool MediaRecorderPrivateWriter::appendCompressedAudioSampleBufferIfPossible()
{
    if (!m_audioCompressor)
        return false;

    auto buffer = m_audioCompressor->takeOutputSampleBuffer();
    if (!buffer)
        return false;

    if (m_isFlushingSamples) {
        m_pendingAudioSampleQueue.append(WTFMove(buffer));
        return true;
    }

    while (!m_pendingAudioSampleQueue.isEmpty() && [m_audioAssetWriterInput isReadyForMoreMediaData])
        [m_audioAssetWriterInput appendSampleBuffer:m_pendingAudioSampleQueue.takeFirst().get()];

    if (![m_audioAssetWriterInput isReadyForMoreMediaData]) {
        m_pendingAudioSampleQueue.append(WTFMove(buffer));
        return true;
    }

    [m_audioAssetWriterInput appendSampleBuffer:buffer.get()];
    return true;
}

bool MediaRecorderPrivateWriter::appendCompressedVideoSampleBufferIfPossible()
{
    if (!m_videoCompressor)
        return false;

    auto buffer = m_videoCompressor->takeOutputSampleBuffer();
    if (!buffer)
        return false;

    if (m_isFlushingSamples) {
        m_pendingVideoFrameQueue.append(WTFMove(buffer));
        return true;
    }

    while (!m_pendingVideoFrameQueue.isEmpty() && [m_videoAssetWriterInput isReadyForMoreMediaData])
        appendCompressedVideoSampleBuffer(m_pendingVideoFrameQueue.takeFirst().get());

    if (![m_videoAssetWriterInput isReadyForMoreMediaData]) {
        m_pendingVideoFrameQueue.append(WTFMove(buffer));
        return true;
    }

    appendCompressedVideoSampleBuffer(buffer.get());
    return true;
}

void MediaRecorderPrivateWriter::appendCompressedVideoSampleBuffer(CMSampleBufferRef buffer)
{
    ASSERT([m_videoAssetWriterInput isReadyForMoreMediaData]);
    m_lastVideoPresentationTime = PAL::CMSampleBufferGetPresentationTimeStamp(buffer);
    m_lastVideoDecodingTime = PAL::CMSampleBufferGetDecodeTimeStamp(buffer);
    m_hasEncodedVideoSamples = true;

    [m_videoAssetWriterInput appendSampleBuffer:buffer];
}

void MediaRecorderPrivateWriter::appendCompressedSampleBuffers()
{
    while (appendCompressedVideoSampleBufferIfPossible() || appendCompressedAudioSampleBufferIfPossible()) { };
}

static inline void appendEndsPreviousSampleDurationMarker(AVAssetWriterInput *assetWriterInput, CMTime presentationTimeStamp, CMTime decodingTimeStamp)
{
    CMSampleTimingInfo timingInfo = { PAL::kCMTimeInvalid, presentationTimeStamp, decodingTimeStamp};

    CMSampleBufferRef buffer = NULL;
    auto error = PAL::CMSampleBufferCreate(kCFAllocatorDefault, NULL, true, NULL, NULL, NULL, 0, 1, &timingInfo, 0, NULL, &buffer);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter appendEndsPreviousSampleDurationMarker failed CMSampleBufferCreate with %d", error);
        return;
    }
    auto sampleBuffer = adoptCF(buffer);

    PAL::CMSetAttachment(sampleBuffer.get(), PAL::kCMSampleBufferAttachmentKey_EndsPreviousSampleDuration, kCFBooleanTrue, kCMAttachmentMode_ShouldPropagate);
    if (![assetWriterInput appendSampleBuffer:sampleBuffer.get()])
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter appendSampleBuffer to writer input failed");
}

void MediaRecorderPrivateWriter::flushCompressedSampleBuffers(Function<void()>&& callback)
{
    bool hasPendingAudioSamples = !m_pendingAudioSampleQueue.isEmpty();
    bool hasPendingVideoSamples = !m_pendingVideoFrameQueue.isEmpty();

    if (m_hasEncodedVideoSamples) {
        hasPendingVideoSamples |= ![m_videoAssetWriterInput isReadyForMoreMediaData];
        if (!hasPendingVideoSamples)
            appendEndsPreviousSampleDurationMarker(m_videoAssetWriterInput.get(), m_lastVideoPresentationTime, m_lastVideoDecodingTime);
    }

    if (!hasPendingAudioSamples && !hasPendingVideoSamples) {
        callback();
        return;
    }

    ASSERT(!m_isFlushingSamples);
    m_isFlushingSamples = true;
    auto block = makeBlockPtr([this, weakThis = ThreadSafeWeakPtr { *this }, hasPendingAudioSamples, hasPendingVideoSamples, audioSampleQueue = WTFMove(m_pendingAudioSampleQueue), videoSampleQueue = WTFMove(m_pendingVideoFrameQueue), callback = WTFMove(callback)]() mutable {
        auto strongThis = weakThis.get();
        if (!strongThis) {
            callback();
            return;
        }

        while (!audioSampleQueue.isEmpty() && [m_audioAssetWriterInput isReadyForMoreMediaData])
            [m_audioAssetWriterInput appendSampleBuffer:audioSampleQueue.takeFirst().get()];

        while (!videoSampleQueue.isEmpty() && [m_videoAssetWriterInput isReadyForMoreMediaData])
            appendCompressedVideoSampleBuffer(videoSampleQueue.takeFirst().get());

        if (!audioSampleQueue.isEmpty() || !videoSampleQueue.isEmpty() || (hasPendingVideoSamples && ![m_videoAssetWriterInput isReadyForMoreMediaData]))
            return;

        if (hasPendingAudioSamples)
            [m_audioAssetWriterInput markAsFinished];
        if (hasPendingVideoSamples) {
            appendEndsPreviousSampleDurationMarker(m_videoAssetWriterInput.get(), m_lastVideoPresentationTime, m_lastVideoDecodingTime);
            [m_videoAssetWriterInput markAsFinished];
        }
        m_isFlushingSamples = false;
        callback();
        finishedFlushingSamples();
    });

    if (hasPendingAudioSamples)
        [m_audioAssetWriterInput requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:block.get()];
    if (hasPendingVideoSamples)
        [m_videoAssetWriterInput requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:block.get()];
}

void MediaRecorderPrivateWriter::appendVideoFrame(VideoFrame& frame)
{
    if (!m_firstVideoFrame) {
        m_firstVideoFrame = true;
        m_resumedVideoTime = PAL::CMClockGetTime(PAL::CMClockGetHostTimeClock());
        if (frame.rotation() != VideoFrame::Rotation::None || frame.isMirrored()) {
            m_videoTransform = CGAffineTransformMakeRotation(static_cast<int>(frame.rotation()) * M_PI / 180);
            if (frame.isMirrored())
                m_videoTransform = CGAffineTransformScale(*m_videoTransform, -1, 1);
        }
    }

    auto frameTime = PAL::CMTimeSubtract(PAL::CMClockGetTime(PAL::CMClockGetHostTimeClock()), m_resumedVideoTime);
    frameTime = PAL::CMTimeAdd(frameTime, m_currentVideoDuration);
    if (auto bufferWithCurrentTime = createVideoSampleBuffer(frame.pixelBuffer(), frameTime))
        m_videoCompressor->addSampleBuffer(bufferWithCurrentTime.get());
}

void MediaRecorderPrivateWriter::appendAudioSampleBuffer(const PlatformAudioData& data, const AudioStreamDescription& description, const WTF::MediaTime&, size_t sampleCount)
{
    if (auto sampleBuffer = createAudioSampleBuffer(data, description, m_currentAudioSampleTime, sampleCount))
        m_audioCompressor->addSampleBuffer(sampleBuffer.get());
    m_currentAudioSampleTime = PAL::CMTimeAdd(m_currentAudioSampleTime, PAL::toCMTime(MediaTime(sampleCount, description.sampleRate())));
}

void MediaRecorderPrivateWriter::finishedFlushingSamples()
{
    if (m_shouldStopAfterFlushingSamples)
        stopRecording();
}

void MediaRecorderPrivateWriter::stopRecording()
{
    if (m_isFlushingSamples) {
        m_shouldStopAfterFlushingSamples = true;
        return;
    }

    if (m_isStopped)
        return;

    m_isStopped = true;

    if (m_videoCompressor)
        m_videoCompressor->finish();
    if (m_audioCompressor)
        m_audioCompressor->finish();

    m_isStopping = true;
    // We hop to the main thread since finishing the video compressor might trigger starting the writer asynchronously.
    callOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        auto strongThis = weakThis.get();
        if (!strongThis)
            return;

        auto whenFinished = [this, weakThis] {
            auto strongThis = weakThis.get();
            if (!strongThis)
                return;

            m_isStopping = false;
            m_isStopped = false;
            m_hasStartedWriting = false;

            if (m_writer) {
                [m_writer cancelWriting];
                m_writer.clear();
            }

            if (m_fetchDataCompletionHandler)
                m_fetchDataCompletionHandler(takeData(), 0);
        };

        if (!m_hasStartedWriting) {
            whenFinished();
            return;
        }

        ASSERT([m_writer status] == AVAssetWriterStatusWriting);
        flushCompressedSampleBuffers([this, weakThis = WTFMove(weakThis), whenFinished = WTFMove(whenFinished)]() mutable {
            auto strongThis = weakThis.get();
            if (!strongThis)
                return;

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [m_writer flush];
            ALLOW_DEPRECATED_DECLARATIONS_END

            [m_writer finishWritingWithCompletionHandler:[whenFinished = WTFMove(whenFinished)]() mutable {
                callOnMainThread(WTFMove(whenFinished));
            }];
        });
    });
}

void MediaRecorderPrivateWriter::fetchData(CompletionHandler<void(RefPtr<FragmentedSharedBuffer>&&, double)>&& completionHandler)
{
    m_fetchDataCompletionHandler = WTFMove(completionHandler);

    if (m_isStopping)
        return;

    if (!m_hasStartedWriting) {
        completeFetchData();
        return;
    }

    if (m_videoCompressor)
        m_videoCompressor->flush();
    if (m_audioCompressor)
        m_audioCompressor->flush();

    // We hop to the main thread since flushing the video compressor might trigger starting the writer asynchronously.
    callOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        flushCompressedSampleBuffers([weakThis = WTFMove(weakThis)]() mutable {
            auto strongThis = weakThis.get();
            if (!strongThis)
                return;

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [strongThis->m_writer flush];
            ALLOW_DEPRECATED_DECLARATIONS_END

            callOnMainThread([weakThis = WTFMove(weakThis)] {
                if (auto strongThis = weakThis.get())
                    strongThis->completeFetchData();
            });
        });
    });
}

void MediaRecorderPrivateWriter::completeFetchData()
{
    auto currentTimeCode = m_timeCode;
    if (m_hasAudio)
        m_timeCode = PAL::CMTimeGetSeconds(m_currentAudioSampleTime);
    else {
        auto sampleTime = PAL::CMTimeSubtract(PAL::CMClockGetTime(PAL::CMClockGetHostTimeClock()), m_resumedVideoTime);
        m_timeCode = PAL::CMTimeGetSeconds(PAL::CMTimeAdd(sampleTime, m_currentVideoDuration));
    }
    m_fetchDataCompletionHandler(takeData(), currentTimeCode);
}

void MediaRecorderPrivateWriter::appendData(const uint8_t* data, size_t size)
{
    Locker locker { m_dataLock };
    m_data.append(data, size);
}

RefPtr<FragmentedSharedBuffer> MediaRecorderPrivateWriter::takeData()
{
    Locker locker { m_dataLock };
    return m_data.take();
}

void MediaRecorderPrivateWriter::pause()
{
    auto recordingDuration = PAL::CMTimeSubtract(PAL::CMClockGetTime(PAL::CMClockGetHostTimeClock()), m_resumedVideoTime);
    m_currentVideoDuration = PAL::CMTimeAdd(recordingDuration, m_currentVideoDuration);
}

void MediaRecorderPrivateWriter::resume()
{
    m_resumedVideoTime = PAL::CMClockGetTime(PAL::CMClockGetHostTimeClock());
}

const String& MediaRecorderPrivateWriter::mimeType() const
{
    static NeverDestroyed<const String> audioMP4(MAKE_STATIC_STRING_IMPL("audio/mp4"));
    static NeverDestroyed<const String> videoMP4(MAKE_STATIC_STRING_IMPL("video/mp4"));
    // FIXME: we will need to support MIME type codecs parameter values.
    return m_hasVideo ? videoMP4 : audioMP4;
}

unsigned MediaRecorderPrivateWriter::audioBitRate() const
{
    return m_audioCompressor ? m_audioCompressor->bitRate() : 0;
}

unsigned MediaRecorderPrivateWriter::videoBitRate() const
{
    return m_videoCompressor ? m_videoCompressor->bitRate() : 0;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
