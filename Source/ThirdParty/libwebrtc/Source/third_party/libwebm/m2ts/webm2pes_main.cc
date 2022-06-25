// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "m2ts/webm2pes.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void Usage(const char* argv[]) {
  printf("Usage: %s <WebM file> <output file>", argv[0]);
}

}  // namespace

int main(int argc, const char* argv[]) {
  if (argc < 3) {
    Usage(argv);
    return EXIT_FAILURE;
  }

  const std::string input_path = argv[1];
  const std::string output_path = argv[2];

  libwebm::Webm2Pes converter(input_path, output_path);
  return converter.ConvertToFile() == true ? EXIT_SUCCESS : EXIT_FAILURE;
}
