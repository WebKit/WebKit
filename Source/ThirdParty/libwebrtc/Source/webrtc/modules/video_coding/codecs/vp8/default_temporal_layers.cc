/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/default_temporal_layers.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace webrtc {

TemporalLayers::FrameConfig::FrameConfig()
    : FrameConfig(kNone, kNone, kNone, false) {}

TemporalLayers::FrameConfig::FrameConfig(TemporalLayers::BufferFlags last,
                                         TemporalLayers::BufferFlags golden,
                                         TemporalLayers::BufferFlags arf)
    : FrameConfig(last, golden, arf, false) {}

TemporalLayers::FrameConfig::FrameConfig(TemporalLayers::BufferFlags last,
                                         TemporalLayers::BufferFlags golden,
                                         TemporalLayers::BufferFlags arf,
                                         FreezeEntropy)
    : FrameConfig(last, golden, arf, true) {}

TemporalLayers::FrameConfig::FrameConfig(TemporalLayers::BufferFlags last,
                                         TemporalLayers::BufferFlags golden,
                                         TemporalLayers::BufferFlags arf,
                                         bool freeze_entropy)
    : drop_frame(last == TemporalLayers::kNone &&
                 golden == TemporalLayers::kNone &&
                 arf == TemporalLayers::kNone),
      last_buffer_flags(last),
      golden_buffer_flags(golden),
      arf_buffer_flags(arf),
      encoder_layer_id(0),
      packetizer_temporal_idx(kNoTemporalIdx),
      layer_sync(false),
      freeze_entropy(freeze_entropy) {}

namespace {

std::vector<unsigned int> GetTemporalIds(size_t num_layers) {
  switch (num_layers) {
    case 1:
      // Temporal layer structure (single layer):
      // 0 0 0 0 ...
      return {0};
    case 2:
      // Temporal layer structure:
      //   1   1 ...
      // 0   0   ...
      return {0, 1};
    case 3:
      // Temporal layer structure:
      //   2   2   2   2 ...
      //     1       1   ...
      // 0       0       ...
      return {0, 2, 1, 2};
    case 4:
      // Temporal layer structure:
      //   3   3   3   3   3   3   3   3 ...
      //     2       2       2       2   ...
      //         1               1       ...
      // 0               0               ...
      return {0, 3, 2, 3, 1, 3, 2, 3};
    default:
      RTC_NOTREACHED();
      break;
  }
  RTC_NOTREACHED();
  return {0};
}

std::vector<bool> GetTemporalLayerSync(size_t num_layers) {
  switch (num_layers) {
    case 1:
      return {false};
    case 2:
      return {false, true, false, false, false, false, false, false};
    case 3:
      if (field_trial::IsEnabled("WebRTC-UseShortVP8TL3Pattern")) {
        return {false, true, true, false};
      } else {
        return {false, true, true, false, false, false, false, false};
      }
    case 4:
      return {false, true,  true,  false, true,  false, false, false,
              false, false, false, false, false, false, false, false};
    default:
      break;
  }
  RTC_NOTREACHED() << num_layers;
  return {};
}

std::vector<TemporalLayers::FrameConfig> GetTemporalPattern(size_t num_layers) {
  // For indexing in the patterns described below (which temporal layers they
  // belong to), see the diagram above.
  // Layer sync is done similarly for all patterns (except single stream) and
  // happens every 8 frames:
  // TL1 layer syncs by periodically by only referencing TL0 ('last'), but still
  // updating 'golden', so it can be used as a reference by future TL1 frames.
  // TL2 layer syncs just before TL1 by only depending on TL0 (and not depending
  // on TL1's buffer before TL1 has layer synced).
  // TODO(pbos): Consider cyclically updating 'arf' (and 'golden' for 1TL) for
  // the base layer in 1-3TL instead of 'last' periodically on long intervals,
  // so that if scene changes occur (user walks between rooms or rotates webcam)
  // the 'arf' (or 'golden' respectively) is not stuck on a no-longer relevant
  // keyframe.
  switch (num_layers) {
    case 1:
      // All frames reference all buffers and the 'last' buffer is updated.
      return {TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kReference,
                                          TemporalLayers::kReference)};
    case 2:
      // All layers can reference but not update the 'alt' buffer, this means
      // that the 'alt' buffer reference is effectively the last keyframe.
      // TL0 also references and updates the 'last' buffer.
      // TL1 also references 'last' and references and updates 'golden'.
      return {TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kNone,
                                          TemporalLayers::kReference),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kUpdate,
                                          TemporalLayers::kReference),
              TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kNone,
                                          TemporalLayers::kReference),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kReference),
              TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kNone,
                                          TemporalLayers::kReference),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kReference),
              TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kNone,
                                          TemporalLayers::kReference),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kReference,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy)};
    case 3:
      if (field_trial::IsEnabled("WebRTC-UseShortVP8TL3Pattern")) {
        // This field trial is intended to check if it is worth using a shorter
        // temporal pattern, trading some coding efficiency for less risk of
        // dropped frames.
        // The coding efficiency will decrease somewhat since the higher layer
        // state is more volatile, but it will be offset slightly by updating
        // the altref buffer with TL2 frames, instead of just referencing lower
        // layers.
        // If a frame is dropped in a higher layer, the jitter
        // buffer on the receive side won't be able to decode any higher layer
        // frame until the next sync frame. So we expect a noticeable decrease
        // in frame drops on links with high packet loss.

        // TL0 references and updates the 'last' buffer.
        // TL1  references 'last' and references and updates 'golden'.
        // TL2 references both 'last' & 'golden' and references and updates
        // 'arf'.
        return {TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                            TemporalLayers::kNone,
                                            TemporalLayers::kNone),
                TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                            TemporalLayers::kNone,
                                            TemporalLayers::kUpdate),
                TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                            TemporalLayers::kUpdate,
                                            TemporalLayers::kNone),
                TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                            TemporalLayers::kReference,
                                            TemporalLayers::kReference,
                                            TemporalLayers::kFreezeEntropy)};
      } else {
        // All layers can reference but not update the 'alt' buffer, this means
        // that the 'alt' buffer reference is effectively the last keyframe.
        // TL0 also references and updates the 'last' buffer.
        // TL1 also references 'last' and references and updates 'golden'.
        // TL2 references both 'last' and 'golden' but updates no buffer.
        return {TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                            TemporalLayers::kNone,
                                            TemporalLayers::kReference),
                TemporalLayers::FrameConfig(
                    TemporalLayers::kReference, TemporalLayers::kNone,
                    TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
                TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                            TemporalLayers::kUpdate,
                                            TemporalLayers::kReference),
                TemporalLayers::FrameConfig(
                    TemporalLayers::kReference, TemporalLayers::kReference,
                    TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
                TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                            TemporalLayers::kNone,
                                            TemporalLayers::kReference),
                TemporalLayers::FrameConfig(
                    TemporalLayers::kReference, TemporalLayers::kReference,
                    TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
                TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                            TemporalLayers::kReferenceAndUpdate,
                                            TemporalLayers::kReference),
                TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                            TemporalLayers::kReference,
                                            TemporalLayers::kReference,
                                            TemporalLayers::kFreezeEntropy)};
      }
    case 4:
      // TL0 references and updates only the 'last' buffer.
      // TL1 references 'last' and updates and references 'golden'.
      // TL2 references 'last' and 'golden', and references and updates 'arf'.
      // TL3 references all buffers but update none of them.
      return {TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kNone,
                                          TemporalLayers::kNone),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kNone,
                  TemporalLayers::kNone, TemporalLayers::kFreezeEntropy),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kNone,
                                          TemporalLayers::kUpdate),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kNone,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kUpdate,
                                          TemporalLayers::kNone),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kReference,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kReference,
                                          TemporalLayers::kReferenceAndUpdate),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kReference,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
              TemporalLayers::FrameConfig(TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kNone,
                                          TemporalLayers::kNone),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kReference,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kReference,
                                          TemporalLayers::kReferenceAndUpdate),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kReference,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kReferenceAndUpdate,
                                          TemporalLayers::kNone),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kReference,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy),
              TemporalLayers::FrameConfig(TemporalLayers::kReference,
                                          TemporalLayers::kReference,
                                          TemporalLayers::kReferenceAndUpdate),
              TemporalLayers::FrameConfig(
                  TemporalLayers::kReference, TemporalLayers::kReference,
                  TemporalLayers::kReference, TemporalLayers::kFreezeEntropy)};
    default:
      RTC_NOTREACHED();
      break;
  }
  RTC_NOTREACHED();
  return {TemporalLayers::FrameConfig(
      TemporalLayers::kNone, TemporalLayers::kNone, TemporalLayers::kNone)};
}

