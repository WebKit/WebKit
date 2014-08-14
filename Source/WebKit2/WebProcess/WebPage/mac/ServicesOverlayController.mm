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
#import <WebCore/FocusController.h>
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

PassRefPtr<ServicesOverlayController::Highlight> ServicesOverlayController::Highlight::createForSelection(RetainPtr<DDHighlightRef> ddHighlight)
{
    return adoptRef(new Highlight(Type::Selection, ddHighlight, nullptr));
}

PassRefPtr<ServicesOverlayController::Highlight> ServicesOverlayController::Highlight::createForTelephoneNumber(RetainPtr<DDHighlightRef> ddHighlight, PassRefPtr<Range> range)
{
    return adoptRef(new Highlight(Type::TelephoneNumber, ddHighlight, range));
}

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
    : m_webPage(webPage)
    , m_servicesOverlay(nullptr)
    , m_isTextOnly(false)
    , m_repaintHighlightTimer(this, &ServicesOverlayController::repaintHighlightTimerFired)
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
}

void ServicesOverlayController::didMoveToWebPage(PageOverlay*, WebPage*)
{
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

void ServicesOverlayController::selectionRectsDidChange(const Vector<LayoutRect>& rects, const Vector<GapRects>& gapRects, bool isTextOnly)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    m_currentSelectionRects = rects;
    m_isTextOnly = isTextOnly;

    m_lastSelectionChangeTime = std::chrono::steady_clock::now();

    compactRectsWithGapRects(m_currentSelectionRects, gapRects);

    // DataDetectors needs these reversed in order to place the arrow in the right location.
    m_currentSelectionRects.reverse();

    LOG(Services, "ServicesOverlayController - Selection rects changed - Now have %lu\n", rects.size());

    buildSelectionHighlight();
#else
    UNUSED_PARAM(rects);
#endif
}

void ServicesOverlayController::selectedTelephoneNumberRangesChanged()
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    LOG(Services, "ServicesOverlayController - Telephone number ranges changed\n");
    buildPhoneNumberHighlights();
#else
    UNUSED_PARAM(ranges);
#endif
}

bool ServicesOverlayController::mouseIsOverHighlight(Highlight& highlight, bool& mouseIsOverButton) const
{
    Boolean onButton;
    bool hovered = DDHighlightPointIsOnHighlight(highlight.ddHighlight(), (CGPoint)m_mousePosition, &onButton);
    mouseIsOverButton = onButton;
    return hovered;
}

std::chrono::milliseconds ServicesOverlayController::remainingTimeUntilHighlightShouldBeShown() const
{
    // Highlight hysteresis is only for selection services, because telephone number highlights are already much more stable
    // by virtue of being expanded to include the entire telephone number.
    if (m_activeHighlight->type() == Highlight::Type::TelephoneNumber)
        return std::chrono::milliseconds::zero();

    std::chrono::steady_clock::duration minimumTimeUntilHighlightShouldBeShown = 200_ms;

    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastSelectionChange = now - m_lastSelectionChangeTime;
    auto timeSinceHighlightBecameActive = now - m_lastActiveHighlightChangeTime;

    return std::chrono::duration_cast<std::chrono::milliseconds>(std::max(minimumTimeUntilHighlightShouldBeShown - timeSinceLastSelectionChange, minimumTimeUntilHighlightShouldBeShown - timeSinceHighlightBecameActive));
}

void ServicesOverlayController::repaintHighlightTimerFired(WebCore::Timer<ServicesOverlayController>&)
{
    if (m_servicesOverlay)
        m_servicesOverlay->setNeedsDisplay();
}

void ServicesOverlayController::drawHighlight(Highlight& highlight, WebCore::GraphicsContext& graphicsContext)
{
    bool mouseIsOverButton;
    if (!mouseIsOverHighlight(highlight, mouseIsOverButton)) {
        LOG(Services, "ServicesOverlayController::drawHighlight - Mouse is not over highlight, so drawing nothing");
        return;
    }

    auto remainingTimeUntilHighlightShouldBeShown = this->remainingTimeUntilHighlightShouldBeShown();
    if (remainingTimeUntilHighlightShouldBeShown > std::chrono::steady_clock::duration::zero()) {
        m_repaintHighlightTimer.startOneShot(remainingTimeUntilHighlightShouldBeShown);
        return;
    }

    CGContextRef cgContext = graphicsContext.platformContext();
    
    CGLayerRef highlightLayer = DDHighlightGetLayerWithContext(highlight.ddHighlight(), cgContext);
    CGRect highlightBoundingRect = DDHighlightGetBoundingRect(highlight.ddHighlight());

    CGContextDrawLayerInRect(cgContext, highlightBoundingRect, highlightLayer);
}

