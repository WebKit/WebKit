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

#import "RTCVideoFrameReorderQueue.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "common_video/h265/h265_common.h"
#import "common_video/h265/h265_vps_parser.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"
#import "modules/video_coding/include/video_error_codes.h"
#import "nalu_rewriter.h"
#import "rtc_base/bitstream_reader.h"
#import "rtc_base/checks.h"
#import "rtc_base/logging.h"
#import "rtc_base/time_utils.h"
#import <span>

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCH265FrameDecodeParams {
  RTCH265FrameDecodeParams(int64_t ts, uint64_t reorderSize)
      : timestamp(ts), reorderSize(reorderSize) {}
  int64_t timestamp;
  uint64_t reorderSize { 0 };
};

@interface RTCVideoDecoderH265 ()
- (void)setError:(OSStatus)error;
- (void)processFrame:(RTCVideoFrame*)decodedFrame reorderSize:(uint64_t)reorderSize;
@end

static void overrideColorSpaceAttachments(CVImageBufferRef imageBuffer) {
  CVBufferRemoveAttachment(imageBuffer, kCVImageBufferCGColorSpaceKey);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferTransferFunctionKey, kCVImageBufferTransferFunction_sRGB, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, kCVImageBufferYCbCrMatrixKey, kCVImageBufferYCbCrMatrix_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(imageBuffer, (CFStringRef)@"ColorInfoGuessedBy", (CFStringRef)@"RTCVideoDecoderH265", kCVAttachmentMode_ShouldPropagate);
}

std::span<const uint8_t> vpsDataFromHvcc(const uint8_t* hvccData, size_t hvccDataSize) {
  std::vector<uint8_t> unpacked_buffer { hvccData, hvccData + hvccDataSize };
  webrtc::BitstreamReader reader(unpacked_buffer);

  // configuration_version
  auto version = reader.Read<uint8_t>();
  if (version > 1) {
    reader.Ok();
    return { };
  }
  // profile_indication
  reader.ConsumeBits(8);
  // general_profile_compatibility_flags
  reader.ConsumeBits(32);
  // general_constraint_indicator_flags_hi;
  reader.ConsumeBits(32);
  // general_constraint_indicator_flags_lo;
  reader.ConsumeBits(16);
  // general_level_idc;
  reader.ConsumeBits(8);
  // min_spatial_segmentation_idc
  reader.ConsumeBits(16);
  // parallelismType;
  reader.ConsumeBits(8);
  // chromaFormat;
  reader.ConsumeBits(8);
  // bitDepthLumaMinus8
  reader.ConsumeBits(8);
  // bitDepthChromaMinus8
  reader.ConsumeBits(8);
  // avgFrameRate
  reader.ConsumeBits(16);
  //misc
  reader.ConsumeBits(8);
  auto numOfArrays = reader.Read<uint8_t>();

  if (!reader.Ok()) {
    return { };
  }

  size_t position = (8 + 8 + 32 + 32 + 16 + 8 + 16 + 8 + 8 + 8 + 8 + 16 + 8 + 8) / 8;
  for (uint32_t j = 0; j < numOfArrays; j++) {
    // NAL_unit_type: bit(1) array_completeness; unsigned int(1) reserved = 0; unsigned int(6) NAL_unit_type;
    auto nalUnitType = reader.Read<uint8_t>() & 0x3F;
    // numNalus
    auto numOfNalus = reader.Read<uint16_t>();
    position += 3;
    if (!reader.Ok()) {
        return { };
    }

    for (uint32_t k = 0; k < numOfNalus; k++) {
        // nalUnitLength
        auto size = reader.Read<uint16_t>();

        position += 2;
        // nalUnit
        reader.ConsumeBits(8 * size);

        static const size_t hevcNalHeaderSize = 2;
        if (!reader.Ok() || size <= hevcNalHeaderSize) {
            return { };
        }

        if (nalUnitType != webrtc::H265::NaluType::kVps) {
            position += size;
            continue;
        }

        return { hvccData + position + hevcNalHeaderSize, size - hevcNalHeaderSize };
    }
  }
  reader.Ok();
  return { };
}