// Temporary fix for forced SW fallback.
// For VP8 SW codec, |TemporalLayers| is created and reported to
// SimulcastRateAllocator::OnTemporalLayersCreated but not for VP8 HW.
// Causes an issue when going from forced SW -> HW as |TemporalLayers| is not
// deregistred when deleted by SW codec (tl factory might not exist, owned by
// SimulcastRateAllocator).
bool ExcludeOnTemporalLayersCreated(int num_temporal_layers) {
  return webrtc::field_trial::IsEnabled(
             "WebRTC-VP8-Forced-Fallback-Encoder-v2") &&
         num_temporal_layers == 1;
}
}  // namespace

DefaultTemporalLayers::DefaultTemporalLayers(int number_of_temporal_layers,
                                             uint8_t initial_tl0_pic_idx)
    : num_layers_(std::max(1, number_of_temporal_layers)),
      temporal_ids_(GetTemporalIds(num_layers_)),
      temporal_layer_sync_(GetTemporalLayerSync(num_layers_)),
      temporal_pattern_(GetTemporalPattern(num_layers_)),
      tl0_pic_idx_(initial_tl0_pic_idx),
      pattern_idx_(255),
      last_base_layer_sync_(false) {
  RTC_DCHECK_EQ(temporal_pattern_.size(), temporal_layer_sync_.size());
  RTC_CHECK_GE(kMaxTemporalStreams, number_of_temporal_layers);
  RTC_CHECK_GE(number_of_temporal_layers, 0);
  RTC_CHECK_LE(number_of_temporal_layers, 4);
  // pattern_idx_ wraps around temporal_pattern_.size, this is incorrect if
  // temporal_ids_ are ever longer. If this is no longer correct it needs to
  // wrap at max(temporal_ids_.size(), temporal_pattern_.size()).
  RTC_DCHECK_LE(temporal_ids_.size(), temporal_pattern_.size());
}

