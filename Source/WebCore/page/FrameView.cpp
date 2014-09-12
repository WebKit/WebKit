/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
#include "DocumentMarkerController.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FontCache.h"
#include "FontLoader.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
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
#include "ScrollAnimator.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "TextResourceDecoder.h"
#include "TextStream.h"
#include "TiledBacking.h"

#include <wtf/CurrentTime.h>
#include <wtf/Ref.h>
#include <wtf/TemporaryChange.h>

#if USE(TILED_BACKING_STORE)
#include "TiledBackingStore.h"
#endif

#if ENABLE(TEXT_AUTOSIZING)
#include "TextAutosizer.h"
#endif

#if PLATFORM(IOS)
#include "DocumentLoader.h"
#include "LegacyTileCache.h"
#include "MemoryCache.h"
#include "MemoryPressureHandler.h"
#include "SystemMemory.h"
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

FrameView::FrameView(Frame& frame)
    : m_frame(&frame)
    , m_canHaveScrollbars(true)
    , m_layoutTimer(this, &FrameView::layoutTimerFired)
    , m_layoutRoot(0)
    , m_layoutPhase(OutsideLayout)
    , m_inSynchronousPostLayout(false)
    , m_postLayoutTasksTimer(this, &FrameView::postLayoutTimerFired)
    , m_updateEmbeddedObjectsTimer(this, &FrameView::updateEmbeddedObjectsTimerFired)
    , m_isTransparent(false)
    , m_baseBackgroundColor(Color::white)
    , m_mediaType("screen")
    , m_overflowStatusDirty(true)
    , m_viewportRenderer(0)
    , m_wasScrolledByUser(false)
    , m_inProgrammaticScroll(false)
    , m_safeToPropagateScrollToParent(true)
    , m_delayedScrollEventTimer(this, &FrameView::delayedScrollEventTimerFired)
    , m_isTrackingRepaints(false)
    , m_shouldUpdateWhileOffscreen(true)
    , m_exposedRect(FloatRect::infiniteRect())
    , m_deferSetNeedsLayouts(0)
    , m_setNeedsLayoutWasDeferred(false)
    , m_speculativeTilingEnabled(false)
    , m_speculativeTilingEnableTimer(this, &FrameView::speculativeTilingEnableTimerFired)
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
    , m_scrollPinningBehavior(DoNotPin)
{
    init();

    if (frame.isMainFrame()) {
        ScrollableArea::setVerticalScrollElasticity(m_frame->page() ? m_frame->page()->verticalScrollElasticity() : ScrollElasticityAllowed);
        ScrollableArea::setHorizontalScrollElasticity(m_frame->page() ? m_frame->page()->horizontalScrollElasticity() : ScrollElasticityAllowed);
    }
}

PassRefPtr<FrameView> FrameView::create(Frame& frame)
{
    RefPtr<FrameView> view = adoptRef(new FrameView(frame));
    view->show();
    return view.release();
}

PassRefPtr<FrameView> FrameView::create(Frame& frame, const IntSize& initialSize)
{
    RefPtr<FrameView> view = adoptRef(new FrameView(frame));
    view->Widget::setFrameRect(IntRect(view->location(), initialSize));
    view->show();
    return view.release();
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
    m_layoutRoot = 0;
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
    m_maintainScrollPositionAnchor = 0;
}

void FrameView::removeFromAXObjectCache()
{
    if (AXObjectCache* cache = axObjectCache())
        cache->remove(this);
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
    if (ownerElement && (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))) {
        HTMLFrameElementBase* frameElt = toHTMLFrameElementBase(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            setCanHaveScrollbars(false);
        LayoutUnit marginWidth = frameElt->marginWidth();
        LayoutUnit marginHeight = frameElt->marginHeight();
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
            scrollingCoordinator->willDestroyScrollableArea(this);
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
    ScrollbarOverlayStyle overlayStyle = ScrollbarOverlayStyleDefault;

    Color backgroundColor = documentBackgroundColor();
    if (backgroundColor.isValid()) {
        // Reduce the background color from RGB to a lightness value
        // and determine which scrollbar style to use based on a lightness
        // heuristic.
        double hue, saturation, lightness;
        backgroundColor.getHSL(hue, saturation, lightness);
        if (lightness <= .5 && backgroundColor.alpha() > 0)
            overlayStyle = ScrollbarOverlayStyleLight;
    }

    if (oldOverlayStyle != overlayStyle)
        setScrollbarOverlayStyle(overlayStyle);
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
    IntRect oldRect = frameRect();
    if (newRect == oldRect)
        return;

#if ENABLE(TEXT_AUTOSIZING)
    // Autosized font sizes depend on the width of the viewing area.
    if (newRect.width() != oldRect.width()) {
        Page* page = frame().page();
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
    Element* body = doc ? doc->body() : 0;
    if (body && body->renderer() && body->renderer()->style().hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, body);
    
    // If the <body> didn't have a custom style, then the root element might.
    Element* docElement = doc ? doc->documentElement() : 0;
    if (docElement && docElement->renderer() && docElement->renderer()->style().hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, docElement);
        
    // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
    RenderWidget* frameRenderer = frame().ownerRenderer();
    if (frameRenderer && frameRenderer->style().hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, 0, &frame());
    
    // Nobody set a custom style, so we just use a native scrollbar.
    return ScrollView::createScrollbar(orientation);
}

void FrameView::setContentsSize(const IntSize& size)
{
    if (size == contentsSize())
        return;

    m_deferSetNeedsLayouts++;

    ScrollView::setContentsSize(size);
    ScrollView::contentsResized();
    
    Page* page = frame().page();
    if (!page)
        return;

    updateScrollableAreaSet();

    page->chrome().contentsSizeChanged(&frame(), size); // Notify only.

    ASSERT(m_deferSetNeedsLayouts);
    m_deferSetNeedsLayouts--;
    
    if (!m_deferSetNeedsLayouts)
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
    
    setContentsSize(size);
}

void FrameView::applyOverflowToViewport(RenderElement* o, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.

    // To combat the inability to scroll on a page with overflow:hidden on the root when scaled, disregard hidden when
    // there is a frameScaleFactor that is greater than one on the main frame. Also disregard hidden if there is a
    // header or footer.

    bool overrideHidden = frame().isMainFrame() && ((frame().frameScaleFactor() > 1) || headerHeight() || footerHeight());

    EOverflow overflowX = o->style().overflowX();
    EOverflow overflowY = o->style().overflowY();

    if (o->isSVGRoot()) {
        // FIXME: evaluate if we can allow overflow for these cases too.
        // Overflow is always hidden when stand-alone SVG documents are embedded.
        if (toRenderSVGRoot(o)->isEmbeddedThroughFrameContainingSVGDocument()) {
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

    m_viewportRenderer = o;
}

void FrameView::applyPaginationToViewport()
{
    Document* document = frame().document();
    auto documentElement = document->documentElement();
    RenderElement* documentRenderer = documentElement ? documentElement->renderer() : nullptr;
    RenderElement* documentOrBodyRenderer = documentRenderer;
    auto body = document->body();
    if (body && body->renderer()) {
        if (body->hasTagName(bodyTag))
            documentOrBodyRenderer = documentRenderer->style().overflowX() == OVISIBLE && documentElement->hasTagName(htmlTag) ? body->renderer() : documentRenderer;
    }

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
    m_viewportRenderer = 0;

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
    
    if (!m_layoutRoot) {
        Document* document = frame().document();
        auto documentElement = document->documentElement();
        RenderElement* rootRenderer = documentElement ? documentElement->renderer() : nullptr;
        auto body = document->body();
        if (body && body->renderer()) {
            if (body->hasTagName(framesetTag) && !frameFlatteningEnabled()) {
                vMode = ScrollbarAlwaysOff;
                hMode = ScrollbarAlwaysOff;
            } else if (body->hasTagName(bodyTag)) {
                // It's sufficient to just check the X overflow,
                // since it's illegal to have visible in only one direction.
                RenderElement* o = rootRenderer->style().overflowX() == OVISIBLE && document->documentElement()->hasTagName(htmlTag) ? body->renderer() : rootRenderer;
                applyOverflowToViewport(o, hMode, vMode);
            }
        } else if (rootRenderer)
            applyOverflowToViewport(rootRenderer, hMode, vMode);
    }    
}

void FrameView::updateCompositingLayersAfterStyleChange()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    // If we expect to update compositing after an incipient layout, don't do so here.
    if (inPreLayoutStyleUpdate() || layoutPending() || renderView->needsLayout())
        return;

    RenderLayerCompositor& compositor = renderView->compositor();
    // This call will make sure the cached hasAcceleratedCompositing is updated from the pref
    compositor.cacheAcceleratedCompositingFlags();
    compositor.updateCompositingLayers(CompositingUpdateAfterStyleChange);
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
        return 0;
    return renderView->compositor().scrollLayer();
}

GraphicsLayer* FrameView::layerForHorizontalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor().layerForHorizontalScrollbar();
}

GraphicsLayer* FrameView::layerForVerticalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor().layerForVerticalScrollbar();
}

GraphicsLayer* FrameView::layerForScrollCorner() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor().layerForScrollCorner();
}

