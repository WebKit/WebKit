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
#include "CoordinatedGraphicsLayer.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "NicosiaBuffer.h"
#include "PlatformDisplay.h"
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/gl/GrGLInterface.h>
#include <skia/gpu/gl/GrGLTypes.h>
#include <wtf/FastMalloc.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

namespace WebCore {

class AcceleratedBufferPool {
    WTF_MAKE_FAST_ALLOCATED();
public:
    static AcceleratedBufferPool& singleton()
    {
        static std::unique_ptr<AcceleratedBufferPool> pool = makeUnique<AcceleratedBufferPool>();
        return *pool;
    }

    AcceleratedBufferPool()
        : m_releaseUnusedBuffersTimer(RunLoop::main(), this, &AcceleratedBufferPool::releaseUnusedBuffersTimerFired)
    {
    }

    ~AcceleratedBufferPool()
    {
        if (m_buffers.isEmpty())
            return;

        auto* glContext = PlatformDisplay::sharedDisplayForCompositing().skiaGLContext();
        GLContext::ScopedGLContextCurrent scopedCurrent(*glContext);
        m_buffers.clear();
    }

    Ref<Nicosia::Buffer> acquireBuffer(const IntSize& size, bool supportsAlpha)
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

private:
    struct Entry {
        explicit Entry(Ref<Nicosia::Buffer>&& buffer)
            : m_buffer(WTFMove(buffer))
        {
        }

        void markIsInUse() { m_lastUsedTime = MonotonicTime::now(); }
        bool canBeReleased (MonotonicTime minUsedTime) const { return m_lastUsedTime < minUsedTime && m_buffer->refCount() == 1; }

        Ref<Nicosia::Buffer> m_buffer;
        MonotonicTime m_lastUsedTime;
    };

    Ref<Nicosia::Buffer> createAcceleratedBuffer(const IntSize& size, bool supportsAlpha)
    {
        auto* grContext = PlatformDisplay::sharedDisplayForCompositing().skiaGrContext();
        auto imageInfo = SkImageInfo::MakeN32Premul(size.width(), size.height());
        auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, 0, kTopLeft_GrSurfaceOrigin, nullptr);
        return Nicosia::AcceleratedBuffer::create(WTFMove(surface), supportsAlpha ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
    }

    void scheduleReleaseUnusedBuffers()
    {
        if (m_releaseUnusedBuffersTimer.isActive())
            return;

        static const Seconds releaseUnusedBuffersTimerInterval { 500_ms };
        m_releaseUnusedBuffersTimer.startOneShot(releaseUnusedBuffersTimerInterval);
    }

    void releaseUnusedBuffersTimerFired()
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

    Vector<Entry> m_buffers;
    RunLoop::Timer m_releaseUnusedBuffersTimer;
};

Ref<Nicosia::Buffer> CoordinatedGraphicsLayer::paintTile(const IntRect& tileRect, const IntRect& mappedTileRect, float contentsScale)
{
    auto paintBuffer = [&](Nicosia::Buffer& buffer) {
        buffer.beginPainting();

        GraphicsContextSkia context(sk_ref_sp(buffer.surface()));
        context.clip(IntRect { IntPoint::zero(), tileRect.size() });

        if (!contentsOpaque()) {
            context.setCompositeOperation(CompositeOperator::Copy);
            context.fillRect(IntRect { IntPoint::zero(), tileRect.size() }, Color::transparentBlack);
            context.setCompositeOperation(CompositeOperator::SourceOver);
        }

        context.translate(-tileRect.x(), -tileRect.y());
        context.scale({ contentsScale, contentsScale });
        paintGraphicsLayerContents(context, mappedTileRect);

        buffer.completePainting();
    };

    static const char* enableCPURendering = getenv("WEBKIT_SKIA_ENABLE_CPU_RENDERING");
    if (enableCPURendering && strcmp(enableCPURendering, "0")) {
        auto buffer = Nicosia::UnacceleratedBuffer::create(tileRect.size(), contentsOpaque() ? Nicosia::Buffer::NoFlags : Nicosia::Buffer::SupportsAlpha);
        paintBuffer(buffer.get());
        return buffer;
    }

    auto* glContext = PlatformDisplay::sharedDisplayForCompositing().skiaGLContext();
    RELEASE_ASSERT(glContext);
    GLContext::ScopedGLContextCurrent scopedCurrent(*glContext);
    auto buffer = AcceleratedBufferPool::singleton().acquireBuffer(tileRect.size(), !contentsOpaque());
    paintBuffer(buffer.get());
    return buffer;
}

Ref<Nicosia::Buffer> CoordinatedGraphicsLayer::paintImage(Image& image)
{
    // FIXME: can we just get the image texture if accelerated or upload the pixels if not acclerated instead of painting?.
    // Always render unaccelerated here for now.
    auto buffer = Nicosia::UnacceleratedBuffer::create(IntSize(image.size()), !image.currentFrameKnownToBeOpaque() ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
    buffer->beginPainting();
    GraphicsContextSkia context(sk_ref_sp(buffer->surface()));
    IntRect rect { IntPoint::zero(), IntSize { image.size() } };
    context.drawImage(image, rect, rect, ImagePaintingOptions(CompositeOperator::Copy));
    buffer->completePainting();
    return buffer;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
