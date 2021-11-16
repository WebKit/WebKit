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

#include "config.h"
#include "RemoteQueueProxy.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteQueueProxy::RemoteQueueProxy(ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
{
}

RemoteQueueProxy::~RemoteQueueProxy()
{
}

void RemoteQueueProxy::submit(Vector<std::reference_wrapper<PAL::WebGPU::CommandBuffer>>&& commandBuffers)
{
    UNUSED_PARAM(commandBuffers);
}

void RemoteQueueProxy::onSubmittedWorkDone(WTF::Function<void()>&& callback)
{
    UNUSED_PARAM(callback);
}

void RemoteQueueProxy::writeBuffer(
    const PAL::WebGPU::Buffer& buffer,
    PAL::WebGPU::Size64 bufferOffset,
    const void* source,
    size_t byteLength,
    PAL::WebGPU::Size64 dataOffset,
    std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(bufferOffset);
    UNUSED_PARAM(source);
    UNUSED_PARAM(byteLength);
    UNUSED_PARAM(dataOffset);
    UNUSED_PARAM(size);
}

void RemoteQueueProxy::writeTexture(
    const PAL::WebGPU::ImageCopyTexture& destination,
    const void* source,
    size_t byteLength,
    const PAL::WebGPU::ImageDataLayout& dataLayout,
    const PAL::WebGPU::Extent3D& size)
{
    UNUSED_PARAM(destination);
    UNUSED_PARAM(source);
    UNUSED_PARAM(byteLength);
    UNUSED_PARAM(dataLayout);
    UNUSED_PARAM(size);
}

void RemoteQueueProxy::copyExternalImageToTexture(
    const PAL::WebGPU::ImageCopyExternalImage& source,
    const PAL::WebGPU::ImageCopyTextureTagged& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void RemoteQueueProxy::setLabelInternal(const String& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
