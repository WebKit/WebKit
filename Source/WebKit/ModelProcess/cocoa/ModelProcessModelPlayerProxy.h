/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS)

#include "Connection.h"
#include "LayerHostingContext.h"
#include "MessageReceiver.h"
#include "SharedPreferencesForWebProcess.h"
#include <CoreRE/CoreRE.h>
#include <WebCore/Color.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerIdentifier.h>
#include <WebKitAdditions/REPtr.h>
#include <WebKitAdditions/REModelLoaderClient.h>
#include <simd/simd.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS WKModelProcessModelLayer;
OBJC_CLASS WKModelProcessModelPlayerProxyObjCAdapter;
OBJC_CLASS WKSRKEntity;

namespace WebCore {
class Model;
class REModel;
class REModelLoader;
}

namespace WebKit {

class ModelProcessModelPlayerManagerProxy;

class ModelProcessModelPlayerProxy final
    : public WebCore::ModelPlayer
    , public WebCore::REModelLoaderClient
    , private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(ModelProcessModelPlayerProxy);
public:
    static Ref<ModelProcessModelPlayerProxy> create(ModelProcessModelPlayerManagerProxy&, WebCore::ModelPlayerIdentifier, Ref<IPC::Connection>&&);
    ~ModelProcessModelPlayerProxy();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    static bool transformSupported(const simd_float4x4& transform);

    void invalidate();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    template<typename T> void send(T&& message);

    void updateBackgroundColor();
    void updateTransform();
    void updateOpacity();
    void startAnimating();
    void animationPlaybackStateDidUpdate();

    // Messages
    void createLayer();
    void loadModel(Ref<WebCore::Model>&&, WebCore::LayoutSize);

    // WebCore::REModelLoaderClient overrides.
    void didFinishLoading(WebCore::REModelLoader&, Ref<WebCore::REModel>) final;
    void didFailLoading(WebCore::REModelLoader&, const WebCore::ResourceError&) final;

    // WebCore::ModelPlayer overrides.
    WebCore::ModelPlayerIdentifier identifier() const final { return m_id; }
    void load(WebCore::Model&, WebCore::LayoutSize) final;
    void sizeDidChange(WebCore::LayoutSize) final;
    PlatformLayer* layer() final;
    std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier() final;
    void setBackgroundColor(WebCore::Color) final;
    void setEntityTransform(WebCore::TransformationMatrix) final;
    void enterFullscreen() final;
    bool supportsMouseInteraction() final;
    bool supportsDragging() final;
    void setInteractionEnabled(bool) final;
    void handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime) final;
    void getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&&) final;
    void setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&&) final;
    void isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&) final;
    void isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&) final;
    void animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&) final;
    void hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void isMuted(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setIsMuted(bool, CompletionHandler<void(bool success)>&&) final;
    Vector<RetainPtr<id>> accessibilityChildren() final;
    void setAutoplay(bool) final;
    void setLoop(bool) final;
    void setPlaybackRate(double, CompletionHandler<void(double effectivePlaybackRate)>&&) final;
    double duration() const final;
    bool paused() const final;
    void setPaused(bool, CompletionHandler<void(bool succeeded)>&&) final;
    Seconds currentTime() const final;
    void setCurrentTime(Seconds, CompletionHandler<void()>&&) final;
    void setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data) final;

private:
    ModelProcessModelPlayerProxy(ModelProcessModelPlayerManagerProxy&, WebCore::ModelPlayerIdentifier, Ref<IPC::Connection>&&);

    void computeTransform();
    void applyEnvironmentMapDataAndRelease();

    WebCore::ModelPlayerIdentifier m_id;
    Ref<IPC::Connection> m_webProcessConnection;
    WeakPtr<ModelProcessModelPlayerManagerProxy> m_manager;

    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    RetainPtr<WKModelProcessModelLayer> m_layer;
    RefPtr<WebCore::REModelLoader> m_loader;
    RefPtr<WebCore::REModel> m_model;
    RetainPtr<WKSRKEntity> m_modelRKEntity;
    REPtr<RESceneRef> m_scene;
    RetainPtr<WKModelProcessModelPlayerProxyObjCAdapter> m_objCAdapter;

    WebCore::Color m_backgroundColor;
    simd_float3 m_originalBoundingBoxCenter { simd_make_float3(0, 0, 0) };
    simd_float3 m_originalBoundingBoxExtents { simd_make_float3(0, 0, 0) };
    float m_pitch { 0 };
    float m_yaw { 0 };

    RESRT m_transformSRT; // SRT=Scaling/Rotation/Translation. This is stricter than a WebCore::TransformationMatrix.

    bool m_autoplay { false };
    bool m_loop { false };
    double m_playbackRate { 1.0 };

    RefPtr<WebCore::SharedBuffer> m_environmentMapData;
};

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
