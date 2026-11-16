/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*
 * See build_av1_enc_fuzzer.sh for building instructions.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom/aom_image.h"
#include "aom/aomcx.h"

namespace {

constexpr unsigned int kMaxDimension = 1024;
constexpr size_t kMinHeaderSize = 16;

// Bit masks derived from the mode_flags byte. The low two bits are used to
// select the encoding usage (see PickUsage).
enum ModeFlag {
  kLossless = 1u << 2,
  kRowMt = 1u << 3,
  kUseSecondResolution = 1u << 4,
  kKeyFrameSecond = 1u << 5,
  kErrorResilient = 1u << 6,
};

// Number of frames to encode for a given cpu_used value. Lower cpu_used
// values encode more slowly, so fewer frames are used to stay within the
// fuzzer's execution time budget (libFuzzer defaults to one second per
// input). This can be refined based on measured fuzzer performance.
constexpr unsigned int kNumFramesForCpuUsed[12] = {
  50,  // cpu_used 0
  50,  // cpu_used 1
  50,  // cpu_used 2
  60,  // cpu_used 3
  60,  // cpu_used 4
  70,  // cpu_used 5
  70,  // cpu_used 6
  80,  // cpu_used 7
  80,  // cpu_used 8
  90,  // cpu_used 9
  90,  // cpu_used 10
  100  // cpu_used 11
};

struct FuzzReader {
  const uint8_t *data;
  size_t size;
};

uint8_t ReadU8(FuzzReader *reader) {
  if (reader->size == 0) return 0;
  const uint8_t value = *reader->data++;
  --reader->size;
  return value;
}

uint16_t ReadU16(FuzzReader *reader) {
  if (reader->size < 2) return 0;
  const uint16_t value = reader->data[0] | (reader->data[1] << 8);
  reader->data += 2;
  reader->size -= 2;
  return value;
}

unsigned int UsageToIndex(unsigned int usage) {
  switch (usage) {
    case AOM_USAGE_GOOD_QUALITY: return 0;
    case AOM_USAGE_REALTIME: return 1;
    case AOM_USAGE_ALL_INTRA: return 2;
    default: return 0;
  }
}

unsigned int PickUsage(uint8_t raw) {
  switch (raw % 3) {
    case 0: return AOM_USAGE_GOOD_QUALITY;
    case 1: return AOM_USAGE_REALTIME;
    default: return AOM_USAGE_ALL_INTRA;
  }
}

aom_rc_mode PickRcMode(uint8_t raw) {
  switch (raw % 4) {
    case 0: return AOM_VBR;
    case 1: return AOM_CBR;
    case 2: return AOM_CQ;
    default: return AOM_Q;
  }
}

unsigned int PickDimension(uint16_t raw) { return 1u + (raw % kMaxDimension); }

int PickCpuUsed(unsigned int usage, uint8_t raw) {
  const int max_cpu_used = usage == AOM_USAGE_REALTIME ? 11 : 9;
  return raw % (max_cpu_used + 1);
}

bool DrainPackets(aom_codec_ctx_t *codec) {
  bool got_data = false;
  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(codec, &iter) != nullptr) {
    got_data = true;
  }
  return got_data;
}

bool InitDefaultConfig(aom_codec_iface_t *iface, unsigned int usage,
                       aom_codec_enc_cfg_t *cfg) {
  const unsigned int usages[3] = { AOM_USAGE_GOOD_QUALITY, AOM_USAGE_REALTIME,
                                   AOM_USAGE_ALL_INTRA };
  const unsigned int requested_index = UsageToIndex(usage);
  for (unsigned int i = 0; i < 3; ++i) {
    const unsigned int index = (requested_index + i) % 3;
    if (aom_codec_enc_config_default(iface, cfg, usages[index]) ==
        AOM_CODEC_OK) {
      return true;
    }
  }
  return false;
}

