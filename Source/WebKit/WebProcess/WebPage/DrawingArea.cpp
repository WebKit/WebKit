/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebProcess.h"
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/RenderView.h>
#include <WebCore/ScrollView.h>
#include <WebCore/TiledBacking.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/TZoneMallocInlines.h>

// Subclasses
#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingAreaMac.h"
#include "TiledCoreAnimationDrawingArea.h"
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
#if PLATFORM(GTK) || PLATFORM(WPE)
#include "DrawingAreaCoordinatedGraphicsGLib.h"
#else
#include "DrawingAreaCoordinatedGraphics.h"
#endif
#endif
#if USE(GRAPHICS_LAYER_WC)
#include "DrawingAreaWC.h"
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(DrawingArea);

RefPtr<DrawingArea> DrawingArea::create(WebPage& webPage, const WebPageCreationParameters& parameters)
{
#if ENABLE(TILED_CA_DRAWING_AREA)
    SandboxExtension::consumePermanently(parameters.renderServerMachExtensionHandle);
    switch (parameters.drawingAreaType) {
    case DrawingAreaType::TiledCoreAnimation:
        return TiledCoreAnimationDrawingArea::create(webPage, parameters);
    case DrawingAreaType::RemoteLayerTree:
        return RemoteLayerTreeDrawingAreaMac::create(webPage, parameters);
    }
    RELEASE_ASSERT_NOT_REACHED();
#elif PLATFORM(MAC)
    return RemoteLayerTreeDrawingAreaMac::create(webPage, parameters);
#elif PLATFORM(IOS_FAMILY)
    return RemoteLayerTreeDrawingArea::create(webPage, parameters);
#elif USE(GRAPHICS_LAYER_WC)
    return DrawingAreaWC::create(webPage, parameters);
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    return DrawingAreaCoordinatedGraphics::create(webPage, parameters);
#endif
}

DrawingArea::DrawingArea(DrawingAreaIdentifier identifier, WebPage& webPage)
    : m_identifier(identifier)
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
    Ref { m_webPage.get() }->willStartRenderingUpdateDisplay();
}

void DrawingArea::didCompleteRenderingUpdateDisplay()
{
    Ref { m_webPage.get() }->didCompleteRenderingUpdateDisplay();
}

void DrawingArea::didCompleteRenderingFrame()
{
    Ref { m_webPage.get() }->didCompleteRenderingFrame();
}

