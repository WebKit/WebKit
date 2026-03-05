/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#if HAVE(SCENEKIT)

#include <WebCore/Model.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerClient.h>
#include <WebCore/SceneKitModelLoaderClient.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>
#include <wtf/Forward.h>

OBJC_CLASS SCNMetalLayer;

namespace WebCore {

class SceneKitModel;
class SceneKitModelLoader;

class WEBCORE_EXPORT SceneKitModelPlayer final : public ModelPlayer, public SceneKitModelLoaderClient {
public:
    static Ref<SceneKitModelPlayer> create(ModelPlayerClient&);
    virtual ~SceneKitModelPlayer();

private:
    SceneKitModelPlayer(ModelPlayerClient&);

    void updateScene();

    // ModelPlayer overrides.
    ModelPlayerIdentifier NODELETE identifier() const override;
    void load(Model&, LayoutSize) override;
    void configureGraphicsLayer(GraphicsLayer&, ModelPlayerGraphicsLayerConfiguration&&) override;
    void NODELETE sizeDidChange(LayoutSize) override;
    void NODELETE enterFullscreen() override;
    void NODELETE handleMouseDown(const LayoutPoint&, MonotonicTime) override;
    void NODELETE handleMouseMove(const LayoutPoint&, MonotonicTime) override;
    void NODELETE handleMouseUp(const LayoutPoint&, MonotonicTime) override;
    void NODELETE getCamera(CompletionHandler<void(std::optional<HTMLModelElementCamera>&&)>&&) override;
    void NODELETE setCamera(HTMLModelElementCamera, CompletionHandler<void(bool success)>&&) override;
    void NODELETE isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void NODELETE setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&) override;
    void NODELETE isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void NODELETE setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&) override;
    void NODELETE animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&) override;
    void NODELETE animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&) override;
    void NODELETE setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&) override;
    void NODELETE hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void NODELETE isMuted(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void NODELETE setIsMuted(bool, CompletionHandler<void(bool success)>&&) override;
    ModelPlayerAccessibilityChildren accessibilityChildren() override;

    // SceneKitModelLoaderClient overrides.
    virtual void didFinishLoading(SceneKitModelLoader&, Ref<SceneKitModel>) override;
    virtual void didFailLoading(SceneKitModelLoader&, const ResourceError&) override;

    WeakPtr<ModelPlayerClient> m_client;

    RefPtr<SceneKitModelLoader> m_loader;
    RefPtr<SceneKitModel> m_model;

    RetainPtr<SCNMetalLayer> m_layer;
    ModelPlayerIdentifier m_id;
};

}

#endif
