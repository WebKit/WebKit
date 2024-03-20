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
    m_paintingWorkQueue->dispatch([protectedThis = Ref { *this }, pdfDocument = WTFMove(pdfDocument), pagePreviewRequest]() mutable {
        protectedThis->paintPagePreviewOnWorkQueue(WTFMove(pdfDocument), pagePreviewRequest);
    });
}

void AsyncPDFRenderer::removePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::removePreviewForPage " << pageIndex);

    // We could use a purgeable cache here.
    m_enqueuedPagePreviews.remove(pageIndex);
    m_pagePreviews.remove(pageIndex);
}

void AsyncPDFRenderer::paintPagePreviewOnWorkQueue(RetainPtr<PDFDocument>&& pdfDocument, const PagePreviewRequest& pagePreviewRequest)
{
    ASSERT(!isMainRunLoop());

    auto pageImageBuffer = ImageBuffer::create(pagePreviewRequest.normalizedPageBounds.size(), RenderingPurpose::Unspecified, pagePreviewRequest.scale, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
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

        RefPtr plugin = protectedThis->m_plugin.get();
        if (!plugin)
            return;

        protectedThis->m_pagePreviews.set(pagePreviewRequest.pageIndex, WTFMove(imageBuffer));
        plugin->didGeneratePreviewForPage(pagePreviewRequest.pageIndex);
    });
}

RefPtr<WebCore::ImageBuffer> AsyncPDFRenderer::previewImageForPage(PDFDocumentLayout::PageIndex pageIndex) const
{
    return m_pagePreviews.get(pageIndex);
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

void AsyncPDFRenderer::coverageRectDidChange(const FloatRect& coverageRect)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto pageCoverage = plugin->pageCoverageForRect(coverageRect);
    auto pagePreviewScale = plugin->scaleForPagePreviews();

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::coverageRectDidChange " << coverageRect << " " << pageCoverage << " - preview scale " << pagePreviewScale);

    PDFPageIndexSet unwantedPageIndices;
    for (auto pageIndex : m_pagePreviews.keys())
        unwantedPageIndices.add(pageIndex);

    for (auto& pageInfo : pageCoverage.pages) {
        auto it = unwantedPageIndices.find(pageInfo.pageIndex);
        if (it != unwantedPageIndices.end()) {
            unwantedPageIndices.remove(it);
            continue;
        }

        generatePreviewImageForPage(pageInfo.pageIndex, pagePreviewScale);
    }

    for (auto pageIndex : unwantedPageIndices)
        removePreviewForPage(pageIndex);
}

void AsyncPDFRenderer::tilingScaleFactorDidChange(float)
{
    layoutConfigurationChanged();
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

    auto tilingScaleFactor = 1.0f;
    if (auto* tiledBacking = m_pdfContentsLayer->tiledBacking())
        tilingScaleFactor = tiledBacking->tilingScaleFactor();

    auto paintingClipRect = convertTileRectToPaintingCoords(tileRect, tilingScaleFactor);
    auto pageCoverage = plugin->pageCoverageForRect(paintingClipRect);
    if (pageCoverage.pages.isEmpty())
        return;

    auto tileRenderInfo = TileRenderInfo { tileRect, { }, pageCoverage, m_currentConfigurationIdentifier };
    m_enqueuedTileRenders.add(tileInfo, tileRenderInfo);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueuePaintWithClip for tile " << tileInfo << " " << tileRenderInfo.pageCoverage);

    m_paintingWorkQueue->dispatch([protectedThis = Ref { *this }, pdfDocument = WTFMove(pdfDocument), tileInfo, tileRenderInfo]() mutable {
        protectedThis->paintTileOnWorkQueue(WTFMove(pdfDocument), tileInfo, tileRenderInfo, TileRenderRequestType::NewTile);
    });
}

