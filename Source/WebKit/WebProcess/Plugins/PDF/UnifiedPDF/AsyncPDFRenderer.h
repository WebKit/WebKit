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
    bool paintTilesForPaintingRect(WebCore::GraphicsContext&, float pageScaleFactor, const WebCore::FloatRect& paintingRect);

    void invalidateTilesForPaintingRect(float pageScaleFactor, const WebCore::FloatRect& paintingRect);

    void setShowDebugBorders(bool);

    void layoutConfigurationChanged();

private:
    AsyncPDFRenderer(UnifiedPDFPlugin&);

    struct TileRenderInfo {
        PDFPageCoverage pageCoverage;
        PDFConfigurationIdentifier configurationIdentifier;
    };

    // TiledBackingClient
    void willRepaintTile(WebCore::TileGridIndex, WebCore::TileIndex, const WebCore::FloatRect& tileRect, const WebCore::FloatRect& tileDirtyRect) final;
    void willRemoveTile(WebCore::TileGridIndex, WebCore::TileIndex) final;
    void willRepaintAllTiles(WebCore::TileGridIndex) final;

    void enqueuePaintWithClip(const TileForGrid&, const WebCore::FloatRect& tileRect);
    void paintTileOnWorkQueue(RetainPtr<PDFDocument>&&, const TileForGrid&, const WebCore::FloatRect& tileRect, const TileRenderInfo&);
    void paintPDFIntoBuffer(RetainPtr<PDFDocument>&&, Ref<WebCore::ImageBuffer>, const TileForGrid&, const WebCore::FloatRect& tileRect, const TileRenderInfo&);
    void transferBufferToMainThread(RefPtr<WebCore::ImageBuffer>&&, const TileForGrid&, const WebCore::FloatRect& tileRect, const TileRenderInfo&);

    void clearRequestsAndCachedTiles();

    static WebCore::FloatRect convertTileRectToPaintingCoords(const WebCore::FloatRect&, float pageScaleFactor);
    static WebCore::AffineTransform tileToPaintingTransform(float pageScaleFactor);

    ThreadSafeWeakPtr<UnifiedPDFPlugin> m_plugin;
    RefPtr<WebCore::GraphicsLayer> m_pdfContentsLayer;
    Ref<ConcurrentWorkQueue> m_paintingWorkQueue;

    PDFConfigurationIdentifier m_currentConfigurationIdentifier;

    HashMap<TileForGrid, TileRenderInfo> m_enqueuedTileRenders;

    struct BufferAndClip {
        RefPtr<WebCore::ImageBuffer> buffer;
        WebCore::FloatRect tileClip;
        PDFConfigurationIdentifier configurationIdentifier;
    };
    HashMap<TileForGrid, BufferAndClip> m_rendereredTiles;

    std::atomic<bool> m_showDebugBorders { false };
};


} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
