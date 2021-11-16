/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if HAVE(SCENEKIT)

#import "SceneKitModelPlayer.h"

#import "SceneKitModel.h"
#import "SceneKitModelLoader.h"
#import <pal/spi/cocoa/SceneKitSPI.h>

namespace WebCore {

Ref<SceneKitModelPlayer> SceneKitModelPlayer::create(ModelPlayerClient& client)
{
    return adoptRef(*new SceneKitModelPlayer(client));
}

SceneKitModelPlayer::SceneKitModelPlayer(ModelPlayerClient& client)
    : m_client { client }
    , m_layer { adoptNS([[SCNMetalLayer alloc] init]) }
{
    m_layer.get().autoenablesDefaultLighting = YES;

    // FIXME: This should be done by the caller.
    m_layer.get().contentsScale = 2.0;
}

SceneKitModelPlayer::~SceneKitModelPlayer()
{
    // If there is an outstanding load, as indicated by a non-null m_loader, cancel it.
    if (m_loader)
        m_loader->cancel();
}

// MARK: - ModelPlayer overrides.

void SceneKitModelPlayer::load(Model& modelSource, LayoutSize)
{
    if (m_loader)
        m_loader->cancel();

    m_loader = loadSceneKitModel(modelSource, *this);
}

PlatformLayer* SceneKitModelPlayer::layer()
{
    return m_layer.get();
}

// MARK: - SceneKitModelLoaderClient overrides.

void SceneKitModelPlayer::didFinishLoading(SceneKitModelLoader& loader, Ref<SceneKitModel> model)
{
    dispatch_assert_queue(dispatch_get_main_queue());
    ASSERT_UNUSED(loader, &loader == m_loader.get());

    m_loader = nullptr;
    m_model = WTFMove(model);

    updateScene();

    if (m_client)
        m_client->didFinishLoading(*this);
}

void SceneKitModelPlayer::didFailLoading(SceneKitModelLoader& loader, const ResourceError& error)
{
    dispatch_assert_queue(dispatch_get_main_queue());
    ASSERT_UNUSED(loader, &loader == m_loader.get());

    m_loader = nullptr;

    if (!m_client)
        m_client->didFailLoading(*this, error);
}

void SceneKitModelPlayer::updateScene()
{
    if (m_layer.get().scene == m_model->defaultScene())
        return;
    m_layer.get().scene = m_model->defaultScene();
}

}

#endif
