/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoDecoderH265.h"

#import <VideoToolbox/VideoToolbox.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCH265FrameDecodeParams {
  RTCH265FrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts)
      : callback(cb), timestamp(ts) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};

@interface RTCVideoDecoderH265 ()
- (void)setError:(OSStatus)error;
@end

static void overrideColorSpaceAttachments(CVImageBufferRef imageBuffer) {
  CVBufferRemoveAttachment(imageBuffer, kCVImageBufferCGColorSpaceKey);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferTransferFunctionKey, kCVImageBufferTransferFunction_sRGB, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferYCbCrMatrixKey, kCVImageBufferYCbCrMatrix_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, (CFStringRef)@"ColorInfoGuessedBy", (CFStringRef)@"RTCVideoDecoderH265", kCVAttachmentMode_ShouldPropagate);
}

// This is the callback function that VideoToolbox calls when decode is
// complete.
void h265DecompressionOutputCallback(void* decoderRef,
                                     void* params,
                                     OSStatus status,
                                     VTDecodeInfoFlags infoFlags,
                                     CVImageBufferRef imageBuffer,
                                     CMTime timestamp,
                                     CMTime duration) {
  if (status != noErr || !imageBuffer) {
    RTCVideoDecoderH265 *decoder = (__bridge RTCVideoDecoderH265 *)decoderRef;
    [decoder setError:status != noErr ? status : 1];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }

  overrideColorSpaceAttachments(imageBuffer);

  std::unique_ptr<RTCH265FrameDecodeParams> decodeParams(reinterpret_cast<RTCH265FrameDecodeParams*>(params));
  // TODO(tkchin): Handle CVO properly.
  RTCCVPixelBuffer* frameBuffer =
      [[RTCCVPixelBuffer alloc] initWithPixelBuffer:imageBuffer];
  RTCVideoFrame* decodedFrame = [[RTCVideoFrame alloc]
      initWithBuffer:frameBuffer
            rotation:RTCVideoRotation_0
         timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
}

// Decoder.
@implementation RTCVideoDecoderH265 {
  CMVideoFormatDescriptionRef _videoFormat;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
}

- (instancetype)init {
  if (self = [super init]) {
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

- (NSInteger)decode:(RTCEncodedImage*)inputImage
          missingFrames:(BOOL)missingFrames
      codecSpecificInfo:(__nullable id<RTCCodecSpecificInfo>)info
           renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);
  return [self decodeData: (uint8_t *)inputImage.buffer.bytes size: inputImage.buffer.length timeStamp: inputImage.timeStamp];
}

- (NSInteger)decodeData:(const uint8_t *)data size:(size_t)size timeStamp:(uint32_t)timeStamp {
  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat =
      rtc::ScopedCF(webrtc::CreateH265VideoFormatDescription(
          (uint8_t*)data, size));
  if (inputFormat) {
    CMVideoDimensions dimensions =
        CMVideoFormatDescriptionGetDimensions(inputFormat.get());
    RTC_LOG(LS_INFO) << "Resolution: " << dimensions.width << " x "
                     << dimensions.height;
    // Check if the video format has changed, and reinitialize decoder if
    // needed.
    if (!CMFormatDescriptionEqual(inputFormat.get(), _videoFormat)) {
      [self setVideoFormat:inputFormat.get()];
      int resetDecompressionSessionError = [self resetDecompressionSession];
      if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
        return resetDecompressionSessionError;
      }
    }
  }
  if (!_videoFormat) {
    // We received a frame but we don't have format information so we can't
    // decode it.
    // This can happen after backgrounding. We need to wait for the next
    // sps/pps before we can resume so we request a keyframe by returning an
    // error.
    RTC_LOG(LS_WARNING) << "Missing video format. Frame with sps/pps required.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  CMSampleBufferRef sampleBuffer = nullptr;
  if (!webrtc::H265AnnexBBufferToCMSampleBuffer(
          (uint8_t*)data, size,
          _videoFormat, &sampleBuffer)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sampleBuffer);
  VTDecodeFrameFlags decodeFlags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<RTCH265FrameDecodeParams> frameDecodeParams;
  frameDecodeParams.reset(
      new RTCH265FrameDecodeParams(_callback, timeStamp));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer, decodeFlags,
      frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active and retry the decode request.
  if (status == kVTInvalidSessionErr &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    frameDecodeParams.reset(
        new RTCH265FrameDecodeParams(_callback, timeStamp));
    status = VTDecompressionSessionDecodeFrame(
        _decompressionSession, sampleBuffer, decodeFlags,
        frameDecodeParams.release(), nullptr);
  }
#endif
  CFRelease(sampleBuffer);
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

  // Need to wait for the first SPS to initialize decoder.
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
  CFNumberRef pixelFormat =
      CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue,
                                      pixelFormat};
  CFDictionaryRef attributes =
      CreateCFTypeDictionary(keys, values, attributesSize);
  if (ioSurfaceValue) {
    CFRelease(ioSurfaceValue);
    ioSurfaceValue = nullptr;
  }
  if (pixelFormat) {
    CFRelease(pixelFormat);
    pixelFormat = nullptr;
  }
  VTDecompressionOutputCallbackRecord record = {
      h265DecompressionOutputCallback,
      nullptr,
  };
  OSStatus status =
      VTDecompressionSessionCreate(nullptr, _videoFormat, nullptr, attributes,
                                   &record, &_decompressionSession);
  CFRelease(attributes);
  if (status != noErr) {
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
#if defined(WEBRTC_IOS)
    VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
#endif
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
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

- (NSString*)implementationName {
  return @"VideoToolbox";
}

@end
