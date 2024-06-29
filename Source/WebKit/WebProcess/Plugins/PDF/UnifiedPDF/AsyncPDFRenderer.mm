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
#include <wtf/NumberOfCores.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

Ref<AsyncPDFRenderer> AsyncPDFRenderer::create(UnifiedPDFPlugin& plugin)
{
    return adoptRef(*new AsyncPDFRenderer(plugin));
}

// m_maxConcurrentTileRenders is a trade-off between rendering multiple tiles concurrently, and getting backed up because
// in-flight renders can't be canceled when resizing or zooming makes them invalid.
AsyncPDFRenderer::AsyncPDFRenderer(UnifiedPDFPlugin& plugin)
    : m_plugin(plugin)
    , m_paintingWorkQueue(ConcurrentWorkQueue::create("WebKit: PDF Painting Work Queue"_s, WorkQueue::QOS::UserInteractive)) // Maybe make this concurrent?
    , m_maxConcurrentTileRenders(std::clamp(WTF::numberOfProcessorCores() - 2, 4, 16))
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

void AsyncPDFRenderer::releaseMemory()
{
    auto* tiledBacking = m_pdfContentsLayer->tiledBacking();
    if (!tiledBacking)
        return;

#if !LOG_DISABLED
    auto oldPagePreviewCount = m_pagePreviews.size();
#endif
    // Ideally we'd be able to make the ImageBuffer memory volatile which would eliminate the need for this callback: webkit.org/b/274878
    removePagePreviewsOutsideCoverageRect(tiledBacking->coverageRect());

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::releaseMemory - reduced page preview count from " << oldPagePreviewCount << " to " << m_pagePreviews.size());
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

void AsyncPDFRenderer::generatePreviewImageForPage(PDFDocumentLayout::PageIndex pageIndex, float scale)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    RetainPtr pdfDocument = plugin->pdfDocument();
    if (!pdfDocument)
        return;

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::generatePreviewImageForPage " << pageIndex << " (have request " << m_enqueuedPagePreviews.contains(pageIndex) << ")");

    if (m_enqueuedPagePreviews.contains(pageIndex))
        return;

    auto pageBounds = plugin->layoutBoundsForPageAtIndex(pageIndex);
    pageBounds.setLocation({ });

    auto pagePreviewRequest = PagePreviewRequest { pageIndex, pageBounds, scale };
    m_enqueuedPagePreviews.set(pageIndex, pagePreviewRequest);

    m_paintingWorkQueue->dispatch([protectedThis = Ref { *this }, pdfDocument = WTFMove(pdfDocument), pagePreviewRequest]() mutable {
        protectedThis->paintPagePreviewOnWorkQueue(WTFMove(pdfDocument), pagePreviewRequest);
    });
}

void AsyncPDFRenderer::removePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::removePreviewForPage " << pageIndex);

    m_enqueuedPagePreviews.remove(pageIndex);
    m_pagePreviews.remove(pageIndex);
}

void AsyncPDFRenderer::paintPagePreviewOnWorkQueue(RetainPtr<PDFDocument>&& pdfDocument, const PagePreviewRequest& pagePreviewRequest)
{
    ASSERT(!isMainRunLoop());

    auto pageImageBuffer = ImageBuffer::create(pagePreviewRequest.normalizedPageBounds.size(), RenderingPurpose::Unspecified, pagePreviewRequest.scale, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!pageImageBuffer)
        return;

    paintPDFPageIntoBuffer(WTFMove(pdfDocument), *pageImageBuffer, pagePreviewRequest.pageIndex, pagePreviewRequest.normalizedPageBounds);

    // This is really a no-op (but only works if there's just one ref).
    auto bufferCopy = ImageBuffer::sinkIntoBufferForDifferentThread(WTFMove(pageImageBuffer));
    ASSERT(bufferCopy);

    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, imageBuffer = WTFMove(bufferCopy), pagePreviewRequest]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->didCompletePagePreviewRender(WTFMove(imageBuffer), pagePreviewRequest);
    });
}

