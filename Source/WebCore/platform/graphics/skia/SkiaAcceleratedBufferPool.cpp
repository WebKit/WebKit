/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "SkiaAcceleratedBufferPool.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "FontRenderOptions.h"
#include "GLContext.h"
#include "PlatformDisplay.h"
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/gl/GrGLInterface.h>
#include <skia/gpu/gl/GrGLTypes.h>

namespace WebCore {

SkiaAcceleratedBufferPool::SkiaAcceleratedBufferPool()
    : m_releaseUnusedBuffersTimer(RunLoop::main(), this, &SkiaAcceleratedBufferPool::releaseUnusedBuffersTimerFired)
{
}

SkiaAcceleratedBufferPool::~SkiaAcceleratedBufferPool() = default;

RefPtr<Nicosia::Buffer> SkiaAcceleratedBufferPool::acquireBuffer(const IntSize& size, bool supportsAlpha)
{
    Entry* selectedEntry = std::find_if(m_buffers.begin(), m_buffers.end(), [&](Entry& entry) {
        return entry.m_buffer->refCount() == 1 && entry.m_buffer->size() == size && entry.m_buffer->supportsAlpha() == supportsAlpha;
    });

    if (selectedEntry == m_buffers.end()) {
        auto buffer = createAcceleratedBuffer(size, supportsAlpha);
        if (!buffer)
            return nullptr;

        m_buffers.append(Entry(Ref { *buffer }));
        selectedEntry = &m_buffers.last();
    }

    scheduleReleaseUnusedBuffers();
    selectedEntry->markIsInUse();
    return selectedEntry->m_buffer.ptr();
}

RefPtr<Nicosia::Buffer> SkiaAcceleratedBufferPool::createAcceleratedBuffer(const IntSize& size, bool supportsAlpha)
{
    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    RELEASE_ASSERT(grContext);
    auto imageInfo = SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    SkSurfaceProps properties = { 0, FontRenderOptions::singleton().subpixelOrder() };
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, PlatformDisplay::sharedDisplay().msaaSampleCount(), kTopLeft_GrSurfaceOrigin, &properties);
    if (!surface)
        return nullptr;
    return Nicosia::AcceleratedBuffer::create(WTFMove(surface), supportsAlpha ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
}

void SkiaAcceleratedBufferPool::scheduleReleaseUnusedBuffers()
{
    if (m_releaseUnusedBuffersTimer.isActive())
        return;

    static const Seconds releaseUnusedBuffersTimerInterval { 500_ms };
    m_releaseUnusedBuffersTimer.startOneShot(releaseUnusedBuffersTimerInterval);
}

void SkiaAcceleratedBufferPool::releaseUnusedBuffersTimerFired()
{
    if (m_buffers.isEmpty())
        return;

    // Delete entries, which have been unused in releaseUnusedSecondsTolerance.
    static const Seconds releaseUnusedSecondsTolerance { 3_s };
    MonotonicTime minUsedTime = MonotonicTime::now() - releaseUnusedSecondsTolerance;

    m_buffers.removeAllMatching([&minUsedTime](const Entry& entry) {
        return entry.canBeReleased(minUsedTime);
    });

    if (!m_buffers.isEmpty())
        scheduleReleaseUnusedBuffers();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
