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
#import "SelectionOverlayController.h"

#if ENABLE(SERVICE_CONTROLS)

#import "WebProcess.h"
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

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectors)
SOFT_LINK(DataDetectors, DDHighlightCreateWithRectsInVisibleRect, DDHighlightRef, (CFAllocatorRef allocator, CGRect * rects, CFIndex count, CGRect globalVisibleRect, Boolean withArrow), (allocator, rects, count, globalVisibleRect, withArrow))
SOFT_LINK(DataDetectors, DDHighlightGetLayerWithContext, CGLayerRef, (DDHighlightRef highlight, CGContextRef context), (highlight, context))
SOFT_LINK(DataDetectors, DDHighlightGetBoundingRect, CGRect, (DDHighlightRef highlight), (highlight))
SOFT_LINK(DataDetectors, DDHighlightPointIsOnHighlight, Boolean, (DDHighlightRef highlight, CGPoint point, Boolean* onButton), (highlight, point, onButton))
SOFT_LINK(DataDetectors, DDHighlightSetButtonPressed, void, (DDHighlightRef highlight, Boolean buttonPressed), (highlight, buttonPressed))

using namespace WebCore;

namespace WebKit {

static double hoverTimerInterval = 2.0;

void SelectionOverlayController::drawRect(PageOverlay* overlay, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    if (m_currentSelectionRects.isEmpty())
        return;

    if (!WebProcess::shared().hasSelectionServices()) {
        destroyOverlay();
        return;
    }

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
            m_currentHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRect(nullptr, cgRects.begin(), cgRects.size(), bounds, true));
            m_currentHighlightIsDirty = false;

            Boolean onButton;
            m_mouseIsOverHighlight = DDHighlightPointIsOnHighlight(m_currentHighlight.get(), (CGPoint)m_mousePosition, &onButton);

            mouseHoverStateChanged();
        }
    }

    // If the UI is not visibile or if the mouse is not over the DDHighlight we have no drawing to do.
    if (!m_mouseIsOverHighlight || !m_visible)
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
    
bool SelectionOverlayController::mouseEvent(PageOverlay*, const WebMouseEvent& event)
{
    m_mousePosition = m_webPage->corePage()->mainFrame().view()->rootViewToContents(event.position());

    bool mouseWasOverHighlight = m_mouseIsOverHighlight;
    Boolean onButton = false;
    m_mouseIsOverHighlight = m_currentHighlight ? DDHighlightPointIsOnHighlight(m_currentHighlight.get(), (CGPoint)m_mousePosition, &onButton) : false;

    if (mouseWasOverHighlight != m_mouseIsOverHighlight) {
        m_selectionOverlay->setNeedsDisplay();
        mouseHoverStateChanged();
    }

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

            // FIXME: We need to do the following, but SOFT_LINK isn't working for this method.
            // DDHighlightSetButtonPressed(m_currentHighlight.get(), true);
            
            m_selectionOverlay->setNeedsDisplay();
            return true;
        }

        return false;
    }
        
    return false;
}

void SelectionOverlayController::handleClick(const WebCore::IntPoint& point)
{
    m_webPage->handleSelectionServiceClick(m_webPage->corePage()->mainFrame().selection(), point);
}

void SelectionOverlayController::mouseHoverStateChanged()
{
    if (m_mouseIsOverHighlight) {
        if (!m_hoverTimer.isActive())
            m_hoverTimer.startOneShot(hoverTimerInterval);
    } else {
        m_visible = false;
        m_hoverTimer.stop();
    }
}
    
} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS)

