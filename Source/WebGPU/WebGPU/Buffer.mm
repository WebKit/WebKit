/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

    auto usage = descriptor.usage;
    if (!usage)
        return false;

    constexpr auto allUsages = (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index | WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform | WGPUBufferUsage_Storage | WGPUBufferUsage_Indirect | WGPUBufferUsage_QueryResolve);
    if (!(usage & allUsages) || usage > allUsages)
        return false;

    if ((usage & WGPUBufferUsage_MapRead) && (usage & ~WGPUBufferUsage_CopyDst & ~WGPUBufferUsage_MapRead))
        return false;

    if ((usage & WGPUBufferUsage_MapWrite) && (usage & ~WGPUBufferUsage_CopySrc & ~WGPUBufferUsage_MapWrite))
        return false;

    if (descriptor.mappedAtCreation && (descriptor.size % 4))
        return false;

    if (descriptor.size > device.limits().maxBufferSize)
        return false;

    return true;
}

static MTLStorageMode storageMode(bool deviceHasUnifiedMemory, WGPUBufferUsageFlags usage, bool mappedAtCreation)
{
    if (deviceHasUnifiedMemory)
        return MTLStorageModeShared;
    if (usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite | WGPUBufferUsage_Index))
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
    id<MTLBuffer> buffer = [m_device newBufferWithLength:std::max<NSUInteger>(1, length) options:resourceOptions];
    setOwnerWithIdentity(buffer);
    return buffer;
}

Ref<Buffer> Device::createBuffer(const WGPUBufferDescriptor& descriptor)
{
    if (descriptor.nextInChain || !isValid())
        return Buffer::createInvalid(*this);

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createbuffer

    if (!validateCreateBuffer(*this, descriptor)) {
        generateAValidationError("Validation failure."_s);

        return Buffer::createInvalid(*this);
    }

    // FIXME(PERFORMANCE): Consider write-combining CPU cache mode.
    // FIXME(PERFORMANCE): Consider implementing hazard tracking ourself.
    MTLStorageMode storageMode = WebGPU::storageMode(hasUnifiedMemory(), descriptor.usage, descriptor.mappedAtCreation);
    auto buffer = safeCreateBuffer(static_cast<NSUInteger>(descriptor.size), storageMode);
    if (!buffer) {
        generateAnOutOfMemoryError("Allocation failure."_s);

        return Buffer::createInvalid(*this);
    }

    buffer.label = fromAPI(descriptor.label);

    if (descriptor.mappedAtCreation)
        return Buffer::create(buffer, descriptor.size, descriptor.usage, Buffer::State::MappedAtCreation, { static_cast<size_t>(0), static_cast<size_t>(descriptor.size) }, *this);

    return Buffer::create(buffer, descriptor.size, descriptor.usage, Buffer::State::Unmapped, { static_cast<size_t>(0), static_cast<size_t>(0) }, *this);
}

Buffer::Buffer(id<MTLBuffer> buffer, uint64_t initialSize, WGPUBufferUsageFlags usage, State initialState, MappingRange initialMappingRange, Device& device)
    : m_buffer(buffer)
    , m_initialSize(initialSize)
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

void Buffer::setCommandEncoder(CommandEncoder& commandEncoder, bool mayModifyBuffer) const
{
    UNUSED_PARAM(mayModifyBuffer);
    m_commandEncoder = commandEncoder;
    if (m_state == State::Mapped || m_state == State::MappedAtCreation)
        commandEncoder.incrementBufferMapCount();
    if (isDestroyed())
        commandEncoder.makeSubmitInvalid();
}

void Buffer::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-destroy

    if (m_state != State::Unmapped && m_state != State::Destroyed) {
        // FIXME: ASSERT() that this call doesn't fail.
        unmap();
    }

    m_state = State::Destroyed;
    if (m_commandEncoder)
        m_commandEncoder.get()->makeSubmitInvalid();

    m_commandEncoder = nullptr;
    m_buffer = m_device->placeholderBuffer();
}

const void* Buffer::getConstMappedRange(size_t offset, size_t size)
{
    return getMappedRange(offset, size);
}

