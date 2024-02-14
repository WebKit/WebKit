/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AsyncPDFRenderer.h"

#if ENABLE(UNIFIED_PDF)

#include "Logging.h"
#include "UnifiedPDFPlugin.h"
#include <CoreGraphics/CoreGraphics.h>
#include <PDFKit/PDFKit.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

Ref<AsyncPDFRenderer> AsyncPDFRenderer::create(UnifiedPDFPlugin& plugin)
{
    return adoptRef(*new AsyncPDFRenderer(plugin));
}

AsyncPDFRenderer::AsyncPDFRenderer(UnifiedPDFPlugin& plugin)
    : m_plugin(plugin)
    , m_paintingWorkQueue(WorkQueue::create("WebKit: PDF Painting Work Queue"_s, WorkQueue::QOS::UserInteractive)) // Maybe make this concurrent?
{
}

AsyncPDFRenderer::~AsyncPDFRenderer()
{
    if (auto* tiledBacking = m_pdfContentsLayer->tiledBacking())
        tiledBacking->setClient(nullptr);
}

void AsyncPDFRenderer::setupWithLayer(GraphicsLayer& layer)
{
    m_pdfContentsLayer = &layer;

    if (auto* tiledBacking = m_pdfContentsLayer->tiledBacking())
        tiledBacking->setClient(this);
}

void AsyncPDFRenderer::willRepaintTile(TileGridIndex gridIndex, TileIndex tileIndex, const FloatRect& tileRect, const FloatRect& tileDirtyRect)
{
    auto tileInfo = TileForGrid { gridIndex, tileIndex };

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRepaintTile " << tileIndex << " " << tileRect << " (dirty rect " << tileDirtyRect << ") - already queued " << m_enqueuedTileRenders.contains(tileInfo) << " have cached tile " << m_rendereredTiles.contains(tileInfo));
    // Currently we always do full tile paints.
    UNUSED_PARAM(tileDirtyRect);

    if (m_enqueuedTileRenders.contains(tileInfo))
        return;

    // If we have a tile, we can just paint it.
    if (m_rendereredTiles.contains(tileInfo))
        return;

    enqueuePaintWithClip(tileInfo, tileRect);
}

void AsyncPDFRenderer::willRemoveTile(TileGridIndex gridIndex, TileIndex tileIndex)
{
    auto tileInfo = TileForGrid { gridIndex, tileIndex };

    m_enqueuedTileRenders.remove(tileInfo);
    m_rendereredTiles.remove(tileInfo);
}

void AsyncPDFRenderer::willRepaintAllTiles(TileGridIndex)
{
    // FIXME: Could remove per-grid, when we use more than one grid.
    m_enqueuedTileRenders.clear();
    m_rendereredTiles.clear();
}

void AsyncPDFRenderer::clearCachedTiles()
{
    m_enqueuedTileRenders.clear();
    m_rendereredTiles.clear();
}

void AsyncPDFRenderer::enqueuePaintWithClip(const TileForGrid& tileInfo, const FloatRect& tileRect)
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto pdfDocument = plugin->pdfDocument();
    if (!pdfDocument) {
        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueueLayerPaint - document is null, bailing");
        return;
    }

    auto pageCoverage = plugin->pageCoverageForRect(tileRect);
    if (pageCoverage.pages.isEmpty())
        return;

    m_enqueuedTileRenders.add(tileInfo, pageCoverage);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueuePaintWithClip for tile at " << tileInfo.tileIndex);

    m_paintingWorkQueue->dispatch([protectedThis = Ref { *this }, pdfDocument = WTFMove(pdfDocument), tileInfo, tileRect, pageCoverage]() mutable {
        protectedThis->paintTileOnWorkQueue(WTFMove(pdfDocument), tileInfo, tileRect, pageCoverage);
    });
}

