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
#include <WebCore/NativeImage.h>

namespace WebKit {
using namespace WebCore;

Ref<WebImage> WebImage::createEmpty()
{
    return adoptRef(*new WebImage(nullptr));
}

Ref<WebImage> WebImage::create(const IntSize& size, ImageOptions options, const DestinationColorSpace& colorSpace, ChromeClient* client)
{
    ImageBufferPixelFormat pixelFormat = ImageBufferPixelFormat::BGRA8;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (options.contains(ImageOption::AllowHDR) && colorSpace.usesExtendedRange())
        pixelFormat = ImageBufferPixelFormat::RGBA16F;
#endif

    if (client) {
        auto purpose = options.contains(ImageOption::Shareable) ? RenderingPurpose::ShareableSnapshot : RenderingPurpose::Snapshot;
        purpose = options.contains(ImageOption::Local) ? RenderingPurpose::ShareableLocalSnapshot : purpose;
        auto accelerated = options.contains(ImageOption::Accelerated) ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;

        if (auto buffer = client->createImageBuffer(size, accelerated, purpose, 1, colorSpace, pixelFormat))
            return WebImage::create(buffer.releaseNonNull());
    }

    if (options.contains(ImageOption::Shareable)) {
        auto buffer = ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, 1, colorSpace, pixelFormat, RenderingPurpose::ShareableSnapshot, { });
        if (!buffer)
            return createEmpty();
        return WebImage::create(buffer.releaseNonNull());
    }

    auto buffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Snapshot, 1, colorSpace, pixelFormat);
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
    RefPtr buffer = m_buffer;
    return buffer ? buffer->backendSize() : IntSize();
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
    RefPtr buffer = m_buffer;
    return buffer ? &buffer->context() : nullptr;
}

RefPtr<NativeImage> WebImage::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    RefPtr buffer = m_buffer;
    if (!buffer)
        return nullptr;
    if (copyBehavior == CopyBackingStore)
        return buffer->copyNativeImage();
    return buffer->createNativeImageReference();
}

RefPtr<ShareableBitmap> WebImage::bitmap() const
{
    RefPtr buffer = m_buffer;
    if (!buffer)
        return nullptr;
    buffer->flushDrawingContext();

    auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(buffer->toBackendSharing());
    return sharing ? sharing->bitmap() : nullptr;
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
    auto backendHandle = createImageBufferBackendHandle(protection);
    if (!backendHandle || !std::holds_alternative<ShareableBitmap::Handle>(*backendHandle))
        return { };

    return std::get<ShareableBitmap::Handle>(WTFMove(*backendHandle));
}

std::optional<ImageBufferBackendHandle> WebImage::createImageBufferBackendHandle(SharedMemory::Protection protection) const
{
    RefPtr buffer = m_buffer;
    if (!buffer)
        return { };
    buffer->flushDrawingContext();

    auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(buffer->toBackendSharing());
    if (!sharing)
        return { };

    return sharing->createBackendHandle(protection);
}

} // namespace WebKit