void AsyncPDFRenderer::didCompletePagePreviewRender(RefPtr<ImageBuffer>&& imageBuffer, const PagePreviewRequest& pagePreviewRequest)
{
    ASSERT(isMainRunLoop());
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto pageIndex = pagePreviewRequest.pageIndex;
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompletePagePreviewRender for page " << pageIndex << " (have request " << m_enqueuedPagePreviews.contains(pageIndex) << ")");

    m_enqueuedPagePreviews.remove(pageIndex);
    m_pagePreviews.set(pageIndex, WTFMove(imageBuffer));
    plugin->didGeneratePreviewForPage(pageIndex);
}

RefPtr<WebCore::ImageBuffer> AsyncPDFRenderer::previewImageForPage(PDFDocumentLayout::PageIndex pageIndex) const
{
    return m_pagePreviews.get(pageIndex);
}

bool AsyncPDFRenderer::renderInfoIsValidForTile(const TileForGrid& tileInfo, const TileRenderInfo& renderInfo) const
{
    ASSERT(isMainRunLoop());
    if (!m_pdfContentsLayer)
        return false;

    auto* tiledBacking = m_pdfContentsLayer->tiledBacking();
    if (!tiledBacking)
        return false;

    auto currentTileRect = tiledBacking->rectForTile(tileInfo.tileIndex);
    auto currentRenderInfo = renderInfoForTile(tileInfo, currentTileRect);

    return renderInfo.equivalentForPainting(currentRenderInfo);
}

void AsyncPDFRenderer::willRepaintTile(TiledBacking&, TileGridIdentifier gridIdentifier, TileIndex tileIndex, const FloatRect& tileRect, const FloatRect& tileDirtyRect)
{
    auto tileInfo = TileForGrid { gridIdentifier, tileIndex };

    auto haveValidTile = [&](const TileForGrid& tileInfo) {
        auto it = m_rendereredTiles.find(tileInfo);
        if (it == m_rendereredTiles.end())
            return false;

        auto& renderInfo = it->value.tileInfo;
        if (renderInfo.tileRect != tileRect)
            return false;

        return renderInfoIsValidForTile(tileInfo, renderInfo);
    };

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRepaintTile " << tileInfo << " rect " << tileRect << " (dirty rect " << tileDirtyRect << ") - already queued "
        << m_currentValidTileRenders.contains(tileInfo) << " have cached tile " << m_rendereredTiles.contains(tileInfo) << " which is valid " << haveValidTile(tileInfo));

    // If we have a tile, we can just paint it.
    if (haveValidTile(tileInfo))
        return;

    m_rendereredTiles.remove(tileInfo);

    // Currently we always do full tile paints when the grid changes.
    UNUSED_PARAM(tileDirtyRect);
    enqueueTilePaintIfNecessary(tileInfo, tileRect);
}

void AsyncPDFRenderer::willRemoveTile(TiledBacking&, TileGridIdentifier gridIdentifier, TileIndex tileIndex)
{
    auto tileInfo = TileForGrid { gridIdentifier, tileIndex };

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRemoveTile " << tileInfo);

    m_requestWorkQueue.remove(tileInfo);
    m_currentValidTileRenders.remove(tileInfo);
    m_rendereredTiles.remove(tileInfo);
}

void AsyncPDFRenderer::willRepaintAllTiles(TiledBacking&, TileGridIdentifier)
{
    clearRequestsAndCachedTiles();
}

void AsyncPDFRenderer::coverageRectDidChange(TiledBacking&, const FloatRect& coverageRect)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto pageCoverage = plugin->pageCoverageForRect(coverageRect);
    auto pagePreviewScale = plugin->scaleForPagePreviews();

    for (auto& pageInfo : pageCoverage) {
        if (m_pagePreviews.contains(pageInfo.pageIndex))
            continue;

        generatePreviewImageForPage(pageInfo.pageIndex, pagePreviewScale);
    }

    if (!plugin->shouldCachePagePreviews())
        removePagePreviewsOutsideCoverageRect(coverageRect);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::coverageRectDidChange " << coverageRect << " " << pageCoverage << " - preview scale " << pagePreviewScale << " - have " << m_pagePreviews.size() << " page previews and " << m_enqueuedPagePreviews.size() << " enqueued");
}

void AsyncPDFRenderer::removePagePreviewsOutsideCoverageRect(const FloatRect& coverageRect)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto pageCoverage = plugin->pageCoverageForRect(coverageRect);

    PDFPageIndexSet unwantedPageIndices;
    for (auto pageIndex : m_pagePreviews.keys())
        unwantedPageIndices.add(pageIndex);

    for (auto& pageInfo : pageCoverage) {
        auto it = unwantedPageIndices.find(pageInfo.pageIndex);
        if (it != unwantedPageIndices.end()) {
            unwantedPageIndices.remove(it);
            continue;
        }
    }

    for (auto pageIndex : unwantedPageIndices)
        removePreviewForPage(pageIndex);
}

