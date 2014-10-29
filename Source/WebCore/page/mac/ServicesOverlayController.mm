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

#if (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)) && PLATFORM(MAC)

#import "Chrome.h"
#import "ChromeClient.h"
#import "Document.h"
#import "Editor.h"
#import "EventHandler.h"
#import "FloatQuad.h"
#import "FocusController.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "GapRects.h"
#import "GraphicsContext.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerCA.h"
#import "Logging.h"
#import "MainFrame.h"
#import "Page.h"
#import "PageOverlayController.h"
#import "PlatformCAAnimationMac.h"
#import "Settings.h"
#import "SoftLinking.h"
#import <QuartzCore/QuartzCore.h>

#if __has_include(<DataDetectors/DDHighlightDrawing.h>)
#import <DataDetectors/DDHighlightDrawing.h>
#else
typedef struct __DDHighlight DDHighlight, *DDHighlightRef;
#endif

#if __has_include(<DataDetectors/DDHighlightDrawing_Private.h>)
#import <DataDetectors/DDHighlightDrawing_Private.h>
#endif

const float highlightFadeAnimationDuration = 0.3;

typedef NSUInteger DDHighlightStyle;
static const DDHighlightStyle DDHighlightNoOutlineWithArrow = (1 << 16);
static const DDHighlightStyle DDHighlightOutlineWithArrow = (1 << 16) | 1;

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectors)
SOFT_LINK(DataDetectors, DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection, DDHighlightRef, (CFAllocatorRef allocator, CGRect* rects, CFIndex count, CGRect globalVisibleRect, DDHighlightStyle style, Boolean withArrow, NSWritingDirection writingDirection, Boolean endsWithEOL, Boolean flipped), (allocator, rects, count, globalVisibleRect, style, withArrow, writingDirection, endsWithEOL, flipped))
SOFT_LINK(DataDetectors, DDHighlightGetLayerWithContext, CGLayerRef, (DDHighlightRef highlight, CGContextRef context), (highlight, context))
SOFT_LINK(DataDetectors, DDHighlightGetBoundingRect, CGRect, (DDHighlightRef highlight), (highlight))
SOFT_LINK(DataDetectors, DDHighlightPointIsOnHighlight, Boolean, (DDHighlightRef highlight, CGPoint point, Boolean* onButton), (highlight, point, onButton))

namespace WebCore {

PassRefPtr<ServicesOverlayController::Highlight> ServicesOverlayController::Highlight::createForSelection(ServicesOverlayController& controller, RetainPtr<DDHighlightRef> ddHighlight, PassRefPtr<Range> range)
{
    return adoptRef(new Highlight(controller, Type::Selection, ddHighlight, range));
}

PassRefPtr<ServicesOverlayController::Highlight> ServicesOverlayController::Highlight::createForTelephoneNumber(ServicesOverlayController& controller, RetainPtr<DDHighlightRef> ddHighlight, PassRefPtr<Range> range)
{
    return adoptRef(new Highlight(controller, Type::TelephoneNumber, ddHighlight, range));
}

ServicesOverlayController::Highlight::Highlight(ServicesOverlayController& controller, Type type, RetainPtr<DDHighlightRef> ddHighlight, PassRefPtr<WebCore::Range> range)
    : m_range(range)
    , m_type(type)
    , m_controller(&controller)
{
    ASSERT(ddHighlight);
    ASSERT(m_range);

    Page* page = controller.mainFrame().page();
    m_graphicsLayer = GraphicsLayer::create(page ? page->chrome().client().graphicsLayerFactory() : nullptr, *this);
    m_graphicsLayer->setDrawsContent(true);

    setDDHighlight(ddHighlight.get());

    // Set directly on the PlatformCALayer so that when we leave the 'from' value implicit
    // in our animations, we get the right initial value regardless of flush timing.
    toGraphicsLayerCA(layer())->platformCALayer()->setOpacity(0);

    controller.didCreateHighlight(this);
}

ServicesOverlayController::Highlight::~Highlight()
{
    if (m_controller)
        m_controller->willDestroyHighlight(this);
}

void ServicesOverlayController::Highlight::setDDHighlight(DDHighlightRef highlight)
{
    if (!m_controller)
        return;

    m_ddHighlight = highlight;

    if (!m_ddHighlight)
        return;

    CGRect highlightBoundingRect = DDHighlightGetBoundingRect(m_ddHighlight.get());
    m_graphicsLayer->setPosition(FloatPoint(highlightBoundingRect.origin));
    m_graphicsLayer->setSize(FloatSize(highlightBoundingRect.size));

    m_graphicsLayer->setNeedsDisplay();
}

void ServicesOverlayController::Highlight::invalidate()
{
    layer()->removeFromParent();
    m_controller = nullptr;
}

void ServicesOverlayController::Highlight::notifyFlushRequired(const GraphicsLayer*)
{
    if (!m_controller)
        return;

    Page* page = m_controller->mainFrame().page();
    if (!page)
        return;

    page->chrome().client().scheduleCompositingLayerFlush();
}

void ServicesOverlayController::Highlight::paintContents(const GraphicsLayer*, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const FloatRect&)
{
    CGContextRef cgContext = graphicsContext.platformContext();

    CGLayerRef highlightLayer = DDHighlightGetLayerWithContext(ddHighlight(), cgContext);
    CGRect highlightBoundingRect = DDHighlightGetBoundingRect(ddHighlight());
    highlightBoundingRect.origin = CGPointZero;

    CGContextDrawLayerInRect(cgContext, highlightBoundingRect, highlightLayer);
}

float ServicesOverlayController::Highlight::deviceScaleFactor() const
{
    if (!m_controller)
        return 1;

    Page* page = m_controller->mainFrame().page();
    if (!page)
        return 1;

    return page->deviceScaleFactor();
}

void ServicesOverlayController::Highlight::fadeIn()
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [animation setDuration:highlightFadeAnimationDuration];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:false];
    [animation setToValue:@1];

    RefPtr<PlatformCAAnimation> platformAnimation = PlatformCAAnimationMac::create(animation.get());
    toGraphicsLayerCA(layer())->platformCALayer()->addAnimationForKey("FadeHighlightIn", platformAnimation.get());
}

