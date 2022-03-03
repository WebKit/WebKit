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

#import "Device.h"

namespace WebGPU {

RefPtr<Buffer> Device::createBuffer(const WGPUBufferDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return Buffer::create(nil);
}

Buffer::Buffer(id<MTLBuffer> buffer)
    : m_buffer(buffer)
{
}

Buffer::~Buffer() = default;

void Buffer::destroy()
{
}

const void* Buffer::getConstMappedRange(size_t offset, size_t size)
{
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
    return nullptr;
}

void* Buffer::getMappedRange(size_t offset, size_t size)
{
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
    return nullptr;
}

void Buffer::mapAsync(WGPUMapModeFlags mode, size_t offset, size_t size, WTF::Function<void(WGPUBufferMapAsyncStatus)>&& callback)
{
    UNUSED_PARAM(mode);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
    UNUSED_PARAM(callback);
}

void Buffer::unmap()
{
}

void Buffer::setLabel(const char* label)
{
    m_buffer.label = [NSString stringWithCString:label encoding:NSUTF8StringEncoding];
}

} // namespace WebGPU

void wgpuBufferRelease(WGPUBuffer buffer)
{
    delete buffer;
}

void wgpuBufferDestroy(WGPUBuffer buffer)
{
    buffer->buffer->destroy();
}

const void* wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    return buffer->buffer->getConstMappedRange(offset, size);
}

void* wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    return buffer->buffer->getMappedRange(offset, size);
}

void wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapCallback callback, void* userdata)
{
    buffer->buffer->mapAsync(mode, offset, size, [callback, userdata] (WGPUBufferMapAsyncStatus status) {
        callback(status, userdata);
    });
}

void wgpuBufferMapAsyncWithBlock(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback)
{
    buffer->buffer->mapAsync(mode, offset, size, [callback] (WGPUBufferMapAsyncStatus status) {
        callback(status);
    });
}

void wgpuBufferUnmap(WGPUBuffer buffer)
{
    buffer->buffer->unmap();
}

void wgpuBufferSetLabel(WGPUBuffer buffer, const char* label)
{
    buffer->buffer->setLabel(label);
}
