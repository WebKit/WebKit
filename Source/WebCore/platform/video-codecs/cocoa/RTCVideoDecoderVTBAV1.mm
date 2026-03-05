/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
 *
 */

#import "config.h"
#import "RTCVideoDecoderVTBAV1.h"

#if USE(LIBWEBRTC)

#import "AV1Utilities.h"
#import "CMUtilities.h"
#import "Logging.h"
#import <span>
#import <webrtc/modules/video_coding/include/video_error_codes.h>
#import <wtf/BlockPtr.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/FastMalloc.h>
#import <wtf/RetainPtr.h>
#import <wtf/cf/VectorCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

using namespace WebCore;

typedef struct OpaqueVTDecompressionSession*  VTDecompressionSessionRef;
typedef void (*VTDecompressionOutputCallback)(void * decompressionOutputRefCon, void * sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration);

static RetainPtr<CMVideoFormatDescriptionRef> computeAV1InputFormat(std::span<const uint8_t> data, int32_t width, int32_t height)
{
#if ENABLE(AV1)
    RefPtr videoInfo = createVideoInfoFromAV1Stream(data);
    if (!videoInfo)
        return { };

    if (width && videoInfo->size().width() != width)
        return { };
    if (height && videoInfo->size().height() != height)
        return { };

    return createFormatDescriptionFromTrackInfo(*videoInfo);
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    ASSERT_NOT_REACHED();
    return { };
#endif
}

struct RTCFrameDecodeParams {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RTCFrameDecodeParams);

    BlockPtr<void(CVPixelBufferRef, long long, long long, bool)> callback;
    int64_t timestamp { 0 };
};

@interface RTCVideoDecoderVTBAV1 ()
- (void)setError:(OSStatus)error;
@end

static RetainPtr<CMSampleBufferRef> av1BufferToCMSampleBuffer(std::span<const uint8_t> buffer, CMVideoFormatDescriptionRef videoFormat)
{
    CMBlockBufferRef newVlockBuffer;
    if (auto error = PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, buffer.size(), kCFAllocatorDefault, NULL, 0, buffer.size(), kCMBlockBufferAssureMemoryNowFlag, &newVlockBuffer)) {
        RELEASE_LOG_ERROR(WebRTC, "AV1BufferToCMSampleBuffer CMBlockBufferCreateWithMemoryBlock failed with: %d", error);
        return nullptr;
    }
    auto blockBuffer = adoptCF(newVlockBuffer);

    if (auto error = PAL::CMBlockBufferReplaceDataBytes(buffer.data(), blockBuffer.get(), 0, buffer.size())) {
        RELEASE_LOG_ERROR(WebRTC, "AV1BufferToCMSampleBuffer CMBlockBufferReplaceDataBytes failed with: %d", error);
        return nullptr;
    }

    CMSampleBufferRef sampleBuffer = nullptr;
    if (auto error = PAL::CMSampleBufferCreate(kCFAllocatorDefault, blockBuffer.get(), true, nullptr, nullptr, videoFormat, 1, 0, nullptr, 0, nullptr, &sampleBuffer)) {
        RELEASE_LOG_ERROR(WebRTC, "AV1BufferToCMSampleBuffer CMSampleBufferCreate failed with: %d", error);
        return nullptr;
    }

    return adoptCF(sampleBuffer);
}

static void av1DecompressionOutputCallback(void* decoderRef, void* params, OSStatus status, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime timestamp, CMTime)
{
    std::unique_ptr<RTCFrameDecodeParams> decodeParams(static_cast<RTCFrameDecodeParams*>(params));
    if (status != noErr || !imageBuffer) {
        RTCVideoDecoderVTBAV1 *decoder = (__bridge RTCVideoDecoderVTBAV1 *)decoderRef;
        [decoder setError:status != noErr ? status : 1];
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 failed to decode with status: %d", status);
        decodeParams->callback.get()(nil, 0, 0, false);
        return;
    }

    static const int64_t kNumNanosecsPerSec = 1000000000;
    decodeParams->callback.get()(imageBuffer, decodeParams->timestamp, PAL::CMTimeGetSeconds(timestamp) * kNumNanosecsPerSec, false);
}

@implementation RTCVideoDecoderVTBAV1 {
    RetainPtr<CMVideoFormatDescriptionRef> _videoFormat;
    RetainPtr<VTDecompressionSessionRef> _decompressionSession;
    BlockPtr<void(CVPixelBufferRef, long long, long long, bool)> _callback;
    OSStatus _error;
    int32_t _width;
    int32_t _height;
}

