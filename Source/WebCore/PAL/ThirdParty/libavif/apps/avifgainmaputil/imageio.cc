// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "imageio.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

#include "avif/avif_cxx.h"
#include "avifjpeg.h"
#include "avifpng.h"
#include "avifutil.h"
#include "y4m.h"

namespace avif {

template <typename T>
inline T Clamp(T x, T low, T high) {  // Only exists in C++17.
  return (x < low) ? low : (high < x) ? high : x;
}

avifResult WriteImage(const avifImage* image,
                      const std::string& output_filename, int quality,
                      int speed) {
  quality = Clamp(quality, 0, 100);
  speed = Clamp(speed, 0, 10);
  const avifAppFileFormat output_format =
      avifGuessFileFormat(output_filename.c_str());
  if (output_format == AVIF_APP_FILE_FORMAT_UNKNOWN) {
    std::cerr << "Cannot determine output file extension: " << output_filename
              << "\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  } else if (output_format == AVIF_APP_FILE_FORMAT_Y4M) {
    if (!y4mWrite(output_filename.c_str(), image)) {
      return AVIF_RESULT_UNKNOWN_ERROR;
    }
  } else if (output_format == AVIF_APP_FILE_FORMAT_JPEG) {
    if (!avifJPEGWrite(output_filename.c_str(), image, quality,
                       AVIF_CHROMA_UPSAMPLING_AUTOMATIC)) {
      return AVIF_RESULT_UNKNOWN_ERROR;
    }
  } else if (output_format == AVIF_APP_FILE_FORMAT_PNG) {
    const int compression_level = Clamp(10 - speed, 0, 9);
    if (!avifPNGWrite(output_filename.c_str(), image, /*requestedDepth=*/0,
                      AVIF_CHROMA_UPSAMPLING_AUTOMATIC, compression_level)) {
      return AVIF_RESULT_UNKNOWN_ERROR;
    }
  } else if (output_format == AVIF_APP_FILE_FORMAT_AVIF) {
    EncoderPtr encoder(avifEncoderCreate());
    if (encoder == nullptr) {
      return AVIF_RESULT_OUT_OF_MEMORY;
    }
    encoder->quality = quality;
    encoder->speed = speed;
    return WriteAvif(image, encoder.get(), output_filename);
  } else {
    std::cerr << "Unsupported output file extension: " << output_filename
              << "\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  }
  return AVIF_RESULT_OK;
}

avifResult WriteAvif(const avifImage* image, avifEncoder* encoder,
                     const std::string& output_filename) {
  avifRWData encoded = AVIF_DATA_EMPTY;
  std::cout << "AVIF to be written:\n";
  const bool gain_map_present =
      (image->gainMap != nullptr && image->gainMap->image != nullptr);
  avifImageDump(image,
                /*gridCols=*/1,
                /*gridRows=*/1, gain_map_present,
                AVIF_PROGRESSIVE_STATE_UNAVAILABLE);
  std::cout << "Encoding AVIF at quality " << encoder->quality << " speed "
            << encoder->speed << ", please wait...\n";
  avifResult result = avifEncoderWrite(encoder, image, &encoded);
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Failed to encode image: " << avifResultToString(result)
              << " (" << encoder->diag.error << ")\n";
    return result;
  }
  std::ofstream f(output_filename, std::ios::binary);
  f.write(reinterpret_cast<char*>(encoded.data), encoded.size);
  if (f.fail()) {
    std::cerr << "Failed to write image " << output_filename << ": "
              << std::strerror(errno) << "\n";
    return AVIF_RESULT_IO_ERROR;
  }
  std::cout << "Wrote AVIF: " << output_filename << "\n";
  return AVIF_RESULT_OK;
}

avifResult ReadImage(avifImage* image, const std::string& input_filename,
                     avifPixelFormat requested_format, uint32_t requested_depth,
                     bool ignore_profile) {
  avifAppFileFormat input_format = avifGuessFileFormat(input_filename.c_str());
  if (input_format == AVIF_APP_FILE_FORMAT_UNKNOWN) {
    std::cerr << "Cannot determine input format: " << input_filename;
    return AVIF_RESULT_INVALID_ARGUMENT;
  } else if (input_format == AVIF_APP_FILE_FORMAT_AVIF) {
    DecoderPtr decoder(avifDecoderCreate());
    if (decoder == NULL) {
      return AVIF_RESULT_OUT_OF_MEMORY;
    }
    avifResult result = ReadAvif(decoder.get(), input_filename, ignore_profile);
    if (result != AVIF_RESULT_OK) {
      return result;
    }
    if (decoder->image->imageOwnsYUVPlanes &&
        (decoder->image->alphaPlane == NULL ||
         decoder->image->imageOwnsAlphaPlane)) {
      std::swap(*image, *decoder->image);
    } else {
      result = avifImageCopy(image, decoder->image, AVIF_PLANES_ALL);
      if (result != AVIF_RESULT_OK) {
        return result;
      }
    }
  } else {
    const avifAppFileFormat file_format = avifReadImage(
        input_filename.c_str(), requested_format,
        static_cast<int>(requested_depth), AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
        ignore_profile, /*ignoreExif=*/false, /*ignoreXMP=*/false,
        /*allowChangingCicp=*/true, /*ignoreGainMap=*/true,
        AVIF_DEFAULT_IMAGE_SIZE_LIMIT, image, /*outDepth=*/nullptr,
        /*sourceTiming=*/nullptr, /*frameIter=*/nullptr);
    if (file_format == AVIF_APP_FILE_FORMAT_UNKNOWN) {
      std::cout << "Failed to decode image: " << input_filename;
      return AVIF_RESULT_INVALID_ARGUMENT;
    }
    if (image->icc.size == 0) {
      // Assume sRGB by default.
      if (image->colorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED &&
          image->transferCharacteristics == AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
        image->colorPrimaries = AVIF_COLOR_PRIMARIES_SRGB;
        image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
      }
    }
  }
  return AVIF_RESULT_OK;
}

avifResult ReadAvif(avifDecoder* decoder, const std::string& input_filename,
                    bool ignore_profile) {
  avifResult result = avifDecoderSetIOFile(decoder, input_filename.c_str());
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Cannot open file for read: " << input_filename << "\n";
    return result;
  }
  result = avifDecoderParse(decoder);
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Failed to parse image: " << avifResultToString(result) << " ("
              << decoder->diag.error << ")\n";
    return result;
  }
  result = avifDecoderNextImage(decoder);
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Failed to decode image: " << avifResultToString(result)
              << " (" << decoder->diag.error << ")\n";
    return result;
  }
  if (ignore_profile) {
    avifRWDataFree(&decoder->image->icc);
  }

  return AVIF_RESULT_OK;
}

}  // namespace avif
