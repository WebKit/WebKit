/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#include "DynamicContentScalingImageBufferBackend.h"
#include "Logging.h"
#include "PDFPresentationController.h"
#include <CoreGraphics/CoreGraphics.h>
#include <PDFKit/PDFKit.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <wtf/NumberOfCores.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)

// This will be moved to WebCore/platform/graphics/cocoa.
class DynamicContentScalingImageBuffer : public WebCore::ImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED(DynamicContentScalingImageBuffer);
public:
    using ImageBuffer::ImageBuffer;

    std::optional<DynamicContentScalingDisplayList> dynamicContentScalingDisplayList() final
    {
        auto* backend = static_cast<DynamicContentScalingImageBufferBackend*>(this->backend());
        return backend->displayList();
    }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(DynamicContentScalingImageBuffer);

#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(AsyncPDFRenderer);

Ref<AsyncPDFRenderer> AsyncPDFRenderer::create(PDFPresentationController& presentationController)
{
    return adoptRef(*new AsyncPDFRenderer { presentationController });
}

// m_maxConcurrentTileRenders is a trade-off between rendering multiple tiles concurrently, and getting backed up because
// in-flight renders can't be canceled when resizing or zooming makes them invalid.
AsyncPDFRenderer::AsyncPDFRenderer(PDFPresentationController& presentationController)
    : m_presentationController(presentationController)
    , m_workQueue(ConcurrentWorkQueue::create("WebKit: PDF Painting Work Queue"_s, WorkQueue::QOS::UserInteractive))
    , m_workQueueSlots(std::clamp(WTF::numberOfProcessorCores() - 2, 4, 16))
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
        if (CheckedPtr tiledBacking = layer->tiledBacking())
            tiledBacking->setClient(nullptr);
    }

    m_layerIDtoLayerMap.clear();
}

void AsyncPDFRenderer::releaseMemory()
{
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    m_dynamicContentScalingResourceCache.clear();
#endif
#if !LOG_DISABLED
    auto oldPagePreviewCount = m_pagePreviews.size();
#endif

    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    for (Ref layer : m_layerIDtoLayerMap.values()) {
        // Ideally we'd be able to make the ImageBuffer memory volatile which would eliminate the need for this callback: webkit.org/b/274878
        if (CheckedPtr tiledBacking = layer->tiledBacking())
            removePagePreviewsOutsideCoverageRect(tiledBacking->coverageRect(), presentationController->rowForLayer(layer.ptr()));
    }

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::releaseMemory - reduced page preview count from " << oldPagePreviewCount << " to " << m_pagePreviews.size());
}

void AsyncPDFRenderer::startTrackingLayer(GraphicsLayer& layer)
{
    CheckedPtr tiledBacking = layer.tiledBacking();
    if (!tiledBacking)
        return;
    tiledBacking->setClient(this);
    m_tileGridToLayerIDMap.set(tiledBacking->primaryGridIdentifier(), *layer.primaryLayerID());
    m_layerIDtoLayerMap.set(*layer.primaryLayerID(), layer);
}

void AsyncPDFRenderer::stopTrackingLayer(GraphicsLayer& layer)
{
    CheckedPtr tiledBacking = layer.tiledBacking();
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

static RefPtr<NativeImage> renderPDFPagePreview(RetainPtr<PDFDocument>&& pdfDocument, const PDFPagePreviewRenderRequest& request)
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
        RetainPtr platformContext = context.platformContext();
        CGContextSetShouldSubpixelQuantizeFonts(platformContext.get(), false);
        CGContextSetAllowsFontSubpixelPositioning(platformContext.get(), true);
        LOG_WITH_STREAM(PDFAsyncRendering, stream << "renderPDFPagePreview - page:" << request.pageIndex);
        [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:platformContext.get()];
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

    PDFPagePreviewRenderKey key { pageIndex };
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::generatePreviewImageForPage " << pageIndex << " (have request " << m_pendingPagePreviews.contains(key) << ")");

    if (m_pendingPagePreviews.contains(key))
        return;

    auto pageBounds = presentationController->layoutBoundsForPageAtIndex(pageIndex);
    pageBounds.setLocation({ });

    PDFPagePreviewRenderRequest request { pageIndex, pageBounds, scale, m_showDebugBorders };
    m_pendingPagePreviews.set(key, request);
    m_pendingPagePreviewOrder.appendOrMoveToLast(key);
    serviceRequestQueues();
}

