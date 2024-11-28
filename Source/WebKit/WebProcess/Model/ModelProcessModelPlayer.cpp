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

#include "config.h"
#include "ModelProcessModelPlayer.h"

#if ENABLE(MODEL_PROCESS)

#include "ModelProcessModelPlayerManager.h"
#include "ModelProcessModelPlayerProxy.h"
#include "ModelProcessModelPlayerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/LayerHostingContextIdentifier.h>
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
    m_client->didUpdateLayerHostingContextIdentifier(*this, identifier);
}

void ModelProcessModelPlayer::didFinishLoading(const WebCore::FloatPoint3D& boundingBoxCenter, const WebCore::FloatPoint3D& boundingBoxExtents)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer didFinishLoading id=%" PRIu64, this, m_id.toUInt64());
    RELEASE_ASSERT(modelProcessEnabled());

    m_client->didFinishLoading(*this);
    m_client->didUpdateBoundingBox(*this, boundingBoxCenter, boundingBoxExtents);
}

/// This comes from Model Process side, so that Web Process has the most up-to-date knowledge about the transform actually applied to the entity.
/// Not to be confused with setEntityTransform().
void ModelProcessModelPlayer::didUpdateEntityTransform(const WebCore::TransformationMatrix& transform)
{
    RELEASE_ASSERT(modelProcessEnabled());

    m_client->didUpdateEntityTransform(*this, transform);
}

void ModelProcessModelPlayer::didUpdateAnimationPlaybackState(bool isPaused, double playbackRate, Seconds duration, Seconds currentTime, MonotonicTime clockTimestamp)
{
    RELEASE_ASSERT(modelProcessEnabled());

    m_paused = isPaused;
    m_effectivePlaybackRate = fmax(playbackRate, 0);
    m_duration = duration;
    m_lastCachedCurrentTime = currentTime;
    m_lastCachedClockTimestamp = clockTimestamp;
}

void ModelProcessModelPlayer::didFinishEnvironmentMapLoading(bool succeeded)
{
    RELEASE_ASSERT(modelProcessEnabled());

    m_client->didFinishEnvironmentMapLoading(succeeded);
}

// MARK: - WebCore::ModelPlayer

void ModelProcessModelPlayer::load(WebCore::Model& model, WebCore::LayoutSize size)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer load model id=%" PRIu64, this, m_id.toUInt64());
    send(Messages::ModelProcessModelPlayerProxy::LoadModel(model, size));
}

void ModelProcessModelPlayer::sizeDidChange(WebCore::LayoutSize size)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayer size did change to w=%f h=%f id=%" PRIu64, this, size.width().toFloat(), size.height().toFloat(), m_id.toUInt64());
    send(Messages::ModelProcessModelPlayerProxy::SizeDidChange(size));
}

PlatformLayer* ModelProcessModelPlayer::layer()
{
    return nullptr;
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

void ModelProcessModelPlayer::setBackgroundColor(WebCore::Color color)
{
    send(Messages::ModelProcessModelPlayerProxy::SetBackgroundColor(color));
}

/// This comes from JS side, so we need to tell Model Process about it. Not to be confused with didUpdateEntityTransform().
void ModelProcessModelPlayer::setEntityTransform(WebCore::TransformationMatrix transform)
{
    send(Messages::ModelProcessModelPlayerProxy::SetEntityTransform(transform));
}

bool ModelProcessModelPlayer::supportsTransform(WebCore::TransformationMatrix transform)
{
    return ModelProcessModelPlayerProxy::transformSupported(transform);
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

Vector<RetainPtr<id>> ModelProcessModelPlayer::accessibilityChildren()
{
    return { };
}

void ModelProcessModelPlayer::setAutoplay(bool autoplay)
{
    if (m_autoplay == autoplay)
        return;

    m_autoplay = autoplay;
    send(Messages::ModelProcessModelPlayerProxy::SetAutoplay(autoplay));
}

void ModelProcessModelPlayer::setLoop(bool loop)
{
    if (m_loop == loop)
        return;

    m_loop = loop;
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
    return m_duration.seconds();
}

bool ModelProcessModelPlayer::paused() const
{
    return m_paused;
}

void ModelProcessModelPlayer::setPaused(bool paused, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    sendWithAsyncReply(Messages::ModelProcessModelPlayerProxy::SetPaused(paused), WTFMove(completionHandler));
}

Seconds ModelProcessModelPlayer::currentTime() const
{
    if (!m_duration || !m_lastCachedCurrentTime || !m_lastCachedClockTimestamp || !m_effectivePlaybackRate)
        return 0_s;

    if (m_pendingCurrentTime)
        return *m_pendingCurrentTime;

    Seconds lastCachedCurrentTime = *m_lastCachedCurrentTime;
    MonotonicTime lastCachedTimestamp = *m_lastCachedClockTimestamp;
    double playbackRate = *m_effectivePlaybackRate;

    if (m_paused)
        return lastCachedCurrentTime;

    // Approximate based on last cached animation time, clock timestamp, and playbackRate
    Seconds timePassedSinceLastSync = MonotonicTime::now() - lastCachedTimestamp;
    Seconds animationTimePassed = Seconds::fromMilliseconds(floor((timePassedSinceLastSync * playbackRate).milliseconds()));
    Seconds estimatedCurrentTime = lastCachedCurrentTime + animationTimePassed;
    if (estimatedCurrentTime > m_duration)
        estimatedCurrentTime = m_loop ? estimatedCurrentTime % m_duration : m_duration;
    return estimatedCurrentTime;
}

void ModelProcessModelPlayer::setCurrentTime(Seconds currentTime, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    double durationSeconds = m_duration.seconds();
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

}

#endif // ENABLE(MODEL_PROCESS)
