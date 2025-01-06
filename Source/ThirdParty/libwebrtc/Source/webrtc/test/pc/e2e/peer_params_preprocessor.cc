/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/peer_params_preprocessor.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/rtp_parameters.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/video_codecs/scalability_mode.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/svc/create_scalability_structure.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "modules/video_coding/svc/scalable_video_controller.h"
#include "rtc_base/checks.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

// List of default names of generic participants according to
// https://en.wikipedia.org/wiki/Alice_and_Bob
constexpr absl::string_view kDefaultNames[] = {"alice", "bob",  "charlie",
                                               "david", "erin", "frank"};

}  // namespace

class PeerParamsPreprocessor::DefaultNamesProvider {
 public:
  // Caller have to ensure that default names array will outlive names provider
  // instance.
  explicit DefaultNamesProvider(
      absl::string_view prefix,
      rtc::ArrayView<const absl::string_view> default_names = {})
      : prefix_(prefix), default_names_(default_names) {}

  void MaybeSetName(std::optional<std::string>& name) {
    if (name.has_value()) {
      known_names_.insert(name.value());
    } else {
      name = GenerateName();
    }
  }

 private:
  std::string GenerateName() {
    std::string name;
    do {
      name = GenerateNameInternal();
    } while (!known_names_.insert(name).second);
    return name;
  }

  std::string GenerateNameInternal() {
    if (counter_ < default_names_.size()) {
      return std::string(default_names_[counter_++]);
    }
    return prefix_ + std::to_string(counter_++);
  }

  const std::string prefix_;
  const rtc::ArrayView<const absl::string_view> default_names_;

  std::set<std::string> known_names_;
  size_t counter_ = 0;
};

PeerParamsPreprocessor::PeerParamsPreprocessor()
    : peer_names_provider_(
          std::make_unique<DefaultNamesProvider>("peer_", kDefaultNames)) {}
PeerParamsPreprocessor::~PeerParamsPreprocessor() = default;

void PeerParamsPreprocessor::SetDefaultValuesForMissingParams(
    PeerConfigurer& peer) {
  Params* params = peer.params();
  ConfigurableParams* configurable_params = peer.configurable_params();
  peer_names_provider_->MaybeSetName(params->name);
  DefaultNamesProvider video_stream_names_provider(*params->name +
                                                   "_auto_video_stream_label_");
  for (VideoConfig& config : configurable_params->video_configs) {
    video_stream_names_provider.MaybeSetName(config.stream_label);
  }
  if (params->audio_config) {
    DefaultNamesProvider audio_stream_names_provider(
        *params->name + "_auto_audio_stream_label_");
    audio_stream_names_provider.MaybeSetName(
        params->audio_config->stream_label);
  }

  if (params->video_codecs.empty()) {
    params->video_codecs.push_back(VideoCodecConfig(cricket::kVp8CodecName));
  }
}

void PeerParamsPreprocessor::ValidateParams(const PeerConfigurer& peer) {
  const Params& p = peer.params();
  RTC_CHECK_GT(p.video_encoder_bitrate_multiplier, 0.0);
  // Each peer should at least support 1 video codec.
  RTC_CHECK_GE(p.video_codecs.size(), 1);

  {
    RTC_CHECK(p.name);
    bool inserted = peer_names_.insert(p.name.value()).second;
    RTC_CHECK(inserted) << "Duplicate name=" << p.name.value();
  }

  // Validate that all video stream labels are unique and sync groups are
  // valid.
  for (const VideoConfig& video_config :
       peer.configurable_params().video_configs) {
    RTC_CHECK(video_config.stream_label);
    bool inserted =
        video_labels_.insert(video_config.stream_label.value()).second;
    RTC_CHECK(inserted) << "Duplicate video_config.stream_label="
                        << video_config.stream_label.value();

    // TODO(bugs.webrtc.org/4762): remove this check after synchronization of
    // more than two streams is supported.
    if (video_config.sync_group.has_value()) {
      bool sync_group_inserted =
          video_sync_groups_.insert(video_config.sync_group.value()).second;
      RTC_CHECK(sync_group_inserted)
          << "Sync group shouldn't consist of more than two streams (one "
             "video and one audio). Duplicate video_config.sync_group="
          << video_config.sync_group.value();
    }

    if (video_config.simulcast_config) {
      if (!video_config.encoding_params.empty()) {
        RTC_CHECK_EQ(video_config.simulcast_config->simulcast_streams_count,
                     video_config.encoding_params.size())
            << "|encoding_params| have to be specified for each simulcast "
            << "stream in |video_config|.";
      }
    } else {
      RTC_CHECK_LE(video_config.encoding_params.size(), 1)
          << "|encoding_params| has multiple values but simulcast is not "
             "enabled.";
    }

    if (video_config.emulated_sfu_config) {
      if (video_config.simulcast_config &&
          video_config.emulated_sfu_config->target_layer_index) {
        RTC_CHECK_LT(*video_config.emulated_sfu_config->target_layer_index,
                     video_config.simulcast_config->simulcast_streams_count);
      }
      if (!video_config.encoding_params.empty()) {
        bool is_svc = false;
        for (const auto& encoding_param : video_config.encoding_params) {
          if (!encoding_param.scalability_mode)
            continue;

          std::optional<ScalabilityMode> scalability_mode =
              ScalabilityModeFromString(*encoding_param.scalability_mode);
          RTC_CHECK(scalability_mode) << "Unknown scalability_mode requested";

          std::optional<ScalableVideoController::StreamLayersConfig>
              stream_layers_config =
                  ScalabilityStructureConfig(*scalability_mode);
          is_svc |= stream_layers_config->num_spatial_layers > 1;
          RTC_CHECK(stream_layers_config->num_spatial_layers == 1 ||
                    video_config.encoding_params.size() == 1)
              << "Can't enable SVC modes with multiple spatial layers ("
              << stream_layers_config->num_spatial_layers
              << " layers) or simulcast ("
              << video_config.encoding_params.size() << " layers)";
          if (video_config.emulated_sfu_config->target_layer_index) {
            RTC_CHECK_LT(*video_config.emulated_sfu_config->target_layer_index,
                         stream_layers_config->num_spatial_layers);
          }
        }
        if (!is_svc && video_config.emulated_sfu_config->target_layer_index) {
          RTC_CHECK_LT(*video_config.emulated_sfu_config->target_layer_index,
                       video_config.encoding_params.size());
        }
      }
    }
  }
  if (p.audio_config) {
    bool inserted =
        audio_labels_.insert(p.audio_config->stream_label.value()).second;
    RTC_CHECK(inserted) << "Duplicate audio_config.stream_label="
                        << p.audio_config->stream_label.value();
    // TODO(bugs.webrtc.org/4762): remove this check after synchronization of
    // more than two streams is supported.
    if (p.audio_config->sync_group.has_value()) {
      bool sync_group_inserted =
          audio_sync_groups_.insert(p.audio_config->sync_group.value()).second;
      RTC_CHECK(sync_group_inserted)
          << "Sync group shouldn't consist of more than two streams (one "
             "video and one audio). Duplicate audio_config.sync_group="
          << p.audio_config->sync_group.value();
    }
    // Check that if input file name specified, the file actually exists.
    if (p.audio_config.value().input_file_name) {
      RTC_CHECK(
          test::FileExists(p.audio_config.value().input_file_name.value()))
          << p.audio_config.value().input_file_name.value() << " doesn't exist";
    }
  }
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