void AsyncPDFRenderer::removePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::removePreviewForPage " << pageIndex);
    PDFPagePreviewRenderKey key { pageIndex };
    m_pendingPagePreviewOrder.remove(key);
    m_pendingPagePreviews.remove(key);
    m_pagePreviews.remove(key);
}

void AsyncPDFRenderer::didCompletePagePreviewRender(RefPtr<NativeImage>&& image, const PDFPagePreviewRenderRequest& request)
{
    ASSERT(isMainRunLoop());
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;
    PDFPagePreviewRenderKey key { request.pageIndex };
    if (!m_pendingPagePreviews.contains(key))
        return;
    m_pendingPagePreviews.remove(key);
    m_pagePreviews.set(key, RenderedPDFPagePreview { WTFMove(image), request.scale });
    presentationController->didGeneratePreviewForPage(key.pageIndex);
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

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<DynamicContentScalingDisplayList> AsyncPDFRenderer::dynamicContentScalingDisplayListForTile(TiledBacking&, TileGridIdentifier identifier, TileIndex index)
{
    auto it = m_rendereredTiles.find({ identifier, index });
    if (it == m_rendereredTiles.end())
        return std::nullopt;
    auto& maybeDisplayList = it->value.dynamicContentScalingDisplayList;
    if (!maybeDisplayList)
        return std::nullopt;
    // Return a copy because typical caller wants to consume.
    return DynamicContentScalingDisplayList { *maybeDisplayList };
}
#endif

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
        << m_pendingTileRenders.contains(tileInfo) << " have cached tile " << m_rendereredTiles.contains(tileInfo) << " which is valid " << haveValidTile(tileInfo) << " doing scale change " << inScaleChangeRepaint);

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

    m_pendingTileRenderOrder.remove(tileInfo);
    m_pendingTileRenders.remove(tileInfo);

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

void AsyncPDFRenderer::ensurePreviewsForCurrentPageCoverage()
{
    if (!m_currentPageCoverage)
        return;

    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    auto pageCoverage = *m_currentPageCoverage;
    auto pagePreviewScale = presentationController->scaleForPagePreviews();

    for (auto& pageInfo : pageCoverage) {
        if (m_pagePreviews.contains({ pageInfo.pageIndex }))
            continue;

        generatePreviewImageForPage(pageInfo.pageIndex, pagePreviewScale);
    }

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::ensurePreviewImagesForPageCoverage " << pageCoverage << " - preview scale " << pagePreviewScale << " - have " << m_pagePreviews.size() << " page previews and " << m_pendingPagePreviews.size() << " enqueued");
}

void AsyncPDFRenderer::coverageRectDidChange(TiledBacking& tiledBacking, const FloatRect& coverageRect)
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    std::optional<PDFLayoutRow> layoutRow;
    RefPtr layer = layerForTileGrid(tiledBacking.primaryGridIdentifier());
    if (layer)
        layoutRow = presentationController->rowForLayer(layer.get());

    m_currentPageCoverage = presentationController->pageCoverageForContentsRect(coverageRect, layoutRow);
    ensurePreviewsForCurrentPageCoverage();

    if (!presentationController->pluginShouldCachePagePreviews())
        removePagePreviewsOutsideCoverageRect(coverageRect, layoutRow);

    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::coverageRectDidChange " << coverageRect);
}

void AsyncPDFRenderer::removePagePreviewsOutsideCoverageRect(const FloatRect& coverageRect, const std::optional<PDFLayoutRow>& layoutRow)
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    auto pageCoverage = presentationController->pageCoverageForContentsRect(coverageRect, layoutRow);

    using PDFPageIndex = PDFDocumentLayout::PageIndex;
    HashSet<PDFPageIndex, IntHash<PDFPageIndex>, WTF::UnsignedWithZeroKeyHashTraits<PDFPageIndex>> unwantedPageIndices;
    for (auto key : m_pagePreviews.keys())
        unwantedPageIndices.add(key.pageIndex);

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

    m_pendingTileRenders.removeIf([gridIdentifier](const auto& keyValuePair) {
        return keyValuePair.key.gridIdentifier == gridIdentifier;
    });

    Vector<TileForGrid> requestsToRemove;
    for (auto& tileRequests : m_pendingTileRenderOrder) {
        if (tileRequests.gridIdentifier == gridIdentifier)
            requestsToRemove.append(tileRequests);
    }

    for (auto& tile : requestsToRemove)
        m_pendingTileRenderOrder.remove(tile);

    m_tileGridToLayerIDMap.remove(gridIdentifier);
}

