/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004-2008, 2013-2015 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FrameView.h"

#include "AXObjectCache.h"
#include "AnimationController.h"
#include "BackForwardController.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWindow.h"
#include "DebugPageOverlays.h"
#include "DocumentMarkerController.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLBodyElement.h"
#include "HTMLDocument.h"
#include "HTMLEmbedElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLPlugInImageElement.h"
#include "ImageDocument.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "MemoryPressureHandler.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "PageCache.h"
#include "PageOverlayController.h"
#include "ProgressTracker.h"
#include "RenderEmbeddedObject.h"
#include "RenderFullScreen.h"
#include "RenderIFrame.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderSVGRoot.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGDocument.h"
#include "SVGSVGElement.h"
#include "ScriptedAnimationController.h"
#include "ScrollAnimator.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "TextResourceDecoder.h"
#include "TextStream.h"
#include "TiledBacking.h"
#include "WheelEventTestTrigger.h"

#include <wtf/CurrentTime.h>
#include <wtf/Ref.h>
#include <wtf/SystemTracing.h>
#include <wtf/TemporaryChange.h>

#if USE(COORDINATED_GRAPHICS)
#include "TiledBackingStore.h"
#endif

#if ENABLE(TEXT_AUTOSIZING)
#include "TextAutosizer.h"
#endif

#if ENABLE(CSS_SCROLL_SNAP)
#include "AxisScrollSnapOffsets.h"
#endif

#if PLATFORM(IOS)
#include "DocumentLoader.h"
#include "LegacyTileCache.h"
#endif

namespace WebCore {

using namespace HTMLNames;

double FrameView::sCurrentPaintTimeStamp = 0.0;

// The maximum number of updateEmbeddedObjects iterations that should be done before returning.
static const unsigned maxUpdateEmbeddedObjectsIterations = 2;

static RenderLayer::UpdateLayerPositionsFlags updateLayerPositionFlags(RenderLayer* layer, bool isRelayoutingSubtree, bool didFullRepaint)
{
    RenderLayer::UpdateLayerPositionsFlags flags = RenderLayer::defaultFlags;
    if (didFullRepaint) {
        flags &= ~RenderLayer::CheckForRepaint;
        flags |= RenderLayer::NeedsFullRepaintInBacking;
    }
    if (isRelayoutingSubtree && layer->enclosingPaginationLayer(RenderLayer::IncludeCompositedPaginatedLayers))
        flags |= RenderLayer::UpdatePagination;
    return flags;
}

Pagination::Mode paginationModeForRenderStyle(const RenderStyle& style)
{
    EOverflow overflow = style.overflowY();
    if (overflow != OPAGEDX && overflow != OPAGEDY)
        return Pagination::Unpaginated;

    bool isHorizontalWritingMode = style.isHorizontalWritingMode();
    TextDirection textDirection = style.direction();
    WritingMode writingMode = style.writingMode();

    // paged-x always corresponds to LeftToRightPaginated or RightToLeftPaginated. If the WritingMode
    // is horizontal, then we use TextDirection to choose between those options. If the WritingMode
    // is vertical, then the direction of the verticality dictates the choice.
    if (overflow == OPAGEDX) {
        if ((isHorizontalWritingMode && textDirection == LTR) || writingMode == LeftToRightWritingMode)
            return Pagination::LeftToRightPaginated;
        return Pagination::RightToLeftPaginated;
    }

    // paged-y always corresponds to TopToBottomPaginated or BottomToTopPaginated. If the WritingMode
    // is horizontal, then the direction of the horizontality dictates the choice. If the WritingMode
    // is vertical, then we use TextDirection to choose between those options. 
    if (writingMode == TopToBottomWritingMode || (!isHorizontalWritingMode && textDirection == RTL))
        return Pagination::TopToBottomPaginated;
    return Pagination::BottomToTopPaginated;
}

class SubtreeLayoutStateMaintainer {
public:
    SubtreeLayoutStateMaintainer(RenderElement* subtreeLayoutRoot)
        : m_layoutRoot(subtreeLayoutRoot)
    {
        if (m_layoutRoot) {
            RenderView& view = m_layoutRoot->view();
            view.pushLayoutState(*m_layoutRoot);
            if (shouldDisableLayoutStateForSubtree()) {
                view.disableLayoutState();
                m_didDisableLayoutState = true;
            }
        }
    }

    ~SubtreeLayoutStateMaintainer()
    {
        if (m_layoutRoot) {
            RenderView& view = m_layoutRoot->view();
            view.popLayoutState(*m_layoutRoot);
            if (m_didDisableLayoutState)
                view.enableLayoutState();
        }
    }

    bool shouldDisableLayoutStateForSubtree()
    {
        for (auto* renderer = m_layoutRoot; renderer; renderer = renderer->container()) {
            if (renderer->hasTransform() || renderer->hasReflection())
                return true;
        }
        return false;
    }
    
private:
    RenderElement* m_layoutRoot { nullptr };
    bool m_didDisableLayoutState { false };
};

FrameView::FrameView(Frame& frame)
    : m_frame(frame)
    , m_canHaveScrollbars(true)
    , m_layoutTimer(*this, &FrameView::layoutTimerFired)
    , m_layoutPhase(OutsideLayout)
    , m_inSynchronousPostLayout(false)
    , m_postLayoutTasksTimer(*this, &FrameView::performPostLayoutTasks)
    , m_updateEmbeddedObjectsTimer(*this, &FrameView::updateEmbeddedObjectsTimerFired)
    , m_isTransparent(false)
    , m_baseBackgroundColor(Color::white)
    , m_mediaType("screen")
    , m_overflowStatusDirty(true)
    , m_wasScrolledByUser(false)
    , m_inProgrammaticScroll(false)
    , m_safeToPropagateScrollToParent(true)
    , m_delayedScrollEventTimer(*this, &FrameView::sendScrollEvent)
    , m_isTrackingRepaints(false)
    , m_shouldUpdateWhileOffscreen(true)
    , m_deferSetNeedsLayoutCount(0)
    , m_setNeedsLayoutWasDeferred(false)
    , m_speculativeTilingEnabled(false)
    , m_speculativeTilingEnableTimer(*this, &FrameView::speculativeTilingEnableTimerFired)
#if PLATFORM(IOS)
    , m_useCustomFixedPositionLayoutRect(false)
    , m_useCustomSizeForResizeEvent(false)
#endif
    , m_hasOverrideViewportSize(false)
    , m_shouldAutoSize(false)
    , m_inAutoSize(false)
    , m_didRunAutosize(false)
    , m_autoSizeFixedMinimumHeight(0)
    , m_headerHeight(0)
    , m_footerHeight(0)
    , m_milestonesPendingPaint(0)
    , m_visualUpdatesAllowedByClient(true)
    , m_hasFlippedBlockRenderers(false)
    , m_scrollPinningBehavior(DoNotPin)
{
    init();

#if ENABLE(RUBBER_BANDING)
    ScrollElasticity verticalElasticity = ScrollElasticityNone;
    ScrollElasticity horizontalElasticity = ScrollElasticityNone;
    if (m_frame->isMainFrame()) {
        verticalElasticity = m_frame->page() ? m_frame->page()->verticalScrollElasticity() : ScrollElasticityAllowed;
        horizontalElasticity = m_frame->page() ? m_frame->page()->horizontalScrollElasticity() : ScrollElasticityAllowed;
    } else if (m_frame->settings().rubberBandingForSubScrollableRegionsEnabled()) {
        verticalElasticity = ScrollElasticityAutomatic;
        horizontalElasticity = ScrollElasticityAutomatic;
    }

    ScrollableArea::setVerticalScrollElasticity(verticalElasticity);
    ScrollableArea::setHorizontalScrollElasticity(horizontalElasticity);
#endif
}

Ref<FrameView> FrameView::create(Frame& frame)
{
    Ref<FrameView> view = adoptRef(*new FrameView(frame));
    view->show();
    return view;
}

Ref<FrameView> FrameView::create(Frame& frame, const IntSize& initialSize)
{
    Ref<FrameView> view = adoptRef(*new FrameView(frame));
    view->Widget::setFrameRect(IntRect(view->location(), initialSize));
    view->show();
    return view;
}

FrameView::~FrameView()
{
    if (m_postLayoutTasksTimer.isActive())
        m_postLayoutTasksTimer.stop();
    
    removeFromAXObjectCache();
    resetScrollbars();

    // Custom scrollbars should already be destroyed at this point
    ASSERT(!horizontalScrollbar() || !horizontalScrollbar()->isCustomScrollbar());
    ASSERT(!verticalScrollbar() || !verticalScrollbar()->isCustomScrollbar());

    setHasHorizontalScrollbar(false); // Remove native scrollbars now before we lose the connection to the HostWindow.
    setHasVerticalScrollbar(false);
    
    ASSERT(!m_scrollCorner);

    ASSERT(frame().view() != this || !frame().contentRenderer());
}

void FrameView::reset()
{
    m_cannotBlitToWindow = false;
    m_isOverlapped = false;
    m_contentIsOpaque = false;
    m_layoutTimer.stop();
    m_layoutRoot = nullptr;
    m_delayedLayout = false;
    m_needsFullRepaint = true;
    m_layoutSchedulingEnabled = true;
    m_layoutPhase = OutsideLayout;
    m_inSynchronousPostLayout = false;
    m_layoutCount = 0;
    m_nestedLayoutCount = 0;
    m_postLayoutTasksTimer.stop();
    m_updateEmbeddedObjectsTimer.stop();
    m_firstLayout = true;
    m_firstLayoutCallbackPending = false;
    m_wasScrolledByUser = false;
    m_safeToPropagateScrollToParent = true;
    m_delayedScrollEventTimer.stop();
    m_lastViewportSize = IntSize();
    m_lastZoomFactor = 1.0f;
    m_isTrackingRepaints = false;
    m_trackedRepaintRects.clear();
    m_lastPaintTime = 0;
    m_paintBehavior = PaintBehaviorNormal;
    m_isPainting = false;
    m_visuallyNonEmptyCharacterCount = 0;
    m_visuallyNonEmptyPixelCount = 0;
    m_isVisuallyNonEmpty = false;
    m_firstVisuallyNonEmptyLayoutCallbackPending = true;
    m_viewportIsStable = true;
    m_needsDeferredScrollbarsUpdate = false;
    m_maintainScrollPositionAnchor = nullptr;
}

void FrameView::removeFromAXObjectCache()
{
    if (AXObjectCache* cache = axObjectCache()) {
        if (HTMLFrameOwnerElement* owner = frame().ownerElement())
            cache->childrenChanged(owner->renderer());
        cache->remove(this);
    }
}

void FrameView::resetScrollbars()
{
    // Reset the document's scrollbars back to our defaults before we yield the floor.
    m_firstLayout = true;
    setScrollbarsSuppressed(true);
    if (m_canHaveScrollbars)
        setScrollbarModes(ScrollbarAuto, ScrollbarAuto);
    else
        setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
    setScrollbarsSuppressed(false);
}

void FrameView::resetScrollbarsAndClearContentsSize()
{
    resetScrollbars();

    setScrollbarsSuppressed(true);
    setContentsSize(IntSize());
    setScrollbarsSuppressed(false);
}

void FrameView::init()
{
    reset();

    m_margins = LayoutSize(-1, -1); // undefined
    m_size = LayoutSize();

    // Propagate the marginwidth/height and scrolling modes to the view.
    Element* ownerElement = frame().ownerElement();
    if (is<HTMLFrameElementBase>(ownerElement)) {
        HTMLFrameElementBase& frameElement = downcast<HTMLFrameElementBase>(*ownerElement);
        if (frameElement.scrollingMode() == ScrollbarAlwaysOff)
            setCanHaveScrollbars(false);
        LayoutUnit marginWidth = frameElement.marginWidth();
        LayoutUnit marginHeight = frameElement.marginHeight();
        if (marginWidth != -1)
            setMarginWidth(marginWidth);
        if (marginHeight != -1)
            setMarginHeight(marginHeight);
    }

    Page* page = frame().page();
    if (page && page->chrome().client().shouldPaintEntireContents())
        setPaintsEntireContents(true);
}
    
void FrameView::prepareForDetach()
{
    detachCustomScrollbars();
    // When the view is no longer associated with a frame, it needs to be removed from the ax object cache
    // right now, otherwise it won't be able to reach the topDocument()'s axObject cache later.
    removeFromAXObjectCache();

    if (frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = frame().page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(*this);
    }
}

void FrameView::detachCustomScrollbars()
{
    Scrollbar* horizontalBar = horizontalScrollbar();
    if (horizontalBar && horizontalBar->isCustomScrollbar())
        setHasHorizontalScrollbar(false);

    Scrollbar* verticalBar = verticalScrollbar();
    if (verticalBar && verticalBar->isCustomScrollbar())
        setHasVerticalScrollbar(false);

    m_scrollCorner = nullptr;
}

void FrameView::recalculateScrollbarOverlayStyle()
{
    ScrollbarOverlayStyle oldOverlayStyle = scrollbarOverlayStyle();
    WTF::Optional<ScrollbarOverlayStyle> clientOverlayStyle = frame().page() ? frame().page()->chrome().client().preferredScrollbarOverlayStyle() : ScrollbarOverlayStyleDefault;
    if (clientOverlayStyle) {
        if (clientOverlayStyle.value() != oldOverlayStyle)
            setScrollbarOverlayStyle(clientOverlayStyle.value());
        return;
    }

    ScrollbarOverlayStyle computedOverlayStyle = ScrollbarOverlayStyleDefault;

    Color backgroundColor = documentBackgroundColor();
    if (backgroundColor.isValid()) {
        // Reduce the background color from RGB to a lightness value
        // and determine which scrollbar style to use based on a lightness
        // heuristic.
        double hue, saturation, lightness;
        backgroundColor.getHSL(hue, saturation, lightness);
        if (lightness <= .5 && backgroundColor.alpha() > 0)
            computedOverlayStyle = ScrollbarOverlayStyleLight;
    }

    if (oldOverlayStyle != computedOverlayStyle)
        setScrollbarOverlayStyle(computedOverlayStyle);
}

void FrameView::clear()
{
    setCanBlitOnScroll(true);
    
    reset();

    setScrollbarsSuppressed(true);

#if PLATFORM(IOS)
    // To avoid flashes of white, disable tile updates immediately when view is cleared at the beginning of a page load.
    // Tiling will be re-enabled from UIKit via [WAKWindow setTilingMode:] when we have content to draw.
    if (LegacyTileCache* tileCache = legacyTileCache())
        tileCache->setTilingMode(LegacyTileCache::Disabled);
#endif
}

#if PLATFORM(IOS)
void FrameView::didReplaceMultipartContent()
{
    // Re-enable tile updates that were disabled in clear().
    if (LegacyTileCache* tileCache = legacyTileCache())
        tileCache->setTilingMode(LegacyTileCache::Normal);
}
#endif

bool FrameView::didFirstLayout() const
{
    return !m_firstLayout;
}

void FrameView::invalidateRect(const IntRect& rect)
{
    if (!parent()) {
        if (HostWindow* window = hostWindow())
            window->invalidateContentsAndRootView(rect);
        return;
    }

    RenderWidget* renderer = frame().ownerRenderer();
    if (!renderer)
        return;

    IntRect repaintRect = rect;
    repaintRect.move(renderer->borderLeft() + renderer->paddingLeft(),
                     renderer->borderTop() + renderer->paddingTop());
    renderer->repaintRectangle(repaintRect);
}

void FrameView::setFrameRect(const IntRect& newRect)
{
    Ref<FrameView> protectedThis(*this);
    IntRect oldRect = frameRect();
    if (newRect == oldRect)
        return;

#if ENABLE(TEXT_AUTOSIZING)
    // Autosized font sizes depend on the width of the viewing area.
    if (newRect.width() != oldRect.width()) {
        if (frame().isMainFrame() && page->settings().textAutosizingEnabled()) {
            for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext())
                frame().document()->textAutosizer()->recalculateMultipliers();
        }
    }
#endif

    ScrollView::setFrameRect(newRect);

    updateScrollableAreaSet();

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidChangeSize();
    }

    if (frame().isMainFrame())
        frame().mainFrame().pageOverlayController().didChangeViewSize();

    viewportContentsChanged();
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
bool FrameView::scheduleAnimation()
{
    if (HostWindow* window = hostWindow()) {
        window->scheduleAnimation();
        return true;
    }
    return false;
}
#endif

void FrameView::setMarginWidth(LayoutUnit w)
{
    // make it update the rendering area when set
    m_margins.setWidth(w);
}

void FrameView::setMarginHeight(LayoutUnit h)
{
    // make it update the rendering area when set
    m_margins.setHeight(h);
}

bool FrameView::frameFlatteningEnabled() const
{
    return frame().settings().frameFlatteningEnabled();
}

bool FrameView::isFrameFlatteningValidForThisFrame() const
{
    if (!frameFlatteningEnabled())
        return false;

    HTMLFrameOwnerElement* owner = frame().ownerElement();
    if (!owner)
        return false;

    // Frame flattening is valid only for <frame> and <iframe>.
    return owner->hasTagName(frameTag) || owner->hasTagName(iframeTag);
}

bool FrameView::avoidScrollbarCreation() const
{
    // with frame flattening no subframe can have scrollbars
    // but we also cannot turn scrollbars off as we determine
    // our flattening policy using that.
    return isFrameFlatteningValidForThisFrame();
}

void FrameView::setCanHaveScrollbars(bool canHaveScrollbars)
{
    m_canHaveScrollbars = canHaveScrollbars;
    ScrollView::setCanHaveScrollbars(canHaveScrollbars);
}

void FrameView::updateCanHaveScrollbars()
{
    ScrollbarMode hMode;
    ScrollbarMode vMode;
    scrollbarModes(hMode, vMode);
    if (hMode == ScrollbarAlwaysOff && vMode == ScrollbarAlwaysOff)
        setCanHaveScrollbars(false);
    else
        setCanHaveScrollbars(true);
}

