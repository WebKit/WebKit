/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ImageBufferShareableAllocator.h"

#include "ImageBufferShareableBitmapBackend.h"
#include "ShareablePixelBuffer.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <wtf/TZoneMallocInlines.h>

#if HAVE(IOSURFACE)
#include <WebCore/ImageBufferIOSurfaceBackend.h>
#endif

#if ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageBufferShareableAllocator);

#if HAVE(IOSURFACE)
ImageBufferShareableAllocator::ImageBufferShareableAllocator(const ProcessIdentity& resourceOwner, IOSurfacePool* ioSurfacePool)
    : m_resourceOwner(resourceOwner)
    , m_ioSurfacePool(ioSurfacePool)
{
}
#else
ImageBufferShareableAllocator::ImageBufferShareableAllocator(const ProcessIdentity& resourceOwner)
    : m_resourceOwner(resourceOwner)
{
}
#endif

RefPtr<ImageBuffer> ImageBufferShareableAllocator::createImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, RenderingMode renderingMode) const
{
#if HAVE(IOSURFACE)
    if (renderingMode == RenderingMode::Accelerated) {
        ImageBufferCreationContext creationContext;
        creationContext.resourceOwner = m_resourceOwner;
        if (m_ioSurfacePool)
            creationContext.surfacePool = m_ioSurfacePool.get();

        return ImageBuffer::create<ImageBufferIOSurfaceBackend>(size, 1, colorSpace, { PixelFormat::BGRA8 }, RenderingPurpose::Unspecified, creationContext);
    }
#endif

    RefPtr<ImageBuffer> imageBuffer = ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, 1, colorSpace, { PixelFormat::BGRA8 }, RenderingPurpose::Unspecified, { });
    if (!imageBuffer)
        return nullptr;

    auto* sharing = imageBuffer->toBackendSharing();
    ASSERT(is<ImageBufferBackendHandleSharing>(sharing));

    auto bitmap = downcast<ImageBufferBackendHandleSharing>(*sharing).bitmap();
    if (!bitmap)
        return nullptr;

    auto handle = bitmap->createHandle();
    if (!handle)
        return nullptr;

    transferMemoryOwnership(WTF::move(handle->handle()));
    return imageBuffer;
}

RefPtr<PixelBuffer> ImageBufferShareableAllocator::createPixelBuffer(const PixelBufferFormat& format, const IntSize& size) const
{
    RefPtr pixelBuffer = ShareablePixelBuffer::tryCreate(format, size);
    if (!pixelBuffer)
        return nullptr;

    auto handle = protect(pixelBuffer->data())->createHandle(SharedMemory::Protection::ReadOnly);
    if (!handle)
        return nullptr;

    transferMemoryOwnership(WTF::move(*handle));
    return pixelBuffer;
}

void ImageBufferShareableAllocator::transferMemoryOwnership(SharedMemory::Handle&& handle) const
{
    handle.setOwnershipOfMemory(m_resourceOwner, MemoryLedger::Graphics);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
