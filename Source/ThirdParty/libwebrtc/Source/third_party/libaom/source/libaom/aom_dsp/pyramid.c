/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_dsp/pyramid.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/bitops.h"
#include "aom_util/aom_thread.h"

// TODO(rachelbarker): Move needed code from av1/ to aom_dsp/
#include "av1/common/resize.h"

#include <assert.h>
#include <string.h>

// Lifecycle:
// * Frame buffer alloc code calls aom_get_pyramid_alloc_size()
//   to work out how much space is needed for a given number of pyramid
//   levels. This is counted in the size checked against the max allocation
//   limit
// * Then calls aom_alloc_pyramid() to actually create the pyramid
// * Pyramid is initially marked as invalid (no data)
// * Whenever pyramid is needed, we check the valid flag. If set, use existing
//   data. If not set, compute full pyramid
// * Whenever frame buffer is reused, clear the valid flag
// * Whenever frame buffer is resized, reallocate pyramid

size_t aom_get_pyramid_alloc_size(int width, int height, int n_levels,
                                  bool image_is_16bit) {
  // Limit number of levels on small frames
  const int msb = get_msb(AOMMIN(width, height));
  const int max_levels = AOMMAX(msb - MIN_PYRAMID_SIZE_LOG2, 1);
  n_levels = AOMMIN(n_levels, max_levels);

  size_t alloc_size = 0;
  alloc_size += sizeof(ImagePyramid);
  alloc_size += n_levels * sizeof(PyramidLayer);

  // Calculate how much memory is needed for downscaled frame buffers
  size_t buffer_size = 0;

  // Work out if we need to allocate a few extra bytes for alignment.
  // aom_memalign() will ensure that the start of the allocation is aligned
  // to a multiple of PYRAMID_ALIGNMENT. But we want the first image pixel
  // to be aligned, not the first byte of the allocation.
  //
  // In the loop below, we ensure that the stride of every image is a multiple
  // of PYRAMID_ALIGNMENT. Thus the allocated size of each pyramid level will
  // also be a multiple of PYRAMID_ALIGNMENT. Thus, as long as we can get the
  // first pixel in the first pyramid layer aligned properly, that will
  // automatically mean that the first pixel of every row of every layer is
  // properly aligned too.
  //
  // Thus all we need to consider is the first pixel in the first layer.
  // This is located at offset
  //   extra_bytes + level_stride * PYRAMID_PADDING + PYRAMID_PADDING
  // bytes into the buffer. Since level_stride is a multiple of
  // PYRAMID_ALIGNMENT, we can ignore that. So we need
  //   extra_bytes + PYRAMID_PADDING = multiple of PYRAMID_ALIGNMENT
  //
  // To solve this, we can round PYRAMID_PADDING up to the next multiple
  // of PYRAMID_ALIGNMENT, then subtract the orginal value to calculate
  // how many extra bytes are needed.
  size_t first_px_offset =
      (PYRAMID_PADDING + PYRAMID_ALIGNMENT - 1) & ~(PYRAMID_ALIGNMENT - 1);
  size_t extra_bytes = first_px_offset - PYRAMID_PADDING;
  buffer_size += extra_bytes;

  // If the original image is stored in an 8-bit buffer, then we can point the
  // lowest pyramid level at that buffer rather than allocating a new one.
  int first_allocated_level = image_is_16bit ? 0 : 1;

  for (int level = first_allocated_level; level < n_levels; level++) {
    int level_width = width >> level;
    int level_height = height >> level;

    // Allocate padding for each layer
    int padded_width = level_width + 2 * PYRAMID_PADDING;
    int padded_height = level_height + 2 * PYRAMID_PADDING;

    // Align the layer stride to be a multiple of PYRAMID_ALIGNMENT
    // This ensures that, as long as the top-left pixel in this pyramid level is
    // properly aligned, then so will the leftmost pixel in every row of the
    // pyramid level.
    int level_stride =
        (padded_width + PYRAMID_ALIGNMENT - 1) & ~(PYRAMID_ALIGNMENT - 1);

    buffer_size += level_stride * padded_height;
  }

  alloc_size += buffer_size;

  return alloc_size;
}

