/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoDecoderH264.h"

#import <VideoToolbox/VideoToolbox.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"

#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCFrameDecodeParams {
  RTCFrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts) : callback(cb), timestamp(ts) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};

@interface RTCVideoDecoderH264 ()
- (void)setError:(OSStatus)error;
@end

static void overrideColorSpaceAttachments(CVImageBufferRef imageBuffer) {
  CVBufferRemoveAttachment(imageBuffer, kCVImageBufferCGColorSpaceKey);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferTransferFunctionKey, kCVImageBufferTransferFunction_sRGB, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferYCbCrMatrixKey, kCVImageBufferYCbCrMatrix_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, (CFStringRef)@"ColorInfoGuessedBy", (CFStringRef)@"RTCVideoDecoderH264", kCVAttachmentMode_ShouldPropagate);
}

// This is the callback function that VideoToolbox calls when decode is
// complete.
void decompressionOutputCallback(void *decoderRef,
                                 void *params,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime timestamp,
                                 CMTime duration) {
  std::unique_ptr<RTCFrameDecodeParams> decodeParams(reinterpret_cast<RTCFrameDecodeParams *>(params));
  if (status != noErr || !imageBuffer) {
    RTCVideoDecoderH264 *decoder = (__bridge RTCVideoDecoderH264 *)decoderRef;
    [decoder setError:status != noErr ? status : 1];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    decodeParams->callback(nil);
    return;
  }

  overrideColorSpaceAttachments(imageBuffer);

  // TODO(tkchin): Handle CVO properly.
  RTCCVPixelBuffer *frameBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:imageBuffer];
  RTCVideoFrame *decodedFrame =
      [[RTCVideoFrame alloc] initWithBuffer:frameBuffer
                                   rotation:RTCVideoRotation_0
                                timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
}

// Decoder.
@implementation RTCVideoDecoderH264 {
  CMVideoFormatDescriptionRef _videoFormat;
  CMMemoryPoolRef _memoryPool;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
  bool _useAVC;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _memoryPool = CMMemoryPoolCreate(nil);
    _useAVC = false;
  }
  return self;
}

- (void)dealloc {
  CMMemoryPoolInvalidate(_memoryPool);
  CFRelease(_memoryPool);
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

CMSampleBufferRef H264BufferToCMSampleBuffer(const uint8_t* buffer, size_t buffer_size, CMVideoFormatDescriptionRef video_format) {
  CMBlockBufferRef new_block_buffer;
  if (auto error = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, buffer_size, kCFAllocatorDefault, NULL, 0, buffer_size, kCMBlockBufferAssureMemoryNowFlag, &new_block_buffer)) {
    RTC_LOG(LS_ERROR) << "H264BufferToCMSampleBuffer CMBlockBufferCreateWithMemoryBlock failed with: " << error;
    return nullptr;
  }
  auto block_buffer = rtc::ScopedCF(new_block_buffer);

  if (auto error = CMBlockBufferReplaceDataBytes(buffer, block_buffer.get(), 0, buffer_size)) {
    RTC_LOG(LS_ERROR) << "H264BufferToCMSampleBuffer CMBlockBufferReplaceDataBytes failed with: " << error;
    return nullptr;
  }

  CMSampleBufferRef sample_buffer = nullptr;
  if (auto error = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer.get(), true, nullptr, nullptr, video_format, 1, 0, nullptr, 0, nullptr, &sample_buffer)) {
    RTC_LOG(LS_ERROR) << "H264BufferToCMSampleBuffer CMSampleBufferCreate failed with: " << error;
    return nullptr;
  }
  return sample_buffer;
}

- (NSInteger)decode:(RTCEncodedImage *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);

  return [self decodeData: (uint8_t *)inputImage.buffer.bytes size: inputImage.buffer.length timeStamp: inputImage.timeStamp];
}

- (NSInteger)decodeData:(const uint8_t *)data
        size:(size_t)size
        timeStamp:(int64_t)timeStamp {

  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (!data || !size) {
    RTC_LOG(LS_WARNING) << "Empty frame.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat =
      rtc::ScopedCF(webrtc::CreateVideoFormatDescription(data, size));
  if (inputFormat) {
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
  if (_useAVC) {
    sampleBuffer = H264BufferToCMSampleBuffer(data, size, _videoFormat);
    if (!sampleBuffer) {
      RTC_LOG(LS_ERROR) << "Cannot create sample from data.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  } else if (!webrtc::H264AnnexBBufferToCMSampleBuffer(data,
                                                size,
                                                _videoFormat,
                                                &sampleBuffer,
                                                _memoryPool)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sampleBuffer);
  VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<RTCFrameDecodeParams> frameDecodeParams;
  frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, timeStamp));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active or decoder malfunctions and retry the decode request.
  if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_INFO) << "Failed to decode frame with code: " << status
                     << " retrying decode after decompression session reset";
    frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, timeStamp));
    status = VTDecompressionSessionDecodeFrame(
        _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
  }
#endif
  CFRelease(sampleBuffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)setAVCFormat:(const uint8_t *)data size:(size_t)size width:(uint16_t)width height:(uint16_t)height {
  CFStringRef avcCString = (CFStringRef)@"avcC";
  CFDataRef codecConfig = CFDataCreate(kCFAllocatorDefault, data, size);
  CFDictionaryRef atomsDict = CFDictionaryCreate(NULL,
    (const void **)&avcCString,
    (const void **)&codecConfig,
    1,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks);
  CFDictionaryRef extensionsDict = CFDictionaryCreate(NULL,
    (const void **)&kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
    (const void **)&atomsDict,
    1,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks);

  CMVideoFormatDescriptionRef videoFormatDescription = nullptr;
  auto err = CMVideoFormatDescriptionCreate(NULL, kCMVideoCodecType_H264, width, height, extensionsDict, &videoFormatDescription);
  CFRelease(codecConfig);
  CFRelease(atomsDict);
  CFRelease(extensionsDict);

  if (err) {
      RTC_LOG(LS_ERROR) << "Cannot create fromat description.";
      return err;
  }

  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat = rtc::ScopedCF(videoFormatDescription);
  if (inputFormat) {
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
  _useAVC = true;
  return 0;
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
      decompressionOutputCallback, (__bridge void *)self,
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
