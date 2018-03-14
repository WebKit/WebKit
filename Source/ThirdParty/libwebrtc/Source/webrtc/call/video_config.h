/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_VIDEO_CONFIG_H_
#define CALL_VIDEO_CONFIG_H_

#include <string>
#include <vector>

#include "api/optional.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/basictypes.h"
#include "rtc_base/refcount.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

struct VideoStream {
  VideoStream();
  ~VideoStream();
  VideoStream(const VideoStream& other);
  std::string ToString() const;

  size_t width;
  size_t height;
  int max_framerate;

  int min_bitrate_bps;
  int target_bitrate_bps;
  int max_bitrate_bps;
  rtc::Optional<double> bitrate_priority;

  int max_qp;

  // TODO(bugs.webrtc.org/8653): Support active per-simulcast layer.
  bool active;

  // Bitrate thresholds for enabling additional temporal layers. Since these are
  // thresholds in between layers, we have one additional layer. One threshold
  // gives two temporal layers, one below the threshold and one above, two give
  // three, and so on.
  // The VideoEncoder may redistribute bitrates over the temporal layers so a
  // bitrate threshold of 100k and an estimate of 105k does not imply that we
  // get 100k in one temporal layer and 5k in the other, just that the bitrate
  // in the first temporal layer should not exceed 100k.
  // TODO(kthelgason): Apart from a special case for two-layer screencast these
  // thresholds are not propagated to the VideoEncoder. To be implemented.
  std::vector<int> temporal_layer_thresholds_bps;
};

class VideoEncoderConfig {
 public:
  // These are reference counted to permit copying VideoEncoderConfig and be
  // kept alive until all encoder_specific_settings go out of scope.
  // TODO(kthelgason): Consider removing the need for copying VideoEncoderConfig
  // and use rtc::Optional for encoder_specific_settings instead.
  class EncoderSpecificSettings : public rtc::RefCountInterface {
   public:
    // TODO(pbos): Remove FillEncoderSpecificSettings as soon as VideoCodec is
    // not in use and encoder implementations ask for codec-specific structs
    // directly.
    void FillEncoderSpecificSettings(VideoCodec* codec_struct) const;

    virtual void FillVideoCodecVp8(VideoCodecVP8* vp8_settings) const;
    virtual void FillVideoCodecVp9(VideoCodecVP9* vp9_settings) const;
    virtual void FillVideoCodecH264(VideoCodecH264* h264_settings) const;

   private:
    ~EncoderSpecificSettings() override {}
    friend class VideoEncoderConfig;
  };

  class H264EncoderSpecificSettings : public EncoderSpecificSettings {
   public:
    explicit H264EncoderSpecificSettings(const VideoCodecH264& specifics);
    void FillVideoCodecH264(VideoCodecH264* h264_settings) const override;

   private:
    VideoCodecH264 specifics_;
  };

  class Vp8EncoderSpecificSettings : public EncoderSpecificSettings {
   public:
    explicit Vp8EncoderSpecificSettings(const VideoCodecVP8& specifics);
    void FillVideoCodecVp8(VideoCodecVP8* vp8_settings) const override;

   private:
    VideoCodecVP8 specifics_;
  };

  class Vp9EncoderSpecificSettings : public EncoderSpecificSettings {
   public:
    explicit Vp9EncoderSpecificSettings(const VideoCodecVP9& specifics);
    void FillVideoCodecVp9(VideoCodecVP9* vp9_settings) const override;

   private:
    VideoCodecVP9 specifics_;
  };

  enum class ContentType {
    kRealtimeVideo,
    kScreen,
  };

  class VideoStreamFactoryInterface : public rtc::RefCountInterface {
   public:
    // An implementation should return a std::vector<VideoStream> with the
    // wanted VideoStream settings for the given video resolution.
    // The size of the vector may not be larger than
    // |encoder_config.number_of_streams|.
    virtual std::vector<VideoStream> CreateEncoderStreams(
        int width,
        int height,
        const VideoEncoderConfig& encoder_config) = 0;

   protected:
    ~VideoStreamFactoryInterface() override {}
  };

  VideoEncoderConfig& operator=(VideoEncoderConfig&&) = default;
  VideoEncoderConfig& operator=(const VideoEncoderConfig&) = delete;

  // Mostly used by tests.  Avoid creating copies if you can.
  VideoEncoderConfig Copy() const { return VideoEncoderConfig(*this); }

  VideoEncoderConfig();
  VideoEncoderConfig(VideoEncoderConfig&&);
  ~VideoEncoderConfig();
  std::string ToString() const;

  rtc::scoped_refptr<VideoStreamFactoryInterface> video_stream_factory;
  std::vector<SpatialLayer> spatial_layers;
  ContentType content_type;
  rtc::scoped_refptr<const EncoderSpecificSettings> encoder_specific_settings;

  // Padding will be used up to this bitrate regardless of the bitrate produced
  // by the encoder. Padding above what's actually produced by the encoder helps
  // maintaining a higher bitrate estimate. Padding will however not be sent
  // unless the estimated bandwidth indicates that the link can handle it.
  int min_transmit_bitrate_bps;
  int max_bitrate_bps;
  // The bitrate priority used for all VideoStreams.
  double bitrate_priority;

  // The simulcast layer's configurations set by the application for this video
  // sender. These are modified by the video_stream_factory before being passed
  // down to lower layers for the video encoding.
  std::vector<VideoStream> simulcast_layers;

  // Max number of encoded VideoStreams to produce.
  size_t number_of_streams;

 private:
  // Access to the copy constructor is private to force use of the Copy()
  // method for those exceptional cases where we do use it.
  VideoEncoderConfig(const VideoEncoderConfig&);
};

}  // namespace webrtc

#endif  // CALL_VIDEO_CONFIG_H_