void ServicesOverlayController::Highlight::fadeOut()
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [animation setDuration:highlightFadeAnimationDuration];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:false];
    [animation setToValue:@0];

    RefPtr<Highlight> retainedSelf = this;
    [CATransaction begin];
    [CATransaction setCompletionBlock:[retainedSelf] () {
        retainedSelf->didFinishFadeOutAnimation();
    }];

    RefPtr<PlatformCAAnimation> platformAnimation = PlatformCAAnimationMac::create(animation.get());
    toGraphicsLayerCA(layer())->platformCALayer()->addAnimationForKey("FadeHighlightOut", platformAnimation.get());
    [CATransaction commit];
}

void ServicesOverlayController::Highlight::didFinishFadeOutAnimation()
{
    if (!m_controller)
        return;

    if (m_controller->activeHighlight() == this)
        return;

    layer()->removeFromParent();
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

ServicesOverlayController::ServicesOverlayController(MainFrame& mainFrame)
    : m_mainFrame(mainFrame)
    , m_servicesOverlay(nullptr)
    , m_isTextOnly(false)
    , m_determineActiveHighlightTimer(this, &ServicesOverlayController::determineActiveHighlightTimerFired)
{
}

ServicesOverlayController::~ServicesOverlayController()
{
    for (auto& highlight : m_highlights)
        highlight->invalidate();
}

void ServicesOverlayController::pageOverlayDestroyed(PageOverlay&)
{
    // Before the overlay is destroyed, it should have moved out of the Page,
    // at which point we already cleared our back pointer.
    ASSERT(!m_servicesOverlay);
}

void ServicesOverlayController::willMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        return;

    ASSERT(m_servicesOverlay);
    m_servicesOverlay = nullptr;
}

void ServicesOverlayController::didMoveToPage(PageOverlay&, Page*)
{
}

static const uint8_t AlignmentNone = 0;
static const uint8_t AlignmentLeft = 1 << 0;
static const uint8_t AlignmentRight = 1 << 1;

static void expandForGap(Vector<LayoutRect>& rects, uint8_t* alignments, const GapRects& gap)
{
    if (!gap.left().isEmpty()) {
        LayoutUnit leftEdge = gap.left().x();
        for (unsigned i = 0; i < rects.size(); ++i) {
            if (alignments[i] & AlignmentLeft)
                rects[i].shiftXEdgeTo(leftEdge);
        }
    }

    if (!gap.right().isEmpty()) {
        LayoutUnit rightEdge = gap.right().maxX();
        for (unsigned i = 0; i < rects.size(); ++i) {
            if (alignments[i] & AlignmentRight)
                rects[i].shiftMaxXEdgeTo(rightEdge);
        }
    }
}