uint8_t DefaultTemporalLayers::Tl0PicIdx() const {
  return tl0_pic_idx_;
}

std::vector<uint32_t> DefaultTemporalLayers::OnRatesUpdated(
    int bitrate_kbps,
    int max_bitrate_kbps,
    int framerate) {
  std::vector<uint32_t> bitrates;
  for (size_t i = 0; i < num_layers_; ++i) {
    float layer_bitrate =
        bitrate_kbps * kVp8LayerRateAlloction[num_layers_ - 1][i];
    bitrates.push_back(static_cast<uint32_t>(layer_bitrate + 0.5));
  }
  new_bitrates_kbps_ = rtc::Optional<std::vector<uint32_t>>(bitrates);

  // Allocation table is of aggregates, transform to individual rates.
  uint32_t sum = 0;
  for (size_t i = 0; i < num_layers_; ++i) {
    uint32_t layer_bitrate = bitrates[i];
    RTC_DCHECK_LE(sum, bitrates[i]);
    bitrates[i] -= sum;
    sum = layer_bitrate;

    if (sum >= static_cast<uint32_t>(bitrate_kbps)) {
      // Sum adds up; any subsequent layers will be 0.
      bitrates.resize(i + 1);
      break;
    }
  }

  return bitrates;
}

bool DefaultTemporalLayers::UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) {
  if (!new_bitrates_kbps_)
    return false;

  for (size_t i = 0; i < num_layers_; ++i) {
    cfg->ts_target_bitrate[i] = (*new_bitrates_kbps_)[i];
    // ..., 4, 2, 1
    cfg->ts_rate_decimator[i] = 1 << (num_layers_ - i - 1);
  }

  cfg->ts_number_layers = num_layers_;
  cfg->ts_periodicity = temporal_ids_.size();
  memcpy(cfg->ts_layer_id, &temporal_ids_[0],
         sizeof(unsigned int) * temporal_ids_.size());

  new_bitrates_kbps_ = rtc::Optional<std::vector<uint32_t>>();

  return true;
}

TemporalLayers::FrameConfig DefaultTemporalLayers::UpdateLayerConfig(
    uint32_t timestamp) {
  RTC_DCHECK_GT(num_layers_, 0);
  RTC_DCHECK_LT(0, temporal_pattern_.size());
  pattern_idx_ = (pattern_idx_ + 1) % temporal_pattern_.size();
  TemporalLayers::FrameConfig tl_config = temporal_pattern_[pattern_idx_];
  tl_config.layer_sync =
      temporal_layer_sync_[pattern_idx_ % temporal_layer_sync_.size()];
  tl_config.encoder_layer_id = tl_config.packetizer_temporal_idx =
      temporal_ids_[pattern_idx_ % temporal_ids_.size()];
  return tl_config;
}

