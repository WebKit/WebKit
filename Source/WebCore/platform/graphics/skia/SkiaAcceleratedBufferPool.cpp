/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SkiaAcceleratedBufferPool.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "GLContext.h"
#include "PlatformDisplay.h"
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

SkiaAcceleratedBufferPool::~SkiaAcceleratedBufferPool()
{
    if (m_buffers.isEmpty())
        return;

    auto* glContext = PlatformDisplay::sharedDisplayForCompositing().skiaGLContext();
    GLContext::ScopedGLContextCurrent scopedCurrent(*glContext);
    m_buffers.clear();
}

Ref<Nicosia::Buffer> SkiaAcceleratedBufferPool::acquireBuffer(const IntSize& size, bool supportsAlpha)
{
    Entry* selectedEntry = std::find_if(m_buffers.begin(), m_buffers.end(), [&](Entry& entry) {
        return entry.m_buffer->refCount() == 1 && entry.m_buffer->size() == size && entry.m_buffer->supportsAlpha() == supportsAlpha;
    });

    if (selectedEntry == m_buffers.end()) {
        m_buffers.append(Entry(createAcceleratedBuffer(size, supportsAlpha)));
        selectedEntry = &m_buffers.last();
    }

    scheduleReleaseUnusedBuffers();
    selectedEntry->markIsInUse();
    return selectedEntry->m_buffer.copyRef();
}

Ref<Nicosia::Buffer> SkiaAcceleratedBufferPool::createAcceleratedBuffer(const IntSize& size, bool supportsAlpha)
{
    auto* grContext = PlatformDisplay::sharedDisplayForCompositing().skiaGrContext();
    auto imageInfo = SkImageInfo::MakeN32Premul(size.width(), size.height());
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, 0, kTopLeft_GrSurfaceOrigin, nullptr);
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

    {
        auto* glContext = PlatformDisplay::sharedDisplayForCompositing().skiaGLContext();
        GLContext::ScopedGLContextCurrent scopedCurrent(*glContext);
        m_buffers.removeAllMatching([&minUsedTime](const Entry& entry) {
            return entry.canBeReleased(minUsedTime);
        });
    }

    if (!m_buffers.isEmpty())
        scheduleReleaseUnusedBuffers();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
