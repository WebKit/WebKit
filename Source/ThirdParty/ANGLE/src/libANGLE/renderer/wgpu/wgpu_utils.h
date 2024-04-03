//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_RENDERER_WGPU_WGPU_UTILS_H_
#define LIBANGLE_RENDERER_WGPU_WGPU_UTILS_H_

#include <dawn/webgpu_cpp.h>
#include <stdint.h>

#include "libANGLE/Error.h"
#include "libANGLE/angletypes.h"

namespace webgpu
{

// WebGPU image level index.
using LevelIndex = gl::LevelIndexWrapper<uint32_t>;

}  // namespace webgpu

namespace wgpu_gl
{
gl::LevelIndex getLevelIndex(webgpu::LevelIndex levelWgpu, gl::LevelIndex baseLevel);
}  // namespace wgpu_gl

namespace gl_wgpu
{
webgpu::LevelIndex getLevelIndex(gl::LevelIndex levelGl, gl::LevelIndex baseLevel);
}  // namespace gl_wgpu

#endif  // LIBANGLE_RENDERER_WGPU_WGPU_UTILS_H_
