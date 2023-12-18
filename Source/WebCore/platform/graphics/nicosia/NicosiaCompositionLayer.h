/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Color.h"
#include "FilterOperations.h"
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "NicosiaAnimatedBackingStoreClient.h"
#include "NicosiaAnimation.h"
#include "NicosiaBackingStore.h"
#include "NicosiaContentLayer.h"
#include "NicosiaImageBacking.h"
#include "NicosiaPlatformLayer.h"
#include "ScrollTypes.h"
#include "TextureMapperLayer.h"
#include "TransformationMatrix.h"
#include <wtf/Lock.h>

namespace Nicosia {

class CompositionLayer final : public PlatformLayer {
public:
    static Ref<CompositionLayer> create(uint64_t id)
    {
        return adoptRef(*new CompositionLayer(id));
    }

    struct LayerState {
        struct Delta {
            Delta() = default;

            union {
                struct {
                    bool positionChanged : 1;
                    bool anchorPointChanged : 1;
                    bool sizeChanged : 1;
                    bool boundsOriginChanged : 1;
                    bool transformChanged : 1;
                    bool childrenTransformChanged : 1;
                    bool contentsRectChanged : 1;
                    bool contentsTilingChanged : 1;
                    bool contentsClippingRectChanged : 1;
                    bool opacityChanged : 1;
                    bool solidColorChanged : 1;
                    bool filtersChanged : 1;
                    bool backdropFiltersChanged : 1;
                    bool backdropFiltersRectChanged : 1;
                    bool animationsChanged : 1;
                    bool childrenChanged : 1;
                    bool maskChanged : 1;
                    bool replicaChanged : 1;
                    bool flagsChanged : 1;
                    bool contentLayerChanged : 1;
                    bool backingStoreChanged : 1;
                    bool imageBackingChanged : 1;
                    bool animatedBackingStoreClientChanged : 1;
                    bool repaintCounterChanged : 1;
                    bool debugBorderChanged : 1;
                    bool scrollingNodeChanged : 1;
                    bool eventRegionChanged : 1;
                };
                uint32_t value { 0 };
            };
        } delta;

        struct Flags {
            Flags()
                : contentsVisible(true)
                , backfaceVisible(true)
            { }

            union {
                struct {
                    bool contentsOpaque : 1;
                    bool drawsContent : 1;
                    bool contentsVisible : 1;
                    bool backfaceVisible : 1;
                    bool masksToBounds : 1;
                    bool contentsRectClipsDescendants : 1;
                    bool preserves3D : 1;
                };
                uint32_t value { 0 };
            };
        } flags;

        WebCore::FloatPoint position;
        WebCore::FloatPoint3D anchorPoint;
        WebCore::FloatSize size;
        WebCore::FloatPoint boundsOrigin;

        WebCore::TransformationMatrix transform;
        WebCore::TransformationMatrix childrenTransform;

        WebCore::FloatRect contentsRect;
        WebCore::FloatSize contentsTilePhase;
        WebCore::FloatSize contentsTileSize;
        WebCore::FloatRoundedRect contentsClippingRect;

        float opacity { 0 };
        WebCore::Color solidColor;

        WebCore::FilterOperations filters;
        // FIXME: Despite the name, this implementation is not
        // TextureMapper-specific. Should be renamed when necessary.
        Animations animations;

        Vector<RefPtr<CompositionLayer>> children;
        RefPtr<CompositionLayer> replica;
        RefPtr<CompositionLayer> mask;
        RefPtr<CompositionLayer> backdropLayer;
        WebCore::FloatRoundedRect backdropFiltersRect;

        RefPtr<ContentLayer> contentLayer;
        RefPtr<BackingStore> backingStore;
        RefPtr<ImageBacking> imageBacking;
        RefPtr<AnimatedBackingStoreClient> animatedBackingStoreClient;

        struct RepaintCounter {
            unsigned count { 0 };
            bool visible { false };
        } repaintCounter;
        struct DebugBorder {
            WebCore::Color color;
            float width { 0 };
            bool visible { false };
        } debugBorder;

        WebCore::ScrollingNodeID scrollingNodeID { 0 };
        WebCore::EventRegion eventRegion;
    };