void AsyncPDFRenderer::paintTileOnWorkQueue(RetainPtr<PDFDocument>&& pdfDocument, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo, TileRenderRequestType requestType)
{
    ASSERT(!isMainRunLoop());

    // FIXME: We should take a lock and check m_enqueuedTileRenders here, since we don't want to spend time painting a tile which will only be thrown away.

    auto bufferRect = renderInfo.clipRect.value_or(renderInfo.tileRect);
    auto tileBuffer = ImageBuffer::create(bufferRect.size(), RenderingPurpose::Unspecified, renderInfo.pageCoverage.deviceScaleFactor, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!tileBuffer)
        return;

    paintPDFIntoBuffer(WTFMove(pdfDocument), *tileBuffer, tileInfo, renderInfo);

    // This is really a no-op (but only works if there's just one ref).
    auto bufferCopy = ImageBuffer::sinkIntoBufferForDifferentThread(WTFMove(tileBuffer));
    ASSERT(bufferCopy);

    transferBufferToMainThread(WTFMove(bufferCopy), tileInfo, renderInfo, requestType);
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

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer: tile " << tileInfo << " painting PDF page " << pageInfo.pageIndex << " into rect " << destinationRect << " with clip " << bufferRect);
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

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintPDFPageIntoBuffer - painting page " << pageIndex);
    [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
}


void AsyncPDFRenderer::transferBufferToMainThread(RefPtr<ImageBuffer>&& imageBuffer, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo, TileRenderRequestType requestType)
{
    ASSERT(!isMainRunLoop());

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::transferBufferToMainThread for tile " << tileInfo);

    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, imageBuffer = WTFMove(imageBuffer), tileInfo, renderInfo, requestType]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!protectedThis->m_pdfContentsLayer)
            return;

        switch (requestType) {
        case TileRenderRequestType::NewTile:
            protectedThis->didCompleteNewTileRender(WTFMove(imageBuffer), tileInfo, renderInfo);
            break;
        case TileRenderRequestType::TileUpdate:
            protectedThis->didCompleteTileUpdateRender(WTFMove(imageBuffer), tileInfo, renderInfo);
            break;
        }

        auto paintingClipRect = convertTileRectToPaintingCoords(renderInfo.tileRect, renderInfo.pageCoverage.tilingScaleFactor);
        protectedThis->m_pdfContentsLayer->setNeedsDisplayInRect(paintingClipRect);
    });
}

void AsyncPDFRenderer::didCompleteNewTileRender(RefPtr<WebCore::ImageBuffer>&& imageBuffer, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteNewTileRender - got results for tile at " << tileInfo
        << " (" << m_rendereredTiles.size() << " tiles in cache). Request removed " << !m_enqueuedTileRenders.contains(tileInfo)
        << " configuration changed " << (renderInfo.configurationIdentifier != m_currentConfigurationIdentifier));

    // If the request for this tile has been revoked, don't cache it.
    if (!m_enqueuedTileRenders.contains(tileInfo))
        return;

    // Configuration has changed.
    if (renderInfo.configurationIdentifier != m_currentConfigurationIdentifier) {
        auto it = m_enqueuedTileRenders.find(tileInfo);
        if (it != m_enqueuedTileRenders.end() && it->value.configurationIdentifier == renderInfo.configurationIdentifier)
            m_enqueuedTileRenders.remove(it);
        return;
    }

    auto renderedTileInfo = RenderedTile { WTFMove(imageBuffer), renderInfo };
    m_rendereredTiles.set(tileInfo, WTFMove(renderedTileInfo));
}

void AsyncPDFRenderer::didCompleteTileUpdateRender(RefPtr<WebCore::ImageBuffer>&& imageBuffer, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileUpdateRender - got results for tile at " << tileInfo
        << " (" << m_rendereredTiles.size() << " tiles in cache). Request removed " << !m_enqueuedTileUpdates.contains(tileInfo)
        << " configuration changed " << (renderInfo.configurationIdentifier != m_currentConfigurationIdentifier));

    auto enqueuedTileUpdatesItr = m_enqueuedTileUpdates.find(tileInfo);

    // If the request for this tile has been revoked, don't cache it.
    if (enqueuedTileUpdatesItr == m_enqueuedTileUpdates.end() || enqueuedTileUpdatesItr->value.clipRect != renderInfo.clipRect)
        return;

    // Configuration has changed.
    if (renderInfo.configurationIdentifier != m_currentConfigurationIdentifier) {
        if (enqueuedTileUpdatesItr->value.configurationIdentifier == renderInfo.configurationIdentifier)
            m_enqueuedTileUpdates.remove(enqueuedTileUpdatesItr);
        return;
    }

    auto it = m_rendereredTiles.find(tileInfo);
    if (it == m_rendereredTiles.end()) {
        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileUpdateRender - tile to be updated " << tileInfo << " has been removed");
        return;
    }

    m_enqueuedTileUpdates.remove(tileInfo);

    if (renderInfo.clipRect) {
        RefPtr existingBuffer = it->value.buffer;

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileUpdateRender - updating tile " << tileInfo << " in rect " << *renderInfo.clipRect);

        auto& context = existingBuffer->context();
        auto stateSaver = GraphicsContextStateSaver(context);

        auto destinationRect = *renderInfo.clipRect;
        destinationRect.moveBy(-renderInfo.tileRect.location());

        context.drawImageBuffer(*imageBuffer, destinationRect, { CompositeOperator::Copy });
        return;
    }

    // We have a whole new tile. Just replace the existing one.
    auto renderedTileInfo = RenderedTile { WTFMove(imageBuffer), renderInfo };
    m_rendereredTiles.set(tileInfo, WTFMove(renderedTileInfo));
}

