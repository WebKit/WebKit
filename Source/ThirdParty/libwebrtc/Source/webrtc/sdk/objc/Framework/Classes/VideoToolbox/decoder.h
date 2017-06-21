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

#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOX_DECODER_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOX_DECODER_H_

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"

#include <VideoToolbox/VideoToolbox.h>

// This file provides a H264 encoder implementation using the VideoToolbox
// APIs. Since documentation is almost non-existent, this is largely based on
// the information in the VideoToolbox header files, a talk from WWDC 2014 and
// experimentation.

namespace webrtc {

class H264VideoToolboxDecoder : public H264Decoder {
 public:
  H264VideoToolboxDecoder();

  ~H264VideoToolboxDecoder() override;

  int InitDecode(const VideoCodec* video_codec, int number_of_cores) override;

  int Decode(const EncodedImage& input_image,
             bool missing_frames,
             const RTPFragmentationHeader* fragmentation,
             const CodecSpecificInfo* codec_specific_info,
             int64_t render_time_ms) override;

  int RegisterDecodeCompleteCallback(DecodedImageCallback* callback) override;

  int Release() override;

  const char* ImplementationName() const override;

 private:
  int ResetDecompressionSession();
  void ConfigureDecompressionSession();
  void DestroyDecompressionSession();
  void SetVideoFormat(CMVideoFormatDescriptionRef video_format);

  DecodedImageCallback* callback_;
  CMVideoFormatDescriptionRef video_format_;
  VTDecompressionSessionRef decompression_session_;
};  // H264VideoToolboxDecoder

}  // namespace webrtc

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOX_DECODER_H_
