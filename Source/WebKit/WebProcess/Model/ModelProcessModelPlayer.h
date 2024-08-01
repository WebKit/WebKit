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

#import "ModelIdentifier.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/ModelPlayer.h>
#import <WebCore/ModelPlayerClient.h>
#import <WebCore/ModelPlayerIdentifier.h>
#import <wtf/Compiler.h>

namespace WebKit {

class ModelProcessModelPlayer
    : public WebCore::ModelPlayer
    , public IPC::MessageReceiver {
public:
    static Ref<ModelProcessModelPlayer> create(WebCore::ModelPlayerIdentifier, WebPage&, WebCore::ModelPlayerClient&);
    virtual ~ModelProcessModelPlayer();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier() { return m_layerHostingContextIdentifier; };

private:
    explicit ModelProcessModelPlayer(WebCore::ModelPlayerIdentifier, WebPage&, WebCore::ModelPlayerClient&);

    WebCore::ModelPlayerIdentifier identifier() { return m_id; }
    WebPage* page() { return m_page.get(); }
    WebCore::ModelPlayerClient* client() { return m_client.get(); }

    template<typename T> void send(T&& message);

    // Messages
    void didCreateLayer(WebCore::LayerHostingContextIdentifier);
    void didFinishLoading(const WebCore::FloatPoint3D&, const WebCore::FloatPoint3D&);
    void didUpdateEntityTransform(const WebCore::TransformationMatrix&);

    // WebCore::ModelPlayer overrides.
    void load(WebCore::Model&, WebCore::LayoutSize) final;
    void sizeDidChange(WebCore::LayoutSize) final;
    PlatformLayer* layer() final;
    void handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime) final;
    void setBackgroundColor(WebCore::Color) final;
    void setEntityTransform(WebCore::TransformationMatrix) final;
    bool supportsTransform(WebCore::TransformationMatrix) final;
    void enterFullscreen() final;
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

    WebCore::ModelPlayerIdentifier m_id;
    WeakPtr<WebPage> m_page;
    WeakPtr<WebCore::ModelPlayerClient> m_client;

    std::optional<WebCore::LayerHostingContextIdentifier> m_layerHostingContextIdentifier;
};

}

#endif // ENABLE(MODEL_PROCESS)