bool Buffer::validateGetMappedRange(size_t offset, size_t rangeSize) const
{
    if (m_state == State::Destroyed)
        return false;

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
    if (!isValid())
        return nullptr;

    auto rangeSize = size;
    if (size == WGPU_WHOLE_MAP_SIZE)
        rangeSize = computeRangeSize(this->currentSize(), offset);

    if (!validateGetMappedRange(offset, rangeSize))
        return nullptr;

    m_mappedRanges.add({ offset, offset + rangeSize });
    m_mappedRanges.compact();

    if (!m_buffer.contents)
        return nullptr;
    return static_cast<char*>(m_buffer.contents) + offset;
}

uint8_t* Buffer::getBufferContents()
{
    return static_cast<uint8_t*>(m_buffer.contents);
}

NSString* Buffer::errorValidatingMapAsync(WGPUMapModeFlags mode, size_t offset, size_t rangeSize) const
{
#define ERROR_STRING(x) (@"GPUBuffer.mapAsync: " x)
    if (!isValid())
        return ERROR_STRING(@"Buffer is not valid");

    if (offset % 8)
        return ERROR_STRING(@"Offset is not divisible by 8");

    if (rangeSize % 4)
        return ERROR_STRING(@"range size is not divisible by 4");

    auto end = checkedSum<uint64_t>(offset, rangeSize);
    if (end.hasOverflowed() || end.value() > currentSize())
        return ERROR_STRING(@"offset and rangeSize overflowed");

    if (m_state != State::Unmapped)
        return ERROR_STRING(@"state != Unmapped");

    auto readWriteModeFlags = mode & (WGPUMapMode_Read | WGPUMapMode_Write);
    if (readWriteModeFlags != WGPUMapMode_Read && readWriteModeFlags != WGPUMapMode_Write)
        return ERROR_STRING(@"readWriteModeFlags != Read && readWriteModeFlags != Write");

    if ((mode & WGPUMapMode_Read) && !(m_usage & WGPUBufferUsage_MapRead))
        return ERROR_STRING(@"(mode & Read) && !(usage & Read)");

    if ((mode & WGPUMapMode_Write) && !(m_usage & WGPUBufferUsage_MapWrite))
        return ERROR_STRING(@"(mode & Write) && !(usage & Write)");

#undef ERROR_STRING
    return nil;
}

void Buffer::mapAsync(WGPUMapModeFlags mode, size_t offset, size_t size, CompletionHandler<void(WGPUBufferMapAsyncStatus)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-mapasync

    auto rangeSize = size;
    if (size == WGPU_WHOLE_MAP_SIZE)
        rangeSize = computeRangeSize(currentSize(), offset);

    if (NSString* error = errorValidatingMapAsync(mode, offset, rangeSize)) {
        m_device->generateAValidationError(error);

        callback(WGPUBufferMapAsyncStatus_ValidationError);
        return;
    }

    m_state = State::MappingPending;

    m_mapMode = mode;

    m_device->getQueue().onSubmittedWorkDone([protectedThis = Ref { *this }, offset, rangeSize, callback = WTFMove(callback)](WGPUQueueWorkDoneStatus status) mutable {
        if (protectedThis->m_state == State::MappingPending) {
            protectedThis->m_state = State::Mapped;
            if (protectedThis->m_commandEncoder)
                protectedThis->m_commandEncoder->incrementBufferMapCount();

            protectedThis->m_mappingRange = { offset, offset + rangeSize };

            protectedThis->m_mappedRanges = MappedRanges();
        }

        switch (status) {
        case WGPUQueueWorkDoneStatus_Success:
            callback(WGPUBufferMapAsyncStatus_Success);
            return;
        case WGPUQueueWorkDoneStatus_Error:
            callback(WGPUBufferMapAsyncStatus_ValidationError);
            return;
        case WGPUQueueWorkDoneStatus_Unknown:
            callback(WGPUBufferMapAsyncStatus_Unknown);
            return;
        case WGPUQueueWorkDoneStatus_DeviceLost:
            callback(WGPUBufferMapAsyncStatus_DeviceLost);
            return;
        case WGPUQueueWorkDoneStatus_Force32:
            ASSERT_NOT_REACHED();
            callback(WGPUBufferMapAsyncStatus_ValidationError);
            return;
        }
    });
}

bool Buffer::validateUnmap() const
{
    return true;
}
  
