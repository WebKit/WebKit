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
#include "WebFullScreenManager.h"

#if ENABLE(FULLSCREEN_API)

#include "Connection.h"
#include "Logging.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include <WebCore/Color.h>
#include <WebCore/Element.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/Page.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>
#include <WebCore/TypedElementDescendantIterator.h>

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManager.h"
#endif

namespace WebKit {
using namespace WebCore;

static IntRect screenRectOfContents(Element* element)
{
    ASSERT(element);
    if (element->renderer() && element->renderer()->hasLayer() && element->renderer()->enclosingLayer()->isComposited()) {
        FloatQuad contentsBox = static_cast<FloatRect>(element->renderer()->enclosingLayer()->backing()->contentsBox());
        contentsBox = element->renderer()->localToAbsoluteQuad(contentsBox);
        return element->renderer()->view().frameView().contentsToScreen(contentsBox.enclosingBoundingBox());
    }

    return element->screenRect();
}

Ref<WebFullScreenManager> WebFullScreenManager::create(WebPage* page)
{
    return adoptRef(*new WebFullScreenManager(page));
}

WebFullScreenManager::WebFullScreenManager(WebPage* page)
    : m_page(page)
{
}
    
WebFullScreenManager::~WebFullScreenManager()
{
}

WebCore::Element* WebFullScreenManager::element() 
{ 
    return m_element.get(); 
}

void WebFullScreenManager::videoControlsManagerDidChange()
{
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    LOG(Fullscreen, "WebFullScreenManager %p videoControlsManagerDidChange()", this);

    auto* currentPlaybackControlsElement = m_page->playbackSessionManager().currentPlaybackControlsElement();
    if (!m_element || !is<HTMLVideoElement>(currentPlaybackControlsElement)) {
        setPIPStandbyElement(nullptr);
        return;
    }

    setPIPStandbyElement(downcast<HTMLVideoElement>(currentPlaybackControlsElement));
#endif
}

void WebFullScreenManager::setPIPStandbyElement(WebCore::HTMLVideoElement* pipStandbyElement)
{
#if ENABLE(VIDEO)
    if (pipStandbyElement == m_pipStandbyElement)
        return;

    LOG(Fullscreen, "WebFullScreenManager %p setPIPStandbyElement() - old element %p, new element %p", this, m_pipStandbyElement.get(), pipStandbyElement);

    if (m_pipStandbyElement)
        m_pipStandbyElement->setVideoFullscreenStandby(false);

    m_pipStandbyElement = pipStandbyElement;

    if (m_pipStandbyElement)
        m_pipStandbyElement->setVideoFullscreenStandby(true);
#endif
}

void WebFullScreenManager::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    didReceiveWebFullScreenManagerMessage(connection, decoder);
}

bool WebFullScreenManager::supportsFullScreen(bool withKeyboard)
{
    if (!m_page->corePage()->settings().fullScreenEnabled())
        return false;

    return m_page->injectedBundleFullScreenClient().supportsFullScreen(m_page.get(), withKeyboard);
}

void WebFullScreenManager::enterFullScreenForElement(WebCore::Element* element)
{
    LOG(Fullscreen, "WebFullScreenManager %p enterFullScreenForElement(%p)", this, element);

    ASSERT(element);
    m_element = element;
    m_initialFrame = screenRectOfContents(m_element.get());
    m_page->injectedBundleFullScreenClient().enterFullScreenForElement(m_page.get(), element);
}

void WebFullScreenManager::exitFullScreenForElement(WebCore::Element* element)
{
    LOG(Fullscreen, "WebFullScreenManager %p exitFullScreenForElement(%p) - fullscreen element %p", this, element, m_element.get());
    m_page->injectedBundleFullScreenClient().exitFullScreenForElement(m_page.get(), element);
}

void WebFullScreenManager::willEnterFullScreen()
{
    LOG(Fullscreen, "WebFullScreenManager %p willEnterFullScreen() - element %p", this, m_element.get());

    m_element->document().webkitWillEnterFullScreen(*m_element);
#if !PLATFORM(IOS_FAMILY)
    m_page->hidePageBanners();
#endif
    m_element->document().updateLayout();
    m_page->forceRepaintWithoutCallback();
    m_finalFrame = screenRectOfContents(m_element.get());
    m_page->injectedBundleFullScreenClient().beganEnterFullScreen(m_page.get(), m_initialFrame, m_finalFrame);
}

void WebFullScreenManager::didEnterFullScreen()
{
    LOG(Fullscreen, "WebFullScreenManager %p didEnterFullScreen() - element %p", this, m_element.get());

    m_element->document().webkitDidEnterFullScreen();

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    auto* currentPlaybackControlsElement = m_page->playbackSessionManager().currentPlaybackControlsElement();
    setPIPStandbyElement(is<HTMLVideoElement>(currentPlaybackControlsElement) ? downcast<HTMLVideoElement>(currentPlaybackControlsElement) : nullptr);
#endif
}

void WebFullScreenManager::willExitFullScreen()
{
    LOG(Fullscreen, "WebFullScreenManager %p willExitFullScreen() - element %p", this, m_element.get());
    ASSERT(m_element);

#if ENABLE(VIDEO)
    setPIPStandbyElement(nullptr);
#endif

    m_finalFrame = screenRectOfContents(m_element.get());
    m_element->document().webkitWillExitFullScreen();
#if !PLATFORM(IOS_FAMILY)
    m_page->showPageBanners();
#endif
    m_page->injectedBundleFullScreenClient().beganExitFullScreen(m_page.get(), m_finalFrame, m_initialFrame);
}

void WebFullScreenManager::didExitFullScreen()
{
    LOG(Fullscreen, "WebFullScreenManager %p didExitFullScreen() - element %p", this, m_element.get());

    ASSERT(m_element);
    setFullscreenInsets(FloatBoxExtent());
    setFullscreenAutoHideDuration(0_s);
    m_element->document().webkitDidExitFullScreen();
}

void WebFullScreenManager::setAnimatingFullScreen(bool animating)
{
    ASSERT(m_element);
    m_element->document().setAnimatingFullScreen(animating);
}

void WebFullScreenManager::requestExitFullScreen()
{
    ASSERT(m_element);
    m_element->document().webkitCancelFullScreen();
}

void WebFullScreenManager::close()
{
    LOG(Fullscreen, "WebFullScreenManager %p close()", this);
    m_page->injectedBundleFullScreenClient().closeFullScreen(m_page.get());
}

void WebFullScreenManager::saveScrollPosition()
{
    m_scrollPosition = m_page->corePage()->mainFrame().view()->scrollPosition();
}

void WebFullScreenManager::restoreScrollPosition()
{
    m_page->corePage()->mainFrame().view()->setScrollPosition(m_scrollPosition);
}

void WebFullScreenManager::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    m_page->corePage()->setFullscreenInsets(insets);
}

void WebFullScreenManager::setFullscreenAutoHideDuration(Seconds duration)
{
    m_page->corePage()->setFullscreenAutoHideDuration(duration);
}

void WebFullScreenManager::setFullscreenControlsHidden(bool hidden)
{
    m_page->corePage()->setFullscreenControlsHidden(hidden);
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
