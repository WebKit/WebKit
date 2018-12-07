/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/codecs/vp8/default_temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

Vp8TemporalLayers::FrameConfig::FrameConfig()
    : FrameConfig(kNone, kNone, kNone, false) {}

Vp8TemporalLayers::FrameConfig::FrameConfig(
    Vp8TemporalLayers::BufferFlags last,
    Vp8TemporalLayers::BufferFlags golden,
    Vp8TemporalLayers::BufferFlags arf)
    : FrameConfig(last, golden, arf, false) {}

Vp8TemporalLayers::FrameConfig::FrameConfig(
    Vp8TemporalLayers::BufferFlags last,
    Vp8TemporalLayers::BufferFlags golden,
    Vp8TemporalLayers::BufferFlags arf,
    FreezeEntropy)
    : FrameConfig(last, golden, arf, true) {}

Vp8TemporalLayers::FrameConfig::FrameConfig(
    Vp8TemporalLayers::BufferFlags last,
    Vp8TemporalLayers::BufferFlags golden,
    Vp8TemporalLayers::BufferFlags arf,
    bool freeze_entropy)
    : drop_frame(last == Vp8TemporalLayers::kNone &&
                 golden == Vp8TemporalLayers::kNone &&
                 arf == Vp8TemporalLayers::kNone),
      last_buffer_flags(last),
      golden_buffer_flags(golden),
      arf_buffer_flags(arf),
      encoder_layer_id(0),
      packetizer_temporal_idx(kNoTemporalIdx),
      layer_sync(false),
      freeze_entropy(freeze_entropy),
      first_reference(Vp8BufferReference::kNone),
      second_reference(Vp8BufferReference::kNone) {}

DefaultTemporalLayers::PendingFrame::PendingFrame() = default;
DefaultTemporalLayers::PendingFrame::PendingFrame(
    bool expired,
    uint8_t updated_buffers_mask,
    const FrameConfig& frame_config)
    : expired(expired),
      updated_buffer_mask(updated_buffers_mask),
      frame_config(frame_config) {}

namespace {
static constexpr uint8_t kUninitializedPatternIndex =
    std::numeric_limits<uint8_t>::max();
static constexpr std::array<Vp8BufferReference, 3> kAllBuffers = {
    {Vp8BufferReference::kLast, Vp8BufferReference::kGolden,
     Vp8BufferReference::kAltref}};

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

uint8_t GetUpdatedBuffers(const Vp8TemporalLayers::FrameConfig& config) {
  uint8_t flags = 0;
  if (config.last_buffer_flags & Vp8TemporalLayers::BufferFlags::kUpdate) {
    flags |= static_cast<uint8_t>(Vp8BufferReference::kLast);
  }
  if (config.golden_buffer_flags & Vp8TemporalLayers::BufferFlags::kUpdate) {
    flags |= static_cast<uint8_t>(Vp8BufferReference::kGolden);
  }
  if (config.arf_buffer_flags & Vp8TemporalLayers::BufferFlags::kUpdate) {
    flags |= static_cast<uint8_t>(Vp8BufferReference::kAltref);
  }
  return flags;
}

// Find the set of buffers that are never updated by the given pattern.
std::set<Vp8BufferReference> FindKfBuffers(
    const std::vector<Vp8TemporalLayers::FrameConfig>& frame_configs) {
  std::set<Vp8BufferReference> kf_buffers(kAllBuffers.begin(),
                                          kAllBuffers.end());
  for (Vp8TemporalLayers::FrameConfig config : frame_configs) {
    // Get bit-masked set of update buffers for this frame config.
    uint8_t updated_buffers = GetUpdatedBuffers(config);
    for (Vp8BufferReference buffer : kAllBuffers) {
      if (static_cast<uint8_t>(buffer) & updated_buffers) {
        kf_buffers.erase(buffer);
      }
    }
  }
  return kf_buffers;
}
}  // namespace

