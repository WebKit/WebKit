/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "TransformationMatrix.h"
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>

namespace Nicosia {

class PlatformLayer : public ThreadSafeRefCounted<PlatformLayer> {
public:
    virtual ~PlatformLayer();

    virtual bool isCompositionLayer() const { return false; }

    uint64_t id() const { return m_id; }

protected:
    explicit PlatformLayer(uint64_t);

    uint64_t m_id;

    struct {
        Lock lock;
    } m_state;
};

class CompositionLayer : public PlatformLayer {
public:
    static Ref<CompositionLayer> create(uint64_t id)
    {
        return adoptRef(*new CompositionLayer(id));
    }
    virtual ~CompositionLayer();

    bool isCompositionLayer() const override { return true; }

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
                    bool childrenChanged : 1;
                    bool maskChanged : 1;
                    bool replicaChanged : 1;
                    bool flagsChanged : 1;
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

        Vector<RefPtr<CompositionLayer>> children;
        RefPtr<CompositionLayer> replica;
        RefPtr<CompositionLayer> mask;
    };

    template<typename T>
    void updateState(const T& functor)
    {
        LockHolder locker(PlatformLayer::m_state.lock);
        functor(m_state.pending);
    }

private:
    explicit CompositionLayer(uint64_t);

    struct {
        LayerState pending;
    } m_state;
};

} // namespace Nicosia

#define SPECIALIZE_TYPE_TRAITS_NICOSIA_PLATFORMLAYER(ToClassName, predicate) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(Nicosia::ToClassName) \
    static bool isType(const Nicosia::PlatformLayer& layer) { return layer.predicate; } \
    SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_NICOSIA_PLATFORMLAYER(CompositionLayer, isCompositionLayer());
