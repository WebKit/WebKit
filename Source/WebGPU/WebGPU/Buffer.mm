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
#import <wtf/StdLibExtras.h>

namespace WebGPU {

static bool validateDescriptor(const Device& device, const WGPUBufferDescriptor& descriptor)
{
    UNUSED_PARAM(device);

    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpubufferdescriptor

    // FIXME: "If device is lost return false."

    // FIXME: "If any of the bits of descriptor’s usage aren’t present in this device’s [[allowed buffer usages]] return false."

    // "If both the MAP_READ and MAP_WRITE bits of descriptor’s usage attribute are set, return false."
    if ((descriptor.usage & WGPUBufferUsage_MapRead) && (descriptor.usage & WGPUBufferUsage_MapWrite))
        return false;

    return true;
}

static bool validateCreateBuffer(const Device& device, const WGPUBufferDescriptor& descriptor)
{
    // FIXME: "this is a valid GPUDevice."

    // "validating GPUBufferDescriptor(this, descriptor) returns true."
    if (!validateDescriptor(device, descriptor))
        return false;

    // "descriptor.usage must not be 0."
    if (!descriptor.usage)
        return false;

    // FIXME: "descriptor.usage is a subset of this.[[allowed buffer usages]]."

    // "If descriptor.usage contains MAP_READ: descriptor.usage contains no other flags except COPY_DST."
    if ((descriptor.usage & WGPUBufferUsage_MapRead)
        && (descriptor.usage & ~WGPUBufferUsage_CopyDst & ~WGPUBufferUsage_MapRead))
        return false;

    // "If descriptor.usage contains MAP_WRITE: descriptor.usage contains no other flags except COPY_SRC."
    if ((descriptor.usage & WGPUBufferUsage_MapWrite)
        && (descriptor.usage & ~WGPUBufferUsage_CopySrc & ~WGPUBufferUsage_MapWrite))
        return false;

    // "If descriptor.mappedAtCreation is true: descriptor.size is a multiple of 4."
    if (descriptor.mappedAtCreation && (descriptor.size % 4))
        return false;

    // FIXME: Make sure descriptor.size is less than maxBufferSize

    return true;
}

static MTLStorageMode storageMode(bool deviceHasUnifiedMemory, WGPUBufferUsageFlags usage)
{
    if (deviceHasUnifiedMemory)
        return MTLStorageModeShared;
    // On discrete memory GPUs, mappable memory is shared, whereas non-mappable memory is private.
    // There is no situation where we'll use the managed storage mode.
    if ((usage & WGPUBufferUsage_MapRead) || (usage & WGPUBufferUsage_MapWrite))
        return MTLStorageModeShared;
    return MTLStorageModePrivate;
}

RefPtr<Buffer> Device::createBuffer(const WGPUBufferDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return nullptr;

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createbuffer

    // "If any of the following conditions are unsatisfied, return an error buffer and stop."
    if (!validateCreateBuffer(*this, descriptor))
        return nullptr;

    // FIXME: Use heap allocation to make this faster.
    // If/when we do that, we have to make sure that "new" buffers get cleared to 0.
    // FIXME: Consider write-combining CPU cache mode.
    // FIXME: Consider implementing hazard tracking ourself.
    MTLStorageMode storageMode = WebGPU::storageMode(hasUnifiedMemory(), descriptor.usage);
    id<MTLBuffer> buffer = [m_device newBufferWithLength:static_cast<NSUInteger>(descriptor.size) options:storageMode];
    if (!buffer)
        return nullptr;

    buffer.label = fromAPI(descriptor.label);

    // FIXME: Handle descriptor.mappedAtCreation.
    // Because non-mappable buffers can be mapped at creation,
    // this means we have to have a temporary buffer mapped,
    // and we can to schedule a copy command when it gets unmapped,
    // presumably at first use.

    // "Let b be a new GPUBuffer object."
    // "Set b.[[size]] to descriptor.size."
    // "Set b.[[usage]] to descriptor.usage."

    // "If descriptor.mappedAtCreation is true:"
    if (descriptor.mappedAtCreation) {
        // "Set b.[[mapping]] to a new ArrayBuffer of size b.[[size]]." This is unnecessary.
        // "Set b.[[mapping_range]] to [0, descriptor.size]."
        // "Set b.[[mapped_ranges]] to []."
        // "Set b.[[state]] to mapped at creation."
        return Buffer::create(buffer, descriptor.size, descriptor.usage, Buffer::State::MappedAtCreation, { static_cast<size_t>(0), static_cast<size_t>(descriptor.size) }, *this);
    }

    // "Set b.[[mapping]] to null." This is unnecessary.
    // "Set b.[[mapping_range]] to null."
    // "Set b.[[mapped_ranges]] to null."
    // "Set b.[[state]] to unmapped."
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

Buffer::~Buffer() = default;

void Buffer::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-destroy

    // "If the this.[[state]] is not either of unmapped or destroyed: Run the steps to unmap this."
    if (m_state != State::Unmapped && m_state != State::Destroyed) {
        // FIXME: ASSERT() that this call doesn't fail.
        unmap();
    }

    // "Set this.[[state]] to destroyed."
    m_state = State::Destroyed;

    m_buffer = nil;
}

const void* Buffer::getConstMappedRange(size_t offset, size_t size)
{
    return getMappedRange(offset, size);
}

bool Buffer::validateGetMappedRange(size_t offset, size_t rangeSize) const
{
    // "this.[[state]] is mapped or mapped at creation."
    if (m_state != State::Mapped && m_state != State::MappedAtCreation)
        return false;

    // "offset is a multiple of 8."
    if (offset % 8)
        return false;

    // "rangeSize is a multiple of 4."
    if (rangeSize % 4)
        return false;

    // "offset is greater than or equal to this.[[mapping_range]][0]."
    if (offset < m_mappingRange.beginOffset)
        return false;

    // "offset + rangeSize is less than or equal to this.[[mapping_range]][1]."
    // FIXME: Used checked arithmetic.
    auto endOffset = offset + rangeSize;
    if (endOffset > m_mappingRange.endOffset)
        return false;

    // "[offset, offset + rangeSize) does not overlap another range in this.[[mapped_ranges]]."
    if (m_mappedRanges.overlaps({ offset, endOffset }))
        return false;

    return true;
}
  
void* Buffer::getMappedRange(size_t offset, size_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-getmappedrange

    // "If size is missing: Let rangeSize be max(0, this.[[size]] - offset)."
    // FIXME: Use checked arithmetic.
    auto rangeSize = size;
    if (size == WGPU_WHOLE_MAP_SIZE)
        rangeSize = std::max(static_cast<uint64_t>(0), m_size - static_cast<uint64_t>(offset));

    // "If any of the following conditions are unsatisfied"
    if (!validateGetMappedRange(offset, rangeSize)) {
        // FIXME: "throw an OperationError and stop."
        return nullptr;
    }

    // "Let m be a new ArrayBuffer of size rangeSize pointing at the content of this.[[mapping]] at offset offset - this.[[mapping_range]][0]."

    // "Append m to this.[[mapped_ranges]]."
    m_mappedRanges.add({ offset, offset + rangeSize });

    // "Return m."
    return static_cast<char*>(m_buffer.contents) + offset;
}

bool Buffer::validateMapAsync(WGPUMapModeFlags mode, size_t offset, size_t rangeSize) const
{
    // FIXME: "this is a valid GPUBuffer. TODO: check destroyed state?"

    // "offset is a multiple of 8."
    if (offset % 8)
        return false;

    // "rangeSize is a multiple of 4."
    if (rangeSize % 4)
        return false;

    // "offset + rangeSize is less or equal to this.[[size]]"
    // FIXME: Use checked arithmetic.
    if (static_cast<uint64_t>(offset + rangeSize) > m_size)
        return false;

    // "this.[[state]] is unmapped"
    if (m_state != State::Unmapped)
        return false;

    // "mode contains exactly one of READ or WRITE."
    auto readWriteModeFlags = mode & (WGPUMapMode_Read | WGPUMapMode_Write);
    if (readWriteModeFlags != WGPUMapMode_Read && readWriteModeFlags != WGPUMapMode_Write)
        return false;

    // "If mode contains READ then this.[[usage]] must contain MAP_READ."
    if ((mode & WGPUMapMode_Read) && !(m_usage & WGPUBufferUsage_MapRead))
        return false;

    // "If mode contains WRITE then this.[[usage]] must contain MAP_WRITE."
    if ((mode & WGPUMapMode_Write) && !(m_usage & WGPUBufferUsage_MapWrite))
        return false;

    return true;
}

void Buffer::mapAsync(WGPUMapModeFlags mode, size_t offset, size_t size, CompletionHandler<void(WGPUBufferMapAsyncStatus)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-mapasync

    // "If size is missing: Let rangeSize be max(0, this.[[size]] - offset)."
    // FIXME: Use checked arithmetic.
    auto rangeSize = size;
    if (size == WGPU_WHOLE_MAP_SIZE)
        rangeSize = std::max(static_cast<uint64_t>(0), static_cast<uint64_t>(m_size - offset));

    // "If any of the following conditions are unsatisfied:"
    if (!validateMapAsync(mode, offset, rangeSize)) {
        // FIXME: "Record a validation error on the current scope."

        // "Return a promise rejected with an OperationError on the Device timeline."
        callback(WGPUBufferMapAsyncStatus_Error);
        return;
    }

    // "Set this.[[mapping]] to p."

    // "Set this.[[state]] to mapping pending."
    m_state = State::MappingPending;

    // "Set this.[[map_mode]] to mode."
    m_mapMode = mode;

    // "Enqueue an operation on the default queue’s Queue timeline that will execute the following:"
    m_device->getQueue().onSubmittedWorkDone(0, [protectedThis = Ref { *this }, offset, rangeSize, callback = WTFMove(callback)](WGPUQueueWorkDoneStatus status) mutable {
        // "If this.[[state]] is mapping pending:"
        if (protectedThis->m_state == State::MappingPending) {
            // "Let m be a new ArrayBuffer of size rangeSize."

            // "Set the content of m to the content of this’s allocation starting at offset offset and for rangeSize bytes."

            // "Set this.[[mapping]] to m."

            // "Set this.[[state]] to mapped."
            protectedThis->m_state = State::Mapped;

            // "Set this.[[mapping_range]] to [offset, offset + rangeSize]."
            protectedThis->m_mappingRange = { offset, offset + rangeSize };

            // "Set this.[[mapped_ranges]] to []."
            protectedThis->m_mappedRanges = MappedRanges();
        }

        // "Resolve p."
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
        default:
            callback(WGPUBufferMapAsyncStatus_Error);
            return;
        }
    });
}

