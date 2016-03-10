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

#ifndef WOFF2_WOFF2_DEC_H_
#define WOFF2_WOFF2_DEC_H_

#include <stddef.h>
#include <inttypes.h>

namespace woff2 {

// Compute the size of the final uncompressed font, or 0 on error.
size_t ComputeWOFF2FinalSize(const uint8_t *data, size_t length);

// Decompresses the font into the target buffer. The result_length should
// be the same as determined by ComputeFinalSize(). Returns true on successful
// decompression.
bool ConvertWOFF2ToTTF(uint8_t *result, size_t result_length,
                       const uint8_t *data, size_t length);

} // namespace woff2

#endif  // WOFF2_WOFF2_DEC_H_