void DefaultTemporalLayers::PopulateCodecSpecific(
    bool frame_is_keyframe,
    const TemporalLayers::FrameConfig& tl_config,
    CodecSpecificInfoVP8* vp8_info,
    uint32_t timestamp) {
  RTC_DCHECK_GT(num_layers_, 0);

  if (num_layers_ == 1) {
    vp8_info->temporalIdx = kNoTemporalIdx;
    vp8_info->layerSync = false;
    vp8_info->tl0PicIdx = kNoTl0PicIdx;
  } else {
    vp8_info->temporalIdx = tl_config.packetizer_temporal_idx;
    vp8_info->layerSync = tl_config.layer_sync;
    if (frame_is_keyframe) {
      vp8_info->temporalIdx = 0;
      vp8_info->layerSync = true;
    }
    if (last_base_layer_sync_ && vp8_info->temporalIdx != 0) {
      // Regardless of pattern the frame after a base layer sync will always
      // be a layer sync.
      vp8_info->layerSync = true;
    }
    if (vp8_info->temporalIdx == 0)
      tl0_pic_idx_++;
    last_base_layer_sync_ = frame_is_keyframe;
    vp8_info->tl0PicIdx = tl0_pic_idx_;
  }
}

TemporalLayers* TemporalLayersFactory::Create(
    int simulcast_id,
    int temporal_layers,
    uint8_t initial_tl0_pic_idx) const {
  TemporalLayers* tl =
      new DefaultTemporalLayers(temporal_layers, initial_tl0_pic_idx);
  if (listener_ && !ExcludeOnTemporalLayersCreated(temporal_layers))
    listener_->OnTemporalLayersCreated(simulcast_id, tl);
  return tl;
}

std::unique_ptr<TemporalLayersChecker> TemporalLayersFactory::CreateChecker(
    int /*simulcast_id*/,
    int temporal_layers,
    uint8_t initial_tl0_pic_idx) const {
  TemporalLayersChecker* tlc =
      new DefaultTemporalLayersChecker(temporal_layers, initial_tl0_pic_idx);
  return std::unique_ptr<TemporalLayersChecker>(tlc);
}

void TemporalLayersFactory::SetListener(TemporalLayersListener* listener) {
  listener_ = listener;
}

// Returns list of temporal dependencies for each frame in the temporal pattern.
// Values are lists of indecies in the pattern.
std::vector<std::set<uint8_t>> GetTemporalDependencies(
    int num_temporal_layers) {
  switch (num_temporal_layers) {
    case 1:
      return {{0}};
    case 2:
      return {{6}, {0}, {0}, {1, 2}, {2}, {3, 4}, {4}, {5, 6}};
    case 3:
      if (field_trial::IsEnabled("WebRTC-UseShortVP8TL3Pattern")) {
        return {{0}, {0}, {0}, {0, 1, 2}};
      } else {
        return {{4}, {0}, {0}, {0, 2}, {0}, {2, 4}, {2, 4}, {4, 6}};
      }
    case 4:
      return {{8},    {0},         {0},         {0, 2},
              {0},    {0, 2, 4},   {0, 2, 4},   {0, 4, 6},
              {0},    {4, 6, 8},   {4, 6, 8},   {4, 8, 10},
              {4, 8}, {8, 10, 12}, {8, 10, 12}, {8, 12, 14}};
    default:
      RTC_NOTREACHED();
      return {};
  }
}

DefaultTemporalLayersChecker::DefaultTemporalLayersChecker(
    int num_temporal_layers,
    uint8_t initial_tl0_pic_idx)
    : TemporalLayersChecker(num_temporal_layers, initial_tl0_pic_idx),
      num_layers_(std::max(1, num_temporal_layers)),
      temporal_ids_(GetTemporalIds(num_layers_)),
      temporal_dependencies_(GetTemporalDependencies(num_layers_)),
      pattern_idx_(255) {
  int i = 0;
  while (temporal_ids_.size() < temporal_dependencies_.size()) {
    temporal_ids_.push_back(temporal_ids_[i++]);
  }
}

