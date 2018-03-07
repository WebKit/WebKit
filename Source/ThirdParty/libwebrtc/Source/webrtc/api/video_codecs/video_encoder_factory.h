/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

namespace webrtc {

class VideoEncoder;
struct SdpVideoFormat;

// A factory that creates VideoEncoders.
// NOTE: This class is still under development and may change without notice.
class VideoEncoderFactory {
 public:
  // TODO(magjed): Try to get rid of this struct.
  struct CodecInfo {
    // |is_hardware_accelerated| is true if the encoders created by this factory
    // of the given codec will use hardware support.
    bool is_hardware_accelerated;
    // |has_internal_source| is true if encoders created by this factory of the
    // given codec will use internal camera sources, meaning that they don't
    // require/expect frames to be delivered via webrtc::VideoEncoder::Encode.
    // This flag is used as the internal_source parameter to
    // webrtc::ViEExternalCodec::RegisterExternalSendCodec.
    bool has_internal_source;
  };

  // Returns a list of supported video formats in order of preference, to use
  // for signaling etc.
  virtual std::vector<SdpVideoFormat> GetSupportedFormats() const = 0;

  // Returns information about how this format will be encoded. The specified
  // format must be one of the supported formats by this factory.
  // TODO(magjed): Try to get rid of this method.
  virtual CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const = 0;

  // Creates a VideoEncoder for the specified format.
  virtual std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) = 0;

  virtual ~VideoEncoderFactory() {}
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_H_
