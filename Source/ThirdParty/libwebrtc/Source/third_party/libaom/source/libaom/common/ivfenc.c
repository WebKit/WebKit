/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "common/ivfenc.h"

#include "aom/aom_encoder.h"
#include "aom_ports/mem_ops.h"

void ivf_write_file_header(FILE *outfile, const struct aom_codec_enc_cfg *cfg,
                           unsigned int fourcc, int frame_cnt) {
  char header[32];

  header[0] = 'D';
  header[1] = 'K';
  header[2] = 'I';
  header[3] = 'F';
  mem_put_le16(header + 4, 0);                     // version
  mem_put_le16(header + 6, 32);                    // header size
  mem_put_le32(header + 8, fourcc);                // fourcc
  mem_put_le16(header + 12, cfg->g_w);             // width
  mem_put_le16(header + 14, cfg->g_h);             // height
  mem_put_le32(header + 16, cfg->g_timebase.den);  // rate
  mem_put_le32(header + 20, cfg->g_timebase.num);  // scale
  mem_put_le32(header + 24, frame_cnt);            // length
  mem_put_le32(header + 28, 0);                    // unused

  fwrite(header, 1, 32, outfile);
}

void ivf_write_frame_header(FILE *outfile, int64_t pts, size_t frame_size) {
  char header[12];

  mem_put_le32(header, (int)frame_size);
  mem_put_le32(header + 4, (int)(pts & 0xFFFFFFFF));
  mem_put_le32(header + 8, (int)(pts >> 32));
  fwrite(header, 1, 12, outfile);
}

void ivf_write_frame_size(FILE *outfile, size_t frame_size) {
  char header[4];

  mem_put_le32(header, (int)frame_size);
  fwrite(header, 1, 4, outfile);
}
