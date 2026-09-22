/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "CoordinatedPlatformLayerBufferSkiaImage.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
#include "SkiaUtilities.h"

#if ENABLE(WEBGL)
#include "GLContext.h"
#include "GLFence.h"
#include "PlatformDisplay.h"
#include <epoxy/egl.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/private/chromium/GrPromiseImageTexture.h>
#include <skia/private/chromium/SkImageChromium.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferSkiaImage> CoordinatedPlatformLayerBufferSkiaImage::create(const sk_sp<SkImage>& image, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    sk_sp<SkImage> skiaImage = image->isTextureBacked() ? SkiaUtilities::createPromiseImageIfNeeded(image, threadSafeGrContext) : image;
    return makeUnique<CoordinatedPlatformLayerBufferSkiaImage>(WTF::move(skiaImage), image->isOpaque() ? AlphaMode::Opaque : AlphaMode::Premultiplied, Rotation::None);
}

#if ENABLE(WEBGL)
struct PromiseWebGLImageContext {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(PromiseWebGLImageContext);

    PromiseWebGLImageContext(unsigned texture, const IntSize& textureSize, std::unique_ptr<GLFence>&& glFence)
        : textureID(texture)
        , size(textureSize)
        , fence(WTF::move(glFence))
    {
    }

    sk_sp<GrPromiseImageTexture> promiseImageTexture()
    {
        auto* glContext = PlatformDisplay::sharedDisplay().skiaGLContext();
        if (!glContext || !glContext->makeContextCurrent())
            return nullptr;

        if (fence) {
            fence->serverWait();
            fence = nullptr;
        }

        GrGLTextureInfo externalTexture;
        externalTexture.fTarget = GL_TEXTURE_2D;
        externalTexture.fID = textureID;
        externalTexture.fFormat = GL_RGBA8;
        return GrPromiseImageTexture::Make(GrBackendTextures::MakeGL(size.width(), size.height(), skgpu::Mipmapped::kNo, externalTexture));
    }

    unsigned textureID { 0 };
    IntSize size;
    std::unique_ptr<GLFence> fence;
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(PromiseWebGLImageContext);

std::unique_ptr<CoordinatedPlatformLayerBufferSkiaImage> CoordinatedPlatformLayerBufferSkiaImage::create(unsigned textureID, const IntSize& size, AlphaMode alphaMode, std::unique_ptr<GLFence>&& fence, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    if (!threadSafeGrContext)
        return nullptr;

    auto backendFormat = threadSafeGrContext->defaultBackendFormat(kRGBA_8888_SkColorType, GrRenderable::kYes);
    ASSERT(backendFormat.isValid());

    auto context = makeUnique<PromiseWebGLImageContext>(textureID, size, WTF::move(fence));
    auto skiaImage = SkImages::PromiseTextureFrom(threadSafeGrContext, backendFormat, SkISize::Make(size.width(), size.height()), skgpu::Mipmapped::kNo,
        kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, toSkiaAlphaType(alphaMode), SkColorSpace::MakeSRGB(),
        +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
            auto& context = *static_cast<PromiseWebGLImageContext*>(userData);
            return context.promiseImageTexture();
        },
        +[](void* userData) {
            std::unique_ptr<PromiseWebGLImageContext> context(static_cast<PromiseWebGLImageContext*>(userData));
        }, context.release());

    return makeUnique<CoordinatedPlatformLayerBufferSkiaImage>(WTF::move(skiaImage), alphaMode, Rotation::None);
}
#endif

CoordinatedPlatformLayerBufferSkiaImage::CoordinatedPlatformLayerBufferSkiaImage(sk_sp<SkImage>&& image, AlphaMode alphaMode, Rotation rotation)
    : CoordinatedPlatformLayerBuffer(Type::SkiaImage, { image->width(), image->height() }, alphaMode, rotation)
    , m_image(WTF::move(image))
{
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
