/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#import "MediaRecorderPrivateWriterAVFObjC.h"

#if ENABLE(MEDIA_RECORDER)

#import "AudioStreamDescription.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import <AVFoundation/AVAssetWriter.h>
#import <AVFoundation/AVAssetWriterInput.h>
#import <pal/spi/cocoa/AVAssetWriterSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/NativePromise.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/SpanCocoa.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface WebAVAssetWriterDelegate : NSObject <AVAssetWriterDelegate> {
    ThreadSafeWeakPtr<WebCore::MediaRecorderPrivateWriterListener> m_writer;
}

- (instancetype)initWithWriter:(WebCore::MediaRecorderPrivateWriterListener&)writer;

@end

@implementation WebAVAssetWriterDelegate {
};

- (instancetype)initWithWriter:(WebCore::MediaRecorderPrivateWriterListener&)writer
{
    ASSERT(isMainThread());
    self = [super init];
    if (self)
        self->m_writer = writer;

    return self;
}

- (void)assetWriter:(AVAssetWriter *)assetWriter didOutputSegmentData:(NSData *)segmentData segmentType:(AVAssetSegmentType)segmentType
{
    UNUSED_PARAM(assetWriter);
    UNUSED_PARAM(segmentType);
    if (RefPtr writer = m_writer.get())
        writer->appendData(span(segmentData));
}

@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaRecorderPrivateWriterAVFObjC);

std::unique_ptr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriterAVFObjC::create(MediaRecorderPrivateWriterListener& listener)
{
    NSError *error = nil;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr writer = adoptNS([PAL::allocAVAssetWriterInstance() initWithFileType:AVFileTypeMPEG4 error:&error]);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "create AVAssetWriter instance failed with error code %ld", (long)error.code);
        return nullptr;
    }

    return std::unique_ptr<MediaRecorderPrivateWriter> { new MediaRecorderPrivateWriterAVFObjC(WTFMove(writer), listener) };
}

MediaRecorderPrivateWriterAVFObjC::MediaRecorderPrivateWriterAVFObjC(RetainPtr<AVAssetWriter>&& writer, MediaRecorderPrivateWriterListener& listener)
    : m_delegate(adoptNS([[WebAVAssetWriterDelegate alloc] initWithWriter:listener]))
    , m_writer(WTFMove(writer))
{
    [m_writer setPreferredOutputSegmentInterval:PAL::kCMTimeIndefinite];
    [m_writer setDelegate:m_delegate.get()];
}

MediaRecorderPrivateWriterAVFObjC::~MediaRecorderPrivateWriterAVFObjC() = default;

std::optional<uint8_t> MediaRecorderPrivateWriterAVFObjC::addAudioTrack(CMFormatDescriptionRef description)
{
    m_audioAssetWriterInput = adoptNS([PAL::allocAVAssetWriterInputInstance() initWithMediaType:AVMediaTypeAudio outputSettings:nil sourceFormatHint:description]);
    [m_audioAssetWriterInput setExpectsMediaDataInRealTime:true];
    if (![m_writer canAddInput:m_audioAssetWriterInput.get()]) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriterAVFObjC::addAudioTrack failed canAddInput for audio with %ld", static_cast<long>([m_writer error].code));
        return { };
    }
    [m_writer addInput:m_audioAssetWriterInput.get()];

    m_audioTrackIndex = ++m_currentTrackIndex;

    return m_audioTrackIndex;
}

std::optional<uint8_t> MediaRecorderPrivateWriterAVFObjC::addVideoTrack(CMFormatDescriptionRef description, const std::optional<CGAffineTransform>& transform)
{
    m_videoAssetWriterInput = adoptNS([PAL::allocAVAssetWriterInputInstance() initWithMediaType:AVMediaTypeVideo outputSettings:nil sourceFormatHint:description]);
    [m_videoAssetWriterInput setExpectsMediaDataInRealTime:true];
    if (transform)
        [m_videoAssetWriterInput setTransform:*transform];

    if (![m_writer canAddInput:m_videoAssetWriterInput.get()]) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriterAVFObjC::addVideoTrack failed canAddInput for video with %ld", static_cast<long>([m_writer error].code));
        return { };
    }
    [m_writer addInput:m_videoAssetWriterInput.get()];

    m_videoTrackIndex = ++m_currentTrackIndex;

    return m_videoTrackIndex;
}