static inline void stitchRects(Vector<LayoutRect>& rects)
{
    if (rects.size() <= 1)
        return;
    
    Vector<LayoutRect> newRects;
    
    // FIXME: Need to support vertical layout.
    // First stitch together all the rects on the first line of the selection.
    size_t indexFromStart = 0;
    LayoutUnit firstTop = rects[indexFromStart].y();
    LayoutRect& currentRect = rects[indexFromStart];
    while (indexFromStart < rects.size() && rects[indexFromStart].y() == firstTop)
        currentRect.unite(rects[indexFromStart++]);
    
    newRects.append(currentRect);
    if (indexFromStart == rects.size()) {
        // All the rects are on one line. There is nothing else to do.
        rects.swap(newRects);
        return;
    }
    
    // Next stitch together all the rects on the last line of the selection.
    size_t indexFromEnd = rects.size() - 1;
    LayoutUnit lastTop = rects[indexFromEnd].y();
    LayoutRect lastRect = rects[indexFromEnd];
    while (indexFromEnd >= indexFromStart && rects[indexFromEnd].y() == lastTop)
        lastRect.unite(rects[indexFromEnd--]);
    
    // indexFromStart is the index of the first rectangle on the second line.
    // indexFromEnd is the index of the last rectangle on the second to the last line.
    // if they are equal, there is one additional rectangle for the line in the middle.
    if (indexFromEnd == indexFromStart)
        newRects.append(rects[indexFromStart]);
    
    if (indexFromEnd <= indexFromStart) {
        // There are no more rects to stitch. Just append the last line.
        newRects.append(lastRect);
        rects.swap(newRects);
        return;
    }
    
    // Stitch together all the rects after the first line until the second to the last included.
    currentRect = rects[indexFromStart];
    while (indexFromStart != indexFromEnd)
        currentRect.unite(rects[++indexFromStart]);
    
    newRects.append(currentRect);
    newRects.append(lastRect);

    rects.swap(newRects);
}

static void compactRectsWithGapRects(Vector<LayoutRect>& rects, const Vector<GapRects>& gapRects)
{
    stitchRects(rects);
    
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
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
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
    UNUSED_PARAM(gapRects);
    UNUSED_PARAM(isTextOnly);
#endif
}

void ServicesOverlayController::selectedTelephoneNumberRangesChanged()
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    LOG(Services, "ServicesOverlayController - Telephone number ranges changed\n");
    buildPhoneNumberHighlights();
#endif
}

bool ServicesOverlayController::mouseIsOverHighlight(Highlight& highlight, bool& mouseIsOverButton) const
{
    Boolean onButton;
    bool hovered = DDHighlightPointIsOnHighlight(highlight.ddHighlight(), (CGPoint)m_mousePosition, &onButton);
    mouseIsOverButton = onButton;
    return hovered;
}

std::chrono::milliseconds ServicesOverlayController::remainingTimeUntilHighlightShouldBeShown(Highlight* highlight) const
{
    if (!highlight)
        return std::chrono::milliseconds::zero();

    auto minimumTimeUntilHighlightShouldBeShown = 200_ms;
    Page* page = m_mainFrame.page();
    if (page && page->focusController().focusedOrMainFrame().selection().selection().isContentEditable())
        minimumTimeUntilHighlightShouldBeShown = 1000_ms;

    bool mousePressed = m_mainFrame.eventHandler().mousePressed();

    // Highlight hysteresis is only for selection services, because telephone number highlights are already much more stable
    // by virtue of being expanded to include the entire telephone number. However, we will still avoid highlighting
    // telephone numbers while the mouse is down.
    if (highlight->type() == Highlight::Type::TelephoneNumber)
        return mousePressed ? minimumTimeUntilHighlightShouldBeShown : 0_ms;

    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastSelectionChange = now - m_lastSelectionChangeTime;
    auto timeSinceHighlightBecameActive = now - m_nextActiveHighlightChangeTime;
    auto timeSinceLastMouseUp = mousePressed ? 0_ms : now - m_lastMouseUpTime;

    auto remainingDelay = minimumTimeUntilHighlightShouldBeShown - std::min(std::min(timeSinceLastSelectionChange, timeSinceHighlightBecameActive), timeSinceLastMouseUp);
    return std::chrono::duration_cast<std::chrono::milliseconds>(remainingDelay);
}

