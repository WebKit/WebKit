/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_coding/codecs/vp8/default_temporal_layers.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_common_types.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

namespace webrtc {

DefaultTemporalLayers::DefaultTemporalLayers(int numberOfTemporalLayers,
                                             uint8_t initial_tl0_pic_idx)
    : number_of_temporal_layers_(numberOfTemporalLayers),
      temporal_ids_length_(0),
      temporal_pattern_length_(0),
      tl0_pic_idx_(initial_tl0_pic_idx),
      pattern_idx_(255),
      timestamp_(0),
      last_base_layer_sync_(false) {
  assert(kMaxTemporalStreams >= numberOfTemporalLayers);
  memset(temporal_ids_, 0, sizeof(temporal_ids_));
  memset(temporal_pattern_, 0, sizeof(temporal_pattern_));
}

int DefaultTemporalLayers::CurrentLayerId() const {
  assert(temporal_ids_length_ > 0);
  int index = pattern_idx_ % temporal_ids_length_;
  assert(index >= 0);
  return temporal_ids_[index];
}

bool DefaultTemporalLayers::ConfigureBitrates(int bitrateKbit,
                                              int max_bitrate_kbit,
                                              int framerate,
                                              vpx_codec_enc_cfg_t* cfg) {
  switch (number_of_temporal_layers_) {
    case 0:
    case 1:
      temporal_ids_length_ = 1;
      temporal_ids_[0] = 0;
      cfg->ts_number_layers = number_of_temporal_layers_;
      cfg->ts_periodicity = temporal_ids_length_;
      cfg->ts_target_bitrate[0] = bitrateKbit;
      cfg->ts_rate_decimator[0] = 1;
      memcpy(cfg->ts_layer_id, temporal_ids_,
             sizeof(unsigned int) * temporal_ids_length_);
      temporal_pattern_length_ = 1;
      temporal_pattern_[0] = kTemporalUpdateLastRefAll;
      break;
    case 2:
      temporal_ids_length_ = 2;
      temporal_ids_[0] = 0;
      temporal_ids_[1] = 1;
      cfg->ts_number_layers = number_of_temporal_layers_;
      cfg->ts_periodicity = temporal_ids_length_;
      // Split stream 60% 40%.
      // Bitrate API for VP8 is the agregated bitrate for all lower layers.
      cfg->ts_target_bitrate[0] = bitrateKbit * kVp8LayerRateAlloction[1][0];
      cfg->ts_target_bitrate[1] = bitrateKbit;
      cfg->ts_rate_decimator[0] = 2;
      cfg->ts_rate_decimator[1] = 1;
      memcpy(cfg->ts_layer_id, temporal_ids_,
             sizeof(unsigned int) * temporal_ids_length_);
      temporal_pattern_length_ = 8;
      temporal_pattern_[0] = kTemporalUpdateLastAndGoldenRefAltRef;
      temporal_pattern_[1] = kTemporalUpdateGoldenWithoutDependencyRefAltRef;
      temporal_pattern_[2] = kTemporalUpdateLastRefAltRef;
      temporal_pattern_[3] = kTemporalUpdateGoldenRefAltRef;
      temporal_pattern_[4] = kTemporalUpdateLastRefAltRef;
      temporal_pattern_[5] = kTemporalUpdateGoldenRefAltRef;
      temporal_pattern_[6] = kTemporalUpdateLastRefAltRef;
      temporal_pattern_[7] = kTemporalUpdateNone;
      break;
    case 3:
      temporal_ids_length_ = 4;
      temporal_ids_[0] = 0;
      temporal_ids_[1] = 2;
      temporal_ids_[2] = 1;
      temporal_ids_[3] = 2;
      cfg->ts_number_layers = number_of_temporal_layers_;
      cfg->ts_periodicity = temporal_ids_length_;
      // Split stream 40% 20% 40%.
      // Bitrate API for VP8 is the agregated bitrate for all lower layers.
      cfg->ts_target_bitrate[0] = bitrateKbit * kVp8LayerRateAlloction[2][0];
      cfg->ts_target_bitrate[1] = bitrateKbit * kVp8LayerRateAlloction[2][1];
      cfg->ts_target_bitrate[2] = bitrateKbit;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, temporal_ids_,
             sizeof(unsigned int) * temporal_ids_length_);
      temporal_pattern_length_ = 8;
      temporal_pattern_[0] = kTemporalUpdateLastAndGoldenRefAltRef;
      temporal_pattern_[1] = kTemporalUpdateNoneNoRefGoldenRefAltRef;
      temporal_pattern_[2] = kTemporalUpdateGoldenWithoutDependencyRefAltRef;
      temporal_pattern_[3] = kTemporalUpdateNone;
      temporal_pattern_[4] = kTemporalUpdateLastRefAltRef;
      temporal_pattern_[5] = kTemporalUpdateNone;
      temporal_pattern_[6] = kTemporalUpdateGoldenRefAltRef;
      temporal_pattern_[7] = kTemporalUpdateNone;
      break;
    case 4:
      temporal_ids_length_ = 8;
      temporal_ids_[0] = 0;
      temporal_ids_[1] = 3;
      temporal_ids_[2] = 2;
      temporal_ids_[3] = 3;
      temporal_ids_[4] = 1;
      temporal_ids_[5] = 3;
      temporal_ids_[6] = 2;
      temporal_ids_[7] = 3;
      // Split stream 25% 15% 20% 40%.
      // Bitrate API for VP8 is the agregated bitrate for all lower layers.
      cfg->ts_number_layers = 4;
      cfg->ts_periodicity = temporal_ids_length_;
      cfg->ts_target_bitrate[0] = bitrateKbit * kVp8LayerRateAlloction[3][0];
      cfg->ts_target_bitrate[1] = bitrateKbit * kVp8LayerRateAlloction[3][1];
      cfg->ts_target_bitrate[2] = bitrateKbit * kVp8LayerRateAlloction[3][2];
      cfg->ts_target_bitrate[3] = bitrateKbit;
      cfg->ts_rate_decimator[0] = 8;
      cfg->ts_rate_decimator[1] = 4;
      cfg->ts_rate_decimator[2] = 2;
      cfg->ts_rate_decimator[3] = 1;
      memcpy(cfg->ts_layer_id, temporal_ids_,
             sizeof(unsigned int) * temporal_ids_length_);
      temporal_pattern_length_ = 16;
      temporal_pattern_[0] = kTemporalUpdateLast;
      temporal_pattern_[1] = kTemporalUpdateNone;
      temporal_pattern_[2] = kTemporalUpdateAltrefWithoutDependency;
      temporal_pattern_[3] = kTemporalUpdateNone;
      temporal_pattern_[4] = kTemporalUpdateGoldenWithoutDependency;
      temporal_pattern_[5] = kTemporalUpdateNone;
      temporal_pattern_[6] = kTemporalUpdateAltref;
      temporal_pattern_[7] = kTemporalUpdateNone;
      temporal_pattern_[8] = kTemporalUpdateLast;
      temporal_pattern_[9] = kTemporalUpdateNone;
      temporal_pattern_[10] = kTemporalUpdateAltref;
      temporal_pattern_[11] = kTemporalUpdateNone;
      temporal_pattern_[12] = kTemporalUpdateGolden;
      temporal_pattern_[13] = kTemporalUpdateNone;
      temporal_pattern_[14] = kTemporalUpdateAltref;
      temporal_pattern_[15] = kTemporalUpdateNone;
      break;
    default:
      assert(false);
      return false;
  }
  return true;
}

