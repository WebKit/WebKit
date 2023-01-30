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
#include "WebGPUCompositorIntegrationImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace PAL::WebGPU {

CompositorIntegrationImpl::CompositorIntegrationImpl(ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
{
}

CompositorIntegrationImpl::~CompositorIntegrationImpl() = default;

#if PLATFORM(COCOA)
Vector<MachSendRight> CompositorIntegrationImpl::getRenderBuffers()
{
    return m_renderBuffers.map([] (const auto& renderBuffer) {
        return MachSendRight::adopt(IOSurfaceCreateMachPort(renderBuffer.get()));
    });
}

static RetainPtr<CFNumberRef> toCFNumber(int x)
{
    return adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &x));
}

Vector<RetainPtr<IOSurfaceRef>> CompositorIntegrationImpl::recreateIOSurfaces(const WGPUSwapChainDescriptor& descriptor)
{
    m_renderBuffers.clear();

    auto createIOSurface = [&]() -> RetainPtr<IOSurfaceRef> {
        unsigned bytesPerElement = 4;
        unsigned bytesPerPixel = 4;

        size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, descriptor.width * bytesPerPixel);
        ASSERT(bytesPerRow);

        size_t totalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, descriptor.height * bytesPerRow);
        ASSERT(totalBytes);

        unsigned pixelFormat = 'BGRA';

        auto options = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 8, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(options.get(), kIOSurfaceWidth, toCFNumber(descriptor.width).get());
        CFDictionaryAddValue(options.get(), kIOSurfaceHeight, toCFNumber(descriptor.height).get());
        CFDictionaryAddValue(options.get(), kIOSurfacePixelFormat, toCFNumber(pixelFormat).get());
        CFDictionaryAddValue(options.get(), kIOSurfaceBytesPerElement, toCFNumber(bytesPerElement).get());
        CFDictionaryAddValue(options.get(), kIOSurfaceBytesPerRow, toCFNumber(bytesPerRow).get());
        CFDictionaryAddValue(options.get(), kIOSurfaceAllocSize, toCFNumber(totalBytes).get());
#if PLATFORM(IOS_FAMILY)
        CFDictionaryAddValue(options.get(), kIOSurfaceCacheMode, toCFNumber(kIOMapWriteCombineCache).get());
#endif
        CFDictionaryAddValue(options.get(), kIOSurfaceElementHeight, toCFNumber(1).get());

        return adoptCF(IOSurfaceCreate(options.get()));
    };

    m_renderBuffers.append(createIOSurface());
    m_renderBuffers.append(createIOSurface());

    return m_renderBuffers;
}
#endif

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