    template<typename T>
    void flushState(const T& functor)
    {
        Locker locker { PlatformLayer::m_state.lock };
        auto& pending = m_state.pending;
        auto& staging = m_state.staging;

        staging.delta.value |= pending.delta.value;

        if (pending.delta.positionChanged)
            staging.position = pending.position;
        if (pending.delta.anchorPointChanged)
            staging.anchorPoint = pending.anchorPoint;
        if (pending.delta.sizeChanged)
            staging.size = pending.size;
        if (pending.delta.boundsOriginChanged)
            staging.boundsOrigin = pending.boundsOrigin;

        if (pending.delta.transformChanged)
            staging.transform = pending.transform;
        if (pending.delta.childrenTransformChanged)
            staging.childrenTransform = pending.childrenTransform;

        if (pending.delta.contentsRectChanged)
            staging.contentsRect = pending.contentsRect;
        if (pending.delta.contentsTilingChanged) {
            staging.contentsTilePhase = pending.contentsTilePhase;
            staging.contentsTileSize = pending.contentsTileSize;
        }
        if (pending.delta.contentsClippingRectChanged)
            staging.contentsClippingRect = pending.contentsClippingRect;

        if (pending.delta.opacityChanged)
            staging.opacity = pending.opacity;
        if (pending.delta.solidColorChanged)
            staging.solidColor = pending.solidColor;

        if (pending.delta.filtersChanged)
            staging.filters = pending.filters;
        if (pending.delta.backdropFiltersChanged)
            staging.backdropLayer = pending.backdropLayer;
        if (pending.delta.backdropFiltersRectChanged)
            staging.backdropFiltersRect = pending.backdropFiltersRect;
        if (pending.delta.animationsChanged)
            staging.animations = pending.animations;

        if (pending.delta.childrenChanged)
            staging.children = pending.children;
        if (pending.delta.maskChanged)
            staging.mask = pending.mask;
        if (pending.delta.replicaChanged)
            staging.replica = pending.replica;

        if (pending.delta.flagsChanged)
            staging.flags.value = pending.flags.value;

        if (pending.delta.repaintCounterChanged)
            staging.repaintCounter = pending.repaintCounter;
        if (pending.delta.debugBorderChanged)
            staging.debugBorder = pending.debugBorder;

        if (pending.delta.scrollingNodeChanged)
            staging.scrollingNodeID = pending.scrollingNodeID;

        if (pending.delta.eventRegionChanged)
            staging.eventRegion = pending.eventRegion;

        if (pending.delta.backingStoreChanged)
            staging.backingStore = pending.backingStore;
        if (pending.delta.contentLayerChanged)
            staging.contentLayer = pending.contentLayer;
        if (pending.delta.imageBackingChanged)
            staging.imageBacking = pending.imageBacking;
        if (pending.delta.animatedBackingStoreClientChanged)
            staging.animatedBackingStoreClient = pending.animatedBackingStoreClient;

        pending.delta = { };

        functor(staging);
    }

    template<typename T>
    void commitState(const T& functor)
    {
        Locker locker { PlatformLayer::m_state.lock };
        m_state.committed = m_state.staging;
        m_state.staging.delta = { };

        functor(m_state.committed);
    }

    template<typename T>
    void accessPending(const T& functor)
    {
        Locker locker { PlatformLayer::m_state.lock };
        functor(m_state.pending);
    }

    template<typename T>
    void accessCommitted(const T& functor)
    {
        Locker locker { PlatformLayer::m_state.lock };
        functor(m_state.committed);
    }

    struct CompositionState {
        std::unique_ptr<WebCore::TextureMapperLayer> layer;
    };
    CompositionState& compositionState() { return m_compositionState; }

private:
    explicit CompositionLayer(uint64_t id)
        : PlatformLayer(id)
    {
    }

    bool isCompositionLayer() const override { return true; }

    struct {
        LayerState pending;
        LayerState staging;
        LayerState committed;
    } m_state;
    CompositionState m_compositionState;
};

} // namespace Nicosia

SPECIALIZE_TYPE_TRAITS_NICOSIA_PLATFORMLAYER(CompositionLayer, isCompositionLayer());
