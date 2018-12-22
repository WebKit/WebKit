/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#import "GPUBufferDescriptor.h"
#import "GPUDevice.h"
#import "Logging.h"

#import <Foundation/NSRange.h>
#import <JavaScriptCore/ArrayBuffer.h>
#import <Metal/Metal.h>
#import <wtf/Gigacage.h>
#import <wtf/PageBlock.h>

namespace WebCore {

RefPtr<GPUBuffer> GPUBuffer::create(const GPUDevice& device, GPUBufferDescriptor&& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUBuffer::create(): Invalid GPUDevice!");
        return nullptr;
    }

    size_t pageSize = WTF::pageSize();
    size_t pageAlignedSize = roundUpToMultipleOf(pageSize, descriptor.size);
    void* pageAlignedCopy = Gigacage::tryAlignedMalloc(Gigacage::Primitive, pageSize, pageAlignedSize);
    if (!pageAlignedCopy) {
        LOG(WebGPU, "GPUBuffer::create(): Unable to allocate memory!");
        return nullptr;
    }

    auto arrayBuffer = ArrayBuffer::createFromBytes(pageAlignedCopy, descriptor.size, [] (void* ptr) {
        Gigacage::alignedFree(Gigacage::Primitive, ptr);
    });
    arrayBuffer->ref();
    ArrayBuffer* arrayBufferContents = arrayBuffer.ptr();
    // FIXME: Default this MTLResourceOptions.
    PlatformBufferSmartPtr mtlBuffer = adoptNS([device.platformDevice()
        newBufferWithBytesNoCopy:arrayBuffer->data()
        length:pageAlignedSize
        options:MTLResourceCPUCacheModeDefaultCache
        deallocator:^(void*, NSUInteger) {
            arrayBufferContents->deref();
        }]);

    if (!mtlBuffer) {
        LOG(WebGPU, "GPUBuffer::create(): Unable to create MTLBuffer!");
        arrayBuffer->deref();
        return nullptr;
    }

    return adoptRef(*new GPUBuffer(WTFMove(mtlBuffer), WTFMove(arrayBuffer)));
}

GPUBuffer::GPUBuffer(PlatformBufferSmartPtr&& platformBuffer, RefPtr<ArrayBuffer>&& arrayBuffer)
    : m_platformBuffer(WTFMove(platformBuffer))
    , m_mapping(WTFMove(arrayBuffer))
{
}

GPUBuffer::~GPUBuffer()
{
    if (m_mapping) {
        m_mapping->deref();
        m_mapping = nullptr;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
