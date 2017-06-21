/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_SEND_STREAM_H_
#define WEBRTC_VIDEO_SEND_STREAM_H_

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <utility>

#include "webrtc/api/call/transport.h"
#include "webrtc/base/platform_file.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/frame_callback.h"
#include "webrtc/config.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videosourceinterface.h"

namespace webrtc {

class VideoEncoder;

class VideoSendStream {
 public:
  struct StreamStats {
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
    std::string ToString(int64_t time_ms) const;
    std::string encoder_implementation_name = "unknown";
    int input_frame_rate = 0;
    int encode_frame_rate = 0;
    int avg_encode_time_ms = 0;
    int encode_usage_percent = 0;
    uint32_t frames_encoded = 0;
    rtc::Optional<uint64_t> qp_sum;
    // Bitrate the encoder is currently configured to use due to bandwidth
    // limitations.
    int target_media_bitrate_bps = 0;
    // Bitrate the encoder is actually producing.
    int media_bitrate_bps = 0;
    // Media bitrate this VideoSendStream is configured to prefer if there are
    // no bandwidth limitations.
    int preferred_media_bitrate_bps = 0;
    bool suspended = false;
    bool bw_limited_resolution = false;
    bool cpu_limited_resolution = false;
    bool bw_limited_framerate = false;
    bool cpu_limited_framerate = false;
    // Total number of times resolution as been requested to be changed due to
    // CPU/quality adaptation.
    int number_of_cpu_adapt_changes = 0;
    int number_of_quality_adapt_changes = 0;
    std::map<uint32_t, StreamStats> substreams;
  };

  struct Config {
   public:
    Config() = delete;
    Config(Config&&) = default;
    explicit Config(Transport* send_transport)
        : send_transport(send_transport) {}

    Config& operator=(Config&&) = default;
    Config& operator=(const Config&) = delete;

    // Mostly used by tests.  Avoid creating copies if you can.
    Config Copy() const { return Config(*this); }

    std::string ToString() const;

    struct EncoderSettings {
      EncoderSettings() = default;
      EncoderSettings(std::string payload_name,
                      int payload_type,
                      VideoEncoder* encoder)
          : payload_name(std::move(payload_name)),
            payload_type(payload_type),
            encoder(encoder) {}
      std::string ToString() const;

      std::string payload_name;
      int payload_type = -1;

      // TODO(sophiechang): Delete this field when no one is using internal
      // sources anymore.
      bool internal_source = false;

      // Allow 100% encoder utilization. Used for HW encoders where CPU isn't
      // expected to be the limiting factor, but a chip could be running at
      // 30fps (for example) exactly.
      bool full_overuse_time = false;

      // Uninitialized VideoEncoder instance to be used for encoding. Will be
      // initialized from inside the VideoSendStream.
      VideoEncoder* encoder = nullptr;
    } encoder_settings;

    static const size_t kDefaultMaxPacketSize = 1500 - 40;  // TCP over IPv4.
    struct Rtp {
      std::string ToString() const;

      std::vector<uint32_t> ssrcs;

      // See RtcpMode for description.
      RtcpMode rtcp_mode = RtcpMode::kCompound;

      // Max RTP packet size delivered to send transport from VideoEngine.
      size_t max_packet_size = kDefaultMaxPacketSize;

      // RTP header extensions to use for this send stream.
      std::vector<RtpExtension> extensions;

      // See NackConfig for description.
      NackConfig nack;

      // See UlpfecConfig for description.
      UlpfecConfig ulpfec;

      struct Flexfec {
        // Payload type of FlexFEC. Set to -1 to disable sending FlexFEC.
        int payload_type = -1;

        // SSRC of FlexFEC stream.
        uint32_t ssrc = 0;

        // Vector containing a single element, corresponding to the SSRC of the
        // media stream being protected by this FlexFEC stream.
        // The vector MUST have size 1.
        //
        // TODO(brandtr): Update comment above when we support
        // multistream protection.
        std::vector<uint32_t> protected_media_ssrcs;
      } flexfec;

      // Settings for RTP retransmission payload format, see RFC 4588 for
      // details.
      struct Rtx {
        std::string ToString() const;
        // SSRCs to use for the RTX streams.
        std::vector<uint32_t> ssrcs;

        // Payload type to use for the RTX stream.
        int payload_type = -1;
      } rtx;

      // RTCP CNAME, see RFC 3550.
      std::string c_name;
    } rtp;

    // Transport for outgoing packets.
    Transport* send_transport = nullptr;

    // Called for each I420 frame before encoding the frame. Can be used for
    // effects, snapshots etc. 'nullptr' disables the callback.
    rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback = nullptr;

    // Called for each encoded frame, e.g. used for file storage. 'nullptr'
    // disables the callback. Also measures timing and passes the time
    // spent on encoding. This timing will not fire if encoding takes longer
    // than the measuring window, since the sample data will have been dropped.
    EncodedFrameObserver* post_encode_callback = nullptr;

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

   private:
    // Access to the copy constructor is private to force use of the Copy()
    // method for those exceptional cases where we do use it.
    Config(const Config&) = default;
  };

  // Starts stream activity.
  // When a stream is active, it can receive, process and deliver packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive, process or deliver packets.
  virtual void Stop() = 0;

  // Based on the spec in
  // https://w3c.github.io/webrtc-pc/#idl-def-rtcdegradationpreference.
  // These options are enforced on a best-effort basis. For instance, all of
  // these options may suffer some frame drops in order to avoid queuing.
  // TODO(sprang): Look into possibility of more strictly enforcing the
  // maintain-framerate option.
  enum class DegradationPreference {
    // Don't take any actions based on over-utilization signals.
    kDegradationDisabled,
    // On over-use, request lower frame rate, possibly causing frame drops.
    kMaintainResolution,
    // On over-use, request lower resolution, possibly causing down-scaling.
    kMaintainFramerate,
    // Try to strike a "pleasing" balance between frame rate or resolution.
    kBalanced,
  };

  virtual void SetSource(
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
      const DegradationPreference& degradation_preference) = 0;

  // Set which streams to send. Must have at least as many SSRCs as configured
  // in the config. Encoder settings are passed on to the encoder instance along
  // with the VideoStream settings.
  virtual void ReconfigureVideoEncoder(VideoEncoderConfig config) = 0;

  virtual Stats GetStats() = 0;

  // Takes ownership of each file, is responsible for closing them later.
  // Calling this method will close and finalize any current logs.
  // Some codecs produce multiple streams (VP8 only at present), each of these
  // streams will log to a separate file. kMaxSimulcastStreams in common_types.h
  // gives the max number of such streams. If there is no file for a stream, or
  // the file is rtc::kInvalidPlatformFileValue, frames from that stream will
  // not be logged.
  // If a frame to be written would make the log too large the write fails and
  // the log is closed and finalized. A |byte_limit| of 0 means no limit.
  virtual void EnableEncodedFrameRecording(
      const std::vector<rtc::PlatformFile>& files,
      size_t byte_limit) = 0;
  inline void DisableEncodedFrameRecording() {
    EnableEncodedFrameRecording(std::vector<rtc::PlatformFile>(), 0);
  }

 protected:
  virtual ~VideoSendStream() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_SEND_STREAM_H_
