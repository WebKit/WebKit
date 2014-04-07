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
#import "TelephoneNumberOverlayController.h"

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

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

void TelephoneNumberOverlayController::drawRect(PageOverlay* overlay, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    if (m_currentSelectionRanges.isEmpty())
        return;
    
    clearHighlights();
    
    CGContextRef cgContext = graphicsContext.platformContext();
    
    for (auto& range : m_currentSelectionRanges) {
        // FIXME: This will choke if the range wraps around the edge of the view.
        // What should we do in that case?
        IntRect rect = enclosingIntRect(range->boundingRect());
        CGRect cgRects[] = { (CGRect)rect };

        RetainPtr<DDHighlightRef> highlight = adoptCF(DDHighlightCreateWithRectsInVisibleRect(nullptr, cgRects, 1, (CGRect)dirtyRect, true));
        RefPtr<TelephoneNumberData> telephoneNumberData = TelephoneNumberData::create(range.get(), highlight.get());
        m_telephoneNumberDatas.append(telephoneNumberData);
        
        // Check and see if the mouse is currently down inside this highlight's button.
        if (m_mouseDownPosition != IntPoint()) {
            Boolean onButton;
            if (DDHighlightPointIsOnHighlight(highlight.get(), (CGPoint)m_mouseDownPosition, &onButton)) {
                if (onButton) {
                    m_currentMouseDownNumber = telephoneNumberData;
                    
                    // FIXME: We need to do the following, but SOFT_LINK isn't working for this method.
                    // DDHighlightSetButtonPressed(highlight, true);
                }
            }
        }
        
        CGLayerRef highlightLayer = DDHighlightGetLayerWithContext(highlight.get(), cgContext);
        CGRect highlightBoundingRect = DDHighlightGetBoundingRect(highlight.get());
        
        GraphicsContextStateSaver stateSaver(graphicsContext);

        graphicsContext.translate(toFloatSize(highlightBoundingRect.origin));
        graphicsContext.scale(FloatSize(1, -1));
        graphicsContext.translate(FloatSize(0, -highlightBoundingRect.size.height));
        
        CGRect highlightDrawRect = highlightBoundingRect;
        highlightDrawRect.origin.x = 0;
        highlightDrawRect.origin.y = 0;
        
        CGContextDrawLayerInRect(cgContext, highlightDrawRect, highlightLayer);
    }
}
    
void TelephoneNumberOverlayController::handleTelephoneClick(TelephoneNumberData* number, const IntPoint& point)
{
    ASSERT(number);

    m_webPage->handleTelephoneNumberClick(number->range()->text(), point);
}
    
bool TelephoneNumberOverlayController::mouseEvent(PageOverlay*, const WebMouseEvent& event)
{
    IntPoint mousePosition = m_webPage->corePage()->mainFrame().view()->rootViewToContents(event.position());
    
    // If this event has nothing to do with the left button, it clears the current mouse down tracking and we're done processing it.
    if (event.button() != WebMouseEvent::LeftButton) {
        clearMouseDownInformation();
        return false;
    }
    
    RefPtr<TelephoneNumberData> currentNumber = m_currentMouseDownNumber;
    
    // Check and see if the mouse went up and we have a current mouse down highlight button.
    if (event.type() == WebEvent::MouseUp && currentNumber) {
        clearMouseDownInformation();
        
        // If the mouse lifted while still over the highlight button that it went down on, then that is a click.
        Boolean onButton;
        if (DDHighlightPointIsOnHighlight(currentNumber->highlight(), (CGPoint)mousePosition, &onButton) && onButton) {
            handleTelephoneClick(currentNumber.get(), mousePosition);
            
            return true;
        }
        
        return false;
    }
    
    // Check and see if the mouse moved within the confines of the DD highlight button.
    if (event.type() == WebEvent::MouseMove && currentNumber) {
        Boolean onButton;
        
        // Moving with the mouse button down is okay as long as the mouse never leaves the highlight button.
        if (DDHighlightPointIsOnHighlight(currentNumber->highlight(), (CGPoint)mousePosition, &onButton) && onButton)
            return true;
        
        clearMouseDownInformation();
        
        return false;
    }
    
    // Check and see if the mouse went down over a DD highlight button.
    if (event.type() == WebEvent::MouseDown) {
        ASSERT(!m_currentMouseDownNumber);
        
        for (auto& telephoneNumberData : m_telephoneNumberDatas) {
            Boolean onButton;
            if (DDHighlightPointIsOnHighlight(telephoneNumberData->highlight(), (CGPoint)mousePosition, &onButton) && onButton) {
                m_mouseDownPosition = mousePosition;
                m_currentMouseDownNumber = telephoneNumberData;
                
                // FIXME: We need to do the following, but SOFT_LINK isn't working for this method.
                // DDHighlightSetButtonPressed(highlight.get(), true);
                
                m_telephoneNumberOverlay->setNeedsDisplay();
                return true;
            }
        }
        
        return false;
    }
        
    return false;
}
    
void TelephoneNumberOverlayController::clearMouseDownInformation()
{
    m_currentMouseDownNumber = nullptr;
    m_mouseDownPosition = IntPoint();
}
    
void TelephoneNumberOverlayController::clearHighlights()
{
    m_telephoneNumberDatas.clear();
    m_currentMouseDownNumber = nullptr;
}
    
}

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
