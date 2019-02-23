/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "GPUBufferDescriptor.h"
#import "GPUDevice.h"
#import "Logging.h"
#import <JavaScriptCore/ArrayBuffer.h>
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

RefPtr<GPUBuffer> GPUBuffer::tryCreateSharedBuffer(const GPUDevice& device, const GPUBufferDescriptor& descriptor)
{
    ASSERT(device.platformDevice());

    RetainPtr<MTLBuffer> mtlBuffer;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlBuffer = adoptNS([device.platformDevice() newBufferWithLength:descriptor.size options: MTLResourceCPUCacheModeDefaultCache]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlBuffer) {
        LOG(WebGPU, "GPUBuffer::create(): Unable to create MTLBuffer!");
        return nullptr;
    }

    return adoptRef(*new GPUBuffer(WTFMove(mtlBuffer), descriptor));
}

static const auto readOnlyMask = GPUBufferUsage::Index | GPUBufferUsage::Vertex | GPUBufferUsage::Uniform | GPUBufferUsage::TransferSrc;

RefPtr<GPUBuffer> GPUBuffer::tryCreate(const GPUDevice& device, GPUBufferDescriptor&& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUBuffer::create(): Invalid GPUDevice!");
        return nullptr;
    }

    if ((descriptor.usage & GPUBufferUsage::MapWrite) && (descriptor.usage & GPUBufferUsage::MapRead)) {
        LOG(WebGPU, "GPUBuffer::create(): Buffer cannot have both MAP_READ and MAP_WRITE usage!");
        return nullptr;
    }

    if ((descriptor.usage & readOnlyMask) && (descriptor.usage & GPUBufferUsage::Storage)) {
        LOG(WebGPU, "GPUBuffer::create(): Buffer cannot have both STORAGE and a read-only usage!");
        return nullptr;
    }

    // Mappable buffers need (default) shared storage allocation.
    if (descriptor.usage & (GPUBufferUsage::MapWrite | GPUBufferUsage::MapRead))
        return tryCreateSharedBuffer(device, descriptor);

    LOG(WebGPU, "GPUBuffer::create(): Support for non-mapped buffers not implemented!");
    return nullptr;
}

GPUBuffer::GPUBuffer(RetainPtr<MTLBuffer>&& buffer, const GPUBufferDescriptor& descriptor)
    : m_platformBuffer(WTFMove(buffer))
    , m_byteLength(descriptor.size)
    , m_isMapWrite(descriptor.usage & GPUBufferUsage::MapWrite)
    , m_isMapRead(descriptor.usage & GPUBufferUsage::MapRead)
    , m_isVertex(descriptor.usage & GPUBufferUsage::Vertex)
    , m_isUniform(descriptor.usage & GPUBufferUsage::Uniform)
    , m_isStorage(descriptor.usage & GPUBufferUsage::Storage)
    , m_isReadOnly(descriptor.usage & readOnlyMask)
{
}

GPUBuffer::~GPUBuffer()
{
    unmap();
}

void GPUBuffer::registerMappingCallback(MappingCallback&& callback, bool isRead)
{
    // Reject if request is invalid.
    if (isRead && !isMapReadable()) {
        LOG(WebGPU, "GPUBuffer::mapReadAsync(): Invalid operation!");
        callback(nullptr);
        return;
    }
    if (!isRead && !isMapWriteable()) {
        LOG(WebGPU, "GPUBuffer::mapWriteAsync(): Invalid operation!");
        callback(nullptr);
        return;
    }

    ASSERT(!m_pendingCallback && !m_mappingCallbackTask.hasPendingTask());

    // An existing callback means this buffer is in the mapped state.
    m_pendingCallback = PendingMappingCallback::create(WTFMove(callback));

    // If GPU is not using this buffer, run the callback ASAP.
    if (!m_numScheduledCommandBuffers) {
        m_mappingCallbackTask.scheduleTask([this, protectedThis = makeRef(*this)] () mutable {
            ASSERT(m_pendingCallback);

            m_pendingCallback->callback(m_isMapRead ? stagingBufferForRead() : stagingBufferForWrite());
        });
    }
}

JSC::ArrayBuffer* GPUBuffer::stagingBufferForRead()
{
    if (!m_stagingBuffer)
        m_stagingBuffer = ArrayBuffer::create(m_platformBuffer.get().contents, m_byteLength);
    else
        memcpy(m_stagingBuffer->data(), m_platformBuffer.get().contents, m_byteLength);

    return m_stagingBuffer.get();
}

JSC::ArrayBuffer* GPUBuffer::stagingBufferForWrite()
{
    m_stagingBuffer = ArrayBuffer::create(1, m_byteLength);
    return m_stagingBuffer.get();
}

void GPUBuffer::unmap()
{
    if (!isMappable()) {
        LOG(WebGPU, "GPUBuffer::unmap(): Buffer is not mappable!");
        return;
    }

    if (m_stagingBuffer && m_isMapWrite) {
        memcpy(m_platformBuffer.get().contents, m_stagingBuffer->data(), m_byteLength);
        m_stagingBuffer = nullptr;
    }

    if (m_pendingCallback) {
        m_mappingCallbackTask.cancelTask();
        m_pendingCallback->callback(nullptr);
        m_pendingCallback = nullptr;
    }
}

void GPUBuffer::destroy()
{
    if (isMappable())
        unmap();

    m_isDestroyed = true;

    // FIXME: If GPU is still using the MTLBuffer, it will be released after all relevant commands have executed.
    if (!m_numScheduledCommandBuffers)
        m_platformBuffer = nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
