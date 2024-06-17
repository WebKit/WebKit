/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*
 * See build_av1_dec_fuzzer.sh for building instructions.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <memory>
#include "config/aom_config.h"
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "aom_ports/mem_ops.h"

#define IVF_FRAME_HDR_SZ (4 + 8) /* 4 byte size + 8 byte timestamp */
#define IVF_FILE_HDR_SZ 32

extern "C" void usage_exit(void) { exit(EXIT_FAILURE); }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size <= IVF_FILE_HDR_SZ) {
    return 0;
  }

  // Abusing the four unused bytes at the end of the IVF file header as a source
  // of random bits.
  unsigned int tile_mode = (data[IVF_FILE_HDR_SZ - 1] & 2) != 0;
  unsigned int ext_tile_debug = (data[IVF_FILE_HDR_SZ - 1] & 4) != 0;
  unsigned int is_annexb = (data[IVF_FILE_HDR_SZ - 1] & 8) != 0;
  int output_all_layers = (data[IVF_FILE_HDR_SZ - 1] & 0x10) != 0;
  int operating_point = data[IVF_FILE_HDR_SZ - 2] & 0x1F;

  aom_codec_iface_t *codec_interface = aom_codec_av1_dx();
  aom_codec_ctx_t codec;
  // Set thread count in the range [1, 64].
  const unsigned int threads = (data[IVF_FILE_HDR_SZ] & 0x3f) + 1;
  aom_codec_dec_cfg_t cfg = { threads, 0, 0, !FORCE_HIGHBITDEPTH_DECODING };
  if (aom_codec_dec_init(&codec, codec_interface, &cfg, 0)) {
    return 0;
  }
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1_SET_TILE_MODE, tile_mode);
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_EXT_TILE_DEBUG, ext_tile_debug);
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_SET_IS_ANNEXB, is_annexb);
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_SET_OUTPUT_ALL_LAYERS,
                                output_all_layers);
  AOM_CODEC_CONTROL_TYPECHECKED(&codec, AV1D_SET_OPERATING_POINT,
                                operating_point);

  data += IVF_FILE_HDR_SZ;
  size -= IVF_FILE_HDR_SZ;

  while (size > IVF_FRAME_HDR_SZ) {
    size_t frame_size = mem_get_le32(data);
    size -= IVF_FRAME_HDR_SZ;
    data += IVF_FRAME_HDR_SZ;
    frame_size = std::min(size, frame_size);

    const aom_codec_err_t err =
        aom_codec_decode(&codec, data, frame_size, nullptr);
    static_cast<void>(err);
    aom_codec_iter_t iter = nullptr;
    aom_image_t *img = nullptr;
    while ((img = aom_codec_get_frame(&codec, &iter)) != nullptr) {
    }
    data += frame_size;
    size -= frame_size;
  }
  aom_codec_destroy(&codec);
  return 0;
}
