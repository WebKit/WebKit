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
#include "NicosiaSceneIntegration.h"
#include "TransformationMatrix.h"
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>

namespace Nicosia {

class PlatformLayer : public ThreadSafeRefCounted<PlatformLayer> {
public:
    virtual ~PlatformLayer();

    virtual bool isCompositionLayer() const { return false; }
    virtual bool isContentLayer() const { return false; }

    using LayerID = uint64_t;
    LayerID id() const { return m_id; }

    void setSceneIntegration(RefPtr<SceneIntegration>&& sceneIntegration)
    {
        LockHolder locker(m_state.lock);
        m_state.sceneIntegration = WTFMove(sceneIntegration);
    }

    std::unique_ptr<SceneIntegration::UpdateScope> createUpdateScope()
    {
        LockHolder locker(m_state.lock);
        if (m_state.sceneIntegration)
            return m_state.sceneIntegration->createUpdateScope();
        return nullptr;
    }

protected:
    explicit PlatformLayer(uint64_t);

    uint64_t m_id;

    struct {
        Lock lock;
        RefPtr<SceneIntegration> sceneIntegration;
    } m_state;
};

class ContentLayer;
class BackingStore;
class ImageBacking;

class CompositionLayer : public PlatformLayer {
public:
    class Impl {
    public:
        using Factory = WTF::Function<std::unique_ptr<Impl>(uint64_t, CompositionLayer&)>;

        virtual ~Impl();
        virtual bool isTextureMapperImpl() const { return false; }
    };

    static Ref<CompositionLayer> create(uint64_t id, const Impl::Factory& factory)
    {
        return adoptRef(*new CompositionLayer(id, factory));
    }
    virtual ~CompositionLayer();
    bool isCompositionLayer() const override { return true; }

    Impl& impl() const { return *m_impl; }

    struct LayerState {
        struct Delta {
            Delta() = default;

            union {
                struct {
                    bool positionChanged : 1;
                    bool anchorPointChanged : 1;
                    bool sizeChanged : 1;
                    bool transformChanged : 1;
                    bool childrenTransformChanged : 1;
                    bool contentsRectChanged : 1;
                    bool contentsTilingChanged : 1;
                    bool opacityChanged : 1;
                    bool solidColorChanged : 1;
                    bool filtersChanged : 1;
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
                    bool preserves3D : 1;
                };
                uint32_t value { 0 };
            };
        } flags;

        WebCore::FloatPoint position;
        WebCore::FloatPoint3D anchorPoint;
        WebCore::FloatSize size;

        WebCore::TransformationMatrix transform;
        WebCore::TransformationMatrix childrenTransform;

        WebCore::FloatRect contentsRect;
        WebCore::FloatSize contentsTilePhase;
        WebCore::FloatSize contentsTileSize;

        float opacity { 0 };
        WebCore::Color solidColor;

        WebCore::FilterOperations filters;
        // FIXME: Despite the name, this implementation is not
        // TextureMapper-specific. Should be renamed when necessary.
        Animations animations;

        Vector<RefPtr<CompositionLayer>> children;
        RefPtr<CompositionLayer> replica;
        RefPtr<CompositionLayer> mask;

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
    };

    template<typename T>
    void updateState(const T& functor)
    {
        LockHolder locker(PlatformLayer::m_state.lock);
        functor(m_state.pending);
    }

    template<typename T>
    void flushState(const T& functor)
    {
        LockHolder locker(PlatformLayer::m_state.lock);
        auto& pending = m_state.pending;
        auto& staging = m_state.staging;

        staging.delta.value |= pending.delta.value;

        if (pending.delta.positionChanged)
            staging.position = pending.position;
        if (pending.delta.anchorPointChanged)
            staging.anchorPoint = pending.anchorPoint;
        if (pending.delta.sizeChanged)
            staging.size = pending.size;

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

        if (pending.delta.opacityChanged)
            staging.opacity = pending.opacity;
        if (pending.delta.solidColorChanged)
            staging.solidColor = pending.solidColor;

        if (pending.delta.filtersChanged)
            staging.filters = pending.filters;
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
        LockHolder locker(PlatformLayer::m_state.lock);
        m_state.committed = m_state.staging;
        m_state.staging.delta = { };

        functor(m_state.committed);
    }