#if ENABLE(TILED_CA_DRAWING_AREA)
bool DrawingArea::supportsGPUProcessRendering(DrawingAreaType type)
{
    switch (type) {
    case DrawingAreaType::TiledCoreAnimation:
        return false;
    case DrawingAreaType::RemoteLayerTree:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#else // ENABLE(TILED_CA_DRAWING_AREA)

bool DrawingArea::supportsGPUProcessRendering()
{
#if PLATFORM(COCOA)
    return true;
#elif USE(GRAPHICS_LAYER_WC)
    return true;
#elif USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    return false;
#endif
}

#endif // ENABLE(TILED_CA_DRAWING_AREA)

WebCore::TiledBacking* DrawingArea::mainFrameTiledBacking() const
{
    RefPtr frameView = protectedWebPage()->localMainFrameView();
    return frameView ? frameView->tiledBacking() : nullptr;
}

void DrawingArea::prepopulateRectForZoom(double scale, WebCore::FloatPoint origin)
{
    Ref webPage = m_webPage.get();
    double currentPageScale = webPage->totalScaleFactor();
    RefPtr frameView = webPage->localMainFrameView();
    if (!frameView)
        return;

    FloatRect tileCoverageRect = frameView->visibleContentRectIncludingScrollbars();
    tileCoverageRect.moveBy(-origin);
    tileCoverageRect.scale(currentPageScale / scale);

    if (CheckedPtr tiledBacking = mainFrameTiledBacking())
        tiledBacking->prepopulateRect(tileCoverageRect);
}

void DrawingArea::scaleViewToFitDocumentIfNeeded()
{
    const int maximumDocumentWidthForScaling = 1440;
    const float minimumViewScale = 0.1;

    if (!m_shouldScaleViewToFitDocument)
        return;

    LOG(Resize, "DrawingArea %p scaleViewToFitDocumentIfNeeded", this);
    Ref webPage = m_webPage.get();
    webPage->layoutIfNeeded();

    RefPtr frameView = webPage->localMainFrameView();
    if (!frameView)
        return;

    CheckedPtr renderView = frameView->renderView();
    if (!renderView)
        return;

    int viewWidth = webPage->size().width();
    int documentWidth = renderView->unscaledDocumentRect().width();

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
            m_lastViewSizeForScaleToFit = webPage->size();
            float viewScale = (float)viewWidth / (float)m_lastDocumentSizeForScaleToFit.width();
            if (viewScale < minimumViewScale) {
                viewScale = minimumViewScale;
                documentWidth = std::ceil(viewWidth / viewScale);
            }
            // FIXME: Account for left content insets.
            IntSize fixedLayoutSize(documentWidth, std::ceil((webPage->size().height() - webPage->corePage()->obscuredContentInsets().top()) / viewScale));
            webPage->setFixedLayoutSize(fixedLayoutSize);
            webPage->scaleView(viewScale);

            LOG(Resize, "  using fixed layout at %dx%d. document width %d unchanged, scaled to %.4f to fit view width %d", fixedLayoutSize.width(), fixedLayoutSize.height(), documentWidth, viewScale, viewWidth);
            return;
        }
    
        IntSize fixedLayoutSize = webPage->fixedLayoutSize();
        if (documentWidth > fixedLayoutSize.width()) {
            LOG(Resize, "  page laid out wider than fixed layout width. Not attempting to re-scale");
            return;
        }
    }

    LOG(Resize, "  doing unconstrained layout");

    // Lay out at the view size.
    webPage->setUseFixedLayout(false);
    webPage->layoutIfNeeded();

    frameView = webPage->localMainFrameView();
    if (!frameView)
        return;

    renderView = frameView->renderView();
    if (!renderView)
        return;

    auto documentSize = renderView->unscaledDocumentRect().size();
    m_lastViewSizeForScaleToFit = webPage->size();
    m_lastDocumentSizeForScaleToFit = documentSize;

    documentWidth = documentSize.width();

    float viewScale = 1;

    LOG(Resize, "  unscaled document size %dx%d. need to scale down: %d", documentSize.width(), documentSize.height(), documentWidth && documentWidth < maximumDocumentWidthForScaling && viewWidth < documentWidth);

    // Avoid scaling down documents that don't fit in a certain width, to allow
    // sites that want horizontal scrollbars to continue to have them.
    if (documentWidth && documentWidth < maximumDocumentWidthForScaling && viewWidth < documentWidth) {
        // If the document doesn't fit in the view, scale it down but lay out at the view size.
        m_isScalingViewToFitDocument = true;
        webPage->setUseFixedLayout(true);
        viewScale = (float)viewWidth / (float)documentWidth;
        if (viewScale < minimumViewScale) {
            viewScale = minimumViewScale;
            documentWidth = std::ceil(viewWidth / viewScale);
        }
        // FIXME: Account for left content insets.
        IntSize fixedLayoutSize(documentWidth, std::ceil((webPage->size().height() - webPage->corePage()->obscuredContentInsets().top()) / viewScale));
        webPage->setFixedLayoutSize(fixedLayoutSize);

        LOG(Resize, "  using fixed layout at %dx%d. document width %d, scaled to %.4f to fit view width %d", fixedLayoutSize.width(), fixedLayoutSize.height(), documentWidth, viewScale, viewWidth);
    }

    webPage->scaleView(viewScale);
}

void DrawingArea::setShouldScaleViewToFitDocument(bool shouldScaleView)
{
    if (m_shouldScaleViewToFitDocument == shouldScaleView)
        return;

    m_shouldScaleViewToFitDocument = shouldScaleView;
    triggerRenderingUpdate();
}

} // namespace WebKit
