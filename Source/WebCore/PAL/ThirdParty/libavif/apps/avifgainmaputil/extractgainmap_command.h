// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_APPS_AVIFGAINMAPUTIL_EXTRACTGAINMAP_COMMAND_H_
#define LIBAVIF_APPS_AVIFGAINMAPUTIL_EXTRACTGAINMAP_COMMAND_H_

#include "avif/avif.h"
#include "program_command.h"

namespace avif {

class ExtractGainMapCommand : public ProgramCommand {
 public:
  ExtractGainMapCommand();
  avifResult Run() override;

 private:
  argparse::ArgValue<std::string> arg_input_filename_;
  argparse::ArgValue<std::string> arg_output_filename_;
  BasicImageEncodeArgs arg_image_encode_;
};

}  // namespace avif

#endif  // LIBAVIF_APPS_AVIFGAINMAPUTIL_EXTRACTGAINMAP_COMMAND_H_
