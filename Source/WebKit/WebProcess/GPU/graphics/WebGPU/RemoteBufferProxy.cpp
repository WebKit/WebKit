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

namespace WebKit::WebGPU {

RemoteBufferProxy::RemoteBufferProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteBufferProxy::~RemoteBufferProxy()
{
    auto sendResult = send(Messages::RemoteBuffer::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteBufferProxy::mapAsync(WebCore::WebGPU::MapModeFlags mapModeFlags, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size, CompletionHandler<void(bool)>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteBuffer::MapAsync(mapModeFlags, offset, size), [callback = WTFMove(callback), mapModeFlags, protectedThis = Ref { *this }](auto data) mutable {

        if (!data) {
            // FIXME: Implement error handling.
            callback(false);
            return;
        }

        protectedThis->m_data = WTFMove(data);
        protectedThis->m_mapModeFlags = mapModeFlags;
        callback(true);
    });
    UNUSED_PARAM(sendResult);
}

static bool offsetOrSizeExceedsBounds(size_t dataSize, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> requestedSize)
{
    return offset >= dataSize || (requestedSize.has_value() && requestedSize.value() + offset > dataSize);
}

auto RemoteBufferProxy::getMappedRange(WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size) -> MappedRange
{
    if (m_data.has_value() && m_data->data()) {
        if (offsetOrSizeExceedsBounds(m_data->size(), offset, size))
            return { };

        return { m_data->data() + offset, static_cast<size_t>(size.value_or(m_data->size() - offset)) };
    }

    // FIXME: Implement error handling.
    auto sendResult = sendSync(Messages::RemoteBuffer::GetMappedRange(offset, size));
    auto [data] = sendResult.takeReplyOr(std::nullopt);

    if (!data || !data->data() || offsetOrSizeExceedsBounds(data->size(), offset, size))
        return { };

    m_data = WTFMove(data);
    m_mapModeFlags = { WebCore::WebGPU::MapMode::Write };
    return { m_data->data() + offset, static_cast<size_t>(size.value_or(m_data->size() - offset)) };
}

void RemoteBufferProxy::unmap()
{
    Vector<uint8_t> data;
    if (m_data && m_mapModeFlags.contains(WebCore::WebGPU::MapMode::Write))
        data = WTFMove(*m_data);
    auto sendResult = send(Messages::RemoteBuffer::Unmap(WTFMove(data)));
    UNUSED_VARIABLE(sendResult);

    m_data = std::nullopt;
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
