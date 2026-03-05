/*
 * Copyright (C) 2024, 2025 Igalia S.L.
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
#include "CoordinatedPlatformLayerBufferNativeImage.h"

#if USE(COORDINATED_GRAPHICS)
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "NativeImage.h"
#include "TextureMapper.h"
#include <wtf/MainThread.h>

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(SKIA)
#include "GLContext.h"
#include "PlatformDisplay.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/core/SkPixmap.h> // NOLINT
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferNativeImage> CoordinatedPlatformLayerBufferNativeImage::create(Ref<NativeImage>&& nativeImage, std::unique_ptr<GLFence>&& fence)
{
    OptionSet<TextureMapperFlags> flags;
    if (nativeImage->hasAlpha())
        flags.add(TextureMapperFlags::ShouldBlend);
    return makeUnique<CoordinatedPlatformLayerBufferNativeImage>(WTF::move(nativeImage), flags, WTF::move(fence));
}

CoordinatedPlatformLayerBufferNativeImage::CoordinatedPlatformLayerBufferNativeImage(Ref<NativeImage>&& nativeImage, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::NativeImage, nativeImage->size(), flags, WTF::move(fence))
    , m_image(WTF::move(nativeImage))
{
#if USE(SKIA)
    const auto& image = m_image->platformImage();
    if (!image->isTextureBacked())
        return;

    auto& display = PlatformDisplay::sharedDisplay();
    if (!display.skiaGLContext()->makeContextCurrent())
        return;

    auto* grContext = m_image->grContext();
    RELEASE_ASSERT(grContext);
    grContext->flushAndSubmit(GLFence::isSupported(display.glDisplay()) ? GrSyncCpu::kNo : GrSyncCpu::kYes);

    unsigned textureID = 0;
    GrBackendTexture backendTexture;
    if (SkImages::GetBackendTextureFromImage(image, &backendTexture, false)) {
        GrGLTextureInfo textureInfo;
        if (GrBackendTextures::GetGLTextureInfo(backendTexture, &textureInfo))
            textureID = textureInfo.fID;
    }
    if (!textureID)
        return;

    m_buffer = CoordinatedPlatformLayerBufferRGB::create(textureID, m_image->size(), m_flags, GLFence::create(display.glDisplay()));
#endif
}

CoordinatedPlatformLayerBufferNativeImage::~CoordinatedPlatformLayerBufferNativeImage()
{
#if USE(SKIA)
    // GPU-backed NativeImages must be destroyed on the main thread where the
    // Skia GrDirectContext was created, not on the compositor thread. Releasing
    // GPU resources on the wrong thread corrupts Skia's GrResourceCache.
    if (m_image && m_image->platformImage() && m_image->platformImage()->isTextureBacked()) {
        callOnMainThread([image = WTF::move(m_image)]() mutable {
            image = nullptr;
        });
    }
#endif
}

bool CoordinatedPlatformLayerBufferNativeImage::tryEnsureBuffer()
{
    if (m_buffer)
        return true;

#if USE(SKIA)
    if (m_image->platformImage()->isTextureBacked())
        return false;
#endif

    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_image->hasAlpha())
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = BitmapTexturePool::singleton().acquireTexture(m_size, textureFlags);

#if USE(CAIRO)
    auto* surface = m_image->platformImage().get();
    auto* imageData = cairo_image_surface_get_data(surface);
    texture->updateContents(imageData, IntRect(IntPoint(), m_size), IntPoint(), cairo_image_surface_get_stride(surface), PixelFormat::BGRA8);
#elif USE(SKIA)
    const auto& image = m_image->platformImage();
    SkPixmap pixmap;
    if (image->peekPixels(&pixmap))
        texture->updateContents(pixmap.addr(), IntRect(IntPoint(), m_size), IntPoint(), image->imageInfo().minRowBytes(), PixelFormat::BGRA8);
#endif

    m_buffer = CoordinatedPlatformLayerBufferRGB::create(WTF::move(texture), m_flags, nullptr);
    return true;
}

void CoordinatedPlatformLayerBufferNativeImage::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();

    if (!tryEnsureBuffer())
        return;

    m_buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