bool DefaultTemporalLayersChecker::CheckTemporalConfig(
    bool frame_is_keyframe,
    const TemporalLayers::FrameConfig& frame_config) {
  if (!TemporalLayersChecker::CheckTemporalConfig(frame_is_keyframe,
                                                  frame_config)) {
    return false;
  }
  if (frame_config.drop_frame) {
    return true;
  }
  ++pattern_idx_;
  if (pattern_idx_ == temporal_ids_.size()) {
    // All non key-frame buffers should be updated each pattern cycle.
    if (!last_.is_keyframe && !last_.is_updated_this_cycle) {
      RTC_LOG(LS_ERROR) << "Last buffer was not updated during pattern cycle.";
      return false;
    }
    if (!arf_.is_keyframe && !arf_.is_updated_this_cycle) {
      RTC_LOG(LS_ERROR) << "Arf buffer was not updated during pattern cycle.";
      return false;
    }
    if (!golden_.is_keyframe && !golden_.is_updated_this_cycle) {
      RTC_LOG(LS_ERROR)
          << "Golden buffer was not updated during pattern cycle.";
      return false;
    }
    last_.is_updated_this_cycle = false;
    arf_.is_updated_this_cycle = false;
    golden_.is_updated_this_cycle = false;
    pattern_idx_ = 0;
  }
  uint8_t expected_tl_idx = temporal_ids_[pattern_idx_];
  if (frame_config.packetizer_temporal_idx != expected_tl_idx) {
    RTC_LOG(LS_ERROR) << "Frame has an incorrect temporal index. Expected: "
                      << static_cast<int>(expected_tl_idx) << " Actual: "
                      << static_cast<int>(frame_config.packetizer_temporal_idx);
    return false;
  }

  bool need_sync = temporal_ids_[pattern_idx_] > 0 &&
                   temporal_ids_[pattern_idx_] != kNoTemporalIdx;
  std::vector<int> dependencies;

  if (frame_config.last_buffer_flags &
      TemporalLayers::BufferFlags::kReference) {
    uint8_t referenced_layer = temporal_ids_[last_.pattern_idx];
    if (referenced_layer > 0) {
      need_sync = false;
    }
    if (!last_.is_keyframe) {
      dependencies.push_back(last_.pattern_idx);
    }
  }

  if (frame_config.arf_buffer_flags & TemporalLayers::BufferFlags::kReference) {
    uint8_t referenced_layer = temporal_ids_[arf_.pattern_idx];
    if (referenced_layer > 0) {
      need_sync = false;
    }
    if (!arf_.is_keyframe) {
      dependencies.push_back(arf_.pattern_idx);
    }
  }

  if (frame_config.golden_buffer_flags &
      TemporalLayers::BufferFlags::kReference) {
    uint8_t referenced_layer = temporal_ids_[golden_.pattern_idx];
    if (referenced_layer > 0) {
      need_sync = false;
    }
    if (!golden_.is_keyframe) {
      dependencies.push_back(golden_.pattern_idx);
    }
  }

  if (need_sync != frame_config.layer_sync) {
    RTC_LOG(LS_ERROR) << "Sync bit is set incorrectly on a frame. Expected: "
                      << need_sync << " Actual: " << frame_config.layer_sync;
    return false;
  }

  if (!frame_is_keyframe) {
    size_t i;
    for (i = 0; i < dependencies.size(); ++i) {
      if (temporal_dependencies_[pattern_idx_].find(dependencies[i]) ==
          temporal_dependencies_[pattern_idx_].end()) {
        RTC_LOG(LS_ERROR)
            << "Illegal temporal dependency out of defined pattern "
               "from position "
            << static_cast<int>(pattern_idx_) << " to position "
            << static_cast<int>(dependencies[i]);
        return false;
      }
    }
  }

  if (frame_config.last_buffer_flags & TemporalLayers::BufferFlags::kUpdate) {
    last_.is_updated_this_cycle = true;
    last_.pattern_idx = pattern_idx_;
    last_.is_keyframe = false;
  }
  if (frame_config.arf_buffer_flags & TemporalLayers::BufferFlags::kUpdate) {
    arf_.is_updated_this_cycle = true;
    arf_.pattern_idx = pattern_idx_;
    arf_.is_keyframe = false;
  }
  if (frame_config.golden_buffer_flags & TemporalLayers::BufferFlags::kUpdate) {
    golden_.is_updated_this_cycle = true;
    golden_.pattern_idx = pattern_idx_;
    golden_.is_keyframe = false;
  }
  if (frame_is_keyframe) {
    last_.is_keyframe = true;
    arf_.is_keyframe = true;
    golden_.is_keyframe = true;
  }
  return true;
}

}  // namespace webrtc
