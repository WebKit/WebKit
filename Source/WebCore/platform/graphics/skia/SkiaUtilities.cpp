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

#include "config.h"
#include "SkiaUtilities.h"

#if USE(SKIA)

#include "BitmapTexture.h"
#include "ColorSpaceSkia.h"
#include "GLContext.h"
#include "GLFence.h"
#include "GraphicsTypes.h"
#include "NotImplemented.h"
#include "PlatformDisplay.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/private/chromium/GrPromiseImageTexture.h>
#include <skia/private/chromium/SkImageChromium.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES3/gl3.h>
#endif

namespace WebCore {
namespace SkiaUtilities {

GrBackendTexture createBackendTexture(const BitmapTexture& texture)
{
    RELEASE_ASSERT(texture.id());
    RELEASE_ASSERT(!texture.size().isEmpty());

    GrGLTextureInfo externalTexture;
    externalTexture.fTarget = GL_TEXTURE_2D;
    externalTexture.fID = texture.id();
    // EXT_texture_format_BGRA8888 (unsized form, matches BitmapTexture::textureFormat).
    externalTexture.fFormat = texture.flags().contains(BitmapTexture::Flags::UseBGRALayout) ? GL_BGRA8_EXT : GL_RGBA8;
    return GrBackendTextures::MakeGL(texture.size().width(), texture.size().height(), skgpu::Mipmapped::kNo, externalTexture);
}

sk_sp<SkSurface> createSurface(GrDirectContext* grContext, const BitmapTexture& texture, const SkSurfaceProps& properties, GrSurfaceOrigin origin, unsigned sampleCount)
{
    RELEASE_ASSERT(grContext);

    auto backendTexture = createBackendTexture(texture);
    return SkSurfaces::WrapBackendTexture(grContext, backendTexture, origin, sampleCount, kRGBA_8888_SkColorType, sRGBColorSpaceSingleton(), &properties);
}

sk_sp<SkImage> rewrapImageForContext(GrDirectContext* grContext, const SkImage& image)
{
    GrBackendTexture backendTexture;
    if (!SkImages::GetBackendTextureFromImage(&image, &backendTexture, false))
        return nullptr;
    return SkImages::BorrowTextureFrom(grContext, backendTexture, kTopLeft_GrSurfaceOrigin, image.colorType(), image.alphaType(), image.refColorSpace());
}

std::optional<unsigned> retrieveGLTextureID(const SkImage& image)
{
    GrBackendTexture backendTexture;
    if (!SkImages::GetBackendTextureFromImage(&image, &backendTexture, false))
        return std::nullopt;

    GrGLTextureInfo textureInfo;
    if (!GrBackendTextures::GetGLTextureInfo(backendTexture, &textureInfo))
        return std::nullopt;

    return textureInfo.fID;
}

static std::unique_ptr<GLFence> createFenceAfterFlush(GrDirectContext* grContext)
{
    auto& glDisplay = PlatformDisplay::sharedDisplay().glDisplay();
    if (GLFence::isSupported(glDisplay)) {
        grContext->submit(GrSyncCpu::kNo);

        if (auto fence = GLFence::create(glDisplay))
            return fence;
    }

    grContext->submit(GrSyncCpu::kYes);
    return nullptr;
}

std::unique_ptr<GLFence> flushAndSubmitSurfaceWithFence(GrDirectContext* grContext, SkSurface* surface)
{
    grContext->flush(surface);
    return createFenceAfterFlush(grContext);
}

std::unique_ptr<GLFence> flushAndSubmitImageWithFence(GrDirectContext* grContext, const sk_sp<SkImage>& image)
{
    grContext->flush(image);
    return createFenceAfterFlush(grContext);
}

std::unique_ptr<GLFence> flushAndSubmitWithFence(GrDirectContext* grContext)
{
    grContext->flush();
    return createFenceAfterFlush(grContext);
}

SkBlendMode toSkiaBlendMode(BlendMode blendMode, std::optional<CompositeOperator> operation)
{
    switch (blendMode) {
    case BlendMode::Normal:
        switch (operation.value_or(CompositeOperator::SourceOver)) {
        case CompositeOperator::Clear:
            return SkBlendMode::kClear;
        case CompositeOperator::Copy:
            return SkBlendMode::kSrc;
        case CompositeOperator::SourceOver:
            return SkBlendMode::kSrcOver;
        case CompositeOperator::SourceIn:
            return SkBlendMode::kSrcIn;
        case CompositeOperator::SourceOut:
            return SkBlendMode::kSrcOut;
        case CompositeOperator::SourceAtop:
            return SkBlendMode::kSrcATop;
        case CompositeOperator::DestinationOver:
            return SkBlendMode::kDstOver;
        case CompositeOperator::DestinationIn:
            return SkBlendMode::kDstIn;
        case CompositeOperator::DestinationOut:
            return SkBlendMode::kDstOut;
        case CompositeOperator::DestinationAtop:
            return SkBlendMode::kDstATop;
        case CompositeOperator::XOR:
            return SkBlendMode::kXor;
        case CompositeOperator::PlusLighter:
            return SkBlendMode::kPlus;
        case CompositeOperator::PlusDarker:
            notImplemented();
            return SkBlendMode::kSrcOver;
        case CompositeOperator::Difference:
            return SkBlendMode::kDifference;
        }
        break;
    case BlendMode::Multiply:
        return SkBlendMode::kMultiply;
    case BlendMode::Screen:
        return SkBlendMode::kScreen;
    case BlendMode::Overlay:
        return SkBlendMode::kOverlay;
    case BlendMode::Darken:
        return SkBlendMode::kDarken;
    case BlendMode::Lighten:
        return SkBlendMode::kLighten;
    case BlendMode::ColorDodge:
        return SkBlendMode::kColorDodge;
    case BlendMode::ColorBurn:
        return SkBlendMode::kColorBurn;
    case BlendMode::HardLight:
        return SkBlendMode::kHardLight;
    case BlendMode::SoftLight:
        return SkBlendMode::kSoftLight;
    case BlendMode::Difference:
        return SkBlendMode::kDifference;
    case BlendMode::Exclusion:
        return SkBlendMode::kExclusion;
    case BlendMode::Hue:
        return SkBlendMode::kHue;
    case BlendMode::Saturation:
        return SkBlendMode::kSaturation;
    case BlendMode::Color:
        return SkBlendMode::kColor;
    case BlendMode::Luminosity:
        return SkBlendMode::kLuminosity;
    case BlendMode::PlusLighter:
        return SkBlendMode::kPlus;
    case BlendMode::PlusDarker:
        notImplemented();
        break;
    }

    return SkBlendMode::kSrcOver;
}

class PromiseImageContext {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PromiseImageContext);
public:
    static std::unique_ptr<PromiseImageContext> create(const sk_sp<SkImage>& image)
    {
        auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
        ASSERT(grContext);

        auto fence = flushAndSubmitImageWithFence(grContext, image);

        GrBackendTexture backendTexture;
        if (!SkImages::GetBackendTextureFromImage(image.get(), &backendTexture, false))
            return nullptr;

        return makeUnique<PromiseImageContext>(image, WTF::move(backendTexture), WTF::move(fence));
    }