void AsyncPDFRenderer::clearRequestsAndCachedTiles()
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "\nAsyncPDFRenderer::clearRequestsAndCachedTiles - have " << m_pendingTileRenders.size() << " pending tile renders");

    m_pendingTileRenderOrder.clear();
    m_pendingTileRenders.clear();
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
    auto it = m_pendingTileRenders.find(tileInfo);
    if (it != m_pendingTileRenders.end()) {
        auto& existingRenderInfo = it->value.renderInfo;

        // If we already have a full tile paint pending, no need to start a new one.
        if (existingRenderInfo == renderInfo)
            return std::nullopt;

        renderInfo.renderRect.unite(existingRenderInfo.renderRect);
    }

    auto renderIdentifier = PDFTileRenderIdentifier::generate();
    m_pendingTileRenders.set(tileInfo, TileRenderData { renderIdentifier, renderInfo });
    m_pendingTileRenderOrder.appendOrMoveToLast(tileInfo);
    serviceRequestQueues();
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

static void renderPDFTile(PDFDocument *pdfDocument, const TileRenderInfo& renderInfo, GraphicsContext& context)
{
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
        [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:RetainPtr { context.platformContext() }.get()];
    }
}

static RefPtr<NativeImage> renderPDFTileToImage(PDFDocument *pdfDocument, const TileRenderInfo& renderInfo)
{
    ASSERT(!isMainRunLoop());
    RefPtr tileBuffer = ImageBuffer::create(renderInfo.tileRect.size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, renderInfo.pageCoverage.deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!tileBuffer)
        return nullptr;
    {
        auto& context = tileBuffer->context();
        if (RefPtr background = renderInfo.background)
            context.drawNativeImage(*background, { { }, tileBuffer->logicalSize() }, { { }, background->size() }, { CompositeOperator::Copy });
        renderPDFTile(pdfDocument, renderInfo, context);
    }
    return ImageBuffer::sinkIntoNativeImage(WTFMove(tileBuffer));
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
static std::optional<DynamicContentScalingDisplayList> renderPDFTileToDynamicContentScalingDisplayList(WebCore::DynamicContentScalingResourceCache& dynamicContentScalingResourceCache, PDFDocument *pdfDocument, const TileRenderInfo& renderInfo)
{
    ASSERT(!isMainRunLoop());
    WebCore::ImageBufferCreationContext creationContext;
    creationContext.dynamicContentScalingResourceCache = dynamicContentScalingResourceCache;
    RefPtr tileBuffer = ImageBuffer::create<DynamicContentScalingImageBufferBackend, DynamicContentScalingImageBuffer>(renderInfo.tileRect.size(), renderInfo.pageCoverage.deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, RenderingPurpose::Unspecified, creationContext);
    if (!tileBuffer)
        return std::nullopt;
    // Fixup incremental rendering requests to render the contents covering the full tile.
    auto localRenderInfo = renderInfo;
    if (localRenderInfo.tileRect != renderInfo.renderRect)
        localRenderInfo.renderRect = renderInfo.tileRect;
    renderPDFTile(pdfDocument, localRenderInfo, tileBuffer->context());
    return tileBuffer->dynamicContentScalingDisplayList();
}
#endif

void AsyncPDFRenderer::serviceRequestQueues()
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;

    while (m_workQueueSlots > 0 && !m_pendingPagePreviewOrder.isEmpty()) {
        auto key = m_pendingPagePreviewOrder.takeFirst();
        auto request = m_pendingPagePreviews.getOptional(key);
        if (!request)
            continue;
        m_workQueueSlots--;
        m_workQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }, pdfDocument = RetainPtr { presentationController->pluginPDFDocument() }, request = WTFMove(*request)] mutable {
            RefPtr image = renderPDFPagePreview(WTFMove(pdfDocument), request);
            callOnMainRunLoop([weakThis = WTFMove(weakThis), image = WTFMove(image), request = WTFMove(request)] mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->m_workQueueSlots++;
                protectedThis->serviceRequestQueues();
                protectedThis->didCompletePagePreviewRender(WTFMove(image), request);
            });
        });
    }

    while (m_workQueueSlots > 0 && !m_pendingTileRenderOrder.isEmpty()) {
        TileForGrid renderKey = m_pendingTileRenderOrder.takeFirst();
        auto renderData = m_pendingTileRenders.getOptional(renderKey);
        if (!renderData)
            continue;
        m_workQueueSlots--;
        m_workQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }, pdfDocument = RetainPtr { presentationController->pluginPDFDocument() }, renderKey, renderData = WTFMove(*renderData)
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
            , dynamicContentScalingResourceCache = ensureDynamicContentScalingResourceCache()
