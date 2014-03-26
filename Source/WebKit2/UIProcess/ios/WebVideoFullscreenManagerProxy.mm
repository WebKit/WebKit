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
#include "WebVideoFullscreenManagerProxy.h"

#if PLATFORM(IOS)

#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebVideoFullscreenManagerMessages.h"
#include "WebVideoFullscreenManagerProxyMessages.h"
#include <QuartzCore/CoreAnimation.h>
#include <WebKitSystemInterface.h>
#include <WebCore/TimeRanges.h>

using namespace WebCore;

namespace WebKit {
    
PassRefPtr<WebVideoFullscreenManagerProxy> WebVideoFullscreenManagerProxy::create(WebPageProxy& page)
{
    return adoptRef(new WebVideoFullscreenManagerProxy(page));
}

WebVideoFullscreenManagerProxy::WebVideoFullscreenManagerProxy(WebPageProxy& page)
    : m_page(&page)
{
    m_page->process().addMessageReceiver(Messages::WebVideoFullscreenManagerProxy::messageReceiverName(), m_page->pageID(), *this);
    setWebVideoFullscreenModel(this);
    setWebVideoFullscreenChangeObserver(this);
}

WebVideoFullscreenManagerProxy::~WebVideoFullscreenManagerProxy()
{
    m_page->process().removeMessageReceiver(Messages::WebVideoFullscreenManagerProxy::messageReceiverName(), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::enterFullscreenWithID(uint32_t videoLayerID)
{
    ASSERT(videoLayerID);
    m_layerHost = WKMakeRenderLayer(videoLayerID);
    enterFullscreen(*m_layerHost.get());
}
    
void WebVideoFullscreenManagerProxy::setSeekableRangesVector(Vector<std::pair<double, double>>& ranges)
{
    RefPtr<TimeRanges> timeRanges = TimeRanges::create();
    for (const auto& range : ranges)
    {
        ASSERT(isfinite(range.first));
        ASSERT(isfinite(range.second));
        ASSERT(range.second >= range.first);
        timeRanges->add(range.first, range.second);
    }

    setSeekableRanges(*timeRanges);
}

void WebVideoFullscreenManagerProxy::requestExitFullscreen()
{
    m_page->send(Messages::WebVideoFullscreenManager::RequestExitFullscreen(), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::didExitFullscreen()
{
    m_page->send(Messages::WebVideoFullscreenManager::DidExitFullscreen(), m_page->pageID());
    [m_layerHost removeFromSuperlayer];
    m_layerHost.clear();
}

void WebVideoFullscreenManagerProxy::didEnterFullscreen()
{
    m_page->send(Messages::WebVideoFullscreenManager::DidEnterFullscreen(), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::play()
{
    m_page->send(Messages::WebVideoFullscreenManager::Play(), m_page->pageID());
}
    
void WebVideoFullscreenManagerProxy::pause()
{
    m_page->send(Messages::WebVideoFullscreenManager::Pause(), m_page->pageID());
}
    
void WebVideoFullscreenManagerProxy::togglePlayState()
{
    m_page->send(Messages::WebVideoFullscreenManager::TogglePlayState(), m_page->pageID());
}
    
void WebVideoFullscreenManagerProxy::seekToTime(double time)
{
    m_page->send(Messages::WebVideoFullscreenManager::SeekToTime(time), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::setVideoLayerFrame(WebCore::FloatRect frame)
{
    m_page->send(Messages::WebVideoFullscreenManager::SetVideoLayerFrame(frame), m_page->pageID());
}

void WebVideoFullscreenManagerProxy::setVideoLayerGravity(WebCore::WebVideoFullscreenModel::VideoGravity gravity)
{
    m_page->send(Messages::WebVideoFullscreenManager::SetVideoLayerGravityEnum((unsigned)gravity), m_page->pageID());
}

} // namespace WebKit

#endif // PLATFORM(IOS)
