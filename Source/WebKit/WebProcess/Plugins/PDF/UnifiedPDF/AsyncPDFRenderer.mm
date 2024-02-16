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
#include "PDFPageCoverage.h"
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
    , m_paintingWorkQueue(ConcurrentWorkQueue::create("WebKit: PDF Painting Work Queue"_s, WorkQueue::QOS::UserInteractive)) // Maybe make this concurrent?
    , m_currentConfigurationIdentifier(PDFConfigurationIdentifier::generate())
{
}

AsyncPDFRenderer::~AsyncPDFRenderer()
{
    teardown();
}

void AsyncPDFRenderer::teardown()
{
    if (!m_pdfContentsLayer)
        return;

    if (auto* tiledBacking = m_pdfContentsLayer->tiledBacking())
        tiledBacking->setClient(nullptr);
}

void AsyncPDFRenderer::setupWithLayer(GraphicsLayer& layer)
{
    m_pdfContentsLayer = &layer;

    if (auto* tiledBacking = m_pdfContentsLayer->tiledBacking())
        tiledBacking->setClient(this);
}

void AsyncPDFRenderer::setShowDebugBorders(bool showDebugBorders)
{
    m_showDebugBorders = showDebugBorders;
}

void AsyncPDFRenderer::willRepaintTile(TileGridIndex gridIndex, TileIndex tileIndex, const FloatRect& tileRect, const FloatRect& tileDirtyRect)
{
    auto tileInfo = TileForGrid { gridIndex, tileIndex };

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRepaintTile " << tileInfo << " rect " << tileRect << " (dirty rect " << tileDirtyRect << ") - already queued "
        << m_enqueuedTileRenders.contains(tileInfo) << " have cached tile " << m_rendereredTiles.contains(tileInfo));

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

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRemoveTile " << tileInfo);

    m_enqueuedTileRenders.remove(tileInfo);
    m_rendereredTiles.remove(tileInfo);
}

void AsyncPDFRenderer::willRepaintAllTiles(TileGridIndex)
{
    // FIXME: Could remove per-grid, when we use more than one grid.
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRepaintAllTiles");

    m_enqueuedTileRenders.clear();
    m_rendereredTiles.clear();
}

void AsyncPDFRenderer::layoutConfigurationChanged()
{
    m_currentConfigurationIdentifier = PDFConfigurationIdentifier::generate();
    clearRequestsAndCachedTiles();
}

void AsyncPDFRenderer::clearRequestsAndCachedTiles()
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::clearRequestsAndCachedTiles");

    m_enqueuedTileRenders.clear();
    m_rendereredTiles.clear();
}

AffineTransform AsyncPDFRenderer::tileToPaintingTransform(float pageScaleFactor)
{
    float inversePageScale = 1 / pageScaleFactor;
    return AffineTransform::makeScale({ inversePageScale, inversePageScale });
}

FloatRect AsyncPDFRenderer::convertTileRectToPaintingCoords(const FloatRect& tileRect, float pageScaleFactor)
{
    return tileToPaintingTransform(pageScaleFactor).mapRect(tileRect);
}

void AsyncPDFRenderer::enqueuePaintWithClip(const TileForGrid& tileInfo, const FloatRect& tileRect)
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    RetainPtr pdfDocument = plugin->pdfDocument();
    if (!pdfDocument) {
        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueueLayerPaint - document is null, bailing");
        return;
    }

    auto paintingClipRect = convertTileRectToPaintingCoords(tileRect, plugin->scaleFactor());
    auto pageCoverage = plugin->pageCoverageForRect(paintingClipRect);
    if (pageCoverage.pages.isEmpty())
        return;

    auto tileRenderInfo = TileRenderInfo { pageCoverage, m_currentConfigurationIdentifier };
    m_enqueuedTileRenders.add(tileInfo, tileRenderInfo);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueuePaintWithClip for tile " << tileInfo << " " << tileRenderInfo.pageCoverage);

    m_paintingWorkQueue->dispatch([protectedThis = Ref { *this }, pdfDocument = WTFMove(pdfDocument), tileInfo, tileRect, tileRenderInfo]() mutable {
        protectedThis->paintTileOnWorkQueue(WTFMove(pdfDocument), tileInfo, tileRect, tileRenderInfo);
    });
}

void AsyncPDFRenderer::paintTileOnWorkQueue(RetainPtr<PDFDocument>&& pdfDocument, const TileForGrid& tileInfo, const FloatRect& tileRect, const TileRenderInfo& renderInfo)
{
    ASSERT(!isMainRunLoop());

    // FIXME: We should take a lock and check m_enqueuedTileRenders here, since we don't want to spend time painting a tile which will only be thrown away.

    auto tileBuffer = ImageBuffer::create(tileRect.size(), RenderingPurpose::Unspecified, renderInfo.pageCoverage.deviceScaleFactor, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!tileBuffer)
        return;

    paintPDFIntoBuffer(WTFMove(pdfDocument), *tileBuffer, tileInfo, tileRect, renderInfo);

    // This is really a no-op (but only works if there's just one ref).
    auto bufferCopy = ImageBuffer::sinkIntoBufferForDifferentThread(WTFMove(tileBuffer));
    ASSERT(bufferCopy);

    transferBufferToMainThread(WTFMove(bufferCopy), tileInfo, tileRect, renderInfo);
}

