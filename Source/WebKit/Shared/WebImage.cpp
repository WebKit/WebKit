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

Ref<WebImage> WebImage::createEmpty()
{
    return adoptRef(*new WebImage(nullptr));
}

Ref<WebImage> WebImage::create(const IntSize& size, ImageOptions options, const DestinationColorSpace& colorSpace, ChromeClient* client)
{
    if (client) {
        auto purpose = (options & ImageOptionsShareable) ? RenderingPurpose::ShareableSnapshot : RenderingPurpose::Snapshot;
        purpose = (options & ImageOptionsLocal) ? RenderingPurpose::ShareableLocalSnapshot : purpose;
        
        if (auto buffer = client->createImageBuffer(size, purpose, 1, colorSpace, PixelFormat::BGRA8, { }))
            return WebImage::create(buffer.releaseNonNull());
    }

    if (options & ImageOptionsShareable) {
        auto buffer = ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, 1, colorSpace, PixelFormat::BGRA8, RenderingPurpose::ShareableSnapshot, { });
        if (!buffer)
            return createEmpty();
        return WebImage::create(buffer.releaseNonNull());
    }

    auto buffer = ImageBuffer::create(size, RenderingPurpose::Snapshot, 1, colorSpace, PixelFormat::BGRA8);
    if (!buffer)
        return createEmpty();
    return WebImage::create(buffer.releaseNonNull());
}

Ref<WebImage> WebImage::create(std::optional<ParametersAndHandle>&& parametersAndHandle)
{
    if (!parametersAndHandle)
        return createEmpty();
    auto [parameters, handle] = WTFMove(*parametersAndHandle);

    // FIXME: These should be abstracted as a encodable image buffer handle.
    auto backendParameters = ImageBuffer::backendParameters(parameters);
    auto backend = ImageBufferShareableBitmapBackend::create(backendParameters, WTFMove(handle));
    if (!backend)
        return createEmpty();
    
    auto info = ImageBuffer::populateBackendInfo<ImageBufferShareableBitmapBackend>(backendParameters);

    auto buffer = ImageBuffer::create(WTFMove(parameters), info, { }, WTFMove(backend));
    if (!buffer)
        return createEmpty();

    return WebImage::create(buffer.releaseNonNull());
}

Ref<WebImage> WebImage::create(Ref<ImageBuffer>&& buffer)
{
    return adoptRef(*new WebImage(WTFMove(buffer)));
}

WebImage::WebImage(RefPtr<ImageBuffer>&& buffer)
    : m_buffer(WTFMove(buffer))
{
}

WebImage::~WebImage() = default;

IntSize WebImage::size() const
{
    if (!m_buffer)
        return { };
    return m_buffer->backendSize();
}

const ImageBufferParameters* WebImage::parameters() const
{
    if (!m_buffer)
        return nullptr;
    return &m_buffer->parameters();
}

auto WebImage::parametersAndHandle() const -> std::optional<ParametersAndHandle>
{
    auto handle = createHandle();
    if (!handle)
        return std::nullopt;
    RELEASE_ASSERT(m_buffer);
    return { { m_buffer->parameters(), WTFMove(*handle) } };
}

GraphicsContext* WebImage::context() const
{
    if (!m_buffer)
        return nullptr;
    return &m_buffer->context();
}

RefPtr<NativeImage> WebImage::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    if (!m_buffer)
        return nullptr;
    if (copyBehavior == CopyBackingStore)
        return m_buffer->copyNativeImage();
    return m_buffer->createNativeImageReference();
}

RefPtr<ShareableBitmap> WebImage::bitmap() const
{
    if (!m_buffer)
        return nullptr;
    const_cast<ImageBuffer&>(*m_buffer).flushDrawingContext();

    auto* sharing = m_buffer->toBackendSharing();
    if (!is<ImageBufferBackendHandleSharing>(sharing))
        return nullptr;

    return downcast<ImageBufferBackendHandleSharing>(*sharing).bitmap();
}

#if USE(CAIRO)
RefPtr<cairo_surface_t> WebImage::createCairoSurface()
{
    if (!m_buffer)
        return nullptr;
    return m_buffer->createCairoSurface();
}
#endif

std::optional<ShareableBitmap::Handle> WebImage::createHandle(SharedMemory::Protection protection) const
{
    if (!m_buffer)
        return std::nullopt;
    const_cast<ImageBuffer&>(*m_buffer).flushDrawingContext();

    auto* sharing = m_buffer->toBackendSharing();
    if (!is<ImageBufferBackendHandleSharing>(sharing))
        return std::nullopt;

    auto backendHandle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle(protection);
    if (!backendHandle)
        return std::nullopt;

    return std::get<ShareableBitmap::Handle>(WTFMove(*backendHandle));
}

} // namespace WebKit
