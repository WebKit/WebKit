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

#import "config.h"
#import "MediaRecorderPrivateWriterCocoa.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AudioStreamDescription.h"
#import "Logging.h"
#import "MediaStreamTrackPrivate.h"
#import "WebAudioBufferList.h"
#import <AVFoundation/AVAssetWriter.h>
#import <AVFoundation/AVAssetWriterInput.h>
#import <wtf/CompletionHandler.h>
#import <wtf/FileSystem.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#undef AVEncoderBitRateKey
#define AVEncoderBitRateKey getAVEncoderBitRateKeyWithFallback()
#undef AVFormatIDKey
#define AVFormatIDKey getAVFormatIDKeyWithFallback()
#undef AVNumberOfChannelsKey
#define AVNumberOfChannelsKey getAVNumberOfChannelsKeyWithFallback()
#undef AVSampleRateKey
#define AVSampleRateKey getAVSampleRateKeyWithFallback()

namespace WebCore {

using namespace PAL;

static NSString *getAVFormatIDKeyWithFallback()
{
    if (PAL::canLoad_AVFoundation_AVFormatIDKey())
        return PAL::get_AVFoundation_AVFormatIDKey();

    RELEASE_LOG_ERROR(Media, "Failed to load AVFormatIDKey");
    return @"AVFormatIDKey";
}

static NSString *getAVNumberOfChannelsKeyWithFallback()
{
    if (PAL::canLoad_AVFoundation_AVNumberOfChannelsKey())
        return PAL::get_AVFoundation_AVNumberOfChannelsKey();

    RELEASE_LOG_ERROR(Media, "Failed to load AVNumberOfChannelsKey");
    return @"AVNumberOfChannelsKey";
}

static NSString *getAVSampleRateKeyWithFallback()
{
    if (PAL::canLoad_AVFoundation_AVSampleRateKey())
        return PAL::get_AVFoundation_AVSampleRateKey();

    RELEASE_LOG_ERROR(Media, "Failed to load AVSampleRateKey");
    return @"AVSampleRateKey";
}

static NSString *getAVEncoderBitRateKeyWithFallback()
{
    if (PAL::canLoad_AVFoundation_AVEncoderBitRateKey())
        return PAL::get_AVFoundation_AVEncoderBitRateKey();

    RELEASE_LOG_ERROR(Media, "Failed to load AVEncoderBitRateKey");
    return @"AVEncoderBitRateKey";
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

RefPtr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriter::create(bool hasAudio, int width, int height)
{
    NSString *directory = FileSystem::createTemporaryDirectory(@"videos");
    NSString *filename = [NSString stringWithFormat:@"/%lld.mp4", CMClockGetTime(CMClockGetHostTimeClock()).value];
    NSString *path = [directory stringByAppendingString:filename];

    NSURL *outputURL = [NSURL fileURLWithPath:path];
    String filePath = [path UTF8String];
    NSError *error = nil;
    auto avAssetWriter = adoptNS([PAL::allocAVAssetWriterInstance() initWithURL:outputURL fileType:AVFileTypeMPEG4 error:&error]);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "create AVAssetWriter instance failed with error code %ld", (long)error.code);
        return nullptr;
    }

    auto writer = adoptRef(*new MediaRecorderPrivateWriter(WTFMove(avAssetWriter), WTFMove(filePath)));

    if (hasAudio && !writer->setAudioInput())
        return nullptr;

    if (width && height) {
        if (!writer->setVideoInput(width, height))
            return nullptr;
    }

    return WTFMove(writer);
}

MediaRecorderPrivateWriter::MediaRecorderPrivateWriter(RetainPtr<AVAssetWriter>&& avAssetWriter, String&& filePath)
    : m_writer(WTFMove(avAssetWriter))
    , m_path(WTFMove(filePath))
{
}

MediaRecorderPrivateWriter::~MediaRecorderPrivateWriter()
{
    clear();
}

void MediaRecorderPrivateWriter::clear()
{
    if (m_videoInput) {
        m_videoInput.clear();
        dispatch_release(m_videoPullQueue);
    }
    if (m_audioInput) {
        m_audioInput.clear();
        dispatch_release(m_audioPullQueue);
    }
    if (m_writer)
        m_writer.clear();

    m_data = nullptr;
    if (auto completionHandler = WTFMove(m_fetchDataCompletionHandler))
        completionHandler(nullptr);
}

