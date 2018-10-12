/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_QUALITY_TEST_FIXTURE_H_
#define API_TEST_VIDEO_QUALITY_TEST_FIXTURE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/bitrate_constraints.h"
#include "api/fec_controller.h"
#include "api/mediatypes.h"
#include "api/test/simulated_network.h"
#include "api/video_codecs/video_encoder_config.h"

namespace webrtc {

class VideoQualityTestFixtureInterface {
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
      bool generic_descriptor;
      BitrateConstraints call_bitrate_config;
      int num_thumbnails;
      // Indicates if secondary_(video|ss|screenshare) structures are used.
      bool dual_video;
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
      bool automatic_scaling;
      std::string clip_name;  // "Generator" to generate frames instead.
      size_t capture_device_index;
      SdpVideoFormat::Parameters sdp_params;
    } video[2];
    struct Audio {
      bool enabled;
      bool sync_video;
      bool dtx;
      bool use_real_adm;
    } audio;
    struct Screenshare {
      bool enabled;
      bool generate_slides;
      int32_t slide_change_interval;
      int32_t scroll_duration;
      std::vector<std::string> slides;
    } screenshare[2];
    struct Analyzer {
      std::string test_label;
      double avg_psnr_threshold;  // (*)
      double avg_ssim_threshold;  // (*)
      int test_durations_secs;
      std::string graph_data_output_filename;
      std::string graph_title;
    } analyzer;
    // Deprecated. DO NOT USE. Use config instead. This is not pipe actually,
    // it is just configuration, that will be passed to default implementation
    // of simulation layer.
    DefaultNetworkSimulationConfig pipe;
    // Config for default simulation implementation. Must be nullopt if
    // `sender_network` and `receiver_network` in InjectionComponents are
    // non-null. May be nullopt even if `sender_network` and `receiver_network`
    // are null; in that case, a default config will be used.
    absl::optional<DefaultNetworkSimulationConfig> config;
    struct SS {                          // Spatial scalability.
      std::vector<VideoStream> streams;  // If empty, one stream is assumed.
      size_t selected_stream;
      int num_spatial_layers;
      int selected_sl;
      InterLayerPredMode inter_layer_pred;
      // If empty, bitrates are generated in VP9Impl automatically.
      std::vector<SpatialLayer> spatial_layers;
      // If set, default parameters will be used instead of |streams|.
      bool infer_streams;
    } ss[2];
    struct Logging {
      std::string rtc_event_log_name;
      std::string rtp_dump_name;
      std::string encoded_frame_base_path;
    } logging;
  };

  // Contains objects, that will be injected on different layers of test
  // framework to override the behavior of system parts.
  struct InjectionComponents {
    InjectionComponents();
    ~InjectionComponents();

    // Simulations of sender and receiver networks. They must either both be
    // null (in which case `config` from Params is used), or both be non-null
    // (in which case `config` from Params must be nullopt).
    std::unique_ptr<NetworkBehaviorInterface> sender_network;
    std::unique_ptr<NetworkBehaviorInterface> receiver_network;

    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory;
  };

  virtual ~VideoQualityTestFixtureInterface() = default;

  virtual void RunWithAnalyzer(const Params& params) = 0;
  virtual void RunWithRenderers(const Params& params) = 0;

  virtual const std::map<uint8_t, webrtc::MediaType>& payload_type_map() = 0;
};

}  // namespace webrtc

#endif  // API_TEST_VIDEO_QUALITY_TEST_FIXTURE_H_