PassRefPtr<Scrollbar> FrameView::createScrollbar(ScrollbarOrientation orientation)
{
    if (!frame().settings().allowCustomScrollbarInMainFrame() && frame().isMainFrame())
        return ScrollView::createScrollbar(orientation);

    // FIXME: We need to update the scrollbar dynamically as documents change (or as doc elements and bodies get discovered that have custom styles).
    Document* doc = frame().document();

    // Try the <body> element first as a scrollbar source.
    HTMLElement* body = doc ? doc->bodyOrFrameset() : nullptr;
    if (body && body->renderer() && body->renderer()->style().hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(*this, orientation, body);
    
    // If the <body> didn't have a custom style, then the root element might.
    Element* docElement = doc ? doc->documentElement() : nullptr;
    if (docElement && docElement->renderer() && docElement->renderer()->style().hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(*this, orientation, docElement);
        
    // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
    RenderWidget* frameRenderer = frame().ownerRenderer();
    if (frameRenderer && frameRenderer->style().hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(*this, orientation, nullptr, &frame());
    
    // Nobody set a custom style, so we just use a native scrollbar.
    return ScrollView::createScrollbar(orientation);
}

void FrameView::setContentsSize(const IntSize& size)
{
    if (size == contentsSize())
        return;

    m_deferSetNeedsLayoutCount++;

    ScrollView::setContentsSize(size);
    contentsResized();
    
    Page* page = frame().page();
    if (!page)
        return;

    updateScrollableAreaSet();

    page->chrome().contentsSizeChanged(&frame(), size); // Notify only.

    if (frame().isMainFrame()) {
        frame().mainFrame().pageOverlayController().didChangeDocumentSize();
        PageCache::singleton().markPagesForContentsSizeChanged(*page);
    }

    ASSERT(m_deferSetNeedsLayoutCount);
    m_deferSetNeedsLayoutCount--;
    
    if (!m_deferSetNeedsLayoutCount)
        m_setNeedsLayoutWasDeferred = false; // FIXME: Find a way to make the deferred layout actually happen.
}

void FrameView::adjustViewSize()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    ASSERT(frame().view() == this);

    const IntRect rect = renderView->documentRect();
    const IntSize& size = rect.size();
    ScrollView::setScrollOrigin(IntPoint(-rect.x(), -rect.y()), !frame().document()->printing(), size == contentsSize());

    LOG_WITH_STREAM(Layout, stream << "FrameView " << this << " adjustViewSize: unscaled document rect changed to " << renderView->unscaledDocumentRect() << " (scaled to " << size << ")");

    setContentsSize(size);
}

void FrameView::applyOverflowToViewport(const RenderElement& renderer, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.

    // To combat the inability to scroll on a page with overflow:hidden on the root when scaled, disregard hidden when
    // there is a frameScaleFactor that is greater than one on the main frame. Also disregard hidden if there is a
    // header or footer.

    bool overrideHidden = frame().isMainFrame() && ((frame().frameScaleFactor() > 1) || headerHeight() || footerHeight());

    EOverflow overflowX = renderer.style().overflowX();
    EOverflow overflowY = renderer.style().overflowY();

    if (is<RenderSVGRoot>(renderer)) {
        // FIXME: evaluate if we can allow overflow for these cases too.
        // Overflow is always hidden when stand-alone SVG documents are embedded.
        if (downcast<RenderSVGRoot>(renderer).isEmbeddedThroughFrameContainingSVGDocument()) {
            overflowX = OHIDDEN;
            overflowY = OHIDDEN;
        }
    }

    switch (overflowX) {
        case OHIDDEN:
            if (overrideHidden)
                hMode = ScrollbarAuto;
            else
                hMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            hMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            hMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }
    
     switch (overflowY) {
        case OHIDDEN:
            if (overrideHidden)
                vMode = ScrollbarAuto;
            else
                vMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            vMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            vMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all. Values of OPAGEDX and OPAGEDY are handled by applyPaginationToViewPort().
            ;
    }
}

void FrameView::applyPaginationToViewport()
{
    Document* document = frame().document();
    auto* documentElement = document->documentElement();
    RenderElement* documentRenderer = documentElement ? documentElement->renderer() : nullptr;
    RenderElement* documentOrBodyRenderer = documentRenderer;
    auto* body = document->body();
    if (body && body->renderer())
        documentOrBodyRenderer = documentRenderer->style().overflowX() == OVISIBLE && is<HTMLHtmlElement>(*documentElement) ? body->renderer() : documentRenderer;

    Pagination pagination;

    if (!documentOrBodyRenderer) {
        setPagination(pagination);
        return;
    }

    EOverflow overflowY = documentOrBodyRenderer->style().overflowY();
    if (overflowY == OPAGEDX || overflowY == OPAGEDY) {
        pagination.mode = WebCore::paginationModeForRenderStyle(documentOrBodyRenderer->style());
        pagination.gap = static_cast<unsigned>(documentOrBodyRenderer->style().columnGap());
    }

    setPagination(pagination);
}

void FrameView::calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy strategy)
{
    m_viewportRendererType = ViewportRendererType::None;

    const HTMLFrameOwnerElement* owner = frame().ownerElement();
    if (owner && (owner->scrollingMode() == ScrollbarAlwaysOff)) {
        hMode = ScrollbarAlwaysOff;
        vMode = ScrollbarAlwaysOff;
        return;
    }  
    
    if (m_canHaveScrollbars || strategy == RulesFromWebContentOnly) {
        hMode = ScrollbarAuto;
        vMode = ScrollbarAuto;
    } else {
        hMode = ScrollbarAlwaysOff;
        vMode = ScrollbarAlwaysOff;
    }
    
    if (m_layoutRoot)
        return;
    
    auto* document = frame().document();
    if (!document)
        return;

    auto* documentElement = document->documentElement();
    if (!documentElement)
        return;

    auto* bodyOrFrameset = document->bodyOrFrameset();
    auto* rootRenderer = documentElement->renderer();
    if (!bodyOrFrameset || !bodyOrFrameset->renderer()) {
        if (rootRenderer) {
            applyOverflowToViewport(*rootRenderer, hMode, vMode);
            m_viewportRendererType = ViewportRendererType::Document;
        }
        return;
    }
    
    if (is<HTMLFrameSetElement>(*bodyOrFrameset) && !frameFlatteningEnabled()) {
        vMode = ScrollbarAlwaysOff;
        hMode = ScrollbarAlwaysOff;
        return;
    }

    if (is<HTMLBodyElement>(*bodyOrFrameset) && rootRenderer) {
        // It's sufficient to just check the X overflow,
        // since it's illegal to have visible in only one direction.
        if (rootRenderer->style().overflowX() == OVISIBLE && is<HTMLHtmlElement>(documentElement)) {
            auto* bodyRenderer = bodyOrFrameset->renderer();
            if (bodyRenderer) {
                applyOverflowToViewport(*bodyRenderer, hMode, vMode);
                m_viewportRendererType = ViewportRendererType::Body;
            }
        } else {
            applyOverflowToViewport(*rootRenderer, hMode, vMode);
            m_viewportRendererType = ViewportRendererType::Document;
        }
    }
}

void FrameView::willRecalcStyle()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    renderView->compositor().willRecalcStyle();
}

bool FrameView::updateCompositingLayersAfterStyleChange()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    // If we expect to update compositing after an incipient layout, don't do so here.
    if (inPreLayoutStyleUpdate() || layoutPending() || renderView->needsLayout())
        return false;

    return renderView->compositor().didRecalcStyleWithNoPendingLayout();
}

void FrameView::updateCompositingLayersAfterLayout()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    // This call will make sure the cached hasAcceleratedCompositing is updated from the pref
    renderView->compositor().cacheAcceleratedCompositingFlags();
    renderView->compositor().updateCompositingLayers(CompositingUpdateAfterLayout);
}

void FrameView::clearBackingStores()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    RenderLayerCompositor& compositor = renderView->compositor();
    ASSERT(compositor.inCompositingMode());
    compositor.enableCompositingMode(false);
    compositor.clearBackingForAllLayers();
}

void FrameView::restoreBackingStores()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    RenderLayerCompositor& compositor = renderView->compositor();
    compositor.enableCompositingMode(true);
    compositor.updateCompositingLayers(CompositingUpdateAfterLayout);
}

GraphicsLayer* FrameView::layerForScrolling() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().scrollLayer();
}

GraphicsLayer* FrameView::layerForHorizontalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForHorizontalScrollbar();
}

GraphicsLayer* FrameView::layerForVerticalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForVerticalScrollbar();
}

GraphicsLayer* FrameView::layerForScrollCorner() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForScrollCorner();
}

TiledBacking* FrameView::tiledBacking() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return nullptr;

    return backing->graphicsLayer()->tiledBacking();
}

uint64_t FrameView::scrollLayerID() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return 0;

    return backing->scrollingNodeIDForRole(Scrolling);
}

ScrollableArea* FrameView::scrollableAreaForScrollLayerID(uint64_t nodeID) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    return renderView->compositor().scrollableAreaForScrollLayerID(nodeID);
}

#if ENABLE(RUBBER_BANDING)
GraphicsLayer* FrameView::layerForOverhangAreas() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForOverhangAreas();
}

GraphicsLayer* FrameView::setWantsLayerForTopOverHangArea(bool wantsLayer) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    return renderView->compositor().updateLayerForTopOverhangArea(wantsLayer);
}

GraphicsLayer* FrameView::setWantsLayerForBottomOverHangArea(bool wantsLayer) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    return renderView->compositor().updateLayerForBottomOverhangArea(wantsLayer);
}

#endif // ENABLE(RUBBER_BANDING)

#if ENABLE(CSS_SCROLL_SNAP)
void FrameView::updateSnapOffsets()
{
    if (!frame().document())
        return;

    // FIXME: Should we allow specifying snap points through <html> tags too?
    HTMLElement* body = frame().document()->bodyOrFrameset();
    if (!renderView() || !body || !body->renderer())
        return;
    
    updateSnapOffsetsForScrollableArea(*this, *body, *renderView(), body->renderer()->style());
}

bool FrameView::isScrollSnapInProgress() const
{
    if (scrollbarsSuppressed())
        return false;
    
    // If the scrolling thread updates the scroll position for this FrameView, then we should return
    // ScrollingCoordinator::isScrollSnapInProgress().
    if (Page* page = frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator()) {
            if (!scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously(*this))
                return scrollingCoordinator->isScrollSnapInProgress();
        }
    }
    
    // If the main thread updates the scroll position for this FrameView, we should return
    // ScrollAnimator::isScrollSnapInProgress().
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isScrollSnapInProgress();
    
    return false;
}

void FrameView::updateScrollingCoordinatorScrollSnapProperties() const
{
    renderView()->compositor().updateScrollSnapPropertiesWithFrameView(*this);
}
#endif

bool FrameView::flushCompositingStateForThisFrame(const Frame& rootFrameForFlush)
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return true; // We don't want to keep trying to update layers if we have no renderer.

    ASSERT(frame().view() == this);

    // If we sync compositing layers when a layout is pending, we may cause painting of compositing
    // layer content to occur before layout has happened, which will cause paintContents() to bail.
    if (needsLayout())
        return false;

#if PLATFORM(IOS)
    if (LegacyTileCache* tileCache = legacyTileCache())
        tileCache->doPendingRepaints();
#endif

    renderView->compositor().flushPendingLayerChanges(&rootFrameForFlush == m_frame.ptr());
    return true;
}

void FrameView::setNeedsOneShotDrawingSynchronization()
{
    if (Page* page = frame().page())
        page->chrome().client().setNeedsOneShotDrawingSynchronization();
}

GraphicsLayer* FrameView::graphicsLayerForPlatformWidget(PlatformWidget platformWidget)
{
    // To find the Widget that corresponds with platformWidget we have to do a linear
    // search of our child widgets.
    Widget* foundWidget = nullptr;
    for (auto& widget : children()) {
        if (widget->platformWidget() != platformWidget)
            continue;
        foundWidget = widget.get();
        break;
    }

    if (!foundWidget)
        return nullptr;

    auto* renderWidget = RenderWidget::find(foundWidget);
    if (!renderWidget)
        return nullptr;

    RenderLayer* widgetLayer = renderWidget->layer();
    if (!widgetLayer || !widgetLayer->isComposited())
        return nullptr;

    return widgetLayer->backing()->parentForSublayers();
}

void FrameView::scheduleLayerFlushAllowingThrottling()
{
    RenderView* view = this->renderView();
    if (!view)
        return;
    view->compositor().scheduleLayerFlush(true /* canThrottle */);
}

LayoutRect FrameView::fixedScrollableAreaBoundsInflatedForScrolling(const LayoutRect& uninflatedBounds) const
{
    LayoutPoint scrollPosition = scrollPositionRespectingCustomFixedPosition();

    LayoutSize topLeftExpansion = scrollPosition - minimumScrollPosition();
    LayoutSize bottomRightExpansion = maximumScrollPosition() - scrollPosition;

    return LayoutRect(uninflatedBounds.location() - topLeftExpansion, uninflatedBounds.size() + topLeftExpansion + bottomRightExpansion);
}

LayoutPoint FrameView::scrollPositionRespectingCustomFixedPosition() const
{
#if PLATFORM(IOS)
    return useCustomFixedPositionLayoutRect() ? customFixedPositionLayoutRect().location() : scrollPosition();
#else
    return scrollPositionForFixedPosition();
#endif
}

void FrameView::setHeaderHeight(int headerHeight)
{
    if (frame().page())
        ASSERT(frame().isMainFrame());
    m_headerHeight = headerHeight;

    if (RenderView* renderView = this->renderView())
        renderView->setNeedsLayout();
}

void FrameView::setFooterHeight(int footerHeight)
{
    if (frame().page())
        ASSERT(frame().isMainFrame());
    m_footerHeight = footerHeight;

    if (RenderView* renderView = this->renderView())
        renderView->setNeedsLayout();
}

float FrameView::topContentInset(TopContentInsetType contentInsetTypeToReturn) const
{
    if (platformWidget() && contentInsetTypeToReturn == TopContentInsetType::WebCoreOrPlatformContentInset)
        return platformTopContentInset();

    if (!frame().isMainFrame())
        return 0;
    
    Page* page = frame().page();
    return page ? page->topContentInset() : 0;
}
    
void FrameView::topContentInsetDidChange(float newTopContentInset)
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    if (platformWidget())
        platformSetTopContentInset(newTopContentInset);
    
    layout();

    updateScrollbars(scrollPosition());
    if (renderView->usesCompositing())
        renderView->compositor().frameViewDidChangeSize();

    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setTopContentInset(newTopContentInset);
}

void FrameView::topContentDirectionDidChange()
{
    m_needsDeferredScrollbarsUpdate = true;
}

void FrameView::handleDeferredScrollbarsUpdateAfterDirectionChange()
{
    if (!m_needsDeferredScrollbarsUpdate)
        return;

    m_needsDeferredScrollbarsUpdate = false;

    ASSERT(m_layoutPhase == InPostLayerPositionsUpdatedAfterLayout);
    updateScrollbars(scrollPosition());
    positionScrollbarLayers();
}
    
bool FrameView::hasCompositedContent() const
{
    if (RenderView* renderView = this->renderView())
        return renderView->compositor().inCompositingMode();
    return false;
}

// Sometimes (for plug-ins) we need to eagerly go into compositing mode.
void FrameView::enterCompositingMode()
{
    if (RenderView* renderView = this->renderView()) {
        renderView->compositor().enableCompositingMode();
        if (!needsLayout())
            renderView->compositor().scheduleCompositingLayerUpdate();
    }
}

bool FrameView::isEnclosedInCompositingLayer() const
{
    auto frameOwnerRenderer = frame().ownerRenderer();
    if (frameOwnerRenderer && frameOwnerRenderer->containerForRepaint())
        return true;

    if (FrameView* parentView = parentFrameView())
        return parentView->isEnclosedInCompositingLayer();
    return false;
}

bool FrameView::flushCompositingStateIncludingSubframes()
{
    InspectorInstrumentation::willComposite(frame());

    bool allFramesFlushed = flushCompositingStateForThisFrame(frame());

    for (Frame* child = frame().tree().firstRenderedChild(); child; child = child->tree().traverseNextRendered(m_frame.ptr())) {
        if (!child->view())
            continue;
        bool flushed = child->view()->flushCompositingStateForThisFrame(frame());
        allFramesFlushed &= flushed;
    }
    return allFramesFlushed;
}

bool FrameView::isSoftwareRenderable() const
{
    RenderView* renderView = this->renderView();
    return !renderView || !renderView->compositor().has3DContent();
}

void FrameView::setIsInWindow(bool isInWindow)
{
    if (RenderView* renderView = this->renderView())
        renderView->setIsInWindow(isInWindow);
}

inline void FrameView::forceLayoutParentViewIfNeeded()
{
    RenderWidget* ownerRenderer = frame().ownerRenderer();
    if (!ownerRenderer)
        return;

    RenderBox* contentBox = embeddedContentBox();
    if (!contentBox)
        return;

    auto& svgRoot = downcast<RenderSVGRoot>(*contentBox);
    if (svgRoot.everHadLayout() && !svgRoot.needsLayout())
        return;

    LOG(Layout, "FrameView %p forceLayoutParentViewIfNeeded scheduling layout on parent FrameView %p", this, &ownerRenderer->view().frameView());

    // If the embedded SVG document appears the first time, the ownerRenderer has already finished
    // layout without knowing about the existence of the embedded SVG document, because RenderReplaced
    // embeddedContentBox() returns nullptr, as long as the embedded document isn't loaded yet. Before
    // bothering to lay out the SVG document, mark the ownerRenderer needing layout and ask its
    // FrameView for a layout. After that the RenderEmbeddedObject (ownerRenderer) carries the
    // correct size, which RenderSVGRoot::computeReplacedLogicalWidth/Height rely on, when laying
    // out for the first time, or when the RenderSVGRoot size has changed dynamically (eg. via <script>).

    ownerRenderer->setNeedsLayoutAndPrefWidthsRecalc();
    ownerRenderer->view().frameView().scheduleRelayout();
}

void FrameView::layout(bool allowSubtree)
{
    LOG(Layout, "FrameView %p (%dx%d) layout, main frameview %d, allowSubtree=%d", this, size().width(), size().height(), frame().isMainFrame(), allowSubtree);
    if (isInRenderTreeLayout()) {
        LOG(Layout, "  in layout, bailing");
        return;
    }

    if (layoutDisallowed()) {
        LOG(Layout, "  layout is disallowed, bailing");
        return;
    }

    // Protect the view from being deleted during layout (in recalcStyle).
    Ref<FrameView> protectedThis(*this);

    // Many of the tasks performed during layout can cause this function to be re-entered,
    // so save the layout phase now and restore it on exit.
    TemporaryChange<LayoutPhase> layoutPhaseRestorer(m_layoutPhase, InPreLayout);

    // Every scroll that happens during layout is programmatic.
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);

    bool inChildFrameLayoutWithFrameFlattening = isInChildFrameWithFrameFlattening();

    if (inChildFrameLayoutWithFrameFlattening) {
        startLayoutAtMainFrameViewIfNeeded(allowSubtree);
        RenderElement* root = m_layoutRoot ? m_layoutRoot : frame().document()->renderView();
        if (!root || !root->needsLayout())
            return;
    }
    
    TraceScope tracingScope(LayoutStart, LayoutEnd);

#if PLATFORM(IOS)
    if (updateFixedPositionLayoutRect())
        allowSubtree = false;
