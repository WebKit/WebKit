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

#import "config.h"
#import "ServicesOverlayController.h"

#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/Document.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/MainFrame.h>
#import <WebCore/SoftLinking.h>

#if __has_include(<DataDetectors/DDHighlightDrawing.h>)
#import <DataDetectors/DDHighlightDrawing.h>
#else
typedef void* DDHighlightRef;
#endif

#if __has_include(<DataDetectors/DDHighlightDrawing_Private.h>)
#import <DataDetectors/DDHighlightDrawing_Private.h>
#endif

typedef NSUInteger DDHighlightStyle;
static const DDHighlightStyle DDHighlightNoOutlineWithArrow = (1 << 16);

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectors)
SOFT_LINK(DataDetectors, DDHighlightCreateWithRectsInVisibleRect, DDHighlightRef, (CFAllocatorRef allocator, CGRect * rects, CFIndex count, CGRect globalVisibleRect, Boolean withArrow), (allocator, rects, count, globalVisibleRect, withArrow))
SOFT_LINK(DataDetectors, DDHighlightCreateWithRectsInVisibleRectWithStyle, DDHighlightRef, (CFAllocatorRef allocator, CGRect * rects, CFIndex count, CGRect globalVisibleRect, DDHighlightStyle style, Boolean withArrow), (allocator, rects, count, globalVisibleRect, style, withArrow))
SOFT_LINK(DataDetectors, DDHighlightGetLayerWithContext, CGLayerRef, (DDHighlightRef highlight, CGContextRef context), (highlight, context))
SOFT_LINK(DataDetectors, DDHighlightGetBoundingRect, CGRect, (DDHighlightRef highlight), (highlight))
SOFT_LINK(DataDetectors, DDHighlightPointIsOnHighlight, Boolean, (DDHighlightRef highlight, CGPoint point, Boolean* onButton), (highlight, point, onButton))

using namespace WebCore;

namespace WebKit {

static IntRect textQuadsToBoundingRectForRange(Range& range)
{
    Vector<FloatQuad> textQuads;
    range.textQuads(textQuads);
    FloatRect boundingRect;
    for (auto& quad : textQuads)
        boundingRect.unite(quad.boundingBox());
    return enclosingIntRect(boundingRect);
}

ServicesOverlayController::ServicesOverlayController(WebPage& webPage)
    : m_webPage(&webPage)
    , m_servicesOverlay(nullptr)
    , m_mouseIsDownOnButton(false)
    , m_mouseIsOverHighlight(false)
    , m_drawingTelephoneNumberHighlight(false)
    , m_currentHighlightIsDirty(false)
{
}

ServicesOverlayController::~ServicesOverlayController()
{
    if (!m_servicesOverlay) {
        ASSERT(m_webPage);
        m_webPage->uninstallPageOverlay(m_servicesOverlay, PageOverlay::FadeMode::DoNotFade);
    }
}

void ServicesOverlayController::pageOverlayDestroyed(PageOverlay*)
{
    // Before the overlay is destroyed, it should have moved out of the WebPage,
    // at which point we already cleared our back pointer.
    ASSERT(!m_servicesOverlay);
}

void ServicesOverlayController::willMoveToWebPage(PageOverlay*, WebPage* webPage)
{
    if (webPage)
        return;

    ASSERT(m_servicesOverlay);
    m_servicesOverlay = nullptr;

    ASSERT(m_webPage);
    m_webPage = nullptr;
}

void ServicesOverlayController::didMoveToWebPage(PageOverlay*, WebPage*)
{
}

void ServicesOverlayController::createOverlayIfNeeded()
{
    if (m_servicesOverlay) {
        m_servicesOverlay->setNeedsDisplay();
        return;
    }

    if (m_currentTelephoneNumberRanges.isEmpty() && (!WebProcess::shared().hasSelectionServices() || m_currentSelectionRects.isEmpty()))
        return;

    RefPtr<PageOverlay> overlay = PageOverlay::create(this, PageOverlay::OverlayType::Document);
    m_servicesOverlay = overlay.get();
    m_webPage->installPageOverlay(overlay.release(), PageOverlay::FadeMode::Fade);
    m_servicesOverlay->setNeedsDisplay();
}

void ServicesOverlayController::selectionRectsDidChange(const Vector<LayoutRect>& rects)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    m_currentHighlightIsDirty = true;
    m_currentSelectionRects = rects;

