/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "WebGPUBufferImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include <WebGPU/WebGPUExt.h>

namespace PAL::WebGPU {

BufferImpl::BufferImpl(WGPUBuffer buffer, ConvertToBackingContext& convertToBackingContext)
    : m_backing(buffer)
    , m_convertToBackingContext(convertToBackingContext)
{
}

BufferImpl::~BufferImpl()
{
    wgpuBufferRelease(m_backing);
}

void mapCallback(WGPUBufferMapAsyncStatus status, void* userdata)
{
    auto buffer = adoptRef(*static_cast<BufferImpl*>(userdata)); // adoptRef is balanced by leakRef in mapAsync() below. We have to do this because we're using a C API with no concept of reference counting or blocks.
    buffer->mapCallback(status);
}

void BufferImpl::mapAsync(MapModeFlags mapModeFlags, Size64 offset, std::optional<Size64> size, WTF::Function<void()>&& callback)
{
    Ref protectedThis(*this);

    auto backingMapModeFlags = m_convertToBackingContext->convertMapModeFlagsToBacking(mapModeFlags);

    auto usedSize = size.value_or(WGPU_WHOLE_MAP_SIZE);

    m_callbacks.append(WTFMove(callback));

    // FIXME: Check the casts.
    wgpuBufferMapAsync(m_backing, backingMapModeFlags, static_cast<size_t>(offset), static_cast<size_t>(usedSize), &WebGPU::mapCallback, &protectedThis.leakRef()); // leakRef is balanced by adoptRef in mapCallback() above. We have to do this because we're using a C API with no concept of reference counting or blocks.
}

void BufferImpl::mapCallback(WGPUBufferMapAsyncStatus status)
{
    UNUSED_PARAM(status);
    auto callback = m_callbacks.takeFirst();
    callback();
}

auto BufferImpl::getMappedRange(Size64 offset, std::optional<Size64> size) -> MappedRange
{
    auto usedSize = size.value_or(WGPU_WHOLE_MAP_SIZE);
    // FIXME: Check the casts.
    auto* pointer = wgpuBufferGetMappedRange(m_backing, static_cast<size_t>(offset), static_cast<size_t>(usedSize));
    // FIXME: Check the type narrowing.
    return { pointer, static_cast<size_t>(usedSize) };
}

void BufferImpl::unmap()
{
    wgpuBufferUnmap(m_backing);
}

void BufferImpl::destroy()
{
    wgpuBufferDestroy(m_backing);
}

void BufferImpl::setLabelInternal(const String& label)
{
    wgpuBufferSetLabel(m_backing, label.utf8().data());
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
