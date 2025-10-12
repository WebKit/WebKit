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
#import "CommandBuffer.h"
#import "Device.h"

#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBGPU_SWIFT)
#import "WebGPUSwiftInternal.h"

DEFINE_SWIFTCXX_THUNK(WebGPU::Buffer, copyFrom, void, const std::span<const uint8_t>, const size_t);
#endif

namespace WebGPU {

template <typename T>
static inline auto span(id<MTLBuffer> buffer)
{
    return unsafeMakeSpan(static_cast<T*>(buffer.contents), buffer.length / sizeof(T));
}

template <typename T>
static inline auto span(id<MTLBuffer> buffer, uint64_t byteOffset)
{
    auto byteSpan = span<uint8_t>(buffer).subspan(byteOffset);
    return unsafeMakeSpan(static_cast<T*>(static_cast<void*>(byteSpan.data())), (byteOffset < buffer.length) ? (buffer.length - byteOffset) / sizeof(T) : 0);
}

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
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite | WGPUBufferUsage_Index))
        return MTLStorageModeManaged;
    if (mappedAtCreation)
        return MTLStorageModeManaged;
#else
    UNUSED_PARAM(mappedAtCreation);
    UNUSED_PARAM(usage);
#endif
    return MTLStorageModePrivate;
}

id<MTLBuffer> Device::safeCreateBuffer(NSUInteger length, MTLStorageMode storageMode, bool skipAttribution, MTLCPUCacheMode cpuCacheMode, MTLHazardTrackingMode hazardTrackingMode) const
{
    MTLResourceOptions resourceOptions = (cpuCacheMode << MTLResourceCPUCacheModeShift) | (storageMode << MTLResourceStorageModeShift) | (hazardTrackingMode << MTLResourceHazardTrackingModeShift);
    id<MTLBuffer> buffer = [m_device newBufferWithLength:std::max<NSUInteger>(1, length) options:resourceOptions];
    if (!skipAttribution)
        setOwnerWithIdentity(buffer);
    return buffer;
}

id<MTLBuffer> Device::safeCreateBuffer(NSUInteger length, bool skipAttribution) const
{
    return safeCreateBuffer(length, MTLStorageModeShared, skipAttribution);
}