void ServicesOverlayController::drawRect(PageOverlay* overlay, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    bool mouseIsOverButton;
    determineActiveHighlight(mouseIsOverButton);

    if (m_activeHighlight)
        drawHighlight(*m_activeHighlight, graphicsContext);
}

void ServicesOverlayController::clearActiveHighlight()
{
    if (!m_activeHighlight)
        return;

    if (m_currentMouseDownOnButtonHighlight == m_activeHighlight)
        m_currentMouseDownOnButtonHighlight = nullptr;
    m_activeHighlight = nullptr;
}

void ServicesOverlayController::removeAllPotentialHighlightsOfType(Highlight::Type type)
{
    Vector<RefPtr<Highlight>> highlightsToRemove;
    for (auto& highlight : m_potentialHighlights) {
        if (highlight->type() == type)
            highlightsToRemove.append(highlight);
    }

    while (!highlightsToRemove.isEmpty())
        m_potentialHighlights.remove(highlightsToRemove.takeLast());
}

void ServicesOverlayController::buildPhoneNumberHighlights()
{
    removeAllPotentialHighlightsOfType(Highlight::Type::TelephoneNumber);

    Frame* mainFrame = m_webPage.mainFrame();
    FrameView& mainFrameView = *mainFrame->view();

    for (Frame* frame = mainFrame; frame; frame = frame->tree().traverseNext()) {
        auto& ranges = frame->editor().detectedTelephoneNumberRanges();
        for (auto& range : ranges) {
            // FIXME: This will choke if the range wraps around the edge of the view.
            // What should we do in that case?
            IntRect rect = textQuadsToBoundingRectForRange(*range);

            // Convert to the main document's coordinate space.
            // FIXME: It's a little crazy to call contentsToWindow and then windowToContents in order to get the right coordinate space.
            // We should consider adding conversion functions to ScrollView for contentsToDocument(). Right now, contentsToRootView() is
            // not equivalent to what we need when you have a topContentInset or a header banner.
            FrameView* viewForRange = range->ownerDocument().view();
            if (!viewForRange)
                continue;
            rect.setLocation(mainFrameView.windowToContents(viewForRange->contentsToWindow(rect.location())));

            CGRect cgRect = rect;
            RetainPtr<DDHighlightRef> ddHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, &cgRect, 1, mainFrameView.visibleContentRect(), DDHighlightOutlineWithArrow, YES, NSWritingDirectionNatural, NO, YES));

            m_potentialHighlights.add(Highlight::createForTelephoneNumber(ddHighlight, range));
        }
    }

    didRebuildPotentialHighlights();
}

void ServicesOverlayController::buildSelectionHighlight()
{
    removeAllPotentialHighlightsOfType(Highlight::Type::Selection);

    Vector<CGRect> cgRects;
    cgRects.reserveCapacity(m_currentSelectionRects.size());

    for (auto& rect : m_currentSelectionRects)
        cgRects.append((CGRect)pixelSnappedIntRect(rect));

    if (!cgRects.isEmpty()) {
        CGRect visibleRect = m_webPage.corePage()->mainFrame().view()->visibleContentRect();
        RetainPtr<DDHighlightRef> ddHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, cgRects.begin(), cgRects.size(), visibleRect, DDHighlightNoOutlineWithArrow, YES, NSWritingDirectionNatural, NO, YES));
        
        m_potentialHighlights.add(Highlight::createForSelection(ddHighlight));
    }

    didRebuildPotentialHighlights();
}

bool ServicesOverlayController::hasRelevantSelectionServices()
{
    return (m_isTextOnly && WebProcess::shared().hasSelectionServices()) || WebProcess::shared().hasRichContentServices();
}

void ServicesOverlayController::didRebuildPotentialHighlights()
{
    if (m_potentialHighlights.isEmpty()) {
        if (m_servicesOverlay)
            m_webPage.uninstallPageOverlay(m_servicesOverlay);
        return;
    }

    if (telephoneNumberRangesForFocusedFrame().isEmpty() && !hasRelevantSelectionServices())
        return;

    createOverlayIfNeeded();
}

void ServicesOverlayController::createOverlayIfNeeded()
{
    if (m_servicesOverlay) {
        m_servicesOverlay->setNeedsDisplay();
        return;
    }

    RefPtr<PageOverlay> overlay = PageOverlay::create(this, PageOverlay::OverlayType::Document);
    m_servicesOverlay = overlay.get();
    m_webPage.installPageOverlay(overlay.release(), PageOverlay::FadeMode::DoNotFade);
    m_servicesOverlay->setNeedsDisplay();
}

Vector<RefPtr<Range>> ServicesOverlayController::telephoneNumberRangesForFocusedFrame()
{
    Page* page = m_webPage.corePage();
    if (!page)
        return Vector<RefPtr<Range>>();

    return page->focusController().focusedOrMainFrame().editor().detectedTelephoneNumberRanges();
}

