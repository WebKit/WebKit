// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "tonemap_command.h"

#include <cmath>

#include "avif/avif_cxx.h"
#include "imageio.h"

namespace avif {

TonemapCommand::TonemapCommand()
    : ProgramCommand("tonemap",
                     "Tone maps an avif image that has a gain map to a "
                     "given HDR headroom (how much brighter the display can go "
                     "compared to an SDR display)") {
  argparse_.add_argument(arg_input_filename_, "input_image");
  argparse_.add_argument(arg_output_filename_, "output_image");
  argparse_.add_argument(arg_headroom_, "--headroom")
      .help(
          "HDR headroom to tone map to. This is log2 of the ratio of HDR to "
          "SDR luminance. 0 means SDR.")
      .default_value("0");
  argparse_
      .add_argument<CicpValues, CicpConverter>(arg_input_cicp_, "--cicp-input")
      .help(
          "Override input CICP values, expressed as P/T/M "
          "where P = color primaries, T = transfer characteristics, "
          "M = matrix coefficients.");
  argparse_
      .add_argument<CicpValues, CicpConverter>(arg_output_cicp_,
                                               "--cicp-output")
      .help(
          "CICP values for the output, expressed as P/T/M "
          "where P = color primaries, T = transfer characteristics, "
          "M = matrix coefficients. P and M are only relevant when saving to "
          "AVIF. "
          "If not specified, 'color primaries' defaults to the base image's "
          "primaries, 'transfer characteristics' defaults to 16 (PQ) if "
          "headroom > 0, or 13 (sRGB) otherwise, 'matrix coefficients' "
          "defaults to 6 (BT601).");
  argparse_.add_argument(arg_clli_str_, "--clli")
      .help(
          "Override content light level information expressed as: "
          "MaxCLL,MaxPALL. Only relevant when saving to AVIF.");
  arg_image_read_.Init(argparse_);
  arg_image_encode_.Init(argparse_, /*can_have_alpha=*/true);
}

avifResult TonemapCommand::Run() {
  avifContentLightLevelInformationBox clli_box = {};
  bool clli_set = false;
  if (arg_clli_str_.value().size() > 0) {
    std::vector<uint32_t> clli;
    if (!ParseList(arg_clli_str_, ',', 2, &clli)) {
      std::cerr << "Invalid clli values, expected format: maxCLL,maxPALL where "
                   "both maxCLL and maxPALL are positive integers, got: "
                << arg_clli_str_ << "\n";
      return AVIF_RESULT_INVALID_ARGUMENT;
    }
    clli_box.maxCLL = clli[0];
    clli_box.maxPALL = clli[1];
    clli_set = true;
  }

  const float headroom = arg_headroom_;
  const bool tone_mapping_to_hdr = (headroom > 0.0f);

  DecoderPtr decoder(avifDecoderCreate());
  if (decoder == NULL) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;
  avifResult result = ReadAvif(decoder.get(), arg_input_filename_,
                               arg_image_read_.ignore_profile);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  avifImage* image = decoder->image;
  if (image->gainMap == nullptr || image->gainMap->image == nullptr) {
    std::cerr << "Input image " << arg_input_filename_
              << " does not contain a gain map\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  }

  avifGainMapMetadataDouble metadata;
  if (!avifGainMapMetadataFractionsToDouble(&metadata,
                                            &image->gainMap->metadata)) {
    std::cerr << "Input image " << arg_input_filename_
              << " has invalid gain map metadata\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  }

  // We are either tone mapping to the base image (i.e. leaving it as is),
  // or tone mapping to the alternate image (i.e. fully applying the gain map),
  // or tone mapping in between (partially applying the gain map).
  const bool tone_mapping_to_base =
      (headroom <= metadata.baseHdrHeadroom &&
       metadata.baseHdrHeadroom <= metadata.alternateHdrHeadroom) ||
      (headroom >= metadata.baseHdrHeadroom &&
       metadata.baseHdrHeadroom >= metadata.alternateHdrHeadroom);
  const bool tone_mapping_to_alternate =
      (headroom <= metadata.alternateHdrHeadroom &&
       metadata.alternateHdrHeadroom <= metadata.baseHdrHeadroom) ||
      (headroom >= metadata.alternateHdrHeadroom &&
       metadata.alternateHdrHeadroom >= metadata.baseHdrHeadroom);
  const bool base_is_hdr = (metadata.baseHdrHeadroom != 0.0f);

  // Determine output CICP.
  CicpValues cicp;
  if (arg_output_cicp_.provenance() == argparse::Provenance::SPECIFIED) {
    cicp = arg_output_cicp_;  // User provided values.
  } else if (tone_mapping_to_base || (tone_mapping_to_hdr && base_is_hdr)) {
    cicp = {image->colorPrimaries, image->transferCharacteristics,
            image->matrixCoefficients};
  } else {
    cicp = {image->gainMap->altColorPrimaries,
            image->gainMap->altTransferCharacteristics,
            image->gainMap->altMatrixCoefficients};
  }
  if (cicp.color_primaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
    // TODO(maryla): for now avifImageApplyGainMap always uses the primaries of
    // the base image, but it should take into account the metadata's
    // useBaseColorSpace property.
    cicp.color_primaries = image->colorPrimaries;
  }
  if (cicp.transfer_characteristics ==
      AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
    cicp.transfer_characteristics = tone_mapping_to_hdr
                                        ? AVIF_TRANSFER_CHARACTERISTICS_PQ
                                        : AVIF_TRANSFER_CHARACTERISTICS_SRGB;
  }

  // Determine output depth.
  int depth = arg_image_read_.depth;
  if (depth == 0) {
    if (tone_mapping_to_base) {
      depth = image->depth;
    } else if (tone_mapping_to_alternate) {
      depth = image->gainMap->altDepth;
    }
    if (depth == 0) {
      // Default to the max depth between the base image, the gain map and
      // the specified 'altDepth'.
      depth = std::max(std::max(image->depth, image->gainMap->image->depth),
                       image->gainMap->altDepth);
    }
  }

  // Determine output pixel format.
  avifPixelFormat pixel_format =
      (avifPixelFormat)arg_image_read_.pixel_format.value();
  const avifPixelFormat alt_yuv_format =
      (image->gainMap->altPlaneCount == 1)
          ? AVIF_PIXEL_FORMAT_YUV400
          // Favor the least chroma subsampled format.
          : std::min(image->yuvFormat, image->gainMap->image->yuvFormat);
  if (pixel_format == AVIF_PIXEL_FORMAT_NONE) {
    if (tone_mapping_to_base) {
      pixel_format = image->yuvFormat;
    } else if (tone_mapping_to_alternate) {
      pixel_format = alt_yuv_format;
    }
    if (pixel_format == AVIF_PIXEL_FORMAT_NONE) {
      // Default to the least chroma subsampled format provided.
      pixel_format =
          std::min(std::min(image->yuvFormat, image->gainMap->image->yuvFormat),
                   alt_yuv_format);
    }
  }

  // Use the clli from the base image or the alternate image if the headroom
  // is outside of the (baseHdrHeadroom, alternateHdrHeadroom) range.
  if (!clli_set) {
    if (tone_mapping_to_base) {
      clli_box = image->clli;
    } else if (tone_mapping_to_alternate) {
      clli_box = image->gainMap->image->clli;
    }
  }
  clli_set = (clli_box.maxCLL != 0) || (clli_box.maxPALL != 0);

  ImagePtr tone_mapped(
      avifImageCreate(image->width, image->height, depth, pixel_format));
  if (tone_mapped == nullptr) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  avifRGBImage tone_mapped_rgb;
  avifRGBImageSetDefaults(&tone_mapped_rgb, tone_mapped.get());
  avifDiagnostics diag;
  result = avifImageApplyGainMap(
      decoder->image, image->gainMap, arg_headroom_, cicp.color_primaries,
      cicp.transfer_characteristics, &tone_mapped_rgb,
      clli_set ? nullptr : &clli_box, &diag);
  if (result != AVIF_RESULT_OK) {
    std::cout << "Failed to tone map image: " << avifResultToString(result)
              << " (" << diag.error << ")\n";
    return result;
  }
  result = avifImageRGBToYUV(tone_mapped.get(), &tone_mapped_rgb);
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Failed to convert to YUV: " << avifResultToString(result)
              << "\n";
    return result;
  }

  tone_mapped->clli = clli_box;
  tone_mapped->transferCharacteristics = cicp.transfer_characteristics;
  tone_mapped->colorPrimaries = cicp.color_primaries;
  tone_mapped->matrixCoefficients = cicp.matrix_coefficients;

  return WriteImage(tone_mapped.get(), arg_output_filename_,
                    arg_image_encode_.quality, arg_image_encode_.speed);
}

}  // namespace avif
