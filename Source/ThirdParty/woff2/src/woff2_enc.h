// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Library for converting WOFF2 format font files to their TTF versions.

#ifndef WOFF2_WOFF2_ENC_H_
#define WOFF2_WOFF2_ENC_H_

#include <stddef.h>
#include <inttypes.h>
#include <string>

using std::string;


namespace woff2 {

struct WOFF2Params {
  WOFF2Params() : extended_metadata(""), brotli_quality(11),
                  allow_transforms(true) {}

  string extended_metadata;
  int brotli_quality;
  bool allow_transforms;
};

// Returns an upper bound on the size of the compressed file.
size_t MaxWOFF2CompressedSize(const uint8_t* data, size_t length);
size_t MaxWOFF2CompressedSize(const uint8_t* data, size_t length,
                              const string& extended_metadata);

// Compresses the font into the target buffer. *result_length should be at least
// the value returned by MaxWOFF2CompressedSize(), upon return, it is set to the
// actual compressed size. Returns true on successful compression.
bool ConvertTTFToWOFF2(const uint8_t *data, size_t length,
                       uint8_t *result, size_t *result_length);
bool ConvertTTFToWOFF2(const uint8_t *data, size_t length,
                       uint8_t *result, size_t *result_length,
                       const WOFF2Params& params);

} // namespace woff2

#endif  // WOFF2_WOFF2_ENC_H_