void ServicesOverlayController::determineActiveHighlightTimerFired(Timer<ServicesOverlayController>&)
{
    bool mouseIsOverButton;
    determineActiveHighlight(mouseIsOverButton);
}

void ServicesOverlayController::drawRect(PageOverlay&, GraphicsContext&, const IntRect&)
{
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
    if (!DataDetectorsLibrary())
        return;

    if (!m_mainFrame.settings().serviceControlsEnabled())
        return;

    HashSet<RefPtr<Highlight>> newPotentialHighlights;

    FrameView& mainFrameView = *m_mainFrame.view();

    for (Frame* frame = &m_mainFrame; frame; frame = frame->tree().traverseNext()) {
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

            newPotentialHighlights.add(Highlight::createForTelephoneNumber(*this, ddHighlight, range));
        }
    }

    replaceHighlightsOfTypePreservingEquivalentHighlights(newPotentialHighlights, Highlight::Type::TelephoneNumber);

    didRebuildPotentialHighlights();
}

void ServicesOverlayController::buildSelectionHighlight()
{
    if (!DataDetectorsLibrary())
        return;

    if (!m_mainFrame.settings().serviceControlsEnabled())
        return;

    Page* page = m_mainFrame.page();
    if (!page)
        return;

    HashSet<RefPtr<Highlight>> newPotentialHighlights;

    Vector<CGRect> cgRects;
    cgRects.reserveCapacity(m_currentSelectionRects.size());

    RefPtr<Range> selectionRange = page->selection().firstRange();
    if (selectionRange) {
        FrameView* mainFrameView = m_mainFrame.view();
        if (!mainFrameView)
            return;

        FrameView* viewForRange = selectionRange->ownerDocument().view();

        for (auto& rect : m_currentSelectionRects) {
            IntRect currentRect = pixelSnappedIntRect(rect);
            currentRect.setLocation(mainFrameView->windowToContents(viewForRange->contentsToWindow(currentRect.location())));
            cgRects.append((CGRect)currentRect);
        }

        if (!cgRects.isEmpty()) {
            CGRect visibleRect = mainFrameView->visibleContentRect();
            RetainPtr<DDHighlightRef> ddHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, cgRects.begin(), cgRects.size(), visibleRect, DDHighlightNoOutlineWithArrow, YES, NSWritingDirectionNatural, NO, YES));
            
            newPotentialHighlights.add(Highlight::createForSelection(*this, ddHighlight, selectionRange));
        }
    }

    replaceHighlightsOfTypePreservingEquivalentHighlights(newPotentialHighlights, Highlight::Type::Selection);

    didRebuildPotentialHighlights();
}

void ServicesOverlayController::replaceHighlightsOfTypePreservingEquivalentHighlights(HashSet<RefPtr<Highlight>>& newPotentialHighlights, Highlight::Type type)
{
    // If any old Highlights are equivalent (by Range) to a new Highlight, reuse the old
    // one so that any metadata is retained.
    HashSet<RefPtr<Highlight>> reusedPotentialHighlights;

    for (auto& oldHighlight : m_potentialHighlights) {
        if (oldHighlight->type() != type)
            continue;

        for (auto& newHighlight : newPotentialHighlights) {
            if (highlightsAreEquivalent(oldHighlight.get(), newHighlight.get())) {
                oldHighlight->setDDHighlight(newHighlight->ddHighlight());

                reusedPotentialHighlights.add(oldHighlight);
                newPotentialHighlights.remove(newHighlight);

                break;
            }
        }
    }

    removeAllPotentialHighlightsOfType(type);

    m_potentialHighlights.add(newPotentialHighlights.begin(), newPotentialHighlights.end());
    m_potentialHighlights.add(reusedPotentialHighlights.begin(), reusedPotentialHighlights.end());
}

bool ServicesOverlayController::hasRelevantSelectionServices()
{
    if (Page* page = m_mainFrame.page())
        return page->chrome().client().hasRelevantSelectionServices(m_isTextOnly);
    return false;
}

