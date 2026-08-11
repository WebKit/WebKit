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

ModelProcessModelPlayer::NodeAnimationState& ModelProcessModelPlayer::ensureAnimationState(WebCore::NodeIdentifier nodeID)
{
    return m_animationStates.ensure(nodeID, [] {
        return NodeAnimationState { };
    }).iterator->value;
}

const ModelProcessModelPlayer::NodeAnimationState* ModelProcessModelPlayer::animationStateIfExists(WebCore::NodeIdentifier nodeID) const
{
    auto it = m_animationStates.find(nodeID);
    if (it == m_animationStates.end())
        return nullptr;

    return &it->value;
}

ModelProcessModelPlayer::NodeAnimationState* ModelProcessModelPlayer::animationStateIfExists(WebCore::NodeIdentifier nodeID)
{
    auto it = m_animationStates.find(nodeID);
    if (it == m_animationStates.end())
        return nullptr;

    return &it->value;
}

// MARK: - Messages

void ModelProcessModelPlayer::didCreateLayer(WebCore::LayerHostingContextIdentifier identifier)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer obtained new layerHostingContextIdentifier id=%" PRIu64, this, m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    m_layerHostingContextIdentifier = identifier;
    protect(client())->didUpdate(*this);
}

void ModelProcessModelPlayer::didFinishLoading(WebCore::NodeIdentifier nodeID, const WebCore::FloatPoint3D& boundingBoxCenter, const WebCore::FloatPoint3D& boundingBoxExtents)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer didFinishLoading id=%" PRIu64, this, m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    m_boundingBoxCenter = boundingBoxCenter;
    m_boundingBoxExtents = boundingBoxExtents;

    RefPtr client = m_client.get();
    client->didFinishLoading(*this, nodeID);
    client->didUpdateBoundingBox(*this, nodeID, boundingBoxCenter, boundingBoxExtents);
}

void ModelProcessModelPlayer::didConvertModelData(Ref<WebCore::SharedBuffer>&& convertedData, const String& convertedMIMEType)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer didConvertModelData mimeType=%s id=%" PRIu64, this, convertedMIMEType.utf8().data(), m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    protect(client())->didConvertModelData(*this, WTF::move(convertedData), convertedMIMEType);
}

void ModelProcessModelPlayer::didFailLoading(WebCore::NodeIdentifier nodeID)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer didFailLoading id=%" PRIu64, this, m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    m_animationStates.remove(nodeID);

    protect(client())->didFailLoading(*this, nodeID, WebCore::ResourceError { WebCore::errorDomainWebKitInternal, 0, { }, "Failed to load model data"_s });
}

/// This comes from Model Process side, so that Web Process has the most up-to-date knowledge about the transform actually applied to the entity.
/// Not to be confused with setEntityTransform().
void ModelProcessModelPlayer::didUpdateEntityTransform(WebCore::NodeIdentifier nodeID, const WebCore::TransformationMatrix& transform)
{
    RELEASE_ASSERT(modelProcessEnabled());

    m_entityTransform = transform;
    protect(client())->didUpdateEntityTransform(*this, nodeID, transform);
}

#if ENABLE(SPATIAL_PORTAL)

void ModelProcessModelPlayer::didUpdatePortalTransform(const WebCore::TransformationMatrix& transform)
{
    RELEASE_ASSERT(modelProcessEnabled());

    protect(client())->didUpdatePortalTransform(*this, transform);
}

#endif

void ModelProcessModelPlayer::didUpdateAnimationPlaybackState(WebCore::NodeIdentifier nodeID, bool isPaused, double playbackRate, Seconds duration, Seconds currentTime, MonotonicTime clockTimestamp)
{
    RELEASE_ASSERT(modelProcessEnabled());

    auto* nodeAnimationState = animationStateIfExists(nodeID);
    if (!nodeAnimationState)
        return;

    auto& animationState = nodeAnimationState->playbackState;
    animationState.setPaused(isPaused);
    animationState.setDuration(duration);
    animationState.setPlaybackRate(playbackRate);
    animationState.setCurrentTime(currentTime, clockTimestamp);
}

void ModelProcessModelPlayer::didFinishEnvironmentMapLoading(bool succeeded)
{
    RELEASE_ASSERT(modelProcessEnabled());

    protect(client())->didFinishEnvironmentMapLoading(*this, succeeded);
}

// MARK: - WebCore::ModelPlayer

std::optional<WebCore::ModelPlayerAnimationState> ModelProcessModelPlayer::currentAnimationState(WebCore::NodeIdentifier nodeID) const
{
    // Has no current state to return if the model load hasn't returned with its extents.
    if (!m_boundingBoxExtents)
        return std::nullopt;

    if (auto* nodeAnimationState = animationStateIfExists(nodeID))
        return nodeAnimationState->playbackState;

    return std::nullopt;
}

