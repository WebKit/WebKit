/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ModelPlayerTransformState.h"
#include "TransformationMatrix.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

Ref<PlaceholderModelPlayer> PlaceholderModelPlayer::create(bool suspended, const ModelPlayerAnimationState& animationState, std::unique_ptr<ModelPlayerTransformState>&& transformState)
{
    return adoptRef(*new PlaceholderModelPlayer(suspended, animationState, WTFMove(transformState)));
}

PlaceholderModelPlayer::PlaceholderModelPlayer(bool suspended, const ModelPlayerAnimationState& animationState, std::unique_ptr<ModelPlayerTransformState>&& transformState)
    : m_animationState(animationState)
    , m_transformState(WTFMove(transformState))
#if ENABLE(MODEL_PROCESS) || ENABLE(GPUP_MODEL)
    , m_id(ModelPlayerIdentifier::generate())
#endif
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

std::optional<ModelPlayerAnimationState> PlaceholderModelPlayer::currentAnimationState() const
{
    if (!m_lastPausedStateIfSuspended || *m_lastPausedStateIfSuspended)
        return m_animationState;

    auto unsuspendedAnimationState = ModelPlayerAnimationState(m_animationState);
    unsuspendedAnimationState.setCurrentTime(unsuspendedAnimationState.currentTime(), MonotonicTime::now());
    unsuspendedAnimationState.setPaused(false);
    return unsuspendedAnimationState;
}

std::optional<std::unique_ptr<ModelPlayerTransformState>> PlaceholderModelPlayer::currentTransformState() const
{
    return m_transformState->clone();
}

void PlaceholderModelPlayer::load(Model&, LayoutSize)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void PlaceholderModelPlayer::reload(Model&, LayoutSize, ModelPlayerAnimationState&, std::unique_ptr<ModelPlayerTransformState>&&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

std::optional<WebCore::FloatPoint3D> PlaceholderModelPlayer::boundingBoxCenter() const
{
    return m_transformState->boundingBoxCenter();
}

std::optional<WebCore::FloatPoint3D> PlaceholderModelPlayer::boundingBoxExtents() const
{
    return m_transformState->boundingBoxExtents();
}

std::optional<WebCore::TransformationMatrix> PlaceholderModelPlayer::entityTransform() const
{
    return m_transformState->entityTransform();
}

/// This comes from JS side, so we need to tell Model Process about it. Not to be confused with didUpdateEntityTransform().
void PlaceholderModelPlayer::setEntityTransform(WebCore::TransformationMatrix transform)
{
#if ENABLE(MODEL_PROCESS)
    ASSERT(m_transformState->stageMode() == StageModeOperation::None);
#endif
    m_transformState->setEntityTransform(transform);
}

bool PlaceholderModelPlayer::supportsTransform(WebCore::TransformationMatrix transform)
{
    return m_transformState->isEntityTransformSupported(transform);
}

#if ENABLE(MODEL_PROCESS) || ENABLE(GPUP_MODEL)
void PlaceholderModelPlayer::setAutoplay(bool autoplay)
{
    m_animationState.setAutoplay(autoplay);
}

void PlaceholderModelPlayer::setLoop(bool loop)
{
    m_animationState.setLoop(loop);
}

void PlaceholderModelPlayer::setPlaybackRate(double playbackRate, CompletionHandler<void(double effectivePlaybackRate)>&& completionHandler)
{
    if (m_animationState.effectivePlaybackRate() == playbackRate)
        return;

    m_animationState.setCurrentTime(m_animationState.currentTime(), MonotonicTime::now());
    m_animationState.setPlaybackRate(playbackRate);
    auto effectivePlaybackRate = m_animationState.effectivePlaybackRate();
    completionHandler(effectivePlaybackRate ? *effectivePlaybackRate : 1.0);
}

double PlaceholderModelPlayer::duration() const
{
    return m_animationState.duration().seconds();
}

bool PlaceholderModelPlayer::paused() const
{
    return m_animationState.paused();
}

void PlaceholderModelPlayer::setPaused(bool paused, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    if (m_animationState.paused() == paused)
        return;

    m_animationState.setCurrentTime(m_animationState.currentTime(), MonotonicTime::now());
    m_animationState.setPaused(paused);
    completionHandler(true);
}

Seconds PlaceholderModelPlayer::currentTime() const
{
    return m_animationState.currentTime();
}

void PlaceholderModelPlayer::setCurrentTime(Seconds currentTime, CompletionHandler<void()>&& completionHandler)
{
    double durationSeconds = m_animationState.duration().seconds();
    if (durationSeconds)
        m_animationState.setCurrentTime(currentTime, MonotonicTime::now());

    completionHandler();
}

void PlaceholderModelPlayer::setHasPortal(bool hasPortal)
{
#if ENABLE(MODEL_PROCESS)
    m_transformState->setHasPortal(hasPortal);
#else
    UNUSED_PARAM(hasPortal);
#endif
}

#if ENABLE(MODEL_PROCESS)
void PlaceholderModelPlayer::setStageMode(WebCore::StageModeOperation stageModeOperation)
{
    m_transformState->setStageMode(stageModeOperation);
}
#endif
#endif

// Empty implementation
PlatformLayer* PlaceholderModelPlayer::layer()
{
    return nullptr;
}

std::optional<LayerHostingContextIdentifier> PlaceholderModelPlayer::layerHostingContextIdentifier()
{
    return std::nullopt;
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

void PlaceholderModelPlayer::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&&)
{
}

void PlaceholderModelPlayer::setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&&)
{
}

void PlaceholderModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void PlaceholderModelPlayer::setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&)
{
}

void PlaceholderModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void PlaceholderModelPlayer::setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&)
{
}

void PlaceholderModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void PlaceholderModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void PlaceholderModelPlayer::setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&)
{
}

void PlaceholderModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void PlaceholderModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void PlaceholderModelPlayer::setIsMuted(bool, CompletionHandler<void(bool success)>&&)
{
}

#if PLATFORM(COCOA)
Vector<RetainPtr<id>> PlaceholderModelPlayer::accessibilityChildren()
{
    return { };
}
#endif

#if ENABLE(GPUP_MODEL)
const MachSendRight* PlaceholderModelPlayer::displayBuffer() const
{
    return nullptr;
}

GraphicsLayerContentsDisplayDelegate* PlaceholderModelPlayer::contentsDisplayDelegate()
{
    return nullptr;
}
#endif

}