#endif

    m_layoutTimer.stop();
    m_delayedLayout = false;
    m_setNeedsLayoutWasDeferred = false;
    
    // we shouldn't enter layout() while painting
    ASSERT(!isPainting());
    if (isPainting())
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willLayout(frame());
    AnimationUpdateBlock animationUpdateBlock(&frame().animation());
    
    if (!allowSubtree && m_layoutRoot)
        convertSubtreeLayoutToFullLayout();

    ASSERT(frame().view() == this);
    ASSERT(frame().document());

    Document& document = *frame().document();
    ASSERT(!document.inPageCache());

    {
        TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

        if (!m_nestedLayoutCount && !m_inSynchronousPostLayout && m_postLayoutTasksTimer.isActive() && !inChildFrameLayoutWithFrameFlattening) {
            // This is a new top-level layout. If there are any remaining tasks from the previous
            // layout, finish them now.
            TemporaryChange<bool> inSynchronousPostLayoutChange(m_inSynchronousPostLayout, true);
            performPostLayoutTasks();
        }

        m_layoutPhase = InPreLayoutStyleUpdate;

        // Viewport-dependent media queries may cause us to need completely different style information.
        StyleResolver* styleResolver = document.styleResolverIfExists();
        if (!styleResolver || styleResolver->hasMediaQueriesAffectedByViewportChange()) {
            LOG(Layout, "  hasMediaQueriesAffectedByViewportChange, enqueueing style recalc");
            document.styleResolverChanged(DeferRecalcStyle);
            // FIXME: This instrumentation event is not strictly accurate since cached media query results do not persist across StyleResolver rebuilds.
            InspectorInstrumentation::mediaQueryResultChanged(document);
        } else
            document.evaluateMediaQueryList();

        // If there is any pagination to apply, it will affect the RenderView's style, so we should
        // take care of that now.
        applyPaginationToViewport();

        // Always ensure our style info is up-to-date. This can happen in situations where
        // the layout beats any sort of style recalc update that needs to occur.
        document.updateStyleIfNeeded();
        // If there is only one ref to this view left, then its going to be destroyed as soon as we exit,
        // so there's no point to continuing to layout
        if (hasOneRef())
            return;

        // Close block here so we can set up the font cache purge preventer, which we will still
        // want in scope even after we want m_layoutSchedulingEnabled to be restored again.
        // The next block sets m_layoutSchedulingEnabled back to false once again.
    }

    m_layoutPhase = InPreLayout;

    RenderLayer* layer = nullptr;
    bool subtree = false;
    RenderElement* root = nullptr;

    ++m_nestedLayoutCount;

    {
        TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

        autoSizeIfEnabled();

        root = m_layoutRoot ? m_layoutRoot : document.renderView();
        if (!root)
            return;
        subtree = m_layoutRoot;

        if (!m_layoutRoot) {
            auto* body = document.bodyOrFrameset();
            if (body && body->renderer()) {
                if (is<HTMLFrameSetElement>(*body) && !frameFlatteningEnabled()) {
                    body->renderer()->setChildNeedsLayout();
                } else if (is<HTMLBodyElement>(*body)) {
                    if (!m_firstLayout && m_size.height() != layoutHeight() && body->renderer()->enclosingBox().stretchesToViewport())
                        body->renderer()->setChildNeedsLayout();
                }
            }

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (m_firstLayout && !frame().ownerElement())
                printf("Elapsed time before first layout: %lld\n", document.elapsedTime().count());
#endif
        }

        m_needsFullRepaint = !subtree && (m_firstLayout || downcast<RenderView>(*root).printing());

        if (!subtree) {
            ScrollbarMode hMode;
            ScrollbarMode vMode;    
            calculateScrollbarModesForLayout(hMode, vMode);

            if (m_firstLayout || (hMode != horizontalScrollbarMode() || vMode != verticalScrollbarMode())) {
                if (m_firstLayout) {
                    setScrollbarsSuppressed(true);

                    m_firstLayout = false;
                    m_firstLayoutCallbackPending = true;
                    m_lastViewportSize = sizeForResizeEvent();
                    m_lastZoomFactor = root->style().zoom();

                    // Set the initial vMode to AlwaysOn if we're auto.
                    if (vMode == ScrollbarAuto)
                        setVerticalScrollbarMode(ScrollbarAlwaysOn); // This causes a vertical scrollbar to appear.
                    // Set the initial hMode to AlwaysOff if we're auto.
                    if (hMode == ScrollbarAuto)
                        setHorizontalScrollbarMode(ScrollbarAlwaysOff); // This causes a horizontal scrollbar to disappear.
                    Page* page = frame().page();
                    if (page && page->expectsWheelEventTriggers())
                        scrollAnimator().setWheelEventTestTrigger(page->testTrigger());
                    setScrollbarModes(hMode, vMode);
                    setScrollbarsSuppressed(false, true);
                } else
                    setScrollbarModes(hMode, vMode);
            }

            LayoutSize oldSize = m_size;
            m_size = layoutSize();

            if (oldSize != m_size) {
                LOG(Layout, "  layout size changed from %.3fx%.3f to %.3fx%.3f", oldSize.width().toFloat(), oldSize.height().toFloat(), m_size.width().toFloat(), m_size.height().toFloat());
                m_needsFullRepaint = true;
                if (!m_firstLayout) {
                    RenderBox* rootRenderer = document.documentElement() ? document.documentElement()->renderBox() : nullptr;
                    auto* body = document.bodyOrFrameset();
                    RenderBox* bodyRenderer = rootRenderer && body ? body->renderBox() : nullptr;
                    if (bodyRenderer && bodyRenderer->stretchesToViewport())
                        bodyRenderer->setChildNeedsLayout();
                    else if (rootRenderer && rootRenderer->stretchesToViewport())
                        rootRenderer->setChildNeedsLayout();
                }
            }

            m_layoutPhase = InPreLayout;
        }

        layer = root->enclosingLayer();
        SubtreeLayoutStateMaintainer subtreeLayoutStateMaintainer(m_layoutRoot);

        RenderView::RepaintRegionAccumulator repaintRegionAccumulator(&root->view());

        ASSERT(m_layoutPhase == InPreLayout);
        m_layoutPhase = InRenderTreeLayout;

        forceLayoutParentViewIfNeeded();

        ASSERT(m_layoutPhase == InRenderTreeLayout);

        root->layout();

#if ENABLE(IOS_TEXT_AUTOSIZING)
        if (frame().settings().textAutosizingEnabled() && !root->view().printing()) {
            float minimumZoomFontSize = frame().settings().minimumZoomFontSize();
            float textAutosizingWidth = frame().page() ? frame().page()->textAutosizingWidth() : 0;
            if (int overrideWidth = frame().settings().textAutosizingWindowSizeOverride().width())
                textAutosizingWidth = overrideWidth;

            LOG(TextAutosizing, "Text Autosizing: minimumZoomFontSize=%.2f textAutosizingWidth=%.2f", minimumZoomFontSize, textAutosizingWidth);
            
            if (minimumZoomFontSize && textAutosizingWidth) {
                root->adjustComputedFontSizesOnBlocks(minimumZoomFontSize, textAutosizingWidth);
                if (root->needsLayout())
                    root->layout();
            }
        }
#endif
#if ENABLE(TEXT_AUTOSIZING)
        if (document.textAutosizer()->processSubtree(root) && root->needsLayout())
            root->layout();
#endif

        ASSERT(m_layoutPhase == InRenderTreeLayout);
        m_layoutRoot = nullptr;
        // Close block here to end the scope of changeSchedulingEnabled and SubtreeLayoutStateMaintainer.
    }

    m_layoutPhase = InViewSizeAdjust;

    bool neededFullRepaint = m_needsFullRepaint;

    if (!subtree && !downcast<RenderView>(*root).printing())
        adjustViewSize();

    m_layoutPhase = InPostLayout;

    m_needsFullRepaint = neededFullRepaint;

    // Now update the positions of all layers.
    if (m_needsFullRepaint)
        root->view().repaintRootContents();

    root->view().releaseProtectedRenderWidgets();

    ASSERT(!root->needsLayout());

    layer->updateLayerPositionsAfterLayout(renderView()->layer(), updateLayerPositionFlags(layer, subtree, m_needsFullRepaint));

    updateCompositingLayersAfterLayout();

    m_layoutPhase = InPostLayerPositionsUpdatedAfterLayout;

    m_layoutCount++;

#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK) || PLATFORM(EFL)
    if (AXObjectCache* cache = root->document().existingAXObjectCache())
        cache->postNotification(root, AXObjectCache::AXLayoutComplete);
#endif

#if ENABLE(DASHBOARD_SUPPORT)
    updateAnnotatedRegions();
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    document.dirtyTouchEventRects();
#endif

    updateCanBlitOnScrollRecursively();

    handleDeferredScrollUpdateAfterContentSizeChange();

    handleDeferredScrollbarsUpdateAfterDirectionChange();

    if (document.hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(layoutWidth() < contentsWidth(), layoutHeight() < contentsHeight());

    frame().document()->markers().invalidateRectsForAllMarkers();

    if (!m_postLayoutTasksTimer.isActive()) {
        if (!m_inSynchronousPostLayout) {
            if (inChildFrameLayoutWithFrameFlattening)
                updateWidgetPositions();
            else {
                TemporaryChange<bool> inSynchronousPostLayoutChange(m_inSynchronousPostLayout, true);
                performPostLayoutTasks(); // Calls resumeScheduledEvents().
            }
        }

        if (!m_postLayoutTasksTimer.isActive() && (needsLayout() || m_inSynchronousPostLayout || inChildFrameLayoutWithFrameFlattening)) {
            // If we need layout or are already in a synchronous call to postLayoutTasks(), 
            // defer widget updates and event dispatch until after we return. postLayoutTasks()
            // can make us need to update again, and we can get stuck in a nasty cycle unless
            // we call it through the timer here.
            m_postLayoutTasksTimer.startOneShot(0);
            if (needsLayout())
                layout();
        }
    }

    InspectorInstrumentation::didLayout(cookie, root);
    DebugPageOverlays::didLayout(frame());

    --m_nestedLayoutCount;
}

bool FrameView::shouldDeferScrollUpdateAfterContentSizeChange()
{
    return (m_layoutPhase < InPostLayout) && (m_layoutPhase != OutsideLayout);
}

RenderBox* FrameView::embeddedContentBox() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    RenderObject* firstChild = renderView->firstChild();

    // Curently only embedded SVG documents participate in the size-negotiation logic.
    if (is<RenderSVGRoot>(firstChild))
        return downcast<RenderSVGRoot>(firstChild);

    return nullptr;
}

void FrameView::addEmbeddedObjectToUpdate(RenderEmbeddedObject& embeddedObject)
{
    if (!m_embeddedObjectsToUpdate)
        m_embeddedObjectsToUpdate = std::make_unique<ListHashSet<RenderEmbeddedObject*>>();

    HTMLFrameOwnerElement& element = embeddedObject.frameOwnerElement();
    if (is<HTMLObjectElement>(element) || is<HTMLEmbedElement>(element)) {
        // Tell the DOM element that it needs a widget update.
        HTMLPlugInImageElement& pluginElement = downcast<HTMLPlugInImageElement>(element);
        if (!pluginElement.needsCheckForSizeChange())
            pluginElement.setNeedsWidgetUpdate(true);
    }

    m_embeddedObjectsToUpdate->add(&embeddedObject);
}

void FrameView::removeEmbeddedObjectToUpdate(RenderEmbeddedObject& embeddedObject)
{
    if (!m_embeddedObjectsToUpdate)
        return;

    m_embeddedObjectsToUpdate->remove(&embeddedObject);
}

void FrameView::setMediaType(const String& mediaType)
{
    m_mediaType = mediaType;
}

String FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType = frame().loader().client().overrideMediaType();
    InspectorInstrumentation::applyEmulatedMedia(frame(), overrideType);
    if (!overrideType.isNull())
        return overrideType;
    return m_mediaType;
}

void FrameView::adjustMediaTypeForPrinting(bool printing)
{
    if (printing) {
        if (m_mediaTypeWhenNotPrinting.isNull())
            m_mediaTypeWhenNotPrinting = mediaType();
        setMediaType("print");
    } else {
        if (!m_mediaTypeWhenNotPrinting.isNull())
            setMediaType(m_mediaTypeWhenNotPrinting);
        m_mediaTypeWhenNotPrinting = String();
    }
}

bool FrameView::useSlowRepaints(bool considerOverlap) const
{
    bool mustBeSlow = hasSlowRepaintObjects() || (platformWidget() && hasViewportConstrainedObjects());

    // FIXME: WidgetMac.mm makes the assumption that useSlowRepaints ==
    // m_contentIsOpaque, so don't take the fast path for composited layers
    // if they are a platform widget in order to get painting correctness
    // for transparent layers. See the comment in WidgetMac::paint.
    if (usesCompositedScrolling() && !platformWidget())
        return mustBeSlow;

    bool isOverlapped = m_isOverlapped && considerOverlap;

    if (mustBeSlow || m_cannotBlitToWindow || isOverlapped || !m_contentIsOpaque)
        return true;

    if (FrameView* parentView = parentFrameView())
        return parentView->useSlowRepaints(considerOverlap);

    return false;
}

bool FrameView::useSlowRepaintsIfNotOverlapped() const
{
    return useSlowRepaints(false);
}

void FrameView::updateCanBlitOnScrollRecursively()
{
    for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        if (FrameView* view = frame->view())
            view->setCanBlitOnScroll(!view->useSlowRepaints());
    }
}

bool FrameView::usesCompositedScrolling() const
{
    RenderView* renderView = this->renderView();
    if (renderView && renderView->isComposited()) {
        GraphicsLayer* layer = renderView->layer()->backing()->graphicsLayer();
        if (layer && layer->drawsContent())
            return true;
    }

    return false;
}

bool FrameView::usesAsyncScrolling() const
{
#if ENABLE(ASYNC_SCROLLING)
    if (Page* page = frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            return scrollingCoordinator->coordinatesScrollingForFrameView(*this);
    }
#endif
    return false;
}

bool FrameView::usesMockScrollAnimator() const
{
    return Settings::usesMockScrollAnimator();
}

void FrameView::logMockScrollAnimatorMessage(const String& message) const
{
    Document* document = frame().document();
    if (!document)
        return;
    StringBuilder builder;
    if (frame().isMainFrame())
        builder.appendLiteral("Main");
    builder.appendLiteral("FrameView: ");
    builder.append(message);
    document->addConsoleMessage(MessageSource::Other, MessageLevel::Debug, builder.toString());
}

void FrameView::setCannotBlitToWindow()
{
    m_cannotBlitToWindow = true;
    updateCanBlitOnScrollRecursively();
}

void FrameView::addSlowRepaintObject(RenderElement* o)
{
    bool hadSlowRepaintObjects = hasSlowRepaintObjects();

    if (!m_slowRepaintObjects)
        m_slowRepaintObjects = std::make_unique<HashSet<const RenderElement*>>();

    m_slowRepaintObjects->add(o);

    if (!hadSlowRepaintObjects) {
        updateCanBlitOnScrollRecursively();

        if (Page* page = frame().page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(*this);
        }
    }
}

void FrameView::removeSlowRepaintObject(RenderElement* o)
{
    if (!m_slowRepaintObjects)
        return;

    m_slowRepaintObjects->remove(o);
    if (m_slowRepaintObjects->isEmpty()) {
        m_slowRepaintObjects = nullptr;
        updateCanBlitOnScrollRecursively();

        if (Page* page = frame().page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(*this);
        }
    }
}

void FrameView::addViewportConstrainedObject(RenderElement* object)
{
    if (!m_viewportConstrainedObjects)
        m_viewportConstrainedObjects = std::make_unique<ViewportConstrainedObjectSet>();

    if (!m_viewportConstrainedObjects->contains(object)) {
        m_viewportConstrainedObjects->add(object);
        if (platformWidget())
            updateCanBlitOnScrollRecursively();

        if (Page* page = frame().page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(*this);
        }
    }
}

void FrameView::removeViewportConstrainedObject(RenderElement* object)
{
    if (m_viewportConstrainedObjects && m_viewportConstrainedObjects->remove(object)) {
        if (Page* page = frame().page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(*this);
        }

        // FIXME: In addFixedObject() we only call this if there's a platform widget,
        // why isn't the same check being made here?
        updateCanBlitOnScrollRecursively();
    }
}

LayoutRect FrameView::viewportConstrainedVisibleContentRect() const
{
#if PLATFORM(IOS)
    if (useCustomFixedPositionLayoutRect())
        return customFixedPositionLayoutRect();
#endif
    LayoutRect viewportRect = visibleContentRect();

    viewportRect.setLocation(scrollPositionForFixedPosition());
    return viewportRect;
}

float FrameView::frameScaleFactor() const
{
    return frame().frameScaleFactor();
}

LayoutPoint FrameView::scrollPositionForFixedPosition(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements behaviorForFixed, int headerHeight, int footerHeight)
{
    LayoutPoint position;
    if (behaviorForFixed == StickToDocumentBounds)
        position = ScrollableArea::constrainScrollPositionForOverhang(visibleContentRect, totalContentsSize, scrollPosition, scrollOrigin, headerHeight, footerHeight);
    else {
        position = scrollPosition;
        position.setY(position.y() - headerHeight);
    }

    LayoutSize maxSize = totalContentsSize - visibleContentRect.size();

    float dragFactorX = (fixedElementsLayoutRelativeToFrame || !maxSize.width()) ? 1 : (totalContentsSize.width() - visibleContentRect.width() * frameScaleFactor) / maxSize.width();
    float dragFactorY = (fixedElementsLayoutRelativeToFrame || !maxSize.height()) ? 1 : (totalContentsSize.height() - visibleContentRect.height() * frameScaleFactor) / maxSize.height();

    return LayoutPoint(position.x() * dragFactorX / frameScaleFactor, position.y() * dragFactorY / frameScaleFactor);
}

float FrameView::yPositionForInsetClipLayer(const FloatPoint& scrollPosition, float topContentInset)
{
    if (!topContentInset)
        return 0;

    // The insetClipLayer should not move for negative scroll values.
    float scrollY = std::max<float>(0, scrollPosition.y());

    if (scrollY >= topContentInset)
        return 0;

    return topContentInset - scrollY;
}

float FrameView::yPositionForHeaderLayer(const FloatPoint& scrollPosition, float topContentInset)
{
    if (!topContentInset)
        return 0;

    float scrollY = std::max<float>(0, scrollPosition.y());

    if (scrollY >= topContentInset)
        return topContentInset;

    return scrollY;
}

float FrameView::yPositionForFooterLayer(const FloatPoint& scrollPosition, float topContentInset, float totalContentsHeight, float footerHeight)
{
    return yPositionForHeaderLayer(scrollPosition, topContentInset) + totalContentsHeight - footerHeight;
}

FloatPoint FrameView::positionForRootContentLayer(const FloatPoint& scrollPosition, const FloatPoint& scrollOrigin, float topContentInset, float headerHeight)
{
    return FloatPoint(0, yPositionForHeaderLayer(scrollPosition, topContentInset) + headerHeight) - toFloatSize(scrollOrigin);
}

FloatPoint FrameView::positionForRootContentLayer() const
{
    return positionForRootContentLayer(scrollPosition(), scrollOrigin(), topContentInset(), headerHeight());
}

#if PLATFORM(IOS)
LayoutRect FrameView::rectForViewportConstrainedObjects(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements scrollBehavior)
{
    if (fixedElementsLayoutRelativeToFrame)
        return visibleContentRect;
    
    if (totalContentsSize.isEmpty())
        return visibleContentRect;

    // We impose an lower limit on the size (so an upper limit on the scale) of
    // the rect used to position fixed objects so that they don't crowd into the
    // center of the screen at larger scales.
    const LayoutUnit maxContentWidthForZoomThreshold = LayoutUnit::fromPixel(1024);
    float zoomedOutScale = frameScaleFactor * visibleContentRect.width() / std::min(maxContentWidthForZoomThreshold, totalContentsSize.width());
    float constraintThresholdScale = 1.5 * zoomedOutScale;
    float maxPostionedObjectsRectScale = std::min(frameScaleFactor, constraintThresholdScale);

    LayoutRect viewportConstrainedObjectsRect = visibleContentRect;

    if (frameScaleFactor > constraintThresholdScale) {
        FloatRect contentRect(FloatPoint(), totalContentsSize);
        FloatRect viewportRect = visibleContentRect;
        
        // Scale the rect up from a point that is relative to its position in the viewport.
        FloatSize sizeDelta = contentRect.size() - viewportRect.size();

        FloatPoint scaleOrigin;
        scaleOrigin.setX(contentRect.x() + sizeDelta.width() > 0 ? contentRect.width() * (viewportRect.x() - contentRect.x()) / sizeDelta.width() : 0);
        scaleOrigin.setY(contentRect.y() + sizeDelta.height() > 0 ? contentRect.height() * (viewportRect.y() - contentRect.y()) / sizeDelta.height() : 0);
        
        AffineTransform rescaleTransform = AffineTransform::translation(scaleOrigin.x(), scaleOrigin.y());
        rescaleTransform.scale(frameScaleFactor / maxPostionedObjectsRectScale, frameScaleFactor / maxPostionedObjectsRectScale);
        rescaleTransform = CGAffineTransformTranslate(rescaleTransform, -scaleOrigin.x(), -scaleOrigin.y());

        viewportConstrainedObjectsRect = enclosingLayoutRect(rescaleTransform.mapRect(visibleContentRect));
    }
    
    if (scrollBehavior == StickToDocumentBounds) {
        LayoutRect documentBounds(LayoutPoint(), totalContentsSize);
        viewportConstrainedObjectsRect.intersect(documentBounds);
    }

    return viewportConstrainedObjectsRect;
}
    
LayoutRect FrameView::viewportConstrainedObjectsRect() const
{
    return rectForViewportConstrainedObjects(visibleContentRect(), totalContentsSize(), frame().frameScaleFactor(), fixedElementsLayoutRelativeToFrame(), scrollBehaviorForFixedElements());
}
#endif
    
ScrollPosition FrameView::minimumScrollPosition() const
{
    ScrollPosition minimumPosition = ScrollView::minimumScrollPosition();

    if (frame().isMainFrame() && m_scrollPinningBehavior == PinToBottom)
        minimumPosition.setY(maximumScrollPosition().y());
    
    return minimumPosition;
}

ScrollPosition FrameView::maximumScrollPosition() const
{
    ScrollPosition maximumPosition = ScrollView::maximumScrollPosition();

    if (frame().isMainFrame() && m_scrollPinningBehavior == PinToTop)
        maximumPosition.setY(minimumScrollPosition().y());
    
    return maximumPosition;
}