std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> ModelProcessModelPlayer::currentTransformState(WebCore::NodeIdentifier) const
{
    // Has no current state to return if the model load hasn't returned with its extents.
    if (!m_boundingBoxExtents)
        return std::nullopt;

    return ModelProcessModelPlayerTransformState::create(m_entityTransform, m_boundingBoxCenter, m_boundingBoxExtents, m_hasPortal, m_stageModeOperation);
}

void ModelProcessModelPlayer::load(WebCore::NodeIdentifier nodeID, WebCore::Model& model, WebCore::LayoutSize size, bool isForImmersive)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer load model id=%" PRIu64, this, m_id.toUInt64());

    if (!WebCore::MIMETypeRegistry::isUSDMIMEType(model.mimeType())) {
        RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer::load: Found unexpected model mimetype: %s", this, model.mimeType().utf8().data());
        if (RefPtr client = m_client.get())
            client->logWarning(*this, makeString("Unexpected USDZ MIME type \""_s, model.mimeType(), "\" in <model> element. Expected \"model/vnd.usdz+zip\". Some features of <model> may not work properly. The model may fail to render in a future release."_s));
    }

    send(Messages::ModelProcessModelPlayerProxy::LoadModel(nodeID, model, size, isForImmersive));
}

void ModelProcessModelPlayer::unload(WebCore::NodeIdentifier nodeID)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer unload model nodeID=%" PRIu64 " id=%" PRIu64, this, nodeID.toUInt64(), m_id.toUInt64());

    m_animationStates.remove(nodeID);
    send(Messages::ModelProcessModelPlayerProxy::UnloadModel(nodeID));
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

void ModelProcessModelPlayer::reload(WebCore::NodeIdentifier nodeID, WebCore::Model& model, WebCore::LayoutSize size, WebCore::ModelPlayerAnimationState& animationState, std::unique_ptr<WebCore::ModelPlayerTransformState>&& transformState)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer reload model id=%" PRIu64, this, m_id.toUInt64());

    auto transformStateToRestore = WTF::move(transformState);
    ASSERT(transformStateToRestore);
    m_entityTransform = transformStateToRestore->entityTransform();
    m_boundingBoxCenter = transformStateToRestore->boundingBoxCenter();
    m_boundingBoxExtents = transformStateToRestore->boundingBoxExtents();
    setHasPortal(transformStateToRestore->hasPortal());
    setStageMode(transformStateToRestore->stageMode());
    ensureAnimationState(nodeID).playbackState = WebCore::ModelPlayerAnimationState(animationState);
    send(Messages::ModelProcessModelPlayerProxy::ReloadModel(nodeID, model, size, transformStateToRestore->entityTransform(), animationState));
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
            configuration.contentOrigin,
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

std::optional<WebCore::FloatPoint3D> ModelProcessModelPlayer::boundingBoxCenter(WebCore::NodeIdentifier) const
{
    return m_boundingBoxCenter;
}

std::optional<WebCore::FloatPoint3D> ModelProcessModelPlayer::boundingBoxExtents(WebCore::NodeIdentifier) const
{
    return m_boundingBoxExtents;
}

std::optional<WebCore::TransformationMatrix> ModelProcessModelPlayer::entityTransform(WebCore::NodeIdentifier) const
{
    return m_entityTransform;
}

