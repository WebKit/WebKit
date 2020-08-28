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
#import "Frame.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "GapRects.h"
#import "GraphicsContext.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerCA.h"
#import "Logging.h"
#import "Page.h"
#import "PageOverlayController.h"
#import "PlatformCAAnimationCocoa.h"
#import "Settings.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/mac/DataDetectorsSPI.h>
#import <wtf/SoftLinking.h>

const float highlightFadeAnimationDuration = 0.3;

namespace WebCore {

Ref<ServicesOverlayController::Highlight> ServicesOverlayController::Highlight::createForSelection(ServicesOverlayController& controller, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new Highlight(controller, Highlight::SelectionType, WTFMove(ddHighlight), WTFMove(range)));
}

Ref<ServicesOverlayController::Highlight> ServicesOverlayController::Highlight::createForTelephoneNumber(ServicesOverlayController& controller, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new Highlight(controller, Highlight::TelephoneNumberType, WTFMove(ddHighlight), WTFMove(range)));
}

ServicesOverlayController::Highlight::Highlight(ServicesOverlayController& controller, Type type, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
    : m_controller(&controller)
    , m_range(WTFMove(range))
    , m_graphicsLayer(GraphicsLayer::create(controller.page().chrome().client().graphicsLayerFactory(), *this))
    , m_type(type)
{
    ASSERT(ddHighlight);

    m_graphicsLayer->setDrawsContent(true);

    setDDHighlight(ddHighlight.get());

    // Set directly on the PlatformCALayer so that when we leave the 'from' value implicit
    // in our animations, we get the right initial value regardless of flush timing.
    downcast<GraphicsLayerCA>(layer()).platformCALayer()->setOpacity(0);

    controller.didCreateHighlight(this);
}

ServicesOverlayController::Highlight::~Highlight()
{
    if (m_controller)
        m_controller->willDestroyHighlight(this);
}

void ServicesOverlayController::Highlight::setDDHighlight(DDHighlightRef highlight)
{
    if (!DataDetectorsLibrary())
        return;

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
    layer().removeFromParent();
    m_controller = nullptr;
}

void ServicesOverlayController::Highlight::notifyFlushRequired(const GraphicsLayer*)
{
    if (!m_controller)
        return;

    m_controller->page().renderingUpdateScheduler().scheduleTimedRenderingUpdate();
}

void ServicesOverlayController::Highlight::paintContents(const GraphicsLayer*, GraphicsContext& graphicsContext, const FloatRect&, GraphicsLayerPaintBehavior)
{
    if (!DataDetectorsLibrary())
        return;

    CGContextRef cgContext = graphicsContext.platformContext();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGLayerRef highlightLayer = DDHighlightGetLayerWithContext(ddHighlight(), cgContext);
    ALLOW_DEPRECATED_DECLARATIONS_END
    CGRect highlightBoundingRect = DDHighlightGetBoundingRect(ddHighlight());
    highlightBoundingRect.origin = CGPointZero;

    CGContextDrawLayerInRect(cgContext, highlightBoundingRect, highlightLayer);
}

float ServicesOverlayController::Highlight::deviceScaleFactor() const
{
    if (!m_controller)
        return 1;

    return m_controller->page().deviceScaleFactor();
}

void ServicesOverlayController::Highlight::fadeIn()
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [animation setDuration:highlightFadeAnimationDuration];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:false];
    [animation setToValue:@1];

    auto platformAnimation = PlatformCAAnimationCocoa::create(animation.get());
    downcast<GraphicsLayerCA>(layer()).platformCALayer()->addAnimationForKey("FadeHighlightIn", platformAnimation.get());
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

    auto platformAnimation = PlatformCAAnimationCocoa::create(animation.get());
    downcast<GraphicsLayerCA>(layer()).platformCALayer()->addAnimationForKey("FadeHighlightOut", platformAnimation.get());
    [CATransaction commit];
}

void ServicesOverlayController::Highlight::didFinishFadeOutAnimation()
{
    if (!m_controller)
        return;

    if (m_controller->activeHighlight() == this)
        return;

    layer().removeFromParent();
}

ServicesOverlayController::ServicesOverlayController(Page& page)
    : m_page(page)
    , m_determineActiveHighlightTimer(*this, &ServicesOverlayController::determineActiveHighlightTimerFired)
    , m_buildHighlightsTimer(*this, &ServicesOverlayController::buildPotentialHighlightsIfNeeded)
{
}

