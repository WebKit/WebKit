/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "PlaceholderModelPlayer.h"

#include "FloatPoint3D.h"
#include "Logging.h"
#include "Model.h"
#include "ModelPlayerGraphicsLayerConfiguration.h"
#include "ModelPlayerTransformState.h"
#include "TransformationMatrix.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

Ref<PlaceholderModelPlayer> PlaceholderModelPlayer::create(bool suspended, const ModelPlayerAnimationState& animationState, std::unique_ptr<ModelPlayerTransformState>&& transformState)
{
    return adoptRef(*new PlaceholderModelPlayer(suspended, animationState, WTF::move(transformState)));
}

PlaceholderModelPlayer::PlaceholderModelPlayer(bool suspended, const ModelPlayerAnimationState& animationState, std::unique_ptr<ModelPlayerTransformState>&& transformState)
    : m_animationState(animationState)
    , m_transformState(WTF::move(transformState))
    , m_id(ModelPlayerIdentifier::generate())
{
    ASSERT(m_transformState);

    RELEASE_LOG(ModelElement, "%p - PlaceholderModelPlayer created when model was suspended: %d", this, suspended);
    if (!suspended)
        return;

    m_lastPausedStateIfSuspended = m_animationState.paused();
    if (m_animationState.paused())
        return;

    m_animationState.setCurrentTime(animationState.currentTime(), MonotonicTime::now());
    m_animationState.setPaused(true);
}

PlaceholderModelPlayer::~PlaceholderModelPlayer() = default;

std::optional<ModelPlayerAnimationState> PlaceholderModelPlayer::currentAnimationState(NodeIdentifier) const
{
    if (!m_lastPausedStateIfSuspended || *m_lastPausedStateIfSuspended)
        return m_animationState;

    auto unsuspendedAnimationState = ModelPlayerAnimationState(m_animationState);
    unsuspendedAnimationState.setCurrentTime(unsuspendedAnimationState.currentTime(), MonotonicTime::now());
    unsuspendedAnimationState.setPaused(false);
    return unsuspendedAnimationState;
}

std::optional<std::unique_ptr<ModelPlayerTransformState>> PlaceholderModelPlayer::currentTransformState(NodeIdentifier) const
{
    return m_transformState->clone();
}

void PlaceholderModelPlayer::load(NodeIdentifier, Model&, LayoutSize, bool)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void PlaceholderModelPlayer::reload(NodeIdentifier, Model&, LayoutSize, ModelPlayerAnimationState&, std::unique_ptr<ModelPlayerTransformState>&&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)

std::optional<WebCore::FloatPoint3D> PlaceholderModelPlayer::boundingBoxCenter(NodeIdentifier) const
{
    return m_transformState->boundingBoxCenter();
}

std::optional<WebCore::FloatPoint3D> PlaceholderModelPlayer::boundingBoxExtents(NodeIdentifier) const
{
    return m_transformState->boundingBoxExtents();
}

#endif

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)

std::optional<WebCore::TransformationMatrix> PlaceholderModelPlayer::entityTransform(NodeIdentifier) const
{
    return m_transformState->entityTransform();
}

/// This comes from JS side, so we need to tell Model Process about it. Not to be confused with didUpdateEntityTransform().
void PlaceholderModelPlayer::setEntityTransform(NodeIdentifier, TransformationMatrix transform)
{
#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
    ASSERT(m_transformState->stageMode() == StageModeOperation::None);
#endif
    m_transformState->setEntityTransform(transform);
}

bool PlaceholderModelPlayer::supportsTransform(WebCore::TransformationMatrix transform)
{
    return m_transformState->isEntityTransformSupported(transform);
}

#endif

#if ENABLE(MODEL_ELEMENT_ANIMATIONS_CONTROL)

void PlaceholderModelPlayer::setAutoplay(NodeIdentifier, bool autoplay)
{
    m_animationState.setAutoplay(autoplay);
}

void PlaceholderModelPlayer::setLoop(NodeIdentifier, bool loop)
{
    m_animationState.setLoop(loop);
}

