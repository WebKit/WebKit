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
#include "DummyModelPlayer.h"

#include "Model.h"
#include "ResourceError.h"

namespace WebCore {

Ref<DummyModelPlayer> DummyModelPlayer::create(ModelPlayerClient& client)
{
    return adoptRef(*new DummyModelPlayer(client));
}

DummyModelPlayer::DummyModelPlayer(ModelPlayerClient& client)
    : m_client { client }
{
}

DummyModelPlayer::~DummyModelPlayer() = default;

void DummyModelPlayer::load(Model& model, LayoutSize)
{
    if (m_client)
        m_client->didFailLoading(*this, ResourceError { errorDomainWebKitInternal, 0, model.url(), "Trying to load model via DummyModelPlayer"_s });
}

PlatformLayer* DummyModelPlayer::layer()
{
    return nullptr;
}

std::optional<LayerHostingContextIdentifier> DummyModelPlayer::layerHostingContextIdentifier()
{
    return std::nullopt;
}

void DummyModelPlayer::sizeDidChange(LayoutSize)
{
}

void DummyModelPlayer::enterFullscreen()
{
}

void DummyModelPlayer::handleMouseDown(const LayoutPoint&, MonotonicTime)
{
}

void DummyModelPlayer::handleMouseMove(const LayoutPoint&, MonotonicTime)
{
}

void DummyModelPlayer::handleMouseUp(const LayoutPoint&, MonotonicTime)
{
}

void DummyModelPlayer::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&&)
{
}

void DummyModelPlayer::setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&&)
{
}

void DummyModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DummyModelPlayer::setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&)
{
}

void DummyModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DummyModelPlayer::setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&)
{
}

void DummyModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void DummyModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void DummyModelPlayer::setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&)
{
}

void DummyModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DummyModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DummyModelPlayer::setIsMuted(bool, CompletionHandler<void(bool success)>&&)
{
}

#if PLATFORM(COCOA)
Vector<RetainPtr<id>> DummyModelPlayer::accessibilityChildren()
{
    return { };
}
#endif

}
