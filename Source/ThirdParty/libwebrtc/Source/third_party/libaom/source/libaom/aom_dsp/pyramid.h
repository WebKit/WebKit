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

#ifndef AOM_AOM_DSP_PYRAMID_H_
#define AOM_AOM_DSP_PYRAMID_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "config/aom_config.h"

#include "aom_scale/yv12config.h"
#include "aom_util/aom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// Minimum dimensions of a downsampled image
#define MIN_PYRAMID_SIZE_LOG2 3
#define MIN_PYRAMID_SIZE (1 << MIN_PYRAMID_SIZE_LOG2)

// Size of border around each pyramid image, in pixels
// Similarly to the border around regular image buffers, this border is filled
// with copies of the outermost pixels of the frame, to allow for more efficient
// convolution code
// TODO(rachelbarker): How many pixels do we actually need here?
// I think we only need 9 for disflow, but how many for corner matching?
#define PYRAMID_PADDING 16

// Byte alignment of each line within the image pyramids.
// That is, the first pixel inside the image (ie, not in the border region),
// on each row of each pyramid level, is aligned to this byte alignment.
// This value must be a power of 2.
#define PYRAMID_ALIGNMENT 32

typedef struct {
  uint8_t *buffer;
  int width;
  int height;
  int stride;
} PyramidLayer;

// Struct for an image pyramid
typedef struct image_pyramid {
#if CONFIG_MULTITHREAD
  // Mutex which is used to prevent the pyramid being computed twice at the
  // same time
  //
  // Semantics:
  // * This mutex must be held whenever reading or writing the `valid` flag
  //
  // * This mutex must also be held while computing the image pyramid,
  //   to ensure that only one thread may do so at a time.
  //
  // * However, once you have read the valid flag and seen a true value,
  //   it is safe to drop the mutex and read from the remaining fields.
  //   This is because, once the image pyramid is computed, its contents
  //   will not be changed until the parent frame buffer is recycled,
  //   which will not happen until there are no more outstanding references
  //   to the frame buffer.
  pthread_mutex_t mutex;
#endif
  // Flag indicating whether the pyramid contains valid data
  bool valid;
  // Number of allocated/filled levels in this pyramid
  int n_levels;
  // Pointer to allocated buffer
  uint8_t *buffer_alloc;
  // Data for each level
  // The `buffer` pointers inside this array point into the region which
  // is stored in the `buffer_alloc` field here
  PyramidLayer *layers;
} ImagePyramid;

size_t aom_get_pyramid_alloc_size(int width, int height, int n_levels,
                                  bool image_is_16bit);

ImagePyramid *aom_alloc_pyramid(int width, int height, int n_levels,
                                bool image_is_16bit);

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
                         ImagePyramid *pyr);

#ifndef NDEBUG
// Check if a pyramid has already been computed.
// This is mostly a debug helper - as it is necessary to hold pyr->mutex
// while reading the valid flag, we cannot just write:
//   assert(pyr->valid);
// This function allows the check to be correctly written as:
//   assert(aom_is_pyramid_valid(pyr));
bool aom_is_pyramid_valid(ImagePyramid *pyr);
#endif

// Mark a pyramid as no longer containing valid data.
// This must be done whenever the corresponding frame buffer is reused
void aom_invalidate_pyramid(ImagePyramid *pyr);

// Release the memory associated with a pyramid
void aom_free_pyramid(ImagePyramid *pyr);

#ifdef __cplusplus
}
#endif

#endif  // AOM_AOM_DSP_PYRAMID_H_
