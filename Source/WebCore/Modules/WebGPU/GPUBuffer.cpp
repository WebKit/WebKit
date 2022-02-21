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
#include "GPUBuffer.h"

namespace WebCore {

String GPUBuffer::label() const
{
    return m_backing->label();
}

void GPUBuffer::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

void GPUBuffer::mapAsync(GPUMapModeFlags mode, GPUSize64 offset, std::optional<GPUSize64> size, MapAsyncPromise&& promise)
{
    m_backing->mapAsync(convertMapModeFlagsToBacking(mode), offset, size, [promise = WTFMove(promise)] () mutable {
        promise.resolve(nullptr);
    });
}

Ref<JSC::ArrayBuffer> GPUBuffer::getMappedRange(GPUSize64 offset, std::optional<GPUSize64> size)
{
    auto mappedRange = m_backing->getMappedRange(offset, size);
    return ArrayBuffer::create(mappedRange.source, mappedRange.byteLength);
}

void GPUBuffer::unmap()
{
    m_backing->unmap();
}

void GPUBuffer::destroy()
{
    m_backing->destroy();
}

}
