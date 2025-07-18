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

#pragma once

#if ENABLE(UNIFIED_PDF)

#include "PDFDocumentLayout.h"
#include "PDFPageCoverage.h"
#include <WebCore/FloatRect.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntPoint.h>
#include <WebCore/TiledBacking.h>
#include <limits>
#include <wtf/HashMap.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#include <WebCore/DynamicContentScalingDisplayList.h>
#include <WebCore/DynamicContentScalingResourceCache.h>
#endif

OBJC_CLASS PDFDocument;

namespace WebKit {

struct TileForGrid {
    WebCore::TileGridIdentifier gridIdentifier;
    WebCore::TileIndex tileIndex;

    bool operator==(const TileForGrid&) const = default;

    unsigned computeHash() const
    {
        return WTF::computeHash(gridIdentifier.toUInt64(), tileIndex.x(), tileIndex.y());
    }
};

WTF::TextStream& operator<<(WTF::TextStream&, const TileForGrid&);

struct PDFTileRenderType;
using PDFTileRenderIdentifier = ObjectIdentifier<PDFTileRenderType>;

struct TileRenderInfo {
    WebCore::FloatRect tileRect;
    WebCore::FloatRect renderRect; // Represents the portion of the tile that needs rendering (in the same coordinate system as tileRect).
    RefPtr<WebCore::NativeImage> background; // Optional existing content around renderRect, will be rendered to tileRect.
    PDFPageCoverageAndScales pageCoverage;
    bool showDebugIndicators { false };

