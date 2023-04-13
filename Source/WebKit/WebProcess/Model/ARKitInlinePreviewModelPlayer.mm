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

#import "config.h"
#import "ARKitInlinePreviewModelPlayer.h"

#if ENABLE(ARKIT_INLINE_PREVIEW)

#import "MessageSenderInlines.h"
#import "WebPage.h"

namespace WebKit {

ARKitInlinePreviewModelPlayer::ARKitInlinePreviewModelPlayer(WebPage& page, WebCore::ModelPlayerClient& client)
    : m_page { page }
    , m_client { client }
{
}

ARKitInlinePreviewModelPlayer::~ARKitInlinePreviewModelPlayer()
{
}

void ARKitInlinePreviewModelPlayer::load(WebCore::Model&, WebCore::LayoutSize)
{
}

void ARKitInlinePreviewModelPlayer::sizeDidChange(LayoutSize)
{
}

PlatformLayer* ARKitInlinePreviewModelPlayer::layer()
{
    return nullptr;
}

void ARKitInlinePreviewModelPlayer::enterFullscreen()
{
}

void ARKitInlinePreviewModelPlayer::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(std::nullopt);
        return;
    }

    CompletionHandler<void(Expected<WebCore::HTMLModelElementCamera, WebCore::ResourceError>)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (Expected<WebCore::HTMLModelElementCamera, WebCore::ResourceError> result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(*result);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementGetCamera(*modelIdentifier), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::setCamera(WebCore::HTMLModelElementCamera camera, CompletionHandler<void(bool success)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(false);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(false);
        return;
    }

    CompletionHandler<void(bool)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (bool success) mutable {
        completionHandler(success);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementSetCamera(*modelIdentifier, camera), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(std::nullopt);
        return;
    }

    CompletionHandler<void(Expected<bool, WebCore::ResourceError>)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (Expected<bool, WebCore::ResourceError> result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(*result);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementIsPlayingAnimation(*modelIdentifier), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::setAnimationIsPlaying(bool isPlaying, CompletionHandler<void(bool success)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(false);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(false);
        return;
    }

    CompletionHandler<void(bool)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (bool success) mutable {
        completionHandler(success);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementSetAnimationIsPlaying(*modelIdentifier, isPlaying), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(std::nullopt);
        return;
    }

    CompletionHandler<void(Expected<bool, WebCore::ResourceError>)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (Expected<bool, WebCore::ResourceError> result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(*result);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementIsLoopingAnimation(*modelIdentifier), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::setIsLoopingAnimation(bool isLooping, CompletionHandler<void(bool success)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(false);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(false);
        return;
    }

    CompletionHandler<void(bool)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (bool success) mutable {
        completionHandler(success);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementSetIsLoopingAnimation(*modelIdentifier, isLooping), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(std::nullopt);
        return;
    }

    CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (Expected<Seconds, WebCore::ResourceError> result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(*result);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementAnimationDuration(*modelIdentifier), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(std::nullopt);
        return;
    }

    CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (Expected<Seconds, WebCore::ResourceError> result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(*result);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementAnimationCurrentTime(*modelIdentifier), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::setAnimationCurrentTime(Seconds currentTime, CompletionHandler<void(bool success)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(false);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(false);
        return;
    }

    CompletionHandler<void(bool)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (bool success) mutable {
        completionHandler(success);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementSetAnimationCurrentTime(*modelIdentifier, currentTime), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(std::nullopt);
        return;
    }

    CompletionHandler<void(Expected<bool, WebCore::ResourceError>)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (Expected<bool, WebCore::ResourceError> result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(*result);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementHasAudio(*modelIdentifier), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(std::nullopt);
        return;
    }

    CompletionHandler<void(Expected<bool, WebCore::ResourceError>)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (Expected<bool, WebCore::ResourceError> result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(*result);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementIsMuted(*modelIdentifier), WTFMove(remoteCompletionHandler));
}

void ARKitInlinePreviewModelPlayer::setIsMuted(bool isMuted, CompletionHandler<void(bool success)>&& completionHandler)
{
    auto modelIdentifier = this->modelIdentifier();
    if (!modelIdentifier) {
        completionHandler(false);
        return;
    }

    RefPtr strongPage = m_page.get();
    if (!strongPage) {
        completionHandler(false);
        return;
    }

    CompletionHandler<void(bool)> remoteCompletionHandler = [completionHandler = WTFMove(completionHandler)] (bool success) mutable {
        completionHandler(success);
    };

    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementSetIsMuted(*modelIdentifier, isMuted), WTFMove(remoteCompletionHandler));
}

Vector<RetainPtr<id>> ARKitInlinePreviewModelPlayer::accessibilityChildren()
{
    // FIXME: https://webkit.org/b/233575 Need to return something to create a remote element connection to the InlinePreviewModel hosted in another process.
    return { };
}

}

#endif
