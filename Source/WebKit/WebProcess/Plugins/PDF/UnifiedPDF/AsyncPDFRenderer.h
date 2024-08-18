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
#include <wtf/HashMap.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

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
    static void constructDeletedValue(WebKit::TileForGrid& tileForGrid) { HashTraits<WebCore::TileGridIdentifier>::constructDeletedValue(tileForGrid.gridIdentifier); }
    static bool isDeletedValue(const WebKit::TileForGrid& tileForGrid) { return tileForGrid.gridIdentifier.isHashTableDeletedValue(); }
};
template<> struct DefaultHash<WebKit::TileForGrid> : TileForGridHash { };

} // namespace WTF

namespace WebKit {

class PDFPresentationController;

struct PDFTileRenderType;
using PDFTileRenderIdentifier = LegacyNullableObjectIdentifier<PDFTileRenderType>;

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

    bool paintTilesForPage(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, float documentScale, const WebCore::FloatRect& clipRect, const WebCore::FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex);
    void paintPagePreview(WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect, const WebCore::FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex);

    // Throws away existing tiles. Can result in flashing.
    void invalidateTilesForPaintingRect(float pageScaleFactor, const WebCore::FloatRect& paintingRect);

    // Updates existing tiles. Can result in temporarily stale content.
    void pdfContentChangedInRect(const WebCore::GraphicsLayer*, float pageScaleFactor, const WebCore::FloatRect& paintingRect, std::optional<PDFLayoutRow>);

    void generatePreviewImageForPage(PDFDocumentLayout::PageIndex, float scale);
    RefPtr<WebCore::ImageBuffer> previewImageForPage(PDFDocumentLayout::PageIndex) const;
    void removePreviewForPage(PDFDocumentLayout::PageIndex);

    void setShowDebugBorders(bool);

private:
    AsyncPDFRenderer(PDFPresentationController&);

    WebCore::GraphicsLayer* layerForTileGrid(WebCore::TileGridIdentifier) const;

    struct TileRenderInfo {
        WebCore::FloatRect tileRect;
        std::optional<WebCore::FloatRect> clipRect; // If set, represents the portion of the tile that needs repaint (in the same coordinate system as tileRect).
        PDFPageCoverageAndScales pageCoverage;

        bool equivalentForPainting(const TileRenderInfo& other) const
        {
            return tileRect == other.tileRect && pageCoverage == other.pageCoverage;
        }
    };

    TileRenderInfo renderInfoForTile(const WebCore::TiledBacking&, const TileForGrid& tileInfo, const WebCore::FloatRect& tileRect, const std::optional<WebCore::FloatRect>& clipRect = { }) const;

    bool renderInfoIsValidForTile(WebCore::TiledBacking&, const TileForGrid&, const TileRenderInfo&) const;