ImagePyramid *aom_alloc_pyramid(int width, int height, int n_levels,
                                bool image_is_16bit) {
  // Limit number of levels on small frames
  const int msb = get_msb(AOMMIN(width, height));
  const int max_levels = AOMMAX(msb - MIN_PYRAMID_SIZE_LOG2, 1);
  n_levels = AOMMIN(n_levels, max_levels);

  ImagePyramid *pyr = aom_calloc(1, sizeof(*pyr));
  if (!pyr) {
    return NULL;
  }

  pyr->layers = aom_calloc(n_levels, sizeof(PyramidLayer));
  if (!pyr->layers) {
    aom_free(pyr);
    return NULL;
  }

  pyr->valid = false;
  pyr->n_levels = n_levels;

  // Compute sizes and offsets for each pyramid level
  // These are gathered up first, so that we can allocate all pyramid levels
  // in a single buffer
  size_t buffer_size = 0;
  size_t *layer_offsets = aom_calloc(n_levels, sizeof(size_t));
  if (!layer_offsets) {
    aom_free(pyr);
    aom_free(pyr->layers);
    return NULL;
  }

  // Work out if we need to allocate a few extra bytes for alignment.
  // aom_memalign() will ensure that the start of the allocation is aligned
  // to a multiple of PYRAMID_ALIGNMENT. But we want the first image pixel
  // to be aligned, not the first byte of the allocation.
  //
  // In the loop below, we ensure that the stride of every image is a multiple
  // of PYRAMID_ALIGNMENT. Thus the allocated size of each pyramid level will
  // also be a multiple of PYRAMID_ALIGNMENT. Thus, as long as we can get the
  // first pixel in the first pyramid layer aligned properly, that will
  // automatically mean that the first pixel of every row of every layer is
  // properly aligned too.
  //
  // Thus all we need to consider is the first pixel in the first layer.
  // This is located at offset
  //   extra_bytes + level_stride * PYRAMID_PADDING + PYRAMID_PADDING
  // bytes into the buffer. Since level_stride is a multiple of
  // PYRAMID_ALIGNMENT, we can ignore that. So we need
  //   extra_bytes + PYRAMID_PADDING = multiple of PYRAMID_ALIGNMENT
  //
  // To solve this, we can round PYRAMID_PADDING up to the next multiple
  // of PYRAMID_ALIGNMENT, then subtract the orginal value to calculate
  // how many extra bytes are needed.
  size_t first_px_offset =
      (PYRAMID_PADDING + PYRAMID_ALIGNMENT - 1) & ~(PYRAMID_ALIGNMENT - 1);
  size_t extra_bytes = first_px_offset - PYRAMID_PADDING;
  buffer_size += extra_bytes;

  // If the original image is stored in an 8-bit buffer, then we can point the
  // lowest pyramid level at that buffer rather than allocating a new one.
  int first_allocated_level = image_is_16bit ? 0 : 1;

  for (int level = first_allocated_level; level < n_levels; level++) {
    PyramidLayer *layer = &pyr->layers[level];

    int level_width = width >> level;
    int level_height = height >> level;

    // Allocate padding for each layer
    int padded_width = level_width + 2 * PYRAMID_PADDING;
    int padded_height = level_height + 2 * PYRAMID_PADDING;

    // Align the layer stride to be a multiple of PYRAMID_ALIGNMENT
    // This ensures that, as long as the top-left pixel in this pyramid level is
    // properly aligned, then so will the leftmost pixel in every row of the
    // pyramid level.
    int level_stride =
        (padded_width + PYRAMID_ALIGNMENT - 1) & ~(PYRAMID_ALIGNMENT - 1);

    size_t level_alloc_start = buffer_size;
    size_t level_start =
        level_alloc_start + PYRAMID_PADDING * level_stride + PYRAMID_PADDING;

    buffer_size += level_stride * padded_height;

    layer_offsets[level] = level_start;
    layer->width = level_width;
    layer->height = level_height;
    layer->stride = level_stride;
  }

  pyr->buffer_alloc =
      aom_memalign(PYRAMID_ALIGNMENT, buffer_size * sizeof(*pyr->buffer_alloc));
  if (!pyr->buffer_alloc) {
    aom_free(pyr);
    aom_free(pyr->layers);
    aom_free(layer_offsets);
    return NULL;
  }

  // Fill in pointers for each level
  // If image is 8-bit, then the lowest level is left unconfigured for now,
  // and will be set up properly when the pyramid is filled in
  for (int level = first_allocated_level; level < n_levels; level++) {
    PyramidLayer *layer = &pyr->layers[level];
    layer->buffer = pyr->buffer_alloc + layer_offsets[level];
  }

#if CONFIG_MULTITHREAD
  pthread_mutex_init(&pyr->mutex, NULL);
#endif  // CONFIG_MULTITHREAD

  aom_free(layer_offsets);
  return pyr;
}