    template<typename T>
    void accessCommitted(const T& functor)
    {
        LockHolder locker(PlatformLayer::m_state.lock);
        functor(m_state.committed);
    }

private:
    CompositionLayer(uint64_t, const Impl::Factory&);

    std::unique_ptr<Impl> m_impl;

    struct {
        LayerState pending;
        LayerState staging;
        LayerState committed;
    } m_state;
};

class ContentLayer : public PlatformLayer {
public:
    class Impl {
    public:
        using Factory = WTF::Function<std::unique_ptr<Impl>(ContentLayer&)>;

        virtual ~Impl();
        virtual bool isTextureMapperImpl() const { return false; }
    };

    static Ref<ContentLayer> create(const Impl::Factory& factory)
    {
        return adoptRef(*new ContentLayer(factory));
    }
    virtual ~ContentLayer();
    bool isContentLayer() const override { return true; }

    Impl& impl() const { return *m_impl; }

private:
    ContentLayer(const Impl::Factory&);

    std::unique_ptr<Impl> m_impl;
};

class BackingStore : public ThreadSafeRefCounted<BackingStore> {
public:
    class Impl {
    public:
        using Factory = WTF::Function<std::unique_ptr<Impl>(BackingStore&)>;

        virtual ~Impl();
        virtual bool isTextureMapperImpl() const { return false; }
    };

    static Ref<BackingStore> create(const Impl::Factory& factory)
    {
        return adoptRef(*new BackingStore(factory));
    }
    virtual ~BackingStore();

    Impl& impl() const { return *m_impl; }

private:
    BackingStore(const Impl::Factory&);

    std::unique_ptr<Impl> m_impl;
};

class ImageBacking : public ThreadSafeRefCounted<ImageBacking> {
public:
    class Impl {
    public:
        using Factory = WTF::Function<std::unique_ptr<Impl>(ImageBacking&)>;

        virtual ~Impl();
        virtual bool isTextureMapperImpl() const { return false; }
    };

    static Ref<ImageBacking> create(const Impl::Factory& factory)
    {
        return adoptRef(*new ImageBacking(factory));
    }
    virtual ~ImageBacking();

    Impl& impl() const { return *m_impl; }

private:
    ImageBacking(const Impl::Factory&);

    std::unique_ptr<Impl> m_impl;
};

} // namespace Nicosia

#define SPECIALIZE_TYPE_TRAITS_NICOSIA_PLATFORMLAYER(ToClassName, predicate) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(Nicosia::ToClassName) \
    static bool isType(const Nicosia::PlatformLayer& layer) { return layer.predicate; } \
    SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_NICOSIA_PLATFORMLAYER(CompositionLayer, isCompositionLayer());
SPECIALIZE_TYPE_TRAITS_NICOSIA_PLATFORMLAYER(ContentLayer, isContentLayer());

#define SPECIALIZE_TYPE_TRAITS_NICOSIA_COMPOSITIONLAYER_IMPL(ToClassName, predicate) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(Nicosia::ToClassName) \
    static bool isType(const Nicosia::CompositionLayer::Impl& impl) { return impl.predicate; } \
    SPECIALIZE_TYPE_TRAITS_END()

#define SPECIALIZE_TYPE_TRAITS_NICOSIA_CONTENTLAYER_IMPL(ToClassName, predicate) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(Nicosia::ToClassName) \
    static bool isType(const Nicosia::ContentLayer::Impl& impl) { return impl.predicate; } \
    SPECIALIZE_TYPE_TRAITS_END()

#define SPECIALIZE_TYPE_TRAITS_NICOSIA_BACKINGSTORE_IMPL(ToClassName, predicate) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(Nicosia::ToClassName) \
    static bool isType(const Nicosia::BackingStore::Impl& impl) { return impl.predicate; } \
    SPECIALIZE_TYPE_TRAITS_END()

#define SPECIALIZE_TYPE_TRAITS_NICOSIA_IMAGEBACKING_IMPL(ToClassName, predicate) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(Nicosia::ToClassName) \
    static bool isType(const Nicosia::ImageBacking::Impl& impl) { return impl.predicate; } \
    SPECIALIZE_TYPE_TRAITS_END()
