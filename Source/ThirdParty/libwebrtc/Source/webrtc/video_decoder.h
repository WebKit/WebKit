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
  virtual int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms) {
    // The default implementation ignores custom decode time value.
    return Decoded(decodedImage);
  }
  // TODO(sakal): Remove other implementations when upstream projects have been
  // updated.
  virtual void Decoded(VideoFrame& decodedImage,
                       rtc::Optional<int32_t> decode_time_ms,
                       rtc::Optional<uint8_t> qp) {
    Decoded(decodedImage,
            decode_time_ms ? static_cast<int32_t>(*decode_time_ms) : -1);
  }

  virtual int32_t ReceivedDecodedReferenceFrame(const uint64_t pictureId) {
    return -1;
  }

  virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId) { return -1; }
};

class VideoDecoder {
 public:
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

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_DECODER_H_
