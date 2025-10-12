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

#pragma once

#if ENABLE(GPUP_MODEL)

#include <WebCore/Model.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerClient.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS WebUSDModelLoader;

namespace WebCore::DDModel {
class DDMesh;
}

namespace WebCore {

class ModelDisplayBufferDisplayDelegate;
class ModelPlayerClient;
class Page;

class WEBCORE_EXPORT DDModelPlayer final : public ModelPlayer, public CanMakeWeakPtr<DDModelPlayer> {
public:
    static Ref<DDModelPlayer> create(Page&, ModelPlayerClient&);
    virtual ~DDModelPlayer();

    WebCore::ModelPlayerIdentifier identifier() const final;
    void update();

private:
    DDModelPlayer(Page&, ModelPlayerClient&);

    void updateScene();

    // ModelPlayer overrides.
    void load(Model&, LayoutSize) override;
    void sizeDidChange(LayoutSize) override;
    CALayer *layer() override;
    std::optional<LayerHostingContextIdentifier> layerHostingContextIdentifier() override;
    void enterFullscreen() override;
    void handleMouseDown(const LayoutPoint&, MonotonicTime) override;
    void handleMouseMove(const LayoutPoint&, MonotonicTime) override;
    void handleMouseUp(const LayoutPoint&, MonotonicTime) override;
    void getCamera(CompletionHandler<void(std::optional<HTMLModelElementCamera>&&)>&&) override;
    void setCamera(HTMLModelElementCamera, CompletionHandler<void(bool success)>&&) override;
    void isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&) override;
    void isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&) override;
    void animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&) override;
    void animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&) override;
    void setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&) override;
    void hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void isMuted(CompletionHandler<void(std::optional<bool>&&)>&&) override;
    void setIsMuted(bool, CompletionHandler<void(bool success)>&&) override;
    Vector<RetainPtr<id>> accessibilityChildren() override;

    const MachSendRight* displayBuffer() const override;
    GraphicsLayerContentsDisplayDelegate* contentsDisplayDelegate() override;

    WeakPtr<ModelPlayerClient> m_client;

    WebCore::ModelPlayerIdentifier m_id;
    RetainPtr<WebUSDModelLoader> m_modelLoader;
    Vector<MachSendRight> m_displayBuffers;
    RefPtr<WebCore::DDModel::DDMesh> m_currentModel;
    WeakRef<Page> m_page;
    mutable RefPtr<ModelDisplayBufferDisplayDelegate> m_contentsDisplayDelegate;
    uint32_t m_currentTexture { 0 };
};

}

#endif
