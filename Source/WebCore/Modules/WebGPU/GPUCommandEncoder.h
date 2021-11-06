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

#include "GPUCommandBuffer.h"
#include "GPUCommandBufferDescriptor.h"
#include "GPUComputePassDescriptor.h"
#include "GPUComputePassEncoder.h"
#include "GPUExtent3DDict.h"
#include "GPUImageCopyBuffer.h"
#include "GPUImageCopyTexture.h"
#include "GPUIntegralTypes.h"
#include "GPURenderPassDescriptor.h"
#include "GPURenderPassEncoder.h"
#include <optional>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUBuffer;
class GPUQuerySet;

class GPUCommandEncoder : public RefCounted<GPUCommandEncoder> {
public:
    static Ref<GPUCommandEncoder> create()
    {
        return adoptRef(*new GPUCommandEncoder());
    }

    String label() const;
    void setLabel(String&&);

    Ref<GPURenderPassEncoder> beginRenderPass(GPURenderPassDescriptor);
    Ref<GPUComputePassEncoder> beginComputePass(std::optional<GPUComputePassDescriptor>);

    void copyBufferToBuffer(
        const GPUBuffer& source,
        GPUSize64 sourceOffset,
        const GPUBuffer& destination,
        GPUSize64 destinationOffset,
        GPUSize64);

    void copyBufferToTexture(
        GPUImageCopyBuffer source,
        GPUImageCopyTexture destination,
        GPUExtent3D copySize);

    void copyTextureToBuffer(
        GPUImageCopyTexture source,
        GPUImageCopyBuffer destination,
        GPUExtent3D copySize);

    void copyTextureToTexture(
        GPUImageCopyTexture source,
        GPUImageCopyTexture destination,
        GPUExtent3D copySize);

    void fillBuffer(
        const GPUBuffer& destination,
        GPUSize64 destinationOffset,
        GPUSize64);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void writeTimestamp(const GPUQuerySet&, GPUSize32 queryIndex);

    void resolveQuerySet(
        const GPUQuerySet&,
        GPUSize32 firstQuery,
        GPUSize32 queryCount,
        const GPUBuffer& destination,
        GPUSize64 destinationOffset);

    Ref<GPUCommandBuffer> finish(std::optional<GPUCommandBufferDescriptor>);

private:
    GPUCommandEncoder() = default;
};

}
