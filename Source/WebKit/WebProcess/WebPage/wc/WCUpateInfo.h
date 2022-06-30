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

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << index << willRemove;
        if (!willRemove)
            encoder << backingStore << dirtyRect;
    }

    template <class Decoder>
    static std::optional<WCTileUpdate> decode(Decoder& decoder)
    {
        WCTileUpdate result;
        if (!decoder.decode(result.index))
            return std::nullopt;
        if (!decoder.decode(result.willRemove))
            return std::nullopt;
        if (!result.willRemove) {
            if (!decoder.decode(result.backingStore))
                return std::nullopt;
            if (!decoder.decode(result.dirtyRect))
                return std::nullopt;
        }
        return result;
    }
};

enum class WCLayerChange : uint32_t {
    Children                = 1 <<  0,
    MaskLayer               = 1 <<  1,
    ReplicaLayer            = 1 <<  2,
    Geometry                = 1 <<  3,
    Preserves3D             = 1 <<  4,
    ContentsVisible         = 1 <<  5,
    BackfaceVisibility      = 1 <<  6,
    MasksToBounds           = 1 <<  7,
    SolidColor              = 1 <<  8,
    DebugVisuals            = 1 <<  9,
    RepaintCount            = 1 << 10,
    ContentsRect            = 1 << 11,
    ContentsClippingRect    = 1 << 12,
    Opacity                 = 1 << 13,
    Background              = 1 << 14,
    Transform               = 1 << 15,
    ChildrenTransform       = 1 << 16,
    Filters                 = 1 << 17,
    BackdropFilters         = 1 << 18,
    PlatformLayer           = 1 << 19,
};

struct WCLayerUpateInfo {
    WebCore::GraphicsLayer::PlatformLayerID id;
    OptionSet<WCLayerChange> changes;
    Vector<WebCore::GraphicsLayer::PlatformLayerID> children;
    std::optional<WebCore::GraphicsLayer::PlatformLayerID> maskLayer;
    std::optional<WebCore::GraphicsLayer::PlatformLayerID> replicaLayer;
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
        if (changes & WCLayerChange::Geometry)
            encoder << position << anchorPoint << size << boundsOrigin;
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
        if (changes & WCLayerChange::DebugVisuals)
            encoder << showDebugBorder << debugBorderColor << debugBorderWidth;
        if (changes & WCLayerChange::RepaintCount)
            encoder << showRepaintCounter << repaintCount;
        if (changes & WCLayerChange::ContentsRect)
            encoder << contentsRect;
        if (changes & WCLayerChange::ContentsClippingRect) {
            encoder << contentsClippingRect;
            encoder << contentsRectClipsDescendants;
        }
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
            encoder << backdropFilters << backdropFiltersRect;
        if (changes & WCLayerChange::PlatformLayer)
            encoder << hasPlatformLayer << contentBufferIdentifiers;
    }

    template <class Decoder>
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, WCLayerUpateInfo& result)
    {
        if (!decoder.decode(result.id))
            return false;
        if (!decoder.decode(result.changes))
            return false;
        if (result.changes & WCLayerChange::Children) {
            if (!decoder.decode(result.children))
                return false;
        }
        if (result.changes & WCLayerChange::MaskLayer) {
            if (!decoder.decode(result.maskLayer))
                return false;
        }
        if (result.changes & WCLayerChange::ReplicaLayer) {
            if (!decoder.decode(result.replicaLayer))
                return false;
        }
        if (result.changes & WCLayerChange::Geometry) {
            if (!decoder.decode(result.position))
                return false;
            if (!decoder.decode(result.anchorPoint))
                return false;
            if (!decoder.decode(result.size))
                return false;
            if (!decoder.decode(result.boundsOrigin))
                return false;
        }
        if (result.changes & WCLayerChange::Preserves3D) {
            if (!decoder.decode(result.preserves3D))
                return false;
        }
        if (result.changes & WCLayerChange::ContentsVisible) {
            if (!decoder.decode(result.contentsVisible))
                return false;
        }
        if (result.changes & WCLayerChange::BackfaceVisibility) {
            if (!decoder.decode(result.backfaceVisibility))
                return false;
        }
        if (result.changes & WCLayerChange::MasksToBounds) {
            if (!decoder.decode(result.masksToBounds))
                return false;
        }
        if (result.changes & WCLayerChange::SolidColor) {
            if (!decoder.decode(result.solidColor))
                return false;
        }
        if (result.changes & WCLayerChange::DebugVisuals) {
            if (!decoder.decode(result.showDebugBorder))
                return false;
            if (!decoder.decode(result.debugBorderColor))
                return false;
            if (!decoder.decode(result.debugBorderWidth))
                return false;
        }
        if (result.changes & WCLayerChange::RepaintCount) {
            if (!decoder.decode(result.showRepaintCounter))
                return false;
            if (!decoder.decode(result.repaintCount))
                return false;
        }
        if (result.changes & WCLayerChange::ContentsRect) {
            if (!decoder.decode(result.contentsRect))
                return false;
        }
        if (result.changes & WCLayerChange::ContentsClippingRect) {
            if (!decoder.decode(result.contentsClippingRect))
                return false;
            if (!decoder.decode(result.contentsRectClipsDescendants))
                return false;
        }
        if (result.changes & WCLayerChange::Opacity) {
            if (!decoder.decode(result.opacity))
                return false;
        }
        if (result.changes & WCLayerChange::Background) {
            if (!decoder.decode(result.backgroundColor))
                return false;
            if (!decoder.decode(result.hasBackingStore))
                return false;
            if (!decoder.decode(result.tileUpdate))
                return false;
        }
        if (result.changes & WCLayerChange::Transform) {
            if (!decoder.decode(result.transform))
                return false;
        }
        if (result.changes & WCLayerChange::ChildrenTransform) {
            if (!decoder.decode(result.childrenTransform))
                return false;
        }
        if (result.changes & WCLayerChange::Filters) {
            if (!decoder.decode(result.filters))
                return false;
        }
        if (result.changes & WCLayerChange::BackdropFilters) {
            if (!decoder.decode(result.backdropFilters))
                return false;
            if (!decoder.decode(result.backdropFiltersRect))
                return false;
        }
        if (result.changes & WCLayerChange::PlatformLayer) {
            if (!decoder.decode(result.hasPlatformLayer))
                return false;
            if (!decoder.decode(result.contentBufferIdentifiers))
                return false;
        }
        return true;
    }
};