std::vector<Vp8TemporalLayers::FrameConfig>
DefaultTemporalLayers::GetTemporalPattern(size_t num_layers) {
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
      return {FrameConfig(kReferenceAndUpdate, kReference, kReference)};
    case 2:
      // All layers can reference but not update the 'alt' buffer, this means
      // that the 'alt' buffer reference is effectively the last keyframe.
      // TL0 also references and updates the 'last' buffer.
      // TL1 also references 'last' and references and updates 'golden'.
      if (!field_trial::IsDisabled("WebRTC-UseShortVP8TL2Pattern")) {
        // Shortened 4-frame pattern:
        //   1---1   1---1 ...
        //  /   /   /   /
        // 0---0---0---0 ...
        return {
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kUpdate, kReference),
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kReference, kReference, kFreezeEntropy)};
      } else {
        // "Default" 8-frame pattern:
        //   1---1---1---1   1---1---1---1 ...
        //  /   /   /   /   /   /   /   /
        // 0---0---0---0---0---0---0---0 ...
        return {
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kUpdate, kReference),
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kReferenceAndUpdate, kReference),
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kReferenceAndUpdate, kReference),
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kReference, kReference, kFreezeEntropy)};
      }
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
        return {
            FrameConfig(kReferenceAndUpdate, kNone, kNone),
            FrameConfig(kReference, kNone, kUpdate),
            FrameConfig(kReference, kUpdate, kNone),
            FrameConfig(kReference, kReference, kReference, kFreezeEntropy)};
      } else {
        // All layers can reference but not update the 'alt' buffer, this means
        // that the 'alt' buffer reference is effectively the last keyframe.
        // TL0 also references and updates the 'last' buffer.
        // TL1 also references 'last' and references and updates 'golden'.
        // TL2 references both 'last' and 'golden' but updates no buffer.
        return {
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kNone, kReference, kFreezeEntropy),
            FrameConfig(kReference, kUpdate, kReference),
            FrameConfig(kReference, kReference, kReference, kFreezeEntropy),
            FrameConfig(kReferenceAndUpdate, kNone, kReference),
            FrameConfig(kReference, kReference, kReference, kFreezeEntropy),
            FrameConfig(kReference, kReferenceAndUpdate, kReference),
            FrameConfig(kReference, kReference, kReference, kFreezeEntropy)};
      }
    case 4:
      // TL0 references and updates only the 'last' buffer.
      // TL1 references 'last' and updates and references 'golden'.
      // TL2 references 'last' and 'golden', and references and updates 'arf'.
      // TL3 references all buffers but update none of them.
      return {FrameConfig(kReferenceAndUpdate, kNone, kNone),
              FrameConfig(kReference, kNone, kNone, kFreezeEntropy),
              FrameConfig(kReference, kNone, kUpdate),
              FrameConfig(kReference, kNone, kReference, kFreezeEntropy),
              FrameConfig(kReference, kUpdate, kNone),
              FrameConfig(kReference, kReference, kReference, kFreezeEntropy),
              FrameConfig(kReference, kReference, kReferenceAndUpdate),
              FrameConfig(kReference, kReference, kReference, kFreezeEntropy),
              FrameConfig(kReferenceAndUpdate, kNone, kNone),
              FrameConfig(kReference, kReference, kReference, kFreezeEntropy),
              FrameConfig(kReference, kReference, kReferenceAndUpdate),
              FrameConfig(kReference, kReference, kReference, kFreezeEntropy),
              FrameConfig(kReference, kReferenceAndUpdate, kNone),
              FrameConfig(kReference, kReference, kReference, kFreezeEntropy),
              FrameConfig(kReference, kReference, kReferenceAndUpdate),
              FrameConfig(kReference, kReference, kReference, kFreezeEntropy)};
    default:
      RTC_NOTREACHED();
      break;
  }
  RTC_NOTREACHED();
  return {FrameConfig(kNone, kNone, kNone)};
}

