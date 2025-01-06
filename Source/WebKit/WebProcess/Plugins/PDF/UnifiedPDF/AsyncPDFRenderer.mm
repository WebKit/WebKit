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
#include "PDFPresentationController.h"
#include <CoreGraphics/CoreGraphics.h>
#include <PDFKit/PDFKit.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <wtf/NumberOfCores.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(AsyncPDFRenderer);

Ref<AsyncPDFRenderer> AsyncPDFRenderer::create(PDFPresentationController& presentationController)
{
    return adoptRef(*new AsyncPDFRenderer { presentationController });
}

// m_maxConcurrentTileRenders is a trade-off between rendering multiple tiles concurrently, and getting backed up because
// in-flight renders can't be canceled when resizing or zooming makes them invalid.
AsyncPDFRenderer::AsyncPDFRenderer(PDFPresentationController& presentationController)
    : m_presentationController(presentationController)
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
    for (auto& keyValuePair : m_layerIDtoLayerMap) {
        auto& layer = keyValuePair.value;
        if (auto* tiledBacking = layer->tiledBacking())
            tiledBacking->setClient(nullptr);
    }

    m_layerIDtoLayerMap.clear();
}

void AsyncPDFRenderer::releaseMemory()
{
#if !LOG_DISABLED
    auto oldPagePreviewCount = m_pagePreviews.size();
#endif

    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    for (Ref layer : m_layerIDtoLayerMap.values()) {
        // Ideally we'd be able to make the ImageBuffer memory volatile which would eliminate the need for this callback: webkit.org/b/274878
        if (auto* tiledBacking = layer->tiledBacking())
            removePagePreviewsOutsideCoverageRect(tiledBacking->coverageRect(), presentationController->rowForLayer(layer.ptr()));
    }

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::releaseMemory - reduced page preview count from " << oldPagePreviewCount << " to " << m_pagePreviews.size());
}

void AsyncPDFRenderer::startTrackingLayer(GraphicsLayer& layer)
{
    auto* tiledBacking = layer.tiledBacking();
    if (!tiledBacking)
        return;
    tiledBacking->setClient(this);
    m_tileGridToLayerIDMap.set(tiledBacking->primaryGridIdentifier(), *layer.primaryLayerID());
    m_layerIDtoLayerMap.set(*layer.primaryLayerID(), layer);
}

void AsyncPDFRenderer::stopTrackingLayer(GraphicsLayer& layer)
{
    auto* tiledBacking = layer.tiledBacking();
    if (!tiledBacking)
        return;
    auto gridIdentifier = tiledBacking->primaryGridIdentifier();
    m_tileGridToLayerIDMap.remove(gridIdentifier);
    m_gridRevalidationState.remove(gridIdentifier);
    tiledBacking->setClient(nullptr);
    m_layerIDtoLayerMap.remove(*layer.primaryLayerID());
}

RefPtr<GraphicsLayer> AsyncPDFRenderer::layerForTileGrid(TileGridIdentifier identifier) const
{
    auto layerID = m_tileGridToLayerIDMap.getOptional(identifier);
    if (!layerID)
        return nullptr;

    return m_layerIDtoLayerMap.get(*layerID);
}

void AsyncPDFRenderer::setShowDebugBorders(bool showDebugBorders)
{
    m_showDebugBorders = showDebugBorders;
}

