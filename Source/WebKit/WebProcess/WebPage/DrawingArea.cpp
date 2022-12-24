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
#include <WebCore/ScrollView.h>
#include <WebCore/TransformationMatrix.h>

// Subclasses
#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingAreaMac.h"
#include "TiledCoreAnimationDrawingArea.h"
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
#include "DrawingAreaCoordinatedGraphics.h"
#endif
#if USE(GRAPHICS_LAYER_WC)
#include "DrawingAreaWC.h"
#endif

namespace WebKit {
using namespace WebCore;

std::unique_ptr<DrawingArea> DrawingArea::create(WebPage& webPage, const WebPageCreationParameters& parameters)
{
    switch (parameters.drawingAreaType) {
#if PLATFORM(COCOA)
#if !PLATFORM(IOS_FAMILY)
    case DrawingAreaType::TiledCoreAnimation:
        return makeUnique<TiledCoreAnimationDrawingArea>(webPage, parameters);
#endif
    case DrawingAreaType::RemoteLayerTree:
#if PLATFORM(MAC)
        return makeUnique<RemoteLayerTreeDrawingAreaMac>(webPage, parameters);
#else
        return makeUnique<RemoteLayerTreeDrawingArea>(webPage, parameters);
#endif
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    case DrawingAreaType::CoordinatedGraphics:
        return makeUnique<DrawingAreaCoordinatedGraphics>(webPage, parameters);
#endif
#if USE(GRAPHICS_LAYER_WC)
    case DrawingAreaType::WC:
        return makeUnique<DrawingAreaWC>(webPage, parameters);
#endif
    }

    return nullptr;
}

DrawingArea::DrawingArea(DrawingAreaType type, DrawingAreaIdentifier identifier, WebPage& webPage)
    : m_type(type)
    , m_identifier(identifier)
    , m_webPage(webPage)
{
    WebProcess::singleton().addMessageReceiver(Messages::DrawingArea::messageReceiverName(), m_identifier, *this);
}

DrawingArea::~DrawingArea()
{
    removeMessageReceiverIfNeeded();
}

DelegatedScrollingMode DrawingArea::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::NotDelegated;
}

void DrawingArea::dispatchAfterEnsuringUpdatedScrollPosition(WTF::Function<void ()>&& function)
{
    // Scroll position updates are synchronous by default so we can just call the function right away here.
    function();
}

void DrawingArea::tryMarkLayersVolatile(CompletionHandler<void(bool)>&& completionFunction)
{
    completionFunction(true);
}

void DrawingArea::removeMessageReceiverIfNeeded()
{
    if (m_hasRemovedMessageReceiver)
        return;
    m_hasRemovedMessageReceiver = true;
    WebProcess::singleton().removeMessageReceiver(Messages::DrawingArea::messageReceiverName(), m_identifier);
}

RefPtr<WebCore::DisplayRefreshMonitor> DrawingArea::createDisplayRefreshMonitor(WebCore::PlatformDisplayID)
{
    return nullptr;
}

void DrawingArea::willStartRenderingUpdateDisplay()
{
    m_webPage.willStartRenderingUpdateDisplay();
}

void DrawingArea::didCompleteRenderingUpdateDisplay()
{
    m_webPage.didCompleteRenderingUpdateDisplay();
}

void DrawingArea::didCompleteRenderingFrame()
{
    m_webPage.didCompleteRenderingFrame();
}

bool DrawingArea::supportsGPUProcessRendering(DrawingAreaType type)
{
    switch (type) {
#if PLATFORM(COCOA)
#if !PLATFORM(IOS_FAMILY)
    case DrawingAreaType::TiledCoreAnimation:
        return false;
#endif
    case DrawingAreaType::RemoteLayerTree:
        return true;
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    case DrawingAreaType::CoordinatedGraphics:
        return false;
#endif
#if USE(GRAPHICS_LAYER_WC)
    case DrawingAreaType::WC:
        return true;
#endif
    default:
        return false;
    }
}

} // namespace WebKit
