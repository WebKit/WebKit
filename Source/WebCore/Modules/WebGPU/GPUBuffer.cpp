/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "GPUDevice.h"
#include "JSDOMPromiseDeferred.h"
#include "JSGPUBufferMapState.h"

namespace WebCore {

GPUBuffer::~GPUBuffer()
{
    m_bufferSize = 0;
    m_backing->destroy();
    m_arrayBuffer = nullptr;
}

GPUBuffer::GPUBuffer(Ref<WebGPU::Buffer>&& backing, size_t bufferSize, GPUBufferUsageFlags usage, bool mappedAtCreation, GPUDevice& device)
    : m_backing(WTFMove(backing))
    , m_bufferSize(bufferSize)
    , m_usage(usage)
    , m_mapState(mappedAtCreation ? GPUBufferMapState::Mapped : GPUBufferMapState::Unmapped)
    , m_device(device)
    , m_mappedAtCreation(mappedAtCreation)
{
    if (mappedAtCreation)
        m_mappedRangeSize = m_bufferSize;
}

String GPUBuffer::label() const
{
    return m_backing->label();
}

void GPUBuffer::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

void GPUBuffer::mapAsync(GPUMapModeFlags mode, std::optional<GPUSize64> offset, std::optional<GPUSize64> size, MapAsyncPromise&& promise)
{
    if (m_pendingMapPromise) {
        promise.reject(Exception { ExceptionCode::OperationError, "pendingMapPromise"_s });
        return;
    }

    if (m_mapState == GPUBufferMapState::Unmapped)
        m_mapState = GPUBufferMapState::Pending;

    m_pendingMapPromise = promise;
    // FIXME: Should this capture a weak pointer to |this| instead?
    m_backing->mapAsync(convertMapModeFlagsToBacking(mode), offset.value_or(0), size, [promise = WTFMove(promise), protectedThis = Ref { *this }, offset, size](bool success) mutable {
        if (!protectedThis->m_pendingMapPromise) {
            if (protectedThis->m_destroyed)
                promise.reject(Exception { ExceptionCode::OperationError, "buffer destroyed during mapAsync"_s });
            else
                promise.resolve(nullptr);
            return;
        }

        protectedThis->m_pendingMapPromise = std::nullopt;
        if (success) {
            protectedThis->m_mapState = GPUBufferMapState::Mapped;
            protectedThis->m_mappedRangeOffset = offset.value_or(0);
            protectedThis->m_mappedRangeSize = size.value_or(protectedThis->m_bufferSize - protectedThis->m_mappedRangeOffset);
            promise.resolve(nullptr);
        } else {
            if (protectedThis->m_mapState == GPUBufferMapState::Pending)
                protectedThis->m_mapState = GPUBufferMapState::Unmapped;

            promise.reject(Exception { ExceptionCode::OperationError, "map async was not successful"_s });
        }
    });
}

static auto makeArrayBuffer(auto source, auto byteLength, auto& cachedArrayBuffer, auto& device, auto& buffer)
{
    auto arrayBuffer = ArrayBuffer::create(source, byteLength);
    cachedArrayBuffer = arrayBuffer.ptr();
    cachedArrayBuffer->pin();
    device.addBufferToUnmap(buffer);
    return arrayBuffer;
}

static bool containsRange(size_t offset, size_t endOffset, const auto& mappedRanges, const auto& mappedPoints)
{
    if (offset == endOffset) {
        if (mappedPoints.contains(offset))
            return true;

        for (auto& range : mappedRanges) {
            if (range.begin() < offset && offset < range.end())
                return true;
        }
        return false;
    }

    if (mappedRanges.overlaps({ offset, endOffset }))
        return true;

    for (auto& i : mappedPoints) {
        if (offset < i && i < endOffset)
            return true;
    }

    return false;
}

ExceptionOr<Ref<JSC::ArrayBuffer>> GPUBuffer::getMappedRange(std::optional<GPUSize64> optionalOffset, std::optional<GPUSize64> optionalSize)
{
    if (m_mapState != GPUBufferMapState::Mapped || m_destroyed)
        return Exception { ExceptionCode::OperationError, "not mapped or destroyed"_s };

    auto offset = optionalOffset.value_or(0);
    if (offset > m_bufferSize)
        return Exception { ExceptionCode::OperationError, "offset > bufferSize"_s };

    auto size = optionalSize.value_or(m_bufferSize - offset);
    auto checkedEndOffset = checkedSum<uint64_t>(offset, size);
    if (checkedEndOffset.hasOverflowed())
        return Exception { ExceptionCode::OperationError, "has overflowed"_s };

    auto endOffset = checkedEndOffset.value();
    if (offset % 8)
        return Exception { ExceptionCode::OperationError, "validation failed offset % 8"_s };

    if (size % 4)
        return Exception { ExceptionCode::OperationError, "validation failed size % 4"_s };

    if (offset < m_mappedRangeOffset)
        return Exception { ExceptionCode::OperationError, "validation failed offset < m_mappedRangeOffset"_s };

    if (endOffset > m_mappedRangeSize + m_mappedRangeOffset)
        return Exception { ExceptionCode::OperationError, "getMappedRangeFailed because offset + size > mappedRangeSize + mappedRangeOffset"_s };

    if (endOffset > m_bufferSize)
        return Exception { ExceptionCode::OperationError, "validation failed endOffset > bufferSie"_s };

    if (containsRange(offset, endOffset, m_mappedRanges, m_mappedPoints))
        return Exception { ExceptionCode::OperationError, "validation failed - containsRange"_s };

    if (offset == endOffset)
        m_mappedPoints.add(offset);
    else {
        m_mappedRanges.add({ static_cast<size_t>(offset), static_cast<size_t>(endOffset) });
        m_mappedRanges.compact();
    }

    m_mappedRange = m_backing->getMappedRange(offset, size);
    if (!m_mappedRange.source) {
        m_arrayBuffer = nullptr;
        if (m_mappedAtCreation || !size)
            return makeArrayBuffer(static_cast<size_t>(0U), 1, m_arrayBuffer, m_device, *this);

        return Exception { ExceptionCode::OperationError, "getMappedRange failed"_s };
    }

    return makeArrayBuffer(m_mappedRange.source, m_mappedRange.byteLength, m_arrayBuffer, m_device, *this);
}

void GPUBuffer::unmap(ScriptExecutionContext& scriptExecutionContext)
{
    internalUnmap(scriptExecutionContext);
    m_device.removeBufferToUnmap(*this);
}

void GPUBuffer::internalUnmap(ScriptExecutionContext& scriptExecutionContext)
{
    m_mappedAtCreation = false;
    m_mappedRangeOffset = 0;
    m_mappedRangeSize = 0;
    m_mappedRanges.clear();
    m_mappedPoints.clear();
    if (m_pendingMapPromise) {
        m_pendingMapPromise->reject(Exception { ExceptionCode::AbortError });
        m_pendingMapPromise = std::nullopt;
    }

    m_mapState = GPUBufferMapState::Unmapped;

    if (m_arrayBuffer && m_arrayBuffer->data() && m_mappedRange.byteLength) {
        memcpy(m_mappedRange.source, m_arrayBuffer->data(), m_mappedRange.byteLength);
        JSC::ArrayBufferContents emptyBuffer;
        m_arrayBuffer->unpin();
        m_arrayBuffer->transferTo(scriptExecutionContext.vm(), emptyBuffer);
    }

    m_arrayBuffer = nullptr;
    m_backing->unmap();
    m_mappedRange = { nullptr, 0 };
}

void GPUBuffer::destroy(ScriptExecutionContext& scriptExecutionContext)
{
    m_destroyed = true;
    internalUnmap(scriptExecutionContext);
    m_bufferSize = 0;
    m_backing->destroy();
}

}