void ServicesOverlayController::didRebuildPotentialHighlights()
{
    if (m_potentialHighlights.isEmpty()) {
        if (m_servicesOverlay)
            m_mainFrame.pageOverlayController().uninstallPageOverlay(m_servicesOverlay, PageOverlay::FadeMode::DoNotFade);
        return;
    }

    if (telephoneNumberRangesForFocusedFrame().isEmpty() && !hasRelevantSelectionServices())
        return;

    createOverlayIfNeeded();

    bool mouseIsOverButton;
    determineActiveHighlight(mouseIsOverButton);
}

void ServicesOverlayController::createOverlayIfNeeded()
{
    if (m_servicesOverlay)
        return;

    if (!m_mainFrame.settings().serviceControlsEnabled())
        return;

    RefPtr<PageOverlay> overlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
    m_servicesOverlay = overlay.get();
    m_mainFrame.pageOverlayController().installPageOverlay(overlay.release(), PageOverlay::FadeMode::DoNotFade);
}

Vector<RefPtr<Range>> ServicesOverlayController::telephoneNumberRangesForFocusedFrame()
{
    Page* page = m_mainFrame.page();
    if (!page)
        return { };

    return page->focusController().focusedOrMainFrame().editor().detectedTelephoneNumberRanges();
}

bool ServicesOverlayController::highlightsAreEquivalent(const Highlight* a, const Highlight* b)
{
    if (a == b)
        return true;

    if (!a || !b)
        return false;

    if (a->type() == b->type() && areRangesEqual(a->range(), b->range()))
        return true;

    return false;
}

ServicesOverlayController::Highlight* ServicesOverlayController::findTelephoneNumberHighlightContainingSelectionHighlight(Highlight& selectionHighlight)
{
    if (selectionHighlight.type() != Highlight::Type::Selection)
        return nullptr;

    Page* page = m_mainFrame.page();
    if (!page)
        return nullptr;

    const VisibleSelection& selection = page->selection();
    if (!selection.isRange())
        return nullptr;

    RefPtr<Range> activeSelectionRange = selection.toNormalizedRange();
    if (!activeSelectionRange)
        return nullptr;

    for (auto& highlight : m_potentialHighlights) {
        if (highlight->type() != Highlight::Type::TelephoneNumber)
            continue;

        if (highlight->range()->contains(*activeSelectionRange))
            return highlight.get();
    }

    return nullptr;
}

void ServicesOverlayController::determineActiveHighlight(bool& mouseIsOverActiveHighlightButton)
{
    mouseIsOverActiveHighlightButton = false;

    RefPtr<Highlight> newActiveHighlight;

    for (auto& highlight : m_potentialHighlights) {
        if (highlight->type() == Highlight::Type::Selection) {
            // If we've already found a new active highlight, and it's
            // a telephone number highlight, prefer that over this selection highlight.
            if (newActiveHighlight && newActiveHighlight->type() == Highlight::Type::TelephoneNumber)
                continue;

            // If this highlight has no compatible services, it can't be active.
            if (!hasRelevantSelectionServices())
                continue;
        }

        // If this highlight isn't hovered, it can't be active.
        bool mouseIsOverButton;
        if (!mouseIsOverHighlight(*highlight, mouseIsOverButton))
            continue;

        newActiveHighlight = highlight;
        mouseIsOverActiveHighlightButton = mouseIsOverButton;
    }

    // If our new active highlight is a selection highlight that is completely contained
    // by one of the phone number highlights, we'll make the phone number highlight active even if it's not hovered.
    if (newActiveHighlight && newActiveHighlight->type() == Highlight::Type::Selection) {
        if (Highlight* containedTelephoneNumberHighlight = findTelephoneNumberHighlightContainingSelectionHighlight(*newActiveHighlight)) {
            newActiveHighlight = containedTelephoneNumberHighlight;

            // We will always initially choose the telephone number highlight over the selection highlight if the
            // mouse is over the telephone number highlight's button, so we know that it's not hovered if we got here.
            mouseIsOverActiveHighlightButton = false;
        }
    }

    if (!this->highlightsAreEquivalent(m_activeHighlight.get(), newActiveHighlight.get())) {
        // When transitioning to a new highlight, we might end up in determineActiveHighlight multiple times
        // before the new highlight actually becomes active. Keep track of the last next-but-not-yet-active
        // highlight, and only reset the active highlight hysteresis when that changes.
        if (m_nextActiveHighlight != newActiveHighlight) {
            m_nextActiveHighlight = newActiveHighlight;
            m_nextActiveHighlightChangeTime = std::chrono::steady_clock::now();
        }

        m_currentMouseDownOnButtonHighlight = nullptr;

        if (m_activeHighlight) {
            m_activeHighlight->fadeOut();
            m_activeHighlight = nullptr;
        }

        auto remainingTimeUntilHighlightShouldBeShown = this->remainingTimeUntilHighlightShouldBeShown(newActiveHighlight.get());
        if (remainingTimeUntilHighlightShouldBeShown > std::chrono::steady_clock::duration::zero()) {
            m_determineActiveHighlightTimer.startOneShot(remainingTimeUntilHighlightShouldBeShown);
            return;
        }

        m_activeHighlight = m_nextActiveHighlight.release();

        if (m_activeHighlight) {
            m_servicesOverlay->layer().addChild(m_activeHighlight->layer());
            m_activeHighlight->fadeIn();
        }
    }
}