ServicesOverlayController::~ServicesOverlayController()
{
    for (auto& highlight : m_highlights)
        highlight->invalidate();
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
    m_currentSelectionRects = rects;
    m_isTextOnly = isTextOnly;

    m_lastSelectionChangeTime = MonotonicTime::now();

    compactRectsWithGapRects(m_currentSelectionRects, gapRects);

    // DataDetectors needs these reversed in order to place the arrow in the right location.
    m_currentSelectionRects.reverse();

    LOG(Services, "ServicesOverlayController - Selection rects changed - Now have %lu\n", rects.size());
    invalidateHighlightsOfType(Highlight::SelectionType);
}

void ServicesOverlayController::selectedTelephoneNumberRangesChanged()
{
    LOG(Services, "ServicesOverlayController - Telephone number ranges changed\n");
    invalidateHighlightsOfType(Highlight::TelephoneNumberType);
}

void ServicesOverlayController::invalidateHighlightsOfType(Highlight::Type type)
{
    if (!m_page.settings().serviceControlsEnabled())
        return;

    m_dirtyHighlightTypes |= type;
    m_buildHighlightsTimer.startOneShot(0_s);
}

void ServicesOverlayController::buildPotentialHighlightsIfNeeded()
{
    if (!m_dirtyHighlightTypes)
        return;

    if (m_dirtyHighlightTypes & Highlight::TelephoneNumberType)
        buildPhoneNumberHighlights();

    if (m_dirtyHighlightTypes & Highlight::SelectionType)
        buildSelectionHighlight();

    m_dirtyHighlightTypes = 0;

    if (m_potentialHighlights.isEmpty()) {
        if (m_servicesOverlay)
            m_page.pageOverlayController().uninstallPageOverlay(*m_servicesOverlay, PageOverlay::FadeMode::DoNotFade);
        return;
    }

    if (telephoneNumberRangesForFocusedFrame().isEmpty() && !hasRelevantSelectionServices())
        return;

    createOverlayIfNeeded();

    bool mouseIsOverButton;
    determineActiveHighlight(mouseIsOverButton);
}

bool ServicesOverlayController::mouseIsOverHighlight(Highlight& highlight, bool& mouseIsOverButton) const
{
    if (!DataDetectorsLibrary())
        return false;

    Boolean onButton;
    bool hovered = DDHighlightPointIsOnHighlight(highlight.ddHighlight(), (CGPoint)m_mousePosition, &onButton);
    mouseIsOverButton = onButton;
    return hovered;
}

Seconds ServicesOverlayController::remainingTimeUntilHighlightShouldBeShown(Highlight* highlight) const
{
    if (!highlight)
        return 0_s;

    Seconds minimumTimeUntilHighlightShouldBeShown = 200_ms;
    if (m_page.focusController().focusedOrMainFrame().selection().selection().isContentEditable())
        minimumTimeUntilHighlightShouldBeShown = 1_s;

    bool mousePressed = mainFrame().eventHandler().mousePressed();

    // Highlight hysteresis is only for selection services, because telephone number highlights are already much more stable
    // by virtue of being expanded to include the entire telephone number. However, we will still avoid highlighting
    // telephone numbers while the mouse is down.
    if (highlight->type() == Highlight::TelephoneNumberType)
        return mousePressed ? minimumTimeUntilHighlightShouldBeShown : 0_s;

    MonotonicTime now = MonotonicTime::now();
    Seconds timeSinceLastSelectionChange = now - m_lastSelectionChangeTime;
    Seconds timeSinceHighlightBecameActive = now - m_nextActiveHighlightChangeTime;
    Seconds timeSinceLastMouseUp = mousePressed ? 0_s : now - m_lastMouseUpTime;

    return minimumTimeUntilHighlightShouldBeShown - std::min(std::min(timeSinceLastSelectionChange, timeSinceHighlightBecameActive), timeSinceLastMouseUp);
}