DefaultTemporalLayers::DefaultTemporalLayers(int number_of_temporal_layers)
    : num_layers_(std::max(1, number_of_temporal_layers)),
      temporal_ids_(GetTemporalIds(num_layers_)),
      temporal_pattern_(GetTemporalPattern(num_layers_)),
      kf_buffers_(FindKfBuffers(temporal_pattern_)),
      pattern_idx_(kUninitializedPatternIndex),
      checker_(TemporalLayersChecker::CreateTemporalLayersChecker(
          Vp8TemporalLayersType::kFixedPattern,
          number_of_temporal_layers)) {
  RTC_CHECK_GE(kMaxTemporalStreams, number_of_temporal_layers);
  RTC_CHECK_GE(number_of_temporal_layers, 0);
  RTC_CHECK_LE(number_of_temporal_layers, 4);
  // pattern_idx_ wraps around temporal_pattern_.size, this is incorrect if
  // temporal_ids_ are ever longer. If this is no longer correct it needs to
  // wrap at max(temporal_ids_.size(), temporal_pattern_.size()).
  RTC_DCHECK_LE(temporal_ids_.size(), temporal_pattern_.size());

  // Always need to start with a keyframe, so pre-populate all frame counters.
  for (Vp8BufferReference buffer : kAllBuffers) {
    frames_since_buffer_refresh_[buffer] = 0;
  }
}

DefaultTemporalLayers::~DefaultTemporalLayers() = default;

bool DefaultTemporalLayers::SupportsEncoderFrameDropping() const {
  // This class allows the encoder drop frames as it sees fit.
  return true;
}

void DefaultTemporalLayers::OnRatesUpdated(
    const std::vector<uint32_t>& bitrates_bps,
    int framerate_fps) {
  RTC_DCHECK_GT(bitrates_bps.size(), 0);
  RTC_DCHECK_LE(bitrates_bps.size(), num_layers_);
  // |bitrates_bps| uses individual rate per layer, but Vp8EncoderConfig wants
  // the accumulated rate, so sum them up.
  new_bitrates_bps_ = bitrates_bps;
  new_bitrates_bps_->resize(num_layers_);
  for (size_t i = 1; i < num_layers_; ++i) {
    (*new_bitrates_bps_)[i] += (*new_bitrates_bps_)[i - 1];
  }
}

bool DefaultTemporalLayers::UpdateConfiguration(Vp8EncoderConfig* cfg) {
  if (!new_bitrates_bps_) {
    return false;
  }

  for (size_t i = 0; i < num_layers_; ++i) {
    cfg->ts_target_bitrate[i] = (*new_bitrates_bps_)[i] / 1000;
    // ..., 4, 2, 1
    cfg->ts_rate_decimator[i] = 1 << (num_layers_ - i - 1);
  }

  cfg->ts_number_layers = num_layers_;
  cfg->ts_periodicity = temporal_ids_.size();
  memcpy(cfg->ts_layer_id, &temporal_ids_[0],
         sizeof(unsigned int) * temporal_ids_.size());

  new_bitrates_bps_.reset();

  return true;
}

bool DefaultTemporalLayers::IsSyncFrame(const FrameConfig& config) const {
  // Since we always assign TL0 to 'last' in these patterns, we can infer layer
  // sync by checking if temporal id > 0 and we only reference TL0 or buffers
  // containing the last key-frame.
  if (config.packetizer_temporal_idx == 0) {
    // TL0 frames are per definition not sync frames.
    return false;
  }

  if ((config.last_buffer_flags & BufferFlags::kReference) == 0) {
    // Sync frames must reference TL0.
    return false;
  }

  if ((config.golden_buffer_flags & BufferFlags::kReference) &&
      kf_buffers_.find(Vp8BufferReference::kGolden) == kf_buffers_.end()) {
    // Referencing a golden frame that contains a non-(base layer|key frame).
    return false;
  }
  if ((config.arf_buffer_flags & BufferFlags::kReference) &&
      kf_buffers_.find(Vp8BufferReference::kAltref) == kf_buffers_.end()) {
    // Referencing an altref frame that contains a non-(base layer|key frame).
    return false;
  }

  return true;
}