void AsyncPDFRenderer::paintTileOnWorkQueue(RetainPtr<PDFDocument>&& pdfDocument, const TileForGrid& tileInfo, const FloatRect& tileRect, const PDFPageCoverage& pageCoverage)
{
    assertIsCurrent(m_paintingWorkQueue);

    auto tileBuffer = ImageBuffer::create(tileRect.size(), RenderingPurpose::Unspecified, pageCoverage.deviceScaleFactor, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!tileBuffer)
        return;

    // transform the origin

    paintPDFIntoBuffer(WTFMove(pdfDocument), *tileBuffer, tileInfo, tileRect, pageCoverage);

    // This is really a no-op (but only works if there's just one ref).
    auto bufferCopy = ImageBuffer::sinkIntoBufferForDifferentThread(WTFMove(tileBuffer));
    ASSERT(bufferCopy);

    transferBufferToMainThread(WTFMove(bufferCopy), tileInfo, tileRect);
}

void AsyncPDFRenderer::paintPDFIntoBuffer(RetainPtr<PDFDocument>&& pdfDocument, Ref<ImageBuffer> imageBuffer, const TileForGrid& tileInfo, const FloatRect& tileRect, const PDFPageCoverage& pageCoverage)
{
    assertIsCurrent(m_paintingWorkQueue);

    auto& context = imageBuffer->context();

    auto stateSaver = GraphicsContextStateSaver(context);

    context.translate(FloatPoint { -tileRect.location() });
    context.scale(pageCoverage.documentScale);

    for (auto& pageInfo : pageCoverage.pages) {
        RetainPtr pdfPage = [pdfDocument pageAtIndex:pageInfo.pageIndex];
        if (!pdfPage)
            continue;

        auto destinationRect = pageInfo.pageBounds;

        auto pageStateSaver = GraphicsContextStateSaver(context);
        context.clip(destinationRect);
        context.fillRect(destinationRect, Color::white);

        // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
        // from this page's drawing origin.
        context.translate(destinationRect.minXMaxYCorner());
        context.scale({ 1, -1 });

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer: painting PDF page " << pageInfo.pageIndex << " into rect " << destinationRect << " with clip " << tileRect);
        [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
    }
}

void AsyncPDFRenderer::transferBufferToMainThread(RefPtr<ImageBuffer>&& imageBuffer, const TileForGrid& tileInfo, const FloatRect& tileRect)
{
    assertIsCurrent(m_paintingWorkQueue);

    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, imageBuffer = WTFMove(imageBuffer), tileInfo, tileRect]() {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!protectedThis->m_pdfContentsLayer)
            return;

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::transferBufferToMainThread - got results for tile at " << tileInfo.tileIndex << " (" << protectedThis->m_rendereredTiles.size() << " tiles in cache)");

        // If the request for this tile has been revoked, don't cache it.
        if (!protectedThis->m_enqueuedTileRenders.contains(tileInfo))
            return;

        auto bufferAndClip = BufferAndClip { WTFMove(imageBuffer), tileRect };
        protectedThis->m_rendereredTiles.add(tileInfo, WTFMove(bufferAndClip));

        protectedThis->m_pdfContentsLayer->setNeedsDisplayInRect(tileRect);
    });
}

bool AsyncPDFRenderer::paintTileForClip(GraphicsContext& context, const FloatRect& clip)
{
    ASSERT(isMainRunLoop());

    bool paintedATile = false;

    for (auto& keyValuePair : m_rendereredTiles) {
        auto& tileInfo = keyValuePair.key;
        auto& bufferAndClip = keyValuePair.value;

        if (!clip.intersects(bufferAndClip.tileClip))
            continue;

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintTileForClip - painting tile for " << tileInfo.tileIndex << " with clip " << bufferAndClip.tileClip);

        auto stateSaver = GraphicsContextStateSaver(context);

        context.drawImageBuffer(*bufferAndClip.buffer, bufferAndClip.tileClip.location());
        paintedATile = true;

        m_enqueuedTileRenders.remove(tileInfo);
    }

    // FIXME: Ideally return true if we covered the entire dirty rect.
    return paintedATile;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
