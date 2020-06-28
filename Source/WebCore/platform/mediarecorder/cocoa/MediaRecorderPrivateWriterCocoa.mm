/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "AudioSampleBufferCompressor.h"
#include "AudioStreamDescription.h"
#include "Logging.h"
#include "MediaStreamTrackPrivate.h"
#include "VideoSampleBufferCompressor.h"
#include "WebAudioBufferList.h"
#include <AVFoundation/AVAssetWriter.h>
#include <AVFoundation/AVAssetWriterInput.h>
#include <AVFoundation/AVAssetWriter_Private.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/BlockPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/cf/TypeCastsCF.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/cocoa/AVFoundationSoftLink.h>

@interface WebAVAssetWriterDelegate : NSObject <AVAssetWriterDelegate> {
    WeakPtr<WebCore::MediaRecorderPrivateWriter> m_writer;
}

- (instancetype)initWithWriter:(WebCore::MediaRecorderPrivateWriter*)writer;
- (void)close;

@end

@implementation WebAVAssetWriterDelegate {
};

- (instancetype)initWithWriter:(WebCore::MediaRecorderPrivateWriter*)writer
{
    ASSERT(isMainThread());
    self = [super init];
    if (self)
        self->m_writer = makeWeakPtr(writer);

    return self;
}

- (void)assetWriter:(AVAssetWriter *)assetWriter didProduceFragmentedHeaderData:(NSData *)fragmentedHeaderData
{
    UNUSED_PARAM(assetWriter);
    if (!isMainThread()) {
        if (auto size = [fragmentedHeaderData length]) {
            callOnMainThread([protectedSelf = RetainPtr<WebAVAssetWriterDelegate>(self), buffer = WebCore::SharedBuffer::create(static_cast<const char*>([fragmentedHeaderData bytes]), size)]() mutable {
                if (protectedSelf->m_writer)
                    protectedSelf->m_writer->appendData(WTFMove(buffer));
            });
        }
        return;
    }

    if (m_writer)
        m_writer->appendData(static_cast<const char*>([fragmentedHeaderData bytes]), [fragmentedHeaderData length]);
}

- (void)assetWriter:(AVAssetWriter *)assetWriter didProduceFragmentedMediaData:(NSData *)fragmentedMediaData fragmentedMediaDataReport:(AVFragmentedMediaDataReport *)fragmentedMediaDataReport
{
    UNUSED_PARAM(assetWriter);
    UNUSED_PARAM(fragmentedMediaDataReport);
    if (!isMainThread()) {
        if (auto size = [fragmentedMediaData length]) {
            callOnMainThread([protectedSelf = RetainPtr<WebAVAssetWriterDelegate>(self), buffer = WebCore::SharedBuffer::create(static_cast<const char*>([fragmentedMediaData bytes]), size)]() mutable {
                if (protectedSelf->m_writer)
                    protectedSelf->m_writer->appendData(WTFMove(buffer));
            });
        }
        return;
    }

    if (m_writer)
        m_writer->appendData(static_cast<const char*>([fragmentedMediaData bytes]), [fragmentedMediaData length]);
}

- (void)close
{
    m_writer = nullptr;
}

@end

