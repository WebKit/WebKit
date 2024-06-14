//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/wgpu/wgpu_utils.h"

#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"

namespace rx
{

namespace webgpu
{

ContextWgpu *GetImpl(const gl::Context *context)
{
    return GetImplAs<ContextWgpu>(context);
}

DisplayWgpu *GetDisplay(const gl::Context *context)
{
    ContextWgpu *contextWgpu = GetImpl(context);
    return contextWgpu->getDisplay();
}

wgpu::Device GetDevice(const gl::Context *context)
{
    DisplayWgpu *display = GetDisplay(context);
    return display->getDevice();
}

wgpu::Instance GetInstance(const gl::Context *context)
{
    DisplayWgpu *display = GetDisplay(context);
    return display->getInstance();
}

wgpu::RenderPassColorAttachment CreateNewClearColorAttachment(wgpu::Color clearValue,
                                                              uint32_t depthSlice,
                                                              wgpu::TextureView textureView)
{
    wgpu::RenderPassColorAttachment colorAttachment;
    colorAttachment.view       = textureView;
    colorAttachment.depthSlice = depthSlice;
    colorAttachment.loadOp     = wgpu::LoadOp::Clear;
    colorAttachment.storeOp    = wgpu::StoreOp::Store;
    colorAttachment.clearValue = clearValue;

    return colorAttachment;
}

bool IsWgpuError(wgpu::WaitStatus waitStatus)
{
    return waitStatus != wgpu::WaitStatus::Success;
}

bool IsWgpuError(WGPUBufferMapAsyncStatus mapBufferStatus)
{
    return mapBufferStatus != WGPUBufferMapAsyncStatus_Success;
}

ClearValuesArray::ClearValuesArray() : mValues{}, mEnabled{} {}
ClearValuesArray::~ClearValuesArray() = default;

ClearValuesArray::ClearValuesArray(const ClearValuesArray &other)          = default;
ClearValuesArray &ClearValuesArray::operator=(const ClearValuesArray &rhs) = default;

void ClearValuesArray::store(uint32_t index, ClearValues clearValues)
{
    mValues[index] = clearValues;
    mEnabled.set(index);
}

gl::DrawBufferMask ClearValuesArray::getColorMask() const
{
    return gl::DrawBufferMask(mEnabled.bits() & kUnpackedColorBuffersMask);
}

void EnsureCapsInitialized(const wgpu::Device &device, gl::Caps *nativeCaps)
{
    wgpu::SupportedLimits limitsWgpu = {};
    device.GetLimits(&limitsWgpu);

    nativeCaps->maxElementIndex       = std::numeric_limits<GLuint>::max() - 1;
    nativeCaps->max3DTextureSize      = rx::LimitToInt(limitsWgpu.limits.maxTextureDimension3D);
    nativeCaps->max2DTextureSize      = rx::LimitToInt(limitsWgpu.limits.maxTextureDimension2D);
    nativeCaps->maxArrayTextureLayers = rx::LimitToInt(limitsWgpu.limits.maxTextureArrayLayers);
    nativeCaps->maxCubeMapTextureSize = rx::LimitToInt(limitsWgpu.limits.maxTextureDimension2D);
    nativeCaps->maxRenderbufferSize   = rx::LimitToInt(limitsWgpu.limits.maxTextureDimension2D);

    nativeCaps->maxDrawBuffers       = rx::LimitToInt(limitsWgpu.limits.maxColorAttachments);
    nativeCaps->maxFramebufferWidth  = rx::LimitToInt(limitsWgpu.limits.maxTextureDimension2D);
    nativeCaps->maxFramebufferHeight = rx::LimitToInt(limitsWgpu.limits.maxTextureDimension2D);
    nativeCaps->maxColorAttachments  = rx::LimitToInt(limitsWgpu.limits.maxColorAttachments);

    nativeCaps->maxVertexAttribStride =
        rx::LimitToInt(limitsWgpu.limits.maxVertexBufferArrayStride);

    nativeCaps->maxVertexAttributes = rx::LimitToInt(limitsWgpu.limits.maxVertexAttributes);

    nativeCaps->maxTextureBufferSize = rx::LimitToInt(limitsWgpu.limits.maxBufferSize);
}
}  // namespace webgpu

namespace wgpu_gl
{
gl::LevelIndex getLevelIndex(webgpu::LevelIndex levelWgpu, gl::LevelIndex baseLevel)
{
    return gl::LevelIndex(levelWgpu.get() + baseLevel.get());
}

gl::Extents getExtents(wgpu::Extent3D wgpuExtent)
{
    gl::Extents glExtent;
    glExtent.width  = wgpuExtent.width;
    glExtent.height = wgpuExtent.height;
    glExtent.depth  = wgpuExtent.depthOrArrayLayers;
    return glExtent;
}
}  // namespace wgpu_gl

namespace gl_wgpu
{
webgpu::LevelIndex getLevelIndex(gl::LevelIndex levelGl, gl::LevelIndex baseLevel)
{
    ASSERT(baseLevel <= levelGl);
    return webgpu::LevelIndex(levelGl.get() - baseLevel.get());
}

wgpu::Extent3D getExtent3D(const gl::Extents &glExtent)
{
    wgpu::Extent3D wgpuExtent;
    wgpuExtent.width              = glExtent.width;
    wgpuExtent.height             = glExtent.height;
    wgpuExtent.depthOrArrayLayers = glExtent.depth;
    return wgpuExtent;
}

wgpu::TextureDimension getWgpuTextureDimension(gl::TextureType glTextureType)
{
    wgpu::TextureDimension dimension = {};
    switch (glTextureType)
    {
        case gl::TextureType::_2D:
        case gl::TextureType::_2DMultisample:
        case gl::TextureType::Rectangle:
        case gl::TextureType::External:
        case gl::TextureType::Buffer:
            dimension = wgpu::TextureDimension::e2D;
            break;
        case gl::TextureType::_2DArray:
        case gl::TextureType::_2DMultisampleArray:
        case gl::TextureType::_3D:
        case gl::TextureType::CubeMap:
        case gl::TextureType::CubeMapArray:
        case gl::TextureType::VideoImage:
            dimension = wgpu::TextureDimension::e3D;
            break;
        default:
            break;
    }
    return dimension;
}
}  // namespace gl_wgpu
}  // namespace rx