TiledBacking* FrameView::tiledBacking() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return 0;

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

    return backing->scrollingNodeIDForRole(FrameScrollingNode);
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
        return 0;
    return renderView->compositor().layerForOverhangAreas();
}

GraphicsLayer* FrameView::setWantsLayerForTopOverHangArea(bool wantsLayer) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;

    return renderView->compositor().updateLayerForTopOverhangArea(wantsLayer);
}

GraphicsLayer* FrameView::setWantsLayerForBottomOverHangArea(bool wantsLayer) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;

    return renderView->compositor().updateLayerForBottomOverhangArea(wantsLayer);
}

#endif // ENABLE(RUBBER_BANDING)

bool FrameView::flushCompositingStateForThisFrame(Frame* rootFrameForFlush)
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

    renderView->compositor().flushPendingLayerChanges(rootFrameForFlush == &frame());

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

    updateScrollbars(scrollOffset());
    if (renderView->usesCompositing())
        renderView->compositor().frameViewDidChangeSize();

    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setTopContentInset(newTopContentInset);
}
    
bool FrameView::hasCompositedContent() const
{
    if (RenderView* renderView = this->renderView())
        return renderView->compositor().inCompositingMode();
    return false;
}

bool FrameView::hasCompositedContentIncludingDescendants() const
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
        RenderView* renderView = frame->contentRenderer();
        if (RenderLayerCompositor* compositor = renderView ? &renderView->compositor() : 0) {
            if (compositor->inCompositingMode())
                return true;

            if (!RenderLayerCompositor::allowsIndependentlyCompositedFrames(this))
                break;
        }
    }
    return false;
}

bool FrameView::hasCompositingAncestor() const
{
    for (Frame* frame = this->frame().tree().parent(); frame; frame = frame->tree().parent()) {
        if (FrameView* view = frame->view()) {
            if (view->hasCompositedContent())
                return true;
        }
    }
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
    bool allFramesFlushed = flushCompositingStateForThisFrame(&frame());
    
    for (Frame* child = frame().tree().firstChild(); child; child = child->tree().traverseNext(&frame())) {
        bool flushed = child->view()->flushCompositingStateForThisFrame(&frame());
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

RenderObject* FrameView::layoutRoot(bool onlyDuringLayout) const
{
    return onlyDuringLayout && layoutPending() ? 0 : m_layoutRoot;
}

inline void FrameView::forceLayoutParentViewIfNeeded()
{
    RenderWidget* ownerRenderer = frame().ownerRenderer();
    if (!ownerRenderer)
        return;

    RenderBox* contentBox = embeddedContentBox();
    if (!contentBox)
        return;

    RenderSVGRoot* svgRoot = toRenderSVGRoot(contentBox);
    if (svgRoot->everHadLayout() && !svgRoot->needsLayout())
        return;

    // If the embedded SVG document appears the first time, the ownerRenderer has already finished
    // layout without knowing about the existence of the embedded SVG document, because RenderReplaced
    // embeddedContentBox() returns 0, as long as the embedded document isn't loaded yet. Before
    // bothering to lay out the SVG document, mark the ownerRenderer needing layout and ask its
    // FrameView for a layout. After that the RenderEmbeddedObject (ownerRenderer) carries the
    // correct size, which RenderSVGRoot::computeReplacedLogicalWidth/Height rely on, when laying
    // out for the first time, or when the RenderSVGRoot size has changed dynamically (eg. via <script>).
    Ref<FrameView> frameView(ownerRenderer->view().frameView());

    // Mark the owner renderer as needing layout.
    ownerRenderer->setNeedsLayoutAndPrefWidthsRecalc();

    // Synchronously enter layout, to layout the view containing the host object/embed/iframe.
    frameView->layout();
}

void FrameView::layout(bool allowSubtree)
{
    if (isInLayout())
        return;

    // Protect the view from being deleted during layout (in recalcStyle).
    Ref<FrameView> protect(*this);

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

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willLayout(&frame());

    if (!allowSubtree && m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }

    ASSERT(frame().view() == this);
    ASSERT(frame().document());

    Document& document = *frame().document();
    ASSERT(!document.inPageCache());

    bool subtree;
    RenderElement* root;

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
            document.styleResolverChanged(DeferRecalcStyle);
            // FIXME: This instrumentation event is not strictly accurate since cached media query results do not persist across StyleResolver rebuilds.
            InspectorInstrumentation::mediaQueryResultChanged(&document);
        } else
            document.evaluateMediaQueryList();

        // If there is any pagination to apply, it will affect the RenderView's style, so we should
        // take care of that now.
        applyPaginationToViewport();

        // Always ensure our style info is up-to-date. This can happen in situations where
        // the layout beats any sort of style recalc update that needs to occur.
        document.updateStyleIfNeeded();
        m_layoutPhase = InPreLayout;

        subtree = m_layoutRoot;

        // If there is only one ref to this view left, then its going to be destroyed as soon as we exit, 
        // so there's no point to continuing to layout
        if (hasOneRef())
            return;

        root = subtree ? m_layoutRoot : document.renderView();
        if (!root) {
            // FIXME: Do we need to set m_size here?
            return;
        }

        // Close block here so we can set up the font cache purge preventer, which we will still
        // want in scope even after we want m_layoutSchedulingEnabled to be restored again.
        // The next block sets m_layoutSchedulingEnabled back to false once again.
    }

    FontCachePurgePreventer fontCachePurgePreventer;
    RenderLayer* layer;

    ++m_nestedLayoutCount;

    {
        TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

        if (!m_layoutRoot) {
            HTMLElement* body = document.body();
            if (body && body->renderer()) {
                if (body->hasTagName(framesetTag) && !frameFlatteningEnabled()) {
                    body->renderer()->setChildNeedsLayout();
                } else if (body->hasTagName(bodyTag)) {
                    if (!m_firstLayout && m_size.height() != layoutHeight() && body->renderer()->enclosingBox().stretchesToViewport())
                        body->renderer()->setChildNeedsLayout();
                }
            }

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (m_firstLayout && !frame().ownerElement())
                printf("Elapsed time before first layout: %lld\n", document.elapsedTime().count());
#endif        
        }

        autoSizeIfEnabled();

        m_needsFullRepaint = !subtree && (m_firstLayout || toRenderView(*root).printing());

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

                    setScrollbarModes(hMode, vMode);
                    setScrollbarsSuppressed(false, true);
                } else
                    setScrollbarModes(hMode, vMode);
            }

            LayoutSize oldSize = m_size;

            m_size = layoutSize();

            if (oldSize != m_size) {
                m_needsFullRepaint = true;
                if (!m_firstLayout) {
                    RenderBox* rootRenderer = document.documentElement() ? document.documentElement()->renderBox() : 0;
                    RenderBox* bodyRenderer = rootRenderer && document.body() ? document.body()->renderBox() : 0;
                    if (bodyRenderer && bodyRenderer->stretchesToViewport())
                        bodyRenderer->setChildNeedsLayout();
                    else if (rootRenderer && rootRenderer->stretchesToViewport())
                        rootRenderer->setChildNeedsLayout();

                    document.updateViewportUnitsOnResize();
                }
            }

            m_layoutPhase = InPreLayout;
        }

        layer = root->enclosingLayer();

        bool disableLayoutState = false;
        if (subtree) {
            disableLayoutState = root->view().shouldDisableLayoutStateForSubtree(root);
            root->view().pushLayoutState(*root);
        }
        LayoutStateDisabler layoutStateDisabler(disableLayoutState ? &root->view() : 0);
        RenderView::RepaintRegionAccumulator repaintRegionAccumulator(&root->view());

        ASSERT(m_layoutPhase == InPreLayout);
        m_layoutPhase = InLayout;

        forceLayoutParentViewIfNeeded();

        ASSERT(m_layoutPhase == InLayout);

        root->layout();
#if ENABLE(IOS_TEXT_AUTOSIZING)
        float minZoomFontSize = frame().settings().minimumZoomFontSize();
        float visWidth = frame().page()->textAutosizingWidth();
        if (minZoomFontSize && visWidth && !root->view().printing()) {
            root->adjustComputedFontSizesOnBlocks(minZoomFontSize, visWidth);    
            bool needsLayout = root->needsLayout();
            if (needsLayout)
                root->layout();
        }