bool MediaRecorderPrivateWriter::setVideoInput(int width, int height)
{
    ASSERT(!m_videoInput);
    
    NSDictionary *compressionProperties = @{
        AVVideoAverageBitRateKey : @(width * height * 12),
        AVVideoExpectedSourceFrameRateKey : @(30),
        AVVideoMaxKeyFrameIntervalKey : @(120),
        AVVideoProfileLevelKey : AVVideoProfileLevelH264MainAutoLevel
    };

    NSDictionary *videoSettings = @{
        AVVideoCodecKey: AVVideoCodecH264,
        AVVideoWidthKey: @(width),
        AVVideoHeightKey: @(height),
        AVVideoCompressionPropertiesKey: compressionProperties
    };
    
    m_videoInput = adoptNS([PAL::allocAVAssetWriterInputInstance() initWithMediaType:AVMediaTypeVideo outputSettings:videoSettings sourceFormatHint:nil]);
    [m_videoInput setExpectsMediaDataInRealTime:true];
    
    if (![m_writer canAddInput:m_videoInput.get()]) {
        m_videoInput = nullptr;
        RELEASE_LOG_ERROR(MediaStream, "the video input is not allowed to add to the AVAssetWriter");
        return false;
    }
    [m_writer addInput:m_videoInput.get()];
    m_videoPullQueue = dispatch_queue_create("WebCoreVideoRecordingPullBufferQueue", DISPATCH_QUEUE_SERIAL);
    return true;
}

bool MediaRecorderPrivateWriter::setAudioInput()
{
    ASSERT(!m_audioInput);

    NSDictionary *audioSettings = @{
        AVEncoderBitRateKey : @(28000),
        AVFormatIDKey : @(kAudioFormatMPEG4AAC),
        AVNumberOfChannelsKey : @(1),
        AVSampleRateKey : @(22050)
    };

    m_audioInput = adoptNS([PAL::allocAVAssetWriterInputInstance() initWithMediaType:AVMediaTypeAudio outputSettings:audioSettings sourceFormatHint:nil]);
    [m_audioInput setExpectsMediaDataInRealTime:true];
    
    if (![m_writer canAddInput:m_audioInput.get()]) {
        m_audioInput = nullptr;
        RELEASE_LOG_ERROR(MediaStream, "the audio input is not allowed to add to the AVAssetWriter");
        return false;
    }
    [m_writer addInput:m_audioInput.get()];
    m_audioPullQueue = dispatch_queue_create("WebCoreAudioRecordingPullBufferQueue", DISPATCH_QUEUE_SERIAL);
    return true;
}

static inline RetainPtr<CMSampleBufferRef> copySampleBufferWithCurrentTimeStamp(CMSampleBufferRef originalBuffer)
{
    CMTime startTime = CMClockGetTime(CMClockGetHostTimeClock());
    CMItemCount count = 0;
    CMSampleBufferGetSampleTimingInfoArray(originalBuffer, 0, nil, &count);
    
    Vector<CMSampleTimingInfo> timeInfo(count);
    CMSampleBufferGetSampleTimingInfoArray(originalBuffer, count, timeInfo.data(), &count);
    
    for (CMItemCount i = 0; i < count; i++) {
        timeInfo[i].decodeTimeStamp = kCMTimeInvalid;
        timeInfo[i].presentationTimeStamp = startTime;
    }
    
    CMSampleBufferRef newBuffer = nullptr;
    auto error = CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, originalBuffer, count, timeInfo.data(), &newBuffer);
    if (error)
        return nullptr;
    return adoptCF(newBuffer);
}

void MediaRecorderPrivateWriter::appendVideoSampleBuffer(CMSampleBufferRef sampleBuffer)
{
    ASSERT(m_videoInput);
    if (m_isStopped)
        return;

    if (!m_hasStartedWriting) {
        if (![m_writer startWriting]) {
            m_isStopped = true;
            RELEASE_LOG_ERROR(MediaStream, "create AVAssetWriter instance failed with error code %ld", (long)[m_writer error]);
            return;
        }
        [m_writer startSessionAtSourceTime:CMClockGetTime(CMClockGetHostTimeClock())];
        m_hasStartedWriting = true;
        RefPtr<MediaRecorderPrivateWriter> protectedThis = this;
        [m_videoInput requestMediaDataWhenReadyOnQueue:m_videoPullQueue usingBlock:[this, protectedThis = WTFMove(protectedThis)] {
            do {
                if (![m_videoInput isReadyForMoreMediaData])
                    break;
                auto locker = holdLock(m_videoLock);
                if (m_videoBufferPool.isEmpty())
                    break;
                auto buffer = m_videoBufferPool.takeFirst();
                locker.unlockEarly();
                if (![m_videoInput appendSampleBuffer:buffer.get()])
                    break;
            } while (true);
            if (m_isStopped && m_videoBufferPool.isEmpty()) {
                [m_videoInput markAsFinished];
                m_finishWritingVideoSemaphore.signal();
            }
        }];
        return;
    }
    auto bufferWithCurrentTime = copySampleBufferWithCurrentTimeStamp(sampleBuffer);
    if (!bufferWithCurrentTime)
        return;

    auto locker = holdLock(m_videoLock);
    m_videoBufferPool.append(WTFMove(bufferWithCurrentTime));
}