uint8_t ComputeH265ReorderSizeFromVPS(const uint8_t* spsData, size_t spsDataSize)
{
    auto parsedVps = webrtc::H265VpsParser::ParseVps(spsData, spsDataSize);
    if (!parsedVps)
        return 0;

    auto reorderSize = *std::max_element(parsedVps->vps_max_num_reorder_pics, parsedVps->vps_max_num_reorder_pics + parsedVps->vps_max_sub_layers_minus1 + 1);
    // We use a max value of 16
    return std::min(reorderSize, 16u);
}

uint8_t ComputeH265ReorderSizeFromHVCC(const uint8_t* hvccData, size_t hvccDataSize)
{
    // FIXME: we should probably get the VPS from the SPS sps_video_parameter_set_id.
    auto vpsData = vpsDataFromHvcc(hvccData, hvccDataSize);
    if (!vpsData.size())
        return 0;

    return ComputeH265ReorderSizeFromVPS(vpsData.data(), vpsData.size());
}

uint8_t ComputeH265ReorderSizeFromAnnexB(const uint8_t* annexb_buffer, size_t annexb_buffer_size)
{
    // FIXME: we should probably get the VPS from the SPS sps_video_parameter_set_id.
    webrtc::AnnexBBufferReader bufferReader(annexb_buffer, annexb_buffer_size);
    if (!bufferReader.SeekToNextNaluOfType(webrtc::H265::kVps))
        return 0;

    static const size_t hevcNalHeaderSize = 2;
    const uint8_t* data;
    size_t data_len;
    if (!bufferReader.ReadNalu(&data, &data_len) || data_len <= hevcNalHeaderSize)
        return 0;

    return ComputeH265ReorderSizeFromVPS(data + hevcNalHeaderSize, data_len - hevcNalHeaderSize);
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
  std::unique_ptr<RTCH265FrameDecodeParams> decodeParams(reinterpret_cast<RTCH265FrameDecodeParams*>(params));
    RTCVideoDecoderH265 *decoder = (__bridge RTCVideoDecoderH265 *)decoderRef;
  if (status != noErr || !imageBuffer) {
    [decoder setError:status != noErr ? status : 1];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    [decoder processFrame:nil reorderSize:decodeParams->reorderSize];
    return;
  }

  overrideColorSpaceAttachments(imageBuffer);

  // TODO(tkchin): Handle CVO properly.
  RTCCVPixelBuffer* frameBuffer =
      [[RTCCVPixelBuffer alloc] initWithPixelBuffer:imageBuffer];
  RTCVideoFrame* decodedFrame = [[RTCVideoFrame alloc]
      initWithBuffer:frameBuffer
            rotation:RTCVideoRotation_0
         timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  [decoder processFrame:decodedFrame reorderSize:decodeParams->reorderSize];
}

// Decoder.
@implementation RTCVideoDecoderH265 {
  CMVideoFormatDescriptionRef _videoFormat;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
  bool _useHEVC;
  webrtc::RTCVideoFrameReorderQueue _reorderQueue;
}

- (instancetype)init {
  if (self = [super init]) {
    _useHEVC = false;
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

CMSampleBufferRef H265BufferToCMSampleBuffer(const uint8_t* buffer, size_t buffer_size, CMVideoFormatDescriptionRef video_format) CF_RETURNS_RETAINED {
  CMBlockBufferRef new_block_buffer;
  if (auto error = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, buffer_size, kCFAllocatorDefault, NULL, 0, buffer_size, kCMBlockBufferAssureMemoryNowFlag, &new_block_buffer)) {
    RTC_LOG(LS_ERROR) << "H265BufferToCMSampleBuffer CMBlockBufferCreateWithMemoryBlock failed with: " << error;
    return nullptr;
  }
  auto block_buffer = rtc::ScopedCF(new_block_buffer);

  if (auto error = CMBlockBufferReplaceDataBytes(buffer, block_buffer.get(), 0, buffer_size)) {
    RTC_LOG(LS_ERROR) << "H265BufferToCMSampleBuffer CMBlockBufferReplaceDataBytes failed with: " << error;
    return nullptr;
  }

  CMSampleBufferRef sample_buffer = nullptr;
  if (auto error = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer.get(), true, nullptr, nullptr, video_format, 1, 0, nullptr, 0, nullptr, &sample_buffer)) {
    RTC_LOG(LS_ERROR) << "H265BufferToCMSampleBuffer CMSampleBufferCreate failed with: " << error;
    return nullptr;
  }
  return sample_buffer;
}

- (NSInteger)decode:(RTCEncodedImage*)inputImage
          missingFrames:(BOOL)missingFrames
      codecSpecificInfo:(__nullable id<RTCCodecSpecificInfo>)info
           renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);
  return [self decodeData: (uint8_t *)inputImage.buffer.bytes size: inputImage.buffer.length timeStamp: inputImage.timeStamp];
}