void FrameView::viewportContentsChanged()
{
    if (!frame().view()) {
        // The frame is being destroyed.
        return;
    }

    // When the viewport contents changes (scroll, resize, style recalc, layout, ...),
    // check if we should resume animated images or unthrottle DOM timers.
    applyRecursivelyWithVisibleRect([] (FrameView& frameView, const IntRect& visibleRect) {
        frameView.resumeVisibleImageAnimations(visibleRect);
        frameView.updateScriptedAnimationsAndTimersThrottlingState(visibleRect);

        if (auto* renderView = frameView.frame().contentRenderer())
            renderView->updateVisibleViewportRect(visibleRect);
    });
}

bool FrameView::fixedElementsLayoutRelativeToFrame() const
{
    return frame().settings().fixedElementsLayoutRelativeToFrame();
}

IntPoint FrameView::lastKnownMousePosition() const
{
    return frame().eventHandler().lastKnownMousePosition();
}

bool FrameView::isHandlingWheelEvent() const
{
    return frame().eventHandler().isHandlingWheelEvent();
}

bool FrameView::shouldSetCursor() const
{
    Page* page = frame().page();
    return page && page->isVisible() && page->focusController().isActive();
}

bool FrameView::scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    if (!m_viewportConstrainedObjects || m_viewportConstrainedObjects->isEmpty()) {
        hostWindow()->scroll(scrollDelta, rectToScroll, clipRect);
        return true;
    }

    const bool isCompositedContentLayer = usesCompositedScrolling();

    // Get the rects of the fixed objects visible in the rectToScroll
    Region regionToUpdate;
    for (auto& renderer : *m_viewportConstrainedObjects) {
        if (!renderer->style().hasViewportConstrainedPosition())
            continue;
        if (renderer->isComposited())
            continue;

        // Fixed items should always have layers.
        ASSERT(renderer->hasLayer());
        RenderLayer* layer = downcast<RenderBoxModelObject>(*renderer).layer();

        if (layer->viewportConstrainedNotCompositedReason() == RenderLayer::NotCompositedForBoundsOutOfView
            || layer->viewportConstrainedNotCompositedReason() == RenderLayer::NotCompositedForNoVisibleContent) {
            // Don't invalidate for invisible fixed layers.
            continue;
        }

        if (layer->hasAncestorWithFilterOutsets()) {
            // If the fixed layer has a blur/drop-shadow filter applied on at least one of its parents, we cannot 
            // scroll using the fast path, otherwise the outsets of the filter will be moved around the page.
            return false;
        }

        // FIXME: use pixel snapping instead of enclosing when ScrollView has finished transitioning from IntRect to Float/LayoutRect.
        IntRect updateRect = enclosingIntRect(layer->repaintRectIncludingNonCompositingDescendants());
        updateRect = contentsToRootView(updateRect);
        if (!isCompositedContentLayer && clipsRepaints())
            updateRect.intersect(rectToScroll);
        if (!updateRect.isEmpty())
            regionToUpdate.unite(updateRect);
    }

    // 1) scroll
    hostWindow()->scroll(scrollDelta, rectToScroll, clipRect);

    // 2) update the area of fixed objects that has been invalidated
    for (auto& updateRect : regionToUpdate.rects()) {
        IntRect scrolledRect = updateRect;
        scrolledRect.move(scrollDelta);
        updateRect.unite(scrolledRect);
        if (isCompositedContentLayer) {
            updateRect = rootViewToContents(updateRect);
            ASSERT(renderView());
            renderView()->layer()->setBackingNeedsRepaintInRect(updateRect);
            continue;
        }
        if (clipsRepaints())
            updateRect.intersect(rectToScroll);
        hostWindow()->invalidateContentsAndRootView(updateRect);
    }

    return true;
}

void FrameView::scrollContentsSlowPath(const IntRect& updateRect)
{
    repaintSlowRepaintObjects();

    if (!usesCompositedScrolling() && isEnclosedInCompositingLayer()) {
        if (RenderWidget* frameRenderer = frame().ownerRenderer()) {
            LayoutRect rect(frameRenderer->borderLeft() + frameRenderer->paddingLeft(), frameRenderer->borderTop() + frameRenderer->paddingTop(),
                visibleWidth(), visibleHeight());
            frameRenderer->repaintRectangle(rect);
            return;
        }
    }

    ScrollView::scrollContentsSlowPath(updateRect);
}

void FrameView::repaintSlowRepaintObjects()
{
    if (!m_slowRepaintObjects)
        return;

    // Renderers with fixed backgrounds may be in compositing layers, so we need to explicitly
    // repaint them after scrolling.
    for (auto& renderer : *m_slowRepaintObjects)
        renderer->repaintSlowRepaintObject();
}

// Note that this gets called at painting time.
void FrameView::setIsOverlapped(bool isOverlapped)
{
    if (isOverlapped == m_isOverlapped)
        return;

    m_isOverlapped = isOverlapped;
    updateCanBlitOnScrollRecursively();
}

bool FrameView::isOverlappedIncludingAncestors() const
{
    if (isOverlapped())
        return true;

    if (FrameView* parentView = parentFrameView()) {
        if (parentView->isOverlapped())
            return true;
    }

    return false;
}

void FrameView::setContentIsOpaque(bool contentIsOpaque)
{
    if (contentIsOpaque == m_contentIsOpaque)
        return;

    m_contentIsOpaque = contentIsOpaque;
    updateCanBlitOnScrollRecursively();
}

void FrameView::restoreScrollbar()
{
    setScrollbarsSuppressed(false);
}

bool FrameView::scrollToFragment(const URL& url)
{
    // If our URL has no ref, then we have no place we need to jump to.
    // OTOH If CSS target was set previously, we want to set it to 0, recalc
    // and possibly repaint because :target pseudo class may have been
    // set (see bug 11321).
    if (!url.hasFragmentIdentifier() && !frame().document()->cssTarget())
        return false;

    String fragmentIdentifier = url.fragmentIdentifier();
    if (scrollToAnchor(fragmentIdentifier))
        return true;

    // Try again after decoding the ref, based on the document's encoding.
    if (TextResourceDecoder* decoder = frame().document()->decoder())
        return scrollToAnchor(decodeURLEscapeSequences(fragmentIdentifier, decoder->encoding()));

    return false;
}

bool FrameView::scrollToAnchor(const String& name)
{
    ASSERT(frame().document());
    auto& document = *frame().document();

    if (!document.haveStylesheetsLoaded()) {
        document.setGotoAnchorNeededAfterStylesheetsLoad(true);
        return false;
    }

    document.setGotoAnchorNeededAfterStylesheetsLoad(false);

    Element* anchorElement = document.findAnchor(name);

    // Setting to null will clear the current target.
    document.setCSSTarget(anchorElement);

    if (is<SVGDocument>(document)) {
        if (auto* rootElement = SVGDocument::rootElement(document)) {
            rootElement->scrollToAnchor(name, anchorElement);
            if (!anchorElement)
                return true;
        }
    }
  
    // Implement the rule that "" and "top" both mean top of page as in other browsers.
    if (!anchorElement && !(name.isEmpty() || equalLettersIgnoringASCIICase(name, "top")))
        return false;

    ContainerNode* scrollPositionAnchor = anchorElement;
    if (!scrollPositionAnchor)
        scrollPositionAnchor = frame().document();
    maintainScrollPositionAtAnchor(scrollPositionAnchor);
    
    // If the anchor accepts keyboard focus, move focus there to aid users relying on keyboard navigation.
    if (anchorElement) {
        if (anchorElement->isFocusable())
            document.setFocusedElement(anchorElement);
        else {
            document.setFocusedElement(nullptr);
            document.setFocusNavigationStartingNode(anchorElement);
        }
    }
    
    return true;
}

void FrameView::maintainScrollPositionAtAnchor(ContainerNode* anchorNode)
{
    m_maintainScrollPositionAnchor = anchorNode;
    if (!m_maintainScrollPositionAnchor)
        return;

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    frame().document()->updateStyleIfNeeded();
    // Only do a layout if changes have occurred that make it necessary.
    RenderView* renderView = this->renderView();
    if (renderView && renderView->needsLayout())
        layout();
    else
        scrollToAnchor();
}

void FrameView::scrollElementToRect(const Element& element, const IntRect& rect)
{
    frame().document()->updateLayoutIgnorePendingStylesheets();

    LayoutRect bounds;
    if (RenderElement* renderer = element.renderer())
        bounds = renderer->anchorRect();
    int centeringOffsetX = (rect.width() - bounds.width()) / 2;
    int centeringOffsetY = (rect.height() - bounds.height()) / 2;
    setScrollPosition(IntPoint(bounds.x() - centeringOffsetX - rect.x(), bounds.y() - centeringOffsetY - rect.y()));
}

void FrameView::setScrollPosition(const ScrollPosition& scrollPosition)
{
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);
    m_maintainScrollPositionAnchor = nullptr;
    Page* page = frame().page();
    if (page && page->expectsWheelEventTriggers())
        scrollAnimator().setWheelEventTestTrigger(page->testTrigger());
    ScrollView::setScrollPosition(scrollPosition);
}

void FrameView::contentsResized()
{
    // For non-delegated scrolling, adjustTiledBackingScrollability() is called via addedOrRemovedScrollbar() which occurs less often.
    if (delegatesScrolling())
        adjustTiledBackingScrollability();
}

void FrameView::delegatesScrollingDidChange()
{
    // When we switch to delgatesScrolling mode, we should destroy the scrolling/clipping layers in RenderLayerCompositor.
    if (hasCompositedContent())
        clearBackingStores();
}

#if USE(COORDINATED_GRAPHICS)
void FrameView::setFixedVisibleContentRect(const IntRect& visibleContentRect)
{
    bool visibleContentSizeDidChange = false;
    if (visibleContentRect.size() != this->fixedVisibleContentRect().size()) {
        // When the viewport size changes or the content is scaled, we need to
        // reposition the fixed and sticky positioned elements.
        setViewportConstrainedObjectsNeedLayout();
        visibleContentSizeDidChange = true;
    }

    IntPoint oldPosition = scrollPosition();
    ScrollView::setFixedVisibleContentRect(visibleContentRect);
    IntPoint newPosition = scrollPosition();
    if (oldPosition != newPosition) {
        updateLayerPositionsAfterScrolling();
        if (frame().settings().acceleratedCompositingForFixedPositionEnabled())
            updateCompositingLayersAfterScrolling();
        scrollAnimator().setCurrentPosition(newPosition);
        scrollPositionChanged(oldPosition, newPosition);
    }
    if (visibleContentSizeDidChange) {
        // Update the scroll-bars to calculate new page-step size.
        updateScrollbars(scrollPosition());
    }
    didChangeScrollOffset();
}
#endif

void FrameView::setViewportConstrainedObjectsNeedLayout()
{
    if (!hasViewportConstrainedObjects())
        return;

    for (auto& renderer : *m_viewportConstrainedObjects)
        renderer->setNeedsLayout();
}

void FrameView::didChangeScrollOffset()
{
    frame().mainFrame().pageOverlayController().didScrollFrame(frame());
    frame().loader().client().didChangeScrollOffset();
}

void FrameView::scrollOffsetChangedViaPlatformWidgetImpl(const ScrollOffset& oldOffset, const ScrollOffset& newOffset)
{
    updateLayerPositionsAfterScrolling();
    updateCompositingLayersAfterScrolling();
    repaintSlowRepaintObjects();
    scrollPositionChanged(scrollPositionFromOffset(oldOffset), scrollPositionFromOffset(newOffset));
}

void FrameView::scrollPositionChanged(const ScrollPosition& oldPosition, const ScrollPosition& newPosition)
{
    Page* page = frame().page();
    auto throttlingDelay = page ? page->chrome().client().eventThrottlingDelay() : 0ms;

    if (throttlingDelay == 0ms) {
        m_delayedScrollEventTimer.stop();
        sendScrollEvent();
    } else if (!m_delayedScrollEventTimer.isActive())
        m_delayedScrollEventTimer.startOneShot(throttlingDelay);

    if (Document* document = frame().document())
        document->sendWillRevealEdgeEventsIfNeeded(oldPosition, newPosition, visibleContentRect(), contentsSize());

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidScroll();
    }

    viewportContentsChanged();
}

void FrameView::applyRecursivelyWithVisibleRect(const std::function<void (FrameView& frameView, const IntRect& visibleRect)>& apply)
{
    IntRect windowClipRect = this->windowClipRect();
    auto visibleRect = windowToContents(windowClipRect);
    apply(*this, visibleRect);

    // Recursive call for subframes. We cache the current FrameView's windowClipRect to avoid recomputing it for every subframe.
    TemporaryChange<IntRect*> windowClipRectCache(m_cachedWindowClipRect, &windowClipRect);
    for (Frame* childFrame = frame().tree().firstChild(); childFrame; childFrame = childFrame->tree().nextSibling()) {
        if (auto* childView = childFrame->view())
            childView->applyRecursivelyWithVisibleRect(apply);
    }
}

void FrameView::resumeVisibleImageAnimations(const IntRect& visibleRect)
{
    if (visibleRect.isEmpty())
        return;

    if (auto* renderView = frame().contentRenderer())
        renderView->resumePausedImageAnimationsIfNeeded(visibleRect);
}

void FrameView::updateScriptedAnimationsAndTimersThrottlingState(const IntRect& visibleRect)
{
    if (frame().isMainFrame())
        return;

    auto* document = frame().document();
    if (!document)
        return;

    // We don't throttle zero-size or display:none frames because those are usually utility frames.
    bool shouldThrottle = visibleRect.isEmpty() && !m_size.isEmpty() && frame().ownerRenderer();

#if ENABLE(REQUEST_ANIMATION_FRAME)
    if (auto* scriptedAnimationController = document->scriptedAnimationController())
        scriptedAnimationController->setThrottled(shouldThrottle);
#endif

    document->setTimerThrottlingEnabled(shouldThrottle);
}


void FrameView::resumeVisibleImageAnimationsIncludingSubframes()
{
    applyRecursivelyWithVisibleRect([] (FrameView& frameView, const IntRect& visibleRect) {
        frameView.resumeVisibleImageAnimations(visibleRect);
    });
}

void FrameView::updateLayerPositionsAfterScrolling()
{
    // If we're scrolling as a result of updating the view size after layout, we'll update widgets and layer positions soon anyway.
    if (m_layoutPhase == InViewSizeAdjust)
        return;

    if (m_nestedLayoutCount <= 1 && hasViewportConstrainedObjects()) {
        if (RenderView* renderView = this->renderView()) {
            updateWidgetPositions();
            renderView->layer()->updateLayerPositionsAfterDocumentScroll();
        }
    }
}

bool FrameView::shouldUpdateCompositingLayersAfterScrolling() const
{
#if ENABLE(ASYNC_SCROLLING)
    // If the scrolling thread is updating the fixed elements, then the FrameView should not update them as well.

    Page* page = frame().page();
    if (!page)
        return true;

    if (&page->mainFrame() != m_frame.ptr())
        return true;

    ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return true;

    if (!scrollingCoordinator->supportsFixedPositionLayers())
        return true;

    if (scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously(*this))
        return true;

    if (inProgrammaticScroll())
        return true;

    return false;
#endif
    return true;
}

void FrameView::updateCompositingLayersAfterScrolling()
{
    ASSERT(m_layoutPhase >= InPostLayout || m_layoutPhase == OutsideLayout);

    if (!shouldUpdateCompositingLayersAfterScrolling())
        return;

    if (m_nestedLayoutCount <= 1 && hasViewportConstrainedObjects()) {
        if (RenderView* renderView = this->renderView())
            renderView->compositor().updateCompositingLayers(CompositingUpdateOnScroll);
    }
}

bool FrameView::isRubberBandInProgress() const
{
    if (scrollbarsSuppressed())
        return false;

    // If the scrolling thread updates the scroll position for this FrameView, then we should return
    // ScrollingCoordinator::isRubberBandInProgress().
    if (Page* page = frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator()) {
            if (!scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously(*this))
                return scrollingCoordinator->isRubberBandInProgress();
        }
    }

    // If the main thread updates the scroll position for this FrameView, we should return
    // ScrollAnimator::isRubberBandInProgress().
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();

    return false;
}

bool FrameView::requestScrollPositionUpdate(const ScrollPosition& position)
{
    LOG_WITH_STREAM(Scrolling, stream << "FrameView::requestScrollPositionUpdate " << position);

#if ENABLE(ASYNC_SCROLLING)
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->prepopulateRect(FloatRect(position, visibleContentRect().size()));
#endif

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
    if (Page* page = frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            return scrollingCoordinator->requestScrollPositionUpdate(*this, position);
    }
#else
    UNUSED_PARAM(position);
#endif

    return false;
}

HostWindow* FrameView::hostWindow() const
{
    if (Page* page = frame().page())
        return &page->chrome();
    return nullptr;
}

void FrameView::addTrackedRepaintRect(const FloatRect& r)
{
    if (!m_isTrackingRepaints || r.isEmpty())
        return;

    FloatRect repaintRect = r;
    repaintRect.moveBy(-scrollPosition());
    m_trackedRepaintRects.append(repaintRect);
}

void FrameView::repaintContentRectangle(const IntRect& r)
{
    ASSERT(!frame().ownerElement());

    if (!shouldUpdate())
        return;

    ScrollView::repaintContentRectangle(r);
}

static unsigned countRenderedCharactersInRenderObjectWithThreshold(const RenderElement& renderer, unsigned threshold)
{
    unsigned count = 0;
    for (const RenderObject* descendant = &renderer; descendant; descendant = descendant->nextInPreOrder()) {
        if (is<RenderText>(*descendant)) {
            count += downcast<RenderText>(*descendant).text()->length();
            if (count >= threshold)
                break;
        }
    }
    return count;
}

bool FrameView::renderedCharactersExceed(unsigned threshold)
{
    if (!frame().contentRenderer())
        return false;
    return countRenderedCharactersInRenderObjectWithThreshold(*frame().contentRenderer(), threshold) >= threshold;
}

void FrameView::availableContentSizeChanged(AvailableSizeChangeReason reason)
{
    if (Document* document = frame().document())
        document->updateViewportUnitsOnResize();

    setNeedsLayout();
    ScrollView::availableContentSizeChanged(reason);
}

bool FrameView::shouldLayoutAfterContentsResized() const
{
    return !useFixedLayout() || useCustomFixedPositionLayoutRect();
}

void FrameView::updateContentsSize()
{
    // We check to make sure the view is attached to a frame() as this method can
    // be triggered before the view is attached by Frame::createView(...) setting
    // various values such as setScrollBarModes(...) for example.  An ASSERT is
    // triggered when a view is layout before being attached to a frame().
    if (!frame().view())
        return;

#if PLATFORM(IOS)
    if (RenderView* root = m_frame->contentRenderer()) {
        if (useCustomFixedPositionLayoutRect() && hasViewportConstrainedObjects()) {
            setViewportConstrainedObjectsNeedLayout();
            // We must eagerly enter compositing mode because fixed position elements
            // will not have been made compositing via a preceding style change before
            // m_useCustomFixedPositionLayoutRect was true.
            root->compositor().enableCompositingMode();
        }
    }
#endif

    if (shouldLayoutAfterContentsResized() && needsLayout())
        layout();

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidChangeSize();
    }
}

void FrameView::addedOrRemovedScrollbar()
{
    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidAddOrRemoveScrollbars();
    }

    adjustTiledBackingScrollability();
}

void FrameView::adjustTiledBackingScrollability()
{
    auto* tiledBacking = this->tiledBacking();
    if (!tiledBacking)
        return;
    
    bool horizontallyScrollable;
    bool verticallyScrollable;
    bool clippedByAncestorView = static_cast<bool>(m_viewExposedRect);

#if PLATFORM(IOS)
    if (Page* page = frame().page())
        clippedByAncestorView |= page->enclosedInScrollableAncestorView();
#endif

    if (delegatesScrolling()) {
        IntSize documentSize = contentsSize();
        IntSize visibleSize = this->visibleSize();
        
        horizontallyScrollable = clippedByAncestorView || documentSize.width() > visibleSize.width();
        verticallyScrollable = clippedByAncestorView || documentSize.height() > visibleSize.height();
    } else {
        horizontallyScrollable = clippedByAncestorView || horizontalScrollbar();
        verticallyScrollable = clippedByAncestorView || verticalScrollbar();
    }

    TiledBacking::Scrollability scrollability = TiledBacking::NotScrollable;
    if (horizontallyScrollable)
        scrollability = TiledBacking::HorizontallyScrollable;

    if (verticallyScrollable)
        scrollability |= TiledBacking::VerticallyScrollable;

    tiledBacking->setScrollability(scrollability);
}