#endif
#if ENABLE(TEXT_AUTOSIZING)
        if (document.textAutosizer()->processSubtree(root) && root->needsLayout())
            root->layout();
#endif

        ASSERT(m_layoutPhase == InLayout);

        if (subtree)
            root->view().popLayoutState(*root);

        m_layoutRoot = 0;

        // Close block here to end the scope of changeSchedulingEnabled and layoutStateDisabler.
    }

    m_layoutPhase = InViewSizeAdjust;

    bool neededFullRepaint = m_needsFullRepaint;

    if (!subtree && !toRenderView(*root).printing())
        adjustViewSize();

    m_layoutPhase = InPostLayout;

    m_needsFullRepaint = neededFullRepaint;

    // Now update the positions of all layers.
    if (m_needsFullRepaint)
        root->view().repaintRootContents();

    layer->updateLayerPositionsAfterLayout(renderView()->layer(), updateLayerPositionFlags(layer, subtree, m_needsFullRepaint));

    updateCompositingLayersAfterLayout();
    
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

    ASSERT(!root->needsLayout());

    updateCanBlitOnScrollRecursively();

    if (document.hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(layoutWidth() < contentsWidth(), layoutHeight() < contentsHeight());

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

    --m_nestedLayoutCount;
}

RenderBox* FrameView::embeddedContentBox() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    RenderObject* firstChild = renderView->firstChild();
    if (!firstChild || !firstChild->isBox())
        return nullptr;

    // Curently only embedded SVG documents participate in the size-negotiation logic.
    if (toRenderBox(firstChild)->isSVGRoot())
        return toRenderBox(firstChild);

    return nullptr;
}

void FrameView::addEmbeddedObjectToUpdate(RenderEmbeddedObject& embeddedObject)
{
    if (!m_embeddedObjectsToUpdate)
        m_embeddedObjectsToUpdate = std::make_unique<ListHashSet<RenderEmbeddedObject*>>();

    HTMLFrameOwnerElement& element = embeddedObject.frameOwnerElement();
    if (isHTMLObjectElement(element) || isHTMLEmbedElement(element)) {
        // Tell the DOM element that it needs a widget update.
        HTMLPlugInImageElement& pluginElement = toHTMLPlugInImageElement(element);
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
    InspectorInstrumentation::applyEmulatedMedia(&frame(), &overrideType);
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
    if (contentsInCompositedLayer() && !platformWidget())
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
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
        if (FrameView* view = frame->view())
            view->setCanBlitOnScroll(!view->useSlowRepaints());
    }
}

bool FrameView::contentsInCompositedLayer() const
{
    RenderView* renderView = this->renderView();
    if (renderView && renderView->isComposited()) {
        GraphicsLayer* layer = renderView->layer()->backing()->graphicsLayer();
        if (layer && layer->drawsContent())
            return true;
    }

    return false;
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
        m_slowRepaintObjects = std::make_unique<HashSet<RenderElement*>>();

    m_slowRepaintObjects->add(o);

    if (!hadSlowRepaintObjects) {
        updateCanBlitOnScrollRecursively();

        if (Page* page = frame().page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
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
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
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
                scrollingCoordinator->frameViewFixedObjectsDidChange(this);
        }
    }
}

