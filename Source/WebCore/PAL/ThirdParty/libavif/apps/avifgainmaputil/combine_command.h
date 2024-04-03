// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_APPS_AVIFGAINMAPUTIL_COMBINE_COMMAND_H_
#define LIBAVIF_APPS_AVIFGAINMAPUTIL_COMBINE_COMMAND_H_

#include "avif/avif.h"
#include "program_command.h"

namespace avif {

class CombineCommand : public ProgramCommand {
 public:
  CombineCommand();
  avifResult Run() override;

 private:
  argparse::ArgValue<std::string> arg_base_filename_;
  argparse::ArgValue<std::string> arg_alternate_filename_;
  argparse::ArgValue<std::string> arg_output_filename_;
  argparse::ArgValue<CicpValues> arg_base_cicp_;
  argparse::ArgValue<CicpValues> arg_alternate_cicp_;
  argparse::ArgValue<int> arg_downscaling_;
  argparse::ArgValue<int> arg_gain_map_quality_;
  argparse::ArgValue<int> arg_gain_map_depth_;
  argparse::ArgValue<int> arg_gain_map_pixel_format_;
  BasicImageEncodeArgs arg_image_encode_;
  ImageReadArgs arg_image_read_;
};

}  // namespace avif

#endif  // LIBAVIF_APPS_AVIFGAINMAPUTIL_COMBINE_COMMAND_H_
