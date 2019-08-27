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
#import "GPUErrorScopes.h"
#import <JavaScriptCore/ArrayBuffer.h>
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/MainThread.h>

namespace WebCore {

static constexpr auto readOnlyFlags = OptionSet<GPUBufferUsage::Flags> { GPUBufferUsage::Flags::Index, GPUBufferUsage::Flags::Vertex, GPUBufferUsage::Flags::Uniform, GPUBufferUsage::Flags::TransferSource };


bool GPUBuffer::validateBufferUsage(const GPUDevice& device, OptionSet<GPUBufferUsage::Flags> usage, GPUErrorScopes& errorScopes)
{
    if (!device.platformDevice()) {
        errorScopes.generatePrefixedError("Invalid GPUDevice!");
        return false;
    }

    if (usage.containsAll({ GPUBufferUsage::Flags::MapWrite, GPUBufferUsage::Flags::MapRead })) {
        errorScopes.generatePrefixedError("Buffer cannot have both MAP_READ and MAP_WRITE usage!");
        return false;
    }

    return true;
}

RefPtr<GPUBuffer> GPUBuffer::tryCreate(GPUDevice& device, const GPUBufferDescriptor& descriptor, GPUBufferMappedOption isMapped, GPUErrorScopes& errorScopes)
{
    // MTLBuffer size (NSUInteger) is 32 bits on some platforms.
    NSUInteger size = 0;
    if (!WTF::convertSafely(descriptor.size, size)) {
        errorScopes.generateError("", GPUErrorFilter::OutOfMemory);
        return nullptr;
    }

    auto usage = OptionSet<GPUBufferUsage::Flags>::fromRaw(descriptor.usage);
    if (!validateBufferUsage(device, usage, errorScopes))
        return nullptr;

    // copyBufferToBuffer calls require 4-byte alignment. "Unmapping" a mapped-on-creation GPUBuffer
    // that is otherwise unmappable requires such a copy to upload data.
    if (isMapped == GPUBufferMappedOption::IsMapped
        && !usage.containsAny({ GPUBufferUsage::Flags::MapWrite, GPUBufferUsage::Flags::MapRead })
        && descriptor.size % 4) {
        errorScopes.generatePrefixedError("Data must be aligned to a multiple of 4 bytes!");
        return nullptr;
    }

    // FIXME: Metal best practices: Read-only one-time-use data less than 4 KB should not allocate a MTLBuffer and be used in [MTLCommandEncoder set*Bytes] calls instead.

    MTLResourceOptions resourceOptions = MTLResourceCPUCacheModeDefaultCache;

    // Mappable buffers use shared storage allocation.
    resourceOptions |= usage.containsAny({ GPUBufferUsage::Flags::MapWrite, GPUBufferUsage::Flags::MapRead }) ? MTLResourceStorageModeShared : MTLResourceStorageModePrivate;

    RetainPtr<MTLBuffer> mtlBuffer;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlBuffer = adoptNS([device.platformDevice() newBufferWithLength:static_cast<NSUInteger>(descriptor.size) options:resourceOptions]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlBuffer) {
        errorScopes.generateError("", GPUErrorFilter::OutOfMemory);
        return nullptr;
    }

    return adoptRef(*new GPUBuffer(WTFMove(mtlBuffer), device, size, usage, isMapped));
}

GPUBuffer::GPUBuffer(RetainPtr<MTLBuffer>&& buffer, GPUDevice& device, size_t size, OptionSet<GPUBufferUsage::Flags> usage, GPUBufferMappedOption isMapped)
    : m_platformBuffer(WTFMove(buffer))
    , m_device(makeRef(device))
    , m_byteLength(size)
    , m_usage(usage)
    , m_isMappedFromCreation(isMapped == GPUBufferMappedOption::IsMapped)
{
    m_platformUsage = MTLResourceUsageRead;
    if (isStorage())
        m_platformUsage |= MTLResourceUsageWrite;
}

GPUBuffer::~GPUBuffer()
{
    destroy(nullptr);
}

bool GPUBuffer::isReadOnly() const
{
    return m_usage.containsAny(readOnlyFlags);
}

GPUBuffer::State GPUBuffer::state() const
{
    if (!m_platformBuffer)
        return State::Destroyed;
    if (m_isMappedFromCreation || m_mappingCallback)
        return State::Mapped;

    return State::Unmapped;
}

JSC::ArrayBuffer* GPUBuffer::mapOnCreation()
{
    ASSERT(m_isMappedFromCreation);
    return stagingBufferForWrite();
}

#if USE(METAL)
void GPUBuffer::commandBufferCommitted(MTLCommandBuffer *commandBuffer)
{
    ASSERT(isMainThread());
    ++m_numScheduledCommandBuffers;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    // Make sure |this| only gets ref'd / deref'd on the main thread since it is not ThreadSafeRefCounted.
    [commandBuffer addCompletedHandler:makeBlockPtr([this, protectedThis = makeRef(*this)](id<MTLCommandBuffer>) mutable {
        callOnMainThread([this, protectedThis = WTFMove(protectedThis)] {
            commandBufferCompleted();
        });
    }).get()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void GPUBuffer::commandBufferCompleted()
{
    ASSERT(isMainThread());
    ASSERT(m_numScheduledCommandBuffers);

    if (m_numScheduledCommandBuffers == 1 && state() == State::Mapped)
        runMappingCallback();

    --m_numScheduledCommandBuffers;
}
#endif // USE(METAL)

void GPUBuffer::registerMappingCallback(MappingCallback&& callback, bool isRead, GPUErrorScopes& errorScopes)
{
    // Reject if request is invalid.
    if (isRead && !isMapReadable()) {
        errorScopes.generatePrefixedError("Invalid operation!");
        callback(nullptr);
        return;
    }
    if (!isRead && !isMapWriteable()) {
        errorScopes.generatePrefixedError("Invalid operation!");
        callback(nullptr);
        return;
    }

    ASSERT(!m_mappingCallback && !m_mappingCallbackTask.hasPendingTask());

    // An existing callback means this buffer is in the mapped state.
    m_mappingCallback = PendingMappingCallback::create(WTFMove(callback));

    // If GPU is not using this buffer, run the callback ASAP.
    if (!m_numScheduledCommandBuffers) {
        m_mappingCallbackTask.scheduleTask([this, protectedThis = makeRef(*this)] () mutable {
            runMappingCallback();
        });
    }
}

void GPUBuffer::runMappingCallback()
{
    if (m_mappingCallback)
        m_mappingCallback->callback(isMapRead() ? stagingBufferForRead() : stagingBufferForWrite());
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

void GPUBuffer::copyStagingBufferToGPU(GPUErrorScopes* errorScopes)
{
    MTLCommandQueue *queue;
    if (!m_device->tryGetQueue() || !(queue = m_device->tryGetQueue()->platformQueue()))
        return;

    RetainPtr<MTLBuffer> stagingMtlBuffer;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    // GPUBuffer creation validation ensures m_byteSize fits in NSUInteger.
    stagingMtlBuffer = adoptNS([m_device->platformDevice() newBufferWithLength:static_cast<NSUInteger>(m_byteLength) options:MTLResourceCPUCacheModeDefaultCache]);
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!stagingMtlBuffer && errorScopes) {
        errorScopes->generateError("", GPUErrorFilter::OutOfMemory);
        return;
    }

    memcpy(stagingMtlBuffer.get().contents, m_stagingBuffer->data(), m_byteLength);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    auto commandBuffer = retainPtr([queue commandBuffer]);
    auto blitEncoder = retainPtr([commandBuffer blitCommandEncoder]);

    [blitEncoder copyFromBuffer:stagingMtlBuffer.get() sourceOffset:0 toBuffer:m_platformBuffer.get() destinationOffset:0 size:static_cast<NSUInteger>(m_byteLength)];
    [blitEncoder endEncoding];
    [commandBuffer commit];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void GPUBuffer::unmap(GPUErrorScopes* errorScopes)
{
    if (!m_isMappedFromCreation && !isMappable() && errorScopes) {
        errorScopes->generatePrefixedError("Invalid operation: GPUBuffer is not mappable!");
        return;
    }

    if (m_stagingBuffer) {
        if (isMappable()) {
            // MAP_WRITE and MAP_READ buffers have shared, CPU-accessible storage.
            ASSERT(m_platformBuffer && m_platformBuffer.get().contents);
            memcpy(m_platformBuffer.get().contents, m_stagingBuffer->data(), m_byteLength);
        } else if (m_isMappedFromCreation)
            copyStagingBufferToGPU(errorScopes);

        m_isMappedFromCreation = false;
        m_stagingBuffer = nullptr;
    }

    if (m_mappingCallback) {
        m_mappingCallbackTask.cancelTask();
        m_mappingCallback->callback(nullptr);
        m_mappingCallback = nullptr;
    }
}

void GPUBuffer::destroy(GPUErrorScopes* errorScopes)
{
    if (state() == State::Mapped)
        unmap(errorScopes);

    m_platformBuffer = nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
