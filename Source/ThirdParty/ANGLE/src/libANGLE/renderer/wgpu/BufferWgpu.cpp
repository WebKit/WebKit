//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferWgpu.cpp:
//    Implements the class methods for BufferWgpu.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_libc_calls
#endif

#include "libANGLE/renderer/wgpu/BufferWgpu.h"

#include "common/debug.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{
namespace
{
// Based on a buffer binding target, compute the default wgpu usage flags. More can be added if the
// buffer is used in new ways.
WGPUBufferUsage GetDefaultWGPUBufferUsageForBinding(gl::BufferBinding binding)
{
    switch (binding)
    {
        case gl::BufferBinding::Array:
        case gl::BufferBinding::ElementArray:
            return WGPUBufferUsage_Vertex | WGPUBufferUsage_Index | WGPUBufferUsage_CopySrc |
                   WGPUBufferUsage_CopyDst;

        case gl::BufferBinding::Uniform:
            return WGPUBufferUsage_Uniform | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;

        case gl::BufferBinding::PixelPack:
            return WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;

        case gl::BufferBinding::PixelUnpack:
            return WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;

        case gl::BufferBinding::CopyRead:
        case gl::BufferBinding::CopyWrite:
        case gl::BufferBinding::ShaderStorage:
        case gl::BufferBinding::Texture:
        case gl::BufferBinding::TransformFeedback:
        case gl::BufferBinding::DispatchIndirect:
        case gl::BufferBinding::DrawIndirect:
        case gl::BufferBinding::AtomicCounter:
            UNIMPLEMENTED();
            return WGPUBufferUsage_None;

        default:
            UNREACHABLE();
            return WGPUBufferUsage_None;
    }
}

}  // namespace

BufferWgpu::BufferWgpu(const gl::BufferState &state) : BufferImpl(state) {}

BufferWgpu::~BufferWgpu() {}

angle::Result BufferWgpu::setData(const gl::Context *context,
                                  gl::BufferBinding target,
                                  const void *data,
                                  size_t size,
                                  gl::BufferUsage usage,
                                  BufferFeedback *feedback)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);
    const DawnProcTable *wgpu   = webgpu::GetProcs(contextWgpu);
    webgpu::DeviceHandle device = webgpu::GetDevice(context);

    bool hasData = data && size > 0;

    // Allocate a new buffer if the current one is invalid, the size is different, or the current
    // buffer cannot be mapped for writing when data needs to be uploaded.
    if (!mBuffer.valid() || mBuffer.requestedSize() != size ||
        (hasData && !mBuffer.canMapForWrite()))
    {
        // Allocate a new buffer
        ANGLE_TRY(mBuffer.initBuffer(wgpu, device, size,
                                     GetDefaultWGPUBufferUsageForBinding(target),
                                     webgpu::MapAtCreation::Yes));
    }

    if (hasData)
    {
        ASSERT(mBuffer.canMapForWrite());

        if (!mBuffer.getMappedState().has_value())
        {
            ANGLE_TRY(mBuffer.mapImmediate(contextWgpu, WGPUMapMode_Write, 0, size));
        }

        uint8_t *mappedData = mBuffer.getMapWritePointer(0, size);
        memcpy(mappedData, data, size);
    }

    return angle::Result::Continue;
}

angle::Result BufferWgpu::setSubData(const gl::Context *context,
                                     gl::BufferBinding target,
                                     const void *data,
                                     size_t size,
                                     size_t offset,
                                     BufferFeedback *feedback)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);

    ASSERT(mBuffer.valid());
    if (mBuffer.canMapForWrite())
    {
        if (!mBuffer.getMappedState().has_value())
        {
            ANGLE_TRY(mBuffer.mapImmediate(contextWgpu, WGPUMapMode_Write, offset, size));
        }

        uint8_t *mappedData = mBuffer.getMapWritePointer(offset, size);
        memcpy(mappedData, data, size);
    }
    else
    {
        const DawnProcTable *wgpu = webgpu::GetProcs(context);
        // TODO: Upload into a staging buffer and copy to the destination buffer so that the copy
        // happens at the right point in time for command buffer recording.
        webgpu::QueueHandle queue = contextWgpu->getQueue();
        wgpu->queueWriteBuffer(queue.get(), mBuffer.getBuffer().get(), offset, data, size);
    }

    return angle::Result::Continue;
}

angle::Result BufferWgpu::copySubData(const gl::Context *context,
                                      BufferImpl *source,
                                      GLintptr sourceOffset,
                                      GLintptr destOffset,
                                      GLsizeiptr size,
                                      BufferFeedback *feedback)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::map(const gl::Context *context,
                              GLenum access,
                              void **mapPtr,
                              BufferFeedback *feedback)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::mapRange(const gl::Context *context,
                                   size_t offset,
                                   size_t length,
                                   GLbitfield access,
                                   void **mapPtr,
                                   BufferFeedback *feedback)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::unmap(const gl::Context *context,
                                GLboolean *result,
                                BufferFeedback *feedback)
{
    *result = GL_TRUE;
    return angle::Result::Continue;
}

angle::Result BufferWgpu::getIndexRange(const gl::Context *context,
                                        gl::DrawElementsType type,
                                        size_t offset,
                                        size_t count,
                                        bool primitiveRestartEnabled,
                                        gl::IndexRange *outRange)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);

    const GLuint typeBytes = gl::GetDrawElementsTypeSize(type);

    webgpu::BufferReadback readback;
    ANGLE_TRY(mBuffer.readDataImmediate(contextWgpu, offset, count * typeBytes,
                                        webgpu::RenderPassClosureReason::IndexRangeReadback,
                                        &readback));
    *outRange = gl::ComputeIndexRange(type, readback.data, count, primitiveRestartEnabled);

    return angle::Result::Continue;
}

}  // namespace rx