namespace WebCore {

using namespace PAL;

RefPtr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriter::create(bool hasAudio, int width, int height)
{
    auto writer = adoptRef(*new MediaRecorderPrivateWriter(hasAudio, width && height));
    if (!writer->initialize())
        return nullptr;
    return writer;
}

RefPtr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriter::create(const MediaStreamTrackPrivate* audioTrack, const MediaStreamTrackPrivate* videoTrack)
{
    int width = 0, height = 0;
    if (videoTrack) {
        auto& settings = videoTrack->settings();
        width = settings.width();
        height = settings.height();
    }
    return create(!!audioTrack, width, height);
}

void MediaRecorderPrivateWriter::compressedVideoOutputBufferCallback(void *mediaRecorderPrivateWriter, CMBufferQueueTriggerToken)
{
    callOnMainThread([weakWriter = makeWeakPtr(static_cast<MediaRecorderPrivateWriter*>(mediaRecorderPrivateWriter))] {
        if (weakWriter)
            weakWriter->processNewCompressedVideoSampleBuffers();
    });
}

void MediaRecorderPrivateWriter::compressedAudioOutputBufferCallback(void *mediaRecorderPrivateWriter, CMBufferQueueTriggerToken)
{
    callOnMainThread([weakWriter = makeWeakPtr(static_cast<MediaRecorderPrivateWriter*>(mediaRecorderPrivateWriter))] {
        if (weakWriter)
            weakWriter->processNewCompressedAudioSampleBuffers();
    });
}

MediaRecorderPrivateWriter::MediaRecorderPrivateWriter(bool hasAudio, bool hasVideo)
    : m_hasAudio(hasAudio)
    , m_hasVideo(hasVideo)
{
}

MediaRecorderPrivateWriter::~MediaRecorderPrivateWriter()
{
    clear();
}

bool MediaRecorderPrivateWriter::initialize()
{
    NSError *error = nil;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    m_writer = adoptNS([PAL::allocAVAssetWriterInstance() initWithFileType:AVFileTypeMPEG4 error:&error]);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "create AVAssetWriter instance failed with error code %ld", (long)error.code);
        return false;
    }

    m_writerDelegate = adoptNS([[WebAVAssetWriterDelegate alloc] initWithWriter: this]);
    [m_writer.get() setDelegate:m_writerDelegate.get()];

    if (m_hasAudio) {
        m_audioCompressor = AudioSampleBufferCompressor::create(compressedAudioOutputBufferCallback, this);
        if (!m_audioCompressor)
            return false;
    }
    if (m_hasVideo) {
        m_videoCompressor = VideoSampleBufferCompressor::create(kCMVideoCodecType_H264, compressedVideoOutputBufferCallback, this);
        if (!m_videoCompressor)
            return false;
    }
    return true;
}

void MediaRecorderPrivateWriter::processNewCompressedVideoSampleBuffers()
{
    ASSERT(m_hasVideo);
    if (!m_videoFormatDescription) {
        m_videoFormatDescription = CMSampleBufferGetFormatDescription(m_videoCompressor->getOutputSampleBuffer());

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
        m_audioFormatDescription = CMSampleBufferGetFormatDescription(m_audioCompressor->getOutputSampleBuffer());
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
        if (![m_writer.get() canAddInput:m_videoAssetWriterInput.get()]) {
            RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter::startAssetWriter failed canAddInput for video");
            return;
        }
        [m_writer.get() addInput:m_videoAssetWriterInput.get()];
    }

    if (m_hasAudio) {
        m_audioAssetWriterInput = adoptNS([PAL::allocAVAssetWriterInputInstance() initWithMediaType:AVMediaTypeAudio outputSettings:nil sourceFormatHint:m_audioFormatDescription.get()]);
        [m_audioAssetWriterInput setExpectsMediaDataInRealTime:true];
        if (![m_writer.get() canAddInput:m_audioAssetWriterInput.get()]) {
            RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter::startAssetWriter failed canAddInput for audio");
            return;
        }
        [m_writer.get() addInput:m_audioAssetWriterInput.get()];
    }

    if (![m_writer.get() startWriting]) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter::startAssetWriter failed startWriting");
        return;
    }

    [m_writer.get() startSessionAtSourceTime:kCMTimeZero];

    appendCompressedSampleBuffers();

    m_hasStartedWriting = true;
}

bool MediaRecorderPrivateWriter::appendCompressedAudioSampleBuffer()
{
    if (!m_audioCompressor)
        return false;

    if (![m_audioAssetWriterInput isReadyForMoreMediaData])
        return false;

    auto buffer = m_audioCompressor->takeOutputSampleBuffer();
    if (!buffer)
        return false;

    [m_audioAssetWriterInput.get() appendSampleBuffer:buffer.get()];
    return true;
}

bool MediaRecorderPrivateWriter::appendCompressedVideoSampleBuffer()
{
    if (!m_videoCompressor)
        return false;

    if (![m_videoAssetWriterInput isReadyForMoreMediaData])
        return false;

    auto buffer = m_videoCompressor->takeOutputSampleBuffer();
    if (!buffer)
        return false;

    m_lastVideoPresentationTime = CMSampleBufferGetPresentationTimeStamp(buffer.get());
    m_lastVideoDecodingTime = CMSampleBufferGetDecodeTimeStamp(buffer.get());
    m_hasEncodedVideoSamples = true;

    [m_videoAssetWriterInput.get() appendSampleBuffer:buffer.get()];
    return true;
}

