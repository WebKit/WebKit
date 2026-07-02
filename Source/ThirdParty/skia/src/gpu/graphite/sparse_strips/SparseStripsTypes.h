/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef skgpu_graphite_sparse_strips_SparseStripsTypes_DEFINED
#define skgpu_graphite_sparse_strips_SparseStripsTypes_DEFINED

#include "include/core/SkPoint.h"

namespace skgpu::graphite {

enum class FlattenMode {
    kScalar,
    kSimd,
};

struct Line {
    SkPoint p0;
    SkPoint p1;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_sparse_strips_SparseStripsTypes_DEFINED