static RefPtr<NativeImage> renderPDFPagePreview(RetainPtr<PDFDocument>&& pdfDocument, const PagePreviewRequest& request)
{
    ASSERT(!isMainRunLoop());
    RefPtr imageBuffer = ImageBuffer::create(request.normalizedPageBounds.size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, request.scale, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!imageBuffer)
        return nullptr;
    if (RetainPtr pdfPage = [pdfDocument pageAtIndex:request.pageIndex]) {
        auto& context = imageBuffer->context();
        auto destinationRect = request.normalizedPageBounds;
        if (request.showDebugIndicators)
            context.fillRect(destinationRect, Color::orange.colorWithAlphaByte(32));
        // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
        // from this page's drawing origin.
        context.translate(destinationRect.minXMaxYCorner());
        context.scale({ 1, -1 });
        CGContextSetShouldSubpixelQuantizeFonts(context.platformContext(), false);
        CGContextSetAllowsFontSubpixelPositioning(context.platformContext(), true);
        LOG_WITH_STREAM(PDFAsyncRendering, stream << "renderPDFPagePreview - page:" << request.pageIndex);
        [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
    }
    return ImageBuffer::sinkIntoNativeImage(WTFMove(imageBuffer));
}

void AsyncPDFRenderer::generatePreviewImageForPage(PDFDocumentLayout::PageIndex pageIndex, float scale)
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    RetainPtr pdfDocument = presentationController->pluginPDFDocument();
    if (!pdfDocument)
        return;

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::generatePreviewImageForPage " << pageIndex << " (have request " << m_enqueuedPagePreviews.contains(pageIndex) << ")");

    if (m_enqueuedPagePreviews.contains(pageIndex))
        return;

    auto pageBounds = presentationController->layoutBoundsForPageAtIndex(pageIndex);
    pageBounds.setLocation({ });

    auto pagePreviewRequest = PagePreviewRequest { pageIndex, pageBounds, scale, m_showDebugBorders };
    m_enqueuedPagePreviews.set(pageIndex, pagePreviewRequest);

    protectedPaintingWorkQueue()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, pdfDocument = WTFMove(pdfDocument), pagePreviewRequest]() mutable {
        RefPtr image = renderPDFPagePreview(WTFMove(pdfDocument), pagePreviewRequest);
        callOnMainRunLoop([weakThis = WTFMove(weakThis), image = WTFMove(image), pagePreviewRequest]() mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->didCompletePagePreviewRender(WTFMove(image), pagePreviewRequest);
        });
    });
}

void AsyncPDFRenderer::removePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::removePreviewForPage " << pageIndex);

    m_enqueuedPagePreviews.remove(pageIndex);
    m_pagePreviews.remove(pageIndex);
}

void AsyncPDFRenderer::didCompletePagePreviewRender(RefPtr<NativeImage>&& image, const PagePreviewRequest& pagePreviewRequest)
{
    ASSERT(isMainRunLoop());
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    auto pageIndex = pagePreviewRequest.pageIndex;
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompletePagePreviewRender for page " << pageIndex << " (have request " << m_enqueuedPagePreviews.contains(pageIndex) << ")");

    m_enqueuedPagePreviews.remove(pageIndex);
    m_pagePreviews.set(pageIndex, RenderedPagePreview { WTFMove(image), pagePreviewRequest.scale });
    presentationController->didGeneratePreviewForPage(pageIndex);
}

bool AsyncPDFRenderer::renderInfoIsValidForTile(TiledBacking& tiledBacking, const TileForGrid& tileInfo, const TileRenderInfo& renderInfo) const
{
    ASSERT(isMainRunLoop());

    auto currentTileRect = tiledBacking.rectForTile(tileInfo.tileIndex);
    auto currentRenderInfo = renderInfoForFullTile(tiledBacking, tileInfo, currentTileRect);

    return renderInfo.equivalentForPainting(currentRenderInfo);
}

void AsyncPDFRenderer::willRepaintTile(TiledBacking& tiledBacking, TileGridIdentifier gridIdentifier, TileIndex tileIndex, const FloatRect& tileRect, const FloatRect& tileDirtyRect)
{
    enqueueTileRenderForTileGridRepaint(tiledBacking, gridIdentifier, tileIndex, tileRect, tileDirtyRect);
}