int DefaultTemporalLayers::EncodeFlags(uint32_t timestamp) {
  assert(number_of_temporal_layers_ > 0);
  assert(kMaxTemporalPattern >= temporal_pattern_length_);
  assert(0 < temporal_pattern_length_);
  int flags = 0;
  int patternIdx = ++pattern_idx_ % temporal_pattern_length_;
  assert(kMaxTemporalPattern >= patternIdx);
  switch (temporal_pattern_[patternIdx]) {
    case kTemporalUpdateLast:
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_REF_GF;
      flags |= VP8_EFLAG_NO_REF_ARF;
      break;
    case kTemporalUpdateGoldenWithoutDependency:
      flags |= VP8_EFLAG_NO_REF_GF;
      // Deliberately no break here.
      FALLTHROUGH();
    case kTemporalUpdateGolden:
      flags |= VP8_EFLAG_NO_REF_ARF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kTemporalUpdateAltrefWithoutDependency:
      flags |= VP8_EFLAG_NO_REF_ARF;
      flags |= VP8_EFLAG_NO_REF_GF;
      // Deliberately no break here.
      FALLTHROUGH();
    case kTemporalUpdateAltref:
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kTemporalUpdateNoneNoRefAltref:
      flags |= VP8_EFLAG_NO_REF_ARF;
      // Deliberately no break here.
      FALLTHROUGH();
    case kTemporalUpdateNone:
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      flags |= VP8_EFLAG_NO_UPD_ENTROPY;
      break;
    case kTemporalUpdateNoneNoRefGoldenRefAltRef:
      flags |= VP8_EFLAG_NO_REF_GF;
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      flags |= VP8_EFLAG_NO_UPD_ENTROPY;
      break;
    case kTemporalUpdateGoldenWithoutDependencyRefAltRef:
      flags |= VP8_EFLAG_NO_REF_GF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kTemporalUpdateLastRefAltRef:
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_REF_GF;
      break;
    case kTemporalUpdateGoldenRefAltRef:
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kTemporalUpdateLastAndGoldenRefAltRef:
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_REF_GF;
      break;
    case kTemporalUpdateLastRefAll:
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_GF;
      break;
  }
  return flags;
}