    // TiledBackingClient
    void willRepaintTile(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileIndex, const WebCore::FloatRect& tileRect, const WebCore::FloatRect& tileDirtyRect) final;
    void willRemoveTile(WebCore::TiledBacking&, WebCore::TileGridIdentifier, WebCore::TileIndex) final;
    void willRepaintAllTiles(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;

    void coverageRectDidChange(WebCore::TiledBacking&, const WebCore::FloatRect&) final;
    void tilingScaleFactorDidChange(WebCore::TiledBacking&, float) final;

    void didAddGrid(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;
    void willRemoveGrid(WebCore::TiledBacking&, WebCore::TileGridIdentifier) final;

    void enqueueTilePaintIfNecessary(const WebCore::TiledBacking&, const TileForGrid&, const WebCore::FloatRect& tileRect, const std::optional<WebCore::FloatRect>& clipRect = { });
    void enqueuePaintWithClip(const TileForGrid&, const TileRenderInfo&);

    void serviceRequestQueue();

    void paintTileOnWorkQueue(RetainPtr<PDFDocument>&&, const TileForGrid&, const TileRenderInfo&, PDFTileRenderIdentifier);
    void paintPDFIntoBuffer(RetainPtr<PDFDocument>&&, Ref<WebCore::ImageBuffer>, const TileForGrid&, const TileRenderInfo&);
    void transferBufferToMainThread(RefPtr<WebCore::ImageBuffer>&&, const TileForGrid&, const TileRenderInfo&, PDFTileRenderIdentifier);

    void didCompleteTileRender(RefPtr<WebCore::ImageBuffer>&&, const TileForGrid&, const TileRenderInfo&, PDFTileRenderIdentifier, const WebCore::GraphicsLayer* tileGridLayer);

    void clearRequestsAndCachedTiles();

    struct PagePreviewRequest {
        PDFDocumentLayout::PageIndex pageIndex;
        WebCore::FloatRect normalizedPageBounds;
        float scale;
    };

    void paintPagePreviewOnWorkQueue(RetainPtr<PDFDocument>&&, const PagePreviewRequest&);
    void didCompletePagePreviewRender(RefPtr<WebCore::ImageBuffer>&&, const PagePreviewRequest&);
    void removePagePreviewsOutsideCoverageRect(const WebCore::FloatRect&, const std::optional<PDFLayoutRow>& = { });

    void paintPDFPageIntoBuffer(RetainPtr<PDFDocument>&&, Ref<WebCore::ImageBuffer>, PDFDocumentLayout::PageIndex, const WebCore::FloatRect& pageBounds);

    static WebCore::FloatRect convertTileRectToPaintingCoords(const WebCore::FloatRect&, float pageScaleFactor);
    static WebCore::AffineTransform tileToPaintingTransform(float tilingScaleFactor);
    static WebCore::AffineTransform paintingToTileTransform(float tilingScaleFactor);

    ThreadSafeWeakPtr<PDFPresentationController> m_presentationController;

    HashMap<WebCore::PlatformLayerIdentifier, Ref<WebCore::GraphicsLayer>> m_layerIDtoLayerMap;
    HashMap<WebCore::TileGridIdentifier, WebCore::PlatformLayerIdentifier> m_tileGridToLayerIDMap;

    Ref<ConcurrentWorkQueue> m_paintingWorkQueue;

    struct TileRenderData {
        PDFTileRenderIdentifier renderIdentifier;
        TileRenderInfo renderInfo;
    };
    HashMap<TileForGrid, TileRenderData> m_currentValidTileRenders;

    const int m_maxConcurrentTileRenders { 4 };
    int m_numConcurrentTileRenders { 0 };
    ListHashSet<TileForGrid> m_requestWorkQueue;

    struct RenderedTile {
        RefPtr<WebCore::ImageBuffer> buffer;
        TileRenderInfo tileInfo;
    };
    HashMap<TileForGrid, RenderedTile> m_rendereredTiles;

    using PDFPageIndexSet = HashSet<PDFDocumentLayout::PageIndex, IntHash<PDFDocumentLayout::PageIndex>, WTF::UnsignedWithZeroKeyHashTraits<PDFDocumentLayout::PageIndex>>;
    using PDFPageIndexToPreviewHash = HashMap<PDFDocumentLayout::PageIndex, PagePreviewRequest, IntHash<PDFDocumentLayout::PageIndex>, WTF::UnsignedWithZeroKeyHashTraits<PDFDocumentLayout::PageIndex>>;
    using PDFPageIndexToBufferHash = HashMap<PDFDocumentLayout::PageIndex, RefPtr<WebCore::ImageBuffer>, IntHash<PDFDocumentLayout::PageIndex>, WTF::UnsignedWithZeroKeyHashTraits<PDFDocumentLayout::PageIndex>>;

    PDFPageIndexToPreviewHash m_enqueuedPagePreviews;
    PDFPageIndexToBufferHash m_pagePreviews;

    std::atomic<bool> m_showDebugBorders { false };
};


} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
