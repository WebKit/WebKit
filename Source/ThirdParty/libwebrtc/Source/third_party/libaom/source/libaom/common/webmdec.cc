/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "common/webmdec.h"

#include <cassert>
#include <cstring>
#include <cstdio>

#include "third_party/libwebm/mkvparser/mkvparser.h"
#include "third_party/libwebm/mkvparser/mkvreader.h"

namespace {

void reset(struct WebmInputContext *const webm_ctx) {
  if (webm_ctx->reader != NULL) {
    mkvparser::MkvReader *const reader =
        reinterpret_cast<mkvparser::MkvReader *>(webm_ctx->reader);
    delete reader;
  }
  if (webm_ctx->segment != NULL) {
    mkvparser::Segment *const segment =
        reinterpret_cast<mkvparser::Segment *>(webm_ctx->segment);
    delete segment;
  }
  if (webm_ctx->buffer != NULL) {
    delete[] webm_ctx->buffer;
  }
  webm_ctx->reader = NULL;
  webm_ctx->segment = NULL;
  webm_ctx->buffer = NULL;
  webm_ctx->cluster = NULL;
  webm_ctx->block_entry = NULL;
  webm_ctx->block = NULL;
  webm_ctx->block_frame_index = 0;
  webm_ctx->video_track_index = 0;
  webm_ctx->timestamp_ns = 0;
  webm_ctx->is_key_frame = false;
}

void get_first_cluster(struct WebmInputContext *const webm_ctx) {
  mkvparser::Segment *const segment =
      reinterpret_cast<mkvparser::Segment *>(webm_ctx->segment);
  const mkvparser::Cluster *const cluster = segment->GetFirst();
  webm_ctx->cluster = cluster;
}

void rewind_and_reset(struct WebmInputContext *const webm_ctx,
                      struct AvxInputContext *const aom_ctx) {
  rewind(aom_ctx->file);
  reset(webm_ctx);
}

}  // namespace

int file_is_webm(struct WebmInputContext *webm_ctx,
                 struct AvxInputContext *aom_ctx) {
  mkvparser::MkvReader *const reader = new mkvparser::MkvReader(aom_ctx->file);
  webm_ctx->reader = reader;
  webm_ctx->reached_eos = 0;

  mkvparser::EBMLHeader header;
  long long pos = 0;
  if (header.Parse(reader, pos) < 0) {
    rewind_and_reset(webm_ctx, aom_ctx);
    return 0;
  }

  mkvparser::Segment *segment;
  if (mkvparser::Segment::CreateInstance(reader, pos, segment)) {
    rewind_and_reset(webm_ctx, aom_ctx);
    return 0;
  }
  webm_ctx->segment = segment;
  if (segment->Load() < 0) {
    rewind_and_reset(webm_ctx, aom_ctx);
    return 0;
  }

  const mkvparser::Tracks *const tracks = segment->GetTracks();
  const mkvparser::VideoTrack *video_track = NULL;
  for (unsigned long i = 0; i < tracks->GetTracksCount(); ++i) {
    const mkvparser::Track *const track = tracks->GetTrackByIndex(i);
    if (track->GetType() == mkvparser::Track::kVideo) {
      video_track = static_cast<const mkvparser::VideoTrack *>(track);
      webm_ctx->video_track_index = static_cast<int>(track->GetNumber());
      break;
    }
  }

  if (video_track == NULL || video_track->GetCodecId() == NULL) {
    rewind_and_reset(webm_ctx, aom_ctx);
    return 0;
  }

  if (!strncmp(video_track->GetCodecId(), "V_AV1", 5)) {
    aom_ctx->fourcc = AV1_FOURCC;
  } else {
    rewind_and_reset(webm_ctx, aom_ctx);
    return 0;
  }

  aom_ctx->framerate.denominator = 0;
  aom_ctx->framerate.numerator = 0;
  aom_ctx->width = static_cast<uint32_t>(video_track->GetWidth());
  aom_ctx->height = static_cast<uint32_t>(video_track->GetHeight());

  get_first_cluster(webm_ctx);

  return 1;
}