Vp8TemporalLayers::FrameConfig DefaultTemporalLayers::UpdateLayerConfig(
    uint32_t timestamp) {
  RTC_DCHECK_GT(num_layers_, 0);
  RTC_DCHECK_LT(0, temporal_pattern_.size());

  pattern_idx_ = (pattern_idx_ + 1) % temporal_pattern_.size();
  Vp8TemporalLayers::FrameConfig tl_config = temporal_pattern_[pattern_idx_];
  tl_config.encoder_layer_id = tl_config.packetizer_temporal_idx =
      temporal_ids_[pattern_idx_ % temporal_ids_.size()];

  if (pattern_idx_ == 0) {
    // Start of new pattern iteration, set up clear state by invalidating any
    // pending frames, so that we don't make an invalid reference to a buffer
    // containing data from a previous iteration.
    for (auto& it : pending_frames_) {
      it.second.expired = true;
    }
  }

  // Last is always ok to reference as it contains the base layer. For other
  // buffers though, we need to check if the buffer has actually been refreshed
  // this cycle of the temporal pattern. If the encoder dropped a frame, it
  // might not have.
  ValidateReferences(&tl_config.golden_buffer_flags,
                     Vp8BufferReference::kGolden);
  ValidateReferences(&tl_config.arf_buffer_flags, Vp8BufferReference::kAltref);
  // Update search order to let the encoder know which buffers contains the most
  // recent data.
  UpdateSearchOrder(&tl_config);
  // Figure out if this a sync frame (non-base-layer frame with only base-layer
  // references).
  tl_config.layer_sync = IsSyncFrame(tl_config);

  // Increment frame age, this needs to be in sync with |pattern_idx_|, so must
  // update it here. Resetting age to 0 must be done when encoding is complete
  // though, and so in the case of pipelining encoder it might lag. To prevent
  // this data spill over into the next iteration, the |pedning_frames_| map
  // is reset in loops. If delay is constant, the relative age should still be
  // OK for the search order.
  for (Vp8BufferReference buffer : kAllBuffers) {
    ++frames_since_buffer_refresh_[buffer];
  }

  // Add frame to set of pending frames, awaiting completion.
  pending_frames_[timestamp] =
      PendingFrame{false, GetUpdatedBuffers(tl_config), tl_config};

  if (checker_) {
    // Checker does not yet support encoder frame dropping, so validate flags
    // here before they can be dropped.
    // TODO(sprang): Update checker to support dropping.
    RTC_DCHECK(checker_->CheckTemporalConfig(false, tl_config));
  }

  return tl_config;
}

void DefaultTemporalLayers::ValidateReferences(BufferFlags* flags,
                                               Vp8BufferReference ref) const {
  // Check if the buffer specified by |ref| is actually referenced, and if so
  // if it also a dynamically updating one (buffers always just containing
  // keyframes are always safe to reference).
  if ((*flags & BufferFlags::kReference) &&
      kf_buffers_.find(ref) == kf_buffers_.end()) {
    auto it = frames_since_buffer_refresh_.find(ref);
    if (it == frames_since_buffer_refresh_.end() ||
        it->second >= pattern_idx_) {
      // No valid buffer state, or buffer contains frame that is older than the
      // current pattern. This reference is not valid, so remove it.
      *flags = static_cast<BufferFlags>(*flags & ~BufferFlags::kReference);
    }
  }
}

