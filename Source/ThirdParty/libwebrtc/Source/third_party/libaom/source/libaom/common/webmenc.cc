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

#include "common/webmenc.h"

#include <stdio.h>
#include <string.h>

#include <memory>
#include <new>
#include <string>

#include "common/av1_config.h"
#include "third_party/libwebm/mkvmuxer/mkvmuxer.h"
#include "third_party/libwebm/mkvmuxer/mkvmuxerutil.h"
#include "third_party/libwebm/mkvmuxer/mkvwriter.h"

namespace {
const uint64_t kDebugTrackUid = 0xDEADBEEF;
const int kVideoTrackNumber = 1;

// Simplistic mechanism to detect if an argv parameter refers to
// an input or output file. Returns the total number of arguments that
// should be skipped.
int skip_input_output_arg(const char *arg, const char *input_fname) {
  if (strcmp(arg, input_fname) == 0) {
    return 1;
  }
  if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0) {
    return 2;
  }
  if (strncmp(arg, "--output=", strlen("--output=")) == 0) {
    return 1;
  }
  return 0;
}

}  // namespace

char *extract_encoder_settings(const char *version, const char **argv, int argc,
                               const char *input_fname) {
  // + 9 for "version:" prefix and for null terminator.
  size_t total_size = strlen(version) + 9;
  int i = 1;
  while (i < argc) {
    int num_skip = skip_input_output_arg(argv[i], input_fname);
    i += num_skip;
    if (num_skip == 0) {
      total_size += strlen(argv[i]) + 1;  // + 1 is for space separator.
      ++i;
    }
  }
  char *result = static_cast<char *>(malloc(total_size));
  if (result == nullptr) {
    return nullptr;
  }
  char *cur = result;
  cur += snprintf(cur, total_size, "version:%s", version);
  i = 1;
  while (i < argc) {
    int num_skip = skip_input_output_arg(argv[i], input_fname);
    i += num_skip;
    if (num_skip == 0) {
      cur += snprintf(cur, total_size, " %s", argv[i]);
      ++i;
    }
  }
  *cur = '\0';
  return result;
}