void AsyncPDFRenderer::tilingScaleFactorDidChange(TiledBacking&, float)
{
}

void AsyncPDFRenderer::didAddGrid(TiledBacking&, TileGridIdentifier)
{

}

void AsyncPDFRenderer::willRemoveGrid(WebCore::TiledBacking&, TileGridIdentifier gridIdentifier)
{
    m_rendereredTiles.removeIf([gridIdentifier](const auto& keyValuePair) {
        return keyValuePair.key.gridIdentifier == gridIdentifier;
    });

    m_currentValidTileRenders.removeIf([gridIdentifier](const auto& keyValuePair) {
        return keyValuePair.key.gridIdentifier == gridIdentifier;
    });

    Vector<TileForGrid> requestsToRemove;
    for (auto& tileRequests : m_requestWorkQueue) {
        if (tileRequests.gridIdentifier == gridIdentifier)
            requestsToRemove.append(tileRequests);
    }

    for (auto& tile : requestsToRemove)
        m_requestWorkQueue.remove(tile);
}

void AsyncPDFRenderer::clearRequestsAndCachedTiles()
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::clearRequestsAndCachedTiles");

    m_requestWorkQueue.clear();
    m_currentValidTileRenders.clear();
    m_rendereredTiles.clear();
}

AffineTransform AsyncPDFRenderer::tileToPaintingTransform(float tilingScaleFactor)
{
    float inversePageScale = 1 / tilingScaleFactor;
    return AffineTransform::makeScale({ inversePageScale, inversePageScale });
}

AffineTransform AsyncPDFRenderer::paintingToTileTransform(float tilingScaleFactor)
{
    return AffineTransform::makeScale({ tilingScaleFactor, tilingScaleFactor });
}

FloatRect AsyncPDFRenderer::convertTileRectToPaintingCoords(const FloatRect& tileRect, float pageScaleFactor)
{
    return tileToPaintingTransform(pageScaleFactor).mapRect(tileRect);
}

void AsyncPDFRenderer::enqueueTilePaintIfNecessary(const TileForGrid& tileInfo, const FloatRect& tileRect, const std::optional<FloatRect>& clipRect)
{
    auto renderInfo = renderInfoForTile(tileInfo, tileRect, clipRect);
    if (renderInfo.pageCoverage.pages.isEmpty())
        return;

    // If there's an existing partial render for this tile, we need to override it, uniting partial updates.
    // We can't support multiple full/partial renders per tile since we have no ordering guarantees on which finishes first.
    auto it = m_currentValidTileRenders.find(tileInfo);
    if (it != m_currentValidTileRenders.end()) {
        auto& existingRenderInfo = it->value.renderInfo;

        // If we already have a full tile paint pending, no need to start a new one.
        if (!existingRenderInfo.clipRect && !renderInfo.clipRect && existingRenderInfo.equivalentForPainting(renderInfo))
            return;

        if (renderInfo.clipRect) {
            if (existingRenderInfo.clipRect)
                renderInfo.clipRect->unite(*existingRenderInfo.clipRect);
            else {
                // If the earlier request was a full update, we need to be too.
                renderInfo.clipRect = { };
            }
        }
    }

    enqueuePaintWithClip(tileInfo, renderInfo);
}

auto AsyncPDFRenderer::renderInfoForTile(const TileForGrid& tileInfo, const FloatRect& tileRect, const std::optional<FloatRect>& clipRect) const -> TileRenderInfo
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return { };

    RetainPtr pdfDocument = plugin->pdfDocument();
    if (!pdfDocument) {
        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueueLayerPaint - document is null, bailing");
        return { };
    }

    auto tilingScaleFactor = 1.0f;
    if (auto* tiledBacking = m_pdfContentsLayer->tiledBacking())
        tilingScaleFactor = tiledBacking->tilingScaleFactor();

    auto paintingClipRect = convertTileRectToPaintingCoords(tileRect, tilingScaleFactor);
    auto pageCoverage = plugin->pageCoverageAndScalesForRect(paintingClipRect);

    return TileRenderInfo { tileRect, clipRect, pageCoverage };
}