std::optional<PDFTileRenderIdentifier> AsyncPDFRenderer::enqueueTileRenderForTileGridRepaint(TiledBacking& tiledBacking, TileGridIdentifier gridIdentifier, TileIndex tileIndex, const FloatRect& tileRect, const FloatRect& tileDirtyRect)
{
    auto tileInfo = TileForGrid { gridIdentifier, tileIndex };

    auto haveValidTile = [&](const TileForGrid& tileInfo) {
        auto it = m_rendereredTiles.find(tileInfo);
        if (it == m_rendereredTiles.end())
            return false;

        auto& renderInfo = it->value.tileInfo;
        if (renderInfo.tileRect != tileRect)
            return false;

        return renderInfoIsValidForTile(tiledBacking, tileInfo, renderInfo);
    };

    bool inScaleChangeRepaint = revalidationStateForGrid(gridIdentifier).inScaleChangeRepaint;
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRepaintTile " << tileInfo << " rect " << tileRect << " (dirty rect " << tileDirtyRect << ") - already queued "
        << m_currentValidTileRenders.contains(tileInfo) << " have cached tile " << m_rendereredTiles.contains(tileInfo) << " which is valid " << haveValidTile(tileInfo) << " doing scale change " << inScaleChangeRepaint);

    // If we have a tile, we can use it for layer contents.
    if (haveValidTile(tileInfo))
        return std::nullopt;

    if (inScaleChangeRepaint) {
        auto tile = m_rendereredTiles.take(tileInfo);
        m_rendereredTilesForOldState.add(tileInfo, WTFMove(tile));
    } else
        m_rendereredTiles.remove(tileInfo);

    // Currently we always do full tile paints when the grid changes.
    UNUSED_PARAM(tileDirtyRect);
    return enqueueTileRenderIfNecessary(tileInfo, renderInfoForFullTile(tiledBacking, tileInfo, tileRect));
}

void AsyncPDFRenderer::willRemoveTile(TiledBacking&, TileGridIdentifier gridIdentifier, TileIndex tileIndex)
{
    auto tileInfo = TileForGrid { gridIdentifier, tileIndex };
    bool inFullTileInvalidation = revalidationStateForGrid(gridIdentifier).inFullTileRevalidation;

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::willRemoveTile " << tileInfo << " - in full revalidation " << inFullTileInvalidation);

    m_requestWorkQueue.remove(tileInfo);
    m_currentValidTileRenders.remove(tileInfo);

    if (inFullTileInvalidation) {
        auto tile = m_rendereredTiles.take(tileInfo);
        m_rendereredTilesForOldState.add(tileInfo, WTFMove(tile));
    } else
        m_rendereredTiles.remove(tileInfo);
}

void AsyncPDFRenderer::willRepaintAllTiles(TiledBacking&, TileGridIdentifier)
{
    clearRequestsAndCachedTiles();
}

void AsyncPDFRenderer::coverageRectDidChange(TiledBacking& tiledBacking, const FloatRect& coverageRect)
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    std::optional<PDFLayoutRow> layoutRow;
    RefPtr<GraphicsLayer> layer = layerForTileGrid(tiledBacking.primaryGridIdentifier());
    if (layer)
        layoutRow = presentationController->rowForLayer(layer.get());

    auto pageCoverage = presentationController->pageCoverageForContentsRect(coverageRect, layoutRow);

    auto pagePreviewScale = presentationController->scaleForPagePreviews();

    for (auto& pageInfo : pageCoverage) {
        if (m_pagePreviews.contains(pageInfo.pageIndex))
            continue;

        generatePreviewImageForPage(pageInfo.pageIndex, pagePreviewScale);
    }

    if (!presentationController->pluginShouldCachePagePreviews())
        removePagePreviewsOutsideCoverageRect(coverageRect, layoutRow);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::coverageRectDidChange " << coverageRect << " " << pageCoverage << " - preview scale " << pagePreviewScale << " - have " << m_pagePreviews.size() << " page previews and " << m_enqueuedPagePreviews.size() << " enqueued");
}

void AsyncPDFRenderer::removePagePreviewsOutsideCoverageRect(const FloatRect& coverageRect, const std::optional<PDFLayoutRow>& layoutRow)
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    auto pageCoverage = presentationController->pageCoverageForContentsRect(coverageRect, layoutRow);

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