#if PLATFORM(IOS)
void FrameView::unobscuredContentSizeChanged()
{
    adjustTiledBackingScrollability();
}
#endif

static LayerFlushThrottleState::Flags determineLayerFlushThrottleState(Page& page)
{
    // We only throttle when constantly receiving new data during the inital page load.
    if (!page.progress().isMainLoadProgressing())
        return 0;
    // Scrolling during page loading disables throttling.
    if (page.mainFrame().view()->wasScrolledByUser())
        return 0;
    // Disable for image documents so large GIF animations don't get throttled during loading.
    auto* document = page.mainFrame().document();
    if (!document || is<ImageDocument>(*document))
        return 0;
    return LayerFlushThrottleState::Enabled;
}

void FrameView::disableLayerFlushThrottlingTemporarilyForInteraction()
{
    if (!frame().page())
        return;
    auto& page = *frame().page();

    LayerFlushThrottleState::Flags flags = LayerFlushThrottleState::UserIsInteracting | determineLayerFlushThrottleState(page);
    if (page.chrome().client().adjustLayerFlushThrottling(flags))
        return;

    if (RenderView* view = renderView())
        view->compositor().disableLayerFlushThrottlingTemporarilyForInteraction();
}

void FrameView::loadProgressingStatusChanged()
{
    updateLayerFlushThrottling();
    adjustTiledBackingCoverage();
}

void FrameView::updateLayerFlushThrottling()
{
    Page* page = frame().page();
    if (!page)
        return;

    ASSERT(frame().isMainFrame());

    LayerFlushThrottleState::Flags flags = determineLayerFlushThrottleState(*page);

    // See if the client is handling throttling.
    if (page->chrome().client().adjustLayerFlushThrottling(flags))
        return;

    for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        if (RenderView* renderView = frame->contentRenderer())
            renderView->compositor().setLayerFlushThrottlingEnabled(flags & LayerFlushThrottleState::Enabled);
    }
}

void FrameView::adjustTiledBackingCoverage()
{
    if (!m_speculativeTilingEnabled)
        enableSpeculativeTilingIfNeeded();

    RenderView* renderView = this->renderView();
    if (renderView && renderView->layer()->backing())
        renderView->layer()->backing()->adjustTiledBackingCoverage();
#if PLATFORM(IOS)
    if (LegacyTileCache* tileCache = legacyTileCache())
        tileCache->setSpeculativeTileCreationEnabled(m_speculativeTilingEnabled);
#endif
}

static bool shouldEnableSpeculativeTilingDuringLoading(const FrameView& view)
{
    Page* page = view.frame().page();
    return page && view.isVisuallyNonEmpty() && !page->progress().isMainLoadProgressing();
}

void FrameView::enableSpeculativeTilingIfNeeded()
{
    ASSERT(!m_speculativeTilingEnabled);
    if (m_wasScrolledByUser) {
        m_speculativeTilingEnabled = true;
        return;
    }
    if (!shouldEnableSpeculativeTilingDuringLoading(*this))
        return;
    if (m_speculativeTilingEnableTimer.isActive())
        return;
    // Delay enabling a bit as load completion may trigger further loading from scripts.
    static const double speculativeTilingEnableDelay = 0.5;
    m_speculativeTilingEnableTimer.startOneShot(speculativeTilingEnableDelay);
}

void FrameView::speculativeTilingEnableTimerFired()
{
    if (m_speculativeTilingEnabled)
        return;
    m_speculativeTilingEnabled = shouldEnableSpeculativeTilingDuringLoading(*this);
    adjustTiledBackingCoverage();
}

void FrameView::show()
{
    ScrollView::show();

    if (frame().isMainFrame()) {
        // Turn off speculative tiling for a brief moment after a FrameView appears on screen.
        // Note that adjustTiledBackingCoverage() kicks the (500ms) timer to re-enable it.
        m_speculativeTilingEnabled = false;
        m_wasScrolledByUser = false;
        adjustTiledBackingCoverage();
    }
}
void FrameView::convertSubtreeLayoutToFullLayout()
{
    ASSERT(m_layoutRoot);
    m_layoutRoot->markContainingBlocksForLayout(ScheduleRelayout::No);
    m_layoutRoot = nullptr;
}

void FrameView::layoutTimerFired()
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!frame().document()->ownerElement())
        printf("Layout timer fired at %lld\n", frame().document()->elapsedTime().count());
#endif
    layout();
}

void FrameView::scheduleRelayout()
{
    // FIXME: We should assert the page is not in the page cache, but that is causing
    // too many false assertions.  See <rdar://problem/7218118>.
    ASSERT(frame().view() == this);

    if (m_layoutRoot)
        convertSubtreeLayoutToFullLayout();
    if (!m_layoutSchedulingEnabled)
        return;
    if (!needsLayout())
        return;
    if (!frame().document()->shouldScheduleLayout())
        return;
    InspectorInstrumentation::didInvalidateLayout(frame());
    // When frame flattening is enabled, the contents of the frame could affect the layout of the parent frames.
    // Also invalidate parent frame starting from the owner element of this frame.
    if (frame().ownerRenderer() && isInChildFrameWithFrameFlattening())
        frame().ownerRenderer()->setNeedsLayout(MarkContainingBlockChain);

    std::chrono::milliseconds delay = frame().document()->minimumLayoutDelay();
    if (m_layoutTimer.isActive() && m_delayedLayout && !delay.count())
        unscheduleRelayout();
    if (m_layoutTimer.isActive())
        return;

    m_delayedLayout = delay.count();

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!frame().document()->ownerElement())
        printf("Scheduling layout for %d\n", delay);
#endif

    m_layoutTimer.startOneShot(delay);
}

static bool isObjectAncestorContainerOf(RenderObject* ancestor, RenderObject* descendant)
{
    for (RenderObject* r = descendant; r; r = r->container()) {
        if (r == ancestor)
            return true;
    }
    return false;
}

void FrameView::scheduleRelayoutOfSubtree(RenderElement& newRelayoutRoot)
{
    ASSERT(renderView());
    const RenderView& renderView = *this->renderView();

    // Try to catch unnecessary work during render tree teardown.
    ASSERT(!renderView.documentBeingDestroyed());
    ASSERT(frame().view() == this);

    // When m_layoutRoot is already set, ignore the renderView's needsLayout bit
    // since we need to resolve the conflict between the m_layoutRoot and newRelayoutRoot layouts.
    if (renderView.needsLayout() && !m_layoutRoot) {
        m_layoutRoot = &newRelayoutRoot;
        convertSubtreeLayoutToFullLayout();
        return;
    }

    if (!layoutPending() && m_layoutSchedulingEnabled) {
        std::chrono::milliseconds delay = renderView.document().minimumLayoutDelay();
        ASSERT(!newRelayoutRoot.container() || is<RenderView>(newRelayoutRoot.container()) || !newRelayoutRoot.container()->needsLayout());
        m_layoutRoot = &newRelayoutRoot;
        InspectorInstrumentation::didInvalidateLayout(frame());
        m_delayedLayout = delay.count();
        m_layoutTimer.startOneShot(delay);
        return;
    }

    if (m_layoutRoot == &newRelayoutRoot)
        return;

    if (!m_layoutRoot) {
        // We already have a pending (full) layout. Just mark the subtree for layout.
        newRelayoutRoot.markContainingBlocksForLayout(ScheduleRelayout::No);
        InspectorInstrumentation::didInvalidateLayout(frame());
        return;
    }

    if (isObjectAncestorContainerOf(m_layoutRoot, &newRelayoutRoot)) {
        // Keep the current root.
        newRelayoutRoot.markContainingBlocksForLayout(ScheduleRelayout::No, m_layoutRoot);
        ASSERT(!m_layoutRoot->container() || is<RenderView>(m_layoutRoot->container()) || !m_layoutRoot->container()->needsLayout());
        return;
    }

    if (isObjectAncestorContainerOf(&newRelayoutRoot, m_layoutRoot)) {
        // Re-root at newRelayoutRoot.
        m_layoutRoot->markContainingBlocksForLayout(ScheduleRelayout::No, &newRelayoutRoot);
        m_layoutRoot = &newRelayoutRoot;
        ASSERT(!m_layoutRoot->container() || is<RenderView>(m_layoutRoot->container()) || !m_layoutRoot->container()->needsLayout());
        InspectorInstrumentation::didInvalidateLayout(frame());
        return;
    }
    // Two disjoint subtrees need layout. Mark both of them and issue a full layout instead.
    convertSubtreeLayoutToFullLayout();
    newRelayoutRoot.markContainingBlocksForLayout(ScheduleRelayout::No);
    InspectorInstrumentation::didInvalidateLayout(frame());
}

bool FrameView::layoutPending() const
{
    return m_layoutTimer.isActive();
}

bool FrameView::needsStyleRecalcOrLayout(bool includeSubframes) const
{
    if (frame().document() && frame().document()->childNeedsStyleRecalc())
        return true;
    
    if (needsLayout())
        return true;

    if (!includeSubframes)
        return false;

    for (auto& frameView : renderedChildFrameViews()) {
        if (frameView->needsStyleRecalcOrLayout())
            return true;
    }

    return false;
}

bool FrameView::needsLayout() const
{
    // This can return true in cases where the document does not have a body yet.
    // Document::shouldScheduleLayout takes care of preventing us from scheduling
    // layout in that case.
    RenderView* renderView = this->renderView();
    return layoutPending()
        || (renderView && renderView->needsLayout())
        || m_layoutRoot
        || (m_deferSetNeedsLayoutCount && m_setNeedsLayoutWasDeferred);
}

void FrameView::setNeedsLayout()
{
    if (m_deferSetNeedsLayoutCount) {
        m_setNeedsLayoutWasDeferred = true;
        return;
    }

    if (auto* renderView = this->renderView()) {
        ASSERT(!renderView->inHitTesting());
        renderView->setNeedsLayout();
    }
}

void FrameView::unscheduleRelayout()
{
    if (m_postLayoutTasksTimer.isActive())
        m_postLayoutTasksTimer.stop();

    if (!m_layoutTimer.isActive())
        return;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!frame().document()->ownerElement())
        printf("Layout timer unscheduled at %d\n", frame().document()->elapsedTime());
#endif
    
    m_layoutTimer.stop();
    m_delayedLayout = false;
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void FrameView::serviceScriptedAnimations()
{
    for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext()) {
        frame->view()->serviceScrollAnimations();
        frame->animation().serviceAnimations();
    }

    if (!frame().document() || !frame().document()->domWindow())
        return;

    Vector<RefPtr<Document>> documents;
    for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext())
        documents.append(frame->document());

    double timestamp = frame().document()->domWindow()->nowTimestamp();
    for (auto& document : documents)
        document->serviceScriptedAnimations(timestamp);
}
#endif

bool FrameView::isTransparent() const
{
    return m_isTransparent;
}

void FrameView::setTransparent(bool isTransparent)
{
    if (m_isTransparent == isTransparent)
        return;

    m_isTransparent = isTransparent;

    // setTransparent can be called in the window between FrameView initialization
    // and switching in the new Document; this means that the RenderView that we
    // retrieve is actually attached to the previous Document, which is going away,
    // and must not update compositing layers.
    if (!isViewForDocumentInFrame())
        return;

    renderView()->compositor().rootBackgroundTransparencyChanged();
}

bool FrameView::hasOpaqueBackground() const
{
    return !m_isTransparent && !m_baseBackgroundColor.hasAlpha();
}

Color FrameView::baseBackgroundColor() const
{
    return m_baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(const Color& backgroundColor)
{
    bool hadAlpha = m_baseBackgroundColor.hasAlpha();
    
    if (!backgroundColor.isValid())
        m_baseBackgroundColor = Color::white;
    else
        m_baseBackgroundColor = backgroundColor;

    if (!isViewForDocumentInFrame())
        return;

    recalculateScrollbarOverlayStyle();

    if (m_baseBackgroundColor.hasAlpha() != hadAlpha)
        renderView()->compositor().rootBackgroundTransparencyChanged();
}

void FrameView::updateBackgroundRecursively(const Color& backgroundColor, bool transparent)
{
    for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        if (FrameView* view = frame->view()) {
            view->setTransparent(transparent);
            view->setBaseBackgroundColor(backgroundColor);
        }
    }
}

bool FrameView::hasExtendedBackgroundRectForPainting() const
{
    if (!frame().settings().backgroundShouldExtendBeyondPage())
        return false;

    TiledBacking* tiledBacking = this->tiledBacking();
    if (!tiledBacking)
        return false;

    return tiledBacking->hasMargins();
}

void FrameView::updateExtendBackgroundIfNecessary()
{
    ExtendedBackgroundMode mode = calculateExtendedBackgroundMode();
    if (mode == ExtendedBackgroundModeNone)
        return;

    updateTilesForExtendedBackgroundMode(mode);
}

FrameView::ExtendedBackgroundMode FrameView::calculateExtendedBackgroundMode() const
{
    // Just because Settings::backgroundShouldExtendBeyondPage() is true does not necessarily mean
    // that the background rect needs to be extended for painting. Simple backgrounds can be extended
    // just with RenderLayerCompositor::setRootExtendedBackgroundColor(). More complicated backgrounds,
    // such as images, require extending the background rect to continue painting into the extended
    // region. This function finds out if it is necessary to extend the background rect for painting.

#if PLATFORM(IOS)
    // <rdar://problem/16201373>
    return ExtendedBackgroundModeNone;
#else
    if (!frame().settings().backgroundShouldExtendBeyondPage())
        return ExtendedBackgroundModeNone;

    if (!frame().isMainFrame())
        return ExtendedBackgroundModeNone;

    Document* document = frame().document();
    if (!document)
        return ExtendedBackgroundModeNone;

    auto* documentElement = document->documentElement();
    auto* documentElementRenderer = documentElement ? documentElement->renderer() : nullptr;
    if (!documentElementRenderer)
        return ExtendedBackgroundModeNone;

    auto& renderer = documentElementRenderer->rendererForRootBackground();
    if (!renderer.style().hasBackgroundImage())
        return ExtendedBackgroundModeNone;

    ExtendedBackgroundMode mode = ExtendedBackgroundModeNone;

    if (renderer.style().backgroundRepeatX() == RepeatFill)
        mode |= ExtendedBackgroundModeHorizontal;
    if (renderer.style().backgroundRepeatY() == RepeatFill)
        mode |= ExtendedBackgroundModeVertical;

    return mode;
#endif
}

void FrameView::updateTilesForExtendedBackgroundMode(ExtendedBackgroundMode mode)
{
    if (!frame().settings().backgroundShouldExtendBeyondPage())
        return;

    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return;

    TiledBacking* tiledBacking = backing->graphicsLayer()->tiledBacking();
    if (!tiledBacking)
        return;

    ExtendedBackgroundMode existingMode = ExtendedBackgroundModeNone;
    if (tiledBacking->hasVerticalMargins())
        existingMode |= ExtendedBackgroundModeVertical;
    if (tiledBacking->hasHorizontalMargins())
        existingMode |= ExtendedBackgroundModeHorizontal;

    if (existingMode == mode)
        return;

    renderView->compositor().setRootExtendedBackgroundColor(mode == ExtendedBackgroundModeAll ? Color() : documentBackgroundColor());
    backing->setTiledBackingHasMargins(mode & ExtendedBackgroundModeHorizontal, mode & ExtendedBackgroundModeVertical);
}

IntRect FrameView::extendedBackgroundRectForPainting() const
{
    TiledBacking* tiledBacking = this->tiledBacking();
    if (!tiledBacking)
        return IntRect();
    
    RenderView* renderView = this->renderView();
    if (!renderView)
        return IntRect();
    
    LayoutRect extendedRect = renderView->unextendedBackgroundRect();
    if (!tiledBacking->hasMargins())
        return snappedIntRect(extendedRect);
    
    extendedRect.moveBy(LayoutPoint(-tiledBacking->leftMarginWidth(), -tiledBacking->topMarginHeight()));
    extendedRect.expand(LayoutSize(tiledBacking->leftMarginWidth() + tiledBacking->rightMarginWidth(), tiledBacking->topMarginHeight() + tiledBacking->bottomMarginHeight()));
    return snappedIntRect(extendedRect);
}

bool FrameView::shouldUpdateWhileOffscreen() const
{
    return m_shouldUpdateWhileOffscreen;
}

void FrameView::setShouldUpdateWhileOffscreen(bool shouldUpdateWhileOffscreen)
{
    m_shouldUpdateWhileOffscreen = shouldUpdateWhileOffscreen;
}

bool FrameView::shouldUpdate() const
{
    if (isOffscreen() && !shouldUpdateWhileOffscreen())
        return false;
    return true;
}

void FrameView::scrollToAnchor()
{
    RefPtr<ContainerNode> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;

    if (!anchorNode->renderer())
        return;

    LayoutRect rect;
    if (anchorNode != frame().document() && anchorNode->renderer())
        rect = anchorNode->renderer()->anchorRect();

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    if (anchorNode->renderer()->style().isHorizontalWritingMode())
        anchorNode->renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, rect, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);
    else if (anchorNode->renderer()->style().isFlippedBlocksWritingMode())
        anchorNode->renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, rect, ScrollAlignment::alignRightAlways, ScrollAlignment::alignToEdgeIfNeeded);
    else
        anchorNode->renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, rect, ScrollAlignment::alignLeftAlways, ScrollAlignment::alignToEdgeIfNeeded);

    if (AXObjectCache* cache = frame().document()->existingAXObjectCache())
        cache->handleScrolledToAnchor(anchorNode.get());

    // scrollRectToVisible can call into setScrollPosition(), which resets m_maintainScrollPositionAnchor.
    m_maintainScrollPositionAnchor = anchorNode;
}

void FrameView::updateEmbeddedObject(RenderEmbeddedObject& embeddedObject)
{
    // No need to update if it's already crashed or known to be missing.
    if (embeddedObject.isPluginUnavailable())
        return;

    HTMLFrameOwnerElement& element = embeddedObject.frameOwnerElement();

    if (embeddedObject.isSnapshottedPlugIn()) {
        if (is<HTMLObjectElement>(element) || is<HTMLEmbedElement>(element)) {
            HTMLPlugInImageElement& pluginElement = downcast<HTMLPlugInImageElement>(element);
            pluginElement.checkSnapshotStatus();
        }
        return;
    }

    auto weakRenderer = embeddedObject.createWeakPtr();

    // FIXME: This could turn into a real virtual dispatch if we defined
    // updateWidget(PluginCreationOption) on HTMLElement.
    if (is<HTMLPlugInImageElement>(element)) {
        HTMLPlugInImageElement& pluginElement = downcast<HTMLPlugInImageElement>(element);
        if (pluginElement.needsCheckForSizeChange()) {
            pluginElement.checkSnapshotStatus();
            return;
        }
        if (pluginElement.needsWidgetUpdate())
            pluginElement.updateWidget(CreatePlugins::Yes);
    } else
        ASSERT_NOT_REACHED();

    // It's possible the renderer was destroyed below updateWidget() since loading a plugin may execute arbitrary JavaScript.
    if (!weakRenderer)
        return;

    auto ignoreWidgetState = embeddedObject.updateWidgetPosition();
    UNUSED_PARAM(ignoreWidgetState);
}

bool FrameView::updateEmbeddedObjects()
{
    if (m_nestedLayoutCount > 1 || !m_embeddedObjectsToUpdate || m_embeddedObjectsToUpdate->isEmpty())
        return true;

    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    // Insert a marker for where we should stop.
    ASSERT(!m_embeddedObjectsToUpdate->contains(nullptr));
    m_embeddedObjectsToUpdate->add(nullptr);

    while (!m_embeddedObjectsToUpdate->isEmpty()) {
        RenderEmbeddedObject* embeddedObject = m_embeddedObjectsToUpdate->takeFirst();
        if (!embeddedObject)
            break;
        updateEmbeddedObject(*embeddedObject);
    }

    return m_embeddedObjectsToUpdate->isEmpty();
}