void PlaceholderModelPlayer::setPlaybackRate(NodeIdentifier, double playbackRate, CompletionHandler<void(double effectivePlaybackRate)>&& completionHandler)
{
    if (m_animationState.effectivePlaybackRate() == playbackRate)
        return completionHandler(playbackRate);

    m_animationState.setCurrentTime(m_animationState.currentTime(), MonotonicTime::now());
    m_animationState.setPlaybackRate(playbackRate);
    auto effectivePlaybackRate = m_animationState.effectivePlaybackRate();
    completionHandler(effectivePlaybackRate ? *effectivePlaybackRate : 1.0);
}

double PlaceholderModelPlayer::duration(NodeIdentifier) const
{
    return m_animationState.duration().seconds();
}

bool PlaceholderModelPlayer::paused(NodeIdentifier) const
{
    return m_animationState.paused();
}

void PlaceholderModelPlayer::setPaused(NodeIdentifier, bool paused, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    if (m_animationState.paused() == paused)
        return completionHandler(true);

    m_animationState.setCurrentTime(m_animationState.currentTime(), MonotonicTime::now());
    m_animationState.setPaused(paused);
    completionHandler(true);
}

Seconds PlaceholderModelPlayer::currentTime(NodeIdentifier) const
{
    return m_animationState.currentTime();
}

void PlaceholderModelPlayer::setCurrentTime(NodeIdentifier, Seconds currentTime, CompletionHandler<void()>&& completionHandler)
{
    double durationSeconds = m_animationState.duration().seconds();
    if (durationSeconds)
        m_animationState.setCurrentTime(currentTime, MonotonicTime::now());

    completionHandler();
}

#endif

#if ENABLE(MODEL_ELEMENT_PORTAL)

void PlaceholderModelPlayer::setHasPortal(bool hasPortal)
{
    m_transformState->setHasPortal(hasPortal);
}

#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)

void PlaceholderModelPlayer::setStageMode(WebCore::StageModeOperation stageModeOperation)
{
    m_transformState->setStageMode(stageModeOperation);
}

#endif

// Empty implementation

void PlaceholderModelPlayer::configureGraphicsLayer(GraphicsLayer&, ModelPlayerGraphicsLayerConfiguration&&)
{
}

void PlaceholderModelPlayer::sizeDidChange(LayoutSize)
{
}

void PlaceholderModelPlayer::enterFullscreen()
{
}

void PlaceholderModelPlayer::handleMouseDown(const LayoutPoint&, MonotonicTime)
{
}

void PlaceholderModelPlayer::handleMouseMove(const LayoutPoint&, MonotonicTime)
{
}

void PlaceholderModelPlayer::handleMouseUp(const LayoutPoint&, MonotonicTime)
{
}

void PlaceholderModelPlayer::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void PlaceholderModelPlayer::setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void PlaceholderModelPlayer::isPlayingAnimation(NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void PlaceholderModelPlayer::setAnimationIsPlaying(NodeIdentifier, bool, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void PlaceholderModelPlayer::isLoopingAnimation(NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void PlaceholderModelPlayer::setIsLoopingAnimation(NodeIdentifier, bool, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void PlaceholderModelPlayer::animationDuration(NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void PlaceholderModelPlayer::animationCurrentTime(NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void PlaceholderModelPlayer::setAnimationCurrentTime(NodeIdentifier, Seconds, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)

ModelPlayerAccessibilityChildren PlaceholderModelPlayer::accessibilityChildren()
{
    return { };
}

#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

void PlaceholderModelPlayer::ensureImmersivePresentation(CompletionHandler<void(std::optional<LayerHostingContextIdentifier>)>&& completion)
{
    ASSERT_NOT_REACHED("PlaceholderModelPlayer cannot provide a layer context identifier");
    completion(std::nullopt);
}

void PlaceholderModelPlayer::exitImmersivePresentation(CompletionHandler<void()>&& completion)
{
    m_transformState->invalidateTransform();
    completion();
}

#endif

} // namespace WebCore
