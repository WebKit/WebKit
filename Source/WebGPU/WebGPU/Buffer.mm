/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Buffer.h"

#import "APIConversions.h"
#import "Device.h"
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

static bool validateDescriptor(const Device& device, const WGPUBufferDescriptor& descriptor)
{
    UNUSED_PARAM(device);

    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpubufferdescriptor

    if (device.isLost())
        return false;

    // FIXME: "If any of the bits of descriptor’s usage aren’t present in this device’s [[allowed buffer usages]] return false."

    if ((descriptor.usage & WGPUBufferUsage_MapRead) && (descriptor.usage & WGPUBufferUsage_MapWrite))
        return false;

    return true;
}

static bool validateCreateBuffer(const Device& device, const WGPUBufferDescriptor& descriptor)
{
    if (!device.isValid())
        return false;

    if (!validateDescriptor(device, descriptor))
        return false;

    if (!descriptor.usage)
        return false;

    // FIXME: "descriptor.usage is a subset of this.[[allowed buffer usages]]."

    if ((descriptor.usage & WGPUBufferUsage_MapRead)
        && (descriptor.usage & ~WGPUBufferUsage_CopyDst & ~WGPUBufferUsage_MapRead))
        return false;

    if ((descriptor.usage & WGPUBufferUsage_MapWrite)
        && (descriptor.usage & ~WGPUBufferUsage_CopySrc & ~WGPUBufferUsage_MapWrite))
        return false;

    if (descriptor.mappedAtCreation && (descriptor.size % 4))
        return false;

    // FIXME: Make sure descriptor.size is less than maxBufferSize

    return true;
}

static MTLStorageMode storageMode(bool deviceHasUnifiedMemory, WGPUBufferUsageFlags usage, bool mappedAtCreation)
{
    if (deviceHasUnifiedMemory)
        return MTLStorageModeShared;
    if ((usage & WGPUBufferUsage_MapRead) || (usage & WGPUBufferUsage_MapWrite))
        return MTLStorageModeShared;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (mappedAtCreation)
        return MTLStorageModeManaged;
#else
    UNUSED_PARAM(mappedAtCreation);
#endif
    return MTLStorageModePrivate;
}

id<MTLBuffer> Device::safeCreateBuffer(NSUInteger length, MTLStorageMode storageMode, MTLCPUCacheMode cpuCacheMode, MTLHazardTrackingMode hazardTrackingMode) const
{
    MTLResourceOptions resourceOptions = (cpuCacheMode << MTLResourceCPUCacheModeShift) | (storageMode << MTLResourceStorageModeShift) | (hazardTrackingMode << MTLResourceHazardTrackingModeShift);
    // FIXME(PERFORMANCE): Consider returning nil instead of clamping to 1.
    // FIXME(PERFORMANCE): Suballocate multiple Buffers either from MTLHeaps or from larger MTLBuffers.
    return [m_device newBufferWithLength:std::max(static_cast<NSUInteger>(1), length) options:resourceOptions];
}

Ref<Buffer> Device::createBuffer(const WGPUBufferDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return Buffer::createInvalid(*this);

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createbuffer

    if (!validateCreateBuffer(*this, descriptor))
        return Buffer::createInvalid(*this);

    // FIXME(PERFORMANCE): Consider write-combining CPU cache mode.
    // FIXME(PERFORMANCE): Consider implementing hazard tracking ourself.
    MTLStorageMode storageMode = WebGPU::storageMode(hasUnifiedMemory(), descriptor.usage, descriptor.mappedAtCreation);
    auto buffer = safeCreateBuffer(static_cast<NSUInteger>(descriptor.size), storageMode);
    if (!buffer)
        return Buffer::createInvalid(*this);

    buffer.label = fromAPI(descriptor.label);

    if (descriptor.mappedAtCreation)
        return Buffer::create(buffer, descriptor.size, descriptor.usage, Buffer::State::MappedAtCreation, { static_cast<size_t>(0), static_cast<size_t>(descriptor.size) }, *this);

    return Buffer::create(buffer, descriptor.size, descriptor.usage, Buffer::State::Unmapped, { static_cast<size_t>(0), static_cast<size_t>(0) }, *this);
}

