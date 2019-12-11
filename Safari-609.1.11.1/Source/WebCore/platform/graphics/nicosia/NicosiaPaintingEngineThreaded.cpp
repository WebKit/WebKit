/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
#include "NicosiaPaintingEngineThreaded.h"

#if USE(COORDINATED_GRAPHICS)

#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "NicosiaBuffer.h"
#include "NicosiaPaintingContext.h"

namespace Nicosia {
using namespace WebCore;

static void paintLayer(GraphicsContext& context, GraphicsLayer& layer, const IntRect& sourceRect, const IntRect& mappedSourceRect, const IntRect& targetRect, float contentsScale, bool supportsAlpha)
{
    context.save();
    context.clip(targetRect);
    context.translate(targetRect.x(), targetRect.y());

    if (supportsAlpha) {
        context.setCompositeOperation(CompositeCopy);
        context.fillRect(IntRect(IntPoint::zero(), sourceRect.size()), Color::transparent);
        context.setCompositeOperation(CompositeSourceOver);
    }

    context.translate(-sourceRect.x(), -sourceRect.y());
    context.scale(FloatSize(contentsScale, contentsScale));

    layer.paintGraphicsLayerContents(context, mappedSourceRect);

    context.restore();
}

PaintingEngineThreaded::PaintingEngineThreaded(unsigned numThreads)
    : m_workerPool(WorkerPool::create("PaintingThread"_s, numThreads))
{
}

PaintingEngineThreaded::~PaintingEngineThreaded()
{
}

bool PaintingEngineThreaded::paint(GraphicsLayer& layer, Ref<Buffer>&& buffer, const IntRect& sourceRect, const IntRect& mappedSourceRect, const IntRect& targetRect, float contentsScale)
{
    buffer->beginPainting();

    PaintingOperations paintingOperations;
    PaintingContext::record(paintingOperations,
        [&](GraphicsContext& context)
        {
            paintLayer(context, layer, sourceRect, mappedSourceRect, targetRect, contentsScale, buffer->supportsAlpha());
        });

    m_workerPool->postTask([paintingOperations = WTFMove(paintingOperations), buffer = WTFMove(buffer)] {
        PaintingContext::replay(buffer, paintingOperations);
        buffer->completePainting();
    });

    return true;
}

} // namespace Nicosia

#endif // USE(COORDINATED_GRAPHICS)
