/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_VIDEO_SEND_STREAM_H_
#define CALL_VIDEO_SEND_STREAM_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/call/transport.h"
#include "api/crypto/cryptooptions.h"
#include "api/rtpparameters.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/rtp_config.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class FrameEncryptorInterface;

class VideoSendStream {
 public:
  struct StreamStats {
    StreamStats();
    ~StreamStats();

    std::string ToString() const;

    FrameCounts frame_counts;
    bool is_rtx = false;
    bool is_flexfec = false;
    int width = 0;
    int height = 0;
    // TODO(holmer): Move bitrate_bps out to the webrtc::Call layer.
    int total_bitrate_bps = 0;
    int retransmit_bitrate_bps = 0;
    int avg_delay_ms = 0;
    int max_delay_ms = 0;
    StreamDataCounters rtp_stats;
    RtcpPacketTypeCounter rtcp_packet_type_counts;
    RtcpStatistics rtcp_stats;
  };

  struct Stats {
    Stats();
    ~Stats();
    std::string ToString(int64_t time_ms) const;
    std::string encoder_implementation_name = "unknown";
    int input_frame_rate = 0;
    int encode_frame_rate = 0;
    int avg_encode_time_ms = 0;
    int encode_usage_percent = 0;
    uint32_t frames_encoded = 0;
    uint32_t frames_dropped_by_capturer = 0;
    uint32_t frames_dropped_by_encoder_queue = 0;
    uint32_t frames_dropped_by_rate_limiter = 0;
    uint32_t frames_dropped_by_encoder = 0;
    absl::optional<uint64_t> qp_sum;
    // Bitrate the encoder is currently configured to use due to bandwidth
    // limitations.
    int target_media_bitrate_bps = 0;
    // Bitrate the encoder is actually producing.
    int media_bitrate_bps = 0;
    bool suspended = false;
    bool bw_limited_resolution = false;
    bool cpu_limited_resolution = false;
    bool bw_limited_framerate = false;
    bool cpu_limited_framerate = false;
    // Total number of times resolution as been requested to be changed due to
    // CPU/quality adaptation.
    int number_of_cpu_adapt_changes = 0;
    int number_of_quality_adapt_changes = 0;
    bool has_entered_low_resolution = false;
    std::map<uint32_t, StreamStats> substreams;
    webrtc::VideoContentType content_type =
        webrtc::VideoContentType::UNSPECIFIED;
    uint32_t huge_frames_sent = 0;
  };

  struct Config {
   public:
    Config() = delete;
    Config(Config&&);
    explicit Config(Transport* send_transport);

    Config& operator=(Config&&);
    Config& operator=(const Config&) = delete;

    ~Config();

    // Mostly used by tests.  Avoid creating copies if you can.
    Config Copy() const { return Config(*this); }

    std::string ToString() const;

    VideoStreamEncoderSettings encoder_settings;

    RtpConfig rtp;

    // Time interval between RTCP report for video
    int rtcp_report_interval_ms = 1000;

    // Transport for outgoing packets.
    Transport* send_transport = nullptr;

    // Called for each I420 frame before encoding the frame. Can be used for
    // effects, snapshots etc. 'nullptr' disables the callback.
    rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback = nullptr;

    // Expected delay needed by the renderer, i.e. the frame will be delivered
    // this many milliseconds, if possible, earlier than expected render time.
    // Only valid if |local_renderer| is set.
    int render_delay_ms = 0;

    // Target delay in milliseconds. A positive value indicates this stream is
    // used for streaming instead of a real-time call.
    int target_delay_ms = 0;

    // True if the stream should be suspended when the available bitrate fall
    // below the minimum configured bitrate. If this variable is false, the
    // stream may send at a rate higher than the estimated available bitrate.
    bool suspend_below_min_bitrate = false;

    // Enables periodic bandwidth probing in application-limited region.
    bool periodic_alr_bandwidth_probing = false;

    // Track ID as specified during track creation.
    std::string track_id;

    // An optional custom frame encryptor that allows the entire frame to be
    // encrypted in whatever way the caller chooses. This is not required by
    // default.
    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor;

    // Per PeerConnection cryptography options.
    CryptoOptions crypto_options;

   private:
    // Access to the copy constructor is private to force use of the Copy()
    // method for those exceptional cases where we do use it.
    Config(const Config&);
  };

  // Updates the sending state for all simulcast layers that the video send
  // stream owns. This can mean updating the activity one or for multiple
  // layers. The ordering of active layers is the order in which the
  // rtp modules are stored in the VideoSendStream.
  // Note: This starts stream activity if it is inactive and one of the layers
  // is active. This stops stream activity if it is active and all layers are
  // inactive.
  virtual void UpdateActiveSimulcastLayers(
      const std::vector<bool> active_layers) = 0;

  // Starts stream activity.
  // When a stream is active, it can receive, process and deliver packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive, process or deliver packets.
  virtual void Stop() = 0;

  virtual void SetSource(
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
      const DegradationPreference& degradation_preference) = 0;

  // Set which streams to send. Must have at least as many SSRCs as configured
  // in the config. Encoder settings are passed on to the encoder instance along
  // with the VideoStream settings.
  virtual void ReconfigureVideoEncoder(VideoEncoderConfig config) = 0;

  virtual Stats GetStats() = 0;

 protected:
  virtual ~VideoSendStream() {}
};

}  // namespace webrtc

#endif  // CALL_VIDEO_SEND_STREAM_H_