void Buffer::unmap()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-unmap

    if (!validateUnmap() && !m_device->isValid())
        return;

    if (m_commandEncoder)
        m_commandEncoder->decrementBufferMapCount();

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (m_state == State::MappedAtCreation && m_buffer.storageMode == MTLStorageModeManaged) {
        for (const auto& mappedRange : m_mappedRanges)
            [m_buffer didModifyRange:NSMakeRange(static_cast<NSUInteger>(mappedRange.begin()), static_cast<NSUInteger>(mappedRange.end() - mappedRange.begin()))];
    }
#endif

    m_state = State::Unmapped;
    m_mappedRanges = MappedRanges();
}

void Buffer::setLabel(String&& label)
{
    m_buffer.label = label;
}

uint64_t Buffer::initialSize() const
{
    return m_initialSize;
}

uint64_t Buffer::currentSize() const
{
    return m_buffer.length;
}

bool Buffer::isValid() const
{
    return isDestroyed() || m_buffer;
}

bool Buffer::isDestroyed() const
{
    return state() == State::Destroyed;
}

void Buffer::recomputeMaxIndexValues() const
{
    if (!(m_usage & WGPUBufferUsage_Index))
        return;

    NSUInteger lengthInBytes = m_buffer.length;
    auto bufferPtr = static_cast<uint8_t*>(m_buffer.contents);
    RELEASE_ASSERT(bufferPtr);
    m_max16BitIndex = 0;
    m_max32BitIndex = 0;
    uint8_t* bufferEnd = bufferPtr + lengthInBytes;
    for (; (bufferPtr += sizeof(uint32_t)) <= bufferEnd; bufferPtr += sizeof(uint32_t)) {
        m_max32BitIndex = std::max(*reinterpret_cast<uint32_t*>(bufferPtr), m_max32BitIndex);
        m_max16BitIndex = std::max(*(reinterpret_cast<uint16_t*>(bufferPtr) + 1), std::max(*reinterpret_cast<uint16_t*>(bufferPtr), m_max16BitIndex));
    }
    if (bufferPtr + sizeof(uint16_t) <= bufferEnd)
        m_max16BitIndex = std::max(*reinterpret_cast<uint16_t*>(bufferPtr), m_max16BitIndex);
}

uint32_t Buffer::maxIndex(MTLIndexType indexType) const
{
    ASSERT(m_usage & WGPUBufferUsage_Index);
    return indexType == MTLIndexTypeUInt16 ? m_max16BitIndex : m_max32BitIndex;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuBufferReference(WGPUBuffer buffer)
{
    WebGPU::fromAPI(buffer).ref();
}

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

WGPUBufferMapState wgpuBufferGetMapState(WGPUBuffer buffer)
{
    switch (WebGPU::fromAPI(buffer).state()) {
    case WebGPU::Buffer::State::Mapped:
        return WGPUBufferMapState_Mapped;
    case WebGPU::Buffer::State::MappedAtCreation:
        return WGPUBufferMapState_Mapped;
    case WebGPU::Buffer::State::MappingPending:
        return WGPUBufferMapState_Pending;
    case WebGPU::Buffer::State::Unmapped:
        return WGPUBufferMapState_Unmapped;
    case WebGPU::Buffer::State::Destroyed:
        return WGPUBufferMapState_Unmapped;
    }
}

void* wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    return WebGPU::fromAPI(buffer).getMappedRange(offset, size);
}

void* wgpuBufferGetBufferContents(WGPUBuffer buffer)
{
    return WebGPU::fromAPI(buffer).getBufferContents();
}

uint64_t wgpuBufferGetSize(WGPUBuffer buffer)
{
    return WebGPU::fromAPI(buffer).initialSize();
}

void wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapCallback callback, void* userdata)
{
    WebGPU::fromAPI(buffer).mapAsync(mode, offset, size, [callback, userdata](WGPUBufferMapAsyncStatus status) {
        callback(status, userdata);
    });
}

void wgpuBufferMapAsyncWithBlock(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback)
{
    WebGPU::fromAPI(buffer).mapAsync(mode, offset, size, [callback = WebGPU::fromAPI(WTFMove(callback))](WGPUBufferMapAsyncStatus status) {
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

WGPUBufferUsageFlags wgpuBufferGetUsage(WGPUBuffer buffer)
{
    return WebGPU::fromAPI(buffer).usage();
}
