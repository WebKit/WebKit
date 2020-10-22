/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#if ENABLE(WEBGPU)

#include "GPUBufferDescriptor.h"
#include "GPUDevice.h"
#include <JavaScriptCore/ArrayBuffer.h>

namespace WebCore {

RefPtr<GPUBuffer> GPUBuffer::tryCreate(GPUDevice& device, const GPUBufferDescriptor& descriptor, GPUBufferMappedOption isMapped, GPUErrorScopes& errorScopes)
{
    return nullptr;
}

GPUBuffer::GPUBuffer(PlatformBufferSmartPtr&& buffer, GPUDevice& device, size_t size, OptionSet<GPUBufferUsage::Flags> usage, GPUBufferMappedOption isMapped)
    : m_platformBuffer(WTFMove(buffer))
    , m_device(makeRef(device))
    , m_byteLength(size)
    , m_usage(usage)
    , m_isMappedFromCreation(isMapped == GPUBufferMappedOption::IsMapped)
{
}

GPUBuffer::~GPUBuffer()
{
    destroy(nullptr);
}

bool GPUBuffer::isReadOnly() const
{
    return true;
}

GPUBuffer::State GPUBuffer::state() const
{
    return State::Unmapped;
}

JSC::ArrayBuffer* GPUBuffer::mapOnCreation()
{
    return nullptr;
}

void GPUBuffer::registerMappingCallback(MappingCallback&& callback, bool isRead, GPUErrorScopes& errorScopes)
{
}

void GPUBuffer::runMappingCallback()
{
}

JSC::ArrayBuffer* GPUBuffer::stagingBufferForRead()
{
    return nullptr;
}

JSC::ArrayBuffer* GPUBuffer::stagingBufferForWrite()
{
    return nullptr;
}

void GPUBuffer::copyStagingBufferToGPU(GPUErrorScopes* errorScopes)
{
}

void GPUBuffer::unmap(GPUErrorScopes* errorScopes)
{
}

void GPUBuffer::destroy(GPUErrorScopes* errorScopes)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