bool ServicesOverlayController::highlightsAreEquivalent(const Highlight* a, const Highlight* b)
{
    if (a == b)
        return true;

    if (!a || !b)
        return false;

    if (a->type() == Highlight::Type::TelephoneNumber && b->type() == Highlight::Type::TelephoneNumber && areRangesEqual(a->range(), b->range()))
        return true;

    return false;
}

void ServicesOverlayController::determineActiveHighlight(bool& mouseIsOverActiveHighlightButton)
{
    mouseIsOverActiveHighlightButton = false;

    RefPtr<Highlight> oldActiveHighlight = m_activeHighlight.release();

    for (auto& highlight : m_potentialHighlights) {
        if (highlight->type() == Highlight::Type::Selection) {
            // If we've already found a new active highlight, and it's
            // a telephone number highlight, prefer that over this selection highlight.
            if (m_activeHighlight && m_activeHighlight->type() == Highlight::Type::TelephoneNumber)
                continue;

            // If this highlight has no compatible services, it can't be active, unless we have telephone number highlights to show in the combined menu.
            if (telephoneNumberRangesForFocusedFrame().isEmpty() && !hasRelevantSelectionServices())
                continue;
        }

        // If this highlight isn't hovered, it can't be active.
        bool mouseIsOverButton;
        if (!mouseIsOverHighlight(*highlight, mouseIsOverButton))
            continue;

        m_activeHighlight = highlight;
        mouseIsOverActiveHighlightButton = mouseIsOverButton;
    }

    if (!highlightsAreEquivalent(oldActiveHighlight.get(), m_activeHighlight.get())) {
        m_lastActiveHighlightChangeTime = std::chrono::steady_clock::now();
        m_servicesOverlay->setNeedsDisplay();
        m_currentMouseDownOnButtonHighlight = nullptr;
    }
}

bool ServicesOverlayController::mouseEvent(PageOverlay*, const WebMouseEvent& event)
{
    m_mousePosition = m_webPage.corePage()->mainFrame().view()->rootViewToContents(event.position());

    bool mouseIsOverActiveHighlightButton = false;
    determineActiveHighlight(mouseIsOverActiveHighlightButton);

    // Cancel the potential click if any button other than the left button changes state, and ignore the event.
    if (event.button() != WebMouseEvent::LeftButton) {
        m_currentMouseDownOnButtonHighlight = nullptr;
        return false;
    }

    // If the mouse lifted while still over the highlight button that it went down on, then that is a click.
    if (event.type() == WebEvent::MouseUp) {
        RefPtr<Highlight> mouseDownHighlight = m_currentMouseDownOnButtonHighlight;
        m_currentMouseDownOnButtonHighlight = nullptr;

        if (mouseIsOverActiveHighlightButton && mouseDownHighlight && remainingTimeUntilHighlightShouldBeShown() <= std::chrono::steady_clock::duration::zero()) {
            handleClick(m_mousePosition, *mouseDownHighlight);
            return true;
        }
        
        return false;
    }

    // If the mouse moved outside of the button tracking a potential click, stop tracking the click.
    if (event.type() == WebEvent::MouseMove) {
        if (m_currentMouseDownOnButtonHighlight && mouseIsOverActiveHighlightButton)
            return true;

        m_currentMouseDownOnButtonHighlight = nullptr;
        return false;
    }

    // If the mouse went down over the active highlight's button, track this as a potential click.
    if (event.type() == WebEvent::MouseDown) {
        if (m_activeHighlight && mouseIsOverActiveHighlightButton) {
            m_currentMouseDownOnButtonHighlight = m_activeHighlight;
            m_servicesOverlay->setNeedsDisplay();
            return true;
        }

        return false;
    }

    return false;
}

void ServicesOverlayController::handleClick(const WebCore::IntPoint& clickPoint, Highlight& highlight)
{
    FrameView* frameView = m_webPage.mainFrameView();
    if (!frameView)
        return;

    IntPoint windowPoint = frameView->contentsToWindow(clickPoint);

    if (highlight.type() == Highlight::Type::Selection) {
        auto telephoneNumberRanges = telephoneNumberRangesForFocusedFrame();
        Vector<String> selectedTelephoneNumbers;
        selectedTelephoneNumbers.reserveCapacity(telephoneNumberRanges.size());
        for (auto& range : telephoneNumberRanges)
            selectedTelephoneNumbers.append(range->text());

        m_webPage.handleSelectionServiceClick(m_webPage.corePage()->mainFrame().selection(), selectedTelephoneNumbers, windowPoint);
    } else if (highlight.type() == Highlight::Type::TelephoneNumber)
        m_webPage.handleTelephoneNumberClick(highlight.range()->text(), windowPoint);
}

} // namespace WebKit

#endif // #if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
