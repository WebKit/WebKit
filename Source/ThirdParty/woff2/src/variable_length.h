// Copyright 2015 Google Inc. All Rights Reserved.
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
// Helper functions for woff2 variable length types: 255UInt16 and UIntBase128

#ifndef WOFF2_VARIABLE_LENGTH_H_
#define WOFF2_VARIABLE_LENGTH_H_

#include <inttypes.h>
#include <vector>
#include "./buffer.h"

namespace woff2 {

size_t Size255UShort(uint16_t value);
bool Read255UShort(Buffer* buf, unsigned int* value);
void Write255UShort(std::vector<uint8_t>* out, int value);
void Store255UShort(int val, size_t* offset, uint8_t* dst);

size_t Base128Size(size_t n);
bool ReadBase128(Buffer* buf, uint32_t* value);
void StoreBase128(size_t len, size_t* offset, uint8_t* dst);

} // namespace woff2

#endif  // WOFF2_VARIABLE_LENGTH_H_