void AsyncPDFRenderer::willRevalidateTiles(TiledBacking&, TileGridIdentifier gridIdentifier, TileRevalidationType revalidationType)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "\n\nAsyncPDFRenderer::willRevalidateTiles for grid " << gridIdentifier << " is full revalidation " << (revalidationType == TileRevalidationType::Full) << " with " << m_rendereredTiles.size() << " rendered tiles");

    auto& gridState = revalidationStateForGrid(gridIdentifier);

    ASSERT(!gridState.inFullTileRevalidation);
    gridState.inFullTileRevalidation = true;
}

void AsyncPDFRenderer::didRevalidateTiles(TiledBacking& tiledBacking, TileGridIdentifier gridIdentifier, TileRevalidationType revalidationType, const HashSet<TileIndex>& tilesNeedingDisplay)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didRevalidateTiles for grid " << gridIdentifier << " is full revalidation " << (revalidationType == TileRevalidationType::Full) << " scale " << tiledBacking.tilingScaleFactor() << " - tiles to repaint " << tilesNeedingDisplay << " (saved " << m_rendereredTilesForOldState.size() << " old tiles)\n");

    bool inFullInvalidationForThisGrid = revalidationStateForGrid(gridIdentifier).inFullTileRevalidation;
    ASSERT_IMPLIES(revalidationType == TileRevalidationType::Full, inFullInvalidationForThisGrid);

    HashSet<PDFTileRenderIdentifier> revalidationRenderIdentifiers;

    for (auto tileIndex : tilesNeedingDisplay) {
        auto tileRect = tiledBacking.rectForTile(tileIndex);
        auto renderIdentifier = enqueueTileRenderForTileGridRepaint(tiledBacking, gridIdentifier, tileIndex, tileRect, tileRect);
        if (inFullInvalidationForThisGrid && renderIdentifier)
            revalidationRenderIdentifiers.add(*renderIdentifier);
    }

    if (inFullInvalidationForThisGrid)
        trackRendersForStaleTileMaintenance(gridIdentifier, WTFMove(revalidationRenderIdentifiers));

    revalidationStateForGrid(gridIdentifier).inFullTileRevalidation = false;
}

auto AsyncPDFRenderer::revalidationStateForGrid(TileGridIdentifier gridIdentifier) -> RevalidationStateForGrid&
{
    auto addResult = m_gridRevalidationState.ensure(gridIdentifier, [] {
        return makeUnique<RevalidationStateForGrid>();
    });

    return *addResult.iterator->value;
}

void AsyncPDFRenderer::trackRendersForStaleTileMaintenance(TileGridIdentifier gridIdentifier, HashSet<PDFTileRenderIdentifier>&& revalidationRenderIdentifiers)
{
    auto& revalidationState = revalidationStateForGrid(gridIdentifier);
    revalidationState.renderIdentifiersForCurrentRevalidation = WTFMove(revalidationRenderIdentifiers);
    LOG_WITH_STREAM(PDFAsyncRendering, stream << " tracking " << revalidationState.renderIdentifiersForCurrentRevalidation.size() << " renders before removing stale tiles");
}

void AsyncPDFRenderer::trackRenderCompletionForStaleTileMaintenance(TileGridIdentifier gridIdentifier, PDFTileRenderIdentifier renderIdentifier)
{
    auto& revalidationState = revalidationStateForGrid(gridIdentifier);

    if (revalidationState.renderIdentifiersForCurrentRevalidation.remove(renderIdentifier)) {
        if (revalidationState.renderIdentifiersForCurrentRevalidation.isEmpty()) {
            LOG_WITH_STREAM(PDFAsyncRendering, stream << " tile renders after revalidation complete. Removing " << m_rendereredTilesForOldState.size() << " stale tiles");
            m_rendereredTilesForOldState.clear();
        }
    }
}

void AsyncPDFRenderer::willRepaintTilesAfterScaleFactorChange(TiledBacking&, TileGridIdentifier gridIdentifier)
{
    ASSERT(!revalidationStateForGrid(gridIdentifier).inScaleChangeRepaint);
    revalidationStateForGrid(gridIdentifier).inScaleChangeRepaint = true;
}

