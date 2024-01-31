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
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/TextureMapperSparseBackingStore.h>
#include <optional>
#include <wtf/EnumTraits.h>
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
    OptionSet<WCLayerChange> changes;
    Vector<WebCore::PlatformLayerIdentifier> children;
    std::optional<WebCore::PlatformLayerIdentifier> maskLayer;
    std::optional<WebCore::PlatformLayerIdentifier> replicaLayer;
    WebCore::FloatPoint position;
    WebCore::FloatPoint3D anchorPoint;
    WebCore::FloatSize size;
    WebCore::FloatPoint boundsOrigin;
    bool masksToBounds;
    bool contentsRectClipsDescendants;
    bool showDebugBorder;
    bool showRepaintCounter;
    bool contentsVisible;
    bool backfaceVisibility;
    bool preserves3D;
    bool hasPlatformLayer;
    bool hasBackingStore;
    WebCore::Color solidColor;
    WebCore::Color backgroundColor;
    WebCore::Color debugBorderColor;
    float opacity;
    float debugBorderWidth;
    int repaintCount;
    WebCore::FloatRect contentsRect;
    WebCore::TransformationMatrix transform;
    WebCore::TransformationMatrix childrenTransform;
    WebCore::FilterOperations filters;
    WebCore::FilterOperations backdropFilters;
    WebCore::FloatRoundedRect backdropFiltersRect;
    WebCore::FloatRoundedRect contentsClippingRect;
    Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier;
    Vector<WCTileUpdate> tileUpdate;
    Vector<WCContentBufferIdentifier> contentBufferIdentifiers;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << id;
        encoder << changes;
        if (changes & WCLayerChange::Children)
            encoder << children;
        if (changes & WCLayerChange::MaskLayer)
            encoder << maskLayer;
        if (changes & WCLayerChange::ReplicaLayer)
            encoder << replicaLayer;
        if (changes & WCLayerChange::Position)
            encoder << position;
        if (changes & WCLayerChange::AnchorPoint)
            encoder << anchorPoint;
        if (changes & WCLayerChange::Size)
            encoder << size;
        if (changes & WCLayerChange::BoundsOrigin)
            encoder << boundsOrigin;
        if (changes & WCLayerChange::Preserves3D)
            encoder << preserves3D;
        if (changes & WCLayerChange::ContentsVisible)
            encoder << contentsVisible;
        if (changes & WCLayerChange::BackfaceVisibility)
            encoder << backfaceVisibility;
        if (changes & WCLayerChange::MasksToBounds)
            encoder << masksToBounds;
        if (changes & WCLayerChange::SolidColor)
            encoder << solidColor;
        if (changes & WCLayerChange::ShowDebugBorder)
            encoder << showDebugBorder;
        if (changes & WCLayerChange::DebugBorderColor)
            encoder << debugBorderColor;
        if (changes & WCLayerChange::DebugBorderWidth)
            encoder << debugBorderWidth;
        if (changes & WCLayerChange::ShowRepaintCounter)
            encoder << showRepaintCounter;
        if (changes & WCLayerChange::RepaintCount)
            encoder << repaintCount;
        if (changes & WCLayerChange::ContentsRect)
            encoder << contentsRect;
        if (changes & WCLayerChange::ContentsClippingRect)
            encoder << contentsClippingRect;
        if (changes & WCLayerChange::ContentsRectClipsDescendants)
            encoder << contentsRectClipsDescendants;
        if (changes & WCLayerChange::Opacity)
            encoder << opacity;
        if (changes & WCLayerChange::Background)
            encoder << backgroundColor << hasBackingStore << tileUpdate;
        if (changes & WCLayerChange::Transform)
            encoder << transform;
        if (changes & WCLayerChange::ChildrenTransform)
            encoder << childrenTransform;
        if (changes & WCLayerChange::Filters)
            encoder << filters;
        if (changes & WCLayerChange::BackdropFilters)
            encoder << backdropFilters;
        if (changes & WCLayerChange::BackdropFiltersRect)
            encoder << backdropFiltersRect;
        if (changes & WCLayerChange::PlatformLayer)
            encoder << hasPlatformLayer << contentBufferIdentifiers;
        if (changes & WCLayerChange::RemoteFrame)
            encoder << hostIdentifier;
    }

    template <class Decoder>
    static std::optional<WCLayerUpdateInfo> decode(Decoder& decoder)
    {
        WCLayerUpdateInfo result;

        std::optional<WebCore::PlatformLayerIdentifier> id;
        decoder >> id;
        if (UNLIKELY(!id))
            return std::nullopt;
        result.id = WTFMove(*id);

        std::optional<OptionSet<WCLayerChange>> changes;
        decoder >> changes;
        if (UNLIKELY(!changes))
            return std::nullopt;
        result.changes = WTFMove(*changes);

        if (result.changes & WCLayerChange::Children) {
            std::optional<Vector<WebCore::PlatformLayerIdentifier>> children;
            decoder >> children;
            if (UNLIKELY(!children))
                return std::nullopt;
            result.children = WTFMove(*children);
        }

        if (result.changes & WCLayerChange::MaskLayer) {
            std::optional<std::optional<WebCore::PlatformLayerIdentifier>> maskLayer;
            decoder >> maskLayer;
            if (UNLIKELY(!maskLayer))
                return std::nullopt;
            result.maskLayer = WTFMove(*maskLayer);
        }
        if (result.changes & WCLayerChange::ReplicaLayer) {
            std::optional<std::optional<WebCore::PlatformLayerIdentifier>> replicaLayer;
            decoder >> replicaLayer;
            if (UNLIKELY(!replicaLayer))
                return std::nullopt;
            result.replicaLayer = WTFMove(*replicaLayer);
        }
        if (result.changes & WCLayerChange::Position) {
            std::optional<WebCore::FloatPoint> position;
            decoder >> position;
            if (UNLIKELY(!position))
                return std::nullopt;
            result.position = WTFMove(*position);
        }
        if (result.changes & WCLayerChange::AnchorPoint) {
            std::optional<WebCore::FloatPoint3D> anchorPoint;
            decoder >> anchorPoint;
            if (UNLIKELY(!anchorPoint))
                return std::nullopt;
            result.anchorPoint = WTFMove(*anchorPoint);
        }
        if (result.changes & WCLayerChange::Size) {
            std::optional<WebCore::FloatSize> size;
            decoder >> size;
            if (UNLIKELY(!size))
                return std::nullopt;
            result.size = WTFMove(*size);
        }
        if (result.changes & WCLayerChange::BoundsOrigin) {
            std::optional<WebCore::FloatPoint> boundsOrigin;
            decoder >> boundsOrigin;
            if (UNLIKELY(!boundsOrigin))
                return std::nullopt;
            result.boundsOrigin = WTFMove(*boundsOrigin);
        }
        if (result.changes & WCLayerChange::Preserves3D) {
            std::optional<bool> preserves3D;
            decoder >> preserves3D;
            if (UNLIKELY(!preserves3D))
                return std::nullopt;
            result.preserves3D = WTFMove(*preserves3D);
        }
        if (result.changes & WCLayerChange::ContentsVisible) {
            std::optional<bool> contentsVisible;
            decoder >> contentsVisible;
            if (UNLIKELY(!contentsVisible))
                return std::nullopt;
            result.contentsVisible = WTFMove(*contentsVisible);
        }
        if (result.changes & WCLayerChange::BackfaceVisibility) {
            std::optional<bool> backfaceVisibility;
            decoder >> backfaceVisibility;
            if (UNLIKELY(!backfaceVisibility))
                return std::nullopt;
            result.backfaceVisibility = WTFMove(*backfaceVisibility);
        }
        if (result.changes & WCLayerChange::MasksToBounds) {
            std::optional<bool> masksToBounds;
            decoder >> masksToBounds;
            if (UNLIKELY(!masksToBounds))
                return std::nullopt;
            result.masksToBounds = WTFMove(*masksToBounds);
        }
        if (result.changes & WCLayerChange::SolidColor) {
            std::optional<WebCore::Color> solidColor;
            decoder >> solidColor;
            if (UNLIKELY(!solidColor))
                return std::nullopt;
            result.solidColor = WTFMove(*solidColor);
        }
        if (result.changes & WCLayerChange::ShowDebugBorder) {
            std::optional<bool> showDebugBorder;
            decoder >> showDebugBorder;
            if (UNLIKELY(!showDebugBorder))
                return std::nullopt;
            result.showDebugBorder = WTFMove(*showDebugBorder);
        }
        if (result.changes & WCLayerChange::DebugBorderColor) {
            std::optional<WebCore::Color> debugBorderColor;
            decoder >> debugBorderColor;
            if (UNLIKELY(!debugBorderColor))
                return std::nullopt;
            result.debugBorderColor = WTFMove(*debugBorderColor);
        }
        if (result.changes & WCLayerChange::DebugBorderWidth) {
            std::optional<float> debugBorderWidth;
            decoder >> debugBorderWidth;
            if (UNLIKELY(!debugBorderWidth))
                return std::nullopt;
            result.debugBorderWidth = WTFMove(*debugBorderWidth);
        }
        if (result.changes & WCLayerChange::ShowRepaintCounter) {
            std::optional<bool> showRepaintCounter;
            decoder >> showRepaintCounter;
            if (UNLIKELY(!showRepaintCounter))
                return std::nullopt;
            result.showRepaintCounter = WTFMove(*showRepaintCounter);
        }
        if (result.changes & WCLayerChange::RepaintCount) {
            std::optional<int> repaintCount;
            decoder >> repaintCount;
            if (UNLIKELY(!repaintCount))
                return std::nullopt;
            result.repaintCount = WTFMove(*repaintCount);
        }
        if (result.changes & WCLayerChange::ContentsRect) {
            std::optional<WebCore::FloatRect> contentsRect;
            decoder >> contentsRect;
            if (UNLIKELY(!contentsRect))
                return std::nullopt;
            result.contentsRect = WTFMove(*contentsRect);
        }
        if (result.changes & WCLayerChange::ContentsClippingRect) {
            std::optional<WebCore::FloatRoundedRect> contentsClippingRect;
            decoder >> contentsClippingRect;
            if (UNLIKELY(!contentsClippingRect))
                return std::nullopt;
            result.contentsClippingRect = WTFMove(*contentsClippingRect);
        }
        if (result.changes & WCLayerChange::ContentsRectClipsDescendants) {
            std::optional<bool> contentsRectClipsDescendants;
            decoder >> contentsRectClipsDescendants;
            if (UNLIKELY(!contentsRectClipsDescendants))
                return std::nullopt;
            result.contentsRectClipsDescendants = WTFMove(*contentsRectClipsDescendants);
        }
        if (result.changes & WCLayerChange::Opacity) {
            std::optional<float> opacity;
            decoder >> opacity;
            if (UNLIKELY(!opacity))
                return std::nullopt;
            result.opacity = WTFMove(*opacity);
        }
        if (result.changes & WCLayerChange::Background) {
            std::optional<WebCore::Color> backgroundColor;
            decoder >> backgroundColor;
            if (UNLIKELY(!backgroundColor))
                return std::nullopt;
            result.backgroundColor = WTFMove(*backgroundColor);
            std::optional<bool> hasBackingStore;
            decoder >> hasBackingStore;
            if (UNLIKELY(!hasBackingStore))
                return std::nullopt;
            result.hasBackingStore = WTFMove(*hasBackingStore);
            std::optional<Vector<WCTileUpdate>> tileUpdate;
            decoder >> tileUpdate;
            if (UNLIKELY(!tileUpdate))
                return std::nullopt;
            result.tileUpdate = WTFMove(*tileUpdate);
        }
        if (result.changes & WCLayerChange::Transform) {
            std::optional<WebCore::TransformationMatrix> transform;
            decoder >> transform;
            if (UNLIKELY(!transform))
                return std::nullopt;
            result.transform = WTFMove(*transform);
        }
        if (result.changes & WCLayerChange::ChildrenTransform) {
            std::optional<WebCore::TransformationMatrix> childrenTransform;
            decoder >> childrenTransform;
            if (UNLIKELY(!childrenTransform))
                return std::nullopt;
            result.childrenTransform = WTFMove(*childrenTransform);
        }
        if (result.changes & WCLayerChange::Filters) {
            std::optional<WebCore::FilterOperations> filters;
            decoder >> filters;
            if (UNLIKELY(!filters))
                return std::nullopt;
            result.filters = WTFMove(*filters);
        }
        if (result.changes & WCLayerChange::BackdropFilters) {
            std::optional<WebCore::FilterOperations> backdropFilters;
            decoder >> backdropFilters;
            if (UNLIKELY(!backdropFilters))
                return std::nullopt;
            result.backdropFilters = WTFMove(*backdropFilters);
        }
        if (result.changes & WCLayerChange::BackdropFiltersRect) {
            std::optional<WebCore::FloatRoundedRect> backdropFiltersRect;
            decoder >> backdropFiltersRect;
            if (UNLIKELY(!backdropFiltersRect))
                return std::nullopt;
            result.backdropFiltersRect = WTFMove(*backdropFiltersRect);
        }
        if (result.changes & WCLayerChange::PlatformLayer) {
            std::optional<bool> hasPlatformLayer;
            decoder >> hasPlatformLayer;
            if (UNLIKELY(!hasPlatformLayer))
                return std::nullopt;
            result.hasPlatformLayer = WTFMove(*hasPlatformLayer);
            std::optional<Vector<WCContentBufferIdentifier>> contentBufferIdentifiers;
            decoder >> contentBufferIdentifiers;
            if (UNLIKELY(!contentBufferIdentifiers))
                return std::nullopt;
            result.contentBufferIdentifiers = WTFMove(*contentBufferIdentifiers);
        }
        if (result.changes & WCLayerChange::RemoteFrame) {
            std::optional<Markable<WebCore::LayerHostingContextIdentifier>> hostIdentifier;
            decoder >> hostIdentifier;
            if (UNLIKELY(!hostIdentifier))
                return std::nullopt;
            result.hostIdentifier = WTFMove(*hostIdentifier);
        }
        return result;
    }
};

