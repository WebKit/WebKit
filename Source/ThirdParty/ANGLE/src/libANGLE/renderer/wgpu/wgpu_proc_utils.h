//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_RENDERER_WGPU_WGPU_PROC_UTILS_H_
#define LIBANGLE_RENDERER_WGPU_WGPU_PROC_UTILS_H_

struct DawnProcTable;

namespace rx
{
namespace webgpu
{
const DawnProcTable &GetDefaultProcTable();
}
}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_WGPU_PROC_UTILS_H
