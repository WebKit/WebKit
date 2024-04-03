// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "printmetadata_command.h"

#include <cassert>
#include <iomanip>

#include "avif/avif_cxx.h"

namespace avif {

namespace {
template <typename T>
std::string FormatFraction(T numerator, uint32_t denominator) {
  std::stringstream stream;
  stream << (denominator != 0 ? (double)numerator / denominator : 0)
         << " (as fraction: " << numerator << "/" << denominator << ")";
  return stream.str();
}

template <typename T>
std::string FormatFractions(const T numerator[3],
                            const uint32_t denominator[3]) {
  std::stringstream stream;
  const int w = 40;
  stream << "R " << std::left << std::setw(w)
         << FormatFraction(numerator[0], denominator[0]) << " G " << std::left
         << std::setw(w) << FormatFraction(numerator[1], denominator[1])
         << " B " << std::left << std::setw(w)
         << FormatFraction(numerator[2], denominator[2]);
  return stream.str();
}
}  // namespace

PrintMetadataCommand::PrintMetadataCommand()
    : ProgramCommand("printmetadata",
                     "Prints the metadata of the gain map of an avif file") {
  argparse_.add_argument(arg_input_filename_, "input_filename");
}

avifResult PrintMetadataCommand::Run() {
  DecoderPtr decoder(avifDecoderCreate());
  if (decoder == NULL) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  decoder->enableParsingGainMapMetadata = true;

  avifResult result =
      avifDecoderSetIOFile(decoder.get(), arg_input_filename_.value().c_str());
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Cannot open file for read: " << arg_input_filename_ << "\n";
    return result;
  }
  result = avifDecoderParse(decoder.get());
  if (result != AVIF_RESULT_OK) {
    std::cerr << "Failed to parse image: " << avifResultToString(result) << " ("
              << decoder->diag.error << ")\n";
    return result;
  }
  if (!decoder->gainMapPresent) {
    std::cerr << "Input image " << arg_input_filename_
              << " does not contain a gain map\n";
    return AVIF_RESULT_INVALID_ARGUMENT;
  }
  assert(decoder->image->gainMap);

  const avifGainMapMetadata& metadata = decoder->image->gainMap->metadata;
  const int w = 20;
  std::cout << " * " << std::left << std::setw(w) << "Base headroom: "
            << FormatFraction(metadata.baseHdrHeadroomN,
                              metadata.baseHdrHeadroomD)
            << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Alternate headroom: "
            << FormatFraction(metadata.alternateHdrHeadroomN,
                              metadata.alternateHdrHeadroomD)
            << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Gain Map Min: "
            << FormatFractions(metadata.gainMapMinN, metadata.gainMapMinD)
            << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Gain Map Max: "
            << FormatFractions(metadata.gainMapMaxN, metadata.gainMapMaxD)
            << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Base Offset: "
            << FormatFractions(metadata.baseOffsetN, metadata.baseOffsetD)
            << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Alternate Offset: "
            << FormatFractions(metadata.alternateOffsetN,
                               metadata.alternateOffsetD)
            << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Gain Map Gamma: "
            << FormatFractions(metadata.gainMapGammaN, metadata.gainMapGammaD)
            << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Backward Direction: "
            << (metadata.backwardDirection ? "True" : "False") << "\n";
  std::cout << " * " << std::left << std::setw(w) << "Use Base Color Space: "
            << (metadata.useBaseColorSpace ? "True" : "False") << "\n";

  return AVIF_RESULT_OK;
}

}  // namespace avif