    PromiseImageContext(const sk_sp<SkImage>& image, GrBackendTexture&& backendTexture, std::unique_ptr<GLFence>&& fence)
        : m_image(image)
        , m_backendTexture(WTF::move(backendTexture))
        , m_fence(WTF::move(fence))
        , m_runLoop(RunLoop::currentSingleton())
    {
    }

    ~PromiseImageContext()
    {
        if (auto runLoop = m_runLoop.get()) {
            if (runLoop.get() == &RunLoop::currentSingleton())
                return;

            runLoop->dispatch([image = WTF::move(m_image)]() mutable {
                image = nullptr;
            });
        }

    }

    sk_sp<GrPromiseImageTexture> promiseImageTexture()
    {
        auto* glContext = PlatformDisplay::sharedDisplay().skiaGLContext();
        if (!glContext || !glContext->makeContextCurrent())
            return nullptr;

        if (m_fence) {
            m_fence->serverWait();
            m_fence = nullptr;
        }

        return GrPromiseImageTexture::Make(m_backendTexture);
    }

private:
    sk_sp<SkImage> m_image;
    GrBackendTexture m_backendTexture;
    std::unique_ptr<GLFence> m_fence;
    ThreadSafeWeakPtr<RunLoop> m_runLoop;
};

sk_sp<SkImage> createPromiseImageIfNeeded(const sk_sp<SkImage>& image, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    if (!image->isTextureBacked())
        return image;

    auto context = PromiseImageContext::create(image);
    if (!context)
        return image;

    auto backendFormat = threadSafeGrContext->defaultBackendFormat(image->colorType(), GrRenderable::kYes);
    ASSERT(backendFormat.isValid());
    return SkImages::PromiseTextureFrom(threadSafeGrContext, backendFormat, image->dimensions(), skgpu::Mipmapped::kNo,
        kTopLeft_GrSurfaceOrigin, image->colorType(), image->alphaType(), image->refColorSpace(),
        +[](void* userData) -> sk_sp<GrPromiseImageTexture> {
            auto& context = *static_cast<PromiseImageContext*>(userData);
            return context.promiseImageTexture();
        },
        +[](void* userData) {
            std::unique_ptr<PromiseImageContext> context(static_cast<PromiseImageContext*>(userData));
        }, context.release());
}

} // namespace SkiaUtilities
} // namespace WebCore

#endif // USE(SKIA)
