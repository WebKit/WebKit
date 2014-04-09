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
#include <QuartzCore/CoreAnimation.h>
#include <WebCore/Event.h>
#include <WebCore/EventNames.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/Settings.h>
#include <WebCore/TimeRanges.h>
#include <WebCore/WebCoreThreadRun.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebVideoFullscreenManager> WebVideoFullscreenManager::create(PassRefPtr<WebPage> page)
{
    return adoptRef(new WebVideoFullscreenManager(page));
}

WebVideoFullscreenManager::WebVideoFullscreenManager(PassRefPtr<WebPage> page)
    : m_page(page.get())
    , m_isAnimating(false)
    , m_targetIsFullscreen(false)
    , m_isFullscreen(false)
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
    return Settings::avKitEnabled() && isHTMLVideoElement(node);
}

void WebVideoFullscreenManager::enterFullscreenForNode(Node* node)
{
    ASSERT(node);
    m_node = node;
    m_targetIsFullscreen = true;

    if (m_isAnimating)
        return;

    m_isAnimating = true;
    setMediaElement(toHTMLMediaElement(node));

    PlatformLayer* videoLayer = [CALayer layer];
    m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
    m_layerHostingContext->setRootLayer(videoLayer);
    setVideoFullscreenLayer(videoLayer);

    m_page->send(Messages::WebVideoFullscreenManagerProxy::EnterFullscreenWithID(m_layerHostingContext->contextID()), m_page->pageID());
}

void WebVideoFullscreenManager::exitFullscreenForNode(Node*)
{
    m_node.clear();
    m_targetIsFullscreen = false;

    if (m_isAnimating)
        return;

    m_isAnimating = true;
    m_page->send(Messages::WebVideoFullscreenManagerProxy::ExitFullscreen(), m_page->pageID());
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
    
void WebVideoFullscreenManager::setSeekableRanges(const WebCore::TimeRanges& timeRanges)
{
    Vector<std::pair<double, double>> rangesVector;
    
    for (unsigned i = 0; i < timeRanges.length(); i++) {
        ExceptionCode exceptionCode;
        double start = timeRanges.start(i, exceptionCode);
        double end = timeRanges.end(i, exceptionCode);
        rangesVector.append(std::pair<double,double>(start, end));
    }

    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetSeekableRangesVector(std::move(rangesVector)), m_page->pageID());
}

void WebVideoFullscreenManager::setAudioMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetAudioMediaSelectionOptions(options, selectedIndex), m_page->pageID());
}

void WebVideoFullscreenManager::setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    m_page->send(Messages::WebVideoFullscreenManagerProxy::SetLegibleMediaSelectionOptions(options, selectedIndex), m_page->pageID());
}
    
void WebVideoFullscreenManager::didEnterFullscreen()
{
    m_isAnimating = false;
    m_isFullscreen = false;

    if (m_targetIsFullscreen)
        return;

    // exit fullscreen now if it was previously requested during an animation.
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^ {
        exitFullscreenForNode(m_node.get());
        protect.clear();
    });
}

void WebVideoFullscreenManager::didExitFullscreen()
{
    m_isAnimating = false;
    m_isFullscreen = false;

    m_layerHostingContext->setRootLayer(nullptr);
    m_layerHostingContext = nullptr;
    setVideoFullscreenLayer(nullptr);

    setMediaElement(nullptr);

    if (!m_targetIsFullscreen)
        return;

    // enter fullscreen now if it was previously requested during an animation.
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^ {
        enterFullscreenForNode(m_node.get());
        protect.clear();
    });
}
    
void WebVideoFullscreenManager::setVideoLayerGravityEnum(unsigned gravity)
{
    setVideoLayerGravity((WebVideoFullscreenModel::VideoGravity)gravity);
}

} // namespace WebKit

#endif // PLATFORM(IOS)