void MediaRecorderPrivateWriter::appendCompressedSampleBuffers()
{
    while (appendCompressedVideoSampleBuffer() || appendCompressedAudioSampleBuffer()) { };
}

static inline void appendEndsPreviousSampleDurationMarker(AVAssetWriterInput *assetWriterInput, CMTime presentationTimeStamp, CMTime decodingTimeStamp)
{
    CMSampleTimingInfo timingInfo = { kCMTimeInvalid, presentationTimeStamp, decodingTimeStamp};

    CMSampleBufferRef buffer = NULL;
    auto error = CMSampleBufferCreate(kCFAllocatorDefault, NULL, true, NULL, NULL, NULL, 0, 1, &timingInfo, 0, NULL, &buffer);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter appendEndsPreviousSampleDurationMarker failed CMSampleBufferCreate with %d", error);
        return;
    }
    auto sampleBuffer = adoptCF(buffer);

    CMSetAttachment(sampleBuffer.get(), kCMSampleBufferAttachmentKey_EndsPreviousSampleDuration, kCFBooleanTrue, kCMAttachmentMode_ShouldPropagate);
    if (![assetWriterInput appendSampleBuffer:sampleBuffer.get()])
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter appendSampleBuffer to writer input failed");
}

void MediaRecorderPrivateWriter::appendEndOfVideoSampleDurationIfNeeded(CompletionHandler<void()>&& completionHandler)
{
    if (!m_hasEncodedVideoSamples) {
        completionHandler();
        return;
    }
    if ([m_videoAssetWriterInput isReadyForMoreMediaData]) {
        appendEndsPreviousSampleDurationMarker(m_videoAssetWriterInput.get(), m_lastVideoPresentationTime, m_lastVideoDecodingTime);
        completionHandler();
        return;
    }

    auto block = makeBlockPtr([this, weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)]() mutable {
        if (weakThis) {
            appendEndsPreviousSampleDurationMarker(m_videoAssetWriterInput.get(), m_lastVideoPresentationTime, m_lastVideoDecodingTime);
            [m_videoAssetWriterInput markAsFinished];
        }
        completionHandler();
    });
    [m_videoAssetWriterInput requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:block.get()];
}

void MediaRecorderPrivateWriter::flushCompressedSampleBuffers(CompletionHandler<void()>&& completionHandler)
{
    appendCompressedSampleBuffers();
    appendEndOfVideoSampleDurationIfNeeded(WTFMove(completionHandler));
}

void MediaRecorderPrivateWriter::clear()
{
    if (m_writer)
        m_writer.clear();

    m_data = nullptr;
    if (auto completionHandler = WTFMove(m_fetchDataCompletionHandler))
        completionHandler(nullptr);
}


static inline RetainPtr<CMSampleBufferRef> copySampleBufferWithCurrentTimeStamp(CMSampleBufferRef originalBuffer)
{
    CMTime startTime = CMClockGetTime(CMClockGetHostTimeClock());
    CMItemCount count = 0;
    CMSampleBufferGetSampleTimingInfoArray(originalBuffer, 0, nil, &count);

    Vector<CMSampleTimingInfo> timeInfo(count);
    CMSampleBufferGetSampleTimingInfoArray(originalBuffer, count, timeInfo.data(), &count);

    for (auto i = 0; i < count; i++) {
        timeInfo[i].decodeTimeStamp = kCMTimeInvalid;
        timeInfo[i].presentationTimeStamp = startTime;
    }

    CMSampleBufferRef newBuffer = nullptr;
    if (auto error = CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, originalBuffer, count, timeInfo.data(), &newBuffer)) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter CMSampleBufferCreateCopyWithNewTiming failed with %d", error);
        return nullptr;
    }
    return adoptCF(newBuffer);
}

void MediaRecorderPrivateWriter::appendVideoSampleBuffer(CMSampleBufferRef sampleBuffer)
{
    // FIXME: We should not set the timestamps if they are already set.
    if (auto bufferWithCurrentTime = copySampleBufferWithCurrentTimeStamp(sampleBuffer))
        m_videoCompressor->addSampleBuffer(bufferWithCurrentTime.get());
}

