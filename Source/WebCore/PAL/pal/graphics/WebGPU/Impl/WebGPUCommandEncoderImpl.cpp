/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGPUCommandEncoderImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBufferImpl.h"
#include "WebGPUCommandBufferImpl.h"
#include "WebGPUComputePassEncoderImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUQuerySetImpl.h"
#include "WebGPURenderPassEncoderImpl.h"
#include "WebGPUTextureImpl.h"
#include "WebGPUTextureViewImpl.h"
#include <WebGPU/WebGPUExt.h>

namespace PAL::WebGPU {

CommandEncoderImpl::CommandEncoderImpl(WGPUCommandEncoder CommandEncoder, ConvertToBackingContext& convertToBackingContext)
    : m_backing(CommandEncoder)
    , m_convertToBackingContext(convertToBackingContext)
{
}

CommandEncoderImpl::~CommandEncoderImpl()
{
    wgpuCommandEncoderRelease(m_backing);
}

Ref<RenderPassEncoder> CommandEncoderImpl::beginRenderPass(const RenderPassDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    Vector<WGPURenderPassColorAttachment> colorAttachments;
    for (const auto& colorAttachment : descriptor.colorAttachments) {
        if (colorAttachment) {
            colorAttachments.append(WGPURenderPassColorAttachment {
                m_convertToBackingContext->convertToBacking(colorAttachment->view),
                colorAttachment->resolveTarget ? m_convertToBackingContext->convertToBacking(*colorAttachment->resolveTarget) : nullptr,
                m_convertToBackingContext->convertToBacking(colorAttachment->loadOp),
                m_convertToBackingContext->convertToBacking(colorAttachment->storeOp),
                colorAttachment->clearValue ? m_convertToBackingContext->convertToBacking(*colorAttachment->clearValue) : WGPUColor { 0, 0, 0, 0 },
            });
        } else
            colorAttachments.append({
                nullptr,
                nullptr,
                WGPULoadOp_Clear,
                WGPUStoreOp_Discard,
                { 0, 0, 0, 0 },
            });
    }

    std::optional<WGPURenderPassDepthStencilAttachment> depthStencilAttachment;
    if (descriptor.depthStencilAttachment) {
        depthStencilAttachment = WGPURenderPassDepthStencilAttachment {
            m_convertToBackingContext->convertToBacking(descriptor.depthStencilAttachment->view),
            descriptor.depthStencilAttachment->depthLoadOp ? m_convertToBackingContext->convertToBacking(*descriptor.depthStencilAttachment->depthLoadOp) : WGPULoadOp_Clear,
            descriptor.depthStencilAttachment->depthStoreOp ? m_convertToBackingContext->convertToBacking(*descriptor.depthStencilAttachment->depthStoreOp) : WGPUStoreOp_Discard,
            descriptor.depthStencilAttachment->depthClearValue,
            descriptor.depthStencilAttachment->depthReadOnly,
            descriptor.depthStencilAttachment->stencilLoadOp ? m_convertToBackingContext->convertToBacking(*descriptor.depthStencilAttachment->stencilLoadOp) : WGPULoadOp_Clear,
            descriptor.depthStencilAttachment->stencilStoreOp ? m_convertToBackingContext->convertToBacking(*descriptor.depthStencilAttachment->stencilStoreOp) : WGPUStoreOp_Discard,
            descriptor.depthStencilAttachment->stencilClearValue,
            descriptor.depthStencilAttachment->stencilReadOnly,
        };
    }

    Vector<WGPURenderPassTimestampWrite> timestampWrites;
    for (const auto& timestampWrite : descriptor.timestampWrites) {
        timestampWrites.append(WGPURenderPassTimestampWrite {
            m_convertToBackingContext->convertToBacking(timestampWrite.querySet),
            timestampWrite.queryIndex,
            m_convertToBackingContext->convertToBacking(timestampWrite.location),
        });
    }

    WGPURenderPassDescriptor backingDescriptor {
        nullptr,
        label.data(),
        static_cast<uint32_t>(colorAttachments.size()),
        colorAttachments.data(),
        depthStencilAttachment ? &depthStencilAttachment.value() : nullptr,
        descriptor.occlusionQuerySet ? m_convertToBackingContext->convertToBacking(*descriptor.occlusionQuerySet) : nullptr,
        static_cast<uint32_t>(timestampWrites.size()),
        timestampWrites.data(),
    };

    return RenderPassEncoderImpl::create(wgpuCommandEncoderBeginRenderPass(m_backing, &backingDescriptor), m_convertToBackingContext);
}

Ref<ComputePassEncoder> CommandEncoderImpl::beginComputePass(const std::optional<ComputePassDescriptor>& descriptor)
{
    CString label = descriptor ? descriptor->label.utf8() : CString("");

    Vector<WGPUComputePassTimestampWrite> timestampWrites;
    if (descriptor) {
        for (const auto& timestampWrite : descriptor->timestampWrites) {
            timestampWrites.append(WGPUComputePassTimestampWrite {
                m_convertToBackingContext->convertToBacking(timestampWrite.querySet),
                timestampWrite.queryIndex,
                m_convertToBackingContext->convertToBacking(timestampWrite.location),
            });
        }
    }

    WGPUComputePassDescriptor backingDescriptor {
        nullptr,
        label.data(),
        static_cast<uint32_t>(timestampWrites.size()),
        timestampWrites.data(),
    };

    return ComputePassEncoderImpl::create(wgpuCommandEncoderBeginComputePass(m_backing, &backingDescriptor), m_convertToBackingContext);
}

void CommandEncoderImpl::copyBufferToBuffer(
    const Buffer& source,
    Size64 sourceOffset,
    const Buffer& destination,
    Size64 destinationOffset,
    Size64 size)
{
    wgpuCommandEncoderCopyBufferToBuffer(m_backing, m_convertToBackingContext->convertToBacking(source), sourceOffset, m_convertToBackingContext->convertToBacking(destination), destinationOffset, size);
}

void CommandEncoderImpl::copyBufferToTexture(
    const ImageCopyBuffer& source,
    const ImageCopyTexture& destination,
    const Extent3D& copySize)
{
    WGPUImageCopyBuffer backingSource {
        nullptr, {
            nullptr,
            source.offset,
            source.bytesPerRow.value_or(0),
            source.rowsPerImage.value_or(0),
        },
        m_convertToBackingContext->convertToBacking(source.buffer),
    };

    WGPUImageCopyTexture backingDestination {
        nullptr,
        m_convertToBackingContext->convertToBacking(destination.texture),
        destination.mipLevel,
        destination.origin ? m_convertToBackingContext->convertToBacking(*destination.origin) : WGPUOrigin3D { 0, 0, 0 },
        m_convertToBackingContext->convertToBacking(destination.aspect),
    };

    WGPUExtent3D backingCopySize = m_convertToBackingContext->convertToBacking(copySize);

    wgpuCommandEncoderCopyBufferToTexture(m_backing, &backingSource, &backingDestination, &backingCopySize);
}

void CommandEncoderImpl::copyTextureToBuffer(
    const ImageCopyTexture& source,
    const ImageCopyBuffer& destination,
    const Extent3D& copySize)
{
    WGPUImageCopyTexture backingSource {
        nullptr,
        m_convertToBackingContext->convertToBacking(source.texture),
        source.mipLevel,
        source.origin ? m_convertToBackingContext->convertToBacking(*source.origin) : WGPUOrigin3D { 0, 0, 0 },
        m_convertToBackingContext->convertToBacking(source.aspect),
    };

    WGPUImageCopyBuffer backingDestination {
        nullptr, {
            nullptr,
            destination.offset,
            destination.bytesPerRow.value_or(0),
            destination.rowsPerImage.value_or(0),
        },
        m_convertToBackingContext->convertToBacking(destination.buffer),
    };

    WGPUExtent3D backingCopySize = m_convertToBackingContext->convertToBacking(copySize);

    wgpuCommandEncoderCopyTextureToBuffer(m_backing, &backingSource, &backingDestination, &backingCopySize);
}

void CommandEncoderImpl::copyTextureToTexture(
    const ImageCopyTexture& source,
    const ImageCopyTexture& destination,
    const Extent3D& copySize)
{
    WGPUImageCopyTexture backingSource {
        nullptr,
        m_convertToBackingContext->convertToBacking(source.texture),
        source.mipLevel,
        source.origin ? m_convertToBackingContext->convertToBacking(*source.origin) : WGPUOrigin3D { 0, 0, 0 },
        m_convertToBackingContext->convertToBacking(source.aspect),
    };

    WGPUImageCopyTexture backingDestination {
        nullptr,
        m_convertToBackingContext->convertToBacking(destination.texture),
        destination.mipLevel,
        destination.origin ? m_convertToBackingContext->convertToBacking(*destination.origin) : WGPUOrigin3D { 0, 0, 0 },
        m_convertToBackingContext->convertToBacking(destination.aspect),
    };

    WGPUExtent3D backingCopySize = m_convertToBackingContext->convertToBacking(copySize);

    wgpuCommandEncoderCopyTextureToTexture(m_backing, &backingSource, &backingDestination, &backingCopySize);
}

void CommandEncoderImpl::clearBuffer(
    const Buffer& buffer,
    Size64 offset,
    std::optional<Size64> size)
{
    wgpuCommandEncoderClearBuffer(m_backing, m_convertToBackingContext->convertToBacking(buffer), offset, size.value_or(WGPU_WHOLE_SIZE));
}

void CommandEncoderImpl::pushDebugGroup(String&& groupLabel)
{
    wgpuCommandEncoderPushDebugGroup(m_backing, groupLabel.utf8().data());
}

void CommandEncoderImpl::popDebugGroup()
{
    wgpuCommandEncoderPopDebugGroup(m_backing);
}

void CommandEncoderImpl::insertDebugMarker(String&& markerLabel)
{
    wgpuCommandEncoderInsertDebugMarker(m_backing, markerLabel.utf8().data());
}

void CommandEncoderImpl::writeTimestamp(const QuerySet& querySet, Size32 queryIndex)
{
    wgpuCommandEncoderWriteTimestamp(m_backing, m_convertToBackingContext->convertToBacking(querySet), queryIndex);
}

void CommandEncoderImpl::resolveQuerySet(
    const QuerySet& querySet,
    Size32 firstQuery,
    Size32 queryCount,
    const Buffer& destination,
    Size64 destinationOffset)
{
    wgpuCommandEncoderResolveQuerySet(m_backing, m_convertToBackingContext->convertToBacking(querySet), firstQuery, queryCount, m_convertToBackingContext->convertToBacking(destination), destinationOffset);
}

Ref<CommandBuffer> CommandEncoderImpl::finish(const CommandBufferDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    WGPUCommandBufferDescriptor backingDescriptor {
        nullptr,
        label.data(),
    };

    return CommandBufferImpl::create(wgpuCommandEncoderFinish(m_backing, &backingDescriptor), m_convertToBackingContext);
}

void CommandEncoderImpl::setLabelInternal(const String& label)
{
    wgpuCommandEncoderSetLabel(m_backing, label.utf8().data());
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
