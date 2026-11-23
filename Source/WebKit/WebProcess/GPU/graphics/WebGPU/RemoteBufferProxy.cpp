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
#include "RemoteBufferProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBufferMessages.h"
#include <wtf/Borrow.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteBufferProxy);

RemoteBufferProxy::RemoteBufferProxy(RemoteDeviceProxy& parent, WebGPUBufferIdentifier identifier, const ::WebGPU::BufferDescriptor& descriptor)
    : m_backing(identifier)
    , m_parent(parent)
    , m_size(descriptor.size)
    , m_usage(descriptor.usage)
    , m_mapModeFlags(descriptor.mappedAtCreation ? OptionSet<::WebGPU::MapMode>(::WebGPU::MapMode::Write) : OptionSet<::WebGPU::MapMode>())
{
}

RemoteBufferProxy::~RemoteBufferProxy()
{
    auto sendResult = send(Messages::RemoteBuffer::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteBufferProxy::mapAsync(OptionSet<::WebGPU::MapMode> mapModeFlags, uint64_t offset, std::optional<uint64_t> size, CompletionHandler<void(bool)>&& callback)
{
    // The GPU Process fails the call for a buffer that is not unmapped, possibly before it completes
    // the pending one, so the reply leaves such a buffer as it is.
    bool startsMapping = mapState() == ::WebGPU::BufferMapState::Unmapped;
    if (startsMapping)
        m_isMapPending = true;
    auto mapGeneration = m_mapGeneration;
    auto sendResult = sendWithAsyncReply(Messages::RemoteBuffer::MapAsync(mapModeFlags, offset, size), [callback = WTF::move(callback), mapModeFlags, offset, size, startsMapping, mapGeneration, protectedThis = protect(*this)](auto success) mutable {
        if (!startsMapping || mapGeneration != protectedThis->m_mapGeneration)
            return callback(success);
        protectedThis->m_isMapPending = false;
        if (!success)
            return callback(false);
        protectedThis->m_mapModeFlags = mapModeFlags;
        protectedThis->m_mappedOffset = offset;
        protectedThis->m_mappedSize = size;
        callback(true);
    });
    if (!sendResult && startsMapping)
        m_isMapPending = false;
}

::WebGPU::BufferMapState RemoteBufferProxy::mapState() const
{
    if (m_isMapPending)
        return ::WebGPU::BufferMapState::Pending;
    if (!m_mapModeFlags.isEmpty())
        return ::WebGPU::BufferMapState::Mapped;
    return ::WebGPU::BufferMapState::Unmapped;
}

std::span<uint8_t> RemoteBufferProxy::persistentMappedRange(uint64_t offset, std::optional<uint64_t> size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-getmappedrange, without the check that the
    // ranges don't overlap. The GPU Process kills the Web Process that asks for the range of a buffer
    // that isn't mapped.
    if (m_mapModeFlags.isEmpty())
        return { };

    auto rangeSize = size.value_or(offset <= m_size ? m_size - offset : 0);
    auto mappedSize = m_mappedSize.value_or(m_mappedOffset <= m_size ? m_size - m_mappedOffset : 0);
    if (offset % 8 || rangeSize % 4 || offset < m_mappedOffset || offset - m_mappedOffset > mappedSize || rangeSize > mappedSize - (offset - m_mappedOffset))
        return { };

    if (!m_persistentMappedRange) {
        auto sendResult = sendSync(Messages::RemoteBuffer::GetMappedRange(m_mappedOffset, m_mappedSize));
        auto [data] = sendResult.takeReplyOr(std::nullopt);
        // The GPU Process returns the rest of the buffer after the offset.
        if (!data || data->size() < mappedSize)
            return { };
        data->shrink(static_cast<size_t>(mappedSize));
        m_persistentMappedRange = WTF::move(*data);
    }

    return m_persistentMappedRange->mutableSpan().subspan(static_cast<size_t>(offset - m_mappedOffset), static_cast<size_t>(rangeSize));
}

NODELETE static bool offsetOrSizeExceedsBounds(size_t dataSize, uint64_t offset, std::optional<uint64_t> requestedSize)
{
    return offset >= dataSize || (requestedSize.has_value() && requestedSize.value() + offset > dataSize);
}

void RemoteBufferProxy::getMappedRange(uint64_t offset, std::optional<uint64_t> size, NOESCAPE const Function<void(std::span<uint8_t>)>& callback)
{
    // FIXME: Implement error handling.
    auto sendResult = sendSync(Messages::RemoteBuffer::GetMappedRange(offset, size));
    auto [data] = sendResult.takeReplyOr(std::nullopt);

    offset = 0;
    if (!data || !data->span().data() || offsetOrSizeExceedsBounds(data->size(), offset, size)) {
        callback({ });
        return;
    }

    callback(borrow(*data)->mutableSpan().subspan(offset));
}

std::span<uint8_t> RemoteBufferProxy::getBufferContents()
{
    RELEASE_ASSERT_NOT_REACHED();
}

void RemoteBufferProxy::copyFrom(std::span<const uint8_t> span, size_t offset)
{
    if (!m_mapModeFlags.contains(::WebGPU::MapMode::Write))
        return;

    if (span.size() > maxCrossProcessResourceCopySize) {
        auto handle = WebCore::SharedMemoryHandle::createCopy(span, WebCore::SharedMemory::Protection::ReadOnly);
        auto sendResult = sendWithAsyncReply(Messages::RemoteBuffer::Copy(WTF::move(handle), offset), [](auto) {
        });
        UNUSED_VARIABLE(sendResult);
    } else {
        auto sendResult = send(Messages::RemoteBuffer::CopyWithCopy(span, offset));
        UNUSED_VARIABLE(sendResult);
    }
}

void RemoteBufferProxy::unmap()
{
    // copyFrom() only writes back a mapping for writing.
    if (auto range = std::exchange(m_persistentMappedRange, std::nullopt))
        copyFrom(borrow(*range)->span(), m_mappedOffset);

    auto sendResult = send(Messages::RemoteBuffer::Unmap());
    UNUSED_VARIABLE(sendResult);

    m_mapModeFlags = { };
    m_isMapPending = false;
    ++m_mapGeneration;
    m_mappedOffset = 0;
    m_mappedSize = std::nullopt;
}

void RemoteBufferProxy::destroy()
{
    auto sendResult = send(Messages::RemoteBuffer::Destroy());
    UNUSED_VARIABLE(sendResult);

    // https://gpuweb.github.io/gpuweb/#dom-gpubuffer-destroy unmaps the buffer, without writing
    // back its contents. The JavaScript API unmaps it before.
    m_persistentMappedRange = std::nullopt;
    m_mapModeFlags = { };
    m_isMapPending = false;
    ++m_mapGeneration;
    m_mappedOffset = 0;
    m_mappedSize = std::nullopt;
}

void RemoteBufferProxy::setLabel(String&& label)
{
    auto sendResult = send(Messages::RemoteBuffer::SetLabel(WTF::move(label)));
    UNUSED_VARIABLE(sendResult);
}

bool RemoteBufferProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
