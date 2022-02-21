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

#pragma once

#include "WebGPUCommandBuffer.h"
#include "WebGPUCommandBufferDescriptor.h"
#include "WebGPUComputePassDescriptor.h"
#include "WebGPUComputePassEncoder.h"
#include "WebGPUExtent3D.h"
#include "WebGPUImageCopyBuffer.h"
#include "WebGPUImageCopyTexture.h"
#include "WebGPUIntegralTypes.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPassEncoder.h"
#include <optional>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {

class Buffer;
class QuerySet;

class CommandEncoder : public RefCounted<CommandEncoder> {
public:
    virtual ~CommandEncoder() = default;

    String label() const { return m_label; }

    void setLabel(String&& label)
    {
        m_label = WTFMove(label);
        setLabelInternal(m_label);
    }

    virtual Ref<RenderPassEncoder> beginRenderPass(const RenderPassDescriptor&) = 0;
    virtual Ref<ComputePassEncoder> beginComputePass(const std::optional<ComputePassDescriptor>&) = 0;

    virtual void copyBufferToBuffer(
        const Buffer& source,
        Size64 sourceOffset,
        const Buffer& destination,
        Size64 destinationOffset,
        Size64) = 0;

    virtual void copyBufferToTexture(
        const ImageCopyBuffer& source,
        const ImageCopyTexture& destination,
        const Extent3D& copySize) = 0;

    virtual void copyTextureToBuffer(
        const ImageCopyTexture& source,
        const ImageCopyBuffer& destination,
        const Extent3D& copySize) = 0;

    virtual void copyTextureToTexture(
        const ImageCopyTexture& source,
        const ImageCopyTexture& destination,
        const Extent3D& copySize) = 0;

    virtual void clearBuffer(
        const Buffer&,
        Size64 offset = 0,
        std::optional<Size64> = std::nullopt) = 0;

    virtual void pushDebugGroup(String&& groupLabel) = 0;
    virtual void popDebugGroup() = 0;
    virtual void insertDebugMarker(String&& markerLabel) = 0;

    virtual void writeTimestamp(const QuerySet&, Size32 queryIndex) = 0;

    virtual void resolveQuerySet(
        const QuerySet&,
        Size32 firstQuery,
        Size32 queryCount,
        const Buffer& destination,
        Size64 destinationOffset) = 0;

    virtual Ref<CommandBuffer> finish(const CommandBufferDescriptor&) = 0;

protected:
    CommandEncoder() = default;

private:
    CommandEncoder(const CommandEncoder&) = delete;
    CommandEncoder(CommandEncoder&&) = delete;
    CommandEncoder& operator=(const CommandEncoder&) = delete;
    CommandEncoder& operator=(CommandEncoder&&) = delete;

    virtual void setLabelInternal(const String&) = 0;

    String m_label;
};

} // namespace PAL::WebGPU