void FrameView::removeViewportConstrainedObject(RenderElement* object)
{
    if (m_viewportConstrainedObjects && m_viewportConstrainedObjects->remove(object)) {
        if (Page* page = frame().page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(this);
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

    viewportRect.setLocation(toLayoutPoint(scrollOffsetForFixedPosition()));
    return viewportRect;
}

LayoutSize FrameView::scrollOffsetForFixedPosition(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements behaviorForFixed, int headerHeight, int footerHeight)
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

    return LayoutSize(position.x() * dragFactorX / frameScaleFactor, position.y() * dragFactorY / frameScaleFactor);
}

LayoutSize FrameView::scrollOffsetForFixedPosition() const
{
    IntRect visibleContentRect = this->visibleContentRect();
    IntSize totalContentsSize = this->totalContentsSize();
    IntPoint scrollPosition = this->scrollPosition();
    IntPoint scrollOrigin = this->scrollOrigin();
    float frameScaleFactor = frame().frameScaleFactor();
    ScrollBehaviorForFixedElements behaviorForFixed = scrollBehaviorForFixedElements();
    return scrollOffsetForFixedPosition(visibleContentRect, totalContentsSize, scrollPosition, scrollOrigin, frameScaleFactor, fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight());
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

float FrameView::yPositionForRootContentLayer(const FloatPoint& scrollPosition, float topContentInset, float headerHeight)
{
    return yPositionForHeaderLayer(scrollPosition, topContentInset) + headerHeight;
}

float FrameView::yPositionForFooterLayer(const FloatPoint& scrollPosition, float topContentInset, float totalContentsHeight, float footerHeight)
{
    return yPositionForHeaderLayer(scrollPosition, topContentInset) + totalContentsHeight - footerHeight;
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
    
IntPoint FrameView::minimumScrollPosition() const
{
    IntPoint minimumPosition(ScrollView::minimumScrollPosition());

    if (frame().isMainFrame() && m_scrollPinningBehavior == PinToBottom)
        minimumPosition.setY(maximumScrollPosition().y());
    
    return minimumPosition;
}

IntPoint FrameView::maximumScrollPosition() const
{
    IntPoint maximumOffset(contentsWidth() - visibleWidth() - scrollOrigin().x(), totalContentsSize().height() - visibleHeight() - scrollOrigin().y());

    maximumOffset.clampNegativeToZero();

    if (frame().isMainFrame() && m_scrollPinningBehavior == PinToTop)
        maximumOffset.setY(minimumScrollPosition().y());
    
    return maximumOffset;
}

void FrameView::delayedScrollEventTimerFired(Timer<FrameView>&)
{
    sendScrollEvent();
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

    const bool isCompositedContentLayer = contentsInCompositedLayer();

    // Get the rects of the fixed objects visible in the rectToScroll
    Region regionToUpdate;
    for (auto& renderer : *m_viewportConstrainedObjects) {
        if (!renderer->style().hasViewportConstrainedPosition())
            continue;
        if (renderer->isComposited())
            continue;

        // Fixed items should always have layers.
        ASSERT(renderer->hasLayer());
        RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();

        if (layer->viewportConstrainedNotCompositedReason() == RenderLayer::NotCompositedForBoundsOutOfView
            || layer->viewportConstrainedNotCompositedReason() == RenderLayer::NotCompositedForNoVisibleContent) {
            // Don't invalidate for invisible fixed layers.
            continue;
        }

#if ENABLE(CSS_FILTERS)
        if (layer->hasAncestorWithFilterOutsets()) {
            // If the fixed layer has a blur/drop-shadow filter applied on at least one of its parents, we cannot 
            // scroll using the fast path, otherwise the outsets of the filter will be moved around the page.
            return false;
        }
#endif
        IntRect updateRect = pixelSnappedIntRect(layer->repaintRectIncludingNonCompositingDescendants());
        updateRect = contentsToRootView(updateRect);
        if (!isCompositedContentLayer && clipsRepaints())
            updateRect.intersect(rectToScroll);
        if (!updateRect.isEmpty())
            regionToUpdate.unite(updateRect);
    }

    // 1) scroll
    hostWindow()->scroll(scrollDelta, rectToScroll, clipRect);

    // 2) update the area of fixed objects that has been invalidated
    Vector<IntRect> subRectsToUpdate = regionToUpdate.rects();
    size_t viewportConstrainedObjectsCount = subRectsToUpdate.size();
    for (size_t i = 0; i < viewportConstrainedObjectsCount; ++i) {
        IntRect updateRect = subRectsToUpdate[i];
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
    if (contentsInCompositedLayer()) {
        // FIXME: respect paintsEntireContents()?
        IntRect updateRect = visibleContentRect(LegacyIOSDocumentVisibleRect);

        // Make sure to "apply" the scale factor here since we're converting from frame view
        // coordinates to layer backing coordinates.
        updateRect.scale(1 / frame().frameScaleFactor());

        ASSERT(renderView());
        renderView()->layer()->setBackingNeedsRepaintInRect(updateRect, GraphicsLayer::DoNotClipToLayer);
    }

    repaintSlowRepaintObjects();

    if (RenderWidget* frameRenderer = frame().ownerRenderer()) {
        if (isEnclosedInCompositingLayer()) {
            LayoutRect rect(frameRenderer->borderLeft() + frameRenderer->paddingLeft(),
                            frameRenderer->borderTop() + frameRenderer->paddingTop(),
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

    if (hasCompositedContentIncludingDescendants()) {
        // Overlap can affect compositing tests, so if it changes, we need to trigger
        // a layer update in the parent document.
        if (Frame* parentFrame = frame().tree().parent()) {
            if (RenderView* parentView = parentFrame->contentRenderer()) {
                RenderLayerCompositor& compositor = parentView->compositor();
                compositor.setCompositingLayersNeedRebuild();
                compositor.scheduleCompositingLayerUpdate();
            }
        }

        if (RenderLayerCompositor::allowsIndependentlyCompositedFrames(this)) {
            // We also need to trigger reevaluation for this and all descendant frames,
            // since a frame uses compositing if any ancestor is compositing.
            for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
                if (RenderView* view = frame->contentRenderer()) {
                    RenderLayerCompositor& compositor = view->compositor();
                    compositor.setCompositingLayersNeedRebuild();
                    compositor.scheduleCompositingLayerUpdate();
                }
            }
        }
    }
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

    if (!frame().document()->haveStylesheetsLoaded()) {
        frame().document()->setGotoAnchorNeededAfterStylesheetsLoad(true);
        return false;
    }

    frame().document()->setGotoAnchorNeededAfterStylesheetsLoad(false);

    Element* anchorElement = frame().document()->findAnchor(name);

    // Setting to null will clear the current target.
    frame().document()->setCSSTarget(anchorElement);

    if (frame().document()->isSVGDocument()) {
        if (SVGSVGElement* svg = toSVGDocument(frame().document())->rootElement()) {
            svg->setupInitialView(name, anchorElement);
            if (!anchorElement)
                return true;
        }
    }
  
    // Implement the rule that "" and "top" both mean top of page as in other browsers.
    if (!anchorElement && !(name.isEmpty() || equalIgnoringCase(name, "top")))
        return false;

    maintainScrollPositionAtAnchor(anchorElement ? static_cast<Node*>(anchorElement) : frame().document());
    
    // If the anchor accepts keyboard focus, move focus there to aid users relying on keyboard navigation.
    if (anchorElement && anchorElement->isFocusable())
        frame().document()->setFocusedElement(anchorElement);
    
    return true;
}

void FrameView::maintainScrollPositionAtAnchor(Node* anchorNode)
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

void FrameView::scrollElementToRect(Element* element, const IntRect& rect)
{
    frame().document()->updateLayoutIgnorePendingStylesheets();

    LayoutRect bounds = element->boundingBox();
    int centeringOffsetX = (rect.width() - bounds.width()) / 2;
    int centeringOffsetY = (rect.height() - bounds.height()) / 2;
    setScrollPosition(IntPoint(bounds.x() - centeringOffsetX - rect.x(), bounds.y() - centeringOffsetY - rect.y()));
}

void FrameView::setScrollPosition(const IntPoint& scrollPoint)
{
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);
    m_maintainScrollPositionAnchor = 0;
    ScrollView::setScrollPosition(scrollPoint);
}

void FrameView::delegatesScrollingDidChange()
{
    // When we switch to delgatesScrolling mode, we should destroy the scrolling/clipping layers in RenderLayerCompositor.
    if (hasCompositedContent())
        clearBackingStores();
}

#if USE(TILED_BACKING_STORE)
void FrameView::setFixedVisibleContentRect(const IntRect& visibleContentRect)
{
    bool visibleContentSizeDidChange = false;
    if (visibleContentRect.size() != this->fixedVisibleContentRect().size()) {
        // When the viewport size changes or the content is scaled, we need to
        // reposition the fixed and sticky positioned elements.
        setViewportConstrainedObjectsNeedLayout();
        visibleContentSizeDidChange = true;
    }

    IntSize offset = scrollOffset();
    IntPoint oldPosition = scrollPosition();
    ScrollView::setFixedVisibleContentRect(visibleContentRect);
    if (offset != scrollOffset()) {
        updateLayerPositionsAfterScrolling();
        if (frame().page()->settings().acceleratedCompositingForFixedPositionEnabled())
            updateCompositingLayersAfterScrolling();
        IntPoint newPosition = scrollPosition();
        scrollAnimator()->setCurrentPosition(scrollPosition());
        scrollPositionChanged(oldPosition, newPosition);
    }
    if (visibleContentSizeDidChange) {
        // Update the scroll-bars to calculate new page-step size.
        updateScrollbars(scrollOffset());
    }
    frame().loader().client().didChangeScrollOffset();
}
#endif

void FrameView::setViewportConstrainedObjectsNeedLayout()
{
    if (!hasViewportConstrainedObjects())
        return;

    for (auto& renderer : *m_viewportConstrainedObjects)
        renderer->setNeedsLayout();
}

void FrameView::scrollPositionChangedViaPlatformWidget(const IntPoint& oldPosition, const IntPoint& newPosition)
{
    updateLayerPositionsAfterScrolling();
    updateCompositingLayersAfterScrolling();
    repaintSlowRepaintObjects();
    scrollPositionChanged(oldPosition, newPosition);
}

void FrameView::scrollPositionChanged(const IntPoint& oldPosition, const IntPoint& newPosition)
{
    std::chrono::milliseconds throttlingDelay = frame().page()->chrome().client().eventThrottlingDelay();

    if (throttlingDelay == std::chrono::milliseconds::zero()) {
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

    resumeVisibleImageAnimationsIncludingSubframes();
}

void FrameView::resumeVisibleImageAnimationsIncludingSubframes()
{
    // A change in scroll position may affect image visibility in subframes.
    for (auto* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
        if (auto* renderView = frame->contentRenderer())
            renderView->resumePausedImageAnimationsIfNeeded();
    }
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

    if (&page->mainFrame() != &frame())
        return true;

    ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return true;

    if (!scrollingCoordinator->supportsFixedPositionLayers())
        return true;

    if (scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously())
        return true;

    if (inProgrammaticScroll())
        return true;

    return false;
#endif
    return true;
}

void FrameView::updateCompositingLayersAfterScrolling()
{
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
            if (!scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously())
                return scrollingCoordinator->isRubberBandInProgress();
        }
    }

    // If the main thread updates the scroll position for this FrameView, we should return
    // ScrollAnimator::isRubberBandInProgress().
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();

    return false;
}

bool FrameView::requestScrollPositionUpdate(const IntPoint& position)
{
#if ENABLE(ASYNC_SCROLLING)
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->prepopulateRect(FloatRect(position, visibleContentRect().size()));

    if (Page* page = frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            return scrollingCoordinator->requestScrollPositionUpdate(this, position);
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
    return 0;
}

void FrameView::addTrackedRepaintRect(const FloatRect& r)
{
    if (!m_isTrackingRepaints || r.isEmpty())
        return;

    FloatRect repaintRect = r;
    repaintRect.move(-scrollOffset());
    m_trackedRepaintRects.append(repaintRect);
}

void FrameView::repaintContentRectangle(const IntRect& r)
{
    ASSERT(!frame().ownerElement());

    if (!shouldUpdate())
        return;

    ScrollView::repaintContentRectangle(r);
}

static unsigned countRenderedCharactersInRenderObjectWithThreshold(const RenderObject& renderer, unsigned countSoFar, unsigned threshold)
{
    // FIXME: Consider writing this using RenderObject::nextInPreOrder() instead of using recursion.
    if (renderer.isText())
        countSoFar += toRenderText(renderer).text()->length();

    for (RenderObject* child = renderer.firstChildSlow(); child; child = child->nextSibling()) {
        if (countSoFar >= threshold)
            break;
        countSoFar = countRenderedCharactersInRenderObjectWithThreshold(*child, countSoFar, threshold);
    }
    return countSoFar;
}

bool FrameView::renderedCharactersExceed(unsigned threshold)
{
    if (!m_frame->contentRenderer())
        return false;
    return countRenderedCharactersInRenderObjectWithThreshold(*m_frame->contentRenderer(), 0, threshold) >= threshold;
}

void FrameView::contentsResized()
{
    ScrollView::contentsResized();
    setNeedsLayout();
}

void FrameView::fixedLayoutSizeChanged()
{
    // Can be triggered before the view is set, see comment in FrameView::visibleContentsResized().
    // An ASSERT is triggered when a view schedules a layout before being attached to a frame.
    if (!frame().view())
        return;
    ScrollView::fixedLayoutSizeChanged();
}

bool FrameView::shouldLayoutAfterContentsResized() const
{
    return !useFixedLayout() || useCustomFixedPositionLayoutRect();
}

void FrameView::visibleContentsResized()
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
}

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
    if (!document || isImageDocument(*document))
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
    ASSERT(frame().isMainFrame());
    auto& page = *frame().page();

    LayerFlushThrottleState::Flags flags = determineLayerFlushThrottleState(page);

    // See if the client is handling throttling.
    if (page.chrome().client().adjustLayerFlushThrottling(flags))
        return;

    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
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
    return view.isVisuallyNonEmpty() && !view.frame().page()->progress().isMainLoadProgressing();
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

void FrameView::speculativeTilingEnableTimerFired(Timer<FrameView>&)
{
    if (m_speculativeTilingEnabled)
        return;
    m_speculativeTilingEnabled = shouldEnableSpeculativeTilingDuringLoading(*this);
    adjustTiledBackingCoverage();
}

void FrameView::layoutTimerFired(Timer<FrameView>&)
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

    if (m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }
    if (!m_layoutSchedulingEnabled)
        return;
    if (!needsLayout())
        return;
    if (!frame().document()->shouldScheduleLayout())
        return;
    InspectorInstrumentation::didInvalidateLayout(&frame());
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
    RenderView& renderView = *this->renderView();

    // Try to catch unnecessary work during render tree teardown.
    ASSERT(!renderView.documentBeingDestroyed());
    ASSERT(frame().view() == this);

    if (renderView.needsLayout()) {
        newRelayoutRoot.markContainingBlocksForLayout(false);
        return;
    }

    if (!layoutPending() && m_layoutSchedulingEnabled) {
        std::chrono::milliseconds delay = renderView.document().minimumLayoutDelay();
        ASSERT(!newRelayoutRoot.container() || !newRelayoutRoot.container()->needsLayout());
        m_layoutRoot = &newRelayoutRoot;
        InspectorInstrumentation::didInvalidateLayout(&frame());
        m_delayedLayout = delay.count();
        m_layoutTimer.startOneShot(delay);
        return;
    }

    if (m_layoutRoot == &newRelayoutRoot)
        return;

    if (!m_layoutRoot) {
        // Just relayout the subtree.
        newRelayoutRoot.markContainingBlocksForLayout(false);
        InspectorInstrumentation::didInvalidateLayout(&frame());
        return;
    }

    if (isObjectAncestorContainerOf(m_layoutRoot, &newRelayoutRoot)) {
        // Keep the current root.
        newRelayoutRoot.markContainingBlocksForLayout(false, m_layoutRoot);
        ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
        return;
    }

    if (isObjectAncestorContainerOf(&newRelayoutRoot, m_layoutRoot)) {
        // Re-root at newRelayoutRoot.
        m_layoutRoot->markContainingBlocksForLayout(false, &newRelayoutRoot);
        m_layoutRoot = &newRelayoutRoot;
        ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
        InspectorInstrumentation::didInvalidateLayout(&frame());
        return;
    }

    // Just do a full relayout.
    m_layoutRoot->markContainingBlocksForLayout(false);
    m_layoutRoot = 0;
    newRelayoutRoot.markContainingBlocksForLayout(false);
    InspectorInstrumentation::didInvalidateLayout(&frame());
}

bool FrameView::layoutPending() const
{
    return m_layoutTimer.isActive();
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
        || (m_deferSetNeedsLayouts && m_setNeedsLayoutWasDeferred);
}

void FrameView::setNeedsLayout()
{
    if (m_deferSetNeedsLayouts) {
        m_setNeedsLayoutWasDeferred = true;
        return;
    }

    if (RenderView* renderView = this->renderView())
        renderView->setNeedsLayout();
}

void FrameView::unscheduleRelayout()
{
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
void FrameView::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext()) {
        frame->view()->serviceScrollAnimations();
        frame->animation().serviceAnimations();
    }

    Vector<RefPtr<Document>> documents;
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext())
        documents.append(frame->document());

    for (size_t i = 0; i < documents.size(); ++i)
        documents[i]->serviceScriptedAnimations(monotonicAnimationStartTime);
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

    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    RenderLayerCompositor& compositor = renderView->compositor();
    compositor.setCompositingLayersNeedRebuild();
    compositor.scheduleCompositingLayerUpdate();
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
    if (!backgroundColor.isValid())
        m_baseBackgroundColor = Color::white;
    else
        m_baseBackgroundColor = backgroundColor;

    recalculateScrollbarOverlayStyle();
}

void FrameView::updateBackgroundRecursively(const Color& backgroundColor, bool transparent)
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
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
#endif

    if (!frame().settings().backgroundShouldExtendBeyondPage())
        return ExtendedBackgroundModeNone;

    if (!frame().isMainFrame())
        return ExtendedBackgroundModeNone;

    Document* document = frame().document();
    if (!document)
        return ExtendedBackgroundModeNone;

    auto documentElement = document->documentElement();
    auto documentElementRenderer = documentElement ? documentElement->renderer() : nullptr;
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
    
    LayoutRect extendedRect = renderView->unextendedBackgroundRect(renderView);
    if (!tiledBacking->hasMargins())
        return pixelSnappedIntRect(extendedRect);
    
    extendedRect.moveBy(LayoutPoint(-tiledBacking->leftMarginWidth(), -tiledBacking->topMarginHeight()));
    extendedRect.expand(LayoutSize(tiledBacking->leftMarginWidth() + tiledBacking->rightMarginWidth(), tiledBacking->topMarginHeight() + tiledBacking->bottomMarginHeight()));
    return pixelSnappedIntRect(extendedRect);
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
    RefPtr<Node> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;

    if (!anchorNode->renderer())
        return;

    LayoutRect rect;
    if (anchorNode != frame().document())
        rect = anchorNode->boundingBox();

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    anchorNode->renderer()->scrollRectToVisible(rect, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);

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
        if (isHTMLObjectElement(element) || isHTMLEmbedElement(element)) {
            HTMLPlugInImageElement& pluginElement = toHTMLPlugInImageElement(element);
            pluginElement.checkSnapshotStatus();
        }
        return;
    }

    auto weakRenderer = embeddedObject.createWeakPtr();

    // FIXME: This could turn into a real virtual dispatch if we defined
    // updateWidget(PluginCreationOption) on HTMLElement.
    if (isHTMLObjectElement(element) || isHTMLEmbedElement(element) || isHTMLAppletElement(element)) {
        HTMLPlugInImageElement& pluginElement = toHTMLPlugInImageElement(element);
        if (pluginElement.needsCheckForSizeChange()) {
            pluginElement.checkSnapshotStatus();
            return;
        }
        if (pluginElement.needsWidgetUpdate())
            pluginElement.updateWidget(CreateAnyWidgetType);
    } else
        ASSERT_NOT_REACHED();

    // It's possible the renderer was destroyed below updateWidget() since loading a plugin may execute arbitrary JavaScript.
    if (!weakRenderer)
        return;

    embeddedObject.updateWidgetPosition();
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

