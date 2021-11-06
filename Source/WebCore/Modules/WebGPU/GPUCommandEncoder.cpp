/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "config.h"
#include "GPUCommandEncoder.h"

#include "GPUBuffer.h"
#include "GPUQuerySet.h"

namespace WebCore {

String GPUCommandEncoder::label() const
{
    return StringImpl::empty();
}

void GPUCommandEncoder::setLabel(String&&)
{
}

Ref<GPURenderPassEncoder> GPUCommandEncoder::beginRenderPass(GPURenderPassDescriptor)
{
    return GPURenderPassEncoder::create();
}

Ref<GPUComputePassEncoder> GPUCommandEncoder::beginComputePass(std::optional<GPUComputePassDescriptor>)
{
    return GPUComputePassEncoder::create();
}

void GPUCommandEncoder::copyBufferToBuffer(
    const GPUBuffer& source,
    GPUSize64 sourceOffset,
    const GPUBuffer& destination,
    GPUSize64 destinationOffset,
    GPUSize64 size)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(sourceOffset);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
    UNUSED_PARAM(size);
}

void GPUCommandEncoder::copyBufferToTexture(
    GPUImageCopyBuffer source,
    GPUImageCopyTexture destination,
    GPUExtent3D copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void GPUCommandEncoder::copyTextureToBuffer(
    GPUImageCopyTexture source,
    GPUImageCopyBuffer destination,
    GPUExtent3D copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void GPUCommandEncoder::copyTextureToTexture(
    GPUImageCopyTexture source,
    GPUImageCopyTexture destination,
    GPUExtent3D copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void GPUCommandEncoder::fillBuffer(
    const GPUBuffer& destination,
    GPUSize64 destinationOffset,
    GPUSize64 size)
{
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
    UNUSED_PARAM(size);
}

void GPUCommandEncoder::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void GPUCommandEncoder::popDebugGroup()
{

}

void GPUCommandEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void GPUCommandEncoder::writeTimestamp(const GPUQuerySet&, GPUSize32 queryIndex)
{
    UNUSED_PARAM(queryIndex);
}

void GPUCommandEncoder::resolveQuerySet(
    const GPUQuerySet&,
    GPUSize32 firstQuery,
    GPUSize32 queryCount,
    const GPUBuffer& destination,
    GPUSize64 destinationOffset)
{
    UNUSED_PARAM(firstQuery);
    UNUSED_PARAM(queryCount);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
}

Ref<GPUCommandBuffer> GPUCommandEncoder::finish(std::optional<GPUCommandBufferDescriptor>)
{
    return GPUCommandBuffer::create();
}

}
