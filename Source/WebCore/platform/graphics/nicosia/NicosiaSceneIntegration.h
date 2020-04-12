/*
 * Copyright (C) 2019 Metrological Group B.V.
 * Copyright (C) 2019 Igalia S.L.
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

#include <memory>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace Nicosia {

class Scene;

class SceneIntegration : public ThreadSafeRefCounted<SceneIntegration> {
public:
    class Client {
    public:
        virtual ~Client();

        virtual void requestUpdate() = 0;
    };

    static Ref<SceneIntegration> create(Scene& scene, Client& client)
    {
        return adoptRef(*new SceneIntegration(scene, client));
    }
    ~SceneIntegration();

    void setClient(Client&);
    void invalidate();

    void requestUpdate();

    class UpdateScope {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit UpdateScope(Ref<SceneIntegration>&&);
        ~UpdateScope();

    private:
        Ref<SceneIntegration> m_sceneIntegration;
        LockHolder m_locker;
    };

    std::unique_ptr<UpdateScope> createUpdateScope();

private:
    explicit SceneIntegration(Scene&, Client&);

    struct {
        Lock lock;
        RefPtr<Scene> scene;
        Client* object { nullptr };
    } m_client;
};

} // namespace Nicosia
