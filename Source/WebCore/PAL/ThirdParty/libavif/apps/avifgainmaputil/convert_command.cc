// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "convert_command.h"

#include "avif/avif_cxx.h"
#include "avifutil.h"
#include "imageio.h"
#include "swapbase_command.h"

namespace avif {

ConvertCommand::ConvertCommand()
    : ProgramCommand("convert", "Convert a jpeg with a gain map to avif.") {
  argparse_.add_argument(arg_input_filename_, "input_filename.jpg");
  argparse_.add_argument(arg_output_filename_, "output_image.avif");
  argparse_.add_argument(arg_swap_base_, "--swap-base")
      .help("Make the HDR image the base image")
      .action(argparse::Action::STORE_TRUE)
      .default_value("false");
  argparse_.add_argument(arg_gain_map_quality_, "--qgain-map")
      .help("Quality for the gain map (0-100, where 100 is lossless)")
      .default_value("60");
  argparse_.add_argument<CicpValues, CicpConverter>(arg_cicp_, "--cicp")
      .help(
          "Set the cicp values for the input image, expressed as "
          "P/T/M where P = color primaries, T = transfer characteristics, "
          "M = matrix coefficients.");
  arg_image_encode_.Init(argparse_, /*can_have_alpha=*/false);
  arg_image_read_.Init(argparse_);
}

avifResult ConvertCommand::Run() {
  const avifPixelFormat pixel_format =
      static_cast<avifPixelFormat>(arg_image_read_.pixel_format.value());

  ImagePtr image(avifImageCreateEmpty());
  if (image == nullptr) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }

  const avifAppFileFormat file_format = avifReadImage(
      arg_input_filename_.value().c_str(), pixel_format, arg_image_read_.depth,
      AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC, arg_image_read_.ignore_profile,
      /*ignoreExif=*/false,
      /*ignoreXMP=*/false,
      /*allowChangingCicp=*/true,
      /*ignoreGainMap=*/false, AVIF_DEFAULT_IMAGE_SIZE_LIMIT, image.get(),
      /*outDepth=*/nullptr,
      /*sourceTiming=*/nullptr,
      /*frameIter=*/nullptr);
  if (file_format == AVIF_APP_FILE_FORMAT_UNKNOWN) {
    std::cout << "Failed to decode image: " << arg_input_filename_;
    return AVIF_RESULT_INVALID_ARGUMENT;
  }
  if (arg_cicp_.provenance() == argparse::Provenance::SPECIFIED) {
    image->colorPrimaries = arg_cicp_.value().color_primaries;
    image->transferCharacteristics = arg_cicp_.value().transfer_characteristics;
    image->matrixCoefficients = arg_cicp_.value().matrix_coefficients;
  } else if (image->icc.size == 0 &&
             image->colorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED &&
             image->transferCharacteristics ==
                 AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
    // If there is no ICC and no CICP, assume sRGB by default.
    image->colorPrimaries = AVIF_COLOR_PRIMARIES_SRGB;
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
  }

  if (image->gainMap && image->gainMap->altICC.size == 0) {
    if (image->gainMap->altColorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
      // Assume the alternate image has the same primaries as the base image.
      image->gainMap->altColorPrimaries = image->colorPrimaries;
    }
    if (image->gainMap->altTransferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
      // Assume the alternate image is PQ HDR.
      image->gainMap->altTransferCharacteristics =
          AVIF_TRANSFER_CHARACTERISTICS_PQ;
    }
  }

  if (image->gainMap == nullptr || image->gainMap->image == nullptr) {
    std::cerr << "Input image " << arg_input_filename_
              << " does not contain a gain map\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  }

  if (arg_swap_base_) {
    int depth = arg_image_read_.depth;
    if (depth == 0) {
      depth = image->gainMap->metadata.alternateHdrHeadroomN == 0 ? 8 : 10;
    }
    ImagePtr new_base(avifImageCreateEmpty());
    if (new_base == nullptr) {
      return AVIF_RESULT_OUT_OF_MEMORY;
    }
    const avifResult result =
        ChangeBase(*image, depth, image->yuvFormat, new_base.get());
    if (result != AVIF_RESULT_OK) {
      return result;
    }
    std::swap(image, new_base);
  }

  EncoderPtr encoder(avifEncoderCreate());
  if (encoder == nullptr) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  encoder->quality = arg_image_encode_.quality;
  encoder->qualityAlpha = arg_image_encode_.quality_alpha;
  encoder->qualityGainMap = arg_gain_map_quality_;
  encoder->speed = arg_image_encode_.speed;
  const avifResult result =
      WriteAvif(image.get(), encoder.get(), arg_output_filename_);
  if (result != AVIF_RESULT_OK) {
    std::cout << "Failed to encode image: " << avifResultToString(result)
              << " (" << encoder->diag.error << ")\n";
    return result;
  }

  return AVIF_RESULT_OK;
}

}  // namespace avif
