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
#include "CoordinatedGraphicsLayer.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "CoordinatedTileBuffer.h"
#include "DisplayListDrawingContext.h"
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "PlatformDisplay.h"
#include "SkiaThreadedPaintingPool.h"
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/ganesh/gl/GrGLInterface.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>
#include <wtf/Vector.h>

namespace WebCore {

void CoordinatedGraphicsLayer::paintIntoGraphicsContext(GraphicsContext& context, const IntRect& dirtyRect) const
{
    IntRect initialClip(IntPoint::zero(), dirtyRect.size());
    context.clip(initialClip);

    if (!contentsOpaque()) {
        context.setCompositeOperation(CompositeOperator::Copy);
        context.fillRect(initialClip, Color::transparentBlack);
        context.setCompositeOperation(CompositeOperator::SourceOver);
    }

    auto scale = effectiveContentsScale();
    FloatRect clipRect(dirtyRect);
    clipRect.scale(1 / scale);

    context.translate(-dirtyRect.x(), -dirtyRect.y());
    context.scale(scale);
    paintGraphicsLayerContents(context, clipRect);
}

Ref<CoordinatedTileBuffer> CoordinatedGraphicsLayer::paintTile(const IntRect& dirtyRect)
{
    auto paintBuffer = [&](CoordinatedTileBuffer& buffer) {
        buffer.beginPainting();

        if (auto* canvas = buffer.canvas()) {
            canvas->save();
            canvas->clear(SkColors::kTransparent);

            GraphicsContextSkia context(*canvas, buffer.isBackedByOpenGL() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
            paintIntoGraphicsContext(context, dirtyRect);

            canvas->restore();
        }

        buffer.completePainting();
    };

    // Skia/GPU - accelerated rendering.
    auto* skiaGLContext = PlatformDisplay::sharedDisplay().skiaGLContext();
    if (auto* acceleratedBitmapTexturePool = m_coordinator->skiaAcceleratedBitmapTexturePool(); acceleratedBitmapTexturePool && skiaGLContext->makeContextCurrent()) {
        OptionSet<BitmapTexture::Flags> textureFlags;
        if (!contentsOpaque())
            textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

        auto buffer = CoordinatedAcceleratedTileBuffer::create(acceleratedBitmapTexturePool->acquireTexture(dirtyRect.size(), textureFlags));
        WTFBeginSignpost(this, PaintTile, "Skia accelerated, dirty region %ix%i+%i+%i", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
        paintBuffer(buffer.get());
        WTFEndSignpost(this, PaintTile);

        return buffer;
    }

    auto buffer = CoordinatedUnacceleratedTileBuffer::create(dirtyRect.size(), contentsOpaque() ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);

    // Skia/CPU - threaded unaccelerated rendering.
    if (auto* workerPool = m_coordinator->skiaThreadedPaintingPool()) {
        workerPool->postPaintingTask(buffer, *this, dirtyRect);
        return buffer;
    }

    // Blocking, single-thread variant.
    WTFBeginSignpost(this, PaintTile, "Skia unaccelerated, dirty region %ix%i+%i+%i", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
    paintBuffer(buffer.get());
    WTFEndSignpost(this, PaintTile);
    return buffer;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