// Deterministic PRNG so that the frame content is derived from the fuzzer
// input without requiring the reader to supply a full plane-sized chunk of
// data for every frame.
uint32_t Rand(uint32_t *state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

void FillPlane(uint8_t *plane, int stride, unsigned int width,
               unsigned int height, uint32_t *state) {
  for (unsigned int row = 0; row < height; ++row) {
    for (unsigned int col = 0; col < width; ++col) {
      plane[static_cast<size_t>(row) * stride + col] =
          static_cast<uint8_t>(Rand(state) >> 24);
    }
  }
}

bool BuildImage(FuzzReader *reader, unsigned int width, unsigned int height,
                aom_image_t *image, unsigned int frame_index) {
  if (aom_img_alloc(image, AOM_IMG_FMT_I420, width, height, 1) == nullptr) {
    return false;
  }

  // Seed a deterministic PRNG from the fuzzer input and the frame index so
  // that each encoded frame differs without consuming a plane-sized chunk of
  // input per frame.
  uint32_t state = 0;
  for (unsigned int i = 0; i < 4; ++i) {
    state = (state << 8) | ReadU8(reader);
  }
  const uint32_t golden_ratio = 0x9e3779b9u;
  state ^= frame_index * golden_ratio;

  const unsigned int uv_width = (width + 1) / 2;
  const unsigned int uv_height = (height + 1) / 2;
  FillPlane(image->planes[AOM_PLANE_Y], image->stride[AOM_PLANE_Y], width,
            height, &state);
  FillPlane(image->planes[AOM_PLANE_U], image->stride[AOM_PLANE_U], uv_width,
            uv_height, &state);
  FillPlane(image->planes[AOM_PLANE_V], image->stride[AOM_PLANE_V], uv_width,
            uv_height, &state);
  return true;
}

void ApplyControls(aom_codec_ctx_t *codec, unsigned int usage,
                   uint8_t mode_flags, uint8_t cpu_used_raw, uint8_t ctl0,
                   uint8_t ctl1) {
  const int cpu_used = PickCpuUsed(usage, cpu_used_raw);
  const unsigned int lossless = (mode_flags & kLossless) != 0;
  const unsigned int row_mt = (mode_flags & kRowMt) != 0;
  const unsigned int aq_mode = ctl0 % 4;
  const unsigned int deltaq_mode = ctl1 % 4;
  const unsigned int tile_columns = (ctl0 >> 4) & 0x3;
  const unsigned int tile_rows = (ctl1 >> 4) & 0x3;
  const unsigned int enable_cdef = (ctl0 >> 2) & 1;
  const unsigned int enable_restoration = (ctl1 >> 2) & 1;

  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AOME_SET_CPUUSED, cpu_used);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_LOSSLESS, lossless);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_ROW_MT, row_mt);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_AQ_MODE, aq_mode);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_DELTAQ_MODE, deltaq_mode);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_TILE_COLUMNS,
                                      tile_columns);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_TILE_ROWS, tile_rows);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_ENABLE_CDEF, enable_cdef);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_ENABLE_RESTORATION,
                                      enable_restoration);
}

