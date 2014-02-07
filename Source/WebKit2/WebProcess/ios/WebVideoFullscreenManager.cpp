/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebVideoFullscreenManager.h"

#if PLATFORM(IOS)

#include "WebPage.h"
#include "WebProcess.h"
#include "WebVideoFullscreenManagerMessages.h"
#include "WebVideoFullscreenManagerProxyMessages.h"
#include <WebCore/Event.h>
#include <WebCore/EventNames.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {
    
PassRefPtr<WebVideoFullscreenManager> WebVideoFullscreenManager::create(PassRefPtr<WebPage> page)
{
    return adoptRef(new WebVideoFullscreenManager(page));
}

WebVideoFullscreenManager::WebVideoFullscreenManager(PassRefPtr<WebPage> page)
    : m_page(page.get())
{
    setWebVideoFullscreenInterface(this);
    WebProcess::shared().addMessageReceiver(Messages::WebVideoFullscreenManager::messageReceiverName(), page->pageID(), *this);
}

WebVideoFullscreenManager::~WebVideoFullscreenManager()
{
    WebProcess::shared().removeMessageReceiver(Messages::WebVideoFullscreenManager::messageReceiverName(), m_page->pageID());
}

bool WebVideoFullscreenManager::supportsFullscreen(const Node* node) const
{
    if (!Settings::avKitEnabled())
        return false;
    return isHTMLVideoElement(node);
}

void WebVideoFullscreenManager::enterFullscreenForNode(Node* node)
{
    ASSERT(node);
    m_node = node;
    setMediaElement(toHTMLMediaElement(node));
    enterFullscreen();
}

void WebVideoFullscreenManager::exitFullscreenForNode(Node*)
{
    exitFullscreen();
    setMediaElement(nullptr);
}

void WebVideoFullscreenManager::setDuration(double duration)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetDuration(duration), m_page->pageID());
}
    
void WebVideoFullscreenManager::setCurrentTime(double currentTime, double anchorTime)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetCurrentTime(currentTime, anchorTime), m_page->pageID());
}
    
void WebVideoFullscreenManager::setRate(bool isPlaying, float playbackRate)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetRate(isPlaying, playbackRate), m_page->pageID());
}

void WebVideoFullscreenManager::setVideoDimensions(bool hasVideo, float width, float height)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetVideoDimensions(hasVideo, width, height), m_page->pageID());
}
    
void WebVideoFullscreenManager::setVideoLayer(PlatformLayer*)
{
    // TODO: implement with correct layer ID.
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetVideoLayerID(0), m_page->pageID());
}
    
void WebVideoFullscreenManager::setVideoLayerID(uint32_t videoLayerID)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetVideoLayerID(videoLayerID), m_page->pageID());
}

void WebVideoFullscreenManager::enterFullscreen()
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::EnterFullscreen(), m_page->pageID());
}
    
void WebVideoFullscreenManager::exitFullscreen()
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::ExitFullscreen(), m_page->pageID());
}

} // namespace WebKit

#endif // PLATFORM(IOS)