Ref<Buffer> Device::createBuffer(const WGPUBufferDescriptor& descriptor)
{
    if (!isValid())
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

    buffer.label = fromAPI(descriptor.label).createNSString().get();

    auto initialMapState = Buffer::State::Unmapped;
    auto initialMappingRange = Buffer::MappingRange {
        .beginOffset = static_cast<size_t>(0),
        .endOffset = static_cast<size_t>(0)
    };
    if (descriptor.mappedAtCreation) {
        initialMapState = Buffer::State::MappedAtCreation;
        initialMappingRange = Buffer::MappingRange {
            .beginOffset = static_cast<size_t>(0),
            .endOffset = static_cast<size_t>(descriptor.size)
        };
    }

    auto apiBuffer = Buffer::create(buffer, descriptor.size, descriptor.usage, initialMapState, initialMappingRange, *this);
    m_bufferMap.add(buffer.gpuAddress, apiBuffer.ptr());
    return apiBuffer;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(Buffer);

Buffer::Buffer(id<MTLBuffer> buffer, uint64_t initialSize, WGPUBufferUsageFlags usage, State initialState, MappingRange initialMappingRange, Device& device)
    : m_buffer(buffer)
    , m_initialSize(initialSize)
    , m_usage(usage)
    , m_state(initialState)
    , m_mappingRange(initialMappingRange)
    , m_device(device)
#if CPU(X86_64) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))
    , m_mappedAtCreation(m_state == State::MappedAtCreation)
#endif
{
    if (m_usage & WGPUBufferUsage_Indirect) {
        m_indirectBuffer = device.safeCreateBuffer(sizeof(WebKitMTLDrawPrimitivesIndirectArguments), MTLStorageModeShared);
        m_indirectIndexedBuffer = device.safeCreateBuffer(sizeof(WebKitMTLDrawIndexedPrimitivesIndirectArguments), MTLStorageModeShared);
    }
}

Buffer::Buffer(Device& device)
    : m_device(device)
{
}

Buffer::~Buffer()
{
    m_device->removeBufferFromCache(m_buffer.gpuAddress);
}

void Buffer::incrementBufferMapCount()
{
    for (auto commandEncoder : m_commandEncoders) {
        if (RefPtr ptr = m_device->commandEncoderFromIdentifier(commandEncoder))
            ptr->incrementBufferMapCount();
    }
}

void Buffer::decrementBufferMapCount()
{
    for (auto commandEncoder : m_commandEncoders) {
        if (RefPtr ptr = m_device->commandEncoderFromIdentifier(commandEncoder))
            ptr->decrementBufferMapCount();
    }
}

void Buffer::setCommandEncoder(CommandEncoder& commandEncoder, bool mayModifyBuffer) const
{
    UNUSED_PARAM(mayModifyBuffer);
    CommandEncoder::trackEncoder(commandEncoder, m_commandEncoders);
#if !CPU(X86_64)
    if (m_device->isShaderValidationEnabled())
#endif
        commandEncoder.addBuffer(m_buffer);

    if (m_state != State::Unmapped)
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

    setState(State::Destroyed);
    for (auto commandEncoder : m_commandEncoders) {
        if (RefPtr ptr = m_device->commandEncoderFromIdentifier(commandEncoder))
            ptr->makeSubmitInvalid();
    }

    m_commandEncoders.clear();
    m_device->removeBufferFromCache(m_buffer.gpuAddress);
    m_buffer = protectedDevice()->placeholderBuffer();
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
    auto result = checkedDifference<uint64_t>(size, offset);
    if (result.hasOverflowed())
        return 0;
    return result.value();
}
  
std::span<uint8_t> Buffer::getMappedRange(size_t offset, size_t size)
{
#if ENABLE(WEBGPU_SWIFT)
    if (isWebGPUSwiftEnabled())
        return Buffer_getMappedRange_thunk(this, offset, size);
#endif

    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-getmappedrange
    if (!isValid())
        return std::span<uint8_t> { };

    auto rangeSize = size;
    if (size == WGPU_WHOLE_MAP_SIZE)
        rangeSize = computeRangeSize(this->currentSize(), offset);

    if (!validateGetMappedRange(offset, rangeSize))
        return std::span<uint8_t> { };

    m_mappedRanges.add({ offset, offset + rangeSize });
    m_mappedRanges.compact();

    if (!m_buffer.contents)
        return { };
    return getBufferContents().subspan(offset);
}

std::span<uint8_t> Buffer::getBufferContents()
{
    return span<uint8_t>(m_buffer);
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

    auto device = protectedDevice();

    if (NSString* error = errorValidatingMapAsync(mode, offset, rangeSize)) {
        device->generateAValidationError(error);

        callback(WGPUBufferMapAsyncStatus_ValidationError);
        return;
    }

    setState(State::MappingPending);
    incrementBufferMapCount();

    m_mapMode = mode;

    device->protectedQueue()->onSubmittedWorkDone([protectedThis = Ref { *this }, offset, rangeSize, callback = WTFMove(callback)](WGPUQueueWorkDoneStatus status) mutable {
        if (protectedThis->m_state == State::MappingPending) {
            protectedThis->setState(State::Mapped);

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

void Buffer::setState(State state)
{
    if (m_state != State::Destroyed)
        m_state = state;
}
  
void Buffer::unmap()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-unmap

    if (!validateUnmap() && !protectedDevice()->isValid())
        return;

    decrementBufferMapCount();
    m_maxUnsignedIndex = m_maxUshortIndex = 0;
    indirectBufferInvalidated();

#if CPU(X86_64) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))
    if (m_buffer.storageMode == MTLStorageModeManaged) {
        if (m_mappedAtCreation)
            [m_buffer didModifyRange:NSMakeRange(0, m_buffer.length)];
        else {
            for (const auto& mappedRange : m_mappedRanges)
                [m_buffer didModifyRange:NSMakeRange(static_cast<NSUInteger>(mappedRange.begin()), static_cast<NSUInteger>(mappedRange.end() - mappedRange.begin()))];
        }
    }
#endif

    setState(State::Unmapped);
    m_mappedRanges = MappedRanges();
}

void Buffer::setLabel(String&& label)
{
    m_buffer.label = label.createNSString().get();
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

id<MTLBuffer> Buffer::indirectBuffer() const
{
    return m_indirectBuffer;
}

static DrawIndexCacheContainerKey makeKey(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, uint32_t instanceCount, MTLIndexType indexType, uint32_t firstInstance, uint32_t baseVertex, uint32_t primitiveOffset, uint32_t minInstanceCount, id<MTLIndirectCommandBuffer> icb)
{
    using namespace std;
    return make_pair(firstIndex, make_pair(indexCount, make_pair(vertexCount, make_pair(instanceCount, make_pair(firstInstance, make_pair(baseVertex, make_pair(minInstanceCount, make_pair(primitiveOffset | (indexType << 1), icb.gpuResourceID._impl))))))));
}

std::optional<DrawIndexCacheContainerIterator> Buffer::canSkipDrawIndexedValidation(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, uint32_t instanceCount, MTLIndexType indexType, uint32_t firstInstance, uint32_t baseVertex, uint32_t primitiveOffset, uint32_t minInstanceCount, id<MTLIndirectCommandBuffer> icb) const
{
    auto containerIt = m_drawIndexedCache.find(makeKey(firstIndex, indexCount, vertexCount, instanceCount, indexType, firstInstance, baseVertex, primitiveOffset, minInstanceCount, icb));
    if (containerIt != m_drawIndexedCache.end() && containerIt->value)
        return containerIt;

    return std::nullopt;
}

void Buffer::drawIndexedValidated(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, uint32_t instanceCount, MTLIndexType indexType, uint32_t firstInstance, uint32_t baseVertex, uint32_t primitiveOffset, uint32_t minInstanceCount, id<MTLIndirectCommandBuffer> icb)
{
    m_drawIndexedCache.set(makeKey(firstIndex, indexCount, vertexCount, instanceCount, indexType, firstInstance, baseVertex, primitiveOffset, minInstanceCount, icb), 1);
}

template <typename T>
static bool verifyIndexBufferData(id<MTLBuffer> buffer, uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseVertex, uint32_t baseInstance, uint32_t minInstanceCount, uint32_t primitiveRestart, uint32_t indexBufferOffsetInBytes = 0)
{
    if (baseInstance + instanceCount > minInstanceCount || baseInstance >= minInstanceCount)
        return false;

    bool negativeCondition = baseVertex + primitiveRestart < baseVertex;
    auto indexData = span<T>(buffer, indexBufferOffsetInBytes);
    if (firstIndex + indexCount > indexData.size() || negativeCondition)
        return false;
    for (size_t index = firstIndex; index < firstIndex + indexCount; ++index) {
        T vertexIndex = primitiveRestart + indexData[index];
        if (vertexIndex + baseVertex >= vertexCount + primitiveRestart)
            return false;
    }

    return true;
}

void Buffer::takeSlowIndexValidationPath(CommandBuffer& commandBuffer, uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, uint32_t instanceCount, MTLIndexType indexType, uint32_t firstInstance, uint32_t baseVertex, uint32_t minInstanceCount, uint32_t primitiveOffset)
{
    WTFLogAlways("WARNING: Severe performance penalty due to encoding drawIndexed calls out of order with submission"); // NOLINT
    Ref queue = protectedDevice()->getQueue();
    queue->waitForAllCommitedWorkToComplete();
    queue->synchronizeResourceAndWait(m_buffer);
    bool verified = false;
    if (indexType == MTLIndexTypeUInt16)
        verified = verifyIndexBufferData<uint16_t>(m_buffer, firstIndex, indexCount, vertexCount, instanceCount, baseVertex, firstInstance, minInstanceCount, primitiveOffset);
    else
        verified = verifyIndexBufferData<uint32_t>(m_buffer, firstIndex, indexCount, vertexCount, instanceCount, baseVertex, firstInstance, minInstanceCount, primitiveOffset);

    if (!verified) {
        auto priorData = getBufferContents();
        queue->clearBuffer(m_buffer);
        queue->finalizeBlitCommandEncoder();
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        if (m_buffer.storageMode == MTLStorageModeManaged)
            [m_buffer didModifyRange:NSMakeRange(0, m_buffer.length)];
#endif
        commandBuffer.addPostCommitHandler([queue, priorData, protectedThis = Ref { *this }](id<MTLCommandBuffer> mtlCommandBuffer) {
            [mtlCommandBuffer waitUntilCompleted];

            queue->writeBuffer(*protectedThis.ptr(), 0, priorData);
        });
    }
}

void Buffer::takeSlowIndirectIndexValidationPath(CommandBuffer& commandBuffer, Buffer& apiIndexBuffer, MTLIndexType indexType, uint32_t indexBufferOffsetInBytes, uint32_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount, MTLPrimitiveType primitiveType)
{
    WTFLogAlways("WARNING: Severe performance penalty due to encoding drawIndexedIndirect calls out of order with submission"); // NOLINT
    Ref queue = protectedDevice()->getQueue();
    queue->waitForAllCommitedWorkToComplete();
    queue->synchronizeResourceAndWait(m_buffer);
    if (m_buffer.length < indexBufferOffsetInBytes + sizeof(MTLDrawIndexedPrimitivesIndirectArguments))
        return;
    auto bufferSubData = span<MTLDrawIndexedPrimitivesIndirectArguments>(m_buffer, indexBufferOffsetInBytes);
    if (!bufferSubData.data() || !bufferSubData.size())
        return;

    auto& args = *bufferSubData.data();
    bool verified = false;
    auto primitiveOffset = primitiveType == MTLPrimitiveTypeLineStrip || primitiveType == MTLPrimitiveTypeTriangleStrip ? 1u : 0u;
    auto effectiveVertexCount = minVertexCount == RenderBundleEncoder::invalidVertexInstanceCount ? (minVertexCount - primitiveOffset) : (args.baseVertex + minVertexCount);
    if (indexType == MTLIndexTypeUInt16)
        verified = verifyIndexBufferData<uint16_t>(apiIndexBuffer.buffer(), args.indexStart, args.indexCount, effectiveVertexCount, args.instanceCount, args.baseVertex, args.baseInstance, minInstanceCount, primitiveOffset);
    else
        verified = verifyIndexBufferData<uint32_t>(apiIndexBuffer.buffer(), args.indexStart, args.indexCount, effectiveVertexCount, args.instanceCount, args.baseVertex, args.baseInstance, minInstanceCount, primitiveOffset);

    if (!verified) {
        auto priorData = getBufferContents();
        queue->clearBuffer(m_buffer, indirectOffset, sizeof(MTLDrawPrimitivesIndirectArguments));
        queue->finalizeBlitCommandEncoder();
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        if (m_buffer.storageMode == MTLStorageModeManaged)
            [m_buffer didModifyRange:NSMakeRange(0, m_buffer.length)];
#endif
        commandBuffer.addPostCommitHandler([queue, priorData, protectedThis = Ref { *this }](id<MTLCommandBuffer> mtlCommandBuffer) {
            [mtlCommandBuffer waitUntilCompleted];

            queue->writeBuffer(*protectedThis.ptr(), 0, priorData);
        });
    }
}

static bool verifyIndirectBufferData(MTLDrawPrimitivesIndirectArguments& input, uint32_t minVertexCount, uint32_t minInstanceCount)
{
    bool vertexCondition = input.vertexCount + input.vertexStart > minVertexCount || input.vertexStart >= minVertexCount;
    bool instanceCondition = input.baseInstance + input.instanceCount > minInstanceCount || input.baseInstance >= minInstanceCount;
    return !vertexCondition && !instanceCondition;
}

void Buffer::takeSlowIndirectValidationPath(CommandBuffer& commandBuffer, uint64_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount)
{
    WTFLogAlways("WARNING: Severe performance penalty due to encoding drawIndirect calls out of order with submission"); // NOLINT
    Ref queue = protectedDevice()->getQueue();
    queue->waitForAllCommitedWorkToComplete();
    queue->synchronizeResourceAndWait(m_buffer);
    auto bufferSubData = span<MTLDrawPrimitivesIndirectArguments>(m_buffer, indirectOffset);
    if (!bufferSubData.data() || !bufferSubData.size())
        return;

    auto& args = *bufferSubData.data();
    bool verified = verifyIndirectBufferData(args, minVertexCount, minInstanceCount);

    if (!verified) {
        auto priorData = getBufferContents();
        MTLDrawPrimitivesIndirectArguments data = {
            .vertexCount = std::min(args.vertexCount, minVertexCount),
            .instanceCount = std::min(args.instanceCount, minInstanceCount),
            .vertexStart = args.vertexStart,
            .baseInstance = args.baseInstance
        };
        auto newDataSpan = asMutableByteSpan(data);
        queue->writeBuffer(m_buffer, indirectOffset, newDataSpan);
        queue->finalizeBlitCommandEncoder();
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        if (m_buffer.storageMode == MTLStorageModeManaged)
            [m_buffer didModifyRange:NSMakeRange(0, m_buffer.length)];
#endif
        commandBuffer.addPostCommitHandler([queue, priorData, protectedThis = Ref { *this }](id<MTLCommandBuffer> mtlCommandBuffer) {
            [mtlCommandBuffer waitUntilCompleted];

            queue->writeBuffer(*protectedThis.ptr(), 0, priorData);
        });
    }
}

void Buffer::skippedDrawIndexedValidation(CommandEncoder& commandEncoder, DrawIndexCacheContainerIterator it)
{
    CommandEncoder::trackEncoder(commandEncoder, m_skippedValidationCommandEncoders);
    commandEncoder.skippedDrawIndexedValidation(m_buffer.gpuAddress, it);
}

void Buffer::skippedDrawIndirectIndexedValidation(CommandEncoder& commandEncoder, Buffer* apiIndexBuffer, MTLIndexType indexType, uint32_t indexBufferOffsetInBytes, uint64_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount, MTLPrimitiveType primitiveType)
{
    if (!apiIndexBuffer)
        return;

    CommandEncoder::trackEncoder(commandEncoder, m_skippedValidationCommandEncoders);
    commandEncoder.addOnCommitHandler([weakThis = ThreadSafeWeakPtr { *this }, apiIndexBuffer = RefPtr { apiIndexBuffer }, indexType, indexBufferOffsetInBytes, indirectOffset, minVertexCount, minInstanceCount, primitiveType](CommandBuffer& commandBuffer, CommandEncoder& commandEncoder) {
        if (!weakThis.get())
            return true;

        RefPtr protectedThis = weakThis.get();
        protectedThis->m_skippedValidationCommandEncoders.remove(commandEncoder.uniqueId());
        if (protectedThis->m_mustTakeSlowIndexValidationPath) {
            protectedThis->takeSlowIndirectIndexValidationPath(commandBuffer, *apiIndexBuffer.get(), indexType, indexBufferOffsetInBytes, indirectOffset, minVertexCount, minInstanceCount, primitiveType);
            commandBuffer.addPostCommitHandler([protectedThis = WTFMove(protectedThis)](id<MTLCommandBuffer>) {
                protectedThis->m_mustTakeSlowIndexValidationPath = false;
            });
        }
        return true;
    });
}

void Buffer::skippedDrawIndirectValidation(CommandEncoder& commandEncoder, uint64_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount)
{
    CommandEncoder::trackEncoder(commandEncoder, m_skippedValidationCommandEncoders);
    commandEncoder.addOnCommitHandler([weakThis = ThreadSafeWeakPtr { *this }, indirectOffset, minVertexCount, minInstanceCount](CommandBuffer& commandBuffer, CommandEncoder& commandEncoder) {
        if (!weakThis.get())
            return true;

        RefPtr protectedThis = weakThis.get();
        protectedThis->m_skippedValidationCommandEncoders.remove(commandEncoder.uniqueId());
        if (protectedThis->m_mustTakeSlowIndexValidationPath) {
            protectedThis->takeSlowIndirectValidationPath(commandBuffer, indirectOffset, minVertexCount, minInstanceCount);
            commandBuffer.addPostCommitHandler([protectedThis = WTFMove(protectedThis)](id<MTLCommandBuffer>) {
                protectedThis->m_mustTakeSlowIndexValidationPath = false;
            });
        }
        return true;
    });
}

bool Buffer::didReadOOB(id<MTLIndirectCommandBuffer> icb) const
{
    auto it = m_didReadOOB.find(icb.gpuResourceID._impl);
    return it == m_didReadOOB.end() ? false : it->value;
}

void Buffer::didReadOOB(uint32_t v, id<MTLIndirectCommandBuffer> icb)
{
    m_didReadOOB.set(icb.gpuResourceID._impl, !!v || didReadOOB(icb));
}

bool Buffer::indirectBufferRequiresRecomputation(uint64_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount) const
{
    return m_indirectCache.indirectOffset != indirectOffset || m_indirectCache.minVertexCount != minVertexCount || m_indirectCache.minInstanceCount != minInstanceCount || m_indirectCache.drawType != IndirectArgsCache::IndirectDraw;
}

bool Buffer::indirectIndexedBufferRequiresRecomputation(MTLIndexType indexType, NSUInteger indexBufferOffsetInBytes, uint64_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount) const
{
    return Buffer::indirectBufferRequiresRecomputation(indirectOffset, minVertexCount, minInstanceCount) || m_indirectCache.indexType != indexType || m_indirectCache.indexBufferOffsetInBytes != indexBufferOffsetInBytes || m_indirectCache.drawType != IndirectArgsCache::IndirectIndexedDraw;
}

void Buffer::indirectBufferRecomputed(uint64_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount)
{
    m_indirectCache.indirectOffset = indirectOffset;
    m_indirectCache.minVertexCount = minVertexCount;
    m_indirectCache.minInstanceCount = minInstanceCount;
    m_indirectCache.drawType = IndirectArgsCache::IndirectDraw;
}

void Buffer::indirectIndexedBufferRecomputed(MTLIndexType indexType, NSUInteger indexBufferOffsetInBytes, uint64_t indirectOffset, uint32_t minVertexCount, uint32_t minInstanceCount)
{
    m_indirectCache.indexType = indexType;
    m_indirectCache.indexBufferOffsetInBytes = indexBufferOffsetInBytes;
    m_indirectCache.indirectOffset = indirectOffset;
    m_indirectCache.minVertexCount = minVertexCount;
    m_indirectCache.minInstanceCount = minInstanceCount;
    m_indirectCache.drawType = IndirectArgsCache::IndirectIndexedDraw;
}

void Buffer::indirectBufferInvalidated(CommandEncoder& commandEncoder)
{
    m_maxUnsignedIndex = m_maxUshortIndex = 0;
    indirectBufferInvalidated();

    commandEncoder.addOnCommitHandler([weakThis = ThreadSafeWeakPtr { *this }, weakCommandEncoder = WeakPtr { commandEncoder }](CommandBuffer&, CommandEncoder&) {
        if (!weakThis.get() || !weakCommandEncoder)
            return true;

        RefPtr protectedThis = weakThis.get();
        protectedThis->m_maxUnsignedIndex = protectedThis->m_maxUshortIndex = 0;
        RefPtr commandEncoder = weakCommandEncoder.get();
        protectedThis->indirectBufferInvalidated(commandEncoder.get());
        return true;
    });
}

static size_t computeSize(HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>& encoders, Device& device)
{
    encoders.removeIf([&](uint64_t encoderId) {
        return !device.commandEncoderFromIdentifier(encoderId);
    });
    return encoders.size();
}

bool Buffer::needsIndexValidation(uint32_t maxUnsignedIndex, uint16_t maxUshortIndex)
{
    bool needsUpdate = false;
    if (maxUnsignedIndex > m_maxUnsignedIndex) {
        m_maxUnsignedIndex = maxUnsignedIndex;
        needsUpdate = true;
    }
    if (m_maxUshortIndex > maxUshortIndex) {
        m_maxUshortIndex = maxUshortIndex;
        needsUpdate = true;
    }

    return needsUpdate;
}

void Buffer::indirectBufferInvalidated(CommandEncoder* commandEncoder)
{
    if (!(m_usage & (WGPUBufferUsage_Indirect | WGPUBufferUsage_Index)))
        return;

    if (auto currentSize = computeSize(m_skippedValidationCommandEncoders, m_device.get())) {
        bool validationNotNeeded = currentSize == 1 && commandEncoder && commandEncoder == m_device->commandEncoderFromIdentifier(*m_skippedValidationCommandEncoders.begin().get());
        if (!validationNotNeeded)
            m_mustTakeSlowIndexValidationPath = true;
    }

    m_gpuResourceMap.clear();
    m_drawIndexedCache.clear();
    m_indirectCache = {
        .indirectOffset = UINT64_MAX,
        .indexBufferOffsetInBytes = UINT64_MAX,
        .minVertexCount = 0,
        .minInstanceCount = 0,
        .indexType = MTLIndexTypeUInt16
    };
}

void Buffer::removeSkippedValidationCommandEncoder(uint64_t uniqueId)
{
    m_skippedValidationCommandEncoders.remove(uniqueId);
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
    WebGPU::protectedFromAPI(buffer)->destroy();
}

WGPUBufferMapState wgpuBufferGetMapState(WGPUBuffer buffer)
{
    switch (WebGPU::protectedFromAPI(buffer)->state()) {
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

std::span<uint8_t> wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    return WebGPU::protectedFromAPI(buffer)->getMappedRange(offset, size);
}

std::span<uint8_t> wgpuBufferGetBufferContents(WGPUBuffer buffer)
{
    return WebGPU::protectedFromAPI(buffer)->getBufferContents();
}

uint64_t wgpuBufferGetInitialSize(WGPUBuffer buffer)
{
    return WebGPU::protectedFromAPI(buffer)->initialSize();
}

uint64_t wgpuBufferGetCurrentSize(WGPUBuffer buffer)
{
    return WebGPU::protectedFromAPI(buffer)->currentSize();
}

void wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapCallback callback, void* userdata)
{
    WebGPU::protectedFromAPI(buffer)->mapAsync(mode, offset, size, [callback, userdata](WGPUBufferMapAsyncStatus status) {
        callback(status, userdata);
    });
}

void wgpuBufferMapAsyncWithBlock(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback)
{
    WebGPU::protectedFromAPI(buffer)->mapAsync(mode, offset, size, [callback = WebGPU::fromAPI(WTFMove(callback))](WGPUBufferMapAsyncStatus status) {
        callback(status);
    });
}

void wgpuBufferUnmap(WGPUBuffer buffer)
{
    WebGPU::protectedFromAPI(buffer)->unmap();
}

void wgpuBufferSetLabel(WGPUBuffer buffer, const char* label)
{
    WebGPU::protectedFromAPI(buffer)->setLabel(WebGPU::fromAPI(label));
}

WGPUBufferUsageFlags wgpuBufferGetUsage(WGPUBuffer buffer)
{
    return WebGPU::protectedFromAPI(buffer)->usage();
}

void wgpuBufferCopy(WGPUBuffer buffer, std::span<const uint8_t> data, size_t offset)
{
#if ENABLE(WEBGPU_SWIFT)
    WebGPU::protectedFromAPI(buffer)->copyFrom(data, offset);
#else
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(data);
    UNUSED_PARAM(offset);

#endif
}