void AsyncPDFRenderer::enqueuePaintWithClip(const TileForGrid& tileInfo, const TileRenderInfo& renderInfo)
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

    if (renderInfo.pageCoverage.pages.isEmpty())
        return;

    auto renderIdentifier = PDFTileRenderIdentifier::generate();
    m_currentValidTileRenders.set(tileInfo, TileRenderData { renderIdentifier, renderInfo });

    m_requestWorkQueue.appendOrMoveToLast(tileInfo);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueueTileRequest for tile " << tileInfo << " " << renderInfo.pageCoverage << " identifier " << renderIdentifier << " (" << m_requestWorkQueue.size() << " requests in queue)");

    serviceRequestQueue();
}

void AsyncPDFRenderer::serviceRequestQueue()
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    while (m_numConcurrentTileRenders < m_maxConcurrentTileRenders) {
        if (m_requestWorkQueue.isEmpty())
            break;

        TileForGrid tileInfo = m_requestWorkQueue.takeFirst();
        auto it = m_currentValidTileRenders.find(tileInfo);
        if (it == m_currentValidTileRenders.end())
            continue;

        TileRenderData renderData = it->value;

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::serviceRequestQueue - rendering tile " << tileInfo << " identifier " << renderData.renderIdentifier << " (" << m_numConcurrentTileRenders << " concurrent renders)");

        ++m_numConcurrentTileRenders;

        m_paintingWorkQueue->dispatch([protectedThis = Ref { *this }, pdfDocument = RetainPtr { plugin->pdfDocument() }, tileInfo, renderData]() mutable {
            protectedThis->paintTileOnWorkQueue(WTFMove(pdfDocument), tileInfo, renderData.renderInfo, renderData.renderIdentifier);
        });
    }

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::serviceRequestQueue() - " << m_numConcurrentTileRenders << " renders in flight, " << m_requestWorkQueue.size() << " in queue");
}

void AsyncPDFRenderer::paintTileOnWorkQueue(RetainPtr<PDFDocument>&& pdfDocument, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo, PDFTileRenderIdentifier renderIdentifier)
{
    ASSERT(!isMainRunLoop());

    auto bufferRect = renderInfo.clipRect.value_or(renderInfo.tileRect);
    auto tileBuffer = ImageBuffer::create(bufferRect.size(), RenderingPurpose::Unspecified, renderInfo.pageCoverage.deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!tileBuffer) {
        transferBufferToMainThread(nullptr, tileInfo, renderInfo, renderIdentifier);
        return;
    }

    paintPDFIntoBuffer(WTFMove(pdfDocument), *tileBuffer, tileInfo, renderInfo);

    // This is really a no-op (but only works if there's just one ref).
    auto bufferCopy = ImageBuffer::sinkIntoBufferForDifferentThread(WTFMove(tileBuffer));
    ASSERT(bufferCopy);

    transferBufferToMainThread(WTFMove(bufferCopy), tileInfo, renderInfo, renderIdentifier);
}

void AsyncPDFRenderer::paintPDFIntoBuffer(RetainPtr<PDFDocument>&& pdfDocument, Ref<ImageBuffer> imageBuffer, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo)
{
    ASSERT(!isMainRunLoop());

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintPDFIntoBuffer for tile " << tileInfo);

    auto& context = imageBuffer->context();

    auto stateSaver = GraphicsContextStateSaver(context);

    auto bufferRect = renderInfo.clipRect.value_or(renderInfo.tileRect);
    context.translate(FloatPoint { -bufferRect.location() });

    context.fillRect(bufferRect, Color::white);

    if (m_showDebugBorders.load())
        context.fillRect(bufferRect, Color::green.colorWithAlphaByte(32));

    context.scale(renderInfo.pageCoverage.pdfDocumentScale * renderInfo.pageCoverage.tilingScaleFactor);

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

        LOG_WITH_STREAM(PDFAsyncRendering, stream << " tile " << tileInfo << " painting PDF page " << pageInfo.pageIndex << " into rect " << destinationRect << " with clip " << bufferRect);
        [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
    }
}