void DefaultTemporalLayers::UpdateSearchOrder(FrameConfig* config) {
  // Figure out which of the buffers we can reference, and order them so that
  // the most recently refreshed is first. Otherwise prioritize last first,
  // golden second, and altref third.
  using BufferRefAge = std::pair<Vp8BufferReference, size_t>;
  std::vector<BufferRefAge> eligible_buffers;
  if (config->last_buffer_flags & BufferFlags::kReference) {
    eligible_buffers.emplace_back(
        Vp8BufferReference::kLast,
        frames_since_buffer_refresh_[Vp8BufferReference::kLast]);
  }
  if (config->golden_buffer_flags & BufferFlags::kReference) {
    eligible_buffers.emplace_back(
        Vp8BufferReference::kGolden,
        frames_since_buffer_refresh_[Vp8BufferReference::kGolden]);
  }
  if (config->arf_buffer_flags & BufferFlags::kReference) {
    eligible_buffers.emplace_back(
        Vp8BufferReference::kAltref,
        frames_since_buffer_refresh_[Vp8BufferReference::kAltref]);
  }

  std::sort(eligible_buffers.begin(), eligible_buffers.end(),
            [](const BufferRefAge& lhs, const BufferRefAge& rhs) {
              if (lhs.second != rhs.second) {
                // Lower count has highest precedence.
                return lhs.second < rhs.second;
              }
              return lhs.first < rhs.first;
            });

  // Populate the search order fields where possible.
  if (!eligible_buffers.empty()) {
    config->first_reference = eligible_buffers.front().first;
    if (eligible_buffers.size() > 1)
      config->second_reference = eligible_buffers[1].first;
  }
}

void DefaultTemporalLayers::OnEncodeDone(uint32_t rtp_timestamp,
                                         size_t size_bytes,
                                         bool is_keyframe,
                                         int qp,
                                         CodecSpecificInfoVP8* vp8_info) {
  RTC_DCHECK_GT(num_layers_, 0);

  auto pending_frame = pending_frames_.find(rtp_timestamp);
  RTC_DCHECK(pending_frame != pending_frames_.end());

  if (size_bytes == 0) {
    pending_frames_.erase(pending_frame);
    return;
  }

  PendingFrame& frame = pending_frame->second;
  if (is_keyframe && checker_) {
    // Signal key-frame so checker resets state.
    RTC_DCHECK(checker_->CheckTemporalConfig(true, frame.frame_config));
  }

  if (num_layers_ == 1) {
    vp8_info->temporalIdx = kNoTemporalIdx;
    vp8_info->layerSync = false;
  } else {
    if (is_keyframe) {
      // Restart the temporal pattern on keyframes.
      pattern_idx_ = 0;
      vp8_info->temporalIdx = 0;
      vp8_info->layerSync = true;  // Keyframes are always sync frames.

      for (Vp8BufferReference buffer : kAllBuffers) {
        if (kf_buffers_.find(buffer) != kf_buffers_.end()) {
          // Update frame count of all kf-only buffers, regardless of state of
          // |pending_frames_|.
          frames_since_buffer_refresh_[buffer] = 0;
        } else {
          // Key-frames update all buffers, this should be reflected when
          // updating state in FrameEncoded().
          frame.updated_buffer_mask |= static_cast<uint8_t>(buffer);
        }
      }
    } else {
      // Delta frame, update codec specifics with temporal id and sync flag.
      vp8_info->temporalIdx = frame.frame_config.packetizer_temporal_idx;
      vp8_info->layerSync = frame.frame_config.layer_sync;
    }
  }

  if (!frame.expired) {
    for (Vp8BufferReference buffer : kAllBuffers) {
      if (frame.updated_buffer_mask & static_cast<uint8_t>(buffer)) {
        frames_since_buffer_refresh_[buffer] = 0;
      }
    }
  }
}

