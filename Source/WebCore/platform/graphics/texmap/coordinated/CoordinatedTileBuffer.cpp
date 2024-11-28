/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017, 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoordinatedTileBuffer.h"

#if USE(COORDINATED_GRAPHICS)

#if USE(SKIA)
#include "BitmapTexture.h"
#include "FontRenderOptions.h"
#include "GLContext.h"
#include "GLFence.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkStream.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/MainThread.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES3/gl3.h>
#endif
#endif

#include "PixelFormat.h"

namespace WebCore {

Lock CoordinatedTileBuffer::s_layersMemoryUsageLock;
double CoordinatedTileBuffer::s_currentLayersMemoryUsage = 0.0;
double CoordinatedTileBuffer::s_maxLayersMemoryUsage = 0.0;

void CoordinatedTileBuffer::resetMemoryUsage()
{
    Locker locker { s_layersMemoryUsageLock };
    s_maxLayersMemoryUsage = s_currentLayersMemoryUsage;
}

double CoordinatedTileBuffer::getMemoryUsage()
{
    // The memory usage is max of memory usage since last resetMemoryUsage or getMemoryUsage.
    Locker locker { s_layersMemoryUsageLock };
    const auto memoryUsage = s_maxLayersMemoryUsage;
    s_maxLayersMemoryUsage = s_currentLayersMemoryUsage;
    return memoryUsage;
}

#if USE(SKIA)
SkCanvas* CoordinatedTileBuffer::canvas()
{
    if (!tryEnsureSurface())
        return nullptr;
    return m_surface->getCanvas();
}
#endif

void CoordinatedTileBuffer::beginPainting()
{
    Locker locker { m_painting.lock };
    ASSERT(m_painting.state == PaintingState::Complete);
    m_painting.state = PaintingState::InProgress;
}

void CoordinatedTileBuffer::completePainting()
{
    Locker locker { m_painting.lock };
    ASSERT(m_painting.state == PaintingState::InProgress);
    m_painting.state = PaintingState::Complete;
    m_painting.condition.notifyOne();

#if USE(SKIA)
    // Surface is no longer needed, destroy it (in the same thread that created it).
    m_surface = nullptr;
#endif
}

void CoordinatedTileBuffer::waitUntilPaintingComplete()
{
    Locker locker { m_painting.lock };
    m_painting.condition.wait(m_painting.lock, [this] {
        return m_painting.state == PaintingState::Complete;
    });
}

Ref<CoordinatedTileBuffer> CoordinatedUnacceleratedTileBuffer::create(const IntSize& size, Flags flags)
{
    return adoptRef(*new CoordinatedUnacceleratedTileBuffer(size, flags));
}

CoordinatedUnacceleratedTileBuffer::CoordinatedUnacceleratedTileBuffer(const IntSize& size, Flags flags)
    : CoordinatedTileBuffer(flags)
    , m_size(size)
{
    const auto checkedArea = size.area() * 4;
    m_data = MallocSpan<unsigned char>::tryZeroedMalloc(checkedArea);

    {
        Locker locker { s_layersMemoryUsageLock };
        s_currentLayersMemoryUsage += checkedArea;
        s_maxLayersMemoryUsage = std::max(s_maxLayersMemoryUsage, s_currentLayersMemoryUsage);
    }
}

PixelFormat CoordinatedUnacceleratedTileBuffer::pixelFormat() const
{
#if USE(SKIA)
    // For GPU/hybrid rendering, prefer RGBA, otherwise use BGRA.
    if (ProcessCapabilities::canUseAcceleratedBuffers())
        return PixelFormat::RGBA8;
#endif

    return PixelFormat::BGRA8;
}

CoordinatedUnacceleratedTileBuffer::~CoordinatedUnacceleratedTileBuffer()
{
    const auto checkedArea = m_size.area().value() * 4;
    {
        Locker locker { s_layersMemoryUsageLock };
        s_currentLayersMemoryUsage -= checkedArea;
    }
}

#if USE(SKIA)
bool CoordinatedUnacceleratedTileBuffer::tryEnsureSurface()
{
    if (m_surface)
        return true;

    auto colorType = pixelFormat() == PixelFormat::BGRA8 ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType;
    auto imageInfo = SkImageInfo::Make(m_size.width(), m_size.height(), colorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    // FIXME: ref buffer and unref on release proc?
    SkSurfaceProps properties = { 0, FontRenderOptions::singleton().subpixelOrder() };
    m_surface = SkSurfaces::WrapPixels(imageInfo, data(), imageInfo.minRowBytes64(), &properties);
    return true;
}
#endif

#if USE(SKIA)
Ref<CoordinatedTileBuffer> CoordinatedAcceleratedTileBuffer::create(Ref<BitmapTexture>&& texture)
{
    auto flags = CoordinatedTileBuffer::Flags { texture->isOpaque() ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha };
    return adoptRef(*new CoordinatedAcceleratedTileBuffer(WTFMove(texture), flags));
}

CoordinatedAcceleratedTileBuffer::CoordinatedAcceleratedTileBuffer(Ref<BitmapTexture>&& texture, Flags flags)
    : CoordinatedTileBuffer(flags)
    , m_texture(WTFMove(texture))
{
}

CoordinatedAcceleratedTileBuffer::~CoordinatedAcceleratedTileBuffer()
{
    ensureOnMainThread([fence = WTFMove(m_fence)]() mutable {
        PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();
        fence = nullptr;
    });
}

IntSize CoordinatedAcceleratedTileBuffer::size() const
{
    return m_texture->size();
}

bool CoordinatedAcceleratedTileBuffer::tryEnsureSurface()
{
    if (m_surface)
        return true;

    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return false;

    GrGLTextureInfo externalTexture;
    externalTexture.fTarget = GL_TEXTURE_2D;
    externalTexture.fID = m_texture->id();
    externalTexture.fFormat = GL_RGBA8;

    const auto& size = m_texture->size();
    auto backendTexture = GrBackendTextures::MakeGL(size.width(), size.height(), skgpu::Mipmapped::kNo, externalTexture);

    SkSurfaceProps properties = { 0, FontRenderOptions::singleton().subpixelOrder() };
    m_surface = SkSurfaces::WrapBackendTexture(PlatformDisplay::sharedDisplay().skiaGrContext(),
        backendTexture,
        kTopLeft_GrSurfaceOrigin,
        PlatformDisplay::sharedDisplay().msaaSampleCount(),
        kRGBA_8888_SkColorType,
        SkColorSpace::MakeSRGB(),
        &properties);

    return true;
}

void CoordinatedAcceleratedTileBuffer::completePainting()
{
    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    if (GLFence::isSupported()) {
        grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kNo);
        m_fence = GLFence::create();
        if (!m_fence)
            grContext->submit(GrSyncCpu::kYes);
    } else
        grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kYes);

    CoordinatedTileBuffer::completePainting();
}

void CoordinatedAcceleratedTileBuffer::waitUntilPaintingComplete()
{
    CoordinatedTileBuffer::waitUntilPaintingComplete();

    if (!m_fence)
        return;

    m_fence->serverWait();
    m_fence = nullptr;
}
#endif

CoordinatedTileBuffer::CoordinatedTileBuffer(Flags flags)
    : m_flags(flags)
{
}

CoordinatedTileBuffer::~CoordinatedTileBuffer() = default;

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
