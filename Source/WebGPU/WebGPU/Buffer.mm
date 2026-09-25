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

#import <wtf/Borrow.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBGPU_SWIFT)
#import "CxxBridging.h"
#import <WebGPU/CxxBridgingPublic.h>
#import <WebGPU/WGPUTextureImpl.h>
#import <WebGPU/WebGPU.h>
#import "WebGPUSwift-Generated.h"
#endif

namespace WebGPU::Metal {

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

static bool NODELETE validateDescriptor(const Device& device, const WebGPU::BufferDescriptor& descriptor)
{
    UNUSED_PARAM(device);

    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpubufferdescriptor

    if (device.isLost())
        return false;

    // FIXME: "If any of the bits of descriptor’s usage aren’t present in this device’s [[allowed buffer usages]] return false."

    if (descriptor.usage.containsAll({ WebGPU::BufferUsage::MapRead, WebGPU::BufferUsage::MapWrite }))
        return false;

    return true;
}

static bool NODELETE validateCreateBuffer(const Device& device, const WebGPU::BufferDescriptor& descriptor)
{
    if (!device.isValid())
        return false;

    if (!validateDescriptor(device, descriptor))
        return false;

    // The C API conversion already rejected unknown usage bits.
    auto usage = descriptor.usage;
    if (usage.isEmpty())
        return false;

    if (usage.contains(WebGPU::BufferUsage::MapRead) && !usage.containsOnly({ WebGPU::BufferUsage::CopyDestination, WebGPU::BufferUsage::MapRead }))
        return false;

    if (usage.contains(WebGPU::BufferUsage::MapWrite) && !usage.containsOnly({ WebGPU::BufferUsage::CopySource, WebGPU::BufferUsage::MapWrite }))
        return false;

    if (descriptor.mappedAtCreation && (descriptor.size % 4))
        return false;

    if (descriptor.size > device.limits().maxBufferSize)
        return false;

    return true;
}

static MTLStorageMode NODELETE storageMode(bool deviceHasUnifiedMemory, OptionSet<WebGPU::BufferUsage> usage, bool mappedAtCreation)
{
    if (deviceHasUnifiedMemory)
        return MTLStorageModeShared;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (usage.containsAny({ WebGPU::BufferUsage::MapRead, WebGPU::BufferUsage::MapWrite, WebGPU::BufferUsage::Index }))
        return MTLStorageModeManaged;
    if (mappedAtCreation)
        return MTLStorageModeManaged;
    ALLOW_DEPRECATED_DECLARATIONS_END
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

Ref<Buffer> Device::createBuffer(const WebGPU::BufferDescriptor& descriptor)
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
    MTLStorageMode storageMode = WebGPU::Metal::storageMode(hasUnifiedMemory(), descriptor.usage, descriptor.mappedAtCreation);
    auto buffer = safeCreateBuffer(static_cast<NSUInteger>(descriptor.size), storageMode);
    if (!buffer) {
        generateAnOutOfMemoryError("Allocation failure."_s);

        return Buffer::createInvalid(*this);
    }

    buffer.label = descriptor.label.createNSString().get();

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

Buffer::Buffer(id<MTLBuffer> buffer, uint64_t initialSize, OptionSet<WebGPU::BufferUsage> usage, State initialState, MappingRange initialMappingRange, Device& device)
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
    bool isNewEntry = commandEncoder.trackEncoderForBuffer(*this, m_commandEncoders);
#if !CPU(X86_64)
    if (m_device->isShaderValidationEnabled())
#endif
        commandEncoder.addBuffer(m_buffer);

    if (m_state != State::Unmapped && isNewEntry)
        commandEncoder.incrementBufferMapCount();
    if (isDestroyed())
        commandEncoder.makeSubmitInvalid();
}

void Buffer::destroy()
{
    crashIfBorrowed();

    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-destroy

    if (m_state != State::Unmapped && m_state != State::Destroyed) {
        // FIXME: ASSERT() that this call doesn't fail.
        unmap();
    }

    setState(State::Destroyed);
    m_device->makeSubmitInvalidClearingEncoders(m_commandEncoders);
    m_device->removeBufferFromCache(m_buffer.gpuAddress);
    m_buffer = m_device->placeholderBuffer();
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

static uint64_t NODELETE computeRangeSize(uint64_t size, uint64_t offset)
{
    auto result = checkedDifference<uint64_t>(size, offset);
    if (result.hasOverflowed())
        return 0;
    return result.value();
}

// The offset and size of a mapped range as size_t. std::nullopt when either does not fit in size_t
// (32-bit platforms): no buffer is that large, so such a range is out of bounds.
static std::optional<std::pair<size_t, size_t>> NODELETE mappedRangeOffsetAndSize(uint64_t bufferSize, uint64_t offset, std::optional<uint64_t> size)
{
    uint64_t rangeSize = size ? *size : computeRangeSize(bufferSize, offset);
    if (!isInBounds<size_t>(offset) || !isInBounds<size_t>(rangeSize))
        return std::nullopt;
    return std::pair { static_cast<size_t>(offset), static_cast<size_t>(rangeSize) };
}

void Buffer::getMappedRange(uint64_t offset, std::optional<uint64_t> size, NOESCAPE const Function<void(std::span<uint8_t>)>& callback)
{
    callback(getMappedRangeSpan(offset, size));
}

std::span<uint8_t> Buffer::getMappedRangeSpan(uint64_t apiOffset, std::optional<uint64_t> size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-getmappedrange
    auto offsetAndSize = mappedRangeOffsetAndSize(currentSize(), apiOffset, size);
    if (!offsetAndSize)
        return std::span<uint8_t> { };
    auto [offset, rangeSize] = *offsetAndSize;

#if ENABLE(WEBGPU_SWIFT)
    if (isWebGPUSwiftEnabled())
        return bufferGetMappedRange(this, offset, rangeSize);
#endif

    if (!isValid())
        return std::span<uint8_t> { };

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

void Buffer::copyFrom(std::span<const uint8_t> data, size_t offset)
{
#if ENABLE(WEBGPU_SWIFT)
    bufferCopyFrom(this, data, offset);
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(offset);
#endif
}

NSString *Buffer::errorValidatingMapAsync(OptionSet<WebGPU::MapMode> mode, size_t offset, size_t rangeSize) const
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

    if (mode != WebGPU::MapMode::Read && mode != WebGPU::MapMode::Write)
        return ERROR_STRING(@"readWriteModeFlags != Read && readWriteModeFlags != Write");

    if (mode.contains(WebGPU::MapMode::Read) && !m_usage.contains(WebGPU::BufferUsage::MapRead))
        return ERROR_STRING(@"(mode & Read) && !(usage & Read)");

    if (mode.contains(WebGPU::MapMode::Write) && !m_usage.contains(WebGPU::BufferUsage::MapWrite))
        return ERROR_STRING(@"(mode & Write) && !(usage & Write)");

#undef ERROR_STRING
    return nil;
}

void Buffer::mapAsync(OptionSet<WebGPU::MapMode> mode, uint64_t apiOffset, std::optional<uint64_t> size, CompletionHandler<void(bool)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-mapasync

    auto offsetAndSize = mappedRangeOffsetAndSize(currentSize(), apiOffset, size);

    Ref device = m_device;

    if (NSString* error = offsetAndSize ? errorValidatingMapAsync(mode, offsetAndSize->first, offsetAndSize->second) : @"GPUBuffer.mapAsync: offset and rangeSize overflowed") {
        device->generateAValidationError(error);

        callback(false);
        return;
    }

    setState(State::MappingPending);
    incrementBufferMapCount();

    m_mapMode = mode;

    device->getQueue()->onSubmittedWorkDone([protectedThis = protect(*this), offset = offsetAndSize->first, rangeSize = offsetAndSize->second, callback = WTF::move(callback)](WGPUQueueWorkDoneStatus status) mutable {
        if (protectedThis->m_state == State::MappingPending) {
            protectedThis->setState(State::Mapped);

            protectedThis->m_mappingRange = { offset, offset + rangeSize };

            protectedThis->m_mappedRanges = MappedRanges();
        }

        ASSERT(status != WGPUQueueWorkDoneStatus_Force32);
        callback(status == WGPUQueueWorkDoneStatus_Success);
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

    if (!validateUnmap() && !m_device->isValid())
        return;

    decrementBufferMapCount();
    m_maxUnsignedIndex = m_maxUshortIndex = 0;
    indirectBufferInvalidated();

#if CPU(X86_64) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (m_buffer.storageMode == MTLStorageModeManaged) {
        if (m_mappedAtCreation)
            [m_buffer didModifyRange:NSMakeRange(0, m_buffer.length)];
        else {
            for (const auto& mappedRange : m_mappedRanges)
                [m_buffer didModifyRange:NSMakeRange(static_cast<NSUInteger>(mappedRange.begin()), static_cast<NSUInteger>(mappedRange.end() - mappedRange.begin()))];
        }
    }
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    setState(State::Unmapped);
    m_mappedRanges = MappedRanges();
}

void Buffer::setLabel(String&& label)
{
    m_buffer.label = label.createNSString().get();
}

void Buffer::generateAValidationError(String&& message)
{
    m_device->generateAValidationError(WTF::move(message));
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

static DrawIndexCacheContainerKey makeKey(uint32_t firstIndex, uint32_t indexCount, MTLIndexType indexType, uint32_t primitiveOffset, id<MTLIndirectCommandBuffer> icb)
{
    return { firstIndex, indexCount, primitiveOffset | static_cast<uint32_t>(indexType << 1), static_cast<uint32_t>(icb.gpuResourceID._impl & 0xffffffff), static_cast<uint32_t>((icb.gpuResourceID._impl >> 32) & 0xffffffff) };
}

std::optional<DrawIndexCacheContainerIterator> Buffer::canSkipDrawIndexedValidation(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, MTLIndexType indexType, uint32_t primitiveOffset, id<MTLIndirectCommandBuffer> icb) const
{
    auto containerIt = m_drawIndexedCache.find(makeKey(firstIndex, indexCount, indexType, primitiveOffset, icb));
    if (containerIt != m_drawIndexedCache.end() && containerIt->value.indexContentsGeneration == m_indexContentsGeneration && containerIt->value.vertexCount <= vertexCount)
        return containerIt;

    return std::nullopt;
}

void Buffer::drawIndexedValidated(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, MTLIndexType indexType, uint32_t primitiveOffset, uint64_t validationGeneration, id<MTLIndirectCommandBuffer> icb)
{
    constexpr auto maxCacheSize = 1000000;
    if (m_drawIndexedCache.size() > maxCacheSize)
        m_drawIndexedCache.clear();

    m_drawIndexedCache.set(makeKey(firstIndex, indexCount, indexType, primitiveOffset, icb), DrawIndexValidationCacheEntry { vertexCount, validationGeneration });

    // Update the validated high-water marks so future writeBuffer calls
    // with indices at or below these values don't need to invalidate.
    m_maxValidatedUnsignedIndex = std::max(m_maxValidatedUnsignedIndex, m_maxUnsignedIndex);
    m_maxValidatedUshortIndex = std::max(m_maxValidatedUshortIndex, m_maxUshortIndex);
}

template <typename T>
static bool verifyIndexBufferData(id<MTLBuffer> buffer, uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, uint32_t primitiveRestart, uint32_t indexBufferOffsetInBytes = 0)
{
    auto indexData = span<T>(buffer, indexBufferOffsetInBytes);
    if (firstIndex + indexCount > indexData.size())
        return false;
    for (size_t index = firstIndex; index < firstIndex + indexCount; ++index) {
        T vertexIndex = primitiveRestart + indexData[index];
        if (vertexIndex >= vertexCount + primitiveRestart)
            return false;
    }

    return true;
}

void Buffer::takeSlowIndexValidationPath(CommandBuffer& commandBuffer, uint32_t firstIndex, uint32_t indexCount, MTLIndexType indexType, uint32_t primitiveOffset, uint32_t vertexCount)
{
    WTFLogAlways("WARNING: Severe performance penalty due to encoding drawIndexed calls out of order with submission"); // NOLINT
    Ref queue = m_device->getQueue();
    queue->waitForAllCommitedWorkToComplete();
    queue->synchronizeResourceAndWait(m_buffer);
    bool verified = false;
    if (indexType == MTLIndexTypeUInt16)
        verified = verifyIndexBufferData<uint16_t>(m_buffer, firstIndex, indexCount, vertexCount, primitiveOffset);
    else
        verified = verifyIndexBufferData<uint32_t>(m_buffer, firstIndex, indexCount, vertexCount, primitiveOffset);

    if (!verified) {
        SUPPRESS_UNCOUNTED_ARG Vector<uint8_t> priorData = borrow(*this)->getBufferContents();

        queue->clearBuffer(m_buffer);
        queue->finalizeBlitCommandEncoder();
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (m_buffer.storageMode == MTLStorageModeManaged)
            [m_buffer didModifyRange:NSMakeRange(0, m_buffer.length)];
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
        commandBuffer.addPostCommitHandler([queue, priorData = WTF::move(priorData), protectedThis = protect(*this)](id<MTLCommandBuffer> mtlCommandBuffer) mutable {
            [mtlCommandBuffer waitUntilCompleted];
            queue->writeBuffer(*protectedThis.ptr(), 0, priorData.mutableSpan());
        });
    }
}

void Buffer::skippedDrawIndexedValidation(CommandEncoder& commandEncoder, DrawIndexCacheContainerIterator it)
{
    CommandEncoder::trackEncoder(commandEncoder, m_skippedValidationCommandEncoders);
    commandEncoder.skippedDrawIndexedValidation(m_buffer.gpuAddress, it);
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

void Buffer::indirectBufferInvalidated(CommandEncoder& commandEncoder)
{
    m_maxUnsignedIndex = m_maxUshortIndex = 0;
    indirectBufferInvalidated();

    commandEncoder.addOnCommitHandler([weakThis = ThreadSafeWeakPtr { *this }, weakCommandEncoder = ThreadSafeWeakPtr { commandEncoder }](CommandBuffer&, CommandEncoder&) {
        if (!weakThis.get() || !weakCommandEncoder.get())
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
    const bool needsUpdate = maxUnsignedIndex > m_maxValidatedUnsignedIndex || maxUshortIndex > m_maxValidatedUshortIndex;
    // Track the current write's max so we can update the validated
    // threshold after a successful clamp-shader pass.
    m_maxUnsignedIndex = maxUnsignedIndex;
    m_maxUshortIndex = maxUshortIndex;

    return needsUpdate;
}

void Buffer::indirectBufferInvalidated(CommandEncoder* commandEncoder)
{
    if (!m_usage.containsAny({ WebGPU::BufferUsage::Indirect, WebGPU::BufferUsage::Index }))
        return;

    if (auto currentSize = computeSize(m_skippedValidationCommandEncoders, m_device.get())) {
        bool validationNotNeeded = currentSize == 1 && commandEncoder && commandEncoder == m_device->commandEncoderFromIdentifier(*m_skippedValidationCommandEncoders.begin().get());
        if (!validationNotNeeded)
            m_mustTakeSlowIndexValidationPath = true;
    }

    m_gpuResourceMap.clear();
    m_drawIndexedCache.clear();
    ++m_contentsGeneration;
    ++m_indexContentsGeneration;
    m_indexValueUpperBoundUint = UINT32_MAX;
    m_indexValueUpperBoundUshort = UINT16_MAX;
    m_maxValidatedUnsignedIndex = 0;
    m_maxValidatedUshortIndex = 0;
}

void Buffer::removeSkippedValidationCommandEncoder(uint64_t uniqueId)
{
    m_skippedValidationCommandEncoders.remove(uniqueId);
}

void Buffer::clearMustTakeSlowIndexValidationPath()
{
    if (computeSize(m_skippedValidationCommandEncoders, m_device.get()))
        return;
    m_mustTakeSlowIndexValidationPath = false;
}

} // namespace WebGPU::Metal

#pragma mark WGPU Stubs

void NODELETE wgpuBufferAddRef(WGPUBuffer buffer)
{
    WebGPU::Metal::fromAPI(buffer).ref();
}

void wgpuBufferRelease(WGPUBuffer buffer)
{
    WebGPU::Metal::fromAPI(buffer).deref();
}

void wgpuBufferDestroy(WGPUBuffer buffer)
{
    protect(WebGPU::Metal::fromAPI(buffer))->destroy();
}

WGPUBufferMapState wgpuBufferGetMapState(WGPUBuffer buffer)
{
    switch (protect(WebGPU::Metal::fromAPI(buffer))->state()) {
    case WebGPU::Metal::Buffer::State::Mapped:
        return WGPUBufferMapState_Mapped;
    case WebGPU::Metal::Buffer::State::MappedAtCreation:
        return WGPUBufferMapState_Mapped;
    case WebGPU::Metal::Buffer::State::MappingPending:
        return WGPUBufferMapState_Pending;
    case WebGPU::Metal::Buffer::State::Unmapped:
        return WGPUBufferMapState_Unmapped;
    case WebGPU::Metal::Buffer::State::Destroyed:
        return WGPUBufferMapState_Unmapped;
    }
}

std::span<uint8_t> wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    return protect(WebGPU::Metal::fromAPI(buffer))->getMappedRangeSpan(offset, WebGPU::Metal::mapSizeFromAPI(size));
}

std::span<uint8_t> wgpuBufferGetBufferContents(WGPUBuffer buffer)
{
    return protect(WebGPU::Metal::fromAPI(buffer))->getBufferContents();
}

uint64_t wgpuBufferGetInitialSize(WGPUBuffer buffer)
{
    return WebGPU::Metal::fromAPI(buffer).initialSize();
}

uint64_t wgpuBufferGetCurrentSize(WGPUBuffer buffer)
{
    return protect(WebGPU::Metal::fromAPI(buffer))->currentSize();
}

// mapAsync() validates only the Read and Write bits of the mode, as in the WebGPU specification, so the
// conversion drops unknown bits instead of failing.
static OptionSet<WebGPU::MapMode> mapModeFromAPIIgnoringUnknownBits(WGPUMapMode mode)
{
    return *WebGPU::Metal::mapModeFromAPI(mode & (WGPUMapMode_Read | WGPUMapMode_Write));
}

static WGPUMapAsyncStatus mapAsyncStatusToAPI(bool success)
{
    return success ? WGPUMapAsyncStatus_Success : WGPUMapAsyncStatus_ValidationError;
}

void wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallback callback, void* userdata)
{
    protect(WebGPU::Metal::fromAPI(buffer))->mapAsync(mapModeFromAPIIgnoringUnknownBits(mode), offset, WebGPU::Metal::mapSizeFromAPI(size), [callback, userdata](bool success) {
        callback(mapAsyncStatusToAPI(success), userdata);
    });
}

void wgpuBufferMapAsyncWithBlock(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback)
{
    protect(WebGPU::Metal::fromAPI(buffer))->mapAsync(mapModeFromAPIIgnoringUnknownBits(mode), offset, WebGPU::Metal::mapSizeFromAPI(size), [callback = WebGPU::Metal::fromAPI(WTF::move(callback))](bool success) {
        callback(mapAsyncStatusToAPI(success));
    });
}

void wgpuBufferUnmap(WGPUBuffer buffer)
{
    protect(WebGPU::Metal::fromAPI(buffer))->unmap();
}

void wgpuBufferGenerateAValidationError(WGPUBuffer buffer)
{
    protect(WebGPU::Metal::fromAPI(buffer))->generateAValidationError("Buffer state was not unmapped"_s);
}

void wgpuBufferSetLabel(WGPUBuffer buffer, WGPUStringView label)
{
    protect(WebGPU::Metal::fromAPI(buffer))->setLabel(WebGPU::Metal::fromAPI(label));
}

WGPUBufferUsage wgpuBufferGetUsage(WGPUBuffer buffer)
{
    return WebGPU::Metal::toAPI(WebGPU::Metal::fromAPI(buffer).usage());
}

void wgpuBufferCopy(WGPUBuffer buffer, std::span<const uint8_t> data, size_t offset)
{
#if ENABLE(WEBGPU_SWIFT)
    protect(WebGPU::Metal::fromAPI(buffer))->copyFrom(data, offset);
#else
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(data);
    UNUSED_PARAM(offset);

#endif
}
