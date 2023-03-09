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
#import "RemoteLayerTreeDrawingAreaMac.h"

#if PLATFORM(MAC)

#import "WebPage.h"
#import "WebPageCreationParameters.h"
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/RenderLayerBacking.h>

namespace WebKit {
using namespace WebCore;

RemoteLayerTreeDrawingAreaMac::RemoteLayerTreeDrawingAreaMac(WebPage& webPage, const WebPageCreationParameters& parameters)
    : RemoteLayerTreeDrawingArea(webPage, parameters)
{
    setColorSpace(parameters.colorSpace);
}

RemoteLayerTreeDrawingAreaMac::~RemoteLayerTreeDrawingAreaMac() = default;

DelegatedScrollingMode RemoteLayerTreeDrawingAreaMac::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToWebKit;
}

void RemoteLayerTreeDrawingAreaMac::setColorSpace(std::optional<WebCore::DestinationColorSpace> colorSpace)
{
    m_displayColorSpace = colorSpace;

    // We rely on the fact that the full style recalc that happens when moving a window between displays triggers repaints,
    // which causes PlatformCALayerRemote::updateBackingStore() to re-create backing stores with the new colorspace.
}

std::optional<WebCore::DestinationColorSpace> RemoteLayerTreeDrawingAreaMac::displayColorSpace() const
{
    return m_displayColorSpace;
}

void RemoteLayerTreeDrawingAreaMac::mainFrameContentSizeChanged(const WebCore::IntSize&)
{
    // Do nothing. This is only relevant to DelegatedToNativeScrollView implementations.
}

void RemoteLayerTreeDrawingAreaMac::applyTransientZoomToPage(double scale, FloatPoint origin)
{
    auto& frameView = *m_webPage.mainFrameView();

    auto unscrolledOrigin = origin;
    FloatRect unobscuredContentRect = frameView.unobscuredContentRectIncludingScrollbars();
    unscrolledOrigin.moveBy(-unobscuredContentRect.location());
    m_webPage.scalePage(scale / m_webPage.viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));
    updateRendering();
}

void RemoteLayerTreeDrawingAreaMac::adjustTransientZoom(double scale, WebCore::FloatPoint origin)
{
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaMac::adjustTransientZoom - scale " << scale << " origin " << origin);

    auto totalScale = scale * m_webPage.viewScaleFactor();

    // FIXME: Need to trigger some re-rendering here to render at the new scale, so tiles update while zooming.

    prepopulateRectForZoom(totalScale, origin);
}

void RemoteLayerTreeDrawingAreaMac::commitTransientZoom(double scale, WebCore::FloatPoint origin)
{
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaMac::commitTransientZoom - scale " << scale << " origin " << origin);

    scale *= m_webPage.viewScaleFactor();
    
    // FIXME: Constrain scale and origin
    applyTransientZoomToPage(scale, origin);
}

void RemoteLayerTreeDrawingAreaMac::willCommitLayerTree(RemoteLayerTreeTransaction& transaction)
{
    // FIXME: Probably need something here for PDF.
    auto* frameView = m_webPage.mainFrameView();
    if (!frameView)
        return;

    auto* renderViewGraphicsLayer = frameView->graphicsLayerForPageScale();
    if (!renderViewGraphicsLayer)
        return;

    transaction.setPageScalingLayerID(renderViewGraphicsLayer->primaryLayerID());
}

} // namespace WebKit

#endif // PLATFORM(MAC)