static inline RetainPtr<CMFormatDescriptionRef> createAudioFormatDescription(const AudioStreamDescription& description)
{
    auto basicDescription = WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    CMFormatDescriptionRef format = nullptr;
    auto error = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, basicDescription, 0, NULL, 0, NULL, NULL, &format);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter CMAudioFormatDescriptionCreate failed with %d", error);
        return nullptr;
    }
    return adoptCF(format);
}

static inline RetainPtr<CMSampleBufferRef> createAudioSampleBuffer(const PlatformAudioData& data, const AudioStreamDescription& description, const WTF::MediaTime& time, size_t sampleCount)
{
    auto format = createAudioFormatDescription(description);
    if (!format)
        return nullptr;

    CMSampleBufferRef sampleBuffer = nullptr;
    auto error = CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, NULL, false, NULL, NULL, format.get(), sampleCount, toCMTime(time), NULL, &sampleBuffer);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter createAudioSampleBufferWithPacketDescriptions failed with %d", error);
        return nullptr;
    }
    auto buffer = adoptCF(sampleBuffer);

    error = CMSampleBufferSetDataBufferFromAudioBufferList(buffer.get(), kCFAllocatorDefault, kCFAllocatorDefault, 0, downcast<WebAudioBufferList>(data).list());
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter CMSampleBufferSetDataBufferFromAudioBufferList failed with %d", error);
        return nullptr;
    }
    return buffer;
}

void MediaRecorderPrivateWriter::appendAudioSampleBuffer(const PlatformAudioData& data, const AudioStreamDescription& description, const WTF::MediaTime& time, size_t sampleCount)
{
    if (auto sampleBuffer = createAudioSampleBuffer(data, description, time, sampleCount))
        m_audioCompressor->addSampleBuffer(sampleBuffer.get());
}

void MediaRecorderPrivateWriter::stopRecording()
{
    if (m_isStopped)
        return;

    m_isStopped = true;

    if (m_videoCompressor)
        m_videoCompressor->finish();
    if (m_audioCompressor)
        m_audioCompressor->finish();

    m_isStopping = true;
    // We hop to the main thread since finishing the video compressor might trigger starting the writer asynchronously.
    callOnMainThread([this, weakThis = makeWeakPtr(this)]() mutable {
        auto whenFinished = [this, weakThis] {
            if (!weakThis)
                return;

            m_isStopping = false;
            m_isStopped = false;
            m_hasStartedWriting = false;

            if (m_writer)
                m_writer.clear();
            if (m_fetchDataCompletionHandler)
                m_fetchDataCompletionHandler(std::exchange(m_data, nullptr));
        };

        if (!m_hasStartedWriting) {
            whenFinished();
            return;
        }

        ASSERT([m_writer status] == AVAssetWriterStatusWriting);
        flushCompressedSampleBuffers([this, weakThis = WTFMove(weakThis), whenFinished = WTFMove(whenFinished)]() mutable {
            if (!weakThis)
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

void MediaRecorderPrivateWriter::fetchData(CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    if (m_isStopping) {
        m_fetchDataCompletionHandler = WTFMove(completionHandler);
        return;
    }

    if (m_hasStartedWriting) {
        flushCompressedSampleBuffers([this, weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)]() mutable {
            if (!weakThis) {
                completionHandler(nullptr);
                return;
            }

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [m_writer flush];
            ALLOW_DEPRECATED_DECLARATIONS_END

            callOnMainThread([this, weakThis = WTFMove(weakThis), completionHandler = WTFMove(completionHandler)]() mutable {
                if (!weakThis) {
                    completionHandler(nullptr);
                    return;
                }

                completionHandler(std::exchange(m_data, nullptr));
            });
        });
        return;
    }

    completionHandler(std::exchange(m_data, nullptr));
}

void MediaRecorderPrivateWriter::appendData(const char* data, size_t size)
{
    if (!m_data) {
        m_data = SharedBuffer::create(data, size);
        return;
    }
    m_data->append(data, size);
}

void MediaRecorderPrivateWriter::appendData(Ref<SharedBuffer>&& buffer)
{
    if (!m_data) {
        m_data = WTFMove(buffer);
        return;
    }
    m_data->append(WTFMove(buffer));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