bool Buffer::validateUnmap() const
{
    // "this.[[state]] must be mapped at creation, mapping pending, or mapped."
    if (m_state != State::MappedAtCreation
        && m_state != State::MappingPending
        && m_state != State::Mapped)
        return false;

    return true;
}
  
void Buffer::unmap()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-unmap

    // "If any of the following requirements are unmet"
    if (!validateUnmap()) {
        // FIXME: "generate a validation error and stop."
        return;
    }

    // "If this.[[state]] is mapping pending:"
    if (m_state == State::MappingPending) {
        // FIXME: "Reject [[mapping]] with an AbortError."

        // "Set this.[[mapping]] to null."
    }

    // "If this.[[state]] is mapped or mapped at creation:"
    if (m_state == State::Mapped || m_state == State::MappedAtCreation) {
        // "If one of the two following conditions holds:"
        // "this.[[state]] is mapped at creation"
        // "this.[[state]] is mapped and this.[[map_mode]] contains WRITE"
        // "Enqueue an operation on the default queue’s Queue timeline that updates the this.[[mapping_range]] of this’s allocation to the content of this.[[mapping]]."

        // "Detach each ArrayBuffer in this.[[mapped_ranges]] from its content."

        // "Set this.[[mapping]] to null."

        // "Set this.[[mapping_range]] to null."

        // "Set this.[[mapped_ranges]] to null."
    }

    // "Set this.[[state]] to unmapped."
    m_state = State::Unmapped;
}

void Buffer::setLabel(String&& label)
{
    m_buffer.label = label;
}

} // namespace WebGPU

void wgpuBufferRelease(WGPUBuffer buffer)
{
    delete buffer;
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
