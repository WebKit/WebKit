// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "swapbase_command.h"

#include "avif/avif_cxx.h"
#include "imageio.h"

namespace avif {

avifResult ChangeBase(const avifImage& image, int depth,
                      avifPixelFormat yuvFormat, avifImage* swapped) {
  if (image.gainMap == nullptr || image.gainMap->image == nullptr) {
    return AVIF_RESULT_INVALID_ARGUMENT;
  }

  // Copy all metadata (no planes).
  avifResult result = avifImageCopy(swapped, &image, /*planes=*/0);
  if (result != AVIF_RESULT_OK) {
    return result;
  }
  swapped->depth = depth;
  swapped->yuvFormat = yuvFormat;

  const float headroom =
      static_cast<double>(image.gainMap->metadata.alternateHdrHeadroomN) /
      image.gainMap->metadata.alternateHdrHeadroomD;
  const bool tone_mapping_to_sdr = (headroom == 0.0f);

  swapped->colorPrimaries = image.gainMap->altColorPrimaries;
  if (swapped->colorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
    // Default to the same primaries as the base image if unspecified.
    swapped->colorPrimaries = image.colorPrimaries;
  }

  swapped->transferCharacteristics = image.gainMap->altTransferCharacteristics;
  if (swapped->transferCharacteristics ==
      AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
    // Default to PQ for HDR and sRGB for SDR if unspecified.
    const avifTransferCharacteristics transfer_characteristics =
        tone_mapping_to_sdr ? AVIF_TRANSFER_CHARACTERISTICS_SRGB
                            : AVIF_TRANSFER_CHARACTERISTICS_PQ;
    swapped->transferCharacteristics = transfer_characteristics;
  }

  swapped->matrixCoefficients = image.gainMap->altMatrixCoefficients;
  if (swapped->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED) {
    // Default to the same matrix as the base image if unspecified.
    swapped->matrixCoefficients = image.matrixCoefficients;
  }

  avifRGBImage swapped_rgb;
  avifRGBImageSetDefaults(&swapped_rgb, swapped);

  avifContentLightLevelInformationBox clli = image.gainMap->image->clli;
  const bool compute_clli =
      !tone_mapping_to_sdr && clli.maxCLL == 0 && clli.maxPALL == 0;

  avifDiagnostics diag;
  result = avifImageApplyGainMap(&image, image.gainMap, headroom,
                                 swapped->colorPrimaries,
                                 swapped->transferCharacteristics, &swapped_rgb,
                                 (compute_clli ? &clli : nullptr), &diag);
  if (result != AVIF_RESULT_OK) {
    std::cout << "Failed to tone map image: " << avifResultToString(result)
              << " (" << diag.error << ")\n";
    return result;
  }
  result = avifImageRGBToYUV(swapped, &swapped_rgb);
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Failed to convert to YUV: " << avifResultToString(result)
              << "\n";
    return result;
  }

  swapped->clli = clli;
  // Copy the gain map's planes.
  result = avifImageCopy(swapped->gainMap->image, image.gainMap->image,
                         AVIF_PLANES_YUV);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  // Fill in the information on the alternate image
  result =
      avifRWDataSet(&swapped->gainMap->altICC, image.icc.data, image.icc.size);
  if (result != AVIF_RESULT_OK) {
    return result;
  }
  swapped->gainMap->altColorPrimaries = image.colorPrimaries;
  swapped->gainMap->altTransferCharacteristics = image.transferCharacteristics;
  swapped->gainMap->altMatrixCoefficients = image.matrixCoefficients;
  swapped->gainMap->altYUVRange = image.yuvRange;
  swapped->gainMap->altDepth = image.depth;
  swapped->gainMap->altPlaneCount =
      (image.yuvFormat == AVIF_PIXEL_FORMAT_YUV400) ? 1 : 3;
  swapped->gainMap->altCLLI = image.clli;

  // Swap base and alternate in the gain map metadata.
  avifGainMapMetadata& metadata = swapped->gainMap->metadata;
  metadata.backwardDirection = !metadata.backwardDirection;
  metadata.useBaseColorSpace = !metadata.useBaseColorSpace;
  std::swap(metadata.baseHdrHeadroomN, metadata.alternateHdrHeadroomN);
  std::swap(metadata.baseHdrHeadroomD, metadata.alternateHdrHeadroomD);
  for (int c = 0; c < 3; ++c) {
    std::swap(metadata.baseOffsetN, metadata.alternateOffsetN);
    std::swap(metadata.baseOffsetD, metadata.alternateOffsetD);
  }

  return AVIF_RESULT_OK;
}

SwapBaseCommand::SwapBaseCommand()
    : ProgramCommand(
          "swapbase",
          "Swaps the base and alternate images (e.g. if the base image is SDR "
          "and the alternate is HDR, makes the base HDR). The alternate image "
          "is the result ot fully applying the gain map.") {
  argparse_.add_argument(arg_input_filename_, "input_filename");
  argparse_.add_argument(arg_output_filename_, "output_filename");
  arg_image_read_.Init(argparse_);
  arg_image_encode_.Init(argparse_, /*can_have_alpha=*/true);
  argparse_.add_argument(arg_gain_map_quality_, "--qgain-map")
      .help("Quality for the gain map (0-100, where 100 is lossless)")
      .default_value("60");
}

avifResult SwapBaseCommand::Run() {
  DecoderPtr decoder(avifDecoderCreate());
  if (decoder == NULL) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  decoder->enableParsingGainMapMetadata = true;
  decoder->enableDecodingGainMap = true;
  avifResult result = ReadAvif(decoder.get(), arg_input_filename_,
                               arg_image_read_.ignore_profile);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  const avifImage* image = decoder->image;
  if (image->gainMap == nullptr || image->gainMap->image == nullptr) {
    std::cerr << "Input image " << arg_input_filename_
              << " does not contain a gain map\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  }

  int depth = arg_image_read_.depth;
  if (depth == 0) {
    depth = image->gainMap->altDepth;
  }
  if (depth == 0) {
    // Default to the max depth between the base image and the gain map/
    depth = std::max(image->depth, image->gainMap->image->depth);
  }

  avifPixelFormat pixel_format =
      (avifPixelFormat)arg_image_read_.pixel_format.value();
  if (pixel_format == AVIF_PIXEL_FORMAT_NONE) {
    pixel_format = (image->gainMap->altPlaneCount == 1)
                       ? AVIF_PIXEL_FORMAT_YUV420
                       : AVIF_PIXEL_FORMAT_YUV444;
  }

  ImagePtr new_base(avifImageCreateEmpty());
  if (new_base == nullptr) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  result = ChangeBase(*image, depth, pixel_format, new_base.get());
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  EncoderPtr encoder(avifEncoderCreate());
  if (encoder == nullptr) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  encoder->quality = arg_image_encode_.quality;
  encoder->qualityAlpha = arg_image_encode_.quality_alpha;
  encoder->qualityGainMap = arg_gain_map_quality_;
  encoder->speed = arg_image_encode_.speed;
  result = WriteAvif(new_base.get(), encoder.get(), arg_output_filename_);
  if (result != AVIF_RESULT_OK) {
    std::cout << "Failed to encode image: " << avifResultToString(result)
              << " (" << encoder->diag.error << ")\n";
    return result;
  }

  return AVIF_RESULT_OK;
}

}  // namespace avif
