//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_RENDERER_WGPU_WGPU_UTILS_H_
#define LIBANGLE_RENDERER_WGPU_WGPU_UTILS_H_

#include <dawn/webgpu_cpp.h>
#include <stdint.h>

#include "libANGLE/Caps.h"
#include "libANGLE/Error.h"
#include "libANGLE/angletypes.h"

#define ANGLE_WGPU_TRY(context, command)                                                     \
    do                                                                                       \
    {                                                                                        \
        auto ANGLE_LOCAL_VAR = command;                                                      \
        if (ANGLE_UNLIKELY(::rx::webgpu::IsError(ANGLE_LOCAL_VAR)))                          \
        {                                                                                    \
            (context)->handleError(GL_INVALID_OPERATION, "Internal WebGPU error.", __FILE__, \
                                   ANGLE_FUNCTION, __LINE__);                                \
            return angle::Result::Stop;                                                      \
        }                                                                                    \
    } while (0)

namespace rx
{

class ContextWgpu;
class DisplayWgpu;

namespace webgpu
{
// WebGPU image level index.
using LevelIndex = gl::LevelIndexWrapper<uint32_t>;

enum class RenderPassClosureReason
{
    NewRenderPass,

    InvalidEnum,
    EnumCount = InvalidEnum,
};
void EnsureCapsInitialized(const wgpu::Device &device, gl::Caps *nativeCaps);

ContextWgpu *GetImpl(const gl::Context *context);
DisplayWgpu *GetDisplay(const gl::Context *context);
wgpu::Device GetDevice(const gl::Context *context);
wgpu::Instance GetInstance(const gl::Context *context);

bool IsError(wgpu::WaitStatus waitStatus);
bool IsError(WGPUBufferMapAsyncStatus mapBufferStatus);
}  // namespace webgpu

namespace wgpu_gl
{
gl::LevelIndex getLevelIndex(webgpu::LevelIndex levelWgpu, gl::LevelIndex baseLevel);
gl::Extents getExtents(wgpu::Extent3D wgpuExtent);
}  // namespace wgpu_gl

namespace gl_wgpu
{
webgpu::LevelIndex getLevelIndex(gl::LevelIndex levelGl, gl::LevelIndex baseLevel);
wgpu::TextureDimension getWgpuTextureDimension(gl::TextureType glTextureType);
wgpu::Extent3D getExtent3D(const gl::Extents &glExtent);
}  // namespace gl_wgpu

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_WGPU_UTILS_H_