void AsyncPDFRenderer::didRepaintTilesAfterScaleFactorChange(TiledBacking&, TileGridIdentifier gridIdentifier)
{
    ASSERT(revalidationStateForGrid(gridIdentifier).inScaleChangeRepaint);
    revalidationStateForGrid(gridIdentifier).inScaleChangeRepaint = false;
}

void AsyncPDFRenderer::didAddGrid(TiledBacking& tiledBacking, TileGridIdentifier gridIdentifier)
{
    m_tileGridToLayerIDMap.set(gridIdentifier, tiledBacking.layerIdentifier());
}

void AsyncPDFRenderer::willRemoveGrid(TiledBacking&, TileGridIdentifier gridIdentifier)
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

    m_tileGridToLayerIDMap.remove(gridIdentifier);
}

void AsyncPDFRenderer::clearRequestsAndCachedTiles()
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "\nAsyncPDFRenderer::clearRequestsAndCachedTiles - have " << m_rendereredTiles.size() << " rendered tiles");

    m_requestWorkQueue.clear();
    m_currentValidTileRenders.clear();
//    m_rendereredTiles.clear();
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

std::optional<PDFTileRenderIdentifier> AsyncPDFRenderer::enqueueTileRenderIfNecessary(const TileForGrid& tileInfo, TileRenderInfo&& renderInfo)
{
    if (renderInfo.pageCoverage.pages.isEmpty())
        return std::nullopt;

    // If there's an existing partial render for this tile, we need to override it, uniting partial updates.
    // We can't support multiple full/partial renders per tile since we have no ordering guarantees on which finishes first.
    auto it = m_currentValidTileRenders.find(tileInfo);
    if (it != m_currentValidTileRenders.end()) {
        auto& existingRenderInfo = it->value.renderInfo;

        // If we already have a full tile paint pending, no need to start a new one.
        if (existingRenderInfo == renderInfo)
            return std::nullopt;

        renderInfo.renderRect.unite(existingRenderInfo.renderRect);
    }

    auto renderIdentifier = PDFTileRenderIdentifier::generate();
    m_currentValidTileRenders.set(tileInfo, TileRenderData { renderIdentifier, renderInfo });
    m_requestWorkQueue.appendOrMoveToLast(tileInfo);
    serviceRequestQueue();
    return renderIdentifier;
}

TileRenderInfo AsyncPDFRenderer::renderInfoForFullTile(const TiledBacking& tiledBacking, const TileForGrid& tileInfo, const FloatRect& tileRect) const
{
    return renderInfoForTile(tiledBacking, tileInfo, tileRect, tileRect, nullptr);
}

TileRenderInfo AsyncPDFRenderer::renderInfoForTile(const TiledBacking& tiledBacking, const TileForGrid& tileInfo, const FloatRect& tileRect, const FloatRect& renderRect, RefPtr<NativeImage>&& background) const
{
    ASSERT(isMainRunLoop());

    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return { };

    RetainPtr pdfDocument = presentationController->pluginPDFDocument();
    if (!pdfDocument) {
        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::enqueueLayerPaint - document is null, bailing");
        return { };
    }

    auto tilingScaleFactor = tiledBacking.tilingScaleFactor();
    auto paintingClipRect = convertTileRectToPaintingCoords(tileRect, tilingScaleFactor);

    std::optional<PDFLayoutRow> layoutRow;
    if (RefPtr layer = layerForTileGrid(tileInfo.gridIdentifier))
        layoutRow = presentationController->rowForLayer(layer.get());

    auto pageCoverage = presentationController->pageCoverageAndScalesForContentsRect(paintingClipRect, layoutRow, tilingScaleFactor);

    return TileRenderInfo { tileRect, renderRect, WTFMove(background), pageCoverage, m_showDebugBorders };
}

