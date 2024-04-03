// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "extractgainmap_command.h"

#include "avif/avif_cxx.h"
#include "imageio.h"

namespace avif {

ExtractGainMapCommand::ExtractGainMapCommand()
    : ProgramCommand("extractgainmap",
                     "Saves the gain map of an avif file as an image") {
  argparse_.add_argument(arg_input_filename_, "input_filename");
  argparse_.add_argument(arg_output_filename_, "output_filename");
  arg_image_encode_.Init(argparse_, /*can_have_alpha=*/false);
}

avifResult ExtractGainMapCommand::Run() {
  DecoderPtr decoder(avifDecoderCreate());
  if (decoder == NULL) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  decoder->enableDecodingGainMap = true;
  decoder->ignoreColorAndAlpha = true;

  avifResult result =
      ReadAvif(decoder.get(), arg_input_filename_, /*ignore_profile=*/true);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  if (decoder->image->gainMap == nullptr ||
      decoder->image->gainMap->image == nullptr) {
    std::cerr << "Input image " << arg_input_filename_
              << " does not contain a gain map\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  }

  return WriteImage(decoder->image->gainMap->image, arg_output_filename_,
                    arg_image_encode_.quality, arg_image_encode_.speed);
}

}  // namespace avif
