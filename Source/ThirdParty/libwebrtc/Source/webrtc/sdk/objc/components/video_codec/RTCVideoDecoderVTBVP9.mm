/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

/*
 * VP9 decoding through VTB, highly based on RTCVideoDecoderH264.mm
 */

#import "RTCVideoDecoderVTBVP9.h"

#if defined(RTC_ENABLE_VP9)

#import <VideoToolbox/VideoToolbox.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"

#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

extern const CFStringRef kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms;

static uint8_t convertSubsampling(absl::optional<webrtc::Vp9YuvSubsampling> value)
{
    if (!value)
        return 1;

    switch (*value) {
    case webrtc::Vp9YuvSubsampling::k444:
        return 3;
    case webrtc::Vp9YuvSubsampling::k440:
        return 1;
    case webrtc::Vp9YuvSubsampling::k422:
        return 2;
    case webrtc::Vp9YuvSubsampling::k420:
        return 1;
    }
}

rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> computeInputFormat(const uint8_t* data, size_t size, int32_t width, int32_t height)
{
  constexpr size_t VPCodecConfigurationContentsSize = 12;

  auto result = webrtc::ParseUncompressedVp9Header(rtc::MakeArrayView(data, size));

  if (!result)
      return { };
  auto chromaSubsampling = convertSubsampling(result->sub_sampling);
  uint8_t bitDepthChromaAndRange = (0xF & (uint8_t)result->bit_detph) << 4 | (0x7 & chromaSubsampling) << 1 | (0x1 & (uint8_t)result->color_range.value_or(webrtc::Vp9ColorRange::kStudio));

  uint8_t record[VPCodecConfigurationContentsSize];
  memset((void*)record, 0, VPCodecConfigurationContentsSize);
  // Version and flags (4 bytes)
  record[0] = 1;
  // profile
  record[4] = result->profile;
  // level
  record[5] = 10;
  // bitDepthChromaAndRange
  record[6] = bitDepthChromaAndRange;
  // colourPrimaries
  record[7] = 2; // Unspecified.
  // transferCharacteristics
  record[8] = 2; // Unspecified.
  // matrixCoefficients
  record[9] = 2; // Unspecified.

  auto cfData = rtc::ScopedCF(CFDataCreate(kCFAllocatorDefault, record, VPCodecConfigurationContentsSize));
  auto configurationDict = @{ @"vpcC": (__bridge NSData *)cfData.get() };
  auto extensions = @{ (__bridge NSString *)kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms: configurationDict };

  CMVideoFormatDescriptionRef formatDescription = nullptr;
  // Use kCMVideoCodecType_VP9 once added to CMFormatDescription.h
  if (noErr != CMVideoFormatDescriptionCreate(kCFAllocatorDefault, 'vp09', width, height, (__bridge CFDictionaryRef)extensions, &formatDescription))
      return { };

  return rtc::ScopedCF(formatDescription);
}

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCFrameDecodeParams {
  RTCFrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts) : callback(cb), timestamp(ts) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};

@interface RTCVideoDecoderVTBVP9 ()
- (void)setError:(OSStatus)error;
@end

CMSampleBufferRef VP9BufferToCMSampleBuffer(const uint8_t* buffer,
                              size_t buffer_size,
                              CMVideoFormatDescriptionRef video_format) {
  CMBlockBufferRef new_block_buffer;
  if (auto error = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, buffer_size, kCFAllocatorDefault, NULL, 0, buffer_size, kCMBlockBufferAssureMemoryNowFlag, &new_block_buffer)) {
    RTC_LOG(LS_ERROR) << "VP9BufferToCMSampleBuffer CMBlockBufferCreateWithMemoryBlock failed with: " << error;
    return nullptr;
  }
  auto block_buffer = rtc::ScopedCF(new_block_buffer);

  if (auto error = CMBlockBufferReplaceDataBytes(buffer, block_buffer.get(), 0, buffer_size)) {
    RTC_LOG(LS_ERROR) << "VP9BufferToCMSampleBuffer CMBlockBufferReplaceDataBytes failed with: " << error;
    return nullptr;
  }

  CMSampleBufferRef sample_buffer = nullptr;
  if (auto error = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer.get(), true, nullptr, nullptr, video_format, 1, 0, nullptr, 0, nullptr, &sample_buffer)) {
    RTC_LOG(LS_ERROR) << "VP9BufferToCMSampleBuffer CMSampleBufferCreate failed with: " << error;
    return nullptr;
  }
  return sample_buffer;
}

// This is the callback function that VideoToolbox calls when decode is
// complete.
void vp9DecompressionOutputCallback(void *decoderRef,
                                 void *params,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime timestamp,
                                 CMTime duration) {
  std::unique_ptr<RTCFrameDecodeParams> decodeParams(reinterpret_cast<RTCFrameDecodeParams *>(params));
  if (status != noErr || !imageBuffer) {
    RTCVideoDecoderVTBVP9 *decoder = (__bridge RTCVideoDecoderVTBVP9 *)decoderRef;
    [decoder setError:status != noErr ? status : 1];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    decodeParams->callback(nil);
    return;
  }

  RTCCVPixelBuffer *frameBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:imageBuffer];
  RTCVideoFrame *decodedFrame =
      [[RTCVideoFrame alloc] initWithBuffer:frameBuffer
                                   rotation:RTCVideoRotation_0
                                timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
}