static RefPtr<NativeImage> renderPDFTile(RetainPtr<PDFDocument>&& pdfDocument, const TileRenderInfo& renderInfo)
{
    ASSERT(!isMainRunLoop());
    RefPtr tileBuffer = ImageBuffer::create(renderInfo.tileRect.size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, renderInfo.pageCoverage.deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!tileBuffer)
        return nullptr;

    {
        auto& context = tileBuffer->context();
        if (RefPtr background = renderInfo.background)
            context.drawNativeImage(*background, { { }, tileBuffer->logicalSize() }, { { }, background->size() }, { CompositeOperator::Copy });
        context.translate(-renderInfo.tileRect.location());
        if (renderInfo.tileRect != renderInfo.renderRect)
            context.clip(renderInfo.renderRect);
        context.fillRect(renderInfo.renderRect, Color::white);
        if (renderInfo.showDebugIndicators)
            context.fillRect(renderInfo.renderRect, Color::green.colorWithAlphaByte(32));

        context.scale(renderInfo.pageCoverage.tilingScaleFactor);
        context.translate(-renderInfo.pageCoverage.contentsOffset);
        context.scale(renderInfo.pageCoverage.pdfDocumentScale);

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

            LOG_WITH_STREAM(PDFAsyncRendering, stream << "renderPDFTile renderInfo:" << renderInfo << ", PDF page:" << pageInfo.pageIndex << " destinationRect:" << destinationRect);
            [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
        }
    }
    return ImageBuffer::sinkIntoNativeImage(WTFMove(tileBuffer));
}

void AsyncPDFRenderer::serviceRequestQueue()
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    while (m_numConcurrentTileRenders < m_maxConcurrentTileRenders) {
        if (m_requestWorkQueue.isEmpty())
            break;

        TileForGrid tileInfo = m_requestWorkQueue.takeFirst();
        auto it = m_currentValidTileRenders.find(tileInfo);
        if (it == m_currentValidTileRenders.end())
            continue;

        TileRenderData renderData = it->value;

        LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::serviceRequestQueue - rendering tile: " << tileInfo << " renderData: " << renderData << " (" << m_numConcurrentTileRenders << " concurrent renders)");

        ++m_numConcurrentTileRenders;

        protectedPaintingWorkQueue()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, pdfDocument = RetainPtr { presentationController->pluginPDFDocument() }, tileInfo, renderData] mutable {
            RefPtr image = renderPDFTile(WTFMove(pdfDocument), renderData.renderInfo);
            callOnMainRunLoop([weakThis = WTFMove(weakThis), image = WTFMove(image), tileInfo, renderData] mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->didCompleteTileRender(WTFMove(image), tileInfo, renderData);
            });
        });
    }

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::serviceRequestQueue() - " << m_numConcurrentTileRenders << " renders in flight, " << m_requestWorkQueue.size() << " in queue");
}

// The image may be null if allocation on the decoding thread failed.
void AsyncPDFRenderer::didCompleteTileRender(RefPtr<NativeImage>&& image, const TileForGrid& tileInfo, const TileRenderData& renderData)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileRender - got results for tile: " << tileInfo << " renderData: " << renderData << " (" << m_rendereredTiles.size() << " tiles in cache).");

    ASSERT(m_numConcurrentTileRenders);
    --m_numConcurrentTileRenders;
    serviceRequestQueue();
    auto& [renderIdentifier, renderInfo] = renderData;
    trackRenderCompletionForStaleTileMaintenance(tileInfo.gridIdentifier, renderIdentifier);

    {
        auto it = m_currentValidTileRenders.find(tileInfo);
        if (it == m_currentValidTileRenders.end() || it->value.renderIdentifier != renderIdentifier) {
            LOG_WITH_STREAM(PDFAsyncRendering, stream << "  Tile render request was revoked.");
            return;
        }
        m_currentValidTileRenders.remove(it);
    }

    if (!image)
        return;

    // State may have changed since we started the tile paint; check that it's still valid.
    RefPtr tileGridLayer = layerForTileGrid(tileInfo.gridIdentifier);
    if (!tileGridLayer)
        return;

    auto* tiledBacking = tileGridLayer->tiledBacking();
    if (!tiledBacking)
        return;

    if (!renderInfoIsValidForTile(*tiledBacking, tileInfo, renderInfo))
        return;

    m_rendereredTiles.set(tileInfo, RenderedTile { image.releaseNonNull(), renderInfo });
    auto paintingClipRect = convertTileRectToPaintingCoords(renderInfo.tileRect, renderInfo.pageCoverage.tilingScaleFactor);
    tileGridLayer->setNeedsDisplayInRect(paintingClipRect);
}