void FrameView::updateEmbeddedObjectsTimerFired(Timer<FrameView>*)
{
    RefPtr<FrameView> protect(this);
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
        updateEmbeddedObjectsTimerFired(nullptr);
}

void FrameView::performPostLayoutTasks()
{
    // FIXME: We should not run any JavaScript code in this function.

    m_postLayoutTasksTimer.stop();

    frame().selection().didLayout();

    if (m_nestedLayoutCount <= 1 && frame().document()->documentElement())
        fireLayoutRelatedMilestonesIfNeeded();

#if PLATFORM(IOS)
    // Only send layout-related delegate callbacks synchronously for the main frame to
    // avoid re-entering layout for the main frame while delivering a layout-related delegate
    // callback for a subframe.
    if (frame().isMainFrame())
        frame().page()->chrome().client().didLayout();
#endif

#if ENABLE(FONT_LOAD_EVENTS)
    if (RuntimeEnabledFeatures::sharedFeatures().fontLoadEventsEnabled())
        frame().document()->fontloader()->didLayout();
#endif
    
    // FIXME: We should consider adding DidLayout as a LayoutMilestone. That would let us merge this
    // with didLayout(LayoutMilestones).
    frame().loader().client().dispatchDidLayout();

    updateWidgetPositions();
    
    // layout() protects FrameView, but it still can get destroyed when updateEmbeddedObjects()
    // is called through the post layout timer.
    Ref<FrameView> protect(*this);

    m_updateEmbeddedObjectsTimer.startOneShot(0);

    if (auto* page = frame().page()) {
        if (auto* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewLayoutUpdated(this);
    }

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidLayout();
    }

    scrollToAnchor();

    sendResizeEventIfNeeded();
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
    if (isInLayout() || needsLayout())
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

    RefPtr<Event> resizeEvent = Event::create(eventNames().resizeEvent, false, false);
    if (canSendResizeEventSynchronously)
        frame().document()->dispatchWindowEvent(resizeEvent.release());
    else {
        // FIXME: Queueing this event for an unpredictable time in the future seems
        // intrinsically racy. By the time this resize event fires, the frame might
        // be resized again, so we could end up with two resize events for the same size.
        frame().document()->enqueueWindowEvent(resizeEvent.release());
    }

#if ENABLE(INSPECTOR)
    if (InspectorInstrumentation::hasFrontends() && isMainFrame) {
        if (Page* page = frame().page()) {
            if (InspectorClient* inspectorClient = page->inspectorController().inspectorClient())
                inspectorClient->didResizeMainFrame(&frame());
        }
    }
#endif
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

void FrameView::postLayoutTimerFired(Timer<FrameView>&)
{
    performPostLayoutTasks();
}

void FrameView::autoSizeIfEnabled()
{
    if (!m_shouldAutoSize)
        return;

    if (m_inAutoSize)
        return;

    TemporaryChange<bool> changeInAutoSize(m_inAutoSize, true);

    Document* document = frame().document();
    if (!document)
        return;

    RenderView* documentView = document->renderView();
    Element* documentElement = document->documentElement();
    if (!documentView || !documentElement)
        return;

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
            if (!localHorizontalScrollbar->isOverlayScrollbar())
                newSize.setHeight(newSize.height() + localHorizontalScrollbar->height());

            // Don't bother checking for a vertical scrollbar because the width is at
            // already greater the maximum.
        } else if (newSize.height() > m_maxAutoSize.height()) {
            RefPtr<Scrollbar> localVerticalScrollbar = verticalScrollbar();
            if (!localVerticalScrollbar)
                localVerticalScrollbar = createScrollbar(VerticalScrollbar);
            if (!localVerticalScrollbar->isOverlayScrollbar())
                newSize.setWidth(newSize.width() + localVerticalScrollbar->width());

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

        resize(newSize.width(), newSize.height());
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

void FrameView::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    if (!m_viewportRenderer)
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

        RefPtr<OverflowEvent> overflowEvent = OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow,
            verticalOverflowChanged, verticalOverflow);
        overflowEvent->setTarget(m_viewportRenderer->element());

        frame().document()->enqueueOverflowEvent(overflowEvent.release());
    }
    
}

