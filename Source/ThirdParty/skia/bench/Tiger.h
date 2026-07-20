/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef Tiger_DEFINED
#define Tiger_DEFINED

#include <cstdint>
#include <vector>

class SkPath;

namespace Tiger {
    static constexpr uint16_t kTigerWidth = 500;
    static constexpr uint16_t kTigerHeight = 600;
    static constexpr float kTigerWidthF = static_cast<float>(kTigerWidth);
    static constexpr float kTigerHeightF = static_cast<float>(kTigerHeight);

    std::vector<SkPath> GetTigerPaths();
}

#endif  // Tiger_DEFINED