int write_webm_file_header(struct WebmOutputContext *webm_ctx,
                           aom_codec_ctx_t *encoder_ctx,
                           const aom_codec_enc_cfg_t *cfg,
                           stereo_format_t stereo_fmt, unsigned int fourcc,
                           const struct AvxRational *par,
                           const char *encoder_settings) {
  std::unique_ptr<mkvmuxer::MkvWriter> writer(
      new (std::nothrow) mkvmuxer::MkvWriter(webm_ctx->stream));
  std::unique_ptr<mkvmuxer::Segment> segment(new (std::nothrow)
                                                 mkvmuxer::Segment());
  if (writer == nullptr || segment == nullptr) {
    fprintf(stderr, "webmenc> mkvmuxer objects alloc failed, out of memory?\n");
    return -1;
  }

  bool ok = segment->Init(writer.get());
  if (!ok) {
    fprintf(stderr, "webmenc> mkvmuxer Init failed.\n");
    return -1;
  }

  segment->set_mode(mkvmuxer::Segment::kFile);
  segment->OutputCues(true);

  mkvmuxer::SegmentInfo *const info = segment->GetSegmentInfo();
  if (!info) {
    fprintf(stderr, "webmenc> Cannot retrieve Segment Info.\n");
    return -1;
  }

  const uint64_t kTimecodeScale = 1000000;
  info->set_timecode_scale(kTimecodeScale);
  std::string version = "aomenc";
  if (!webm_ctx->debug) {
    version.append(std::string(" ") + aom_codec_version_str());
  }
  info->set_writing_app(version.c_str());

  const uint64_t video_track_id =
      segment->AddVideoTrack(static_cast<int>(cfg->g_w),
                             static_cast<int>(cfg->g_h), kVideoTrackNumber);
  mkvmuxer::VideoTrack *const video_track = static_cast<mkvmuxer::VideoTrack *>(
      segment->GetTrackByNumber(video_track_id));

  if (!video_track) {
    fprintf(stderr, "webmenc> Video track creation failed.\n");
    return -1;
  }

  ok = false;
  aom_fixed_buf_t *obu_sequence_header =
      aom_codec_get_global_headers(encoder_ctx);
  if (obu_sequence_header) {
    Av1Config av1_config;
    if (get_av1config_from_obu(
            reinterpret_cast<const uint8_t *>(obu_sequence_header->buf),
            obu_sequence_header->sz, false, &av1_config) == 0) {
      uint8_t av1_config_buffer[4] = { 0 };
      size_t bytes_written = 0;
      if (write_av1config(&av1_config, sizeof(av1_config_buffer),
                          &bytes_written, av1_config_buffer) == 0) {
        ok = video_track->SetCodecPrivate(av1_config_buffer,
                                          sizeof(av1_config_buffer));
      }
    }
    free(obu_sequence_header->buf);
    free(obu_sequence_header);
  }
  if (!ok) {
    fprintf(stderr, "webmenc> Unable to set AV1 config.\n");
    return -1;
  }

  ok = video_track->SetStereoMode(stereo_fmt);
  if (!ok) {
    fprintf(stderr, "webmenc> Unable to set stereo mode.\n");
    return -1;
  }

  if (fourcc != AV1_FOURCC) {
    fprintf(stderr, "webmenc> Unsupported codec (unknown 4 CC).\n");
    return -1;
  }
  video_track->set_codec_id("V_AV1");

  if (par->numerator > 1 || par->denominator > 1) {
    // TODO(fgalligan): Add support of DisplayUnit, Display Aspect Ratio type
    // to WebM format.
    const uint64_t display_width = static_cast<uint64_t>(
        ((cfg->g_w * par->numerator * 1.0) / par->denominator) + .5);
    video_track->set_display_width(display_width);
    video_track->set_display_height(cfg->g_h);
  }

  if (encoder_settings != nullptr) {
    mkvmuxer::Tag *tag = segment->AddTag();
    if (tag == nullptr) {
      fprintf(stderr,
              "webmenc> Unable to allocate memory for encoder settings tag.\n");
      return -1;
    }
    ok = tag->add_simple_tag("ENCODER_SETTINGS", encoder_settings);
    if (!ok) {
      fprintf(stderr,
              "webmenc> Unable to allocate memory for encoder settings tag.\n");
      return -1;
    }
  }

  if (webm_ctx->debug) {
    video_track->set_uid(kDebugTrackUid);
  }

  webm_ctx->writer = writer.release();
  webm_ctx->segment = segment.release();
  return 0;
}

int write_webm_block(struct WebmOutputContext *webm_ctx,
                     const aom_codec_enc_cfg_t *cfg,
                     const aom_codec_cx_pkt_t *pkt) {
  if (!webm_ctx->segment) {
    fprintf(stderr, "webmenc> segment is NULL.\n");
    return -1;
  }
  mkvmuxer::Segment *const segment =
      reinterpret_cast<mkvmuxer::Segment *>(webm_ctx->segment);
  int64_t pts_ns = pkt->data.frame.pts * 1000000000ll * cfg->g_timebase.num /
                   cfg->g_timebase.den;
  if (pts_ns <= webm_ctx->last_pts_ns) pts_ns = webm_ctx->last_pts_ns + 1000000;
  webm_ctx->last_pts_ns = pts_ns;

  if (!segment->AddFrame(static_cast<uint8_t *>(pkt->data.frame.buf),
                         pkt->data.frame.sz, kVideoTrackNumber, pts_ns,
                         pkt->data.frame.flags & AOM_FRAME_IS_KEY)) {
    fprintf(stderr, "webmenc> AddFrame failed.\n");
    return -1;
  }
  return 0;
}

int write_webm_file_footer(struct WebmOutputContext *webm_ctx) {
  if (!webm_ctx->writer || !webm_ctx->segment) {
    fprintf(stderr, "webmenc> segment or writer NULL.\n");
    return -1;
  }
  mkvmuxer::MkvWriter *const writer =
      reinterpret_cast<mkvmuxer::MkvWriter *>(webm_ctx->writer);
  mkvmuxer::Segment *const segment =
      reinterpret_cast<mkvmuxer::Segment *>(webm_ctx->segment);
  const bool ok = segment->Finalize();
  delete segment;
  delete writer;
  webm_ctx->writer = NULL;
  webm_ctx->segment = NULL;

  if (!ok) {
    fprintf(stderr, "webmenc> Segment::Finalize failed.\n");
    return -1;
  }

  return 0;
}