void ServicesOverlayController::determineActiveHighlightTimerFired()
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
    Vector<SimpleRange> phoneNumberRanges;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        phoneNumberRanges.appendVector(frame->editor().detectedTelephoneNumberRanges());

    if (phoneNumberRanges.isEmpty()) {
        removeAllPotentialHighlightsOfType(Highlight::TelephoneNumberType);
        return;
    }

    if (!DataDetectorsLibrary())
        return;

    HashSet<RefPtr<Highlight>> newPotentialHighlights;

    FrameView& mainFrameView = *mainFrame().view();

    for (auto& range : phoneNumberRanges) {
        // FIXME: This makes a big rect if the range extends from the end of one line to the start of the next. Handle that case better?
        auto rect = enclosingIntRect(unitedBoundingBoxes(RenderObject::absoluteTextQuads(range)));

        // Convert to the main document's coordinate space.
        // FIXME: It's a little crazy to call contentsToWindow and then windowToContents in order to get the right coordinate space.
        // We should consider adding conversion functions to ScrollView for contentsToDocument(). Right now, contentsToRootView() is
        // not equivalent to what we need when you have a topContentInset or a header banner.
        auto* viewForRange = range.start.document().view();
        if (!viewForRange)
            continue;

        rect.setLocation(mainFrameView.windowToContents(viewForRange->contentsToWindow(rect.location())));

        CGRect cgRect = rect;
#if HAVE(DD_HIGHLIGHT_CREATE_WITH_SCALE)
        auto ddHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection(nullptr, &cgRect, 1, mainFrameView.visibleContentRect(), DDHighlightStyleBubbleStandard | DDHighlightStyleStandardIconArrow, YES, NSWritingDirectionNatural, NO, YES, 0));
#else
        auto ddHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, &cgRect, 1, mainFrameView.visibleContentRect(), DDHighlightStyleBubbleStandard | DDHighlightStyleStandardIconArrow, YES, NSWritingDirectionNatural, NO, YES));
#endif
        newPotentialHighlights.add(Highlight::createForTelephoneNumber(*this, WTFMove(ddHighlight), WTFMove(range)));
    }

    replaceHighlightsOfTypePreservingEquivalentHighlights(newPotentialHighlights, Highlight::TelephoneNumberType);
}

void ServicesOverlayController::buildSelectionHighlight()
{
    if (m_currentSelectionRects.isEmpty()) {
        removeAllPotentialHighlightsOfType(Highlight::SelectionType);
        return;
    }

    if (!DataDetectorsLibrary())
        return;

    HashSet<RefPtr<Highlight>> newPotentialHighlights;

    Vector<CGRect> cgRects;
    cgRects.reserveCapacity(m_currentSelectionRects.size());

    if (auto selectionRange = m_page.selection().firstRange()) {
        FrameView* mainFrameView = mainFrame().view();
        if (!mainFrameView)
            return;

        auto viewForRange = makeRefPtr(selectionRange->start.document().view());
        if (!viewForRange)
            return;

        for (auto& rect : m_currentSelectionRects) {
            IntRect currentRect = snappedIntRect(rect);
            currentRect.setLocation(mainFrameView->windowToContents(viewForRange->contentsToWindow(currentRect.location())));
            cgRects.append(currentRect);
        }

        if (!cgRects.isEmpty()) {
            CGRect visibleRect = mainFrameView->visibleContentRect();
#if HAVE(DD_HIGHLIGHT_CREATE_WITH_SCALE)
            auto ddHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection(nullptr, cgRects.begin(), cgRects.size(), visibleRect, DDHighlightStyleBubbleNone | DDHighlightStyleStandardIconArrow | DDHighlightStyleButtonShowAlways, YES, NSWritingDirectionNatural, NO, YES, 0));
#else
            auto ddHighlight = adoptCF(DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, cgRects.begin(), cgRects.size(), visibleRect, DDHighlightStyleBubbleNone | DDHighlightStyleStandardIconArrow | DDHighlightStyleButtonShowAlways, YES, NSWritingDirectionNatural, NO, YES));
#endif
            newPotentialHighlights.add(Highlight::createForSelection(*this, WTFMove(ddHighlight), WTFMove(*selectionRange)));
        }
    }

    replaceHighlightsOfTypePreservingEquivalentHighlights(newPotentialHighlights, Highlight::SelectionType);
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
    return m_page.chrome().client().hasRelevantSelectionServices(m_isTextOnly);
}

void ServicesOverlayController::createOverlayIfNeeded()
{
    if (m_servicesOverlay)
        return;

    if (!m_page.settings().serviceControlsEnabled())
        return;

    auto overlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
    m_servicesOverlay = overlay.ptr();
    m_page.pageOverlayController().installPageOverlay(WTFMove(overlay), PageOverlay::FadeMode::DoNotFade);
}

