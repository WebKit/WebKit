/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ModelProcessModelPlayer.h"

#if ENABLE(MODEL_PROCESS)

#include "ModelProcessModelPlayerManager.h"
#include "ModelProcessModelPlayerProxy.h"
#include "ModelProcessModelPlayerProxyMessages.h"
#include "ModelProcessModelPlayerTransformState.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/FloatPoint3D.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/Model.h>
#include <WebCore/ModelContext.h>
#include <WebCore/ModelPlayerAnimationState.h>
#include <WebCore/ModelPlayerGraphicsLayerConfiguration.h>
#include <WebCore/Page.h>
#include <WebCore/ResourceError.h>
#include <WebCore/Settings.h>
#include <WebCore/TransformationMatrix.h>

namespace WebKit {

Ref<ModelProcessModelPlayer> ModelProcessModelPlayer::create(WebCore::ModelPlayerIdentifier identifier, WebPage& page, WebCore::ModelPlayerClient& client)
{
    return adoptRef(*new ModelProcessModelPlayer(identifier, page, client));
}

ModelProcessModelPlayer::ModelProcessModelPlayer(WebCore::ModelPlayerIdentifier identifier, WebPage& page, WebCore::ModelPlayerClient& client)
    : m_id { identifier }
    , m_page { page }
    , m_client { client }
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer spawned id=%" PRIu64, this, m_id.toUInt64());
    send(Messages::ModelProcessModelPlayerProxy::CreateLayer());
}

ModelProcessModelPlayer::~ModelProcessModelPlayer()
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer deallocating id=%" PRIu64, this, m_id.toUInt64());
}

template<typename T>
ALWAYS_INLINE void ModelProcessModelPlayer::send(T&& message)
{
    WebProcess::singleton().modelProcessModelPlayerManager().modelProcessConnection().connection().send(std::forward<T>(message), m_id);
}

template<typename T, typename C>
ALWAYS_INLINE void ModelProcessModelPlayer::sendWithAsyncReply(T&& message, C&& completionHandler)
{
    WebProcess::singleton().modelProcessModelPlayerManager().modelProcessConnection().connection().sendWithAsyncReply(std::forward<T>(message), std::forward<C>(completionHandler), m_id);
}

bool ModelProcessModelPlayer::modelProcessEnabled() const
{
    RefPtr strongPage = m_page.get();
    return strongPage && strongPage->corePage() && strongPage->corePage()->settings().modelElementEnabled() && strongPage->corePage()->settings().modelProcessEnabled();
}

// MARK: - Messages

void ModelProcessModelPlayer::didCreateLayer(WebCore::LayerHostingContextIdentifier identifier)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer obtained new layerHostingContextIdentifier id=%" PRIu64, this, m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    m_layerHostingContextIdentifier = identifier;
    protectedClient()->didUpdate(*this);
}

void ModelProcessModelPlayer::didFinishLoading(const WebCore::FloatPoint3D& boundingBoxCenter, const WebCore::FloatPoint3D& boundingBoxExtents)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer didFinishLoading id=%" PRIu64, this, m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    m_boundingBoxCenter = boundingBoxCenter;
    m_boundingBoxExtents = boundingBoxExtents;

    RefPtr client = m_client.get();
    client->didFinishLoading(*this);
    client->didUpdateBoundingBox(*this, boundingBoxCenter, boundingBoxExtents);
}

void ModelProcessModelPlayer::didFailLoading()
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer didFailLoading id=%" PRIu64, this, m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    protectedClient()->didFailLoading(*this, WebCore::ResourceError { WebCore::errorDomainWebKitInternal, 0, { }, "Failed to load model data"_s });
}

/// This comes from Model Process side, so that Web Process has the most up-to-date knowledge about the transform actually applied to the entity.
/// Not to be confused with setEntityTransform().
void ModelProcessModelPlayer::didUpdateEntityTransform(const WebCore::TransformationMatrix& transform)
{
    RELEASE_ASSERT(modelProcessEnabled());

    m_entityTransform = transform;
    protectedClient()->didUpdateEntityTransform(*this, transform);
}