Buffer::Buffer(id<MTLBuffer> buffer, uint64_t size, WGPUBufferUsageFlags usage, State initialState, MappingRange initialMappingRange, Device& device)
    : m_buffer(buffer)
    , m_size(size)
    , m_usage(usage)
    , m_state(initialState)
    , m_mappingRange(initialMappingRange)
    , m_device(device)
{
}

Buffer::Buffer(Device& device)
    : m_device(device)
{
}

Buffer::~Buffer() = default;

void Buffer::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-destroy

    if (m_state != State::Unmapped && m_state != State::Destroyed) {
        // FIXME: ASSERT() that this call doesn't fail.
        unmap();
    }

    m_state = State::Destroyed;

    m_buffer = nil;
}

const void* Buffer::getConstMappedRange(size_t offset, size_t size)
{
    return getMappedRange(offset, size);
}

bool Buffer::validateGetMappedRange(size_t offset, size_t rangeSize) const
{
    if (m_state != State::Mapped && m_state != State::MappedAtCreation)
        return false;

    if (offset % 8)
        return false;

    if (rangeSize % 4)
        return false;

    if (offset < m_mappingRange.beginOffset)
        return false;

    auto endOffset = checkedSum<size_t>(offset, rangeSize);
    if (endOffset.hasOverflowed() || endOffset.value() > m_mappingRange.endOffset)
        return false;

    if (m_mappedRanges.overlaps({ offset, endOffset }))
        return false;

    return true;
}

static size_t computeRangeSize(uint64_t size, size_t offset)
{
    auto result = checkedDifference<size_t>(size, offset);
    if (result.hasOverflowed())
        return 0;
    return result.value();
}
  
void* Buffer::getMappedRange(size_t offset, size_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-getmappedrange

    auto rangeSize = size;
    if (size == WGPU_WHOLE_MAP_SIZE)
        rangeSize = computeRangeSize(m_size, offset);

    if (!validateGetMappedRange(offset, rangeSize)) {
        // FIXME: "throw an OperationError and stop."
        return nullptr;
    }

    m_mappedRanges.add({ offset, offset + rangeSize });
    m_mappedRanges.compact();

    return static_cast<char*>(m_buffer.contents) + offset;
}

bool Buffer::validateMapAsync(WGPUMapModeFlags mode, size_t offset, size_t rangeSize) const
{
    if (!isValid())
        return false;

    // FIXME: The spec says "TODO: check destroyed state?"

    if (offset % 8)
        return false;

    if (rangeSize % 4)
        return false;

    auto end = checkedSum<uint64_t>(offset, rangeSize);
    if (end.hasOverflowed() || end.value() > m_size)
        return false;

    if (m_state != State::Unmapped)
        return false;

    auto readWriteModeFlags = mode & (WGPUMapMode_Read | WGPUMapMode_Write);
    if (readWriteModeFlags != WGPUMapMode_Read && readWriteModeFlags != WGPUMapMode_Write)
        return false;

    if ((mode & WGPUMapMode_Read) && !(m_usage & WGPUBufferUsage_MapRead))
        return false;

    if ((mode & WGPUMapMode_Write) && !(m_usage & WGPUBufferUsage_MapWrite))
        return false;

    return true;
}