    createOverlayIfNeeded();
#else
    UNUSED_PARAM(rects);
#endif
}

void ServicesOverlayController::selectedTelephoneNumberRangesChanged(const Vector<RefPtr<Range>>& ranges)
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    m_currentHighlightIsDirty = true;
    m_currentTelephoneNumberRanges = ranges;
    m_drawingTelephoneNumberHighlight = false;

    if (ranges.size() == 1) {
        RefPtr<Range> selectionRange = m_webPage->corePage()->mainFrame().selection().toNormalizedRange();
        if (ranges[0]->contains(*selectionRange))
            m_drawingTelephoneNumberHighlight = true;
    }
    
    createOverlayIfNeeded();
#else
    UNUSED_PARAM(ranges);
#endif
}

void ServicesOverlayController::clearHighlightState()
{
    m_mouseIsDownOnButton = false;
    m_mouseIsOverHighlight = false;
    m_drawingTelephoneNumberHighlight = false;

    m_currentHighlight = nullptr;
}

void ServicesOverlayController::drawRect(PageOverlay* overlay, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    if (m_currentSelectionRects.isEmpty() && m_currentTelephoneNumberRanges.isEmpty()) {
        clearHighlightState();
        return;
    }

    if (m_drawingTelephoneNumberHighlight)
        drawTelephoneNumberHighlight(graphicsContext, dirtyRect);
    else
        drawSelectionHighlight(graphicsContext, dirtyRect);
}

void ServicesOverlayController::drawSelectionHighlight(WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    ASSERT(!m_drawingTelephoneNumberHighlight);
    ASSERT(m_currentSelectionRects.size());

    // If there are no installed selection services and we have no phone numbers detected, then we have nothing to draw.
    if (!WebProcess::shared().hasSelectionServices() && m_currentTelephoneNumberRanges.isEmpty())
        return;

    if (!m_currentHighlight || m_currentHighlightIsDirty) {
        Vector<CGRect> cgRects;
        cgRects.reserveCapacity(m_currentSelectionRects.size());

        for (auto& rect : m_currentSelectionRects) {
            IntRect selectionRect(rect.pixelSnappedLocation(), rect.pixelSnappedSize());

            if (!selectionRect.intersects(dirtyRect))
                continue;

            cgRects.append((CGRect)pixelSnappedIntRect(rect));
        }

        if (!cgRects.isEmpty()) {
            CGRect bounds = m_webPage->corePage()->mainFrame().view()->boundsRect();
            m_currentHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyle(nullptr, cgRects.begin(), cgRects.size(), bounds, DDHighlightNoOutlineWithArrow, true));
            m_currentHighlightIsDirty = false;
        }
    }

    drawCurrentHighlight(graphicsContext);
}

void ServicesOverlayController::drawTelephoneNumberHighlight(WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    ASSERT(m_drawingTelephoneNumberHighlight);
    ASSERT(m_currentTelephoneNumberRanges.size() == 1);

    auto& range = m_currentTelephoneNumberRanges[0];

    // FIXME: This will choke if the range wraps around the edge of the view.
    // What should we do in that case?
    IntRect rect = textQuadsToBoundingRectForRange(*range);

    // Convert to the main document's coordinate space.
    // FIXME: It's a little crazy to call contentsToWindow and then windowToContents in order to get the right coordinate space.
    // We should consider adding conversion functions to ScrollView for contentsToDocument(). Right now, contentsToRootView() is
    // not equivalent to what we need when you have a topContentInset or a header banner.
    FrameView* viewForRange = range->ownerDocument().view();
    if (!viewForRange)
        return;
    FrameView& mainFrameView = *m_webPage->corePage()->mainFrame().view();
    rect.setLocation(mainFrameView.windowToContents(viewForRange->contentsToWindow(rect.location())));

    // If the selection rect is completely outside this drawing tile, don't process it further
    if (!rect.intersects(dirtyRect))
        return;

    if (!m_currentHighlight || m_currentHighlightIsDirty) {
        CGRect cgRect = (CGRect)rect;

        m_currentHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRect(nullptr, &cgRect, 1, viewForRange->boundsRect(), true));
        m_currentHighlightIsDirty = false;
    }

    drawCurrentHighlight(graphicsContext);
}