const Pagination& FrameView::pagination() const
{
    if (m_pagination != Pagination())
        return m_pagination;

    if (frame().isMainFrame())
        return frame().page()->pagination();

    return m_pagination;
}

void FrameView::setPagination(const Pagination& pagination)
{
    if (m_pagination == pagination)
        return;

    m_pagination = pagination;

    frame().document()->styleResolverChanged(DeferRecalcStyle);
}

IntRect FrameView::windowClipRect(bool clipToContents) const
{
    ASSERT(frame().view() == this);

    if (paintsEntireContents())
        return IntRect(IntPoint(), totalContentsSize());

    // Set our clip rect to be our contents.
    IntRect clipRect;
    if (clipToContents)
        clipRect = contentsToWindow(visibleContentRect(LegacyIOSDocumentVisibleRect));
    else
        clipRect = contentsToWindow(visibleContentRectIncludingScrollbars(LegacyIOSDocumentVisibleRect));
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
        clipRect = pixelSnappedIntRect(enclosingLayer->childrenClipRect());
    else
        clipRect = pixelSnappedIntRect(enclosingLayer->selfClipRect());
    clipRect = contentsToWindow(clipRect); 
    return intersection(clipRect, windowClipRect());
}

bool FrameView::isActive() const
{
    Page* page = frame().page();
    return page && page->focusController().isActive();
}

bool FrameView::updatesScrollLayerPositionOnMainThread() const
{
    if (Page* page = frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            return scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously();
    }

    return true;
}

bool FrameView::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    Page* page = frame().page();
    return page && page->settings().forceUpdateScrollbarsOnMainThreadForPerformanceTesting();
}

void FrameView::scrollTo(const IntSize& newOffset)
{
    LayoutSize offset = scrollOffset();
    IntPoint oldPosition = scrollPosition();
    ScrollView::scrollTo(newOffset);
    if (offset != scrollOffset())
        scrollPositionChanged(oldPosition, scrollPosition());
    frame().loader().client().didChangeScrollOffset();
}

void FrameView::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    // Add in our offset within the FrameView.
    IntRect dirtyRect = rect;
    dirtyRect.moveBy(scrollbar->location());
    invalidateRect(dirtyRect);
}

IntRect FrameView::windowResizerRect() const
{
    if (Page* page = frame().page())
        return page->chrome().windowResizerRect();
    return IntRect();
}

float FrameView::visibleContentScaleFactor() const
{
    if (!frame().isMainFrame() || !frame().settings().delegatesPageScaling())
        return 1;

    return frame().page()->pageScaleFactor();
}

void FrameView::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
{
    if (!frame().isMainFrame())
        return;

    frame().page()->chrome().client().notifyScrollerThumbIsVisibleInRect(scrollerThumb);
}

ScrollableArea* FrameView::enclosingScrollableArea() const
{
    // FIXME: Walk up the frame tree and look for a scrollable parent frame or RenderLayer.
    return 0;
}

IntRect FrameView::scrollableAreaBoundingBox() const
{
    RenderWidget* ownerRenderer = frame().ownerRenderer();
    if (!ownerRenderer)
        return frameRect();

    return ownerRenderer->absoluteContentQuad().enclosingBoundingBox();
}

bool FrameView::isScrollable()
{
    // Check for:
    // 1) If there an actual overflow.
    // 2) display:none or visibility:hidden set to self or inherited.
    // 3) overflow{-x,-y}: hidden;
    // 4) scrolling: no;

    // Covers #1
    IntSize totalContentsSize = this->totalContentsSize();
    IntSize visibleContentSize = visibleContentRect(LegacyIOSDocumentVisibleRect).size();
    if ((totalContentsSize.height() <= visibleContentSize.height() && totalContentsSize.width() <= visibleContentSize.width()))
        return false;

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

void FrameView::scrollbarStyleChanged(int newStyle, bool forceUpdate)
{
    if (!frame().isMainFrame())
        return;

    frame().page()->chrome().client().recommendedScrollbarStyleDidChange(newStyle);

    if (forceUpdate)
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
    RenderElement* renderer = 0;
    RefPtr<RenderStyle> cornerStyle;
    IntRect cornerRect = scrollCornerRect();
    
    if (!cornerRect.isEmpty()) {
        // Try the <body> element first as a scroll corner source.
        Document* doc = frame().document();
        Element* body = doc ? doc->body() : 0;
        if (body && body->renderer()) {
            renderer = body->renderer();
            cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), &renderer->style());
        }
        
        if (!cornerStyle) {
            // If the <body> didn't have a custom style, then the root element might.
            Element* docElement = doc ? doc->documentElement() : 0;
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
            m_scrollCorner = createRenderer<RenderScrollbarPart>(renderer->document(), cornerStyle.releaseNonNull());
            m_scrollCorner->initializeStyle();
        } else
            m_scrollCorner->setStyle(cornerStyle.releaseNonNull());
        invalidateScrollCorner(cornerRect);
    }

    ScrollView::updateScrollCorner();
}

void FrameView::paintScrollCorner(GraphicsContext* context, const IntRect& cornerRect)
{
    if (context->updatingControlTints()) {
        updateScrollCorner();
        return;
    }

    if (m_scrollCorner) {
        if (frame().isMainFrame())
            context->fillRect(cornerRect, baseBackgroundColor(), ColorSpaceDeviceRGB);
        m_scrollCorner->paintIntoRect(context, cornerRect.location(), cornerRect);
        return;
    }

    ScrollView::paintScrollCorner(context, cornerRect);
}

void FrameView::paintScrollbar(GraphicsContext* context, Scrollbar* bar, const IntRect& rect)
{
    if (bar->isCustomScrollbar() && frame().isMainFrame()) {
        IntRect toFill = bar->frameRect();
        toFill.intersect(rect);
        context->fillRect(toFill, baseBackgroundColor(), ColorSpaceDeviceRGB);
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

    Element* htmlElement = frame().document()->documentElement();
    Element* bodyElement = frame().document()->body();

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
        if (widget->isFrameView()) {
            if (toFrameView(*widget).hasCustomScrollbars())
                return true;
        } else if (widget->isScrollbar()) {
            if (toScrollbar(*widget).isCustomScrollbar())
                return true;
        }
    }

    return false;
}

FrameView* FrameView::parentFrameView() const
{
    if (!parent())
        return 0;

    if (Frame* parentFrame = frame().tree().parent())
        return parentFrame->view();

    return 0;
}

