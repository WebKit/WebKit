/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "DrawingArea.h"

#include "DrawingAreaMessages.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebProcess.h"
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/TransformationMatrix.h>

// Subclasses
#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingArea.h"
#include "TiledCoreAnimationDrawingArea.h"
#elif PLATFORM(WPE)
#include "AcceleratedDrawingArea.h"
#else
#include "DrawingAreaImpl.h"
#endif

namespace WebKit {
using namespace WebCore;

std::unique_ptr<DrawingArea> DrawingArea::create(WebPage& webPage, const WebPageCreationParameters& parameters)
{
    switch (parameters.drawingAreaType) {
#if PLATFORM(COCOA)
#if !PLATFORM(IOS_FAMILY)
    case DrawingAreaTypeTiledCoreAnimation:
        return std::make_unique<TiledCoreAnimationDrawingArea>(webPage, parameters);
#endif
    case DrawingAreaTypeRemoteLayerTree:
        return std::make_unique<RemoteLayerTreeDrawingArea>(webPage, parameters);
#else
    case DrawingAreaTypeImpl:
#if PLATFORM(WPE)
        return std::make_unique<AcceleratedDrawingArea>(webPage, parameters);
#else
        return std::make_unique<DrawingAreaImpl>(webPage, parameters);
#endif
#endif
    }

    return nullptr;
}

DrawingArea::DrawingArea(DrawingAreaType type, WebPage& webPage)
    : m_type(type)
    , m_webPage(webPage)
{
    WebProcess::singleton().addMessageReceiver(Messages::DrawingArea::messageReceiverName(), m_webPage.pageID(), *this);
}

DrawingArea::~DrawingArea()
{
    removeMessageReceiverIfNeeded();
}

void DrawingArea::dispatchAfterEnsuringUpdatedScrollPosition(WTF::Function<void ()>&& function)
{
    // Scroll position updates are synchronous by default so we can just call the function right away here.
    function();
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && !(PLATFORM(MAC) && ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING))
RefPtr<WebCore::DisplayRefreshMonitor> DrawingArea::createDisplayRefreshMonitor(PlatformDisplayID)
{
    return nullptr;
}
#endif

void DrawingArea::removeMessageReceiverIfNeeded()
{
    if (m_hasRemovedMessageReceiver)
        return;
    m_hasRemovedMessageReceiver = true;
    WebProcess::singleton().removeMessageReceiver(Messages::DrawingArea::messageReceiverName(), m_webPage.pageID());
}

} // namespace WebKit