// Returns list of temporal dependencies for each frame in the temporal pattern.
// Values are lists of indecies in the pattern.
std::vector<std::set<uint8_t>> GetTemporalDependencies(
    int num_temporal_layers) {
  switch (num_temporal_layers) {
    case 1:
      return {{0}};
    case 2:
      if (!field_trial::IsDisabled("WebRTC-UseShortVP8TL2Pattern")) {
        return {{2}, {0}, {0}, {1, 2}};
      } else {
        return {{6}, {0}, {0}, {1, 2}, {2}, {3, 4}, {4}, {5, 6}};
      }
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
    int num_temporal_layers)
    : TemporalLayersChecker(num_temporal_layers),
      num_layers_(std::max(1, num_temporal_layers)),
      temporal_ids_(GetTemporalIds(num_layers_)),
      temporal_dependencies_(GetTemporalDependencies(num_layers_)),
      pattern_idx_(255) {
  int i = 0;
  while (temporal_ids_.size() < temporal_dependencies_.size()) {
    temporal_ids_.push_back(temporal_ids_[i++]);
  }
}

DefaultTemporalLayersChecker::~DefaultTemporalLayersChecker() = default;

bool DefaultTemporalLayersChecker::CheckTemporalConfig(
    bool frame_is_keyframe,
    const Vp8TemporalLayers::FrameConfig& frame_config) {
  if (!TemporalLayersChecker::CheckTemporalConfig(frame_is_keyframe,
                                                  frame_config)) {
    return false;
  }
  if (frame_config.drop_frame) {
    return true;
  }

  if (frame_is_keyframe) {
    pattern_idx_ = 0;
    last_ = BufferState();
    golden_ = BufferState();
    arf_ = BufferState();
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
      Vp8TemporalLayers::BufferFlags::kReference) {
    uint8_t referenced_layer = temporal_ids_[last_.pattern_idx];
    if (referenced_layer > 0) {
      need_sync = false;
    }
    if (!last_.is_keyframe) {
      dependencies.push_back(last_.pattern_idx);
    }
  } else if (frame_config.first_reference == Vp8BufferReference::kLast ||
             frame_config.second_reference == Vp8BufferReference::kLast) {
    RTC_LOG(LS_ERROR)
        << "Last buffer not referenced, but present in search order.";
    return false;
  }

  if (frame_config.arf_buffer_flags &
      Vp8TemporalLayers::BufferFlags::kReference) {
    uint8_t referenced_layer = temporal_ids_[arf_.pattern_idx];
    if (referenced_layer > 0) {
      need_sync = false;
    }
    if (!arf_.is_keyframe) {
      dependencies.push_back(arf_.pattern_idx);
    }
  } else if (frame_config.first_reference == Vp8BufferReference::kAltref ||
             frame_config.second_reference == Vp8BufferReference::kAltref) {
    RTC_LOG(LS_ERROR)
        << "Altret buffer not referenced, but present in search order.";
    return false;
  }

  if (frame_config.golden_buffer_flags &
      Vp8TemporalLayers::BufferFlags::kReference) {
    uint8_t referenced_layer = temporal_ids_[golden_.pattern_idx];
    if (referenced_layer > 0) {
      need_sync = false;
    }
    if (!golden_.is_keyframe) {
      dependencies.push_back(golden_.pattern_idx);
    }
  } else if (frame_config.first_reference == Vp8BufferReference::kGolden ||
             frame_config.second_reference == Vp8BufferReference::kGolden) {
    RTC_LOG(LS_ERROR)
        << "Golden buffer not referenced, but present in search order.";
    return false;
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

  if (frame_config.last_buffer_flags &
      Vp8TemporalLayers::BufferFlags::kUpdate) {
    last_.is_updated_this_cycle = true;
    last_.pattern_idx = pattern_idx_;
    last_.is_keyframe = false;
  }
  if (frame_config.arf_buffer_flags & Vp8TemporalLayers::BufferFlags::kUpdate) {
    arf_.is_updated_this_cycle = true;
    arf_.pattern_idx = pattern_idx_;
    arf_.is_keyframe = false;
  }
  if (frame_config.golden_buffer_flags &
      Vp8TemporalLayers::BufferFlags::kUpdate) {
    golden_.is_updated_this_cycle = true;
    golden_.pattern_idx = pattern_idx_;
    golden_.is_keyframe = false;
  }
  return true;
}

}  // namespace webrtc
