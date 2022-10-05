/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "WebImage.h"

#include "ImageBufferShareableBitmapBackend.h"
#include <WebCore/ChromeClient.h>
#include <WebCore/ImageBuffer.h>

namespace WebKit {
using namespace WebCore;

RefPtr<WebImage> WebImage::create(const IntSize& size, ImageOptions options, const DestinationColorSpace& colorSpace, ChromeClient* client)
{
    if (client) {
        auto purpose = (options & ImageOptionsShareable) ? RenderingPurpose::ShareableSnapshot : RenderingPurpose::Snapshot;
        auto buffer = client->createImageBuffer(size, RenderingMode::Unaccelerated, purpose, 1, colorSpace, PixelFormat::BGRA8);
        if (buffer)
            return WebImage::create(buffer.releaseNonNull());
    }

    if (options & ImageOptionsShareable) {
        auto buffer = ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, 1, colorSpace, PixelFormat::BGRA8, RenderingPurpose::ShareableSnapshot, { });
        if (!buffer)
            return nullptr;
        return WebImage::create(buffer.releaseNonNull());
    }

    auto buffer = ImageBuffer::create(size, RenderingPurpose::Snapshot, 1, colorSpace, PixelFormat::BGRA8);
    if (!buffer)
        return nullptr;
    return WebImage::create(buffer.releaseNonNull());
}

RefPtr<WebImage> WebImage::create(const ImageBufferBackend::Parameters& parameters, ShareableBitmapHandle&& handle)
{
    auto backend = ImageBufferShareableBitmapBackend::create(parameters, WTFMove(handle));
    if (!backend)
        return nullptr;
    
    auto info = ImageBuffer::populateBackendInfo<ImageBufferShareableBitmapBackend>(parameters);

    auto buffer = ImageBuffer::create(parameters, info, WTFMove(backend));
    if (!buffer)
        return nullptr;

    return WebImage::create(buffer.releaseNonNull());
}

Ref<WebImage> WebImage::create(Ref<ImageBuffer>&& buffer)
{
    return adoptRef(*new WebImage(WTFMove(buffer)));
}

WebImage::WebImage(Ref<ImageBuffer>&& buffer)
    : m_buffer(WTFMove(buffer))
{
}

IntSize WebImage::size() const
{
    return m_buffer->backendSize();
}

const ImageBufferBackend::Parameters& WebImage::parameters() const
{
    return m_buffer->parameters();
}

GraphicsContext& WebImage::context() const
{
    return m_buffer->context();
}

RefPtr<NativeImage> WebImage::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    return m_buffer->copyNativeImage(copyBehavior);
}

RefPtr<ShareableBitmap> WebImage::bitmap() const
{
    auto* backend = m_buffer->ensureBackendCreated();
    if (!backend)
        return nullptr;

    const_cast<ImageBuffer&>(*m_buffer.ptr()).flushDrawingContext();

    auto* sharing = backend->toBackendSharing();
    if (!is<ImageBufferBackendHandleSharing>(sharing))
        return nullptr;

    return downcast<ImageBufferBackendHandleSharing>(*sharing).bitmap();
}

#if USE(CAIRO)
RefPtr<cairo_surface_t> WebImage::createCairoSurface()
{
    return m_buffer->createCairoSurface();
}
#endif

ShareableBitmapHandle WebImage::createHandle(SharedMemory::Protection protection) const
{
    auto* backend = m_buffer->ensureBackendCreated();
    if (!backend)
        return { };

    const_cast<ImageBuffer&>(*m_buffer.ptr()).flushDrawingContext();

    auto* sharing = backend->toBackendSharing();
    if (!is<ImageBufferBackendHandleSharing>(sharing))
        return { };

    auto backendHandle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle(protection);

    if (auto handle = std::get_if<ShareableBitmapHandle>(&backendHandle))
        return WTFMove(*handle);

    return { };
}

} // namespace WebKit
