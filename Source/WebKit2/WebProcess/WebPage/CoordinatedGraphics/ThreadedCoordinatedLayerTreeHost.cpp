/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Company 100, Inc.
 * Copyright (C) 2014 Igalia S.L.
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
#include "ThreadedCoordinatedLayerTreeHost.h"

#if USE(COORDINATED_GRAPHICS_THREADED)
#include "WebPage.h"
#include <WebCore/FrameView.h>
#include <WebCore/MainFrame.h>

using namespace WebCore;

namespace WebKit {

Ref<ThreadedCoordinatedLayerTreeHost> ThreadedCoordinatedLayerTreeHost::create(WebPage& webPage)
{
    return adoptRef(*new ThreadedCoordinatedLayerTreeHost(webPage));
}

ThreadedCoordinatedLayerTreeHost::~ThreadedCoordinatedLayerTreeHost()
{
}

ThreadedCoordinatedLayerTreeHost::ThreadedCoordinatedLayerTreeHost(WebPage& webPage)
    : CoordinatedLayerTreeHost(webPage)
    , m_compositorClient(*this)
    , m_compositor(ThreadedCompositor::create(&m_compositorClient))
{
}

void ThreadedCoordinatedLayerTreeHost::scrollNonCompositedContents(const WebCore::IntRect& rect)
{
    m_compositor->scrollTo(rect.location());
    scheduleLayerFlush();
}

void ThreadedCoordinatedLayerTreeHost::contentsSizeChanged(const WebCore::IntSize& newSize)
{
    m_compositor->didChangeContentsSize(newSize);
}

void ThreadedCoordinatedLayerTreeHost::deviceOrPageScaleFactorChanged()
{
    CoordinatedLayerTreeHost::deviceOrPageScaleFactorChanged();
    m_compositor->setDeviceScaleFactor(m_webPage.deviceScaleFactor());
}

void ThreadedCoordinatedLayerTreeHost::sizeDidChange(const IntSize& size)
{
    CoordinatedLayerTreeHost::sizeDidChange(size);
    m_compositor->didChangeViewportSize(size);
}

void ThreadedCoordinatedLayerTreeHost::didChangeViewportProperties(const WebCore::ViewportAttributes& attr)
{
    m_compositor->didChangeViewportAttribute(attr);
}

void ThreadedCoordinatedLayerTreeHost::didScaleFactorChanged(float scale, const IntPoint& origin)
{
    m_webPage.scalePage(scale, origin);
}

#if PLATFORM(GTK)
void ThreadedCoordinatedLayerTreeHost::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    m_layerTreeContext.contextID = handle;
    m_compositor->setNativeSurfaceHandleForCompositing(handle);
    scheduleLayerFlush();
}
#endif

void ThreadedCoordinatedLayerTreeHost::setVisibleContentsRect(const FloatRect& rect, const FloatPoint& trajectoryVector, float scale)
{
    CoordinatedLayerTreeHost::setVisibleContentsRect(rect, trajectoryVector);
    if (m_lastScrollPosition != roundedIntPoint(rect.location())) {
        m_lastScrollPosition = roundedIntPoint(rect.location());

        if (!m_webPage.corePage()->mainFrame().view()->useFixedLayout())
            m_webPage.corePage()->mainFrame().view()->notifyScrollPositionChanged(m_lastScrollPosition);
    }

    if (m_lastScaleFactor != scale) {
        m_lastScaleFactor = scale;
        didScaleFactorChanged(m_lastScaleFactor, m_lastScrollPosition);
    }
}

void ThreadedCoordinatedLayerTreeHost::commitSceneState(const CoordinatedGraphicsState& state)
{
    CoordinatedLayerTreeHost::commitSceneState(state);
    m_compositor->updateSceneState(state);
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