bool ServicesOverlayController::mouseEvent(PageOverlay&, const PlatformMouseEvent& event)
{
    m_mousePosition = m_mainFrame.view()->windowToContents(event.position());

    bool mouseIsOverActiveHighlightButton = false;
    determineActiveHighlight(mouseIsOverActiveHighlightButton);

    // Cancel the potential click if any button other than the left button changes state, and ignore the event.
    if (event.button() != MouseButton::LeftButton) {
        m_currentMouseDownOnButtonHighlight = nullptr;
        return false;
    }

    // If the mouse lifted while still over the highlight button that it went down on, then that is a click.
    if (event.type() == PlatformEvent::MouseReleased) {
        RefPtr<Highlight> mouseDownHighlight = m_currentMouseDownOnButtonHighlight;
        m_currentMouseDownOnButtonHighlight = nullptr;

        m_lastMouseUpTime = std::chrono::steady_clock::now();

        if (mouseIsOverActiveHighlightButton && mouseDownHighlight) {
            handleClick(m_mousePosition, *mouseDownHighlight);
            return true;
        }
        
        return false;
    }

    // If the mouse moved outside of the button tracking a potential click, stop tracking the click.
    if (event.type() == PlatformEvent::MouseMoved) {
        if (m_currentMouseDownOnButtonHighlight && mouseIsOverActiveHighlightButton)
            return true;

        m_currentMouseDownOnButtonHighlight = nullptr;
        return false;
    }

    // If the mouse went down over the active highlight's button, track this as a potential click.
    if (event.type() == PlatformEvent::MousePressed) {
        if (m_activeHighlight && mouseIsOverActiveHighlightButton) {
            m_currentMouseDownOnButtonHighlight = m_activeHighlight;
            return true;
        }

        return false;
    }

    return false;
}

void ServicesOverlayController::didScrollFrame(PageOverlay&, Frame& frame)
{
    if (frame.isMainFrame())
        return;

    buildPhoneNumberHighlights();
    buildSelectionHighlight();

    bool mouseIsOverActiveHighlightButton;
    determineActiveHighlight(mouseIsOverActiveHighlightButton);
}

void ServicesOverlayController::handleClick(const IntPoint& clickPoint, Highlight& highlight)
{
    FrameView* frameView = m_mainFrame.view();
    if (!frameView)
        return;

    Page* page = m_mainFrame.page();
    if (!page)
        return;

    IntPoint windowPoint = frameView->contentsToWindow(clickPoint);

    if (highlight.type() == Highlight::Type::Selection) {
        auto telephoneNumberRanges = telephoneNumberRangesForFocusedFrame();
        Vector<String> selectedTelephoneNumbers;
        selectedTelephoneNumbers.reserveCapacity(telephoneNumberRanges.size());
        for (auto& range : telephoneNumberRanges)
            selectedTelephoneNumbers.append(range->text());

        page->chrome().client().handleSelectionServiceClick(page->focusController().focusedOrMainFrame().selection(), selectedTelephoneNumbers, windowPoint);
    } else if (highlight.type() == Highlight::Type::TelephoneNumber)
        page->chrome().client().handleTelephoneNumberClick(highlight.range()->text(), windowPoint);
}

void ServicesOverlayController::didCreateHighlight(Highlight* highlight)
{
    ASSERT(!m_highlights.contains(highlight));
    m_highlights.add(highlight);
}

void ServicesOverlayController::willDestroyHighlight(Highlight* highlight)
{
    ASSERT(m_highlights.contains(highlight));
    m_highlights.remove(highlight);
}

} // namespace WebKit

#endif // (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)) && PLATFORM(MAC)