- (NSInteger)decodeData:(const uint8_t *)data size:(size_t)size timeStamp:(int64_t)timeStamp {
  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (!data || !size) {
    RTC_LOG(LS_WARNING) << "Empty frame.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (!_useHEVC) {
    rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat = rtc::ScopedCF(webrtc::CreateH265VideoFormatDescription((uint8_t*)data, size));
    if (inputFormat) {
      _reorderQueue.setReorderSize(ComputeH265ReorderSizeFromAnnexB(data, size));

      CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(inputFormat.get());
      RTC_LOG(LS_INFO) << "Resolution: " << dimensions.width << " x " << dimensions.height;
      // Check if the video format has changed, and reinitialize decoder if needed.
      if (!CMFormatDescriptionEqual(inputFormat.get(), _videoFormat)) {
        [self setVideoFormat:inputFormat.get()];
        int resetDecompressionSessionError = [self resetDecompressionSession];
        if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
          return resetDecompressionSessionError;
        }
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
  if (_useHEVC) {
    sampleBuffer = H265BufferToCMSampleBuffer(data, size, _videoFormat);
    if (!sampleBuffer)
      return WEBRTC_VIDEO_CODEC_ERROR;
  } else if (!webrtc::H265AnnexBBufferToCMSampleBuffer(
          (uint8_t*)data, size,
          _videoFormat, &sampleBuffer)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sampleBuffer);
  VTDecodeFrameFlags decodeFlags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<RTCH265FrameDecodeParams> frameDecodeParams;
  frameDecodeParams.reset(
      new RTCH265FrameDecodeParams(timeStamp, _reorderQueue.reorderSize()));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer, decodeFlags,
      frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active and retry the decode request.
  if (status == kVTInvalidSessionErr &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    frameDecodeParams.reset(
        new RTCH265FrameDecodeParams(timeStamp, _reorderQueue.reorderSize()));
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

- (NSInteger)setHVCCFormat:(const uint8_t *)data size:(size_t)size width:(uint16_t)width height:(uint16_t)height {
  CFStringRef avcCString = (CFStringRef)@"hvcC";
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
  auto err = CMVideoFormatDescriptionCreate(NULL, kCMVideoCodecType_HEVC, width, height, extensionsDict, &videoFormatDescription);
  CFRelease(codecConfig);
  CFRelease(atomsDict);
  CFRelease(extensionsDict);

  if (err) {
      RTC_LOG(LS_ERROR) << "Cannot create fromat description.";
      return err;
  }

  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat = rtc::ScopedCF(videoFormatDescription);
  if (inputFormat) {
    _reorderQueue.setReorderSize(ComputeH265ReorderSizeFromHVCC(data, size));

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
  _useHEVC = true;
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
      (__bridge void *)self,
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

- (void)flush {
  if (_decompressionSession)
    VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);

  while (auto *frame = _reorderQueue.takeIfAny()) {
    _callback(frame);
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

- (void)processFrame:(RTCVideoFrame*)decodedFrame reorderSize:(uint64_t)reorderSize {
  // FIXME: In case of IDR, we could push out all queued frames.
  if (!_reorderQueue.isEmpty() || reorderSize) {
    _reorderQueue.append(decodedFrame, reorderSize);
    while (auto *frame = _reorderQueue.takeIfAvailable()) {
      _callback(frame);
    }
    return;
  }
  _callback(decodedFrame);
}

@end
