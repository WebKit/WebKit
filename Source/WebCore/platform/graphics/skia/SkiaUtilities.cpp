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
#include "GLFence.h"
#include "GraphicsTypes.h"
#include "NotImplemented.h"
#include "PlatformDisplay.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

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
    externalTexture.fFormat = GL_RGBA8;
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

sk_sp<SkImage> borrowBackendTextureAsImage(GrDirectContext* grContext, const GrBackendTexture& backendTexture)
{
    return SkImages::BorrowTextureFrom(grContext, backendTexture, kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kPremul_SkAlphaType, sRGBColorSpaceSingleton());
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

} // namespace SkiaUtilities
} // namespace WebCore

#endif // USE(SKIA)
