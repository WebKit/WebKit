/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "FindOverlaySession.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/Markable.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

OBJC_CLASS WKFindOverlayLayer;

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxyMac;
class RemoteLayerTreeTransaction;
class WebPageProxy;

// Draws the find-in-page veil from the UI process: one tile layer per local
// root, parented inside that root's committed paintless reserved-slot layer so
// the veil rides async scroll structurally. Tiles are mutated inside the layer
// tree commit turn, sharing the implicit CATransaction with the content whose
// geometry they annotate; visibility is driven by the page's
// FindOverlaySession verdict, with the web-side slot fade owning dismissal.
class FindOverlayPresenter final {
    WTF_MAKE_TZONE_ALLOCATED(FindOverlayPresenter);
    WTF_MAKE_NONCOPYABLE(FindOverlayPresenter);
public:
    explicit FindOverlayPresenter(RemoteLayerTreeDrawingAreaProxyMac&);
    ~FindOverlayPresenter();

    void findOverlaySessionDidChange();
    void didCommitTransaction(const RemoteLayerTreeTransaction&);

    size_t tileCountForTesting() const { return m_tiles.size(); }

private:
    WebPageProxy* page() const;
    void updateTileForRoot(WebCore::FrameIdentifier rootFrameID, const RemoteLayerTreeTransaction&, FindOverlaySession&);
    void refreshCutoutClearRectsForAllTiles(FindOverlaySession&);
    static Vector<WebCore::FloatRect> cutoutClearRectsForGeometry(const FindOverlaySession::RootGeometry&, FindOverlaySession&);
    static bool sessionHasParentCutoutCoveringRoot(FindOverlaySession&, WebCore::FrameIdentifier);
    void removeDetachedTiles();
    void removeTileForRoot(WebCore::FrameIdentifier);
    void removeAllTiles();
    void setVeilVisible(bool);

    struct Tile {
        RetainPtr<WKFindOverlayLayer> layer;
        Markable<WebCore::PlatformLayerIdentifier> slotLayerID;
    };

    WeakRef<RemoteLayerTreeDrawingAreaProxyMac> m_drawingArea;
    HashMap<WebCore::FrameIdentifier, Tile> m_tiles;
    bool m_veilVisible { false };
};

} // namespace WebKit

#endif // PLATFORM(MAC)