bool EncodeFrame(aom_codec_ctx_t *codec, FuzzReader *reader, unsigned int width,
                 unsigned int height, aom_codec_pts_t pts,
                 aom_enc_frame_flags_t flags) {
  aom_image_t image;
  memset(&image, 0, sizeof(image));

  if (!BuildImage(reader, width, height, &image, pts)) {
    return false;
  }

  const aom_codec_err_t enc_ret =
      aom_codec_encode(codec, &image, pts, 1, flags);
  if (enc_ret == AOM_CODEC_OK) {
    (void)DrainPackets(codec);
  }

  aom_img_free(&image);
  return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  if (iface == nullptr || size < kMinHeaderSize) return 0;

  FuzzReader reader = { data, size };
  const uint8_t mode_flags = ReadU8(&reader);
  const uint8_t cpu_used_raw = ReadU8(&reader);
  const uint8_t ctl0 = ReadU8(&reader);
  const uint8_t ctl1 = ReadU8(&reader);
  const unsigned int usage = PickUsage(mode_flags);
  const unsigned int width0 = PickDimension(ReadU16(&reader));
  const unsigned int height0 = PickDimension(ReadU16(&reader));
  const unsigned int bitrate = 1u + (ReadU16(&reader) % 4000u);
  const unsigned int width1 = PickDimension(ReadU16(&reader));
  const unsigned int height1 = PickDimension(ReadU16(&reader));
  const unsigned int threads0 = ReadU8(&reader) % 9;
  const unsigned int threads1 = ReadU8(&reader) % 9;

  aom_codec_enc_cfg_t cfg;
  if (!InitDefaultConfig(iface, usage, &cfg)) return 0;

  cfg.g_usage = usage;
  cfg.g_w = width0;
  cfg.g_h = height0;
  cfg.g_threads = threads0;
  cfg.g_forced_max_frame_width = kMaxDimension;
  cfg.g_forced_max_frame_height = kMaxDimension;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1000000;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.g_error_resilient =
      (mode_flags & kErrorResilient) ? AOM_ERROR_RESILIENT_DEFAULT : 0;
  cfg.rc_end_usage = PickRcMode(ctl0);
  cfg.rc_target_bitrate = bitrate;

  aom_codec_ctx_t codec;
  memset(&codec, 0, sizeof(codec));
  if (aom_codec_enc_init(&codec, iface, &cfg, 0) != AOM_CODEC_OK) return 0;

  const int cpu_used = PickCpuUsed(usage, cpu_used_raw);
  ApplyControls(&codec, usage, mode_flags, cpu_used_raw, ctl0, ctl1);

  const unsigned int num_frames = kNumFramesForCpuUsed[cpu_used];
  bool config_failed = false;
  for (unsigned int frame = 0; frame < num_frames; ++frame) {
    aom_enc_frame_flags_t flags = 0;
    if (frame == 1 && (mode_flags & kKeyFrameSecond)) {
      flags |= AOM_EFLAG_FORCE_KF;
    }

    unsigned int width = width0;
    unsigned int height = height0;
    if (mode_flags & kUseSecondResolution) {
      // Vary the switch frame based on the input so different encodings
      // exercise the resolution change at different points in the stream.
      // num_frames >= 50 for every cpu_used, so switch_frame and
      // restore_frame stay within [1, num_frames - 1].
      const unsigned int switch_frame = 1u + (ctl0 % (num_frames / 4u));
      const unsigned int restore_frame =
          switch_frame + 1u + (ctl1 % (num_frames / 4u));
      if (frame == switch_frame) {
        cfg.g_w = width1;
        cfg.g_h = height1;
        cfg.g_threads = threads1;
        cfg.rc_end_usage = PickRcMode(ctl1);
        if (aom_codec_enc_config_set(&codec, &cfg) != AOM_CODEC_OK) {
          config_failed = true;
          break;
        }
        width = width1;
        height = height1;
      } else if (frame == restore_frame) {
        cfg.g_w = width0;
        cfg.g_h = height0;
        cfg.g_threads = threads0;
        cfg.rc_end_usage = PickRcMode(ctl0);
        if (aom_codec_enc_config_set(&codec, &cfg) != AOM_CODEC_OK) {
          config_failed = true;
          break;
        }
        width = width0;
        height = height0;
      }
    }

    if (!EncodeFrame(&codec, &reader, width, height, frame, flags)) {
      config_failed = true;
      break;
    }
  }

  if (!config_failed) {
    while (aom_codec_encode(&codec, nullptr, 0, 0, 0) == AOM_CODEC_OK &&
           DrainPackets(&codec)) {
    }
  }

  (void)aom_codec_destroy(&codec);
  return 0;
}
