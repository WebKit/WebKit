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

#import "Logging.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/Document.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FrameView.h>
#import <WebCore/GapRects.h>
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
static const DDHighlightStyle DDHighlightOutlineWithArrow = (1 << 16) | 1;

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectors)
SOFT_LINK(DataDetectors, DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection, DDHighlightRef, (CFAllocatorRef allocator, CGRect* rects, CFIndex count, CGRect globalVisibleRect, DDHighlightStyle style, Boolean withArrow, NSWritingDirection writingDirection, Boolean endsWithEOL, Boolean flipped), (allocator, rects, count, globalVisibleRect, style, withArrow, writingDirection, endsWithEOL, flipped))
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
{
}

ServicesOverlayController::~ServicesOverlayController()
{
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

static const uint8_t AlignmentNone = 0;
static const uint8_t AlignmentLeft = 1 << 0;
static const uint8_t AlignmentRight = 1 << 1;

static void expandForGap(Vector<LayoutRect>& rects, uint8_t* alignments, const GapRects& gap)
{
    if (!gap.left().isEmpty()) {
        LayoutUnit leftEdge = gap.left().x();
        for (unsigned i = 0; i < 3; ++i) {
            if (alignments[i] & AlignmentLeft)
                rects[i].shiftXEdgeTo(leftEdge);
        }
    }

    if (!gap.right().isEmpty()) {
        LayoutUnit rightEdge = gap.right().maxX();
        for (unsigned i = 0; i < 3; ++i) {
            if (alignments[i] & AlignmentRight)
                rects[i].shiftMaxXEdgeTo(rightEdge);
        }
    }
}

static void compactRectsWithGapRects(Vector<LayoutRect>& rects, const Vector<GapRects>& gapRects)
{
    if (rects.isEmpty())
        return;

    // All of the middle rects - everything but the first and last - can be unioned together.
    if (rects.size() > 3) {
        LayoutRect united;
        for (unsigned i = 1; i < rects.size() - 1; ++i)
            united.unite(rects[i]);

        rects[1] = united;
        rects[2] = rects.last();
        rects.shrink(3);
    }

    // FIXME: The following alignments are correct for LTR text.
    // We should also account for RTL.
    uint8_t alignments[3];
    if (rects.size() == 1) {
        alignments[0] = AlignmentLeft | AlignmentRight;
        alignments[1] = AlignmentNone;
        alignments[2] = AlignmentNone;
    } else if (rects.size() == 2) {
        alignments[0] = AlignmentRight;
        alignments[1] = AlignmentLeft;
        alignments[2] = AlignmentNone;
    } else {
        alignments[0] = AlignmentRight;
        alignments[1] = AlignmentLeft | AlignmentRight;
        alignments[2] = AlignmentLeft;
    }

    // Account for each GapRects by extending the edge of certain LayoutRects to meet the gap.
    for (auto& gap : gapRects)
        expandForGap(rects, alignments, gap);

    // If we have 3 rects we might need one final GapRects to align the edges.
    if (rects.size() == 3) {
        LayoutRect left;
        LayoutRect right;
        for (unsigned i = 0; i < 3; ++i) {
            if (alignments[i] & AlignmentLeft) {
                if (left.isEmpty())
                    left = rects[i];
                else if (rects[i].x() < left.x())
                    left = rects[i];
            }
            if (alignments[i] & AlignmentRight) {
                if (right.isEmpty())
                    right = rects[i];
                else if ((rects[i].x() + rects[i].width()) > (right.x() + right.width()))
                    right = rects[i];
            }
        }

        if (!left.isEmpty() || !right.isEmpty()) {
            GapRects gap;
            gap.uniteLeft(left);
            gap.uniteRight(right);
            expandForGap(rects, alignments, gap);
        }
    }
}

void ServicesOverlayController::selectionRectsDidChange(const Vector<LayoutRect>& rects, const Vector<GapRects>& gapRects)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    clearSelectionHighlight();
    m_currentSelectionRects = rects;

    compactRectsWithGapRects(m_currentSelectionRects, gapRects);

    // DataDetectors needs these reversed in order to place the arrow in the right location.
    m_currentSelectionRects.reverse();

    LOG(Services, "ServicesOverlayController - Selection rects changed - Now have %lu\n", rects.size());

    createOverlayIfNeeded();
#else
    UNUSED_PARAM(rects);
#endif
}

