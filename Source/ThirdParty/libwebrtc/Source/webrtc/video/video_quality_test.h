/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_VIDEO_QUALITY_TEST_H_
#define WEBRTC_VIDEO_VIDEO_QUALITY_TEST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/modules/video_coding/codecs/vp8/simulcast_encoder_adapter.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"

namespace webrtc {

class VideoQualityTest : public test::CallTest {
 public:
  // Parameters are grouped into smaller structs to make it easier to set
  // the desired elements and skip unused, using aggregate initialization.
  // Unfortunately, C++11 (as opposed to C11) doesn't support unnamed structs,
  // which makes the implementation of VideoQualityTest a bit uglier.
  struct Params {
    Params();
    ~Params();
    struct CallConfig {
      bool send_side_bwe;
      Call::Config::BitrateConfig call_bitrate_config;
    } call;
    struct Video {
      bool enabled;
      size_t width;
      size_t height;
      int32_t fps;
      int min_bitrate_bps;
      int target_bitrate_bps;
      int max_bitrate_bps;
      bool suspend_below_min_bitrate;
      std::string codec;
      int num_temporal_layers;
      int selected_tl;
      int min_transmit_bps;
      bool ulpfec;
      bool flexfec;
      std::string encoded_frame_base_path;
      std::string clip_name;  // "Generator" to generate frames instead.
      size_t capture_device_index;
    } video;
    struct Audio {
      bool enabled;
      bool sync_video;
      bool dtx;
    } audio;
    struct Screenshare {
      bool enabled;
      int32_t slide_change_interval;
      int32_t scroll_duration;
      std::vector<std::string> slides;
    } screenshare;
    struct Analyzer {
      std::string test_label;
      double avg_psnr_threshold;  // (*)
      double avg_ssim_threshold;  // (*)
      int test_durations_secs;
      std::string graph_data_output_filename;
      std::string graph_title;
    } analyzer;
    FakeNetworkPipe::Config pipe;
    bool logs;
    struct SS {                          // Spatial scalability.
      std::vector<VideoStream> streams;  // If empty, one stream is assumed.
      size_t selected_stream;
      int num_spatial_layers;
      int selected_sl;
      // If empty, bitrates are generated in VP9Impl automatically.
      std::vector<SpatialLayer> spatial_layers;
      // If set, default parameters will be used instead of |streams|.
      bool infer_streams;
    } ss;
    int num_thumbnails;
  };

  VideoQualityTest();
  void RunWithAnalyzer(const Params& params);
  void RunWithRenderers(const Params& params);

  static void FillScalabilitySettings(
      Params* params,
      const std::vector<std::string>& stream_descriptors,
      size_t selected_stream,
      int num_spatial_layers,
      int selected_sl,
      const std::vector<std::string>& sl_descriptors);

 protected:
  std::map<uint8_t, webrtc::MediaType> payload_type_map_;

  // No-op implementation to be able to instantiate this class from non-TEST_F
  // locations.
  void TestBody() override;

  // Helper methods accessing only params_.
  std::string GenerateGraphTitle() const;
  void CheckParams();

  // Helper static methods.
  static VideoStream DefaultVideoStream(const Params& params);
  static VideoStream DefaultThumbnailStream();
  static std::vector<int> ParseCSV(const std::string& str);

  // Helper methods for setting up the call.
  void CreateCapturer();
  void SetupThumbnailCapturers(size_t num_thumbnail_streams);
  void SetupVideo(Transport* send_transport, Transport* recv_transport);
  void SetupThumbnails(Transport* send_transport, Transport* recv_transport);
  void DestroyThumbnailStreams();
  void SetupScreenshareOrSVC();
  void SetupAudio(int send_channel_id,
                  int receive_channel_id,
                  Call* call,
                  Transport* transport,
                  AudioReceiveStream** audio_receive_stream);

  void StartEncodedFrameLogs(VideoSendStream* stream);
  void StartEncodedFrameLogs(VideoReceiveStream* stream);

  // We need a more general capturer than the FrameGeneratorCapturer.
  std::unique_ptr<test::VideoCapturer> video_capturer_;
  std::vector<std::unique_ptr<test::VideoCapturer>> thumbnail_capturers_;
  std::unique_ptr<test::TraceToStderr> trace_to_stderr_;
  std::unique_ptr<test::FrameGenerator> frame_generator_;
  std::unique_ptr<VideoEncoder> video_encoder_;
  std::unique_ptr<VideoEncoderFactory> vp8_encoder_factory_;

  std::vector<std::unique_ptr<VideoEncoder>> thumbnail_encoders_;
  std::vector<VideoSendStream::Config> thumbnail_send_configs_;
  std::vector<VideoEncoderConfig> thumbnail_encoder_configs_;
  std::vector<VideoSendStream*> thumbnail_send_streams_;
  std::vector<VideoReceiveStream::Config> thumbnail_receive_configs_;
  std::vector<VideoReceiveStream*> thumbnail_receive_streams_;

  Clock* const clock_;

  int receive_logs_;
  int send_logs_;

  VideoSendStream::DegradationPreference degradation_preference_ =
      VideoSendStream::DegradationPreference::kMaintainFramerate;
  Params params_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_QUALITY_TEST_H_
