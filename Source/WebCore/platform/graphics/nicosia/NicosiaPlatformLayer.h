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

#include "NicosiaSceneIntegration.h"
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>

namespace Nicosia {

class PlatformLayer : public ThreadSafeRefCounted<PlatformLayer> {
public:
    virtual ~PlatformLayer() = default;

    virtual bool isCompositionLayer() const { return false; }
    virtual bool isContentLayer() const { return false; }

    using LayerID = uint64_t;
    LayerID id() const { return m_id; }

    void setSceneIntegration(RefPtr<SceneIntegration>&& sceneIntegration)
    {
        Locker locker { m_state.lock };
        m_state.sceneIntegration = WTFMove(sceneIntegration);
    }

    std::unique_ptr<SceneIntegration::UpdateScope> createUpdateScope()
    {
        Locker locker { m_state.lock };
        if (m_state.sceneIntegration)
            return m_state.sceneIntegration->createUpdateScope();
        return nullptr;
    }

protected:
    explicit PlatformLayer(uint64_t id)
        : m_id(id)
    {
    }

    uint64_t m_id { 0 };

    struct {
        Lock lock;
        RefPtr<SceneIntegration> sceneIntegration WTF_GUARDED_BY_LOCK(lock);
    } m_state;
};

} // namespace Nicosia

#define SPECIALIZE_TYPE_TRAITS_NICOSIA_PLATFORMLAYER(ToClassName, predicate) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(Nicosia::ToClassName) \
    static bool isType(const Nicosia::PlatformLayer& layer) { return layer.predicate; } \
    SPECIALIZE_TYPE_TRAITS_END()