void ServicesOverlayController::selectedTelephoneNumberRangesChanged(const Vector<RefPtr<Range>>& ranges)
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    LOG(Services, "ServicesOverlayController - Telephone number ranges changed - Had %lu, now have %lu\n", m_currentTelephoneNumberRanges.size(), ranges.size());
    m_currentTelephoneNumberRanges = ranges;
    m_telephoneNumberHighlights.clear();
    m_telephoneNumberHighlights.resize(ranges.size());

    createOverlayIfNeeded();
#else
    UNUSED_PARAM(ranges);
#endif
}

void ServicesOverlayController::clearHighlightState()
{
    clearSelectionHighlight();
    clearHoveredTelephoneNumberHighlight();

    m_telephoneNumberHighlights.clear();
}

void ServicesOverlayController::drawRect(PageOverlay* overlay, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    if (m_currentSelectionRects.isEmpty() && m_currentTelephoneNumberRanges.isEmpty()) {
        clearHighlightState();
        return;
    }

    if (drawTelephoneNumberHighlightIfVisible(graphicsContext, dirtyRect))
        return;

    drawSelectionHighlight(graphicsContext, dirtyRect);
}

void ServicesOverlayController::drawSelectionHighlight(WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    // It's possible to end up drawing the selection highlight before we've actually received the selection rects.
    // If that happens we'll end up here again once we have the rects.
    if (m_currentSelectionRects.isEmpty())
        return;

    // If there are no installed selection services and we have no phone numbers detected, then we have nothing to draw.
    if (!WebProcess::shared().hasSelectionServices() && m_currentTelephoneNumberRanges.isEmpty())
        return;

    if (!m_selectionHighlight)
        maybeCreateSelectionHighlight();

    if (m_selectionHighlight)
        drawHighlight(m_selectionHighlight.get(), graphicsContext);
}

bool ServicesOverlayController::drawTelephoneNumberHighlightIfVisible(WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    // Make sure the hovered telephone number highlight is still hovered.
    if (m_hoveredTelephoneNumberData) {
        Boolean onButton;
        if (!DDHighlightPointIsOnHighlight(m_hoveredTelephoneNumberData->highlight.get(), (CGPoint)m_mousePosition, &onButton))
            clearHoveredTelephoneNumberHighlight();

        bool foundMatchingRange = false;

        // Make sure the hovered highlight still corresponds to a current telephone number range.
        for (auto& range : m_currentTelephoneNumberRanges) {
            if (areRangesEqual(range.get(), m_hoveredTelephoneNumberData->range.get())) {
                foundMatchingRange = true;
                break;
            }
        }

        if (!foundMatchingRange)
            clearHoveredTelephoneNumberHighlight();
    }

    // Found out which - if any - telephone number is hovered.
    if (!m_hoveredTelephoneNumberData) {
        Boolean onButton;
        establishHoveredTelephoneHighlight(onButton);
    }

    // If a telephone number is actually hovered, draw it.
    if (m_hoveredTelephoneNumberData) {
        drawHighlight(m_hoveredTelephoneNumberData->highlight.get(), graphicsContext);
        return true;
    }

    return false;
}

void ServicesOverlayController::drawHighlight(DDHighlightRef highlight, WebCore::GraphicsContext& graphicsContext)
{
    ASSERT(highlight);

    Boolean onButton;
    bool mouseIsOverHighlight = DDHighlightPointIsOnHighlight(highlight, (CGPoint)m_mousePosition, &onButton);

    if (!mouseIsOverHighlight) {
        LOG(Services, "ServicesOverlayController::drawHighlight - Mouse is not over highlight, so drawing nothing");
        return;
    }

    CGContextRef cgContext = graphicsContext.platformContext();
    
    CGLayerRef highlightLayer = DDHighlightGetLayerWithContext(highlight, cgContext);
    CGRect highlightBoundingRect = DDHighlightGetBoundingRect(highlight);
    
    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.translate(toFloatSize(highlightBoundingRect.origin));

    CGRect highlightDrawRect = highlightBoundingRect;
    highlightDrawRect.origin.x = 0;
    highlightDrawRect.origin.y = 0;
    
    CGContextDrawLayerInRect(cgContext, highlightDrawRect, highlightLayer);
}

