/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "WebGPUExtent3D.h"
#include "WebGPUImageCopyExternalImage.h"
#include "WebGPUImageCopyTexture.h"
#include "WebGPUImageCopyTextureTagged.h"
#include "WebGPUImageDataLayout.h"
#include "WebGPUIntegralTypes.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore::WebGPU {

class Buffer;

class Queue : public RefCounted<Queue> {
public:
    virtual ~Queue() = default;

    String label() const { return m_label; }

    void setLabel(String&& label)
    {
        m_label = WTFMove(label);
        setLabelInternal(m_label);
    }

    virtual void submit(Vector<std::reference_wrapper<CommandBuffer>>&&) = 0;

    virtual void onSubmittedWorkDone(CompletionHandler<void()>&&) = 0;

    virtual void writeBuffer(
        const Buffer&,
        Size64 bufferOffset,
        const void* source,
        size_t byteLength,
        Size64 dataOffset = 0,
        std::optional<Size64> = std::nullopt) = 0;

    virtual void writeTexture(
        const ImageCopyTexture& destination,
        const void* source,
        size_t byteLength,
        const ImageDataLayout&,
        const Extent3D& size) = 0;

    virtual void writeBuffer(
        const Buffer&,
        Size64 bufferOffset,
        void* source,
        size_t byteLength,
        Size64 dataOffset = 0,
        std::optional<Size64> = std::nullopt) = 0;

    virtual void writeTexture(
        const ImageCopyTexture& destination,
        void* source,
        size_t byteLength,
        const ImageDataLayout&,
        const Extent3D& size) = 0;

    virtual void copyExternalImageToTexture(
        const ImageCopyExternalImage& source,
        const ImageCopyTextureTagged& destination,
        const Extent3D& copySize) = 0;

protected:
    Queue() = default;

private:
    Queue(const Queue&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue& operator=(Queue&&) = delete;

    virtual void setLabelInternal(const String&) = 0;

    String m_label;
};

} // namespace WebCore::WebGPU