    bool operator==(const TileRenderInfo&) const = default;
    bool equivalentForPainting(const TileRenderInfo& other) const
    {
        return tileRect == other.tileRect && pageCoverage == other.pageCoverage;
    }
};

WTF::TextStream& operator<<(WTF::TextStream&, const TileRenderInfo&);

struct TileRenderData {
    PDFTileRenderIdentifier renderIdentifier;
    TileRenderInfo renderInfo;
};

WTF::TextStream& operator<<(WTF::TextStream&, const TileRenderData&);

struct RenderedPDFTile {
    TileRenderInfo tileInfo;
    RefPtr<WebCore::NativeImage> image;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> dynamicContentScalingDisplayList;
#endif
};


struct PDFPagePreviewRenderKey {
    PDFDocumentLayout::PageIndex pageIndex { std::numeric_limits<PDFDocumentLayout::PageIndex>::max() };
    bool operator==(const PDFPagePreviewRenderKey&) const = default;
};

struct PDFPagePreviewRenderRequest {
    PDFDocumentLayout::PageIndex pageIndex;
    WebCore::FloatRect normalizedPageBounds;
    float scale { 1.0f };
    bool showDebugIndicators { false };
};

struct PDFPagePreviewRenderKeyHash {
    static unsigned hash(const PDFPagePreviewRenderKey& value)
    {
        return WTF::computeHash(value.pageIndex);
    }
    static bool equal(const PDFPagePreviewRenderKey& a, const PDFPagePreviewRenderKey& b)
    {
        return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace WebKit

namespace WTF {

struct TileForGridHash {
    static unsigned hash(const WebKit::TileForGrid& key)
    {
        return key.computeHash();
    }
    static bool equal(const WebKit::TileForGrid& a, const WebKit::TileForGrid& b)
    {
        return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebKit::TileForGrid> : GenericHashTraits<WebKit::TileForGrid> {
    static constexpr bool emptyValueIsZero = true;
    static WebKit::TileForGrid emptyValue() { return { HashTraits<WebCore::TileGridIdentifier>::emptyValue(), { 0, 0 } }; }
    static bool isEmptyValue(const WebKit::TileForGrid& value) { return value.gridIdentifier.isHashTableEmptyValue(); }
    static void constructDeletedValue(WebKit::TileForGrid& tileForGrid) { HashTraits<WebCore::TileGridIdentifier>::constructDeletedValue(tileForGrid.gridIdentifier); }
    static bool isDeletedValue(const WebKit::TileForGrid& tileForGrid) { return tileForGrid.gridIdentifier.isHashTableDeletedValue(); }
};
template<> struct DefaultHash<WebKit::TileForGrid> : TileForGridHash { };

template<> struct HashTraits<WebKit::TileRenderData> : SimpleClassHashTraits<WebKit::TileRenderData> {
    static constexpr bool emptyValueIsZero = false;
    static constexpr bool hasIsEmptyValueFunction = true;
    static WebKit::TileRenderData emptyValue() { return { HashTraits<WebKit::PDFTileRenderIdentifier>::emptyValue(), { } }; }
    static bool isEmptyValue(const WebKit::TileRenderData& data) { return HashTraits<WebKit::PDFTileRenderIdentifier>::isEmptyValue(data.renderIdentifier); }
};

template<> struct HashTraits<WebKit::PDFPagePreviewRenderKey> : GenericHashTraits<WebKit::PDFPagePreviewRenderKey> {
    static constexpr bool emptyValueIsZero = false;
    static WebKit::PDFPagePreviewRenderKey emptyValue() { return { std::numeric_limits<WebKit::PDFDocumentLayout::PageIndex>::max() }; }
    static void constructDeletedValue(WebKit::PDFPagePreviewRenderKey& slot) { slot = { std::numeric_limits<WebKit::PDFDocumentLayout::PageIndex>::max() - 1 }; }
    static bool isDeletedValue(WebKit::PDFPagePreviewRenderKey value) { return value.pageIndex == std::numeric_limits<WebKit::PDFDocumentLayout::PageIndex>::max() - 1; }
};

template<> struct DefaultHash<WebKit::PDFPagePreviewRenderKey> : WebKit::PDFPagePreviewRenderKeyHash {
};

} // namespace WTF

namespace WebKit {

class PDFPresentationController;

class AsyncPDFRenderer final : public WebCore::TiledBackingClient,
    public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AsyncPDFRenderer> {
    WTF_MAKE_TZONE_ALLOCATED(AsyncPDFRenderer);
    WTF_MAKE_NONCOPYABLE(AsyncPDFRenderer);
public:
    static Ref<AsyncPDFRenderer> create(PDFPresentationController&);

    virtual ~AsyncPDFRenderer();

    void startTrackingLayer(WebCore::GraphicsLayer&);
    void stopTrackingLayer(WebCore::GraphicsLayer&);
    void teardown();

    void releaseMemory();

    bool paintTilesForPage(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, float documentScale, const WebCore::FloatRect& clipRect, const WebCore::FloatRect& clipRectInPageCoordinates, const WebCore::FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex);
    void paintPagePreview(WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect, const WebCore::FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex);

    // Throws away existing tiles. Can result in flashing.
    void invalidateTilesForPaintingRect(float pageScaleFactor, const WebCore::FloatRect& paintingRect);

    // Updates existing tiles. Can result in temporarily stale content.
    void setNeedsRenderForRect(WebCore::GraphicsLayer&, const WebCore::FloatRect& bounds);

    void generatePreviewImageForPage(PDFDocumentLayout::PageIndex, float scale);
    void removePreviewForPage(PDFDocumentLayout::PageIndex);
    void invalidatePreviewsForPageCoverage(const PDFPageCoverage&);

    void setShowDebugBorders(bool);

private:
    AsyncPDFRenderer(PDFPresentationController&);

    RefPtr<WebCore::GraphicsLayer> layerForTileGrid(WebCore::TileGridIdentifier) const;

    TileRenderInfo renderInfoForFullTile(const WebCore::TiledBacking&, const TileForGrid& tileInfo, const WebCore::FloatRect& tileRect) const;
    TileRenderInfo renderInfoForTile(const WebCore::TiledBacking&, const TileForGrid& tileInfo, const WebCore::FloatRect& tileRect, const WebCore::FloatRect& renderRect, RefPtr<WebCore::NativeImage>&& background) const;

    bool renderInfoIsValidForTile(WebCore::TiledBacking&, const TileForGrid&, const TileRenderInfo&) const;

    // TiledBackingClient
    void willRepaintTile(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileIndex, const WebCore::FloatRect& tileRect, const WebCore::FloatRect& tileDirtyRect) final;
    void willRemoveTile(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileIndex) final;
    void willRepaintAllTiles(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;

    void coverageRectDidChange(WebCore::TiledBacking&, const WebCore::FloatRect&) final;

    void willRevalidateTiles(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileRevalidationType) final;
    void didRevalidateTiles(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileRevalidationType, const HashSet<WebCore::TileIndex>& tilesNeedingDisplay) final;

    void willRepaintTilesAfterScaleFactorChange(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;
    void didRepaintTilesAfterScaleFactorChange(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;

    void didAddGrid(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;
    void willRemoveGrid(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> dynamicContentScalingDisplayListForTile(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileIndex) final;
#endif

    std::optional<PDFTileRenderIdentifier> enqueueTileRenderForTileGridRepaint(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileIndex, const WebCore::FloatRect& tileRect, const WebCore::FloatRect& tileDirtyRect);
    std::optional<PDFTileRenderIdentifier> enqueueTileRenderIfNecessary(const TileForGrid&, TileRenderInfo&&);

    void serviceRequestQueues();

    void didCompleteTileRender(const TileForGrid& renderKey, PDFTileRenderIdentifier, RenderedPDFTile);

    struct RevalidationStateForGrid {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RevalidationStateForGrid);
        bool inFullTileRevalidation { false };
        bool inScaleChangeRepaint { false };
        HashSet<PDFTileRenderIdentifier> renderIdentifiersForCurrentRevalidation;
    };

    RevalidationStateForGrid& revalidationStateForGrid(WebCore::TileGridIdentifier);
    void trackRendersForStaleTileMaintenance(WebCore::TileGridIdentifier, HashSet<PDFTileRenderIdentifier>&&);
    void trackRenderCompletionForStaleTileMaintenance(WebCore::TileGridIdentifier, PDFTileRenderIdentifier);

    void clearRequestsAndCachedTiles();

    void didCompletePagePreviewRender(RefPtr<WebCore::NativeImage>&&, const PDFPagePreviewRenderRequest&);
    void removePagePreviewsOutsideCoverageRect(const WebCore::FloatRect&, const std::optional<PDFLayoutRow>& = { });
    void ensurePreviewsForCurrentPageCoverage();

    static WebCore::FloatRect convertTileRectToPaintingCoords(const WebCore::FloatRect&, float pageScaleFactor);
    static WebCore::AffineTransform tileToPaintingTransform(float tilingScaleFactor);
    static WebCore::AffineTransform paintingToTileTransform(float tilingScaleFactor);
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WebCore::DynamicContentScalingResourceCache ensureDynamicContentScalingResourceCache();
#endif

    ThreadSafeWeakPtr<PDFPresentationController> m_presentationController;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WebCore::DynamicContentScalingResourceCache m_dynamicContentScalingResourceCache;
#endif

    HashMap<WebCore::PlatformLayerIdentifier, Ref<WebCore::GraphicsLayer>> m_layerIDtoLayerMap;
    HashMap<WebCore::TileGridIdentifier, WebCore::PlatformLayerIdentifier> m_tileGridToLayerIDMap;

    const Ref<ConcurrentWorkQueue> m_workQueue;
    int m_workQueueSlots { 4 };

    ListHashSet<TileForGrid> m_pendingTileRenderOrder;
    HashMap<TileForGrid, TileRenderData> m_pendingTileRenders;
    HashMap<TileForGrid, RenderedPDFTile> m_rendereredTiles;
    HashMap<TileForGrid, RenderedPDFTile> m_rendereredTilesForOldState;

    HashMap<WebCore::TileGridIdentifier, std::unique_ptr<RevalidationStateForGrid>> m_gridRevalidationState;

    struct RenderedPDFPagePreview {
        RefPtr<WebCore::NativeImage> image;
        float scale { 1.0f };
    };
    ListHashSet<PDFPagePreviewRenderKey> m_pendingPagePreviewOrder;
    HashMap<PDFPagePreviewRenderKey, PDFPagePreviewRenderRequest> m_pendingPagePreviews;
    HashMap<PDFPagePreviewRenderKey, RenderedPDFPagePreview> m_pagePreviews;

    std::optional<PDFPageCoverage> m_currentPageCoverage;

    bool m_showDebugBorders { false };
};

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