void ServicesOverlayController::clearSelectionHighlight()
{
    if (!m_selectionHighlight)
        return;

    if (m_currentHoveredHighlight == m_selectionHighlight)
        m_currentHoveredHighlight = nullptr;
    if (m_currentMouseDownOnButtonHighlight == m_selectionHighlight)
        m_currentMouseDownOnButtonHighlight = nullptr;
    m_selectionHighlight = nullptr;
}

void ServicesOverlayController::clearHoveredTelephoneNumberHighlight()
{
    if (!m_hoveredTelephoneNumberData)
        return;

    if (m_currentHoveredHighlight == m_hoveredTelephoneNumberData->highlight)
        m_currentHoveredHighlight = nullptr;
    if (m_currentMouseDownOnButtonHighlight == m_hoveredTelephoneNumberData->highlight)
        m_currentMouseDownOnButtonHighlight = nullptr;
    m_hoveredTelephoneNumberData = nullptr;
}

void ServicesOverlayController::establishHoveredTelephoneHighlight(Boolean& onButton)
{
    ASSERT(m_currentTelephoneNumberRanges.size() == m_telephoneNumberHighlights.size());

    for (unsigned i = 0; i < m_currentTelephoneNumberRanges.size(); ++i) {
        if (!m_telephoneNumberHighlights[i]) {
            // FIXME: This will choke if the range wraps around the edge of the view.
            // What should we do in that case?
            IntRect rect = textQuadsToBoundingRectForRange(*m_currentTelephoneNumberRanges[i]);

            // Convert to the main document's coordinate space.
            // FIXME: It's a little crazy to call contentsToWindow and then windowToContents in order to get the right coordinate space.
            // We should consider adding conversion functions to ScrollView for contentsToDocument(). Right now, contentsToRootView() is
            // not equivalent to what we need when you have a topContentInset or a header banner.
            FrameView* viewForRange = m_currentTelephoneNumberRanges[i]->ownerDocument().view();
            if (!viewForRange)
                continue;
            FrameView& mainFrameView = *m_webPage->corePage()->mainFrame().view();
            rect.setLocation(mainFrameView.windowToContents(viewForRange->contentsToWindow(rect.location())));

            CGRect cgRect = rect;
            m_telephoneNumberHighlights[i] = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, &cgRect, 1, viewForRange->boundsRect(), DDHighlightOutlineWithArrow, YES, NSWritingDirectionNatural, NO, YES));
        }

        if (!DDHighlightPointIsOnHighlight(m_telephoneNumberHighlights[i].get(), (CGPoint)m_mousePosition, &onButton))
            continue;

        if (!m_hoveredTelephoneNumberData || m_hoveredTelephoneNumberData->highlight != m_telephoneNumberHighlights[i])
            m_hoveredTelephoneNumberData = std::make_unique<TelephoneNumberData>(m_telephoneNumberHighlights[i], m_currentTelephoneNumberRanges[i]);

        m_servicesOverlay->setNeedsDisplay();
        return;
    }

    clearHoveredTelephoneNumberHighlight();
    onButton = false;
}

void ServicesOverlayController::maybeCreateSelectionHighlight()
{
    ASSERT(!m_selectionHighlight);
    ASSERT(m_servicesOverlay);

    Vector<CGRect> cgRects;
    cgRects.reserveCapacity(m_currentSelectionRects.size());

    for (auto& rect : m_currentSelectionRects)
        cgRects.append((CGRect)pixelSnappedIntRect(rect));

    if (!cgRects.isEmpty()) {
        CGRect bounds = m_webPage->corePage()->mainFrame().view()->boundsRect();
        m_selectionHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, cgRects.begin(), cgRects.size(), bounds, DDHighlightNoOutlineWithArrow, YES, NSWritingDirectionNatural, NO, YES));

        m_servicesOverlay->setNeedsDisplay();
    }
}