void FrameView::updateEmbeddedObjectsTimerFired()
{
    RefPtr<FrameView> protectedThis(this);
    m_updateEmbeddedObjectsTimer.stop();
    for (unsigned i = 0; i < maxUpdateEmbeddedObjectsIterations; i++) {
        if (updateEmbeddedObjects())
            break;
    }
}

void FrameView::flushAnyPendingPostLayoutTasks()
{
    if (m_postLayoutTasksTimer.isActive())
        performPostLayoutTasks();
    if (m_updateEmbeddedObjectsTimer.isActive())
        updateEmbeddedObjectsTimerFired();
}

void FrameView::queuePostLayoutCallback(Function<void()>&& callback)
{
    m_postLayoutCallbackQueue.append(WTFMove(callback));
}

void FrameView::flushPostLayoutTasksQueue()
{
    if (m_nestedLayoutCount > 1)
        return;

    if (!m_postLayoutCallbackQueue.size())
        return;

    Vector<Function<void()>> queue = WTFMove(m_postLayoutCallbackQueue);
    for (auto& task : queue)
        task();
}

void FrameView::performPostLayoutTasks()
{
    LOG(Layout, "FrameView %p performPostLayoutTasks", this);

    // FIXME: We should not run any JavaScript code in this function.

    m_postLayoutTasksTimer.stop();

    frame().selection().updateAppearanceAfterLayout();

    flushPostLayoutTasksQueue();

    if (m_nestedLayoutCount <= 1 && frame().document()->documentElement())
        fireLayoutRelatedMilestonesIfNeeded();

#if PLATFORM(IOS)
    // Only send layout-related delegate callbacks synchronously for the main frame to
    // avoid re-entering layout for the main frame while delivering a layout-related delegate
    // callback for a subframe.
    if (frame().isMainFrame()) {
        if (Page* page = frame().page())
            page->chrome().client().didLayout();
    }
#endif

#if ENABLE(FONT_LOAD_EVENTS)
    if (RuntimeEnabledFeatures::sharedFeatures().fontLoadEventsEnabled())
        frame().document()->fonts()->didLayout();
#endif
    
    // FIXME: We should consider adding DidLayout as a LayoutMilestone. That would let us merge this
    // with didLayout(LayoutMilestones).
    frame().loader().client().dispatchDidLayout();

    updateWidgetPositions();

#if ENABLE(CSS_SCROLL_SNAP)
    updateSnapOffsets();
#endif

    // layout() protects FrameView, but it still can get destroyed when updateEmbeddedObjects()
    // is called through the post layout timer.
    Ref<FrameView> protectedThis(*this);

    m_updateEmbeddedObjectsTimer.startOneShot(0);

    if (auto* page = frame().page()) {
        if (auto* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewLayoutUpdated(*this);
    }

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidLayout();
    }

    scrollToAnchor();

    sendResizeEventIfNeeded();
    viewportContentsChanged();

    updateScrollSnapState();

    if (AXObjectCache* cache = frame().document()->existingAXObjectCache())
        cache->performDeferredIsIgnoredChange();
}

IntSize FrameView::sizeForResizeEvent() const
{
#if PLATFORM(IOS)
    if (m_useCustomSizeForResizeEvent)
        return m_customSizeForResizeEvent;
#endif
    if (useFixedLayout() && !fixedLayoutSize().isEmpty() && delegatesScrolling())
        return fixedLayoutSize();
    return visibleContentRectIncludingScrollbars().size();
}

void FrameView::sendResizeEventIfNeeded()
{
    if (isInRenderTreeLayout() || needsLayout())
        return;

    RenderView* renderView = this->renderView();
    if (!renderView || renderView->printing())
        return;

    if (frame().page() && frame().page()->chrome().client().isSVGImageChromeClient())
        return;

    IntSize currentSize = sizeForResizeEvent();
    float currentZoomFactor = renderView->style().zoom();

    if (currentSize == m_lastViewportSize && currentZoomFactor == m_lastZoomFactor)
        return;

    m_lastViewportSize = currentSize;
    m_lastZoomFactor = currentZoomFactor;

    if (m_firstLayout)
        return;

#if PLATFORM(IOS)
    // Don't send the resize event if the document is loading. Some pages automatically reload
    // when the window is resized; Safari on iOS often resizes the window while setting up its
    // viewport. This obviously can cause problems.
    if (DocumentLoader* documentLoader = frame().loader().documentLoader()) {
        if (documentLoader->isLoadingInAPISense())
            return;
    }
#endif

    bool isMainFrame = frame().isMainFrame();
    bool canSendResizeEventSynchronously = isMainFrame && !m_shouldAutoSize;

    Ref<Event> resizeEvent = Event::create(eventNames().resizeEvent, false, false);
    if (canSendResizeEventSynchronously)
        frame().document()->dispatchWindowEvent(resizeEvent);
    else {
        // FIXME: Queueing this event for an unpredictable time in the future seems
        // intrinsically racy. By the time this resize event fires, the frame might
        // be resized again, so we could end up with two resize events for the same size.
        frame().document()->enqueueWindowEvent(WTFMove(resizeEvent));
    }

    if (InspectorInstrumentation::hasFrontends() && isMainFrame) {
        if (Page* page = frame().page()) {
            if (InspectorClient* inspectorClient = page->inspectorController().inspectorClient())
                inspectorClient->didResizeMainFrame(&frame());
        }
    }
}

void FrameView::willStartLiveResize()
{
    ScrollView::willStartLiveResize();
    adjustTiledBackingCoverage();
}
    
void FrameView::willEndLiveResize()
{
    ScrollView::willEndLiveResize();
    adjustTiledBackingCoverage();
}

void FrameView::autoSizeIfEnabled()
{
    if (!m_shouldAutoSize)
        return;

    if (m_inAutoSize)
        return;

    LOG(Layout, "FrameView %p autoSizeIfEnabled", this);

    TemporaryChange<bool> changeInAutoSize(m_inAutoSize, true);

    Document* document = frame().document();
    if (!document)
        return;

    RenderView* documentView = document->renderView();
    Element* documentElement = document->documentElement();
    if (!documentView || !documentElement)
        return;

    if (m_layoutRoot)
        convertSubtreeLayoutToFullLayout();
    // Start from the minimum size and allow it to grow.
    resize(m_minAutoSize.width(), m_minAutoSize.height());

    IntSize size = frameRect().size();

    // Do the resizing twice. The first time is basically a rough calculation using the preferred width
    // which may result in a height change during the second iteration.
    for (int i = 0; i < 2; i++) {
        // Update various sizes including contentsSize, scrollHeight, etc.
        document->updateLayoutIgnorePendingStylesheets();
        int width = documentView->minPreferredLogicalWidth();
        int height = documentView->documentRect().height();
        IntSize newSize(width, height);

        // Check to see if a scrollbar is needed for a given dimension and
        // if so, increase the other dimension to account for the scrollbar.
        // Since the dimensions are only for the view rectangle, once a
        // dimension exceeds the maximum, there is no need to increase it further.
        if (newSize.width() > m_maxAutoSize.width()) {
            RefPtr<Scrollbar> localHorizontalScrollbar = horizontalScrollbar();
            if (!localHorizontalScrollbar)
                localHorizontalScrollbar = createScrollbar(HorizontalScrollbar);
            newSize.expand(0, localHorizontalScrollbar->occupiedHeight());

            // Don't bother checking for a vertical scrollbar because the width is at
            // already greater the maximum.
        } else if (newSize.height() > m_maxAutoSize.height()) {
            RefPtr<Scrollbar> localVerticalScrollbar = verticalScrollbar();
            if (!localVerticalScrollbar)
                localVerticalScrollbar = createScrollbar(VerticalScrollbar);
            newSize.expand(localVerticalScrollbar->occupiedWidth(), 0);

            // Don't bother checking for a horizontal scrollbar because the height is
            // already greater the maximum.
        }

        // Ensure the size is at least the min bounds.
        newSize = newSize.expandedTo(m_minAutoSize);

        // Bound the dimensions by the max bounds and determine what scrollbars to show.
        ScrollbarMode horizonalScrollbarMode = ScrollbarAlwaysOff;
        if (newSize.width() > m_maxAutoSize.width()) {
            newSize.setWidth(m_maxAutoSize.width());
            horizonalScrollbarMode = ScrollbarAlwaysOn;
        }
        ScrollbarMode verticalScrollbarMode = ScrollbarAlwaysOff;
        if (newSize.height() > m_maxAutoSize.height()) {
            newSize.setHeight(m_maxAutoSize.height());
            verticalScrollbarMode = ScrollbarAlwaysOn;
        }

        if (newSize == size)
            continue;

        // While loading only allow the size to increase (to avoid twitching during intermediate smaller states)
        // unless autoresize has just been turned on or the maximum size is smaller than the current size.
        if (m_didRunAutosize && size.height() <= m_maxAutoSize.height() && size.width() <= m_maxAutoSize.width()
            && !frame().loader().isComplete() && (newSize.height() < size.height() || newSize.width() < size.width()))
            break;

        // The first time around, resize to the minimum height again; otherwise,
        // on pages (e.g. quirks mode) where the body/document resize to the view size,
        // we'll end up not shrinking back down after resizing to the computed preferred width.
        resize(newSize.width(), i ? newSize.height() : m_minAutoSize.height());
        // Force the scrollbar state to avoid the scrollbar code adding them and causing them to be needed. For example,
        // a vertical scrollbar may cause text to wrap and thus increase the height (which is the only reason the scollbar is needed).
        setVerticalScrollbarLock(false);
        setHorizontalScrollbarLock(false);
        setScrollbarModes(horizonalScrollbarMode, verticalScrollbarMode, true, true);
    }

    m_autoSizeContentSize = contentsSize();

    if (m_autoSizeFixedMinimumHeight) {
        resize(m_autoSizeContentSize.width(), std::max(m_autoSizeFixedMinimumHeight, m_autoSizeContentSize.height()));
        document->updateLayoutIgnorePendingStylesheets();
    }

    m_didRunAutosize = true;
}

void FrameView::setAutoSizeFixedMinimumHeight(int fixedMinimumHeight)
{
    if (m_autoSizeFixedMinimumHeight == fixedMinimumHeight)
        return;

    m_autoSizeFixedMinimumHeight = fixedMinimumHeight;

    setNeedsLayout();
}

RenderElement* FrameView::viewportRenderer() const
{
    if (m_viewportRendererType == ViewportRendererType::None)
        return nullptr;

    auto* document = frame().document();
    if (!document)
        return nullptr;

    if (m_viewportRendererType == ViewportRendererType::Document) {
        auto* documentElement = document->documentElement();
        if (!documentElement)
            return nullptr;
        return documentElement->renderer();
    }

    if (m_viewportRendererType == ViewportRendererType::Body) {
        auto* body = document->body();
        if (!body)
            return nullptr;
        return body->renderer();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void FrameView::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    auto* viewportRenderer = this->viewportRenderer();
    if (!viewportRenderer)
        return;
    
    if (m_overflowStatusDirty) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        m_overflowStatusDirty = false;
        return;
    }
    
    bool horizontalOverflowChanged = (m_horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (m_verticalOverflow != verticalOverflow);
    
    if (horizontalOverflowChanged || verticalOverflowChanged) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;

        Ref<OverflowEvent> overflowEvent = OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow,
            verticalOverflowChanged, verticalOverflow);
        overflowEvent->setTarget(viewportRenderer->element());

        frame().document()->enqueueOverflowEvent(WTFMove(overflowEvent));
    }
}

const Pagination& FrameView::pagination() const
{
    if (m_pagination != Pagination())
        return m_pagination;

    if (frame().isMainFrame()) {
        if (Page* page = frame().page())
            return page->pagination();
    }

    return m_pagination;
}

void FrameView::setPagination(const Pagination& pagination)
{
    if (m_pagination == pagination)
        return;

    m_pagination = pagination;

    frame().document()->styleResolverChanged(DeferRecalcStyle);
}

IntRect FrameView::windowClipRect() const
{
    ASSERT(frame().view() == this);

    if (m_cachedWindowClipRect)
        return *m_cachedWindowClipRect;

    if (paintsEntireContents())
        return contentsToWindow(IntRect(IntPoint(), totalContentsSize()));

    // Set our clip rect to be our contents.
    IntRect clipRect = contentsToWindow(visibleContentRect(LegacyIOSDocumentVisibleRect));

    if (!frame().ownerElement())
        return clipRect;

    // Take our owner element and get its clip rect.
    HTMLFrameOwnerElement* ownerElement = frame().ownerElement();
    if (FrameView* parentView = ownerElement->document().view())
        clipRect.intersect(parentView->windowClipRectForFrameOwner(ownerElement, true));
    return clipRect;
}

IntRect FrameView::windowClipRectForFrameOwner(const HTMLFrameOwnerElement* ownerElement, bool clipToLayerContents) const
{
    // The renderer can sometimes be null when style="display:none" interacts
    // with external content and plugins.
    if (!ownerElement->renderer())
        return windowClipRect();

    // If we have no layer, just return our window clip rect.
    const RenderLayer* enclosingLayer = ownerElement->renderer()->enclosingLayer();
    if (!enclosingLayer)
        return windowClipRect();

    // Apply the clip from the layer.
    IntRect clipRect;
    if (clipToLayerContents)
        clipRect = snappedIntRect(enclosingLayer->childrenClipRect());
    else
        clipRect = snappedIntRect(enclosingLayer->selfClipRect());
    clipRect = contentsToWindow(clipRect); 
    return intersection(clipRect, windowClipRect());
}

bool FrameView::isActive() const
{
    Page* page = frame().page();
    return page && page->focusController().isActive();
}

bool FrameView::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    Page* page = frame().page();
    return page && page->settings().forceUpdateScrollbarsOnMainThreadForPerformanceTesting();
}

void FrameView::scrollTo(const ScrollPosition& newPosition)
{
    IntPoint oldPosition = scrollPosition();
    ScrollView::scrollTo(newPosition);
    if (oldPosition != scrollPosition())
        scrollPositionChanged(oldPosition, scrollPosition());
    didChangeScrollOffset();
}

float FrameView::adjustScrollStepForFixedContent(float step, ScrollbarOrientation orientation, ScrollGranularity granularity)
{
    if (granularity != ScrollByPage || orientation == HorizontalScrollbar)
        return step;

    TrackedRendererListHashSet* positionedObjects = nullptr;
    if (RenderView* root = frame().contentRenderer()) {
        if (!root->hasPositionedObjects())
            return step;
        positionedObjects = root->positionedObjects();
    }

    FloatRect unobscuredContentRect = this->unobscuredContentRect();
    float topObscuredArea = 0;
    float bottomObscuredArea = 0;
    for (const auto& positionedObject : *positionedObjects) {
        const RenderStyle& style = positionedObject->style();
        if (style.position() != FixedPosition || style.visibility() == HIDDEN || !style.opacity())
            continue;

        FloatQuad contentQuad = positionedObject->absoluteContentQuad();
        if (!contentQuad.isRectilinear())
            continue;

        FloatRect contentBoundingBox = contentQuad.boundingBox();
        FloatRect fixedRectInView = intersection(unobscuredContentRect, contentBoundingBox);

        if (fixedRectInView.width() < unobscuredContentRect.width())
            continue;

        if (fixedRectInView.y() == unobscuredContentRect.y())
            topObscuredArea = std::max(topObscuredArea, fixedRectInView.height());
        else if (fixedRectInView.maxY() == unobscuredContentRect.maxY())
            bottomObscuredArea = std::max(bottomObscuredArea, fixedRectInView.height());
    }

    return Scrollbar::pageStep(unobscuredContentRect.height(), unobscuredContentRect.height() - topObscuredArea - bottomObscuredArea);
}

void FrameView::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    // Add in our offset within the FrameView.
    IntRect dirtyRect = rect;
    dirtyRect.moveBy(scrollbar.location());
    invalidateRect(dirtyRect);
}

float FrameView::visibleContentScaleFactor() const
{
    if (!frame().isMainFrame() || !frame().settings().delegatesPageScaling())
        return 1;

    Page* page = frame().page();
    if (!page)
        return 1;

    return page->pageScaleFactor();
}

void FrameView::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
{
    if (!frame().isMainFrame())
        return;

    if (Page* page = frame().page())
        page->chrome().client().notifyScrollerThumbIsVisibleInRect(scrollerThumb);
}

ScrollableArea* FrameView::enclosingScrollableArea() const
{
    // FIXME: Walk up the frame tree and look for a scrollable parent frame or RenderLayer.
    return nullptr;
}

IntRect FrameView::scrollableAreaBoundingBox(bool*) const
{
    RenderWidget* ownerRenderer = frame().ownerRenderer();
    if (!ownerRenderer)
        return frameRect();

    return ownerRenderer->absoluteContentQuad().enclosingBoundingBox();
}

bool FrameView::isScrollable(Scrollability definitionOfScrollable)
{
    // Check for:
    // 1) If there an actual overflow.
    // 2) display:none or visibility:hidden set to self or inherited.
    // 3) overflow{-x,-y}: hidden;
    // 4) scrolling: no;

    bool requiresActualOverflowToBeConsideredScrollable = !frame().isMainFrame() || definitionOfScrollable != Scrollability::ScrollableOrRubberbandable;
#if !ENABLE(RUBBER_BANDING)
    requiresActualOverflowToBeConsideredScrollable = true;
#endif

    // Covers #1
    if (requiresActualOverflowToBeConsideredScrollable) {
        IntSize totalContentsSize = this->totalContentsSize();
        IntSize visibleContentSize = visibleContentRect(LegacyIOSDocumentVisibleRect).size();
        if (totalContentsSize.height() <= visibleContentSize.height() && totalContentsSize.width() <= visibleContentSize.width())
            return false;
    }

    // Covers #2.
    HTMLFrameOwnerElement* owner = frame().ownerElement();
    if (owner && (!owner->renderer() || !owner->renderer()->visibleToHitTesting()))
        return false;

    // Cover #3 and #4.
    ScrollbarMode horizontalMode;
    ScrollbarMode verticalMode;
    calculateScrollbarModesForLayout(horizontalMode, verticalMode, RulesFromWebContentOnly);
    if (horizontalMode == ScrollbarAlwaysOff && verticalMode == ScrollbarAlwaysOff)
        return false;

    return true;
}

bool FrameView::isScrollableOrRubberbandable()
{
    return isScrollable(Scrollability::ScrollableOrRubberbandable);
}

bool FrameView::hasScrollableOrRubberbandableAncestor()
{
    if (frame().isMainFrame())
        return isScrollableOrRubberbandable();

    for (FrameView* parent = this->parentFrameView(); parent; parent = parent->parentFrameView()) {
        Scrollability frameScrollability = parent->frame().isMainFrame() ? Scrollability::ScrollableOrRubberbandable : Scrollability::Scrollable;
        if (parent->isScrollable(frameScrollability))
            return true;
    }

    return false;
}

void FrameView::updateScrollableAreaSet()
{
    // That ensures that only inner frames are cached.
    FrameView* parentFrameView = this->parentFrameView();
    if (!parentFrameView)
        return;

    if (!isScrollable()) {
        parentFrameView->removeScrollableArea(this);
        return;
    }

    parentFrameView->addScrollableArea(this);
}

bool FrameView::shouldSuspendScrollAnimations() const
{
    return frame().loader().state() != FrameStateComplete;
}

void FrameView::scrollbarStyleChanged(ScrollbarStyle newStyle, bool forceUpdate)
{
    if (!frame().isMainFrame())
        return;

    if (Page* page = frame().page())
        page->chrome().client().recommendedScrollbarStyleDidChange(newStyle);

    ScrollView::scrollbarStyleChanged(newStyle, forceUpdate);
}

void FrameView::notifyPageThatContentAreaWillPaint() const
{
    Page* page = frame().page();
    if (!page)
        return;

    contentAreaWillPaint();

    if (!m_scrollableAreas)
        return;

    for (auto& scrollableArea : *m_scrollableAreas)
        scrollableArea->contentAreaWillPaint();
}

bool FrameView::scrollAnimatorEnabled() const
{
#if ENABLE(SMOOTH_SCROLLING)
    if (Page* page = frame().page())
        return page->settings().scrollAnimatorEnabled();
#endif

    return false;
}