static inline RetainPtr<CMFormatDescriptionRef> createAudioFormatDescription(const AudioStreamDescription& description)
{
    auto basicDescription = WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    CMFormatDescriptionRef format = nullptr;
    auto error = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, basicDescription, 0, NULL, 0, NULL, NULL, &format);
    if (error)
        return nullptr;
    return adoptCF(format);
}

static inline RetainPtr<CMSampleBufferRef> createAudioSampleBufferWithPacketDescriptions(CMFormatDescriptionRef format, size_t sampleCount)
{
    CMTime startTime = CMClockGetTime(CMClockGetHostTimeClock());
    CMSampleBufferRef sampleBuffer = nullptr;
    auto error = CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, NULL, false, NULL, NULL, format, sampleCount, startTime, NULL, &sampleBuffer);
    if (error)
        return nullptr;
    return adoptCF(sampleBuffer);
}

void MediaRecorderPrivateWriter::appendAudioSampleBuffer(const PlatformAudioData& data, const AudioStreamDescription& description, const WTF::MediaTime&, size_t sampleCount)
{
    ASSERT(m_audioInput);
    if ((!m_hasStartedWriting && m_videoInput) || m_isStopped)
        return;
    auto format = createAudioFormatDescription(description);
    if (!format)
        return;
    if (m_isFirstAudioSample) {
        if (!m_videoInput) {
            // audio-only recording.
            if (![m_writer startWriting]) {
                m_isStopped = true;
                return;
            }
            [m_writer startSessionAtSourceTime:CMClockGetTime(CMClockGetHostTimeClock())];
            m_hasStartedWriting = true;
        }
        m_isFirstAudioSample = false;
        RefPtr<MediaRecorderPrivateWriter> protectedThis = this;
        [m_audioInput requestMediaDataWhenReadyOnQueue:m_audioPullQueue usingBlock:[this, protectedThis = WTFMove(protectedThis)] {
            do {
                if (![m_audioInput isReadyForMoreMediaData])
                    break;
                auto locker = holdLock(m_audioLock);
                if (m_audioBufferPool.isEmpty())
                    break;
                auto buffer = m_audioBufferPool.takeFirst();
                locker.unlockEarly();
                [m_audioInput appendSampleBuffer:buffer.get()];
            } while (true);
            if (m_isStopped && m_audioBufferPool.isEmpty()) {
                [m_audioInput markAsFinished];
                m_finishWritingAudioSemaphore.signal();
            }
        }];
    }

    auto sampleBuffer = createAudioSampleBufferWithPacketDescriptions(format.get(), sampleCount);
    if (!sampleBuffer)
        return;
    auto error = CMSampleBufferSetDataBufferFromAudioBufferList(sampleBuffer.get(), kCFAllocatorDefault, kCFAllocatorDefault, 0, downcast<WebAudioBufferList>(data).list());
    if (error)
        return;

    auto locker = holdLock(m_audioLock);
    m_audioBufferPool.append(WTFMove(sampleBuffer));
}

void MediaRecorderPrivateWriter::stopRecording()
{
    if (m_isStopped)
        return;

    m_isStopped = true;
    if (!m_hasStartedWriting)
        return;
    ASSERT([m_writer status] == AVAssetWriterStatusWriting);
    if (m_videoInput)
        m_finishWritingVideoSemaphore.wait();

    if (m_audioInput)
        m_finishWritingAudioSemaphore.wait();

    m_isStopping = true;
    [m_writer finishWritingWithCompletionHandler:[this, weakPtr = makeWeakPtr(*this)]() mutable {
        callOnMainThread([this, weakPtr = WTFMove(weakPtr), buffer = SharedBuffer::createWithContentsOfFile(m_path)]() mutable {
            if (!weakPtr)
                return;

            m_isStopping = false;
            if (m_fetchDataCompletionHandler)
                m_fetchDataCompletionHandler(WTFMove(buffer));
            else
                m_data = WTFMove(buffer);

            m_isStopped = false;
            m_hasStartedWriting = false;
            m_isFirstAudioSample = true;
            clear();
        });
        m_finishWritingSemaphore.signal();
    }];
    m_finishWritingSemaphore.wait();
}

void MediaRecorderPrivateWriter::fetchData(CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    if (m_isStopping) {
        m_fetchDataCompletionHandler = WTFMove(completionHandler);
        return;
    }

    auto buffer = WTFMove(m_data);
    completionHandler(WTFMove(buffer));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