Vector<SimpleRange> ServicesOverlayController::telephoneNumberRangesForFocusedFrame()
{
    return m_page.focusController().focusedOrMainFrame().editor().detectedTelephoneNumberRanges();
}

bool ServicesOverlayController::highlightsAreEquivalent(const Highlight* a, const Highlight* b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    return a->type() == b->type() && a->range() == b->range();
}

ServicesOverlayController::Highlight* ServicesOverlayController::findTelephoneNumberHighlightContainingSelectionHighlight(Highlight& selectionHighlight)
{
    if (selectionHighlight.type() != Highlight::SelectionType)
        return nullptr;

    auto selectionRange = m_page.selection().toNormalizedRange();
    if (!selectionRange)
        return nullptr;

    for (auto& highlight : m_potentialHighlights) {
        if (highlight->type() == Highlight::TelephoneNumberType && contains(highlight->range(), *selectionRange))
            return highlight.get();
    }

    return nullptr;
}

void ServicesOverlayController::determineActiveHighlight(bool& mouseIsOverActiveHighlightButton)
{
    buildPotentialHighlightsIfNeeded();

    mouseIsOverActiveHighlightButton = false;

    RefPtr<Highlight> newActiveHighlight;

    for (auto& highlight : m_potentialHighlights) {
        if (highlight->type() == Highlight::SelectionType) {
            // If we've already found a new active highlight, and it's
            // a telephone number highlight, prefer that over this selection highlight.
            if (newActiveHighlight && newActiveHighlight->type() == Highlight::TelephoneNumberType)
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
    if (newActiveHighlight && newActiveHighlight->type() == Highlight::SelectionType) {
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
            m_nextActiveHighlightChangeTime = MonotonicTime::now();
        }

        m_currentMouseDownOnButtonHighlight = nullptr;

        if (m_activeHighlight) {
            m_activeHighlight->fadeOut();
            m_activeHighlight = nullptr;
        }

        auto remainingTimeUntilHighlightShouldBeShown = this->remainingTimeUntilHighlightShouldBeShown(newActiveHighlight.get());
        if (remainingTimeUntilHighlightShouldBeShown > 0_s) {
            m_determineActiveHighlightTimer.startOneShot(remainingTimeUntilHighlightShouldBeShown);
            return;
        }

        m_activeHighlight = WTFMove(m_nextActiveHighlight);

        if (m_activeHighlight) {
            Ref<GraphicsLayer> highlightLayer = m_activeHighlight->layer();
            m_servicesOverlay->layer().addChild(WTFMove(highlightLayer));
            m_activeHighlight->fadeIn();
        }
    }
}

bool ServicesOverlayController::mouseEvent(PageOverlay&, const PlatformMouseEvent& event)
{
    m_mousePosition = mainFrame().view()->windowToContents(event.position());

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

        m_lastMouseUpTime = MonotonicTime::now();

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

    invalidateHighlightsOfType(Highlight::TelephoneNumberType);
    invalidateHighlightsOfType(Highlight::SelectionType);
    buildPotentialHighlightsIfNeeded();

    bool mouseIsOverActiveHighlightButton;
    determineActiveHighlight(mouseIsOverActiveHighlightButton);
}

void ServicesOverlayController::handleClick(const IntPoint& clickPoint, Highlight& highlight)
{
    FrameView* frameView = mainFrame().view();
    if (!frameView)
        return;

    IntPoint windowPoint = frameView->contentsToWindow(clickPoint);

    if (highlight.type() == Highlight::SelectionType) {
        auto telephoneNumberRanges = telephoneNumberRangesForFocusedFrame();
        Vector<String> selectedTelephoneNumbers;
        selectedTelephoneNumbers.reserveCapacity(telephoneNumberRanges.size());
        for (auto& range : telephoneNumberRanges)
            selectedTelephoneNumbers.append(plainText(range));

        m_page.chrome().client().handleSelectionServiceClick(m_page.focusController().focusedOrMainFrame().selection(), selectedTelephoneNumbers, windowPoint);
    } else if (highlight.type() == Highlight::TelephoneNumberType)
        m_page.chrome().client().handleTelephoneNumberClick(plainText(highlight.range()), windowPoint);
}

Frame& ServicesOverlayController::mainFrame() const
{
    return m_page.mainFrame();
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