void ServicesOverlayController::drawCurrentHighlight(WebCore::GraphicsContext& graphicsContext)
{
    Boolean onButton;
    m_mouseIsOverHighlight = DDHighlightPointIsOnHighlight(m_currentHighlight.get(), (CGPoint)m_mousePosition, &onButton);

    // If the mouse is not over the DDHighlight we have no drawing to do.
    if (!m_mouseIsOverHighlight)
        return;

    CGContextRef cgContext = graphicsContext.platformContext();
    
    CGLayerRef highlightLayer = DDHighlightGetLayerWithContext(m_currentHighlight.get(), cgContext);
    CGRect highlightBoundingRect = DDHighlightGetBoundingRect(m_currentHighlight.get());
    
    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.translate(toFloatSize(highlightBoundingRect.origin));
    graphicsContext.scale(FloatSize(1, -1));
    graphicsContext.translate(FloatSize(0, -highlightBoundingRect.size.height));
    
    CGRect highlightDrawRect = highlightBoundingRect;
    highlightDrawRect.origin.x = 0;
    highlightDrawRect.origin.y = 0;
    
    CGContextDrawLayerInRect(cgContext, highlightDrawRect, highlightLayer);
}

bool ServicesOverlayController::mouseEvent(PageOverlay*, const WebMouseEvent& event)
{
    m_mousePosition = m_webPage->corePage()->mainFrame().view()->rootViewToContents(event.position());

    bool mouseWasOverHighlight = m_mouseIsOverHighlight;
    Boolean onButton = false;
    m_mouseIsOverHighlight = m_currentHighlight ? DDHighlightPointIsOnHighlight(m_currentHighlight.get(), (CGPoint)m_mousePosition, &onButton) : false;

    if (mouseWasOverHighlight != m_mouseIsOverHighlight)
        m_servicesOverlay->setNeedsDisplay();

    // If this event has nothing to do with the left button, it clears the current mouse down tracking and we're done processing it.
    if (event.button() != WebMouseEvent::LeftButton) {
        m_mouseIsDownOnButton = false;
        return false;
    }

    if (!m_currentHighlight)
        return false;

    // Check and see if the mouse went up and we have a current mouse down highlight button.
    if (event.type() == WebEvent::MouseUp) {
        bool mouseWasDownOnButton = m_mouseIsDownOnButton;
        m_mouseIsDownOnButton = false;

        // If the mouse lifted while still over the highlight button that it went down on, then that is a click.
        if (m_mouseIsOverHighlight && onButton && mouseWasDownOnButton) {
            handleClick(m_mousePosition);
            return true;
        }
        
        return false;
    }
    
    // Check and see if the mouse moved within the confines of the DD highlight button.
    if (event.type() == WebEvent::MouseMove) {
        // Moving with the mouse button down is okay as long as the mouse never leaves the highlight button.
        if (m_mouseIsOverHighlight && onButton)
            return true;

        m_mouseIsDownOnButton = false;
        
        return false;
    }
    
    // Check and see if the mouse went down over a DD highlight button.
    if (event.type() == WebEvent::MouseDown) {
        if (m_mouseIsOverHighlight && onButton) {
            m_mouseIsDownOnButton = true;
            m_servicesOverlay->setNeedsDisplay();
            return true;
        }

        return false;
    }
        
    return false;
}

void ServicesOverlayController::handleClick(const WebCore::IntPoint& point)
{
    if (m_drawingTelephoneNumberHighlight) {
        ASSERT(m_currentTelephoneNumberRanges.size() == 1);
        m_webPage->handleTelephoneNumberClick(m_currentTelephoneNumberRanges[0]->text(), point);
    } else {
        // FIXME: Include all selected telephone numbers so they can be added to the menu as well.
        m_webPage->handleSelectionServiceClick(m_webPage->corePage()->mainFrame().selection(), point);
    }
}
    
} // namespace WebKit

#endif // #if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
