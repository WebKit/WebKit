// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

#include "webm/buffer_reader.h"
#include "webm/callback.h"
#include "webm/file_reader.h"
#include "webm/reader.h"
#include "webm/status.h"
#include "webm/webm_parser.h"

using webm::BufferReader;
using webm::Callback;
using webm::FileReader;
using webm::Reader;
using webm::Status;
using webm::WebmParser;

static int Run(Reader* reader) {
  Callback callback;
  WebmParser parser;

#if WEBM_FUZZER_SEEK_FIRST
  parser.DidSeek();
#endif

  Status status(-1);
  try {
    status = parser.Feed(&callback, reader);
  } catch (std::bad_alloc&) {
    // Failed allocations are okay. MSan doesn't throw std::bad_alloc, but
    // someday it might.
  }

  // BufferReader/FileReader should never return either of the following codes,
  // which means the parser never should too:
  assert(status.code != Status::kWouldBlock);
  assert(status.code != Status::kOkPartial);

  // Only the following ranges have status codes defined:
  assert((-1031 <= status.code && status.code <= -1025) ||
         (-3 <= status.code && status.code <= 0));

  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
  BufferReader reader(std::vector<std::uint8_t>(data, data + size));
  return Run(&reader);
}

#if __AFL_COMPILER
int main(int argc, char* argv[]) {
  FILE* file = (argc == 2) ? std::fopen(argv[1], "rb")
                           : std::freopen(nullptr, "rb", stdin);
  if (!file) {
    std::cerr << "File cannot be opened\n";
    return EXIT_FAILURE;
  }

  FileReader reader(file);
  return Run(&reader);
}
#endif