bool MediaRecorderPrivateWriterAVFObjC::allTracksAdded()
{
    // AVFileTypeProfileMPEG4AppleHLS allows muxed audio and video inputs
    // AVFileTypeProfileMPEG4CMAFCompliant allows only a single audio or video input
    [m_writer setOutputFileTypeProfile:m_currentTrackIndex > 1 ? AVFileTypeProfileMPEG4AppleHLS : AVFileTypeProfileMPEG4CMAFCompliant];

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (![m_writer startWriting]) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriterAVFObjC::startAssetWriter failed startWriting with %ld", static_cast<long>([m_writer error].code));
        return false;
    }
    END_BLOCK_OBJC_EXCEPTIONS

    [m_writer startSessionAtSourceTime:PAL::kCMTimeZero];
    return true;
}

MediaRecorderPrivateWriterAVFObjC::Result MediaRecorderPrivateWriterAVFObjC::muxFrame(const MediaSample& sample, uint8_t trackIndex)
{
    if (trackIndex == m_audioTrackIndex) {
        if (![m_audioAssetWriterInput isReadyForMoreMediaData])
            return Result::NotReady;

        auto result = [m_audioAssetWriterInput appendSampleBuffer:downcast<MediaSampleAVFObjC>(sample).sampleBuffer()] ? Result::Success : Result::Failure;
        if (result != Result::Success)
            RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPMediaRecorderPrivateWriterAVFObjC::muxFrame audio failed with %ld", static_cast<long>([m_writer error].code));
        return result;
    }

    ASSERT(trackIndex == m_videoTrackIndex);
    if (![m_videoAssetWriterInput isReadyForMoreMediaData])
        return Result::NotReady;

    auto result = [m_videoAssetWriterInput appendSampleBuffer:downcast<MediaSampleAVFObjC>(sample).sampleBuffer()] ? Result::Success : Result::Failure;
    if (result != Result::Success)
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPMediaRecorderPrivateWriterAVFObjC::muxFrame video failed with %ld", static_cast<long>([m_writer error].code));

    m_hasAddedVideoFrame = true;
    return result;
}

static inline void appendEndsPreviousSampleDurationMarker(AVAssetWriterInput *assetWriterInput, CMTime presentationTimeStamp)
{
    CMSampleTimingInfo timingInfo = { PAL::kCMTimeInvalid, presentationTimeStamp, presentationTimeStamp };

    CMSampleBufferRef buffer = NULL;
    auto error = PAL::CMSampleBufferCreate(kCFAllocatorDefault, NULL, true, NULL, NULL, NULL, 0, 1, &timingInfo, 0, NULL, &buffer);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter appendEndsPreviousSampleDurationMarker failed CMSampleBufferCreate with %d", error);
        return;
    }
    RetainPtr sampleBuffer = adoptCF(buffer);

    PAL::CMSetAttachment(sampleBuffer.get(), PAL::kCMSampleBufferAttachmentKey_EndsPreviousSampleDuration, kCFBooleanTrue, kCMAttachmentMode_ShouldPropagate);
    if (![assetWriterInput appendSampleBuffer:sampleBuffer.get()])
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateWriter appendSampleBuffer to writer input failed");
}

void MediaRecorderPrivateWriterAVFObjC::forceNewSegment(const MediaTime& endTime)
{
    if (m_hasAddedVideoFrame)
        appendEndsPreviousSampleDurationMarker(m_videoAssetWriterInput.get(), PAL::toCMTime(endTime));
    m_hasAddedVideoFrame = false;
    [m_writer flushSegment];
}

Ref<GenericPromise> MediaRecorderPrivateWriterAVFObjC::close(const MediaTime& endTime)
{
    if (m_hasAddedVideoFrame)
        appendEndsPreviousSampleDurationMarker(m_videoAssetWriterInput.get(), PAL::toCMTime(endTime));
    GenericPromise::Producer producer;
    Ref promise = producer.promise();
    [m_writer finishWritingWithCompletionHandler:makeBlockPtr([producer = WTFMove(producer)]() mutable {
        producer.resolve();
    }).get()];
    return promise;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
