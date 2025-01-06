/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved.
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
#include "aom_util/aom_pthread.h"

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
// * Pyramid is initially marked as containing no valid data
// * Each pyramid layer is computed on-demand, the first time it is requested
// * Whenever frame buffer is reused, reset the counter of filled levels.
//   This invalidates all of the existing pyramid levels.
// * Whenever frame buffer is resized, reallocate pyramid

size_t aom_get_pyramid_alloc_size(int width, int height, bool image_is_16bit) {
  // Allocate the maximum possible number of layers for this width and height
  const int msb = get_msb(AOMMIN(width, height));
  const int n_levels = AOMMAX(msb - MIN_PYRAMID_SIZE_LOG2, 1);

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

ImagePyramid *aom_alloc_pyramid(int width, int height, bool image_is_16bit) {
  // Allocate the maximum possible number of layers for this width and height
  const int msb = get_msb(AOMMIN(width, height));
  const int n_levels = AOMMAX(msb - MIN_PYRAMID_SIZE_LOG2, 1);

  ImagePyramid *pyr = aom_calloc(1, sizeof(*pyr));
  if (!pyr) {
    return NULL;
  }

  pyr->layers = aom_calloc(n_levels, sizeof(*pyr->layers));
  if (!pyr->layers) {
    aom_free(pyr);
    return NULL;
  }

  pyr->max_levels = n_levels;
  pyr->filled_levels = 0;

  // Compute sizes and offsets for each pyramid level
  // These are gathered up first, so that we can allocate all pyramid levels
  // in a single buffer
  size_t buffer_size = 0;
  size_t *layer_offsets = aom_calloc(n_levels, sizeof(*layer_offsets));
  if (!layer_offsets) {
    aom_free(pyr->layers);
    aom_free(pyr);
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
    aom_free(pyr->layers);
    aom_free(pyr);
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
static inline void fill_border(uint8_t *img_buf, const int width,
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

// Compute downsampling pyramid for a frame
//
// This function will ensure that the first `n_levels` levels of the pyramid
// are filled, unless the frame is too small to have this many levels.
// In that case, we will fill all available levels and then stop.
//
// Returns the actual number of levels filled, capped at n_levels,
// or -1 on error.
//
// This must only be called while holding frame_pyr->mutex
static inline int fill_pyramid(const YV12_BUFFER_CONFIG *frame, int bit_depth,
                               int n_levels, ImagePyramid *frame_pyr) {
  int already_filled_levels = frame_pyr->filled_levels;

  // This condition should already be enforced by aom_compute_pyramid
  assert(n_levels <= frame_pyr->max_levels);

  if (already_filled_levels >= n_levels) {
    return n_levels;
  }

  const int frame_width = frame->y_crop_width;
  const int frame_height = frame->y_crop_height;
  const int frame_stride = frame->y_stride;
  assert((frame_width >> n_levels) >= 0);
  assert((frame_height >> n_levels) >= 0);

  if (already_filled_levels == 0) {
    // Fill in largest level from the original image
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
      // For frames stored in an 8-bit buffer, we don't need to copy anything -
      // we can just reference the original image buffer
      first_layer->buffer = frame->y_buffer;
      first_layer->width = frame_width;
      first_layer->height = frame_height;
      first_layer->stride = frame_stride;
    }

    already_filled_levels = 1;
  }

  // Fill in the remaining levels through progressive downsampling
  for (int level = already_filled_levels; level < n_levels; ++level) {
    bool mem_status = false;
    PyramidLayer *prev_layer = &frame_pyr->layers[level - 1];
    uint8_t *prev_buffer = prev_layer->buffer;
    int prev_stride = prev_layer->stride;

    PyramidLayer *this_layer = &frame_pyr->layers[level];
    uint8_t *this_buffer = this_layer->buffer;
    int this_width = this_layer->width;
    int this_height = this_layer->height;
    int this_stride = this_layer->stride;

    // The width and height of the previous layer that needs to be considered to
    // derive the current layer frame.
    const int input_layer_width = this_width << 1;
    const int input_layer_height = this_height << 1;

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

    // SIMD support has been added specifically for cases where the downsample
    // factor is exactly 2. In such instances, horizontal and vertical resizing
    // is performed utilizing the down2_symeven() function, which considers the
    // even dimensions of the input layer.
    if (should_resize_by_half(input_layer_height, input_layer_width,
                              this_height, this_width)) {
      assert(input_layer_height % 2 == 0 && input_layer_width % 2 == 0 &&
             "Input width or height cannot be odd.");
      mem_status = av1_resize_plane_to_half(
          prev_buffer, input_layer_height, input_layer_width, prev_stride,
          this_buffer, this_height, this_width, this_stride);
    } else {
      mem_status = av1_resize_plane(prev_buffer, input_layer_height,
                                    input_layer_width, prev_stride, this_buffer,
                                    this_height, this_width, this_stride);
    }

    // Terminate early in cases of memory allocation failure.
    if (!mem_status) {
      frame_pyr->filled_levels = n_levels;
      return -1;
    }

    fill_border(this_buffer, this_width, this_height, this_stride);
  }

  frame_pyr->filled_levels = n_levels;
  return n_levels;
}

// Fill out a downsampling pyramid for a given frame.
//
// The top level (index 0) will always be an 8-bit copy of the input frame,
// regardless of the input bit depth. Additional levels are then downscaled
// by powers of 2.
//
// This function will ensure that the first `n_levels` levels of the pyramid
// are filled, unless the frame is too small to have this many levels.
// In that case, we will fill all available levels and then stop.
// No matter how small the frame is, at least one level is guaranteed
// to be filled.
//
// Returns the actual number of levels filled, capped at n_levels,
// or -1 on error.
int aom_compute_pyramid(const YV12_BUFFER_CONFIG *frame, int bit_depth,
                        int n_levels, ImagePyramid *pyr) {
  assert(pyr);

  // Per the comments in the ImagePyramid struct, we must take this mutex
  // before reading or writing the filled_levels field, and hold it while
  // computing any additional pyramid levels, to ensure proper behaviour
  // when multithreading is used
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD

  n_levels = AOMMIN(n_levels, pyr->max_levels);
  int result = n_levels;
  if (pyr->filled_levels < n_levels) {
    // Compute any missing levels that we need
    result = fill_pyramid(frame, bit_depth, n_levels, pyr);
  }

  // At this point, as long as result >= 0, the requested number of pyramid
  // levels are guaranteed to be valid, and can be safely read from without
  // holding the mutex any further
  assert(IMPLIES(result >= 0, pyr->filled_levels >= n_levels));
#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD
  return result;
}

#ifndef NDEBUG
// Check if a pyramid has already been computed to at least n levels
// This is mostly a debug helper - as it is necessary to hold pyr->mutex
// while reading the number of already-computed levels, we cannot just write:
//   assert(pyr->filled_levels >= n_levels);
// This function allows the check to be correctly written as:
//   assert(aom_is_pyramid_valid(pyr, n_levels));
//
// Note: This deliberately does not restrict n_levels based on the maximum
// number of permitted levels for the frame size. This allows the check to
// catch cases where the caller forgets to handle the case where
// max_levels is less than the requested number of levels
bool aom_is_pyramid_valid(ImagePyramid *pyr, int n_levels) {
  assert(pyr);

  // Per the comments in the ImagePyramid struct, we must take this mutex
  // before reading or writing the filled_levels field, to ensure proper
  // behaviour when multithreading is used
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD

  bool result = (pyr->filled_levels >= n_levels);

#if CONFIG_MULTITHREAD
  pthread_mutex_unlock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD

  return result;
}
#endif

// Mark a pyramid as no longer containing valid data.
// This must be done whenever the corresponding frame buffer is reused
void aom_invalidate_pyramid(ImagePyramid *pyr) {
  if (pyr) {
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(&pyr->mutex);
#endif  // CONFIG_MULTITHREAD
    pyr->filled_levels = 0;
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