bool AsyncPDFRenderer::paintTilesForPage(GraphicsContext& context, float documentScale, const FloatRect& clipRect, const WebCore::FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex pageIndex)
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
            auto& tileInfo = keyValuePair.key;
            auto& renderedTile = keyValuePair.value;

            m_enqueuedTileRenders.remove(tileInfo);

            auto tileClipInPaintingCoordinates = scaleTransform.mapRect(renderedTile.tileInfo.tileRect);
            if (!pageBoundsInPaintingCoordinates.intersects(tileClipInPaintingCoordinates))
                continue;

            if (renderedTile.tileInfo.configurationIdentifier != m_currentConfigurationIdentifier) {
                if (m_showDebugBorders.load())
                    context.fillRect(renderedTile.tileInfo.tileRect, Color::orange.colorWithAlphaByte(32));
                continue;
            }

            LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintTilesForPage " << pageBoundsInPaintingCoordinates  << " - painting tile for " << tileInfo << " with clip " << renderedTile.tileInfo.tileRect << " tiling scale " << tilingScaleFactor);

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
        context.drawImageBuffer(*imageBuffer, pageBoundsInPaintingCoordinates);
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

void AsyncPDFRenderer::updateTilesForPaintingRect(float pageScaleFactor, const FloatRect& paintingRect)
{
    // FIXME: If our platform does not support partial updates (supportsPartialRepaint() is false) then this should behave
    // identically to invalidateTilesForPaintingRect().

    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto pageCoverage = plugin->pageCoverageForRect(paintingRect);
    if (pageCoverage.pages.isEmpty())
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

        auto it = m_enqueuedTileUpdates.find(tileInfo);
        auto tileClipRect = intersection(renderedTile.tileInfo.tileRect, paintingRectInTileCoordinates);

        if (it != m_enqueuedTileUpdates.end() && it->value.configurationIdentifier) {
            if (!it->value.clipRect || it->value.clipRect->contains(tileClipRect))
                return;

            tileClipRect.unite(it->value.clipRect.value());
        }

        auto tileRenderInfo = TileRenderInfo { renderedTile.tileInfo.tileRect, tileClipRect, pageCoverage, m_currentConfigurationIdentifier };
        m_enqueuedTileUpdates.set(tileInfo, tileRenderInfo);

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::updateTilesForPaintingRect " << paintingRect << " - enqueue update for tile " << tileInfo
            << " tile clip " << tileClipRect << " in " << renderedTile.tileInfo.tileRect);

        m_paintingWorkQueue->dispatch([protectedThis = Ref { *this }, pdfDocument, tileInfo, tileRenderInfo]() mutable {
            protectedThis->paintTileOnWorkQueue(WTFMove(pdfDocument), tileInfo, tileRenderInfo, TileRenderRequestType::TileUpdate);
        });

        // FIXME: We also need to update the page previews, but probably after a slight delay so they don't compete with tile rendering. webkit.org/b/270040
    }
}

TextStream& operator<<(TextStream& ts, const TileForGrid& tileInfo)
{
    ts << "[" << tileInfo.gridIndex << ":" << tileInfo.tileIndex << "]";
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
