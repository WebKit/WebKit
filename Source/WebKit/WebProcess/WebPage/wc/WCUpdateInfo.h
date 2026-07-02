/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#if USE(GRAPHICS_LAYER_WC)

#include "WCBackingStore.h"
#include "WCContentBufferIdentifier.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/TextureMapperSparseBackingStore.h>
#include <optional>
#include <wtf/OptionSet.h>

namespace WebKit {

struct WCTileUpdate {
    WebCore::TextureMapperSparseBackingStore::TileIndex index;
    bool willRemove { false };
    WCBackingStore backingStore;
    WebCore::IntRect dirtyRect;
};

enum class WCLayerChange : uint32_t {
    Children                     = 1 <<  0,
    MaskLayer                    = 1 <<  1,
    ReplicaLayer                 = 1 <<  2,
    Position                     = 1 <<  3,
    AnchorPoint                  = 1 <<  4,
    Size                         = 1 <<  5,
    BoundsOrigin                 = 1 <<  6,
    MasksToBounds                = 1 <<  7,
    ContentsRectClipsDescendants = 1 <<  8,
    ShowDebugBorder              = 1 <<  9,
    ShowRepaintCounter           = 1 << 10,
    ContentsVisible              = 1 << 11,
    BackfaceVisibility           = 1 << 12,
    Preserves3D                  = 1 << 13,
    SolidColor                   = 1 << 14,
    DebugBorderColor             = 1 << 15,
    Opacity                      = 1 << 16,
    DebugBorderWidth             = 1 << 17,
    RepaintCount                 = 1 << 18,
    ContentsRect                 = 1 << 19,
    Background                   = 1 << 20,
    Transform                    = 1 << 21,
    ChildrenTransform            = 1 << 22,
    Filters                      = 1 << 23,
    BackdropFilters              = 1 << 24,
    BackdropFiltersRect          = 1 << 25,
    ContentsClippingRect         = 1 << 26,
    PlatformLayer                = 1 << 27,
    RemoteFrame                  = 1 << 28,
};

struct WCLayerUpdateInfo {
    WebCore::PlatformLayerIdentifier id;
    OptionSet<WCLayerChange> changes { };
    Vector<WebCore::PlatformLayerIdentifier> children { };
    std::optional<WebCore::PlatformLayerIdentifier> maskLayer { };
    std::optional<WebCore::PlatformLayerIdentifier> replicaLayer { };
    WebCore::FloatPoint position { };
    WebCore::FloatPoint3D anchorPoint { };
    WebCore::FloatSize size { };
    WebCore::FloatPoint boundsOrigin { };
    bool masksToBounds { false };
    bool contentsRectClipsDescendants { false };
    bool showDebugBorder { false };
    bool showRepaintCounter { false };
    bool contentsVisible { false };
    bool backfaceVisibility { false };
    bool preserves3D { false };
    WebCore::Color solidColor { };
    WebCore::Color debugBorderColor { };
    float opacity { 0 };
    float debugBorderWidth { 0 };
    int repaintCount { 0 };
    WebCore::FloatRect contentsRect { };

    struct BackgroundChanges {
        WebCore::Color color;
        bool hasBackingStore;
        WebCore::IntSize backingStoreSize;
        Vector<WCTileUpdate> tileUpdates;
    } background { };

    WebCore::TransformationMatrix transform { };
    WebCore::TransformationMatrix childrenTransform { };
    WebCore::FilterOperations filters { };
    WebCore::FilterOperations backdropFilters { };
    WebCore::FloatRoundedRect backdropFiltersRect { };
    WebCore::FloatRoundedRect contentsClippingRect { };

    struct PlatformLayerChanges {
        bool hasLayer;
        Vector<WCContentBufferIdentifier> identifiers;
    } platformLayer { };

    Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier { };
};

struct WCUpdateInfo {
    WebCore::IntSize viewport;
    Markable<WebCore::LayerHostingContextIdentifier> remoteContextHostedIdentifier;
    Markable<WebCore::PlatformLayerIdentifier> rootLayer;
    Vector<WebCore::PlatformLayerIdentifier> addedLayers;
    Vector<WebCore::PlatformLayerIdentifier> removedLayers;
    Vector<WCLayerUpdateInfo> changedLayers;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
