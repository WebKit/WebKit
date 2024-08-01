/* Copyright (c) 2024, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include "test_data.h"

#include <stdio.h>
#include <stdlib.h>

#include "file_util.h"

#if defined(BORINGSSL_USE_BAZEL_RUNFILES)
#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;
#endif

#if !defined(BORINGSSL_CUSTOM_GET_TEST_DATA)
std::string GetTestData(const char *path) {
#if defined(BORINGSSL_USE_BAZEL_RUNFILES)
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
  if (runfiles == nullptr) {
    fprintf(stderr, "Could not initialize runfiles: %s\n", error.c_str());
    abort();
  }

  std::string full_path = runfiles->Rlocation(std::string("boringssl/") + path);
  if (full_path.empty()) {
    fprintf(stderr, "Could not find runfile '%s'.\n", path);
    abort();
  }
#else
  const char *root = getenv("BORINGSSL_TEST_DATA_ROOT");
  root = root != nullptr ? root : ".";

  std::string full_path = root;
  full_path.push_back('/');
  full_path.append(path);
#endif

  bssl::ScopedFILE file(fopen(full_path.c_str(), "rb"));
  if (file == nullptr) {
    fprintf(stderr, "Could not open '%s'.\n", full_path.c_str());
    abort();
  }

  std::string ret;
  for (;;) {
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf), file.get());
    if (n == 0) {
      if (feof(file.get())) {
        return ret;
      }
      fprintf(stderr, "Error reading from '%s'.\n", full_path.c_str());
      abort();
    }
    ret.append(buf, n);
  }
}
#endif  // !BORINGSSL_CUSTOM_GET_TEST_DATA
