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

#include <cstdint>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace Nicosia {

class CompositionLayer;

class Scene : public ThreadSafeRefCounted<Scene> {
public:
    static Ref<Scene> create()
    {
        return adoptRef(*new Scene);
    }
    ~Scene();

    struct State {
        State();
        ~State();

        uint32_t id { 0 };
        // FIXME: This is needed for a ThreadedCompositor oddity that might not even be
        // necessary anymore. It that has to be checked and ideally removed.
        // https://bugs.webkit.org/show_bug.cgi?id=188839
        bool platformLayerUpdated { false };
        HashSet<RefPtr<Nicosia::CompositionLayer>> layers;
        RefPtr<Nicosia::CompositionLayer> rootLayer;
    };

    template<typename F>
    void accessState(const F& functor)
    {
        LockHolder locker(m_scene.lock);
        functor(m_scene.state);
    }

private:
    Scene();

    struct {
        Lock lock;
        State state;
    } m_scene;
};

} // namespace Nicosia