bool AsyncPDFRenderer::paintTilesForPage(const GraphicsLayer* layer, GraphicsContext& context, float documentScale, const FloatRect& clipRect, const FloatRect& clipRectInPageLayoutCoordinates, const FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex pageIndex)
{
    ASSERT(isMainRunLoop());
    ASSERT(layer);

    auto* tiledBacking = layer->tiledBacking();
    if (!tiledBacking)
        return false;

    auto tilingScaleFactor = tiledBacking->tilingScaleFactor();
    auto tileGridIdentifier = tiledBacking->primaryGridIdentifier();

    bool paintedATile = false;

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "\nAsyncPDFRenderer::paintTilesForPage - painting tiles for page " << pageIndex << " dest rect " << pageBoundsInPaintingCoordinates << " clip " << clipRect << " page clip " << clipRectInPageLayoutCoordinates << " - " << m_rendereredTiles.size() << " tiles, " << m_rendereredTilesForOldState.size() << " old tiles");

    {
        // If we have any tiles from a previous tile size or scale config, paint them first so we don't flash back to the low-res page preview.
        for (auto& keyValuePair : m_rendereredTilesForOldState) {
            auto& tileForGrid = keyValuePair.key;
            auto& renderedTile = keyValuePair.value;

            if (tileForGrid.gridIdentifier != tileGridIdentifier)
                continue;

            for (auto& pageInfo : renderedTile.tileInfo.pageCoverage.pages) {
                if (pageInfo.pageIndex != pageIndex)
                    continue;

                auto rectClippedToCurrentPage = intersection(pageInfo.rectInPageLayoutCoordinates, pageInfo.pageBounds);
                auto bufferCoverageInPageCoords = intersection(rectClippedToCurrentPage, clipRectInPageLayoutCoordinates);
                if (bufferCoverageInPageCoords.isEmpty())
                    continue;

                RefPtr image = renderedTile.image;

                // The old buffers no longer align with tile boundaries, so map via PDF layout coordinates
                // to paint the buffers with the right position and scale.
                auto sourceRect = mapRect(bufferCoverageInPageCoords, pageInfo.rectInPageLayoutCoordinates, { { }, image->size() });
                auto destRect = mapRect(bufferCoverageInPageCoords, pageInfo.pageBounds, pageBoundsInPaintingCoordinates);

                LOG_WITH_STREAM(PDFAsyncRendering, stream << " AsyncPDFRenderer::paintTilesForPage " << pageBoundsInPaintingCoordinates  << " - painting old tile " << tileForGrid.tileIndex << " for " << renderedTile.tileInfo.tileRect << " page layout rect " << rectClippedToCurrentPage);

                context.drawNativeImage(*image, destRect, sourceRect, { CompositeOperator::Copy });

                if (m_showDebugBorders)
                    context.fillRect(destRect, Color::blue.colorWithAlphaByte(64));
            }
        }
    }

    {
        // This scale takes us from "painting" coordinates into the coordinate system of the tile grid,
        // so we can paint tiles directly.
        auto scaleTransform = tileToPaintingTransform(tilingScaleFactor);

        auto stateSaver = GraphicsContextStateSaver(context);
        context.concatCTM(scaleTransform);

        // Linear traversal of all the tiles isn't great.
        for (auto& keyValuePair : m_rendereredTiles) {
            auto& tileForGrid = keyValuePair.key;
            auto& renderedTile = keyValuePair.value;

            if (tileForGrid.gridIdentifier != tileGridIdentifier)
                continue;

            auto tileClipInPaintingCoordinates = scaleTransform.mapRect(renderedTile.tileInfo.tileRect);
            if (!pageBoundsInPaintingCoordinates.intersects(tileClipInPaintingCoordinates))
                continue;

            if (!tileClipInPaintingCoordinates.intersects(clipRect))
                continue;

            LOG_WITH_STREAM(PDFAsyncRendering, stream << " AsyncPDFRenderer::paintTilesForPage " << pageBoundsInPaintingCoordinates  << " - painting tile for " << tileForGrid << " with clip " << renderedTile.tileInfo.tileRect << " tiling scale " << tilingScaleFactor);
            RefPtr image = renderedTile.image;
            context.drawNativeImage(*image, renderedTile.tileInfo.tileRect, { { }, image->size() }, { CompositeOperator::Copy });
            paintedATile = true;
        }
    }

    return paintedATile;
}

