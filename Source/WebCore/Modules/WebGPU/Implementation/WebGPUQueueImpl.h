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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUPtr.h"
#include "WebGPUQueue.h"
#include <WebGPU/WebGPU.h>
#include <wtf/Deque.h>

namespace WebCore::WebGPU {

class ConvertToBackingContext;

class QueueImpl final : public Queue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<QueueImpl> create(WebGPUPtr<WGPUQueue>&& queue, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new QueueImpl(WTFMove(queue), convertToBackingContext));
    }

    virtual ~QueueImpl();

private:
    friend class DowncastConvertToBackingContext;

    QueueImpl(WebGPUPtr<WGPUQueue>&&, ConvertToBackingContext&);

    QueueImpl(const QueueImpl&) = delete;
    QueueImpl(QueueImpl&&) = delete;
    QueueImpl& operator=(const QueueImpl&) = delete;
    QueueImpl& operator=(QueueImpl&&) = delete;

    WGPUQueue backing() const { return m_backing.get(); }

    void submit(Vector<std::reference_wrapper<CommandBuffer>>&&) final;

    void onSubmittedWorkDone(CompletionHandler<void()>&&) final;

    void writeBuffer(
        const Buffer&,
        Size64 bufferOffset,
        const void* source,
        size_t byteLength,
        Size64 dataOffset,
        std::optional<Size64>) final;

    void writeTexture(
        const ImageCopyTexture& destination,
        const void* source,
        size_t byteLength,
        const ImageDataLayout&,
        const Extent3D& size) final;

    void writeBuffer(
        const Buffer&,
        Size64 bufferOffset,
        void* source,
        size_t byteLength,
        Size64 dataOffset,
        std::optional<Size64>) final;

    void writeTexture(
        const ImageCopyTexture& destination,
        void* source,
        size_t byteLength,
        const ImageDataLayout&,
        const Extent3D& size) final;

    void copyExternalImageToTexture(
        const ImageCopyExternalImage& source,
        const ImageCopyTextureTagged& destination,
        const Extent3D& copySize) final;

    void setLabelInternal(const String&) final;

    WebGPUPtr<WGPUQueue> m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