- (instancetype)init {
    self = [super init];
    return self;
}

- (void)dealloc {
    [self destroyDecompressionSession];
    _videoFormat = nullptr;
    [super dealloc];
}

- (void)setWidth:(uint16_t)width height:(uint16_t)height {
    _width = width;
    _height = height;
}

- (NSInteger)decodeData:(const uint8_t *)rawData size:(size_t)size timeStamp:(int64_t)timeStamp {
    if (_error != noErr) {
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 Last frame decode failed");
        _error = noErr;
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    auto data = unsafeMakeSpan(rawData, size);

    if (auto inputFormat = computeAV1InputFormat(data, _width, _height)) {
        if (!PAL::CMFormatDescriptionEqual(inputFormat.get(), _videoFormat.get())) {
            _videoFormat = WTF::move(inputFormat);
            if (int error = [self resetDecompressionSession]; error != WEBRTC_VIDEO_CODEC_OK) {
                _videoFormat = nullptr;
                return error;
            }
        }
    }

    if (!_videoFormat)
        return WEBRTC_VIDEO_CODEC_ERROR;

    auto sampleBuffer = av1BufferToCMSampleBuffer(data, _videoFormat.get());
    if (!sampleBuffer)
        return WEBRTC_VIDEO_CODEC_ERROR;

    VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    std::unique_ptr<RTCFrameDecodeParams> frameDecodeParams(new RTCFrameDecodeParams { _callback, timeStamp });
    auto status = VTDecompressionSessionDecodeFrame(_decompressionSession.get(), sampleBuffer.get(), decodeFlags, frameDecodeParams.release(), nullptr);

#if PLATFORM(IOS_FAMILY)
    if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) && [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
        RELEASE_LOG_INFO(WebRTC, "Failed to decode frame with code %d, retrying decode after decompression session reset", status);
        frameDecodeParams.reset(new RTCFrameDecodeParams { _callback, timeStamp });
        status = VTDecompressionSessionDecodeFrame(_decompressionSession.get(), sampleBuffer.get(), decodeFlags, frameDecodeParams.release(), nullptr);
    }
#endif

    if (status != noErr) {
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 failed to decode frame with code %d", status);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderVTBAV1Callback)callback {
    _callback = callback;
}

- (void)setError:(OSStatus)error {
    _error = error;
}

- (NSInteger)releaseDecoder {
    [self destroyDecompressionSession];
    _videoFormat = nullptr;
    _callback = nullptr;
    return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
    [self destroyDecompressionSession];

    if (!_videoFormat)
        return WEBRTC_VIDEO_CODEC_OK;

    static size_t const attributesSize = 3;
    CFTypeRef keys[attributesSize] = {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        WebCore::kCVPixelBufferOpenGLCompatibilityKey,
#elif PLATFORM(IOS_FAMILY)
        WebCore::kCVPixelBufferExtendedPixelsRightKey,
#endif
        WebCore::kCVPixelBufferIOSurfacePropertiesKey,
        WebCore::kCVPixelBufferPixelFormatTypeKey
    };

    auto ioSurfaceValue = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
    auto pixelFormat = adoptCF(CFNumberCreate(nullptr, kCFNumberLongType, &nv12type));
    CFTypeRef values[attributesSize] = { kCFBooleanTrue, ioSurfaceValue.get(), pixelFormat.get() };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, attributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    VTDecompressionOutputCallbackRecord record { av1DecompressionOutputCallback, (__bridge void *)self };
    VTDecompressionSessionRef decompressionSession;
    auto status = VTDecompressionSessionCreate(nullptr, _videoFormat.get(), nullptr, attributes.get(), &record, &decompressionSession);

    if (status != noErr) {
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 failed to create decompression session %d", status);
        [self destroyDecompressionSession];
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    _decompressionSession = adoptCF(decompressionSession);
    [self configureDecompressionSession];
    return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
    ASSERT(_decompressionSession);
#if PLATFORM(IOS_FAMILY)
    VTSessionSetProperty(_decompressionSession.get(), kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
    if (_decompressionSession) {
        VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession.get());
        VTDecompressionSessionInvalidate(_decompressionSession.get());
        _decompressionSession = nullptr;
    }
}

- (void)flush {
    if (_decompressionSession)
        VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession.get());
}

@end

#endif // USE(LIBWEBRTC)
