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
#include <WebCore/ImageBuffer.h>

#if ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

ImageBufferShareableAllocator::ImageBufferShareableAllocator(const ProcessIdentity& resourceOwner)
    : m_resourceOwner(resourceOwner)
{
}

RefPtr<ImageBuffer> ImageBufferShareableAllocator::createImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, RenderingMode) const
{
    RefPtr<ImageBuffer> imageBuffer = ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, 1, colorSpace, PixelFormat::BGRA8, RenderingPurpose::Unspecified, { });
    if (!imageBuffer)
        return nullptr;

    auto* backend = imageBuffer->backend();
    if (!backend)
        return nullptr;

    auto* sharing = backend->toBackendSharing();
    ASSERT(is<ImageBufferBackendHandleSharing>(sharing));

    auto bitmap = downcast<ImageBufferBackendHandleSharing>(*sharing).bitmap();
    if (!bitmap)
        return nullptr;

    ShareableBitmapHandle handle;
    if (!bitmap->createHandle(handle))
        return nullptr;

    transferMemoryOwnership(WTFMove(handle.handle()));
    return imageBuffer;
}

RefPtr<PixelBuffer> ImageBufferShareableAllocator::createPixelBuffer(const PixelBufferFormat& format, const IntSize& size) const
{
    auto pixelBuffer = ShareablePixelBuffer::tryCreate(format, size);
    if (!pixelBuffer)
        return nullptr;

    SharedMemory::Handle handle;
    if (!pixelBuffer->data().createHandle(handle, SharedMemory::Protection::ReadOnly))
        return nullptr;

    transferMemoryOwnership(WTFMove(handle));
    return pixelBuffer;
}

void ImageBufferShareableAllocator::transferMemoryOwnership(SharedMemory::Handle&& handle) const
{
    handle.setOwnershipOfMemory(m_resourceOwner, MemoryLedger::Graphics);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
