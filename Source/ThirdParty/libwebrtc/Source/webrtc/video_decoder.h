/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_DECODER_H_
#define WEBRTC_VIDEO_DECODER_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"

namespace webrtc {

class RTPFragmentationHeader;
// TODO(pbos): Expose these through a public (root) header or change these APIs.
struct CodecSpecificInfo;
class VideoCodec;

class DecodedImageCallback {
 public:
  virtual ~DecodedImageCallback() {}

  virtual int32_t Decoded(VideoFrame& decodedImage) = 0;
  // Provides an alternative interface that allows the decoder to specify the
  // decode time excluding waiting time for any previous pending frame to
  // return. This is necessary for breaking positive feedback in the delay
  // estimation when the decoder has a single output buffer.
  // TODO(perkj): Remove default implementation when chromium has been updated.
  virtual int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms) {
    // The default implementation ignores custom decode time value.
    return Decoded(decodedImage);
  }

  virtual int32_t ReceivedDecodedReferenceFrame(const uint64_t pictureId) {
    return -1;
  }

  virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId) { return -1; }
};

class VideoDecoder {
 public:
  enum DecoderType {
    kH264,
    kVp8,
    kVp9,
    kUnsupportedCodec,
  };

  static VideoDecoder* Create(DecoderType codec_type);

  virtual ~VideoDecoder() {}

  virtual int32_t InitDecode(const VideoCodec* codec_settings,
                             int32_t number_of_cores) = 0;

  virtual int32_t Decode(const EncodedImage& input_image,
                         bool missing_frames,
                         const RTPFragmentationHeader* fragmentation,
                         const CodecSpecificInfo* codec_specific_info = NULL,
                         int64_t render_time_ms = -1) = 0;

  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) = 0;

  virtual int32_t Release() = 0;

  // Returns true if the decoder prefer to decode frames late.
  // That is, it can not decode infinite number of frames before the decoded
  // frame is consumed.
  virtual bool PrefersLateDecoding() const { return true; }

  virtual const char* ImplementationName() const { return "unknown"; }
};

// Class used to wrap external VideoDecoders to provide a fallback option on
// software decoding when a hardware decoder fails to decode a stream due to
// hardware restrictions, such as max resolution.
class VideoDecoderSoftwareFallbackWrapper : public webrtc::VideoDecoder {
 public:
  VideoDecoderSoftwareFallbackWrapper(VideoCodecType codec_type,
                                      VideoDecoder* decoder);

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;
  bool PrefersLateDecoding() const override;

  const char* ImplementationName() const override;

 private:
  bool InitFallbackDecoder();

  const DecoderType decoder_type_;
  VideoDecoder* const decoder_;

  VideoCodec codec_settings_;
  int32_t number_of_cores_;
  std::string fallback_implementation_name_;
  std::unique_ptr<VideoDecoder> fallback_decoder_;
  DecodedImageCallback* callback_;
};

// Video decoder class to be used for unknown codecs. Doesn't support decoding
// but logs messages to LS_ERROR.
class NullVideoDecoder : public VideoDecoder {
 public:
  NullVideoDecoder();

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;

  const char* ImplementationName() const override;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_DECODER_H_