// Fill the border region of a pyramid frame.
// This must be called after the main image area is filled out.
// `img_buf` should point to the first pixel in the image area,
// ie. it should be pyr->level_buffer + pyr->level_loc[level].
static INLINE void fill_border(uint8_t *img_buf, const int width,
                               const int height, const int stride) {
  // Fill left and right areas
  for (int row = 0; row < height; row++) {
    uint8_t *row_start = &img_buf[row * stride];
    uint8_t left_pixel = row_start[0];
    memset(row_start - PYRAMID_PADDING, left_pixel, PYRAMID_PADDING);
    uint8_t right_pixel = row_start[width - 1];
    memset(row_start + width, right_pixel, PYRAMID_PADDING);
  }

  // Fill top area
  for (int row = -PYRAMID_PADDING; row < 0; row++) {
    uint8_t *row_start = &img_buf[row * stride];
    memcpy(row_start - PYRAMID_PADDING, img_buf - PYRAMID_PADDING,
           width + 2 * PYRAMID_PADDING);
  }

  // Fill bottom area
  uint8_t *last_row_start = &img_buf[(height - 1) * stride];
  for (int row = height; row < height + PYRAMID_PADDING; row++) {
    uint8_t *row_start = &img_buf[row * stride];
    memcpy(row_start - PYRAMID_PADDING, last_row_start - PYRAMID_PADDING,
           width + 2 * PYRAMID_PADDING);
  }
}

// Compute coarse to fine pyramids for a frame
// This must only be called while holding frame_pyr->mutex
static INLINE bool fill_pyramid(const YV12_BUFFER_CONFIG *frame, int bit_depth,
                                ImagePyramid *frame_pyr) {
  int n_levels = frame_pyr->n_levels;
  const int frame_width = frame->y_crop_width;
  const int frame_height = frame->y_crop_height;
  const int frame_stride = frame->y_stride;
  assert((frame_width >> n_levels) >= 0);
  assert((frame_height >> n_levels) >= 0);

  PyramidLayer *first_layer = &frame_pyr->layers[0];
  if (frame->flags & YV12_FLAG_HIGHBITDEPTH) {
    // For frames stored in a 16-bit buffer, we need to downconvert to 8 bits
    assert(first_layer->width == frame_width);
    assert(first_layer->height == frame_height);

    uint16_t *frame_buffer = CONVERT_TO_SHORTPTR(frame->y_buffer);
    uint8_t *pyr_buffer = first_layer->buffer;
    int pyr_stride = first_layer->stride;
    for (int y = 0; y < frame_height; y++) {
      uint16_t *frame_row = frame_buffer + y * frame_stride;
      uint8_t *pyr_row = pyr_buffer + y * pyr_stride;
      for (int x = 0; x < frame_width; x++) {
        pyr_row[x] = frame_row[x] >> (bit_depth - 8);
      }
    }

    fill_border(pyr_buffer, frame_width, frame_height, pyr_stride);
  } else {
    // For frames stored in an 8-bit buffer, we need to configure the first
    // pyramid layer to point at the original image buffer
    first_layer->buffer = frame->y_buffer;
    first_layer->width = frame_width;
    first_layer->height = frame_height;
    first_layer->stride = frame_stride;
  }

  // Fill in the remaining levels through progressive downsampling
  for (int level = 1; level < n_levels; ++level) {
    PyramidLayer *prev_layer = &frame_pyr->layers[level - 1];
    uint8_t *prev_buffer = prev_layer->buffer;
    int prev_stride = prev_layer->stride;

    PyramidLayer *this_layer = &frame_pyr->layers[level];
    uint8_t *this_buffer = this_layer->buffer;
    int this_width = this_layer->width;
    int this_height = this_layer->height;
    int this_stride = this_layer->stride;

    // Compute the this pyramid level by downsampling the current level.
    //
    // We downsample by a factor of exactly 2, clipping the rightmost and
    // bottommost pixel off of the current level if needed. We do this for
    // two main reasons:
    //
    // 1) In the disflow code, when stepping from a higher pyramid level to a
    //    lower pyramid level, we need to not just interpolate the flow field
    //    but also to scale each flow vector by the upsampling ratio.
    //    So it is much more convenient if this ratio is simply 2.
    //
    // 2) Up/downsampling by a factor of 2 can be implemented much more
    //    efficiently than up/downsampling by a generic ratio.
    //    TODO(rachelbarker): Use optimized downsample-by-2 function
    if (!av1_resize_plane(prev_buffer, this_height << 1, this_width << 1,
                          prev_stride, this_buffer, this_height, this_width,
                          this_stride))
      return false;
    fill_border(this_buffer, this_width, this_height, this_stride);
  }
  return true;
}

