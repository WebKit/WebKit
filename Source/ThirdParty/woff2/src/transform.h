// Copyright 2013 Google Inc. All Rights Reserved.
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
// Library for preprocessing fonts as part of the WOFF 2.0 conversion.

#ifndef WOFF2_TRANSFORM_H_
#define WOFF2_TRANSFORM_H_

#include "./font.h"

namespace woff2 {

// Adds the transformed versions of the glyf and loca tables to the font. The
// transformed loca table has zero length. The tag of the transformed tables is
// derived from the original tag by flipping the MSBs of every byte.
bool TransformGlyfAndLocaTables(Font* font);

// Apply transformation to hmtx table if applicable for this font.
bool TransformHmtxTable(Font* font);

} // namespace woff2

#endif  // WOFF2_TRANSFORM_H_
