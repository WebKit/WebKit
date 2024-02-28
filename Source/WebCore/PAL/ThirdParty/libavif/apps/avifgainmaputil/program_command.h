// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_APPS_AVIFGAINMAPUTIL_PROGRAM_COMMAND_H_
#define LIBAVIF_APPS_AVIFGAINMAPUTIL_PROGRAM_COMMAND_H_

#include <string>

#include "argparse.hpp"
#include "avif/avif.h"

namespace avif {

// A command that can be invoked by name (similar to how 'git' has commands like
// 'commit', 'checkout', etc.)
// NOTE: "avifgainmaputil" is currently hardcoded in the implementation (for
// help messages).
class ProgramCommand {
 public:
  // 'name' is the command that should be used to invoke the command on the
  // command line.
  // 'description' should be a one line description of what the command does.
  ProgramCommand(const std::string& name, const std::string& description);

  virtual ~ProgramCommand() = default;

  // Parses command line arguments. Should be called before Run().
  avifResult ParseArgs(int argc, const char* const argv[]);

  // Runs the command.
  virtual avifResult Run() = 0;

  std::string name() const { return name_; }
  std::string description() const { return description_; }

  // Prints this command's help on stdout.
  void PrintUsage();

 protected:
  argparse::ArgumentParser argparse_;

 private:
  std::string name_;
  std::string description_;
};

//------------------------------------------------------------------------------
// Utilities for flag parsing.

// avifPixelFormat converter for use with argparse.
// Actually converts to int, converting to avifPixelFormat didn't seem to
// compile.
struct PixelFormatConverter {
  // Methods expected by argparse.
  argparse::ConvertedValue<int> from_str(const std::string& str);
  std::vector<std::string> default_choices();
};

struct CicpValues {
  avifColorPrimaries color_primaries;
  avifTransferCharacteristics transfer_characteristics;
  avifMatrixCoefficients matrix_coefficients;
};

// CicpValues converter for use with argparse.
struct CicpConverter {
  // Methods expected by argparse.
  argparse::ConvertedValue<CicpValues> from_str(const std::string& str);
  std::vector<std::string> default_choices();
};

// Basic flags for image writing.
struct BasicImageEncodeArgs {
  argparse::ArgValue<int> speed;
  argparse::ArgValue<int> quality;
  argparse::ArgValue<int> quality_alpha;

  // can_have_alpha should be true if the image can have alpha and the
  // output format can be avif.
  void Init(argparse::ArgumentParser& argparse, bool can_have_alpha) {
    argparse.add_argument(speed, "--speed", "-s")
        .help("Encoder speed (0-10, slowest-fastest)")
        .default_value("6");
    argparse.add_argument(quality, "--qcolor", "-q")
        .help((can_have_alpha
                   ? "Quality for color (0-100, where 100 is lossless)"
                   : "Quality (0-100, where 100 is lossless)"))
        .default_value("60");
    if (can_have_alpha) {
      argparse.add_argument(quality_alpha, "--qalpha")
          .help("Quality for alpha (0-100, where 100 is lossless)")
          .default_value("100");
    }
  }
};

// Flags relevant when reading jpeg/png.
struct ImageReadArgs {
  argparse::ArgValue<int> depth;
  argparse::ArgValue<int> pixel_format;
  argparse::ArgValue<bool> ignore_profile;

  void Init(argparse::ArgumentParser& argparse) {
    argparse
        .add_argument<int, PixelFormatConverter>(pixel_format, "--yuv", "-y")
        .help("Output YUV format for avif (default = automatic)");
    argparse.add_argument(depth, "--depth", "-d")
        .choices({"0", "8", "10", "12"})
        .help("Output depth (0 = automatic)");
    argparse.add_argument(ignore_profile, "--ignore-profile")
        .help(
            "If the input file contains an embedded color profile, ignore it "
            "(no-op if absent)")
        .action(argparse::Action::STORE_TRUE)
        .default_value("false");
  }
};

// Helper to parse flags that contain several delimited values.
template <typename T>
bool ParseList(std::string to_parse, char delim, int expected_num,
               std::vector<T>* out) {
  std::stringstream ss(to_parse);
  std::string part;
  T parsed;
  while (std::getline(ss, part, delim)) {
    std::istringstream is(part);
    is >> parsed;
    if (is.bad()) {
      return false;
    }
    out->push_back(parsed);
  }
  if (expected_num > 0 && (int)out->size() != expected_num) {
    return false;
  }
  return true;
}

}  // namespace avif

#endif  // LIBAVIF_APPS_AVIFGAINMAPUTIL_PROGRAM_COMMAND_H_