void ModelProcessModelPlayer::didUpdateAnimationPlaybackState(bool isPaused, double playbackRate, Seconds duration, Seconds currentTime, MonotonicTime clockTimestamp)
{
    RELEASE_ASSERT(modelProcessEnabled());

    m_animationState.setPaused(isPaused);
    m_animationState.setDuration(duration);
    m_animationState.setPlaybackRate(playbackRate);
    m_animationState.setCurrentTime(currentTime, clockTimestamp);
}

void ModelProcessModelPlayer::didFinishEnvironmentMapLoading(bool succeeded)
{
    RELEASE_ASSERT(modelProcessEnabled());

    protectedClient()->didFinishEnvironmentMapLoading(*this, succeeded);
}

// MARK: - WebCore::ModelPlayer

std::optional<WebCore::ModelPlayerAnimationState> ModelProcessModelPlayer::currentAnimationState() const
{
    // Has no current state to return if the model load hasn't returned with its extents.
    if (!m_boundingBoxExtents)
        return std::nullopt;

    return m_animationState;
}

std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> ModelProcessModelPlayer::currentTransformState() const
{
    // Has no current state to return if the model load hasn't returned with its extents.
    if (!m_boundingBoxExtents)
        return std::nullopt;

    return ModelProcessModelPlayerTransformState::create(m_entityTransform, m_boundingBoxCenter, m_boundingBoxExtents, m_hasPortal, m_stageModeOperation);
}

void ModelProcessModelPlayer::load(WebCore::Model& model, WebCore::LayoutSize size)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer load model id=%" PRIu64, this, m_id.toUInt64());

    if (!WebCore::MIMETypeRegistry::isUSDMIMEType(model.mimeType())) {
        RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer::load: Found unexpected model mimetype: %s", this, model.mimeType().utf8().data());
        if (RefPtr client = m_client.get())
            client->logWarning(*this, makeString("Unexpected USDZ MIME type \""_s, model.mimeType(), "\" in <model> element. Expected \"model/vnd.usdz+zip\". Some features of <model> may not work properly. The model may fail to render in a future release."_s));
    }

    send(Messages::ModelProcessModelPlayerProxy::LoadModel(model, size));
}

void ModelProcessModelPlayer::didUnload()
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer unload model id=%" PRIu64, this, m_id.toUInt64());

    // If the page is closing while the model process disconnection resulted in
    // this being called, return early.
    RefPtr strongPage = m_page.get();
    if (!strongPage || !strongPage->corePage())
        return;

    RELEASE_ASSERT(modelProcessEnabled());

    if (RefPtr client = m_client.get())
        client->didUnload(*this);
}

void ModelProcessModelPlayer::reload(WebCore::Model& model, WebCore::LayoutSize size, WebCore::ModelPlayerAnimationState& animationState, std::unique_ptr<WebCore::ModelPlayerTransformState>&& transformState)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer reload model id=%" PRIu64, this, m_id.toUInt64());

    auto transformStateToRestore = WTFMove(transformState);
    ASSERT(transformStateToRestore);
    m_entityTransform = transformStateToRestore->entityTransform();
    m_boundingBoxCenter = transformStateToRestore->boundingBoxCenter();
    m_boundingBoxExtents = transformStateToRestore->boundingBoxExtents();
    setHasPortal(transformStateToRestore->hasPortal());
    setStageMode(transformStateToRestore->stageMode());
    m_animationState = WebCore::ModelPlayerAnimationState(animationState);
    send(Messages::ModelProcessModelPlayerProxy::ReloadModel(model, size, transformStateToRestore->entityTransform(), animationState));
}

