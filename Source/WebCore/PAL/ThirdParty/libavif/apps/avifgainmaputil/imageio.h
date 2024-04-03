// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_APPS_AVIFGAINMAPUTIL_IMAGEIO_H_
#define LIBAVIF_APPS_AVIFGAINMAPUTIL_IMAGEIO_H_

#include <string>

#include "avif/avif.h"

namespace avif {

// Writes an image in any of the supported formats based on the file extension.
avifResult WriteImage(const avifImage* image,
                      const std::string& output_filename, int quality,
                      int speed);
// Reads an image in any of the supported formats. Ignores any gain map.
avifResult ReadImage(avifImage* image, const std::string& input_filename,
                     avifPixelFormat requested_format, uint32_t requested_depth,
                     bool ignore_profile);

// Reads an image in avif format given a pre-configured encoder.
avifResult WriteAvif(const avifImage* image, avifEncoder* encoder,
                     const std::string& output_filename);

// Reads an image in avif format given a pre-configured decoder.
// The image can be accessed at decoder->image.
avifResult ReadAvif(avifDecoder* decoder, const std::string& input_filename,
                    bool ignore_profile);

}  // namespace avif

#endif  // LIBAVIF_APPS_AVIFGAINMAPUTIL_IMAGEIO_H_