// Fill out a downsampling pyramid for a given frame.
//
// The top level (index 0) will always be an 8-bit copy of the input frame,
// regardless of the input bit depth. Additional levels are then downscaled
// by powers of 2.
//
// For small input frames, the number of levels actually constructed
// will be limited so that the smallest image is at least MIN_PYRAMID_SIZE
// pixels along each side.
//
// However, if the input frame has a side of length < MIN_PYRAMID_SIZE,
// we will still construct the top level.
bool aom_compute_pyramid(const YV12_BUFFER_CONFIG *frame, int bit_depth,
                         ImagePyramid *pyr) {
  assert(pyr);

  // Per the comments in the ImagePyramid struct, we must take this mutex
  // before reading or writing the "valid" flag, and hold it while computing
  // the pyramid, to ensure proper behaviour if multiple threads call this
  // function simultaneously
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD

  if (!pyr->valid) {
    pyr->valid = fill_pyramid(frame, bit_depth, pyr);
  }
  bool valid = pyr->valid;

  // At this point, the pyramid is guaranteed to be valid, and can be safely
  // read from without holding the mutex any more

#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD
  return valid;
}

#ifndef NDEBUG
// Check if a pyramid has already been computed.
// This is mostly a debug helper - as it is necessary to hold pyr->mutex
// while reading the valid flag, we cannot just write:
//   assert(pyr->valid);
// This function allows the check to be correctly written as:
//   assert(aom_is_pyramid_valid(pyr));
bool aom_is_pyramid_valid(ImagePyramid *pyr) {
  assert(pyr);

  // Per the comments in the ImagePyramid struct, we must take this mutex
  // before reading or writing the "valid" flag, and hold it while computing
  // the pyramid, to ensure proper behaviour if multiple threads call this
  // function simultaneously
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD

  bool valid = pyr->valid;

#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD

  return valid;
}
#endif

// Mark a pyramid as no longer containing valid data.
// This must be done whenever the corresponding frame buffer is reused
void aom_invalidate_pyramid(ImagePyramid *pyr) {
  if (pyr) {
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD
    pyr->valid = false;
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD
  }
}

// Release the memory associated with a pyramid
void aom_free_pyramid(ImagePyramid *pyr) {
  if (pyr) {
#if CONFIG_MULTITHREAD
    pthread_mutex_destroy(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD
    aom_free(pyr->buffer_alloc);
    aom_free(pyr->layers);
    aom_free(pyr);
  }
}
