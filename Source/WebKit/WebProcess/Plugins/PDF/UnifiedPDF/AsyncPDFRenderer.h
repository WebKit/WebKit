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
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

OBJC_CLASS PDFDocument;

namespace WebKit {

struct TileForGrid {
    WebCore::TileGridIndex gridIndex { 0 };
    WebCore::TileIndex tileIndex;

    bool operator==(const TileForGrid&) const = default;
};

WTF::TextStream& operator<<(WTF::TextStream&, const TileForGrid&);

} // namespace WebKit

namespace WTF {

struct TileForGridHash {
    static unsigned hash(const WebKit::TileForGrid& key)
    {
        return pairIntHash(key.gridIndex, DefaultHash<WebCore::IntPoint>::hash(key.tileIndex));
    }
    static bool equal(const WebKit::TileForGrid& a, const WebKit::TileForGrid& b)
    {
        return a.gridIndex == b.gridIndex && DefaultHash<WebCore::IntPoint>::equal(a.tileIndex, b.tileIndex);

    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebKit::TileForGrid> : GenericHashTraits<WebKit::TileForGrid> {
    static const bool emptyValueIsZero = false;
    static WebKit::TileForGrid emptyValue()  { return { std::numeric_limits<unsigned>::max(), { -1, -1 } }; }
    static WebKit::TileForGrid deletedValue() { return { 0, { -1, -1 } }; }
    static void constructDeletedValue(WebKit::TileForGrid& tileForGrid) { tileForGrid = deletedValue(); }
    static bool isDeletedValue(const WebKit::TileForGrid& tileForGrid) { return tileForGrid == deletedValue(); }
};
template<> struct DefaultHash<WebKit::TileForGrid> : TileForGridHash { };

} // namespace WTF

namespace WebKit {

class UnifiedPDFPlugin;

struct PDFConfiguration;
using PDFConfigurationIdentifier = ObjectIdentifier<PDFConfiguration>;

class AsyncPDFRenderer : public WebCore::TiledBackingClient,
    public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AsyncPDFRenderer> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AsyncPDFRenderer);
public:
    static Ref<AsyncPDFRenderer> create(UnifiedPDFPlugin&);

    virtual ~AsyncPDFRenderer();

    void setupWithLayer(WebCore::GraphicsLayer&);
    void teardown();
    bool paintTilesForPage(WebCore::GraphicsContext&, float documentScale, const WebCore::FloatRect& clipRect, const WebCore::FloatRect& pageBoundsInPaintingCoordinates, PDFDocumentLayout::PageIndex);

    // Throws away existing tiles. Can result in flashing.
    void invalidateTilesForPaintingRect(float pageScaleFactor, const WebCore::FloatRect& paintingRect);

    // Updates existing tiles. Can result in temporarily stale content.
    void updateTilesForPaintingRect(float pageScaleFactor, const WebCore::FloatRect& paintingRect);

    void generatePreviewImageForPage(PDFDocumentLayout::PageIndex, float scale);
    RefPtr<WebCore::ImageBuffer> previewImageForPage(PDFDocumentLayout::PageIndex) const;
    void removePreviewForPage(PDFDocumentLayout::PageIndex);

    void setShowDebugBorders(bool);

    void layoutConfigurationChanged();

private:
    AsyncPDFRenderer(UnifiedPDFPlugin&);

    struct TileRenderInfo {
        WebCore::FloatRect tileRect;
        std::optional<WebCore::FloatRect> clipRect; // If set, represents the portion of the tile that needs repaint (in the same coordinate system as tileRect).
        PDFPageCoverage pageCoverage;
        PDFConfigurationIdentifier configurationIdentifier;
    };

    enum class TileRenderRequestType : uint8_t {
        NewTile,
        TileUpdate
    };

    // TiledBackingClient
    void willRepaintTile(WebCore::TileGridIndex, WebCore::TileIndex, const WebCore::FloatRect& tileRect, const WebCore::FloatRect& tileDirtyRect) final;
    void willRemoveTile(WebCore::TileGridIndex, WebCore::TileIndex) final;
    void willRepaintAllTiles(WebCore::TileGridIndex) final;
    void coverageRectDidChange(const WebCore::FloatRect&) final;
    void tilingScaleFactorDidChange(float) final;

    void enqueuePaintWithClip(const TileForGrid&, const WebCore::FloatRect& tileRect);
    void paintTileOnWorkQueue(RetainPtr<PDFDocument>&&, const TileForGrid&, const TileRenderInfo&, TileRenderRequestType);
    void paintPDFIntoBuffer(RetainPtr<PDFDocument>&&, Ref<WebCore::ImageBuffer>, const TileForGrid&, const TileRenderInfo&);
    void transferBufferToMainThread(RefPtr<WebCore::ImageBuffer>&&, const TileForGrid&, const TileRenderInfo&, TileRenderRequestType);

    void didCompleteNewTileRender(RefPtr<WebCore::ImageBuffer>&&, const TileForGrid&, const TileRenderInfo&);
    void didCompleteTileUpdateRender(RefPtr<WebCore::ImageBuffer>&&, const TileForGrid&, const TileRenderInfo&);

    void clearRequestsAndCachedTiles();

    struct PagePreviewRequest {
        PDFDocumentLayout::PageIndex pageIndex;
        WebCore::FloatRect normalizedPageBounds;
        float scale;
    };

    void paintPagePreviewOnWorkQueue(RetainPtr<PDFDocument>&&, const PagePreviewRequest&);
    void paintPDFPageIntoBuffer(RetainPtr<PDFDocument>&&, Ref<WebCore::ImageBuffer>, PDFDocumentLayout::PageIndex, const WebCore::FloatRect& pageBounds);

    static WebCore::FloatRect convertTileRectToPaintingCoords(const WebCore::FloatRect&, float pageScaleFactor);
    static WebCore::AffineTransform tileToPaintingTransform(float tilingScaleFactor);
    static WebCore::AffineTransform paintingToTileTransform(float tilingScaleFactor);

    ThreadSafeWeakPtr<UnifiedPDFPlugin> m_plugin;
    RefPtr<WebCore::GraphicsLayer> m_pdfContentsLayer;
    Ref<ConcurrentWorkQueue> m_paintingWorkQueue;

    PDFConfigurationIdentifier m_currentConfigurationIdentifier;

    HashMap<TileForGrid, TileRenderInfo> m_enqueuedTileRenders;
    HashMap<TileForGrid, TileRenderInfo> m_enqueuedTileUpdates;

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