void AsyncPDFRenderer::paintPDFPageIntoBuffer(RetainPtr<PDFDocument>&& pdfDocument, Ref<WebCore::ImageBuffer> imageBuffer, PDFDocumentLayout::PageIndex pageIndex, const FloatRect& pageBounds)
{
    ASSERT(!isMainRunLoop());

    auto& context = imageBuffer->context();

    auto stateSaver = GraphicsContextStateSaver(context);

    RetainPtr pdfPage = [pdfDocument pageAtIndex:pageIndex];
    if (!pdfPage)
        return;

    auto pageStateSaver = GraphicsContextStateSaver(context);
    auto destinationRect = pageBounds;

    if (m_showDebugBorders.load())
        context.fillRect(destinationRect, Color::orange.colorWithAlphaByte(32));

    // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
    // from this page's drawing origin.
    context.translate(destinationRect.minXMaxYCorner());
    context.scale({ 1, -1 });

    CGContextSetShouldSubpixelQuantizeFonts(context.platformContext(), false);
    CGContextSetAllowsFontSubpixelPositioning(context.platformContext(), true);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintPDFPageIntoBuffer - painting page " << pageIndex);
    [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
}

void AsyncPDFRenderer::transferBufferToMainThread(RefPtr<ImageBuffer>&& imageBuffer, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo, PDFTileRenderIdentifier renderIdentifier)
{
    ASSERT(!isMainRunLoop());

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::transferBufferToMainThread for tile " << tileInfo);

    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, imageBuffer = WTFMove(imageBuffer), tileInfo, renderInfo, renderIdentifier]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!protectedThis->m_pdfContentsLayer)
            return;

        bool haveBuffer = !!imageBuffer;
        protectedThis->didCompleteTileRender(WTFMove(imageBuffer), tileInfo, renderInfo, renderIdentifier);

        if (haveBuffer) {
            auto paintingClipRect = convertTileRectToPaintingCoords(renderInfo.tileRect, renderInfo.pageCoverage.tilingScaleFactor);
            protectedThis->m_pdfContentsLayer->setNeedsDisplayInRect(paintingClipRect);
        }
    });
}

// imageBuffer may be null if allocation on the decoding thread failed.
void AsyncPDFRenderer::didCompleteTileRender(RefPtr<WebCore::ImageBuffer>&& imageBuffer, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo, PDFTileRenderIdentifier renderIdentifier)
{
    bool requestWasValid = [&]() {
        auto it = m_currentValidTileRenders.find(tileInfo);
        if (it == m_currentValidTileRenders.end())
            return false;

        if (it->value.renderIdentifier == renderIdentifier) {
            m_currentValidTileRenders.remove(it);
            return true;
        }

        return false;
    }();

    ASSERT(m_numConcurrentTileRenders);
    --m_numConcurrentTileRenders;
    serviceRequestQueue();

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileRender - got results for tile at " << tileInfo << " clip " << renderInfo.clipRect << " ident " << renderIdentifier
        << " (" << m_rendereredTiles.size() << " tiles in cache). Request revoked " << !requestWasValid);

    if (!requestWasValid)
        return;

    if (!imageBuffer)
        return;

    // State may have changed since we started the tile paint; check that it's still valid.
    if (!renderInfoIsValidForTile(tileInfo, renderInfo))
        return;

    if (renderInfo.clipRect) {
        auto renderedTilesIt = m_rendereredTiles.find(tileInfo);
        if (renderedTilesIt == m_rendereredTiles.end()) {
            LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileRender - tile to be updated " << tileInfo << " has been removed");
            return;
        }

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileRender - updating tile " << tileInfo << " in rect " << *renderInfo.clipRect);

        RefPtr existingBuffer = renderedTilesIt->value.buffer;
        auto& context = existingBuffer->context();
        auto stateSaver = GraphicsContextStateSaver(context);

        auto destinationRect = *renderInfo.clipRect;
        destinationRect.moveBy(-renderInfo.tileRect.location());

        context.drawImageBuffer(*imageBuffer, destinationRect, { CompositeOperator::Copy });
    } else {
        auto renderedTileInfo = RenderedTile { WTFMove(imageBuffer), renderInfo };
        m_rendereredTiles.set(tileInfo, WTFMove(renderedTileInfo));
    }
}

