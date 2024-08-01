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
#include "NicosiaBuffer.h"

#include <wtf/FastMalloc.h>

#if USE(SKIA)
#include "FontRenderOptions.h"
#include "GLContext.h"
#include "GLFence.h"
#include "PlatformDisplay.h"
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkStream.h>
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <wtf/MainThread.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES3/gl3.h>
#endif
#endif

namespace Nicosia {
using namespace WebCore;

Lock Buffer::s_layersMemoryUsageLock;
double Buffer::s_currentLayersMemoryUsage = 0.0;
double Buffer::s_maxLayersMemoryUsage = 0.0;

void Buffer::resetMemoryUsage()
{
    Locker locker { s_layersMemoryUsageLock };
    s_maxLayersMemoryUsage = s_currentLayersMemoryUsage;
}

double Buffer::getMemoryUsage()
{
    // The memory usage is max of memory usage since last resetMemoryUsage or getMemoryUsage.
    Locker locker { s_layersMemoryUsageLock };
    const auto memoryUsage = s_maxLayersMemoryUsage;
    s_maxLayersMemoryUsage = s_currentLayersMemoryUsage;
    return memoryUsage;
}

Ref<Buffer> UnacceleratedBuffer::create(const WebCore::IntSize& size, Flags flags)
{
    return adoptRef(*new UnacceleratedBuffer(size, flags));
}

UnacceleratedBuffer::UnacceleratedBuffer(const WebCore::IntSize& size, Flags flags)
    : Buffer(flags)
    , m_size(size)
{
    const auto checkedArea = size.area() * 4;
    m_data = MallocPtr<unsigned char>::tryZeroedMalloc(checkedArea);

    {
        Locker locker { s_layersMemoryUsageLock };
        s_currentLayersMemoryUsage += checkedArea;
        s_maxLayersMemoryUsage = std::max(s_maxLayersMemoryUsage, s_currentLayersMemoryUsage);
    }

#if USE(SKIA)
    auto imageInfo = SkImageInfo::MakeN32Premul(size.width(), size.height(), SkColorSpace::MakeSRGB());
    // FIXME: ref buffer and unref on release proc?
    SkSurfaceProps properties = { 0, WebCore::FontRenderOptions::singleton().subpixelOrder() };
    m_surface = SkSurfaces::WrapPixels(imageInfo, m_data.get(), imageInfo.minRowBytes64(), &properties);
#endif
}

UnacceleratedBuffer::~UnacceleratedBuffer()
{
    const auto checkedArea = m_size.area().value() * 4;
    {
        Locker locker { s_layersMemoryUsageLock };
        s_currentLayersMemoryUsage -= checkedArea;
    }
}

void UnacceleratedBuffer::beginPainting()
{
    Locker locker { m_painting.lock };
    ASSERT(m_painting.state == PaintingState::Complete);
    m_painting.state = PaintingState::InProgress;
}

void UnacceleratedBuffer::completePainting()
{
    Locker locker { m_painting.lock };
    ASSERT(m_painting.state == PaintingState::InProgress);
    m_painting.state = PaintingState::Complete;
    m_painting.condition.notifyOne();
}

void UnacceleratedBuffer::waitUntilPaintingComplete()
{
    Locker locker { m_painting.lock };
    m_painting.condition.wait(m_painting.lock, [this] {
        return m_painting.state == PaintingState::Complete;
    });
}

#if USE(SKIA)
Ref<Buffer> AcceleratedBuffer::create(sk_sp<SkSurface>&& surface, Flags flags)
{
    return adoptRef(*new AcceleratedBuffer(WTFMove(surface), flags));
}

AcceleratedBuffer::AcceleratedBuffer(sk_sp<SkSurface>&& surface, Flags flags)
    : Buffer(flags)
{
    m_surface = WTFMove(surface);
}

AcceleratedBuffer::~AcceleratedBuffer()
{
    ensureOnMainThread([surface = WTFMove(m_surface), fence = WTFMove(m_fence)]() mutable {
        PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();
        fence = nullptr;
        surface = nullptr;
    });
}

WebCore::IntSize AcceleratedBuffer::size() const
{
    return { m_surface->width(), m_surface->height() };
}

void AcceleratedBuffer::beginPainting()
{
    m_surface->getCanvas()->save();
    m_surface->getCanvas()->clear(SkColors::kTransparent);
}

void AcceleratedBuffer::completePainting()
{
    m_surface->getCanvas()->restore();

    auto* grContext = WebCore::PlatformDisplay::sharedDisplay().skiaGrContext();
    if (WebCore::GLFence::isSupported()) {
        grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kNo);
        m_fence = WebCore::GLFence::create();
        if (!m_fence)
            grContext->submit(GrSyncCpu::kYes);
    } else
        grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kYes);

    auto texture = SkSurfaces::GetBackendTexture(m_surface.get(), SkSurface::BackendHandleAccess::kFlushRead);
    ASSERT(texture.isValid());
    GrGLTextureInfo textureInfo;
    bool retrievedTextureInfo = GrBackendTextures::GetGLTextureInfo(texture, &textureInfo);
    ASSERT_UNUSED(retrievedTextureInfo, retrievedTextureInfo);
    m_textureID = textureInfo.fID;
    RELEASE_ASSERT(m_textureID > 0);
}

void AcceleratedBuffer::waitUntilPaintingComplete()
{
    if (!m_fence)
        return;

    m_fence->serverWait();
    m_fence = nullptr;
}
#endif

Buffer::Buffer(Flags flags)
    : m_flags(flags)
{
}

Buffer::~Buffer() = default;

} // namespace Nicosia