int webm_read_frame(struct WebmInputContext *webm_ctx, uint8_t **buffer,
                    size_t *bytes_read, size_t *buffer_size) {
  assert(webm_ctx->buffer == *buffer);
  // This check is needed for frame parallel decoding, in which case this
  // function could be called even after it has reached end of input stream.
  if (webm_ctx->reached_eos) {
    return 1;
  }
  mkvparser::Segment *const segment =
      reinterpret_cast<mkvparser::Segment *>(webm_ctx->segment);
  const mkvparser::Cluster *cluster =
      reinterpret_cast<const mkvparser::Cluster *>(webm_ctx->cluster);
  const mkvparser::Block *block =
      reinterpret_cast<const mkvparser::Block *>(webm_ctx->block);
  const mkvparser::BlockEntry *block_entry =
      reinterpret_cast<const mkvparser::BlockEntry *>(webm_ctx->block_entry);
  bool block_entry_eos = false;
  do {
    long status = 0;
    bool get_new_block = false;
    if (block_entry == NULL && !block_entry_eos) {
      status = cluster->GetFirst(block_entry);
      get_new_block = true;
    } else if (block_entry_eos || block_entry->EOS()) {
      cluster = segment->GetNext(cluster);
      if (cluster == NULL || cluster->EOS()) {
        *bytes_read = 0;
        webm_ctx->reached_eos = 1;
        return 1;
      }
      status = cluster->GetFirst(block_entry);
      block_entry_eos = false;
      get_new_block = true;
    } else if (block == NULL ||
               webm_ctx->block_frame_index == block->GetFrameCount() ||
               block->GetTrackNumber() != webm_ctx->video_track_index) {
      status = cluster->GetNext(block_entry, block_entry);
      if (block_entry == NULL || block_entry->EOS()) {
        block_entry_eos = true;
        continue;
      }
      get_new_block = true;
    }
    if (status || block_entry == NULL) {
      return -1;
    }
    if (get_new_block) {
      block = block_entry->GetBlock();
      if (block == NULL) return -1;
      webm_ctx->block_frame_index = 0;
    }
  } while (block_entry_eos ||
           block->GetTrackNumber() != webm_ctx->video_track_index);

  webm_ctx->cluster = cluster;
  webm_ctx->block_entry = block_entry;
  webm_ctx->block = block;

  const mkvparser::Block::Frame &frame =
      block->GetFrame(webm_ctx->block_frame_index);
  ++webm_ctx->block_frame_index;
  if (frame.len > static_cast<long>(*buffer_size)) {
    delete[] * buffer;
    *buffer = new uint8_t[frame.len];
    webm_ctx->buffer = *buffer;
    if (*buffer == NULL) {
      return -1;
    }
    *buffer_size = frame.len;
  }
  *bytes_read = frame.len;
  webm_ctx->timestamp_ns = block->GetTime(cluster);
  webm_ctx->is_key_frame = block->IsKey();

  mkvparser::MkvReader *const reader =
      reinterpret_cast<mkvparser::MkvReader *>(webm_ctx->reader);
  return frame.Read(reader, *buffer) ? -1 : 0;
}

// Calculate the greatest common divisor between two numbers.
static int gcd(int a, int b) {
  int remainder;
  while (b > 0) {
    remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

int webm_guess_framerate(struct WebmInputContext *webm_ctx,
                         struct AvxInputContext *aom_ctx) {
  uint32_t i = 0;
  uint8_t *buffer = NULL;
  size_t buffer_size = 0;
  size_t bytes_read = 0;
  assert(webm_ctx->buffer == NULL);
  while (webm_ctx->timestamp_ns < 1000000000 && i < 50) {
    if (webm_read_frame(webm_ctx, &buffer, &bytes_read, &buffer_size)) {
      break;
    }
    ++i;
  }
  aom_ctx->framerate.numerator = (i - 1) * 1000000;
  aom_ctx->framerate.denominator =
      static_cast<int>(webm_ctx->timestamp_ns / 1000);
  // Fraction might be represented in large numbers, like 49000000/980000
  // for 50fps. Simplify as much as possible.
  int g = gcd(aom_ctx->framerate.numerator, aom_ctx->framerate.denominator);
  if (g != 0) {
    aom_ctx->framerate.numerator /= g;
    aom_ctx->framerate.denominator /= g;
  }

  delete[] buffer;
  webm_ctx->buffer = NULL;

  get_first_cluster(webm_ctx);
  webm_ctx->block = NULL;
  webm_ctx->block_entry = NULL;
  webm_ctx->block_frame_index = 0;
  webm_ctx->timestamp_ns = 0;
  webm_ctx->reached_eos = 0;

  return 0;
}

void webm_free(struct WebmInputContext *webm_ctx) { reset(webm_ctx); }
