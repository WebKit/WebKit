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
#include <WebCore/FrameView.h>
#include <WebCore/RenderView.h>
#include <WebCore/ScrollView.h>
#include <WebCore/TiledBacking.h>
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

WebCore::TiledBacking* DrawingArea::mainFrameTiledBacking() const
{
    auto* frameView = m_webPage.mainFrameView();
    return frameView ? frameView->tiledBacking() : nullptr;
}

void DrawingArea::prepopulateRectForZoom(double scale, WebCore::FloatPoint origin)
{
    double currentPageScale = m_webPage.totalScaleFactor();
    auto* frameView = m_webPage.mainFrameView();
    FloatRect tileCoverageRect = frameView->visibleContentRectIncludingScrollbars();
    tileCoverageRect.moveBy(-origin);
    tileCoverageRect.scale(currentPageScale / scale);

    if (auto* tiledBacking = mainFrameTiledBacking())
        tiledBacking->prepopulateRect(tileCoverageRect);
}

void DrawingArea::scaleViewToFitDocumentIfNeeded()
{
    const int maximumDocumentWidthForScaling = 1440;
    const float minimumViewScale = 0.1;

    if (!m_shouldScaleViewToFitDocument)
        return;

    LOG(Resize, "DrawingArea %p scaleViewToFitDocumentIfNeeded", this);
    m_webPage.layoutIfNeeded();

    if (!m_webPage.mainFrameView() || !m_webPage.mainFrameView()->renderView())
        return;

    int viewWidth = m_webPage.size().width();
    int documentWidth = m_webPage.mainFrameView()->renderView()->unscaledDocumentRect().width();

    bool documentWidthChanged = m_lastDocumentSizeForScaleToFit.width() != documentWidth;
    bool viewWidthChanged = m_lastViewSizeForScaleToFit.width() != viewWidth;

    LOG(Resize, "  documentWidthChanged=%d, viewWidthChanged=%d", documentWidthChanged, viewWidthChanged);

    if (!documentWidthChanged && !viewWidthChanged)
        return;

    // The view is now bigger than the document, so we'll re-evaluate whether we have to scale.
    if (m_isScalingViewToFitDocument && viewWidth >= m_lastDocumentSizeForScaleToFit.width())
        m_isScalingViewToFitDocument = false;

    // Our current understanding of the document width is still up to date, and we're in scaling mode.
    // Update the viewScale without doing an extra layout to re-determine the document width.
    if (m_isScalingViewToFitDocument) {
        if (!documentWidthChanged) {
            m_lastViewSizeForScaleToFit = m_webPage.size();
            float viewScale = (float)viewWidth / (float)m_lastDocumentSizeForScaleToFit.width();
            if (viewScale < minimumViewScale) {
                viewScale = minimumViewScale;
                documentWidth = std::ceil(viewWidth / viewScale);
            }
            IntSize fixedLayoutSize(documentWidth, std::ceil((m_webPage.size().height() - m_webPage.corePage()->topContentInset()) / viewScale));
            m_webPage.setFixedLayoutSize(fixedLayoutSize);
            m_webPage.scaleView(viewScale);

            LOG(Resize, "  using fixed layout at %dx%d. document width %d unchanged, scaled to %.4f to fit view width %d", fixedLayoutSize.width(), fixedLayoutSize.height(), documentWidth, viewScale, viewWidth);
            return;
        }
    
        IntSize fixedLayoutSize = m_webPage.fixedLayoutSize();
        if (documentWidth > fixedLayoutSize.width()) {
            LOG(Resize, "  page laid out wider than fixed layout width. Not attempting to re-scale");
            return;
        }
    }

    LOG(Resize, "  doing unconstrained layout");

    // Lay out at the view size.
    m_webPage.setUseFixedLayout(false);
    m_webPage.layoutIfNeeded();

    if (!m_webPage.mainFrameView() || !m_webPage.mainFrameView()->renderView())
        return;

    IntSize documentSize = m_webPage.mainFrameView()->renderView()->unscaledDocumentRect().size();
    m_lastViewSizeForScaleToFit = m_webPage.size();
    m_lastDocumentSizeForScaleToFit = documentSize;

    documentWidth = documentSize.width();

    float viewScale = 1;

    LOG(Resize, "  unscaled document size %dx%d. need to scale down: %d", documentSize.width(), documentSize.height(), documentWidth && documentWidth < maximumDocumentWidthForScaling && viewWidth < documentWidth);

    // Avoid scaling down documents that don't fit in a certain width, to allow
    // sites that want horizontal scrollbars to continue to have them.
    if (documentWidth && documentWidth < maximumDocumentWidthForScaling && viewWidth < documentWidth) {
        // If the document doesn't fit in the view, scale it down but lay out at the view size.
        m_isScalingViewToFitDocument = true;
        m_webPage.setUseFixedLayout(true);
        viewScale = (float)viewWidth / (float)documentWidth;
        if (viewScale < minimumViewScale) {
            viewScale = minimumViewScale;
            documentWidth = std::ceil(viewWidth / viewScale);
        }
        IntSize fixedLayoutSize(documentWidth, std::ceil((m_webPage.size().height() - m_webPage.corePage()->topContentInset()) / viewScale));
        m_webPage.setFixedLayoutSize(fixedLayoutSize);

        LOG(Resize, "  using fixed layout at %dx%d. document width %d, scaled to %.4f to fit view width %d", fixedLayoutSize.width(), fixedLayoutSize.height(), documentWidth, viewScale, viewWidth);
    }

    m_webPage.scaleView(viewScale);
}

void DrawingArea::setShouldScaleViewToFitDocument(bool shouldScaleView)
{
    if (m_shouldScaleViewToFitDocument == shouldScaleView)
        return;

    m_shouldScaleViewToFitDocument = shouldScaleView;
    triggerRenderingUpdate();
}

} // namespace WebKit