#if ENABLE(DASHBOARD_SUPPORT)
void FrameView::updateAnnotatedRegions()
{
    Document* document = frame().document();
    if (!document->hasAnnotatedRegions())
        return;
    Vector<AnnotatedRegionValue> newRegions;
    document->renderBox()->collectAnnotatedRegions(newRegions);
    if (newRegions == document->annotatedRegions())
        return;
    document->setAnnotatedRegions(newRegions);
    Page* page = frame().page();
    if (!page)
        return;
    page->chrome().client().annotatedRegionsChanged();
}
#endif

void FrameView::updateScrollCorner()
{
    RenderElement* renderer = nullptr;
    std::unique_ptr<RenderStyle> cornerStyle;
    IntRect cornerRect = scrollCornerRect();
    
    if (!cornerRect.isEmpty()) {
        // Try the <body> element first as a scroll corner source.
        Document* doc = frame().document();
        Element* body = doc ? doc->bodyOrFrameset() : nullptr;
        if (body && body->renderer()) {
            renderer = body->renderer();
            cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), &renderer->style());
        }
        
        if (!cornerStyle) {
            // If the <body> didn't have a custom style, then the root element might.
            Element* docElement = doc ? doc->documentElement() : nullptr;
            if (docElement && docElement->renderer()) {
                renderer = docElement->renderer();
                cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), &renderer->style());
            }
        }
        
        if (!cornerStyle) {
            // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
            if (RenderWidget* renderer = frame().ownerRenderer())
                cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), &renderer->style());
        }
    }

    if (!cornerStyle)
        m_scrollCorner = nullptr;
    else {
        if (!m_scrollCorner) {
            m_scrollCorner = createRenderer<RenderScrollbarPart>(renderer->document(), WTFMove(*cornerStyle));
            m_scrollCorner->initializeStyle();
        } else
            m_scrollCorner->setStyle(WTFMove(*cornerStyle));
        invalidateScrollCorner(cornerRect);
    }

    ScrollView::updateScrollCorner();
}

void FrameView::paintScrollCorner(GraphicsContext& context, const IntRect& cornerRect)
{
    if (context.updatingControlTints()) {
        updateScrollCorner();
        return;
    }

    if (m_scrollCorner) {
        if (frame().isMainFrame())
            context.fillRect(cornerRect, baseBackgroundColor());
        m_scrollCorner->paintIntoRect(context, cornerRect.location(), cornerRect);
        return;
    }

    ScrollView::paintScrollCorner(context, cornerRect);
}

void FrameView::paintScrollbar(GraphicsContext& context, Scrollbar& bar, const IntRect& rect)
{
    if (bar.isCustomScrollbar() && frame().isMainFrame()) {
        IntRect toFill = bar.frameRect();
        toFill.intersect(rect);
        context.fillRect(toFill, baseBackgroundColor());
    }

    ScrollView::paintScrollbar(context, bar, rect);
}

Color FrameView::documentBackgroundColor() const
{
    // <https://bugs.webkit.org/show_bug.cgi?id=59540> We blend the background color of
    // the document and the body against the base background color of the frame view.
    // Background images are unfortunately impractical to include.

    // Return invalid Color objects whenever there is insufficient information.
    if (!frame().document())
        return Color();

    auto* htmlElement = frame().document()->documentElement();
    auto* bodyElement = frame().document()->bodyOrFrameset();

    // Start with invalid colors.
    Color htmlBackgroundColor;
    Color bodyBackgroundColor;
    if (htmlElement && htmlElement->renderer())
        htmlBackgroundColor = htmlElement->renderer()->style().visitedDependentColor(CSSPropertyBackgroundColor);
    if (bodyElement && bodyElement->renderer())
        bodyBackgroundColor = bodyElement->renderer()->style().visitedDependentColor(CSSPropertyBackgroundColor);

    if (!bodyBackgroundColor.isValid()) {
        if (!htmlBackgroundColor.isValid())
            return Color();
        return baseBackgroundColor().blend(htmlBackgroundColor);
    }

    if (!htmlBackgroundColor.isValid())
        return baseBackgroundColor().blend(bodyBackgroundColor);

    // We take the aggregate of the base background color
    // the <html> background color, and the <body>
    // background color to find the document color. The
    // addition of the base background color is not
    // technically part of the document background, but it
    // otherwise poses problems when the aggregate is not
    // fully opaque.
    return baseBackgroundColor().blend(htmlBackgroundColor).blend(bodyBackgroundColor);
}

bool FrameView::hasCustomScrollbars() const
{
    for (auto& widget : children()) {
        if (is<FrameView>(*widget)) {
            if (downcast<FrameView>(*widget).hasCustomScrollbars())
                return true;
        } else if (is<Scrollbar>(*widget)) {
            if (downcast<Scrollbar>(*widget).isCustomScrollbar())
                return true;
        }
    }

    return false;
}

FrameView* FrameView::parentFrameView() const
{
    if (!parent())
        return nullptr;

    if (Frame* parentFrame = frame().tree().parent())
        return parentFrame->view();

    return nullptr;
}

bool FrameView::isInChildFrameWithFrameFlattening() const
{
    if (!frameFlatteningEnabled())
        return false;

    if (!parent())
        return false;

    HTMLFrameOwnerElement* ownerElement = frame().ownerElement();
    if (!ownerElement)
        return false;

    if (!ownerElement->renderWidget())
        return false;

    // Frame flattening applies when the owner element is either in a frameset or
    // an iframe with flattening parameters.
    if (is<HTMLIFrameElement>(*ownerElement))
        return downcast<RenderIFrame>(*ownerElement->renderWidget()).flattenFrame();

    if (is<HTMLFrameElement>(*ownerElement))
        return true;

    return false;
}

void FrameView::startLayoutAtMainFrameViewIfNeeded(bool allowSubtree)
{
    // When we start a layout at the child level as opposed to the topmost frame view and this child
    // frame requires flattening, we need to re-initiate the layout at the topmost view. Layout
    // will hit this view eventually.
    FrameView* parentView = parentFrameView();
    if (!parentView)
        return;

    // In the middle of parent layout, no need to restart from topmost.
    if (parentView->m_nestedLayoutCount)
        return;

    // Parent tree is clean. Starting layout from it would have no effect.
    if (!parentView->needsLayout())
        return;

    while (parentView->parentFrameView())
        parentView = parentView->parentFrameView();

    LOG(Layout, "  frame flattening, starting from root");
    parentView->layout(allowSubtree);
}

void FrameView::updateControlTints()
{
    // This is called when control tints are changed from aqua/graphite to clear and vice versa.
    // We do a "fake" paint, and when the theme gets a paint call, it can then do an invalidate.
    // This is only done if the theme supports control tinting. It's up to the theme and platform
    // to define when controls get the tint and to call this function when that changes.
    
    // Optimize the common case where we bring a window to the front while it's still empty.
    if (frame().document()->url().isEmpty())
        return;

    // As noted above, this is a "fake" paint, so we should pause counting relevant repainted objects.
    Page* page = frame().page();
    bool isCurrentlyCountingRelevantRepaintedObject = false;
    if (page) {
        isCurrentlyCountingRelevantRepaintedObject = page->isCountingRelevantRepaintedObjects();
        page->setIsCountingRelevantRepaintedObjects(false);
    }

    RenderView* renderView = this->renderView();
    if ((renderView && renderView->theme().supportsControlTints()) || hasCustomScrollbars())
        paintControlTints();

    if (page)
        page->setIsCountingRelevantRepaintedObjects(isCurrentlyCountingRelevantRepaintedObject);
}

void FrameView::paintControlTints()
{
    if (needsLayout())
        layout();

    GraphicsContext context(GraphicsContext::NonPaintingReasons::UpdatingControlTints);
    if (platformWidget()) {
        // FIXME: consult paintsEntireContents().
        paintContents(context, visibleContentRect(LegacyIOSDocumentVisibleRect));
    } else
        paint(context, frameRect());
}

bool FrameView::wasScrolledByUser() const
{
    return m_wasScrolledByUser;
}

void FrameView::setWasScrolledByUser(bool wasScrolledByUser)
{
    if (m_inProgrammaticScroll)
        return;
    m_maintainScrollPositionAnchor = nullptr;
    if (m_wasScrolledByUser == wasScrolledByUser)
        return;
    m_wasScrolledByUser = wasScrolledByUser;
    if (frame().isMainFrame())
        updateLayerFlushThrottling();
    adjustTiledBackingCoverage();
}

void FrameView::willPaintContents(GraphicsContext& context, const IntRect&, PaintingState& paintingState)
{
    Document* document = frame().document();

    if (!context.paintingDisabled())
        InspectorInstrumentation::willPaint(renderView());

    paintingState.isTopLevelPainter = !sCurrentPaintTimeStamp;

    if (paintingState.isTopLevelPainter && MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        LOG(MemoryPressure, "Under memory pressure: %s", WTF_PRETTY_FUNCTION);

        // To avoid unnecessary image decoding, we don't prune recently-decoded live resources here since
        // we might need some live bitmaps on painting.
        MemoryCache::singleton().prune();
    }

    if (paintingState.isTopLevelPainter)
        sCurrentPaintTimeStamp = monotonicallyIncreasingTime();

    paintingState.paintBehavior = m_paintBehavior;
    
    if (FrameView* parentView = parentFrameView()) {
        if (parentView->paintBehavior() & PaintBehaviorFlattenCompositingLayers)
            m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;
    }

    if (document->printing())
        m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;

    paintingState.isFlatteningPaintOfRootFrame = (m_paintBehavior & PaintBehaviorFlattenCompositingLayers) && !frame().ownerElement();
    if (paintingState.isFlatteningPaintOfRootFrame)
        notifyWidgetsInAllFrames(WillPaintFlattened);

    ASSERT(!m_isPainting);
    m_isPainting = true;
}

void FrameView::didPaintContents(GraphicsContext& context, const IntRect& dirtyRect, PaintingState& paintingState)
{
    m_isPainting = false;

    if (paintingState.isFlatteningPaintOfRootFrame)
        notifyWidgetsInAllFrames(DidPaintFlattened);

    m_paintBehavior = paintingState.paintBehavior;
    m_lastPaintTime = monotonicallyIncreasingTime();

    // Painting can lead to decoding of large amounts of bitmaps
    // If we are low on memory, wipe them out after the paint.
    if (paintingState.isTopLevelPainter && MemoryPressureHandler::singleton().isUnderMemoryPressure())
        MemoryCache::singleton().pruneLiveResources(true);

    // Regions may have changed as a result of the visibility/z-index of element changing.
#if ENABLE(DASHBOARD_SUPPORT)
    if (frame().document()->annotatedRegionsDirty())
        updateAnnotatedRegions();
#endif

    if (paintingState.isTopLevelPainter)
        sCurrentPaintTimeStamp = 0;

    if (!context.paintingDisabled()) {
        InspectorInstrumentation::didPaint(renderView(), dirtyRect);
        // FIXME: should probably not fire milestones for snapshot painting. https://bugs.webkit.org/show_bug.cgi?id=117623
        firePaintRelatedMilestonesIfNeeded();
    }
}

void FrameView::paintContents(GraphicsContext& context, const IntRect& dirtyRect)
{
#ifndef NDEBUG
    bool fillWithRed;
    if (frame().document()->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (frame().ownerElement())
        fillWithRed = false; // Subframe, don't fill with red.
    else if (isTransparent())
        fillWithRed = false; // Transparent, don't fill with red.
    else if (m_paintBehavior & PaintBehaviorSelectionOnly)
        fillWithRed = false; // Selections are transparent, don't fill with red.
    else if (m_nodeToDraw)
        fillWithRed = false; // Element images are transparent, don't fill with red.
    else
        fillWithRed = true;
    
    if (fillWithRed)
        context.fillRect(dirtyRect, Color(0xFF, 0, 0));
#endif

    if (m_layoutPhase == InViewSizeAdjust)
        return;

    TraceScope tracingScope(PaintViewStart, PaintViewEnd);

    ASSERT(m_layoutPhase == InPostLayerPositionsUpdatedAfterLayout || m_layoutPhase == OutsideLayout);
    
    RenderView* renderView = this->renderView();
    if (!renderView) {
        LOG_ERROR("called FrameView::paint with nil renderer");
        return;
    }

    ASSERT(!needsLayout());
    if (needsLayout())
        return;

    PaintingState paintingState;
    willPaintContents(context, dirtyRect, paintingState);

    // m_nodeToDraw is used to draw only one element (and its descendants)
    RenderObject* renderer = m_nodeToDraw ? m_nodeToDraw->renderer() : nullptr;
    RenderLayer* rootLayer = renderView->layer();

#ifndef NDEBUG
    RenderElement::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(&rootLayer->renderer());
#endif

    // To work around http://webkit.org/b/135106, ensure that the paint root isn't an inline with culled line boxes.
    // FIXME: This can cause additional content to be included in the snapshot, so remove this once that bug is fixed.
    while (is<RenderInline>(renderer) && !downcast<RenderInline>(*renderer).firstLineBox())
        renderer = renderer->parent();

    rootLayer->paint(context, dirtyRect, LayoutSize(), m_paintBehavior, renderer);
    if (rootLayer->containsDirtyOverlayScrollbars())
        rootLayer->paintOverlayScrollbars(context, dirtyRect, m_paintBehavior, renderer);

    didPaintContents(context, dirtyRect, paintingState);
}

void FrameView::setPaintBehavior(PaintBehavior behavior)
{
    m_paintBehavior = behavior;
}

PaintBehavior FrameView::paintBehavior() const
{
    return m_paintBehavior;
}

bool FrameView::isPainting() const
{
    return m_isPainting;
}

// FIXME: change this to use the subtreePaint terminology.
void FrameView::setNodeToDraw(Node* node)
{
    m_nodeToDraw = node;
}

void FrameView::paintContentsForSnapshot(GraphicsContext& context, const IntRect& imageRect, SelectionInSnapshot shouldPaintSelection, CoordinateSpaceForSnapshot coordinateSpace)
{
    updateLayoutAndStyleIfNeededRecursive();

    // Cache paint behavior and set a new behavior appropriate for snapshots.
    PaintBehavior oldBehavior = paintBehavior();
    setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);

    // If the snapshot should exclude selection, then we'll clear the current selection
    // in the render tree only. This will allow us to restore the selection from the DOM
    // after we paint the snapshot.
    if (shouldPaintSelection == ExcludeSelection) {
        for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
            if (RenderView* root = frame->contentRenderer())
                root->clearSelection();
        }
    }

    if (coordinateSpace == DocumentCoordinates)
        paintContents(context, imageRect);
    else {
        // A snapshot in ViewCoordinates will include a scrollbar, and the snapshot will contain
        // whatever content the document is currently scrolled to.
        paint(context, imageRect);
    }

    // Restore selection.
    if (shouldPaintSelection == ExcludeSelection) {
        for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr()))
            frame->selection().updateAppearance();
    }

    // Restore cached paint behavior.
    setPaintBehavior(oldBehavior);
}

void FrameView::paintOverhangAreas(GraphicsContext& context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    if (context.paintingDisabled())
        return;

    if (frame().document()->printing())
        return;

    ScrollView::paintOverhangAreas(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect);
}

FrameView::FrameViewList FrameView::renderedChildFrameViews() const
{
    FrameViewList childViews;
    for (Frame* frame = m_frame->tree().firstRenderedChild(); frame; frame = frame->tree().nextRenderedSibling()) {
        if (frame->view())
            childViews.append(*frame->view());
    }
    
    return childViews;
}

void FrameView::updateLayoutAndStyleIfNeededRecursive()
{
    // We have to crawl our entire tree looking for any FrameViews that need
    // layout and make sure they are up to date.
    // Mac actually tests for intersection with the dirty region and tries not to
    // update layout for frames that are outside the dirty region.  Not only does this seem
    // pointless (since those frames will have set a zero timer to layout anyway), but
    // it is also incorrect, since if two frames overlap, the first could be excluded from the dirty
    // region but then become included later by the second frame adding rects to the dirty region
    // when it lays out.

    AnimationUpdateBlock animationUpdateBlock(&frame().animation());

    frame().document()->updateStyleIfNeeded();

    if (needsLayout())
        layout();

    // Grab a copy of the child views, as the list may be mutated by the following updateLayoutAndStyleIfNeededRecursive
    // calls, as they can potentially re-enter a layout of the parent frame view.
    for (auto& frameView : renderedChildFrameViews())
        frameView->updateLayoutAndStyleIfNeededRecursive();

    // A child frame may have dirtied us during its layout.
    frame().document()->updateStyleIfNeeded();
    if (needsLayout())
        layout();

    ASSERT(!frame().isMainFrame() || !needsStyleRecalcOrLayout());
}

bool FrameView::qualifiesAsVisuallyNonEmpty() const
{
    // No content yet.
    Element* documentElement = frame().document()->documentElement();
    if (!documentElement || !documentElement->renderer())
        return false;

    // Ensure that we always get marked visually non-empty eventually.
    if (!frame().document()->parsing() && frame().loader().stateMachine().committedFirstRealDocumentLoad())
        return true;

    // Require the document to grow a bit.
    // Using a value of 48 allows the header on Google's search page to render immediately before search results populate later.
    static const int documentHeightThreshold = 48;
    LayoutRect overflowRect = documentElement->renderBox()->layoutOverflowRect();
    if (snappedIntRect(overflowRect).height() < documentHeightThreshold)
        return false;

    // The first few hundred characters rarely contain the interesting content of the page.
    if (m_visuallyNonEmptyCharacterCount > visualCharacterThreshold)
        return true;
    // Use a threshold value to prevent very small amounts of visible content from triggering didFirstVisuallyNonEmptyLayout
    if (m_visuallyNonEmptyPixelCount > visualPixelThreshold)
        return true;
    return false;
}

void FrameView::updateIsVisuallyNonEmpty()
{
    if (m_isVisuallyNonEmpty)
        return;
    if (!qualifiesAsVisuallyNonEmpty())
        return;
    m_isVisuallyNonEmpty = true;
    adjustTiledBackingCoverage();
}

bool FrameView::isViewForDocumentInFrame() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    return &renderView->frameView() == this;
}

void FrameView::enableAutoSizeMode(bool enable, const IntSize& minSize, const IntSize& maxSize)
{
    ASSERT(!enable || !minSize.isEmpty());
    ASSERT(minSize.width() <= maxSize.width());
    ASSERT(minSize.height() <= maxSize.height());

    if (m_shouldAutoSize == enable && m_minAutoSize == minSize && m_maxAutoSize == maxSize)
        return;

    m_shouldAutoSize = enable;
    m_minAutoSize = minSize;
    m_maxAutoSize = maxSize;
    m_didRunAutosize = false;

    setNeedsLayout();
    scheduleRelayout();
    if (m_shouldAutoSize)
        return;

    // Since autosize mode forces the scrollbar mode, change them to being auto.
    setVerticalScrollbarLock(false);
    setHorizontalScrollbarLock(false);
    setScrollbarModes(ScrollbarAuto, ScrollbarAuto);
}

void FrameView::forceLayout(bool allowSubtree)
{
    layout(allowSubtree);
}