struct WCUpdateInfo {
    Markable<WebCore::LayerHostingContextIdentifier> remoteContextHostedIdentifier;
    WebCore::PlatformLayerIdentifier rootLayer;
    Vector<WebCore::PlatformLayerIdentifier> addedLayers;
    Vector<WebCore::PlatformLayerIdentifier> removedLayers;
    Vector<WCLayerUpdateInfo> changedLayers;
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WCLayerChange> {
    using values = EnumValues < WebKit::WCLayerChange,
        WebKit::WCLayerChange::Children,
        WebKit::WCLayerChange::MaskLayer,
        WebKit::WCLayerChange::ReplicaLayer,
        WebKit::WCLayerChange::Position,
        WebKit::WCLayerChange::AnchorPoint,
        WebKit::WCLayerChange::Size,
        WebKit::WCLayerChange::BoundsOrigin,
        WebKit::WCLayerChange::MasksToBounds,
        WebKit::WCLayerChange::ContentsRectClipsDescendants,
        WebKit::WCLayerChange::ShowDebugBorder,
        WebKit::WCLayerChange::ShowRepaintCounter,
        WebKit::WCLayerChange::ContentsVisible,
        WebKit::WCLayerChange::BackfaceVisibility,
        WebKit::WCLayerChange::Preserves3D,
        WebKit::WCLayerChange::SolidColor,
        WebKit::WCLayerChange::DebugBorderColor,
        WebKit::WCLayerChange::Opacity,
        WebKit::WCLayerChange::DebugBorderWidth,
        WebKit::WCLayerChange::RepaintCount,
        WebKit::WCLayerChange::ContentsRect,
        WebKit::WCLayerChange::Background,
        WebKit::WCLayerChange::Transform,
        WebKit::WCLayerChange::ChildrenTransform,
        WebKit::WCLayerChange::Filters,
        WebKit::WCLayerChange::BackdropFilters,
        WebKit::WCLayerChange::BackdropFiltersRect,
        WebKit::WCLayerChange::ContentsClippingRect,
        WebKit::WCLayerChange::PlatformLayer,
        WebKit::WCLayerChange::RemoteFrame
    >;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