void DefaultTemporalLayers::PopulateCodecSpecific(
    bool base_layer_sync,
    CodecSpecificInfoVP8* vp8_info,
    uint32_t timestamp) {
  assert(number_of_temporal_layers_ > 0);
  assert(0 < temporal_ids_length_);

  if (number_of_temporal_layers_ == 1) {
    vp8_info->temporalIdx = kNoTemporalIdx;
    vp8_info->layerSync = false;
    vp8_info->tl0PicIdx = kNoTl0PicIdx;
  } else {
    if (base_layer_sync) {
      vp8_info->temporalIdx = 0;
      vp8_info->layerSync = true;
    } else {
      vp8_info->temporalIdx = CurrentLayerId();
      TemporalReferences temporal_reference =
          temporal_pattern_[pattern_idx_ % temporal_pattern_length_];

      if (temporal_reference == kTemporalUpdateAltrefWithoutDependency ||
          temporal_reference == kTemporalUpdateGoldenWithoutDependency ||
          temporal_reference ==
              kTemporalUpdateGoldenWithoutDependencyRefAltRef ||
          temporal_reference == kTemporalUpdateNoneNoRefGoldenRefAltRef ||
          (temporal_reference == kTemporalUpdateNone &&
           number_of_temporal_layers_ == 4)) {
        vp8_info->layerSync = true;
      } else {
        vp8_info->layerSync = false;
      }
    }
    if (last_base_layer_sync_ && vp8_info->temporalIdx != 0) {
      // Regardless of pattern the frame after a base layer sync will always
      // be a layer sync.
      vp8_info->layerSync = true;
    }
    if (vp8_info->temporalIdx == 0 && timestamp != timestamp_) {
      timestamp_ = timestamp;
      tl0_pic_idx_++;
    }
    last_base_layer_sync_ = base_layer_sync;
    vp8_info->tl0PicIdx = tl0_pic_idx_;
  }
}

TemporalLayers* TemporalLayersFactory::Create(
    int temporal_layers,
    uint8_t initial_tl0_pic_idx) const {
  return new DefaultTemporalLayers(temporal_layers, initial_tl0_pic_idx);
}
}  // namespace webrtc