bool ServicesOverlayController::mouseEvent(PageOverlay*, const WebMouseEvent& event)
{
    m_mousePosition = m_webPage->corePage()->mainFrame().view()->rootViewToContents(event.position());

    DDHighlightRef oldHoveredHighlight = m_currentHoveredHighlight.get();

    Boolean onButton = false;
    establishHoveredTelephoneHighlight(onButton);
    if (m_hoveredTelephoneNumberData) {
        ASSERT(m_hoveredTelephoneNumberData->highlight);
        m_currentHoveredHighlight = m_hoveredTelephoneNumberData->highlight;
    } else {
        if (!m_selectionHighlight)
            maybeCreateSelectionHighlight();

        if (m_selectionHighlight && DDHighlightPointIsOnHighlight(m_selectionHighlight.get(), (CGPoint)m_mousePosition, &onButton))
            m_currentHoveredHighlight = m_selectionHighlight;
        else
            m_currentHoveredHighlight = nullptr;
    }

    if (oldHoveredHighlight != m_currentHoveredHighlight)
        m_servicesOverlay->setNeedsDisplay();

    // If this event has nothing to do with the left button, it clears the current mouse down tracking and we're done processing it.
    if (event.button() != WebMouseEvent::LeftButton) {
        m_currentMouseDownOnButtonHighlight = nullptr;
        return false;
    }

    // Check and see if the mouse went up and we have a current mouse down highlight button.
    if (event.type() == WebEvent::MouseUp) {
        RetainPtr<DDHighlightRef> mouseDownHighlight = std::move(m_currentMouseDownOnButtonHighlight);

        // If the mouse lifted while still over the highlight button that it went down on, then that is a click.
        if (onButton && mouseDownHighlight) {
            handleClick(m_mousePosition, mouseDownHighlight.get());
            return true;
        }
        
        return false;
    }

    // Check and see if the mouse moved within the confines of the DD highlight button.
    if (event.type() == WebEvent::MouseMove) {
        // Moving with the mouse button down is okay as long as the mouse never leaves the highlight button.
        if (m_currentMouseDownOnButtonHighlight && onButton)
            return true;

        m_currentMouseDownOnButtonHighlight = nullptr;
        return false;
    }

    // Check and see if the mouse went down over a DD highlight button.
    if (event.type() == WebEvent::MouseDown) {
        if (m_currentHoveredHighlight && onButton) {
            m_currentMouseDownOnButtonHighlight = m_currentHoveredHighlight;
            m_servicesOverlay->setNeedsDisplay();
            return true;
        }

        return false;
    }
        
    return false;
}

void ServicesOverlayController::handleClick(const WebCore::IntPoint& clickPoint, DDHighlightRef highlight)
{
    ASSERT(highlight);

    FrameView* frameView = m_webPage->mainFrameView();
    if (!frameView)
        return;

    IntPoint windowPoint = frameView->contentsToWindow(clickPoint);

    if (highlight == m_selectionHighlight) {
        Vector<String> selectedTelephoneNumbers;
        selectedTelephoneNumbers.reserveCapacity(m_currentTelephoneNumberRanges.size());
        for (auto& range : m_currentTelephoneNumberRanges)
            selectedTelephoneNumbers.append(range->text());

        m_webPage->handleSelectionServiceClick(m_webPage->corePage()->mainFrame().selection(), selectedTelephoneNumbers, windowPoint);
    } else if (m_hoveredTelephoneNumberData && m_hoveredTelephoneNumberData->highlight == highlight)
        m_webPage->handleTelephoneNumberClick(m_hoveredTelephoneNumberData->range->text(), windowPoint);
    else
        ASSERT_NOT_REACHED();
}
    
} // namespace WebKit

#endif // #if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