void AsyncPDFRenderer::paintPagePreview(GraphicsContext& context, const FloatRect&, const FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex pageIndex)
{
    auto preview = m_pagePreviews.get(pageIndex);
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::paintPagePreview for page " << pageIndex  << " - buffer " << !!preview.image);
    if (!preview.image)
        return;
    Ref image = *preview.image;
    auto imageRect = pageBoundsInPaintingCoordinates;
    imageRect.scale(preview.scale);
    context.drawNativeImage(image, pageBoundsInPaintingCoordinates, imageRect, { CompositeOperator::Copy });
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

void AsyncPDFRenderer::setNeedsRenderForRect(GraphicsLayer& layer, const FloatRect& bounds)
{
    // FIXME: If our platform does not support partial updates (supportsPartialRepaint() is false) then this should behave
    // identically to invalidateTilesForPaintingRect().

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::setNeedsRenderForRect " << bounds);

    ASSERT(isMainRunLoop());

    auto* tiledBacking = layer.tiledBacking();
    if (!tiledBacking) {
        // We only expect AsyncPDFRenderer to be used with tiled layers.
        ASSERT_NOT_REACHED();
        return;
    }

    auto boundsInTileCoordinates = bounds;
    boundsInTileCoordinates.scale(tiledBacking->tilingScaleFactor());

    for (auto& keyValuePair : m_rendereredTiles) {
        auto& tileInfo = keyValuePair.key;
        auto& renderedTile = keyValuePair.value;
        auto tileRect = renderedTile.tileInfo.tileRect;
        FloatRect renderRect = intersection(tileRect, boundsInTileCoordinates);
        if (renderRect.isEmpty())
            continue;
        RefPtr<NativeImage> background;
        if (renderRect != tileRect)
            background = renderedTile.image;
        enqueueTileRenderIfNecessary(tileInfo, renderInfoForTile(*tiledBacking, tileInfo, tileRect, renderRect, WTFMove(background)));
    }
}

void AsyncPDFRenderer::setNeedsPagePreviewRenderForPageCoverage(const PDFPageCoverage& pageCoverage)
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;
    auto pagePreviewScale = presentationController->scaleForPagePreviews();
    for (auto& pageInfo : pageCoverage)
        generatePreviewImageForPage(pageInfo.pageIndex, pagePreviewScale);
}

TextStream& operator<<(TextStream& ts, const TileForGrid& tileInfo)
{
    ts << "[" << tileInfo.gridIdentifier << ":" << tileInfo.tileIndex << "]";
    return ts;
}

TextStream& operator<<(TextStream& ts, const TileRenderInfo& renderInfo)
{
    ts << "[tileRect:" << renderInfo.tileRect << ", renderRect:" << renderInfo.renderRect << "]";
    return ts;
}

TextStream& operator<<(TextStream& ts, const TileRenderData& renderData)
{
    ts << "[" << renderData.renderIdentifier << ":" << renderData.renderInfo << "]";
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