void Buffer::mapAsync(WGPUMapModeFlags mode, size_t offset, size_t size, CompletionHandler<void(WGPUBufferMapAsyncStatus)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-mapasync

    auto rangeSize = size;
    if (size == WGPU_WHOLE_MAP_SIZE)
        rangeSize = computeRangeSize(m_size, offset);

    if (!validateMapAsync(mode, offset, rangeSize)) {
        m_device->generateAValidationError("Validation failure."_s);

        m_device->instance().scheduleWork([callback = WTFMove(callback)]() mutable {
            callback(WGPUBufferMapAsyncStatus_Error);
        });
        return;
    }

    m_state = State::MappingPending;

    m_mapMode = mode;

    m_device->getQueue().onSubmittedWorkDone(0, [protectedThis = Ref { *this }, offset, rangeSize, callback = WTFMove(callback)](WGPUQueueWorkDoneStatus status) mutable {
        if (protectedThis->m_state == State::MappingPending) {
            protectedThis->m_state = State::Mapped;

            protectedThis->m_mappingRange = { offset, offset + rangeSize };

            protectedThis->m_mappedRanges = MappedRanges();
        }

        switch (status) {
        case WGPUQueueWorkDoneStatus_Success:
            callback(WGPUBufferMapAsyncStatus_Success);
            return;
        case WGPUQueueWorkDoneStatus_Error:
            callback(WGPUBufferMapAsyncStatus_Error);
            return;
        case WGPUQueueWorkDoneStatus_Unknown:
            callback(WGPUBufferMapAsyncStatus_Unknown);
            return;
        case WGPUQueueWorkDoneStatus_DeviceLost:
            callback(WGPUBufferMapAsyncStatus_DeviceLost);
            return;
        case WGPUQueueWorkDoneStatus_Force32:
            ASSERT_NOT_REACHED();
            callback(WGPUBufferMapAsyncStatus_Error);
            return;
        }
    });
}

bool Buffer::validateUnmap() const
{
    if (m_state != State::MappedAtCreation
        && m_state != State::MappingPending
        && m_state != State::Mapped)
        return false;

    return true;
}
  
void Buffer::unmap()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-unmap

    if (!validateUnmap()) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    // FIXME: "If this.[[state]] is mapping pending: Reject [[mapping]] with an AbortError."

    // FIXME: Handle array buffer detaching.

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (m_state == State::MappedAtCreation && m_buffer.storageMode == MTLStorageModeManaged) {
        for (const auto& mappedRange : m_mappedRanges)
            [m_buffer didModifyRange:NSMakeRange(static_cast<NSUInteger>(mappedRange.begin()), static_cast<NSUInteger>(mappedRange.end() - mappedRange.begin()))];
    }
#endif

    m_state = State::Unmapped;
}

void Buffer::setLabel(String&& label)
{
    m_buffer.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuBufferRelease(WGPUBuffer buffer)
{
    WebGPU::fromAPI(buffer).deref();
}

void wgpuBufferDestroy(WGPUBuffer buffer)
{
    WebGPU::fromAPI(buffer).destroy();
}

const void* wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    return WebGPU::fromAPI(buffer).getConstMappedRange(offset, size);
}

void* wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    return WebGPU::fromAPI(buffer).getMappedRange(offset, size);
}

uint64_t wgpuBufferGetSize(WGPUBuffer buffer)
{
    return WebGPU::fromAPI(buffer).size();
}

void wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapCallback callback, void* userdata)
{
    WebGPU::fromAPI(buffer).mapAsync(mode, offset, size, [callback, userdata](WGPUBufferMapAsyncStatus status) {
        callback(status, userdata);
    });
}

void wgpuBufferMapAsyncWithBlock(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback)
{
    WebGPU::fromAPI(buffer).mapAsync(mode, offset, size, [callback = WTFMove(callback)](WGPUBufferMapAsyncStatus status) {
        callback(status);
    });
}

void wgpuBufferUnmap(WGPUBuffer buffer)
{
    WebGPU::fromAPI(buffer).unmap();
}

void wgpuBufferSetLabel(WGPUBuffer buffer, const char* label)
{
    WebGPU::fromAPI(buffer).setLabel(WebGPU::fromAPI(label));
}
