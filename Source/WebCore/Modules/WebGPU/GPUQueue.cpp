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
#include "GPUQueue.h"

#include "GPUBuffer.h"

namespace WebCore {

String GPUQueue::label() const
{
    return StringImpl::empty();
}

void GPUQueue::setLabel(String&&)
{
}

void GPUQueue::submit(Vector<RefPtr<GPUCommandBuffer>>)
{
}

void GPUQueue::onSubmittedWorkDone(Ref<DeferredPromise>&&)
{
}

void GPUQueue::writeBuffer(
    const GPUBuffer&,
    GPUSize64 bufferOffset,
    BufferSource&& data,
    std::optional<GPUSize64> dataOffset,
    std::optional<GPUSize64> size)
{
    UNUSED_PARAM(bufferOffset);
    UNUSED_PARAM(data);
    UNUSED_PARAM(dataOffset);
    UNUSED_PARAM(size);
}

void GPUQueue::writeTexture(
    GPUImageCopyTexture destination,
    BufferSource&& data,
    GPUImageDataLayout,
    GPUExtent3D size)
{
    UNUSED_PARAM(destination);
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
}

void GPUQueue::copyExternalImageToTexture(
    GPUImageCopyExternalImage source,
    GPUImageCopyTextureTagged destination,
    GPUExtent3D copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

}