// Decoder.
@implementation RTCVideoDecoderVTBVP9 {
  CMVideoFormatDescriptionRef _videoFormat;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
  int32_t _width;
  int32_t _height;
  bool _shouldCheckFormat;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _shouldCheckFormat = true;
  }
  return self;
}

- (void)dealloc {
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)decode:(RTCEncodedImage *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);
  if (inputImage.frameType == RTCFrameTypeVideoFrameKey) {
    [self setWidth:inputImage.encodedWidth height: inputImage.encodedHeight];
  }
  return [self decodeData: (uint8_t *)inputImage.buffer.bytes size: inputImage.buffer.length timeStamp: inputImage.timeStamp];
}

- (void)setWidth:(uint16_t)width height:(uint16_t)height {
  _width = width;
  _height = height;
  _shouldCheckFormat = true;
}

- (NSInteger)decodeData:(const uint8_t *)data
        size:(size_t)size
        timeStamp:(int64_t)timeStamp {

  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (_shouldCheckFormat || !_videoFormat) {
    auto inputFormat = computeInputFormat(data, size, _width, _height);
    if (inputFormat) {
      _shouldCheckFormat = false;
      // Check if the video format has changed, and reinitialize decoder if
      // needed.
      if (!CMFormatDescriptionEqual(inputFormat.get(), _videoFormat)) {
        [self setVideoFormat:inputFormat.get()];
        int resetDecompressionSessionError = [self resetDecompressionSession];
        if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
          [self setVideoFormat:nullptr];
          return resetDecompressionSessionError;
        }
      }
    }
  }
  if (!_videoFormat) {
    // We received a frame but we don't have format information so we can't
    // decode it.
    RTC_LOG(LS_WARNING) << "Missing video format.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  auto sampleBuffer = rtc::ScopedCF(VP9BufferToCMSampleBuffer(data, size, _videoFormat));
  if (!sampleBuffer) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<RTCFrameDecodeParams> frameDecodeParams;
  frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, timeStamp));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer.get(), decodeFlags, frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active or decoder malfunctions and retry the decode request.
  if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_INFO) << "Failed to decode frame with code: " << status
                     << " retrying decode after decompression session reset";
    frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, timeStamp));
    status = VTDecompressionSessionDecodeFrame(
        _decompressionSession, sampleBuffer.get(), decodeFlags, frameDecodeParams.release(), nullptr);
  }
#endif
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _callback = callback;
}

- (void)setError:(OSStatus)error {
  _error = error;
}

- (NSInteger)releaseDecoder {
  // Need to invalidate the session so that callbacks no longer occur and it
  // is safe to null out the callback.
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
  [self destroyDecompressionSession];

  if (!_videoFormat) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Set keys for OpenGL and IOSurface compatibilty, which makes the encoder
  // create pixel buffers with GPU backed memory. The intent here is to pass
  // the pixel buffers directly so we avoid a texture upload later during
  // rendering. This currently is moot because we are converting back to an
  // I420 frame after decode, but eventually we will be able to plumb
  // CVPixelBuffers directly to the renderer.
  // TODO(tkchin): Maybe only set OpenGL/IOSurface keys if we know that that
  // we can pass CVPixelBuffers as native handles in decoder output.
  static size_t const attributesSize = 3;
  CFTypeRef keys[attributesSize] = {
#if defined(WEBRTC_MAC) || defined(WEBRTC_MAC_CATALYST)
    kCVPixelBufferOpenGLCompatibilityKey,
#elif defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef ioSurfaceValue = CreateCFTypeDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue, pixelFormat};
  CFDictionaryRef attributes = CreateCFTypeDictionary(keys, values, attributesSize);
  if (ioSurfaceValue) {
    CFRelease(ioSurfaceValue);
    ioSurfaceValue = nullptr;
  }
  if (pixelFormat) {
    CFRelease(pixelFormat);
    pixelFormat = nullptr;
  }

  VTDecompressionOutputCallbackRecord record = {
      vp9DecompressionOutputCallback, (__bridge void *)self,
  };
  OSStatus status = VTDecompressionSessionCreate(
      nullptr, _videoFormat, nullptr, attributes, &record, &_decompressionSession);
  CFRelease(attributes);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create decompression session: " << status;
    [self destroyDecompressionSession];
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  [self configureDecompressionSession];

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
  RTC_DCHECK(_decompressionSession);
#if defined(WEBRTC_IOS)
  VTSessionSetProperty(_decompressionSession, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
  if (_decompressionSession) {
#if defined(WEBRTC_WEBKIT_BUILD)
    VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
#else
#if defined(WEBRTC_IOS)
    if ([UIDevice isIOS11OrLater]) {
      VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
    }
#endif
#endif
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
}

- (void)flush {
  if (_decompressionSession)
    VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
}

- (void)setVideoFormat:(CMVideoFormatDescriptionRef)videoFormat {
  if (_videoFormat == videoFormat) {
    return;
  }
  if (_videoFormat) {
    CFRelease(_videoFormat);
  }
  _videoFormat = videoFormat;
  if (_videoFormat) {
    CFRetain(_videoFormat);
  }
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

@end

#endif // defined(RTC_ENABLE_VP9)