bool FrameView::isInChildFrameWithFrameFlattening() const
{
    if (!parent() || !frame().ownerElement())
        return false;

    // Frame flattening applies when the owner element is either in a frameset or
    // an iframe with flattening parameters.
    if (frame().ownerElement()->hasTagName(iframeTag)) {
        RenderIFrame* iframeRenderer = toRenderIFrame(frame().ownerElement()->renderWidget());
        if (iframeRenderer->flattenFrame())
            return true;
    }

    if (!frameFlatteningEnabled())
        return false;

    if (frame().ownerElement()->hasTagName(frameTag))
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

    parentView->layout(allowSubtree);

    RenderElement* root = m_layoutRoot ? m_layoutRoot : frame().document()->renderView();
    ASSERT_UNUSED(root, !root->needsLayout());
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

    RenderView* renderView = this->renderView();
    if ((renderView && renderView->theme().supportsControlTints()) || hasCustomScrollbars())
        paintControlTints();
}

void FrameView::paintControlTints()
{
    if (needsLayout())
        layout();
    PlatformGraphicsContext* const noContext = 0;
    GraphicsContext context(noContext);
    context.setUpdatingControlTints(true);
    if (platformWidget()) {
        // FIXME: consult paintsEntireContents().
        paintContents(&context, visibleContentRect(LegacyIOSDocumentVisibleRect));
    } else
        paint(&context, frameRect());
}

bool FrameView::wasScrolledByUser() const
{
    return m_wasScrolledByUser;
}

void FrameView::setWasScrolledByUser(bool wasScrolledByUser)
{
    if (m_inProgrammaticScroll)
        return;
    m_maintainScrollPositionAnchor = 0;
    if (m_wasScrolledByUser == wasScrolledByUser)
        return;
    m_wasScrolledByUser = wasScrolledByUser;
    if (frame().isMainFrame())
        updateLayerFlushThrottling();
    adjustTiledBackingCoverage();
}

void FrameView::willPaintContents(GraphicsContext* context, const IntRect& dirtyRect, PaintingState& paintingState)
{
    Document* document = frame().document();

    if (!context->paintingDisabled())
        InspectorInstrumentation::willPaint(renderView());

    paintingState.isTopLevelPainter = !sCurrentPaintTimeStamp;

    if (paintingState.isTopLevelPainter && memoryPressureHandler().isUnderMemoryPressure()) {
        LOG(MemoryPressure, "Under memory pressure: %s", WTF_PRETTY_FUNCTION);

        // To avoid unnecessary image decoding, we don't prune recently-decoded live resources here since
        // we might need some live bitmaps on painting.
        memoryCache()->prune();
    }

    if (paintingState.isTopLevelPainter)
        sCurrentPaintTimeStamp = monotonicallyIncreasingTime();

    paintingState.paintBehavior = m_paintBehavior;
    
    if (FrameView* parentView = parentFrameView()) {
        if (parentView->paintBehavior() & PaintBehaviorFlattenCompositingLayers)
            m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;
    }
    
    if (m_paintBehavior == PaintBehaviorNormal)
        document->markers().invalidateRenderedRectsForMarkersInRect(dirtyRect);

    if (document->printing())
        m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;

    paintingState.isFlatteningPaintOfRootFrame = (m_paintBehavior & PaintBehaviorFlattenCompositingLayers) && !frame().ownerElement();
    if (paintingState.isFlatteningPaintOfRootFrame)
        notifyWidgetsInAllFrames(WillPaintFlattened);

    ASSERT(!m_isPainting);
    m_isPainting = true;
}

void FrameView::didPaintContents(GraphicsContext* context, const IntRect& dirtyRect, PaintingState& paintingState)
{
    m_isPainting = false;

    if (paintingState.isFlatteningPaintOfRootFrame)
        notifyWidgetsInAllFrames(DidPaintFlattened);

    m_paintBehavior = paintingState.paintBehavior;
    m_lastPaintTime = monotonicallyIncreasingTime();

    // Painting can lead to decoding of large amounts of bitmaps
    // If we are low on memory, wipe them out after the paint.
    if (paintingState.isTopLevelPainter && memoryPressureHandler().isUnderMemoryPressure())
        memoryCache()->pruneLiveResources(true);

    // Regions may have changed as a result of the visibility/z-index of element changing.
#if ENABLE(DASHBOARD_SUPPORT)
    if (frame().document()->annotatedRegionsDirty())
        updateAnnotatedRegions();
#endif

    if (paintingState.isTopLevelPainter)
        sCurrentPaintTimeStamp = 0;

    if (!context->paintingDisabled()) {
        InspectorInstrumentation::didPaint(renderView(), context, dirtyRect);
        // FIXME: should probably not fire milestones for snapshot painting. https://bugs.webkit.org/show_bug.cgi?id=117623
        firePaintRelatedMilestonesIfNeeded();
    }
}

void FrameView::paintContents(GraphicsContext* context, const IntRect& dirtyRect)
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
        context->fillRect(dirtyRect, Color(0xFF, 0, 0), ColorSpaceDeviceRGB);
#endif

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

    FontCachePurgePreventer fontCachePurgePreventer;

    // m_nodeToDraw is used to draw only one element (and its descendants)
    RenderObject* eltRenderer = m_nodeToDraw ? m_nodeToDraw->renderer() : 0;
    RenderLayer* rootLayer = renderView->layer();

#ifndef NDEBUG
    RenderElement::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(&rootLayer->renderer());
#endif

    // To work around http://webkit.org/b/135106, ensure that the paint root isn't an inline with culled line boxes.
    // FIXME: This can cause additional content to be included in the snapshot, so remove this once that bug is fixed.
    while (eltRenderer && eltRenderer->isRenderInline() && !toRenderInline(eltRenderer)->firstLineBox())
        eltRenderer = eltRenderer->parent();

    rootLayer->paint(context, dirtyRect, m_paintBehavior, eltRenderer);
    if (rootLayer->containsDirtyOverlayScrollbars())
        rootLayer->paintOverlayScrollbars(context, dirtyRect, m_paintBehavior, eltRenderer);

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

void FrameView::paintContentsForSnapshot(GraphicsContext* context, const IntRect& imageRect, SelectionInSnapshot shouldPaintSelection, CoordinateSpaceForSnapshot coordinateSpace)
{
    updateLayoutAndStyleIfNeededRecursive();

    // Cache paint behavior and set a new behavior appropriate for snapshots.
    PaintBehavior oldBehavior = paintBehavior();
    setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);

    // If the snapshot should exclude selection, then we'll clear the current selection
    // in the render tree only. This will allow us to restore the selection from the DOM
    // after we paint the snapshot.
    if (shouldPaintSelection == ExcludeSelection) {
        for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
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
        for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get()))
            frame->selection().updateAppearance();
    }

    // Restore cached paint behavior.
    setPaintBehavior(oldBehavior);
}

void FrameView::paintOverhangAreas(GraphicsContext* context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    if (context->paintingDisabled())
        return;

    if (frame().document()->printing())
        return;

    ScrollView::paintOverhangAreas(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect);
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

    frame().document()->updateStyleIfNeeded();

    if (needsLayout())
        layout();

    // Grab a copy of the children() set, as it may be mutated by the following updateLayoutAndStyleIfNeededRecursive
    // calls, as they can potentially re-enter a layout of the parent frame view, which may add/remove scrollbars
    // and thus mutates the children() set.
    Vector<Ref<FrameView>, 16> childViews;
    childViews.reserveInitialCapacity(children().size());
    for (auto& widget : children()) {
        if (widget->isFrameView())
            childViews.uncheckedAppend(toFrameView(*widget));
    }

    for (unsigned i = 0; i < childViews.size(); ++i)
        childViews[i]->updateLayoutAndStyleIfNeededRecursive();

    // When frame flattening is on, child frame can mark parent frame dirty. In such case, child frame
    // needs to call layout on parent frame recursively.
    // This assert ensures that parent frames are clean, when child frames finished updating layout and style.
    ASSERT(!needsLayout());
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
    static const int documentHeightThreshold = 200;
    if (documentElement->renderBox()->layoutOverflowRect().pixelSnappedHeight() < documentHeightThreshold)
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
    GraphicsContext context((PlatformGraphicsContext*)0);
    renderView->setTruncatedAt(static_cast<int>(floorf(oldBottom)));
    IntRect dirtyRect(0, static_cast<int>(floorf(oldTop)), renderView->layoutOverflowRect().maxX(), static_cast<int>(ceilf(oldBottom - oldTop)));
    renderView->setPrintRect(dirtyRect);
    renderView->layer()->paint(&context, dirtyRect);
    *newBottom = renderView->bestTruncatedAt();
    if (!*newBottom)
        *newBottom = oldBottom;
    renderView->setPrintRect(IntRect());
}