void ModelProcessModelPlayer::visibilityStateDidChange()
{
    if (RefPtr client = m_client.get())
        send(Messages::ModelProcessModelPlayerProxy::ModelVisibilityDidChange(client->isVisible()));
}

void ModelProcessModelPlayer::sizeDidChange(WebCore::LayoutSize size)
{
    RELEASE_LOG_INFO(ModelElement, "%p - ModelProcessModelPlayer size did change to w=%f h=%f id=%" PRIu64, this, size.width().toFloat(), size.height().toFloat(), m_id.toUInt64());
    send(Messages::ModelProcessModelPlayerProxy::SizeDidChange(size));
}

void ModelProcessModelPlayer::configureGraphicsLayer(WebCore::GraphicsLayer& graphicsLayer, WebCore::ModelPlayerGraphicsLayerConfiguration&& configuration)
{
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    if (configuration.detachedForImmersive)
        return graphicsLayer.removeModelContents();
#endif

    auto modelLayerIdentifier = graphicsLayer.primaryLayerID();
    if (!modelLayerIdentifier)
        return;

    auto layerHostingContextIdentifier = m_layerHostingContextIdentifier;
    if (!layerHostingContextIdentifier)
        return;

    graphicsLayer.setContentsToModelContext(
        WebCore::ModelContext::create(
            *modelLayerIdentifier,
            *layerHostingContextIdentifier,
            configuration.contentSize,
            configuration.hasPortal ? WebCore::ModelContextDisablePortal::No : WebCore::ModelContextDisablePortal::Yes,
            configuration.backgroundColor
        ),
        WebCore::GraphicsLayer::ContentsLayerPurpose::HostedModel
    );
}

void ModelProcessModelPlayer::handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime)
{
}

void ModelProcessModelPlayer::handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime)
{
}

void ModelProcessModelPlayer::handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime)
{
}

void ModelProcessModelPlayer::enterFullscreen()
{
}

std::optional<WebCore::FloatPoint3D> ModelProcessModelPlayer::boundingBoxCenter() const
{
    return m_boundingBoxCenter;
}

std::optional<WebCore::FloatPoint3D> ModelProcessModelPlayer::boundingBoxExtents() const
{
    return m_boundingBoxExtents;
}

std::optional<WebCore::TransformationMatrix> ModelProcessModelPlayer::entityTransform() const
{
    return m_entityTransform;
}

/// This comes from JS side, so we need to tell Model Process about it. Not to be confused with didUpdateEntityTransform().
void ModelProcessModelPlayer::setEntityTransform(WebCore::TransformationMatrix transform)
{
    m_entityTransform = transform;
    send(Messages::ModelProcessModelPlayerProxy::SetEntityTransform(transform));
}

bool ModelProcessModelPlayer::supportsTransform(WebCore::TransformationMatrix transform)
{
    return ModelProcessModelPlayerTransformState::transformSupported(transform);
}

