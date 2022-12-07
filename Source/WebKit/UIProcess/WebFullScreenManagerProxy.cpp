/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebFullScreenManagerProxy.h"

#if ENABLE(FULLSCREEN_API)

#include "APIFullscreenClient.h"
#include "WebAutomationSession.h"
#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/IntRect.h>
#include <WebCore/ScreenOrientationType.h>

namespace WebKit {
using namespace WebCore;

WebFullScreenManagerProxy::WebFullScreenManagerProxy(WebPageProxy& page, WebFullScreenManagerProxyClient& client)
    : m_page(page)
    , m_client(client)
{
    m_page.process().addMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page.webPageID(), *this);
}

WebFullScreenManagerProxy::~WebFullScreenManagerProxy()
{
    m_page.process().removeMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page.webPageID());
    m_client.closeFullScreenManager();
    callCloseCompletionHandlers();
}

void WebFullScreenManagerProxy::willEnterFullScreen()
{
    m_fullscreenState = FullscreenState::EnteringFullscreen;
    m_page.fullscreenClient().willEnterFullscreen(&m_page);
    m_page.send(Messages::WebFullScreenManager::WillEnterFullScreen());
}

void WebFullScreenManagerProxy::didEnterFullScreen()
{
    m_fullscreenState = FullscreenState::InFullscreen;
    m_page.fullscreenClient().didEnterFullscreen(&m_page);
    m_page.send(Messages::WebFullScreenManager::DidEnterFullScreen());

    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession())
            automationSession->didEnterFullScreenForPage(m_page);
    }
}

void WebFullScreenManagerProxy::willExitFullScreen()
{
    m_fullscreenState = FullscreenState::ExitingFullscreen;
    m_page.fullscreenClient().willExitFullscreen(&m_page);
    m_page.send(Messages::WebFullScreenManager::WillExitFullScreen());
}

void WebFullScreenManagerProxy::callCloseCompletionHandlers()
{
    auto closeMediaCallbacks = WTFMove(m_closeCompletionHandlers);
    for (auto& callback : closeMediaCallbacks)
        callback();
}

void WebFullScreenManagerProxy::closeWithCallback(CompletionHandler<void()>&& completionHandler)
{
    m_closeCompletionHandlers.append(WTFMove(completionHandler));
    close();
}

void WebFullScreenManagerProxy::didExitFullScreen()
{
    m_fullscreenState = FullscreenState::NotInFullscreen;
    m_page.fullscreenClient().didExitFullscreen(&m_page);
    m_page.send(Messages::WebFullScreenManager::DidExitFullScreen());
    
    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession())
            automationSession->didExitFullScreenForPage(m_page);
    }
    callCloseCompletionHandlers();
}

void WebFullScreenManagerProxy::setAnimatingFullScreen(bool animating)
{
    m_page.send(Messages::WebFullScreenManager::SetAnimatingFullScreen(animating));
}

void WebFullScreenManagerProxy::requestRestoreFullScreen()
{
    m_page.send(Messages::WebFullScreenManager::RequestRestoreFullScreen());
}

void WebFullScreenManagerProxy::requestExitFullScreen()
{
    m_page.send(Messages::WebFullScreenManager::RequestExitFullScreen());
}

void WebFullScreenManagerProxy::supportsFullScreen(bool withKeyboard, CompletionHandler<void(bool)>&& completionHandler)
{
#if PLATFORM(IOS_FAMILY)
    completionHandler(!withKeyboard);
#else
    completionHandler(true);
#endif
}

void WebFullScreenManagerProxy::saveScrollPosition()
{
    m_page.send(Messages::WebFullScreenManager::SaveScrollPosition());
}

void WebFullScreenManagerProxy::restoreScrollPosition()
{
    m_page.send(Messages::WebFullScreenManager::RestoreScrollPosition());
}

void WebFullScreenManagerProxy::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    m_page.send(Messages::WebFullScreenManager::SetFullscreenInsets(insets));
}

void WebFullScreenManagerProxy::setFullscreenAutoHideDuration(Seconds duration)
{
    m_page.send(Messages::WebFullScreenManager::SetFullscreenAutoHideDuration(duration));
}

void WebFullScreenManagerProxy::setFullscreenControlsHidden(bool hidden)
{
    m_page.send(Messages::WebFullScreenManager::SetFullscreenControlsHidden(hidden));
}

void WebFullScreenManagerProxy::close()
{
    m_client.closeFullScreenManager();
}

bool WebFullScreenManagerProxy::isFullScreen()
{
    return m_client.isFullScreen();
}

bool WebFullScreenManagerProxy::blocksReturnToFullscreenFromPictureInPicture() const
{
    return m_blocksReturnToFullscreenFromPictureInPicture;
}

#if HAVE(UIKIT_WEBKIT_INTERNALS)
bool WebFullScreenManagerProxy::isVideoElementWithControls() const
{
    return m_isVideoElementWithControls;
}
#endif

void WebFullScreenManagerProxy::enterFullScreen(bool blocksReturnToFullscreenFromPictureInPicture, bool isVideoElementWithControls, FloatSize videoDimensions)
{
    m_blocksReturnToFullscreenFromPictureInPicture = blocksReturnToFullscreenFromPictureInPicture;
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    m_isVideoElementWithControls = isVideoElementWithControls;
#else
    UNUSED_PARAM(isVideoElementWithControls);
#endif
#if PLATFORM(IOS_FAMILY)
    m_client.enterFullScreen(videoDimensions);
#else
    UNUSED_PARAM(videoDimensions);
    m_client.enterFullScreen();
#endif
}

void WebFullScreenManagerProxy::exitFullScreen()
{
    m_client.exitFullScreen();
}

void WebFullScreenManagerProxy::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    m_page.callAfterNextPresentationUpdate([weakThis = WeakPtr { *this }, initialFrame = initialFrame, finalFrame = finalFrame](CallbackBase::Error) {
        if (weakThis)
            weakThis->m_client.beganEnterFullScreen(initialFrame, finalFrame);
    });
}

void WebFullScreenManagerProxy::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    m_client.beganExitFullScreen(initialFrame, finalFrame);
}

bool WebFullScreenManagerProxy::lockFullscreenOrientation(WebCore::ScreenOrientationType orientation)
{
    return m_client.lockFullscreenOrientation(orientation);
}

void WebFullScreenManagerProxy::unlockFullscreenOrientation()
{
    m_client.unlockFullscreenOrientation();
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