void FrameView::forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor, AdjustViewSizeOrNot shouldAdjustViewSize)
{
    // Dumping externalRepresentation(frame().renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    if (RenderView* renderView = this->renderView()) {
        float pageLogicalWidth = renderView->style().isHorizontalWritingMode() ? pageSize.width() : pageSize.height();
        float pageLogicalHeight = renderView->style().isHorizontalWritingMode() ? pageSize.height() : pageSize.width();

        renderView->setLogicalWidth(floor(pageLogicalWidth));
        renderView->setPageLogicalHeight(floor(pageLogicalHeight));
        renderView->setNeedsLayoutAndPrefWidthsRecalc();
        forceLayout();

        // If we don't fit in the given page width, we'll lay out again. If we don't fit in the
        // page width when shrunk, we will lay out at maximum shrink and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        bool horizontalWritingMode = renderView->style().isHorizontalWritingMode();
        const LayoutRect& documentRect = renderView->documentRect();
        LayoutUnit docLogicalWidth = horizontalWritingMode ? documentRect.width() : documentRect.height();
        if (docLogicalWidth > pageLogicalWidth) {
            int expectedPageWidth = std::min<float>(documentRect.width(), pageSize.width() * maximumShrinkFactor);
            int expectedPageHeight = std::min<float>(documentRect.height(), pageSize.height() * maximumShrinkFactor);
            FloatSize maxPageSize = frame().resizePageRectsKeepingRatio(FloatSize(originalPageSize.width(), originalPageSize.height()), FloatSize(expectedPageWidth, expectedPageHeight));
            pageLogicalWidth = horizontalWritingMode ? maxPageSize.width() : maxPageSize.height();
            pageLogicalHeight = horizontalWritingMode ? maxPageSize.height() : maxPageSize.width();

            renderView->setLogicalWidth(floor(pageLogicalWidth));
            renderView->setPageLogicalHeight(floor(pageLogicalHeight));
            renderView->setNeedsLayoutAndPrefWidthsRecalc();
            forceLayout();

            const LayoutRect& updatedDocumentRect = renderView->documentRect();
            LayoutUnit docLogicalHeight = horizontalWritingMode ? updatedDocumentRect.height() : updatedDocumentRect.width();
            LayoutUnit docLogicalTop = horizontalWritingMode ? updatedDocumentRect.y() : updatedDocumentRect.x();
            LayoutUnit docLogicalRight = horizontalWritingMode ? updatedDocumentRect.maxX() : updatedDocumentRect.maxY();
            LayoutUnit clippedLogicalLeft = 0;
            if (!renderView->style().isLeftToRightDirection())
                clippedLogicalLeft = docLogicalRight - pageLogicalWidth;
            LayoutRect overflow(clippedLogicalLeft, docLogicalTop, pageLogicalWidth, docLogicalHeight);

            if (!horizontalWritingMode)
                overflow = overflow.transposedRect();
            renderView->clearLayoutOverflow();
            renderView->addLayoutOverflow(overflow); // This is how we clip in case we overflow again.
        }
    }

    if (shouldAdjustViewSize)
        adjustViewSize();
}

void FrameView::adjustPageHeightDeprecated(float *newBottom, float oldTop, float oldBottom, float /*bottomLimit*/)
{
    RenderView* renderView = this->renderView();
    if (!renderView) {
        *newBottom = oldBottom;
        return;

    }
    // Use a context with painting disabled.
    GraphicsContext context((PlatformGraphicsContext*)nullptr);
    renderView->setTruncatedAt(static_cast<int>(floorf(oldBottom)));
    IntRect dirtyRect(0, static_cast<int>(floorf(oldTop)), renderView->layoutOverflowRect().maxX(), static_cast<int>(ceilf(oldBottom - oldTop)));
    renderView->setPrintRect(dirtyRect);
    renderView->layer()->paint(context, dirtyRect);
    *newBottom = renderView->bestTruncatedAt();
    if (!*newBottom)
        *newBottom = oldBottom;
    renderView->setPrintRect(IntRect());
}

IntRect FrameView::convertFromRendererToContainingView(const RenderElement* renderer, const IntRect& rendererRect) const
{
    IntRect rect = snappedIntRect(enclosingLayoutRect(renderer->localToAbsoluteQuad(FloatRect(rendererRect)).boundingBox()));

    if (!delegatesScrolling())
        rect = contentsToView(rect);

    return rect;
}

IntRect FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const IntRect& viewRect) const
{
    IntRect rect = viewRect;
    
    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrolling())
        rect = viewToContents(rect);

    // FIXME: we don't have a way to map an absolute rect down to a local quad, so just
    // move the rect for now.
    rect.setLocation(roundedIntPoint(renderer->absoluteToLocal(rect.location(), UseTransforms)));
    return rect;
}

IntPoint FrameView::convertFromRendererToContainingView(const RenderElement* renderer, const IntPoint& rendererPoint) const
{
    IntPoint point = roundedIntPoint(renderer->localToAbsolute(rendererPoint, UseTransforms));

    // Convert from page ("absolute") to FrameView coordinates.
    if (!delegatesScrolling())
        point = contentsToView(point);

    return point;
}

IntPoint FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const IntPoint& viewPoint) const
{
    IntPoint point = viewPoint;

    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrolling())
        point = viewToContents(point);

    return roundedIntPoint(renderer->absoluteToLocal(point, UseTransforms));
}

IntRect FrameView::convertToContainingView(const IntRect& localRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (is<FrameView>(*parentScrollView)) {
            const FrameView& parentView = downcast<FrameView>(*parentScrollView);
            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return localRect;
                
            IntRect rect(localRect);
            // Add borders and padding??
            rect.move(renderer->borderLeft() + renderer->paddingLeft(),
                      renderer->borderTop() + renderer->paddingTop());
            return parentView.convertFromRendererToContainingView(renderer, rect);
        }
        
        return Widget::convertToContainingView(localRect);
    }
    
    return localRect;
}

IntRect FrameView::convertFromContainingView(const IntRect& parentRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (is<FrameView>(*parentScrollView)) {
            const FrameView& parentView = downcast<FrameView>(*parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return parentRect;

            IntRect rect = parentView.convertFromContainingViewToRenderer(renderer, parentRect);
            // Subtract borders and padding
            rect.move(-renderer->borderLeft() - renderer->paddingLeft(),
                      -renderer->borderTop() - renderer->paddingTop());
            return rect;
        }
        
        return Widget::convertFromContainingView(parentRect);
    }
    
    return parentRect;
}

IntPoint FrameView::convertToContainingView(const IntPoint& localPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (is<FrameView>(*parentScrollView)) {
            const FrameView& parentView = downcast<FrameView>(*parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return localPoint;
                
            IntPoint point(localPoint);

            // Add borders and padding
            point.move(renderer->borderLeft() + renderer->paddingLeft(),
                       renderer->borderTop() + renderer->paddingTop());
            return parentView.convertFromRendererToContainingView(renderer, point);
        }
        
        return Widget::convertToContainingView(localPoint);
    }
    
    return localPoint;
}

IntPoint FrameView::convertFromContainingView(const IntPoint& parentPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (is<FrameView>(*parentScrollView)) {
            const FrameView& parentView = downcast<FrameView>(*parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return parentPoint;

            IntPoint point = parentView.convertFromContainingViewToRenderer(renderer, parentPoint);
            // Subtract borders and padding
            point.move(-renderer->borderLeft() - renderer->paddingLeft(),
                       -renderer->borderTop() - renderer->paddingTop());
            return point;
        }
        
        return Widget::convertFromContainingView(parentPoint);
    }
    
    return parentPoint;
}

void FrameView::setTracksRepaints(bool trackRepaints)
{
    if (trackRepaints == m_isTrackingRepaints)
        return;

    // Force layout to flush out any pending repaints.
    if (trackRepaints) {
        if (frame().document())
            frame().document()->updateLayout();
    }

    for (Frame* frame = &m_frame->tree().top(); frame; frame = frame->tree().traverseNext()) {
        if (RenderView* renderView = frame->contentRenderer())
            renderView->compositor().setTracksRepaints(trackRepaints);
    }

    resetTrackedRepaints();
    m_isTrackingRepaints = trackRepaints;
}

void FrameView::resetTrackedRepaints()
{
    m_trackedRepaintRects.clear();
    if (RenderView* renderView = this->renderView())
        renderView->compositor().resetTrackedRepaintRects();
}

String FrameView::trackedRepaintRectsAsText() const
{
    if (frame().document())
        frame().document()->updateLayout();

    TextStream ts;
    if (!m_trackedRepaintRects.isEmpty()) {
        ts << "(repaint rects\n";
        for (auto& rect : m_trackedRepaintRects)
            ts << "  (rect " << LayoutUnit(rect.x()) << " " << LayoutUnit(rect.y()) << " " << LayoutUnit(rect.width()) << " " << LayoutUnit(rect.height()) << ")\n";
        ts << ")\n";
    }
    return ts.release();
}

bool FrameView::addScrollableArea(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreas)
        m_scrollableAreas = std::make_unique<ScrollableAreaSet>();
    
    if (m_scrollableAreas->add(scrollableArea).isNewEntry) {
        scrollableAreaSetChanged();
        return true;
    }

    return false;
}

bool FrameView::removeScrollableArea(ScrollableArea* scrollableArea)
{
    if (m_scrollableAreas && m_scrollableAreas->remove(scrollableArea)) {
        scrollableAreaSetChanged();
        return true;
    }
    return false;
}

bool FrameView::containsScrollableArea(ScrollableArea* scrollableArea) const
{
    return m_scrollableAreas && m_scrollableAreas->contains(scrollableArea);
}

void FrameView::scrollableAreaSetChanged()
{
    if (auto* page = frame().page()) {
        if (auto* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewEventTrackingRegionsChanged(*this);
    }
}

void FrameView::sendScrollEvent()
{
    frame().eventHandler().sendScrollEvent();
    frame().eventHandler().dispatchFakeMouseMoveEventSoon();
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    frame().animation().scrollWasUpdated();
#endif
}

void FrameView::removeChild(Widget& widget)
{
    if (is<FrameView>(widget))
        removeScrollableArea(&downcast<FrameView>(widget));

    ScrollView::removeChild(widget);
}

bool FrameView::wheelEvent(const PlatformWheelEvent& wheelEvent)
{
    // Note that to allow for rubber-band over-scroll behavior, even non-scrollable views
    // should handle wheel events.
#if !ENABLE(RUBBER_BANDING)
    if (!isScrollable())
        return false;
#endif

    if (delegatesScrolling()) {
        ScrollPosition oldPosition = scrollPosition();
        ScrollPosition newPosition = oldPosition - IntSize(wheelEvent.deltaX(), wheelEvent.deltaY());
        if (oldPosition != newPosition) {
            ScrollView::scrollTo(newPosition);
            scrollPositionChanged(oldPosition, scrollPosition());
            didChangeScrollOffset();
        }
        return true;
    }

    // We don't allow mouse wheeling to happen in a ScrollView that has had its scrollbars explicitly disabled.
    if (!canHaveScrollbars())
        return false;

    if (platformWidget())
        return false;

#if ENABLE(ASYNC_SCROLLING)
    if (Page* page = frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator()) {
            if (scrollingCoordinator->coordinatesScrollingForFrameView(*this))
                return scrollingCoordinator->handleWheelEvent(*this, wheelEvent);
        }
    }
#endif

    return ScrollableArea::handleWheelEvent(wheelEvent);
}


bool FrameView::isVerticalDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return true;

    return renderView->style().isHorizontalWritingMode();
}

bool FrameView::isFlippedDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    return renderView->style().isFlippedBlocksWritingMode();
}

void FrameView::notifyWidgetsInAllFrames(WidgetNotification notification)
{
    for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        if (FrameView* view = frame->view())
            view->notifyWidgets(notification);
    }
}
    
AXObjectCache* FrameView::axObjectCache() const
{
    if (frame().document())
        return frame().document()->existingAXObjectCache();
    return nullptr;
}
    
#if PLATFORM(IOS)
void FrameView::setCustomFixedPositionLayoutRect(const IntRect& rect)
{
    if (m_useCustomFixedPositionLayoutRect && m_customFixedPositionLayoutRect == rect)
        return;
    m_useCustomFixedPositionLayoutRect = true;
    m_customFixedPositionLayoutRect = rect;
    updateContentsSize();
}

bool FrameView::updateFixedPositionLayoutRect()
{
    if (!m_useCustomFixedPositionLayoutRect)
        return false;

    IntRect newRect;
    Page* page = frame().page();
    if (!page || !page->chrome().client().fetchCustomFixedPositionLayoutRect(newRect))
        return false;

    if (newRect != m_customFixedPositionLayoutRect) {
        m_customFixedPositionLayoutRect = newRect;
        setViewportConstrainedObjectsNeedLayout();
        return true;
    }
    return false;
}

void FrameView::setCustomSizeForResizeEvent(IntSize customSize)
{
    m_useCustomSizeForResizeEvent = true;
    m_customSizeForResizeEvent = customSize;
    sendResizeEventIfNeeded();
}

void FrameView::setScrollVelocity(double horizontalVelocity, double verticalVelocity, double scaleChangeRate, double timestamp)
{
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setVelocity(VelocityData(horizontalVelocity, verticalVelocity, scaleChangeRate, timestamp));
}
#endif // PLATFORM(IOS)

void FrameView::setScrollingPerformanceLoggingEnabled(bool flag)
{
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setScrollingPerformanceLoggingEnabled(flag);
}

void FrameView::didAddScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    ScrollableArea::didAddScrollbar(scrollbar, orientation);
    Page* page = frame().page();
    if (page && page->expectsWheelEventTriggers())
        scrollAnimator().setWheelEventTestTrigger(page->testTrigger());
    if (AXObjectCache* cache = axObjectCache())
        cache->handleScrollbarUpdate(this);
}

void FrameView::willRemoveScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    ScrollableArea::willRemoveScrollbar(scrollbar, orientation);
    if (AXObjectCache* cache = axObjectCache()) {
        cache->remove(scrollbar);
        cache->handleScrollbarUpdate(this);
    }
}

void FrameView::addPaintPendingMilestones(LayoutMilestones milestones)
{
    m_milestonesPendingPaint |= milestones;
}

void FrameView::fireLayoutRelatedMilestonesIfNeeded()
{
    LayoutMilestones requestedMilestones = 0;
    LayoutMilestones milestonesAchieved = 0;
    Page* page = frame().page();
    if (page)
        requestedMilestones = page->requestedLayoutMilestones();

    if (m_firstLayoutCallbackPending) {
        m_firstLayoutCallbackPending = false;
        frame().loader().didFirstLayout();
        if (requestedMilestones & DidFirstLayout)
            milestonesAchieved |= DidFirstLayout;
        if (frame().isMainFrame())
            page->startCountingRelevantRepaintedObjects();
    }
    updateIsVisuallyNonEmpty();

    // If the layout was done with pending sheets, we are not in fact visually non-empty yet.
    if (m_isVisuallyNonEmpty && !frame().document()->didLayoutWithPendingStylesheets() && m_firstVisuallyNonEmptyLayoutCallbackPending) {
        m_firstVisuallyNonEmptyLayoutCallbackPending = false;
        if (requestedMilestones & DidFirstVisuallyNonEmptyLayout)
            milestonesAchieved |= DidFirstVisuallyNonEmptyLayout;
    }

    if (milestonesAchieved && frame().isMainFrame())
        frame().loader().didReachLayoutMilestone(milestonesAchieved);
}

void FrameView::firePaintRelatedMilestonesIfNeeded()
{
    Page* page = frame().page();
    if (!page)
        return;

    LayoutMilestones milestonesAchieved = 0;

    // Make sure the pending paint milestones have actually been requested before we send them.
    if (m_milestonesPendingPaint & DidFirstFlushForHeaderLayer) {
        if (page->requestedLayoutMilestones() & DidFirstFlushForHeaderLayer)
            milestonesAchieved |= DidFirstFlushForHeaderLayer;
    }

    if (m_milestonesPendingPaint & DidFirstPaintAfterSuppressedIncrementalRendering) {
        if (page->requestedLayoutMilestones() & DidFirstPaintAfterSuppressedIncrementalRendering)
            milestonesAchieved |= DidFirstPaintAfterSuppressedIncrementalRendering;
    }

    m_milestonesPendingPaint = 0;

    if (milestonesAchieved)
        page->mainFrame().loader().didReachLayoutMilestone(milestonesAchieved);
}

void FrameView::setVisualUpdatesAllowedByClient(bool visualUpdatesAllowed)
{
    if (m_visualUpdatesAllowedByClient == visualUpdatesAllowed)
        return;

    m_visualUpdatesAllowedByClient = visualUpdatesAllowed;

    frame().document()->setVisualUpdatesAllowedByClient(visualUpdatesAllowed);
}
    
void FrameView::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    m_scrollPinningBehavior = pinning;
    
    if (Page* page = frame().page()) {
        if (auto* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->setScrollPinningBehavior(pinning);
    }
    
    updateScrollbars(scrollPosition());
}

ScrollBehaviorForFixedElements FrameView::scrollBehaviorForFixedElements() const
{
    return frame().settings().backgroundShouldExtendBeyondPage() ? StickToViewportBounds : StickToDocumentBounds;
}

RenderView* FrameView::renderView() const
{
    return frame().contentRenderer();
}

int FrameView::mapFromLayoutToCSSUnits(LayoutUnit value) const
{
    return value / (frame().pageZoomFactor() * frame().frameScaleFactor());
}

LayoutUnit FrameView::mapFromCSSToLayoutUnits(int value) const
{
    return value * frame().pageZoomFactor() * frame().frameScaleFactor();
}

void FrameView::didAddWidgetToRenderTree(Widget& widget)
{
    ASSERT(!m_widgetsInRenderTree.contains(&widget));
    m_widgetsInRenderTree.add(&widget);
}

void FrameView::willRemoveWidgetFromRenderTree(Widget& widget)
{
    ASSERT(m_widgetsInRenderTree.contains(&widget));
    m_widgetsInRenderTree.remove(&widget);
}

static Vector<RefPtr<Widget>> collectAndProtectWidgets(const HashSet<Widget*>& set)
{
    Vector<RefPtr<Widget>> widgets;
    copyToVector(set, widgets);
    return widgets;
}

void FrameView::updateWidgetPositions()
{
    // updateWidgetPosition() can possibly cause layout to be re-entered (via plug-ins running
    // scripts in response to NPP_SetWindow, for example), so we need to keep the Widgets
    // alive during enumeration.
    for (auto& widget : collectAndProtectWidgets(m_widgetsInRenderTree)) {
        if (RenderWidget* renderWidget = RenderWidget::find(widget.get())) {
            auto ignoreWidgetState = renderWidget->updateWidgetPosition();
            UNUSED_PARAM(ignoreWidgetState);
        }
    }
}

void FrameView::notifyWidgets(WidgetNotification notification)
{
    for (auto& widget : collectAndProtectWidgets(m_widgetsInRenderTree))
        widget->notifyWidget(notification);
}

void FrameView::setViewExposedRect(Optional<FloatRect> viewExposedRect)
{
    if (m_viewExposedRect == viewExposedRect)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "FrameView " << this << " setViewExposedRect " << (viewExposedRect ? viewExposedRect.value() : FloatRect()));

    bool hasRectChanged = !m_viewExposedRect == !viewExposedRect;
    m_viewExposedRect = viewExposedRect;

    // FIXME: We should support clipping to the exposed rect for subframes as well.
    if (!frame().isMainFrame())
        return;

    if (TiledBacking* tiledBacking = this->tiledBacking()) {
        if (hasRectChanged)
            adjustTiledBackingScrollability();
        adjustTiledBackingCoverage();
        tiledBacking->setTiledScrollingIndicatorPosition(m_viewExposedRect ? m_viewExposedRect.value().location() : FloatPoint());
    }

    if (auto* view = renderView())
        view->compositor().scheduleLayerFlush(false /* canThrottle */);

    frame().mainFrame().pageOverlayController().didChangeViewExposedRect();
}
    
void FrameView::setViewportSizeForCSSViewportUnits(IntSize size)
{
    if (m_hasOverrideViewportSize && m_overrideViewportSize == size)
        return;
    
    m_overrideViewportSize = size;
    m_hasOverrideViewportSize = true;
    
    if (Document* document = frame().document()) {
        // FIXME: this should probably be updateViewportUnitsOnResize(), but synchronously
        // dirtying style here causes assertions on iOS (rdar://problem/19998166).
        document->styleResolverChanged(DeferRecalcStyle);
    }
}
    
IntSize FrameView::viewportSizeForCSSViewportUnits() const
{
    if (m_hasOverrideViewportSize)
        return m_overrideViewportSize;

    if (useFixedLayout())
        return fixedLayoutSize();
    
    // FIXME: the value returned should take into account the value of the overflow
    // property on the root element.
    return visibleContentRectIncludingScrollbars().size();
}

bool FrameView::shouldPlaceBlockDirectionScrollbarOnLeft() const
{
    return renderView() && renderView()->shouldPlaceBlockDirectionScrollbarOnLeft();
}
    
} // namespace WebCore
