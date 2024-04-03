// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_APPS_AVIFGAINMAPUTIL_SWAPBASE_COMMAND_H_
#define LIBAVIF_APPS_AVIFGAINMAPUTIL_SWAPBASE_COMMAND_H_

#include "avif/avif.h"
#include "program_command.h"

namespace avif {

// Given an 'image' with a gain map, tone maps it to get the "alternate" image,
// and saves it to 'output'.
avifResult ChangeBase(const avifImage& image, int depth,
                      avifPixelFormat yuvFormat, avifImage* output);

class SwapBaseCommand : public ProgramCommand {
 public:
  SwapBaseCommand();
  avifResult Run() override;

 private:
  argparse::ArgValue<std::string> arg_input_filename_;
  argparse::ArgValue<std::string> arg_output_filename_;
  ImageReadArgs arg_image_read_;
  BasicImageEncodeArgs arg_image_encode_;
  argparse::ArgValue<int> arg_gain_map_quality_;
};

}  // namespace avif

#endif  // LIBAVIF_APPS_AVIFGAINMAPUTIL_SWAPBASE_COMMAND_H_
