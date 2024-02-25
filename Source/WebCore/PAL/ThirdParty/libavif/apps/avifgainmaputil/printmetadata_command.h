// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_APPS_AVIFGAINMAPUTIL_PRINTMETADATA_COMMAND_H_
#define LIBAVIF_APPS_AVIFGAINMAPUTIL_PRINTMETADATA_COMMAND_H_

#include "avif/avif.h"
#include "program_command.h"

namespace avif {

class PrintMetadataCommand : public ProgramCommand {
 public:
  PrintMetadataCommand();
  avifResult Run() override;

 private:
  argparse::ArgValue<std::string> arg_input_filename_;
};

}  // namespace avif

#endif  // LIBAVIF_APPS_AVIFGAINMAPUTIL_PRINTMETADATA_COMMAND_H_