#endif
        ] mutable {
            RenderedPDFTile tile { renderData.renderInfo };
            tile.image = renderPDFTileToImage(pdfDocument.get(), renderData.renderInfo);
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
            if (tile.image)
                tile.dynamicContentScalingDisplayList = renderPDFTileToDynamicContentScalingDisplayList(dynamicContentScalingResourceCache, pdfDocument.get(), renderData.renderInfo);
#endif
            callOnMainRunLoop([weakThis = WTFMove(weakThis), renderKey, renderIdentifier = renderData.renderIdentifier, tile = WTFMove(tile) ] mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->m_workQueueSlots++;
                protectedThis->serviceRequestQueues();
                protectedThis->didCompleteTileRender(renderKey, renderIdentifier, WTFMove(tile));
            });
        });
    }
}

// The image may be null if allocation on the decoding thread failed.
void AsyncPDFRenderer::didCompleteTileRender(const TileForGrid& renderKey, PDFTileRenderIdentifier renderIdentifier, RenderedPDFTile tile)
{
    LOG_WITH_STREAM(PDFAsyncRendering, stream << "AsyncPDFRenderer::didCompleteTileRender - got results for tile: " << renderKey << " (" << m_rendereredTiles.size() << " tiles in cache).");

    trackRenderCompletionForStaleTileMaintenance(renderKey.gridIdentifier, renderIdentifier);

    {
        auto it = m_pendingTileRenders.find(renderKey);
        if (it == m_pendingTileRenders.end() || it->value.renderIdentifier != renderIdentifier) {
            LOG_WITH_STREAM(PDFAsyncRendering, stream << "  Tile render request was revoked.");
            return;
        }
        m_pendingTileRenders.remove(it);
    }

    if (!tile.image)
        return;

    // State may have changed since we started the tile paint; check that it's still valid.
    RefPtr tileGridLayer = layerForTileGrid(renderKey.gridIdentifier);
    if (!tileGridLayer)
        return;

    CheckedPtr tiledBacking = tileGridLayer->tiledBacking();
    if (!tiledBacking)
        return;

    if (!renderInfoIsValidForTile(*tiledBacking, renderKey, tile.tileInfo))
        return;
    auto paintingClipRect = convertTileRectToPaintingCoords(tile.tileInfo.tileRect, tile.tileInfo.pageCoverage.tilingScaleFactor);
    m_rendereredTiles.set(renderKey, WTFMove(tile));
    tileGridLayer->setNeedsDisplayInRect(paintingClipRect);
}

bool AsyncPDFRenderer::paintTilesForPage(const GraphicsLayer* layer, GraphicsContext& context, float documentScale, const FloatRect& clipRect, const FloatRect& clipRectInPageLayoutCoordinates, const FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex pageIndex)
{
    ASSERT(isMainRunLoop());
    ASSERT(layer);

    CheckedPtr tiledBacking = layer->tiledBacking();
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
    auto preview = m_pagePreviews.get({ pageIndex });
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

    CheckedPtr tiledBacking = layer.tiledBacking();
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

void AsyncPDFRenderer::invalidatePreviewsForPageCoverage(const PDFPageCoverage& pageCoverage)
{
    RefPtr presentationController = m_presentationController.get();
    if (!presentationController)
        return;
    for (auto& pageInfo : pageCoverage)
        removePreviewForPage(pageInfo.pageIndex);

    ensurePreviewsForCurrentPageCoverage();
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
DynamicContentScalingResourceCache AsyncPDFRenderer::ensureDynamicContentScalingResourceCache()
{
    if (!m_dynamicContentScalingResourceCache)
        m_dynamicContentScalingResourceCache = WebCore::DynamicContentScalingResourceCache::create();
    return m_dynamicContentScalingResourceCache;
}
#endif

TextStream& operator<<(TextStream& ts, const TileForGrid& tileInfo)
{
    ts << '[' << tileInfo.gridIdentifier << ':' << tileInfo.tileIndex << ']';
    return ts;
}

TextStream& operator<<(TextStream& ts, const TileRenderInfo& renderInfo)
{
    ts << "[tileRect:"_s << renderInfo.tileRect << ", renderRect:"_s << renderInfo.renderRect << ']';
    return ts;
}

TextStream& operator<<(TextStream& ts, const TileRenderData& renderData)
{
    ts << '[' << renderData.renderIdentifier << ':' << renderData.renderInfo << ']';
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
