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

#include "config.h"
#include "NicosiaSceneIntegration.h"

#include "NicosiaCompositionLayer.h"
#include "NicosiaScene.h"

namespace Nicosia {

SceneIntegration::SceneIntegration(Scene& scene, Client& client)
{
    m_client.scene = &scene;
    m_client.object = &client;
}

SceneIntegration::~SceneIntegration()
{
    ASSERT(!m_client.object);
}

void SceneIntegration::setClient(Client& client)
{
    Locker locker { m_client.lock };
    m_client.object = &client;
}

void SceneIntegration::invalidate()
{
    Locker locker { m_client.lock };
    m_client.scene = nullptr;
    m_client.object = nullptr;
}

void SceneIntegration::requestUpdate()
{
    Locker locker { m_client.lock };
    if (m_client.object)
        m_client.object->requestUpdate();
}

std::unique_ptr<SceneIntegration::UpdateScope> SceneIntegration::createUpdateScope()
{
    return makeUnique<UpdateScope>(Ref { *this });
}

SceneIntegration::Client::~Client() = default;

SceneIntegration::UpdateScope::UpdateScope(Ref<SceneIntegration>&& sceneIntegration)
    : m_sceneIntegration(WTFMove(sceneIntegration))
    , m_locker(m_sceneIntegration->m_client.lock)
{
}

SceneIntegration::UpdateScope::~UpdateScope()
{
    if (!m_sceneIntegration->m_client.scene)
        return;

    m_sceneIntegration->m_client.scene->accessState(
        [](Nicosia::Scene::State& state)
        {
            for (auto& compositionLayer : state.layers)
                compositionLayer->flushState([](auto&) { });
        });

    auto& sceneIntegrationObj = m_sceneIntegration.get();
    if (sceneIntegrationObj.m_client.object)
        sceneIntegrationObj.m_client.object->requestUpdate();
}

} // namespace Nicosia
