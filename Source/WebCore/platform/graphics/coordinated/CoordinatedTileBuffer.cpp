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
#include "BitmapTexturePool.h"
#include "FontRenderOptions.h"
#include "GLContext.h"
#include "GLFence.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include "SkiaUtilities.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkStream.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/MainThread.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES3/gl3.h>
#endif
#endif

#if USE(CAIRO)
#include <cairo.h>
#endif

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

void CoordinatedTileBuffer::didAllocateMemory(size_t size)
{
    ASSERT(size);

    Locker locker { s_layersMemoryUsageLock };
    s_currentLayersMemoryUsage += size;
    s_maxLayersMemoryUsage = std::max(s_maxLayersMemoryUsage, s_currentLayersMemoryUsage);
}

void CoordinatedTileBuffer::didReleaseMemory(size_t size)
{
    if (!size)
        return;

    Locker locker { s_layersMemoryUsageLock };
    s_currentLayersMemoryUsage -= size;
}

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
}

CoordinatedUnacceleratedTileBuffer::~CoordinatedUnacceleratedTileBuffer()
{
    didReleaseMemory(m_data.size());
}

Vector<unsigned char> CoordinatedUnacceleratedTileBuffer::takeData()
{
    auto data = WTF::move(m_data);
    didReleaseMemory(data.size());
    return data;
}

bool CoordinatedUnacceleratedTileBuffer::tryAllocateData()
{
    const auto checkedArea = m_size.area() * 4;
    if (checkedArea.hasOverflowed() || !m_data.tryReserveCapacity(checkedArea.value()))
        return false;

    m_data.grow(checkedArea.value());
    didAllocateMemory(m_data.size());
    return true;
}

#if USE(CAIRO)
RefPtr<cairo_surface_t> CoordinatedUnacceleratedTileBuffer::createSurface()
{
    if (!tryAllocateData())
        return nullptr;

    m_data.fill(0);
    return adoptRef(cairo_image_surface_create_for_data(m_data.mutableSpan().data(), CAIRO_FORMAT_ARGB32, m_size.width(), m_size.height(), m_size.width() * 4));
}
#endif

#if USE(SKIA)
SkCanvas* CoordinatedUnacceleratedTileBuffer::canvas()
{
    if (!m_surface) {
        if (!tryAllocateData())
            return nullptr;

        auto alphaType = supportsAlpha() ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
        auto imageInfo = SkImageInfo::Make(m_size.width(), m_size.height(), kBGRA_8888_SkColorType, alphaType, SkColorSpace::MakeSRGB());
        auto properties = FontRenderOptions::singleton().createSurfaceProps();
        m_surface = SkSurfaces::WrapPixels(imageInfo, m_data.mutableSpan().data(), imageInfo.minRowBytes64(), &properties);
    }
    return m_surface->getCanvas();
}

#if USE(TEXTURE_MAPPER)
Ref<CoordinatedTileBuffer> CoordinatedAcceleratedTileBuffer::create(const IntSize& size, Flags flags)
{
    return adoptRef(*new CoordinatedAcceleratedTileBuffer(size, flags));
}

CoordinatedAcceleratedTileBuffer::CoordinatedAcceleratedTileBuffer(const IntSize& size, Flags flags)
    : CoordinatedTileBuffer(flags)
    , m_size(size)
{
}

#else

Ref<CoordinatedTileBuffer> CoordinatedAcceleratedTileBuffer::create(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, const IntSize& size, Flags flags)
{
    auto alphaType = (flags & SupportsAlpha) ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
    auto imageInfo = SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType, alphaType, SkColorSpace::MakeSRGB());
    auto backendFormat = threadSafeGrContext->defaultBackendFormat(kRGBA_8888_SkColorType, GrRenderable::kYes);
    ASSERT(backendFormat.isValid());
    auto properties = FontRenderOptions::singleton().createSurfaceProps();
    auto maxResourceCacheBytes = PlatformDisplay::sharedDisplay().maxSkiaResourceCacheBytes();
    auto characterization = threadSafeGrContext->createCharacterization(maxResourceCacheBytes, imageInfo, backendFormat, 0, kTopLeft_GrSurfaceOrigin, properties, skgpu::Mipmapped::kNo);
    return adoptRef(*new CoordinatedAcceleratedTileBuffer(WTF::move(characterization), flags));
}

CoordinatedAcceleratedTileBuffer::CoordinatedAcceleratedTileBuffer(GrSurfaceCharacterization&& characterization, Flags flags)
    : CoordinatedTileBuffer(flags)
    , m_characterization(WTF::move(characterization))
{
}
#endif

CoordinatedAcceleratedTileBuffer::~CoordinatedAcceleratedTileBuffer() = default;

IntSize CoordinatedAcceleratedTileBuffer::size() const
{
#if USE(TEXTURE_MAPPER)
    return m_size;
#else
    return { m_characterization.width(), m_characterization.height() };
#endif
}

SkCanvas* CoordinatedAcceleratedTileBuffer::canvas()
{
#if USE(TEXTURE_MAPPER)
    if (!m_surface) {
        if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
            return nullptr;

        OptionSet<BitmapTexture::Flags> textureFlags;
        if (supportsAlpha())
            textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

        m_texture = BitmapTexturePool::singleton().acquireTexture(m_size, textureFlags);
        m_surface = m_texture->createSkiaSurface(PlatformDisplay::sharedDisplay().skiaGrContext());
    }
    return m_surface->getCanvas();
#else
    if (!m_recorder)
        m_recorder.emplace(m_characterization);
    return m_recorder->getCanvas();
#endif
}

void CoordinatedAcceleratedTileBuffer::completePainting()
{
#if USE(TEXTURE_MAPPER)
    if (m_surface) {
        auto* recordingContext = m_surface->recordingContext();
        auto* grContext = recordingContext ? recordingContext->asDirectContext() : nullptr;
        if (!grContext) {
            CoordinatedTileBuffer::completePainting();
            return;
        }

        m_fence = SkiaUtilities::flushAndSubmitSurfaceWithFence(grContext, m_surface.get());
    }
#else
    if (m_recorder)
        m_displayList = m_recorder->detach();
#endif

    CoordinatedTileBuffer::completePainting();
}

#if USE(TEXTURE_MAPPER)
RefPtr<BitmapTexture> CoordinatedAcceleratedTileBuffer::takeTexture()
{
    if (m_fence) {
        m_fence->serverWait();
        m_fence = nullptr;
    }

    return std::exchange(m_texture, nullptr);
}
#endif
#endif // USE(SKIA)

CoordinatedTileBuffer::CoordinatedTileBuffer(Flags flags)
    : m_flags(flags)
{
}

CoordinatedTileBuffer::~CoordinatedTileBuffer() = default;

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
