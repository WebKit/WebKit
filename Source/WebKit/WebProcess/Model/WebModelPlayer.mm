/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebModelPlayer.h"

#if ENABLE(MODEL_PROCESS)

#import "WebPage.h"
#import "WebProcess.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>

namespace WebKit {

Ref<WebModelPlayer> WebModelPlayer::create(WebPage& page, WebCore::ModelPlayerClient& client)
{
    return adoptRef(*new WebModelPlayer(page, client));
}

WebModelPlayer::WebModelPlayer(WebPage& page, WebCore::ModelPlayerClient& client)
    : m_page { page }
    , m_client { client }
{
    // FIXME: This should remotely-host a layer tree from the model process.
    // For now we just launch the process to prove that it works.

    WebProcess::singleton().ensureModelProcessConnection();

    m_layer = adoptNS([[CALayer alloc] init]);
    [m_layer setBackgroundColor:cachedCGColor(Color::green).get()];
    [m_layer setFrame:CGRectMake(0, 0, 100, 100)];
}

WebModelPlayer::~WebModelPlayer()
{
}

void WebModelPlayer::load(WebCore::Model&, WebCore::LayoutSize)
{
}

void WebModelPlayer::sizeDidChange(LayoutSize)
{
}

PlatformLayer* WebModelPlayer::layer()
{
    return m_layer.get();
}

void WebModelPlayer::handleMouseDown(const LayoutPoint&, MonotonicTime)
{
}

void WebModelPlayer::handleMouseMove(const LayoutPoint&, MonotonicTime)
{
}

void WebModelPlayer::handleMouseUp(const LayoutPoint&, MonotonicTime)
{
}

void WebModelPlayer::enterFullscreen()
{
}

void WebModelPlayer::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebModelPlayer::setCamera(WebCore::HTMLModelElementCamera camera, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void WebModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(false);
}

void WebModelPlayer::setAnimationIsPlaying(bool isPlaying, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void WebModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebModelPlayer::setIsLoopingAnimation(bool isLooping, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void WebModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebModelPlayer::setAnimationCurrentTime(Seconds currentTime, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void WebModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void WebModelPlayer::setIsMuted(bool isMuted, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

Vector<RetainPtr<id>> WebModelPlayer::accessibilityChildren()
{
    return { };
}

}

#endif // ENABLE(MODEL_PROCESS)