void AsyncPDFRenderer::paintPDFIntoBuffer(RetainPtr<PDFDocument>&& pdfDocument, Ref<ImageBuffer> imageBuffer, const TileForGrid& tileInfo, const FloatRect& tileRect, const TileRenderInfo& renderInfo)
{
    ASSERT(!isMainRunLoop());

    auto& context = imageBuffer->context();

    auto stateSaver = GraphicsContextStateSaver(context);

    context.translate(FloatPoint { -tileRect.location() });

    if (m_showDebugBorders.load())
        context.fillRect(tileRect, Color::green.colorWithAlphaByte(32));

    context.scale(renderInfo.pageCoverage.pdfDocumentScale * renderInfo.pageCoverage.pageScaleFactor);

    for (auto& pageInfo : renderInfo.pageCoverage.pages) {
        RetainPtr pdfPage = [pdfDocument pageAtIndex:pageInfo.pageIndex];
        if (!pdfPage)
            continue;

        auto destinationRect = pageInfo.pageBounds;

        auto pageStateSaver = GraphicsContextStateSaver(context);
        context.clip(destinationRect);

        // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
        // from this page's drawing origin.
        context.translate(destinationRect.minXMaxYCorner());
        context.scale({ 1, -1 });

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer: tile " << tileInfo << " painting PDF page " << pageInfo.pageIndex << " into rect " << destinationRect << " with clip " << tileRect);
        [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
    }
}

void AsyncPDFRenderer::transferBufferToMainThread(RefPtr<ImageBuffer>&& imageBuffer, const TileForGrid& tileInfo, const FloatRect& tileRect, const TileRenderInfo& renderInfo)
{
    ASSERT(!isMainRunLoop());

    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, imageBuffer = WTFMove(imageBuffer), tileInfo, tileRect, renderInfo]() {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!protectedThis->m_pdfContentsLayer)
            return;

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::transferBufferToMainThread - got results for tile at " << tileInfo
            << " (" << protectedThis->m_rendereredTiles.size() << " tiles in cache). Request removed " << !protectedThis->m_enqueuedTileRenders.contains(tileInfo)
            << " configuration changed " << (renderInfo.configurationIdentifier != protectedThis->m_currentConfigurationIdentifier));

        // If the request for this tile has been revoked, don't cache it.
        if (!protectedThis->m_enqueuedTileRenders.contains(tileInfo))
            return;

        // Configuration has changed.
        if (renderInfo.configurationIdentifier != protectedThis->m_currentConfigurationIdentifier) {
            auto it = protectedThis->m_enqueuedTileRenders.find(tileInfo);
            if (it != protectedThis->m_enqueuedTileRenders.end() && it->value.configurationIdentifier == renderInfo.configurationIdentifier)
                protectedThis->m_enqueuedTileRenders.remove(it);
            return;
        }

        auto bufferAndClip = BufferAndClip { WTFMove(imageBuffer), tileRect, protectedThis->m_currentConfigurationIdentifier };
        protectedThis->m_rendereredTiles.add(tileInfo, WTFMove(bufferAndClip));

        auto paintingClipRect = convertTileRectToPaintingCoords(tileRect, renderInfo.pageCoverage.pageScaleFactor);
        protectedThis->m_pdfContentsLayer->setNeedsDisplayInRect(paintingClipRect);
    });
}

bool AsyncPDFRenderer::paintTilesForPaintingRect(GraphicsContext& context, float pageScaleFactor, const FloatRect& destinationRect)
{
    ASSERT(isMainRunLoop());

    bool paintedATile = false;

    auto stateSaver = GraphicsContextStateSaver(context);

    // This scale takes us from "painting" coordinates into the coordinate system of the tile grid,
    // so we can paint tiles directly.

    auto scaleTransform = tileToPaintingTransform(pageScaleFactor);
    context.concatCTM(scaleTransform);

    for (auto& keyValuePair : m_rendereredTiles) {
        auto& tileInfo = keyValuePair.key;
        auto& bufferAndClip = keyValuePair.value;

        m_enqueuedTileRenders.remove(tileInfo);

        // FIXME: if we stored PDFPageCoverage we could skip non-relevant tiles

        auto tileClipInPaintingCoordinates = scaleTransform.mapRect(bufferAndClip.tileClip);
        if (!destinationRect.intersects(tileClipInPaintingCoordinates))
            continue;

        if (bufferAndClip.configurationIdentifier != m_currentConfigurationIdentifier) {
            if (m_showDebugBorders.load())
                context.fillRect(bufferAndClip.tileClip, Color::orange.colorWithAlphaByte(32));
            continue;
        }

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintTilesForPaintingRect " << destinationRect  << " - painting tile for " << tileInfo << " with clip " << bufferAndClip.tileClip << " scale " << pageScaleFactor);

        context.drawImageBuffer(*bufferAndClip.buffer, bufferAndClip.tileClip.location());
        paintedATile = true;
    }

    // FIXME: Ideally return true if we covered the entire dirty rect.
    return paintedATile;
}

void AsyncPDFRenderer::invalidateTilesForPaintingRect(float pageScaleFactor, const FloatRect& paintingRect)
{
    auto scaleTransform = tileToPaintingTransform(pageScaleFactor);

    m_rendereredTiles.removeIf([&](auto& entry) {
        auto& tileInfo = entry.key;
        auto& bufferAndClip = entry.value;

        auto tileClipInPaintingCoordinates = scaleTransform.mapRect(bufferAndClip.tileClip);
        bool result = paintingRect.intersects(tileClipInPaintingCoordinates);
        if (result)
            LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::invalidateTilesForPaintingRect " << paintingRect << " - removing tile " << tileInfo);

        return result;
    });
}

TextStream& operator<<(TextStream& ts, const TileForGrid& tileInfo)
{
    ts << "[" << tileInfo.gridIndex << ":" << tileInfo.tileIndex << "]";
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