IntRect FrameView::convertFromRendererToContainingView(const RenderElement* renderer, const IntRect& rendererRect) const
{
    IntRect rect = pixelSnappedIntRect(enclosingLayoutRect(renderer->localToAbsoluteQuad(FloatRect(rendererRect)).boundingBox()));

    // Convert from page ("absolute") to FrameView coordinates.
    if (!delegatesScrolling())
        rect.moveBy(-scrollPosition() + IntPoint(0, headerHeight() + topContentInset(TopContentInsetType::WebCoreOrPlatformContentInset)));

    return rect;
}

IntRect FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const IntRect& viewRect) const
{
    IntRect rect = viewRect;
    
    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrolling())
        rect.moveBy(documentScrollPositionRelativeToViewOrigin());

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
        point.moveBy(-scrollPosition() + IntPoint(0, headerHeight() + topContentInset(TopContentInsetType::WebCoreOrPlatformContentInset)));
    return point;
}

IntPoint FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const IntPoint& viewPoint) const
{
    IntPoint point = viewPoint;

    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrolling())
        point = point + documentScrollPositionRelativeToViewOrigin();

    return roundedIntPoint(renderer->absoluteToLocal(point, UseTransforms));
}

IntRect FrameView::convertToContainingView(const IntRect& localRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);
            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return localRect;
                
            IntRect rect(localRect);
            // Add borders and padding??
            rect.move(renderer->borderLeft() + renderer->paddingLeft(),
                      renderer->borderTop() + renderer->paddingTop());
            return parentView->convertFromRendererToContainingView(renderer, rect);
        }
        
        return Widget::convertToContainingView(localRect);
    }
    
    return localRect;
}

IntRect FrameView::convertFromContainingView(const IntRect& parentRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return parentRect;

            IntRect rect = parentView->convertFromContainingViewToRenderer(renderer, parentRect);
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
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return localPoint;
                
            IntPoint point(localPoint);

            // Add borders and padding
            point.move(renderer->borderLeft() + renderer->paddingLeft(),
                       renderer->borderTop() + renderer->paddingTop());
            return parentView->convertFromRendererToContainingView(renderer, point);
        }
        
        return Widget::convertToContainingView(localPoint);
    }
    
    return localPoint;
}

IntPoint FrameView::convertFromContainingView(const IntPoint& parentPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return parentPoint;

            IntPoint point = parentView->convertFromContainingViewToRenderer(renderer, parentPoint);
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
        for (size_t i = 0; i < m_trackedRepaintRects.size(); ++i)
            ts << "  (rect " << LayoutUnit(m_trackedRepaintRects[i].x()) << " " << LayoutUnit(m_trackedRepaintRects[i].y()) << " " << LayoutUnit(m_trackedRepaintRects[i].width()) << " " << LayoutUnit(m_trackedRepaintRects[i].height()) << ")\n";
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
            scrollingCoordinator->frameViewNonFastScrollableRegionChanged(this);
    }
}

void FrameView::sendScrollEvent()
{
    frame().eventHandler().sendScrollEvent();
    frame().eventHandler().dispatchFakeMouseMoveEventSoon();
}

void FrameView::removeChild(Widget* widget)
{
    if (widget->isFrameView())
        removeScrollableArea(toFrameView(widget));

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
        IntSize offset = scrollOffset();
        IntPoint oldPosition = scrollPosition();
        IntSize newOffset = IntSize(offset.width() - wheelEvent.deltaX(), offset.height() - wheelEvent.deltaY());
        if (offset != newOffset) {
            ScrollView::scrollTo(newOffset);
            scrollPositionChanged(oldPosition, scrollPosition());
            frame().loader().client().didChangeScrollOffset();
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
            if (scrollingCoordinator->coordinatesScrollingForFrameView(this))
                return scrollingCoordinator->handleWheelEvent(this, wheelEvent);
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
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
        if (FrameView* view = frame->view())
            view->notifyWidgets(notification);
    }
}
    
AXObjectCache* FrameView::axObjectCache() const
{
    if (frame().document())
        return frame().document()->existingAXObjectCache();
    return 0;
}
    
#if PLATFORM(IOS)
void FrameView::setCustomFixedPositionLayoutRect(const IntRect& rect)
{
    if (m_useCustomFixedPositionLayoutRect && m_customFixedPositionLayoutRect == rect)
        return;
    m_useCustomFixedPositionLayoutRect = true;
    m_customFixedPositionLayoutRect = rect;
    visibleContentsResized();
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
    m_horizontalVelocity = horizontalVelocity;
    m_verticalVelocity = verticalVelocity;
    m_scaleChangeRate = scaleChangeRate;
    m_lastVelocityUpdateTime = timestamp;
}

FloatRect FrameView::computeCoverageRect(double horizontalMargin, double verticalMargin) const
{
    FloatRect exposedContentRect = this->exposedContentRect();
    if (!m_speculativeTilingEnabled || memoryPressureHandler().isUnderMemoryPressure())
        return exposedContentRect;

    double currentTime = monotonicallyIncreasingTime();
    double timeDelta = currentTime - m_lastVelocityUpdateTime;

    FloatRect futureRect = exposedContentRect;
    futureRect.setLocation(FloatPoint(futureRect.location().x() + timeDelta * m_horizontalVelocity, futureRect.location().y() + timeDelta * m_verticalVelocity));

    if (m_horizontalVelocity) {
        futureRect.setWidth(futureRect.width() + horizontalMargin);
        if (m_horizontalVelocity < 0)
            futureRect.setX(futureRect.x() - horizontalMargin);
    }

    if (m_verticalVelocity) {
        futureRect.setHeight(futureRect.height() + verticalMargin);
        if (m_verticalVelocity < 0)
            futureRect.setY(futureRect.y() - verticalMargin);
    }

    if (m_scaleChangeRate <= 0 && !m_horizontalVelocity && !m_verticalVelocity) {
        futureRect.setWidth(futureRect.width() + horizontalMargin);
        futureRect.setHeight(futureRect.height() + verticalMargin);
        futureRect.setX(futureRect.x() - horizontalMargin / 2);
        futureRect.setY(futureRect.y() - verticalMargin / 2);
    }

    IntSize contentSize = contentsSize();
    if (futureRect.maxX() > contentSize.width())
        futureRect.setX(contentSize.width() - futureRect.width());
    if (futureRect.maxY() > contentSize.height())
        futureRect.setY(contentSize.height() - futureRect.height());
    if (futureRect.x() < 0)
        futureRect.setX(0);
    if (futureRect.y() < 0)
        futureRect.setY(0);

    return futureRect;
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
        frame().loader().didLayout(milestonesAchieved);
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
        page->mainFrame().loader().didLayout(milestonesAchieved);
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
    
    if (ScrollingCoordinator* scrollingCoordinator = frame().page()->scrollingCoordinator())
        scrollingCoordinator->setScrollPinningBehavior(pinning);
    
    updateScrollbars(scrollOffset());
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
    auto protectedWidgets = collectAndProtectWidgets(m_widgetsInRenderTree);

    for (unsigned i = 0, size = protectedWidgets.size(); i < size; ++i) {
        if (RenderWidget* renderWidget = RenderWidget::find(protectedWidgets[i].get()))
            renderWidget->updateWidgetPosition();
    }
}

void FrameView::notifyWidgets(WidgetNotification notification)
{
    auto protectedWidgets = collectAndProtectWidgets(m_widgetsInRenderTree);

    for (unsigned i = 0, size = protectedWidgets.size(); i < size; ++i)
        protectedWidgets[i]->notifyWidget(notification);
}

void FrameView::setExposedRect(FloatRect exposedRect)
{
    if (m_exposedRect == exposedRect)
        return;
    m_exposedRect = exposedRect;

    // FIXME: We should support clipping to the exposed rect for subframes as well.
    if (!m_frame->isMainFrame())
        return;
    if (TiledBacking* tiledBacking = this->tiledBacking()) {
        adjustTiledBackingCoverage();
        tiledBacking->setTiledScrollingIndicatorPosition(exposedRect.location());
    }

    if (auto* view = renderView())
        view->compositor().scheduleLayerFlush(false /* canThrottle */);
}
    
void FrameView::setViewportSizeForCSSViewportUnits(IntSize size)
{
    if (m_hasOverrideViewportSize && m_overrideViewportSize == size)
        return;
    
    m_overrideViewportSize = size;
    m_hasOverrideViewportSize = true;
    
    if (Document* document = m_frame->document())
        document->styleResolverChanged(DeferRecalcStyle);
}
    
IntSize FrameView::viewportSizeForCSSViewportUnits() const
{
    if (m_hasOverrideViewportSize)
        return m_overrideViewportSize;
    
    return visibleContentRectIncludingScrollbars().size();
}
    
} // namespace WebCore
