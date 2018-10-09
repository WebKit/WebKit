/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "GPUBuffer.h"

#if ENABLE(WEBMETAL)

#import "GPUDevice.h"
#import "Logging.h"
#import <JavaScriptCore/ArrayBufferView.h>
#import <Metal/Metal.h>
#import <wtf/Gigacage.h>
#import <wtf/PageBlock.h>

namespace WebCore {

GPUBuffer::GPUBuffer(const GPUDevice& device, const JSC::ArrayBufferView& data)
{
    LOG(WebMetal, "GPUBuffer::GPUBuffer()");

    if (!device.metal())
        return;
    
    size_t pageSize = WTF::pageSize();
    size_t pageAlignedSize = roundUpToMultipleOf(pageSize, data.byteLength());
    void* pageAlignedCopy = Gigacage::tryAlignedMalloc(Gigacage::Primitive, pageSize, pageAlignedSize);
    if (!pageAlignedCopy)
        return;
    memcpy(pageAlignedCopy, data.baseAddress(), data.byteLength());
    m_contents = ArrayBuffer::createFromBytes(pageAlignedCopy, data.byteLength(), [] (void* ptr) {
        Gigacage::alignedFree(Gigacage::Primitive, ptr);
    });
    m_contents->ref();
    ArrayBuffer* capturedContents = m_contents.get();
    m_metal = adoptNS([device.metal() newBufferWithBytesNoCopy:m_contents->data() length:pageAlignedSize options:MTLResourceOptionCPUCacheModeDefault deallocator:^(void*, NSUInteger) {
        capturedContents->deref();
    }]);
    if (!m_metal) {
        m_contents->deref();
        m_contents = nullptr;
    }
}

} // namespace WebCore

#endif
