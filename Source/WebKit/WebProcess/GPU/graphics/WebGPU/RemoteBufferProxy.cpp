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
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteBufferProxy);

RemoteBufferProxy::RemoteBufferProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, bool mappedAtCreation)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
    , m_mapModeFlags(mappedAtCreation ? WebCore::WebGPU::MapModeFlags(WebCore::WebGPU::MapMode::Write) : WebCore::WebGPU::MapModeFlags())
{
}

RemoteBufferProxy::~RemoteBufferProxy()
{
    auto sendResult = send(Messages::RemoteBuffer::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteBufferProxy::mapAsync(WebCore::WebGPU::MapModeFlags mapModeFlags, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size, CompletionHandler<void(bool)>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteBuffer::MapAsync(mapModeFlags, offset, size), [callback = WTFMove(callback), mapModeFlags, protectedThis = Ref { *this }](auto success) mutable {
        if (!success)
            return callback(false);
        protectedThis->m_mapModeFlags = mapModeFlags;
        callback(true);
    });
    UNUSED_PARAM(sendResult);
}

static bool offsetOrSizeExceedsBounds(size_t dataSize, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> requestedSize)
{
    return offset >= dataSize || (requestedSize.has_value() && requestedSize.value() + offset > dataSize);
}

void RemoteBufferProxy::getMappedRange(WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size, NOESCAPE const Function<void(std::span<uint8_t>)>& callback)
{
    // FIXME: Implement error handling.
    auto sendResult = sendSync(Messages::RemoteBuffer::GetMappedRange(offset, size));
    auto [data] = sendResult.takeReplyOr(std::nullopt);

    if (!data || !data->span().data() || offsetOrSizeExceedsBounds(data->size(), offset, size)) {
        callback({ });
        return;
    }

    callback(data->mutableSpan().subspan(offset));
}

std::span<uint8_t> RemoteBufferProxy::getBufferContents()
{
    RELEASE_ASSERT_NOT_REACHED();
}

void RemoteBufferProxy::copyFrom(std::span<const uint8_t> span, size_t offset)
{
    if (!m_mapModeFlags.contains(WebCore::WebGPU::MapMode::Write))
        return;

    size_t actualCopySize = span.size() - offset;
    if (actualCopySize > maxCrossProcessResourceCopySize) {
        auto handle = WebCore::SharedMemoryHandle::createCopy(span, WebCore::SharedMemory::Protection::ReadOnly);
        auto sendResult = sendWithAsyncReply(Messages::RemoteBuffer::Copy(WTFMove(handle), offset), [](auto) {
        });
        UNUSED_VARIABLE(sendResult);
    } else {
        auto sendResult = send(Messages::RemoteBuffer::CopyWithCopy(span, offset));
        UNUSED_VARIABLE(sendResult);
    }
}

void RemoteBufferProxy::unmap()
{
    auto sendResult = send(Messages::RemoteBuffer::Unmap());
    UNUSED_VARIABLE(sendResult);

    m_mapModeFlags = { };
}

void RemoteBufferProxy::destroy()
{
    auto sendResult = send(Messages::RemoteBuffer::Destroy());
    UNUSED_VARIABLE(sendResult);
}

void RemoteBufferProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteBuffer::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