bool AsyncPDFRenderer::paintTilesForPage(GraphicsContext& context, float documentScale, const FloatRect& clipRect, const FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex pageIndex)
{
    ASSERT(isMainRunLoop());

    bool paintedATile = false;

    auto tilingScaleFactor = 1.0f;
    if (auto* tiledBacking = m_pdfContentsLayer->tiledBacking())
        tilingScaleFactor = tiledBacking->tilingScaleFactor();

    // This scale takes us from "painting" coordinates into the coordinate system of the tile grid,
    // so we can paint tiles directly.
    auto scaleTransform = tileToPaintingTransform(tilingScaleFactor);
    {
        auto stateSaver = GraphicsContextStateSaver(context);
        context.concatCTM(scaleTransform);

        for (auto& keyValuePair : m_rendereredTiles) {
            auto& renderedTile = keyValuePair.value;

            auto tileClipInPaintingCoordinates = scaleTransform.mapRect(renderedTile.tileInfo.tileRect);
            if (!pageBoundsInPaintingCoordinates.intersects(tileClipInPaintingCoordinates))
                continue;

            if (!tileClipInPaintingCoordinates.intersects(clipRect))
                continue;

            LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintTilesForPage " << pageBoundsInPaintingCoordinates  << " - painting tile for " << keyValuePair.key << " with clip " << renderedTile.tileInfo.tileRect << " tiling scale " << tilingScaleFactor);

            context.drawImageBuffer(*renderedTile.buffer, renderedTile.tileInfo.tileRect.location());
            paintedATile = true;
        }
    }

    return paintedATile;
}

void AsyncPDFRenderer::paintPagePreview(GraphicsContext& context, const FloatRect& clipRect, const FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex pageIndex)
{
    RefPtr imageBuffer = previewImageForPage(pageIndex);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintPagePreview for page " << pageIndex  << " - buffer " << imageBuffer);

    if (imageBuffer)
        context.drawImageBuffer(*imageBuffer, pageBoundsInPaintingCoordinates, pageBoundsInPaintingCoordinates);
}

void AsyncPDFRenderer::invalidateTilesForPaintingRect(float pageScaleFactor, const FloatRect& paintingRect)
{
    auto scaleTransform = tileToPaintingTransform(pageScaleFactor);

    m_rendereredTiles.removeIf([&](auto& entry) {
        auto& renderedTile = entry.value;

        auto tileClipInPaintingCoordinates = scaleTransform.mapRect(renderedTile.tileInfo.tileRect);
        bool result = paintingRect.intersects(tileClipInPaintingCoordinates);
        if (result)
            LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::invalidateTilesForPaintingRect " << paintingRect << " - removing tile " << entry.key);

        return result;
    });
}

void AsyncPDFRenderer::pdfContentChangedInRect(float pageScaleFactor, const FloatRect& paintingRect)
{
    // FIXME: If our platform does not support partial updates (supportsPartialRepaint() is false) then this should behave
    // identically to invalidateTilesForPaintingRect().

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::pdfContentChangedInRect " << paintingRect);

    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto pageCoverage = plugin->pageCoverageForRect(paintingRect);
    if (pageCoverage.isEmpty())
        return;

    RetainPtr pdfDocument = plugin->pdfDocument();
    if (!pdfDocument)
        return;

    auto toTileTransform = paintingToTileTransform(pageScaleFactor);
    auto paintingRectInTileCoordinates = toTileTransform.mapRect(paintingRect);

    for (auto& keyValuePair : m_rendereredTiles) {
        auto& tileInfo = keyValuePair.key;
        auto& renderedTile = keyValuePair.value;

        if (!paintingRectInTileCoordinates.intersects(renderedTile.tileInfo.tileRect))
            continue;

        std::optional<FloatRect> clipRect = intersection(renderedTile.tileInfo.tileRect, paintingRectInTileCoordinates);
        if (*clipRect == renderedTile.tileInfo.tileRect)
            clipRect = { };
        enqueueTilePaintIfNecessary(tileInfo, renderedTile.tileInfo.tileRect, clipRect);
    }

    auto pagePreviewScale = plugin->scaleForPagePreviews();
    for (auto& pageInfo : pageCoverage)
        generatePreviewImageForPage(pageInfo.pageIndex, pagePreviewScale);
}

TextStream& operator<<(TextStream& ts, const TileForGrid& tileInfo)
{
    ts << "[" << tileInfo.gridIdentifier << ":" << tileInfo.tileIndex << "]";
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