void ModelProcessModelPlayer::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::setCamera(WebCore::HTMLModelElementCamera camera, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::setAnimationIsPlaying(bool isPlaying, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::setIsLoopingAnimation(bool isLooping, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::setAnimationCurrentTime(Seconds currentTime, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::setIsMuted(bool isMuted, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

WebCore::ModelPlayerAccessibilityChildren ModelProcessModelPlayer::accessibilityChildren()
{
    return { };
}

void ModelProcessModelPlayer::setAutoplay(bool autoplay)
{
    if (m_animationState.autoplay() == autoplay)
        return;

    m_animationState.setAutoplay(autoplay);
    send(Messages::ModelProcessModelPlayerProxy::SetAutoplay(autoplay));
}

void ModelProcessModelPlayer::setLoop(bool loop)
{
    if (m_animationState.loop() == loop)
        return;

    m_animationState.setLoop(loop);
    send(Messages::ModelProcessModelPlayerProxy::SetLoop(loop));
}

void ModelProcessModelPlayer::setPlaybackRate(double playbackRate, CompletionHandler<void(double effectivePlaybackRate)>&& completionHandler)
{
    // FIXME (280081): Support negative playback rate
    m_requestedPlaybackRate = fmax(playbackRate, 0);
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::SetPlaybackRate(m_requestedPlaybackRate), WTFMove(completionHandler));
}

double ModelProcessModelPlayer::duration() const
{
    return m_animationState.duration().seconds();
}

bool ModelProcessModelPlayer::paused() const
{
    return m_animationState.paused();
}

void ModelProcessModelPlayer::setPaused(bool paused, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::SetPaused(paused), WTFMove(completionHandler));
}

Seconds ModelProcessModelPlayer::currentTime() const
{
    if (m_pendingCurrentTime)
        return *m_pendingCurrentTime;

    return m_animationState.currentTime();
}

void ModelProcessModelPlayer::setCurrentTime(Seconds currentTime, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    double durationSeconds = duration();
    if (!durationSeconds) {
        completionHandler();
        return;
    }

    m_pendingCurrentTime = Seconds(fmax(fmin(currentTime.seconds(), durationSeconds), 0));
    MonotonicTime timestamp = MonotonicTime::now();
    m_clockTimestampOfLastCurrentTimeSet = timestamp;

    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::SetCurrentTime(*m_pendingCurrentTime), [weakThis = WeakPtr { *this }, timestamp, completionHandler = WTFMove(completionHandler)]() mutable {
        ASSERT(RunLoop::isMain());
        if (RefPtr protectedThis = weakThis.get()) {
            if (protectedThis->m_clockTimestampOfLastCurrentTimeSet && *(protectedThis->m_clockTimestampOfLastCurrentTimeSet) <= timestamp) {
                protectedThis->m_pendingCurrentTime = std::nullopt;
                protectedThis->m_clockTimestampOfLastCurrentTimeSet = std::nullopt;
            }
        }
        completionHandler();
    });
}

void ModelProcessModelPlayer::setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data)
{
    send(Messages::ModelProcessModelPlayerProxy::SetEnvironmentMap(WTFMove(data)));
}

void ModelProcessModelPlayer::setHasPortal(bool hasPortal)
{
    if (m_hasPortal == hasPortal)
        return;

    m_hasPortal = hasPortal;
    send(Messages::ModelProcessModelPlayerProxy::SetHasPortal(m_hasPortal));
}

void ModelProcessModelPlayer::setStageMode(WebCore::StageModeOperation stagemodeOp)
{
    if (m_stageModeOperation == stagemodeOp)
        return;

    m_stageModeOperation = stagemodeOp;
    send(Messages::ModelProcessModelPlayerProxy::SetStageMode(m_stageModeOperation));
}

void ModelProcessModelPlayer::beginStageModeTransform(const WebCore::TransformationMatrix& transform)
{
    send(Messages::ModelProcessModelPlayerProxy::BeginStageModeTransform(transform));
}

void ModelProcessModelPlayer::updateStageModeTransform(const WebCore::TransformationMatrix& transform)
{
    send(Messages::ModelProcessModelPlayerProxy::UpdateStageModeTransform(transform));
}

void ModelProcessModelPlayer::endStageModeInteraction()
{
    send(Messages::ModelProcessModelPlayerProxy::EndStageModeInteraction());
}

void ModelProcessModelPlayer::animateModelToFitPortal(CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::AnimateModelToFitPortal(), WTFMove(completionHandler));
}

void ModelProcessModelPlayer::resetModelTransformAfterDrag()
{
    send(Messages::ModelProcessModelPlayerProxy::ResetModelTransformAfterDrag());
}

void ModelProcessModelPlayer::disableUnloadDelayForTesting()
{
    send(Messages::ModelProcessModelPlayerProxy::DisableUnloadDelayForTesting());
}

}

#endif // ENABLE(MODEL_PROCESS)