struct WCUpateInfo {
    WebCore::GraphicsLayer::PlatformLayerID rootLayer;
    Vector<WebCore::GraphicsLayer::PlatformLayerID> addedLayers;
    Vector<WebCore::GraphicsLayer::PlatformLayerID> removedLayers;
    Vector<WCLayerUpateInfo> changedLayers;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << rootLayer;
        encoder << addedLayers;
        encoder << removedLayers;
        encoder << changedLayers;
    }

    template <class Decoder>
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, WCUpateInfo& result)
    {
        if (!decoder.decode(result.rootLayer))
            return false;
        if (!decoder.decode(result.addedLayers))
            return false;
        if (!decoder.decode(result.removedLayers))
            return false;
        if (!decoder.decode(result.changedLayers))
            return false;
        return true;
    }
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WCLayerChange> {
    using values = EnumValues<
        WebKit::WCLayerChange,
        WebKit::WCLayerChange::Children,
        WebKit::WCLayerChange::MaskLayer,
        WebKit::WCLayerChange::ReplicaLayer,
        WebKit::WCLayerChange::Geometry,
        WebKit::WCLayerChange::Preserves3D,
        WebKit::WCLayerChange::ContentsVisible,
        WebKit::WCLayerChange::BackfaceVisibility,
        WebKit::WCLayerChange::MasksToBounds,
        WebKit::WCLayerChange::SolidColor,
        WebKit::WCLayerChange::DebugVisuals,
        WebKit::WCLayerChange::RepaintCount,
        WebKit::WCLayerChange::ContentsRect,
        WebKit::WCLayerChange::ContentsClippingRect,
        WebKit::WCLayerChange::Opacity,
        WebKit::WCLayerChange::Background,
        WebKit::WCLayerChange::Transform,
        WebKit::WCLayerChange::ChildrenTransform,
        WebKit::WCLayerChange::Filters,
        WebKit::WCLayerChange::BackdropFilters,
        WebKit::WCLayerChange::PlatformLayer
    >;
};

} // namespace WebKit