/// This comes from JS side, so we need to tell Model Process about it. Not to be confused with didUpdateEntityTransform().
void ModelProcessModelPlayer::setEntityTransform(WebCore::NodeIdentifier nodeID, WebCore::TransformationMatrix transform)
{
    m_entityTransform = transform;
    send(Messages::ModelProcessModelPlayerProxy::SetEntityTransform(nodeID, transform));
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

void ModelProcessModelPlayer::isPlayingAnimation(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::setAnimationIsPlaying(WebCore::NodeIdentifier, bool isPlaying, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::isLoopingAnimation(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::setIsLoopingAnimation(WebCore::NodeIdentifier, bool isLooping, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayer::animationDuration(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::animationCurrentTime(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayer::setAnimationCurrentTime(WebCore::NodeIdentifier, Seconds currentTime, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

WebCore::ModelPlayerAccessibilityChildren ModelProcessModelPlayer::accessibilityChildren()
{
    return { };
}

void ModelProcessModelPlayer::setAutoplay(WebCore::NodeIdentifier nodeID, bool autoplay)
{
    auto& animationState = ensureAnimationState(nodeID).playbackState;
    if (animationState.autoplay() == autoplay)
        return;

    animationState.setAutoplay(autoplay);
    send(Messages::ModelProcessModelPlayerProxy::SetAutoplay(nodeID, autoplay));
}

void ModelProcessModelPlayer::setLoop(WebCore::NodeIdentifier nodeID, bool loop)
{
    auto& animationState = ensureAnimationState(nodeID).playbackState;
    if (animationState.loop() == loop)
        return;

    animationState.setLoop(loop);
    send(Messages::ModelProcessModelPlayerProxy::SetLoop(nodeID, loop));
}

void ModelProcessModelPlayer::setPlaybackRate(WebCore::NodeIdentifier nodeID, double playbackRate, CompletionHandler<void(double effectivePlaybackRate)>&& completionHandler)
{
    ensureAnimationState(nodeID).playbackState.setPlaybackRate(playbackRate);
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::SetPlaybackRate(nodeID, playbackRate), WTF::move(completionHandler));
}

double ModelProcessModelPlayer::duration(WebCore::NodeIdentifier nodeID) const
{
    if (auto* animationState = animationStateIfExists(nodeID))
        return animationState->playbackState.duration().seconds();

    return 0;
}

bool ModelProcessModelPlayer::paused(WebCore::NodeIdentifier nodeID) const
{
    if (auto* animationState = animationStateIfExists(nodeID))
        return animationState->playbackState.paused();

    return true;
}

void ModelProcessModelPlayer::setPaused(WebCore::NodeIdentifier nodeID, bool paused, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::SetPaused(nodeID, paused), WTF::move(completionHandler));
}

Seconds ModelProcessModelPlayer::currentTime(WebCore::NodeIdentifier nodeID) const
{
    auto* animationState = animationStateIfExists(nodeID);
    if (!animationState)
        return 0_s;

    if (animationState->pendingCurrentTime)
        return *animationState->pendingCurrentTime;

    return animationState->playbackState.currentTime();
}

void ModelProcessModelPlayer::setCurrentTime(WebCore::NodeIdentifier nodeID, Seconds currentTime, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    auto& animationState = ensureAnimationState(nodeID);
    double durationSeconds = animationState.playbackState.duration().seconds();
    if (!durationSeconds) {
        completionHandler();
        return;
    }

    animationState.pendingCurrentTime = Seconds(fmax(fmin(currentTime.seconds(), durationSeconds), 0));
    MonotonicTime timestamp = MonotonicTime::now();
    animationState.clockTimestampOfLastCurrentTimeSet = timestamp;

    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::SetCurrentTime(nodeID, *animationState.pendingCurrentTime), [weakThis = WeakPtr { *this }, nodeID, timestamp, completionHandler = WTF::move(completionHandler)]() mutable {
        ASSERT(RunLoop::isMain());
        if (RefPtr protectedThis = weakThis.get()) {
            auto it = protectedThis->m_animationStates.find(nodeID);
            if (it != protectedThis->m_animationStates.end() && it->value.clockTimestampOfLastCurrentTimeSet && *it->value.clockTimestampOfLastCurrentTimeSet <= timestamp) {
                it->value.pendingCurrentTime = std::nullopt;
                it->value.clockTimestampOfLastCurrentTimeSet = std::nullopt;
            }
        }
        completionHandler();
    });
}

void ModelProcessModelPlayer::setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data)
{
    send(Messages::ModelProcessModelPlayerProxy::SetEnvironmentMap(WTF::move(data)));
}

void ModelProcessModelPlayer::setHasPortal(bool hasPortal)
{
    if (m_hasPortal == hasPortal)
        return;

    m_hasPortal = hasPortal;
    send(Messages::ModelProcessModelPlayerProxy::SetHasPortal(m_hasPortal));
}

#if ENABLE(SPATIAL_PORTAL)

void ModelProcessModelPlayer::setPortalTransform(WebCore::PortalTransformKind kind)
{
    if (m_portalTransform == kind)
        return;

    m_portalTransform = kind;
    send(Messages::ModelProcessModelPlayerProxy::SetPortalTransform(m_portalTransform));
}

void ModelProcessModelPlayer::setPortalAction(WebCore::PortalActionKind kind)
{
    if (m_portalAction == kind)
        return;

    m_portalAction = kind;
    send(Messages::ModelProcessModelPlayerProxy::SetPortalAction(m_portalAction));
}

#endif

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
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::AnimateModelToFitPortal(), WTF::move(completionHandler));
}

void ModelProcessModelPlayer::resetModelTransformAfterDrag()
{
    send(Messages::ModelProcessModelPlayerProxy::ResetModelTransformAfterDrag());
}

void ModelProcessModelPlayer::disableUnloadDelayForTesting()
{
    send(Messages::ModelProcessModelPlayerProxy::DisableUnloadDelayForTesting());
}

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

void ModelProcessModelPlayer::ensureImmersivePresentation(CompletionHandler<void(std::optional<WebCore::LayerHostingContextIdentifier>)>&& completion)
{
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::EnsureImmersivePresentation(), WTF::move(completion));
}

void ModelProcessModelPlayer::exitImmersivePresentation(CompletionHandler<void()>&& completion)
{
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::ExitImmersivePresentation(), WTF::move(completion));
}

#endif

}

#endif // ENABLE(MODEL_PROCESS)
