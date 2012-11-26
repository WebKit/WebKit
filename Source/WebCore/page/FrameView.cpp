/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "BackForwardController.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentMarkerController.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FontCache.h"
#include "Frame.h"
#include "FrameActionScheduler.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInImageElement.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "OverflowEvent.h"
#include "RenderArena.h"
#include "RenderEmbeddedObject.h"
#include "RenderFullScreen.h"
#include "RenderIFrame.h"
#include "RenderLayer.h"
#include "RenderPart.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "TextResourceDecoder.h"

#include <wtf/CurrentTime.h>
#include <wtf/TemporaryChange.h>
#include <wtf/UnusedParam.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#include "TiledBacking.h"
#endif

#if ENABLE(SVG)
#include "RenderSVGRoot.h"
#include "SVGDocument.h"
#include "SVGSVGElement.h"
#endif

#if USE(TILED_BACKING_STORE)
#include "TiledBackingStore.h"
#endif

namespace WebCore {

using namespace HTMLNames;

double FrameView::sCurrentPaintTimeStamp = 0.0;

// REPAINT_THROTTLING now chooses default values for throttling parameters.
// Should be removed when applications start using runtime configuration.
#if ENABLE(REPAINT_THROTTLING)
// Normal delay
double FrameView::s_deferredRepaintDelay = 0.025;
// Negative value would mean that first few repaints happen without a delay
double FrameView::s_initialDeferredRepaintDelayDuringLoading = 0;
// The delay grows on each repaint to this maximum value
double FrameView::s_maxDeferredRepaintDelayDuringLoading = 2.5;
// On each repaint the delay increses by this amount
double FrameView::s_deferredRepaintDelayIncrementDuringLoading = 0.5;
#else
// FIXME: Repaint throttling could be good to have on all platform.
// The balance between CPU use and repaint frequency will need some tuning for desktop.
// More hooks may be needed to reset the delay on things like GIF and CSS animations.
double FrameView::s_deferredRepaintDelay = 0;
double FrameView::s_initialDeferredRepaintDelayDuringLoading = 0;
double FrameView::s_maxDeferredRepaintDelayDuringLoading = 0;
double FrameView::s_deferredRepaintDelayIncrementDuringLoading = 0;
#endif

// The maximum number of updateWidgets iterations that should be done before returning.
static const unsigned maxUpdateWidgetsIterations = 2;

static inline RenderView* rootRenderer(const FrameView* view)
{
    return view->frame() ? view->frame()->contentRenderer() : 0;
}

FrameView::FrameView(Frame* frame)
    : m_frame(frame)
    , m_canHaveScrollbars(true)
    , m_slowRepaintObjectCount(0)
    , m_fixedObjectCount(0)
    , m_layoutTimer(this, &FrameView::layoutTimerFired)
    , m_layoutRoot(0)
    , m_inSynchronousPostLayout(false)
    , m_postLayoutTasksTimer(this, &FrameView::postLayoutTimerFired)
    , m_isTransparent(false)
    , m_baseBackgroundColor(Color::white)
    , m_mediaType("screen")
    , m_actionScheduler(adoptPtr(new FrameActionScheduler))
    , m_overflowStatusDirty(true)
    , m_viewportRenderer(0)
    , m_wasScrolledByUser(false)
    , m_inProgrammaticScroll(false)
    , m_safeToPropagateScrollToParent(true)
    , m_deferredRepaintTimer(this, &FrameView::deferredRepaintTimerFired)
    , m_disableRepaints(0)
    , m_isTrackingRepaints(false)
    , m_shouldUpdateWhileOffscreen(true)
    , m_deferSetNeedsLayouts(0)
    , m_setNeedsLayoutWasDeferred(false)
    , m_scrollCorner(0)
    , m_shouldAutoSize(false)
    , m_inAutoSize(false)
    , m_didRunAutosize(false)
{
    init();

    // FIXME: Can m_frame ever be null here?
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame == page->mainFrame()) {
        ScrollableArea::setVerticalScrollElasticity(ScrollElasticityAllowed);
        ScrollableArea::setHorizontalScrollElasticity(ScrollElasticityAllowed);
    }
}

PassRefPtr<FrameView> FrameView::create(Frame* frame)
{
    RefPtr<FrameView> view = adoptRef(new FrameView(frame));
    view->show();
    return view.release();
}

PassRefPtr<FrameView> FrameView::create(Frame* frame, const IntSize& initialSize)
{
    RefPtr<FrameView> view = adoptRef(new FrameView(frame));
    view->Widget::setFrameRect(IntRect(view->location(), initialSize));
    view->show();
    return view.release();
}

FrameView::~FrameView()
{
    if (m_postLayoutTasksTimer.isActive()) {
        m_postLayoutTasksTimer.stop();
        m_actionScheduler->clear();
    }
    
    if (AXObjectCache::accessibilityEnabled() && axObjectCache())
        axObjectCache()->remove(this);
    
    resetScrollbars();

    // Custom scrollbars should already be destroyed at this point
    ASSERT(!horizontalScrollbar() || !horizontalScrollbar()->isCustomScrollbar());
    ASSERT(!verticalScrollbar() || !verticalScrollbar()->isCustomScrollbar());

    setHasHorizontalScrollbar(false); // Remove native scrollbars now before we lose the connection to the HostWindow.
    setHasVerticalScrollbar(false);
    
    ASSERT(!m_scrollCorner);
    ASSERT(m_actionScheduler->isEmpty());

    if (m_frame) {
        ASSERT(m_frame->view() != this || !m_frame->contentRenderer());
        RenderPart* renderer = m_frame->ownerRenderer();
        if (renderer && renderer->widget() == this)
            renderer->setWidget(0);
    }
}

void FrameView::reset()
{
    m_cannotBlitToWindow = false;
    m_isOverlapped = false;
    m_contentIsOpaque = false;
    m_borderX = 30;
    m_borderY = 30;
    m_layoutTimer.stop();
    m_layoutRoot = 0;
    m_delayedLayout = false;
    m_doFullRepaint = true;
    m_layoutSchedulingEnabled = true;
    m_inLayout = false;
    m_inSynchronousPostLayout = false;
    m_layoutCount = 0;
    m_nestedLayoutCount = 0;
    m_postLayoutTasksTimer.stop();
    m_firstLayout = true;
    m_firstLayoutCallbackPending = false;
    m_wasScrolledByUser = false;
    m_safeToPropagateScrollToParent = true;
    m_lastViewportSize = IntSize();
    m_lastZoomFactor = 1.0f;
    m_deferringRepaints = 0;
    m_repaintCount = 0;
    m_repaintRects.clear();
    m_deferredRepaintDelay = s_initialDeferredRepaintDelayDuringLoading;
    m_deferredRepaintTimer.stop();
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
    m_disableRepaints = 0;
}

bool FrameView::isFrameView() const 
{ 
    return true; 
}

void FrameView::clearFrame()
{
    m_frame = 0;
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
    Element* ownerElement = m_frame ? m_frame->ownerElement() : 0;
    if (ownerElement && (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))) {
        HTMLFrameElement* frameElt = static_cast<HTMLFrameElement*>(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            setCanHaveScrollbars(false);
        LayoutUnit marginWidth = frameElt->marginWidth();
        LayoutUnit marginHeight = frameElt->marginHeight();
        if (marginWidth != -1)
            setMarginWidth(marginWidth);
        if (marginHeight != -1)
            setMarginHeight(marginHeight);
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

    if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
    }
}

void FrameView::recalculateScrollbarOverlayStyle()
{
    ScrollbarOverlayStyle oldOverlayStyle = scrollbarOverlayStyle();
    ScrollbarOverlayStyle overlayStyle = ScrollbarOverlayStyleDefault;

    Color backgroundColor = documentBackgroundColor();
#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* root = rootRenderer(this)) {
        RenderLayerCompositor* compositor = root->compositor();
        compositor->documentBackgroundColorDidChange();
    }
#endif

    if (backgroundColor.isValid()) {
        // Reduce the background color from RGB to a lightness value
        // and determine which scrollbar style to use based on a lightness
        // heuristic.
        double hue, saturation, lightness;
        backgroundColor.getHSL(hue, saturation, lightness);
        if (lightness <= .5)
            overlayStyle = ScrollbarOverlayStyleLight;
    }

    if (oldOverlayStyle != overlayStyle)
        setScrollbarOverlayStyle(overlayStyle);
}

void FrameView::clear()
{
    setCanBlitOnScroll(true);
    
    reset();

    if (m_frame) {
        if (RenderPart* renderer = m_frame->ownerRenderer())
            renderer->viewCleared();
    }

    setScrollbarsSuppressed(true);
}

bool FrameView::didFirstLayout() const
{
    return !m_firstLayout;
}

void FrameView::invalidateRect(const IntRect& rect)
{
    if (!parent()) {
        if (hostWindow())
            hostWindow()->invalidateContentsAndRootView(rect, false /*immediate*/);
        return;
    }

    if (!m_frame)
        return;

    RenderPart* renderer = m_frame->ownerRenderer();
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

#if ENABLE(VIEWPORT)
    if (useFixedLayout()) {
        Document* document = m_frame->document();
        ViewportArguments viewport = document->viewportArguments();
        Page* page = frame() ? frame()->page() : 0;
        if (page)
            page->chrome()->client()->dispatchViewportPropertiesDidChange(viewport);
    }
#endif

    ScrollView::setFrameRect(newRect);

    updateScrollableAreaSet();

#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* root = rootRenderer(this)) {
        if (root->usesCompositing())
            root->compositor()->frameViewDidChangeSize();
    }
#endif
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
bool FrameView::scheduleAnimation()
{
    if (hostWindow()) {
        hostWindow()->scheduleAnimation();
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

bool FrameView::avoidScrollbarCreation() const
{
    ASSERT(m_frame);

    // with frame flattening no subframe can have scrollbars
    // but we also cannot turn scrollbars off as we determine
    // our flattening policy using that.

    if (!m_frame->ownerElement())
        return false;

    if (!m_frame->settings() || m_frame->settings()->frameFlatteningEnabled())
        return true;

    return false;
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
    // FIXME: We need to update the scrollbar dynamically as documents change (or as doc elements and bodies get discovered that have custom styles).
    Document* doc = m_frame->document();

    // Try the <body> element first as a scrollbar source.
    Element* body = doc ? doc->body() : 0;
    if (body && body->renderer() && body->renderer()->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, body);
    
    // If the <body> didn't have a custom style, then the root element might.
    Element* docElement = doc ? doc->documentElement() : 0;
    if (docElement && docElement->renderer() && docElement->renderer()->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, docElement);
        
    // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
    RenderPart* frameRenderer = m_frame->ownerRenderer();
    if (frameRenderer && frameRenderer->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, 0, m_frame.get());
    
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
    
    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return;

    updateScrollableAreaSet();

    page->chrome()->contentsSizeChanged(frame(), size); //notify only

    m_deferSetNeedsLayouts--;
    
    if (!m_deferSetNeedsLayouts)
        m_setNeedsLayoutWasDeferred = false; // FIXME: Find a way to make the deferred layout actually happen.
}

void FrameView::adjustViewSize()
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return;

    ASSERT(m_frame->view() == this);

    const IntRect rect = root->documentRect();
    const IntSize& size = rect.size();
    ScrollView::setScrollOrigin(IntPoint(-rect.x(), -rect.y()), !m_frame->document()->printing(), size == contentsSize());
    
    setContentsSize(size);
}

void FrameView::applyOverflowToViewport(RenderObject* o, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.

    // To combat the inability to scroll on a page with overflow:hidden on the root when scaled, disregard hidden when
    // there is a frameScaleFactor that is greater than one on the main frame.

    bool overrideHidden = m_frame->page() && m_frame->page()->mainFrame() == m_frame && m_frame->frameScaleFactor() > 1;

    EOverflow overflowX = o->style()->overflowX();
    EOverflow overflowY = o->style()->overflowY();

#if ENABLE(SVG)
    if (o->isSVGRoot()) {
        // overflow is ignored in stand-alone SVG documents.
        if (!toRenderSVGRoot(o)->isEmbeddedThroughFrameContainingSVGDocument())
            return;
        overflowX = OHIDDEN;
        overflowY = OHIDDEN;
    }
#endif

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
            // Don't set it at all.
            ;
    }

    m_viewportRenderer = o;
}

void FrameView::calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy strategy)
{
    m_viewportRenderer = 0;

    const HTMLFrameOwnerElement* owner = m_frame->ownerElement();
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
        Document* document = m_frame->document();
        Node* documentElement = document->documentElement();
        RenderObject* rootRenderer = documentElement ? documentElement->renderer() : 0;
        Node* body = document->body();
        if (body && body->renderer()) {
            if (body->hasTagName(framesetTag) && m_frame->settings() && !m_frame->settings()->frameFlatteningEnabled()) {
                vMode = ScrollbarAlwaysOff;
                hMode = ScrollbarAlwaysOff;
            } else if (body->hasTagName(bodyTag)) {
                // It's sufficient to just check the X overflow,
                // since it's illegal to have visible in only one direction.
                RenderObject* o = rootRenderer->style()->overflowX() == OVISIBLE && document->documentElement()->hasTagName(htmlTag) ? body->renderer() : rootRenderer;
                applyOverflowToViewport(o, hMode, vMode);
            }
        } else if (rootRenderer)
            applyOverflowToViewport(rootRenderer, hMode, vMode);
    }    
}

#if USE(ACCELERATED_COMPOSITING)
void FrameView::updateCompositingLayersAfterStyleChange()
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return;

    // This call will make sure the cached hasAcceleratedCompositing is updated from the pref
    root->compositor()->cacheAcceleratedCompositingFlags();
    root->compositor()->updateCompositingLayers(CompositingUpdateAfterStyleChange);
}

void FrameView::updateCompositingLayersAfterLayout()
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return;

    // This call will make sure the cached hasAcceleratedCompositing is updated from the pref
    root->compositor()->cacheAcceleratedCompositingFlags();
    root->compositor()->updateCompositingLayers(CompositingUpdateAfterLayout);
}

void FrameView::clearBackingStores()
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return;

    RenderLayerCompositor* compositor = root->compositor();
    ASSERT(compositor->inCompositingMode());
    compositor->enableCompositingMode(false);
    compositor->clearBackingForAllLayers();
}

void FrameView::restoreBackingStores()
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return;

    RenderLayerCompositor* compositor = root->compositor();
    compositor->enableCompositingMode(true);
    compositor->updateCompositingLayers(CompositingUpdateAfterLayout);
}

GraphicsLayer* FrameView::layerForHorizontalScrollbar() const
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return 0;
    return root->compositor()->layerForHorizontalScrollbar();
}

GraphicsLayer* FrameView::layerForVerticalScrollbar() const
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return 0;
    return root->compositor()->layerForVerticalScrollbar();
}

GraphicsLayer* FrameView::layerForScrollCorner() const
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return 0;
    return root->compositor()->layerForScrollCorner();
}

TiledBacking* FrameView::tiledBacking()
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return 0;

    RenderLayerBacking* backing = root->layer()->backing();
    if (!backing)
        return 0;

    return backing->graphicsLayer()->tiledBacking();
}

#if ENABLE(RUBBER_BANDING)
GraphicsLayer* FrameView::layerForOverhangAreas() const
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return 0;
    return root->compositor()->layerForOverhangAreas();
}
#endif

bool FrameView::syncCompositingStateForThisFrame(Frame* rootFrameForSync)
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return true; // We don't want to keep trying to update layers if we have no renderer.

    ASSERT(m_frame->view() == this);

    // If we sync compositing layers when a layout is pending, we may cause painting of compositing
    // layer content to occur before layout has happened, which will cause paintContents() to bail.
    if (needsLayout())
        return false;

    root->compositor()->flushPendingLayerChanges(rootFrameForSync == m_frame);

    return true;
}

void FrameView::setNeedsOneShotDrawingSynchronization()
{
    Page* page = frame() ? frame()->page() : 0;
    if (page)
        page->chrome()->client()->setNeedsOneShotDrawingSynchronization();
}

#endif // USE(ACCELERATED_COMPOSITING)

bool FrameView::hasCompositedContent() const
{
#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* root = rootRenderer(this))
        return root->compositor()->inCompositingMode();
#endif
    return false;
}

bool FrameView::hasCompositedContentIncludingDescendants() const
{
#if USE(ACCELERATED_COMPOSITING)
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
        RenderView* renderView = frame->contentRenderer();
        RenderLayerCompositor* compositor = renderView ? renderView->compositor() : 0;
        if (compositor) {
            if (compositor->inCompositingMode())
                return true;

            if (!RenderLayerCompositor::allowsIndependentlyCompositedFrames(this))
                break;
        }
    }
#endif
    return false;
}

bool FrameView::hasCompositingAncestor() const
{
#if USE(ACCELERATED_COMPOSITING)
    for (Frame* frame = m_frame->tree()->parent(); frame; frame = frame->tree()->parent()) {
        if (FrameView* view = frame->view()) {
            if (view->hasCompositedContent())
                return true;
        }
    }
#endif
    return false;
}

// Sometimes (for plug-ins) we need to eagerly go into compositing mode.
void FrameView::enterCompositingMode()
{
#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* root = rootRenderer(this)) {
        root->compositor()->enableCompositingMode();
        if (!needsLayout())
            root->compositor()->scheduleCompositingLayerUpdate();
    }
#endif
}

bool FrameView::isEnclosedInCompositingLayer() const
{
#if USE(ACCELERATED_COMPOSITING)
    RenderObject* frameOwnerRenderer = m_frame->ownerRenderer();
    if (frameOwnerRenderer && frameOwnerRenderer->containerForRepaint())
        return true;

    if (FrameView* parentView = parentFrameView())
        return parentView->isEnclosedInCompositingLayer();
#endif
    return false;
}
    
bool FrameView::syncCompositingStateIncludingSubframes()
{
#if USE(ACCELERATED_COMPOSITING)
    bool allFramesSynced = syncCompositingStateForThisFrame(m_frame.get());
    
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->traverseNext(m_frame.get())) {
        bool synced = child->view()->syncCompositingStateForThisFrame(m_frame.get());
        allFramesSynced &= synced;
    }
    return allFramesSynced;
#else // USE(ACCELERATED_COMPOSITING)
    return true;
#endif
}

bool FrameView::isSoftwareRenderable() const
{
#if USE(ACCELERATED_COMPOSITING)
    RenderView* root = rootRenderer(this);
    if (!root)
        return true;

    return !root->compositor()->has3DContent();
#else
    return true;
#endif
}

void FrameView::didMoveOnscreen()
{
#if USE(ACCELERATED_COMPOSITING)
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setIsInWindow(true);
#endif

    if (RenderView* root = rootRenderer(this))
        root->didMoveOnscreen();
    contentAreaDidShow();
}

void FrameView::willMoveOffscreen()
{
#if USE(ACCELERATED_COMPOSITING)
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setIsInWindow(false);
#endif

    if (RenderView* root = rootRenderer(this))
        root->willMoveOffscreen();
    contentAreaDidHide();
}

RenderObject* FrameView::layoutRoot(bool onlyDuringLayout) const
{
    return onlyDuringLayout && layoutPending() ? 0 : m_layoutRoot;
}

static inline void collectFrameViewChildren(FrameView* frameView, Vector<RefPtr<FrameView> >& frameViews)
{
    const HashSet<RefPtr<Widget> >* viewChildren = frameView->children();
    ASSERT(viewChildren);

    const HashSet<RefPtr<Widget> >::iterator end = viewChildren->end();
    for (HashSet<RefPtr<Widget> >::iterator current = viewChildren->begin(); current != end; ++current) {
        Widget* widget = (*current).get();
        if (widget->isFrameView())
            frameViews.append(static_cast<FrameView*>(widget));
    }
}

inline void FrameView::forceLayoutParentViewIfNeeded()
{
#if ENABLE(SVG)
    RenderPart* ownerRenderer = m_frame->ownerRenderer();
    if (!ownerRenderer || !ownerRenderer->frame())
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
    RefPtr<FrameView> frameView = ownerRenderer->frame()->view();

    // Mark the owner renderer as needing layout.
    ownerRenderer->setNeedsLayoutAndPrefWidthsRecalc();

    // Synchronously enter layout, to layout the view containing the host object/embed/iframe.
    ASSERT(frameView);
    frameView->layout();
#endif
}

void FrameView::layout(bool allowSubtree)
{
    if (m_inLayout)
        return;

    // Protect the view from being deleted during layout (in recalcStyle)
    RefPtr<FrameView> protector(this);

    bool inChildFrameLayoutWithFrameFlattening = isInChildFrameWithFrameFlattening();

    if (inChildFrameLayoutWithFrameFlattening) {
        if (doLayoutWithFrameFlattening(allowSubtree))
            return;
    }

    m_layoutTimer.stop();
    m_delayedLayout = false;
    m_setNeedsLayoutWasDeferred = false;

    if (!m_frame) {
        // FIXME: Do we need to set m_size.width here?
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(layoutWidth());
        return;
    }
    
    // we shouldn't enter layout() while painting
    ASSERT(!isPainting());
    if (isPainting())
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willLayout(m_frame.get());

    if (!allowSubtree && m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }

    ASSERT(m_frame->view() == this);

    Document* document = m_frame->document();
    bool subtree;
    RenderObject* root;

    {
        TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

        if (!m_nestedLayoutCount && !m_inSynchronousPostLayout && m_postLayoutTasksTimer.isActive() && !inChildFrameLayoutWithFrameFlattening) {
            // This is a new top-level layout. If there are any remaining tasks from the previous
            // layout, finish them now.
            m_inSynchronousPostLayout = true;
            performPostLayoutTasks();
            m_inSynchronousPostLayout = false;
        }

        // Viewport-dependent media queries may cause us to need completely different style information.
        // Check that here.
        if (document->styleResolver()->affectedByViewportChange()) {
            document->styleResolverChanged(RecalcStyleImmediately);
            InspectorInstrumentation::mediaQueryResultChanged(document);
        } else
            document->evaluateMediaQueryList();

        // Always ensure our style info is up-to-date. This can happen in situations where
        // the layout beats any sort of style recalc update that needs to occur.
        document->updateStyleIfNeeded();

        subtree = m_layoutRoot;

        // If there is only one ref to this view left, then its going to be destroyed as soon as we exit, 
        // so there's no point to continuing to layout
        if (protector->hasOneRef())
            return;

        root = subtree ? m_layoutRoot : document->renderer();
        if (!root) {
            // FIXME: Do we need to set m_size here?
            return;
        }
    } // Reset m_layoutSchedulingEnabled to its previous value.
    // The only reason the scoping was closed here is allow fontCachePurgePreventer
    // to outlive the change and reset of m_layoutSchedulingEnabled.

    FontCachePurgePreventer fontCachePurgePreventer;
    RenderLayer* layer;
    {
        TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

        m_nestedLayoutCount++;

        if (!m_layoutRoot) {
            Document* document = m_frame->document();
            Node* body = document->body();
            if (body && body->renderer()) {
                if (body->hasTagName(framesetTag) && m_frame->settings() && !m_frame->settings()->frameFlatteningEnabled()) {
                    body->renderer()->setChildNeedsLayout(true);
                } else if (body->hasTagName(bodyTag)) {
                    if (!m_firstLayout && m_size.height() != layoutHeight() && body->renderer()->enclosingBox()->stretchesToViewport())
                        body->renderer()->setChildNeedsLayout(true);
                }
            }

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (m_firstLayout && !m_frame->ownerElement())
                printf("Elapsed time before first layout: %d\n", document->elapsedTime());
#endif        
        }

        autoSizeIfEnabled();

        ScrollbarMode hMode;
        ScrollbarMode vMode;    
        calculateScrollbarModesForLayout(hMode, vMode);

        m_doFullRepaint = !subtree && (m_firstLayout || toRenderView(root)->printing());

        if (!subtree) {
            // Now set our scrollbar state for the layout.
            ScrollbarMode currentHMode = horizontalScrollbarMode();
            ScrollbarMode currentVMode = verticalScrollbarMode();

            if (m_firstLayout || (hMode != currentHMode || vMode != currentVMode)) {
                if (m_firstLayout) {
                    setScrollbarsSuppressed(true);

                    m_firstLayout = false;
                    m_firstLayoutCallbackPending = true;
                    if (useFixedLayout() && !fixedLayoutSize().isEmpty() && delegatesScrolling())
                        m_lastViewportSize = fixedLayoutSize();
                    else
                        m_lastViewportSize = visibleContentRect(true /*includeScrollbars*/).size();
                    m_lastZoomFactor = root->style()->zoom();

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

            m_size = LayoutSize(layoutWidth(), layoutHeight());

            if (oldSize != m_size) {
                m_doFullRepaint = true;
                if (!m_firstLayout) {
                    RenderBox* rootRenderer = document->documentElement() ? document->documentElement()->renderBox() : 0;
                    RenderBox* bodyRenderer = rootRenderer && document->body() ? document->body()->renderBox() : 0;
                    if (bodyRenderer && bodyRenderer->stretchesToViewport())
                        bodyRenderer->setChildNeedsLayout(true);
                    else if (rootRenderer && rootRenderer->stretchesToViewport())
                        rootRenderer->setChildNeedsLayout(true);
                }
            }
        }

        layer = root->enclosingLayer();

        m_actionScheduler->pause();

        {
            bool disableLayoutState = false;
            if (subtree) {
                RenderView* view = root->view();
                disableLayoutState = view->shouldDisableLayoutStateForSubtree(root);
                view->pushLayoutState(root);
            }
            LayoutStateDisabler layoutStateDisabler(disableLayoutState ? root->view() : 0);

            m_inLayout = true;
            beginDeferredRepaints();
            forceLayoutParentViewIfNeeded();
            root->layout();
            endDeferredRepaints();
            m_inLayout = false;

            if (subtree)
                root->view()->popLayoutState(root);
        }
        m_layoutRoot = 0;
    } // Reset m_layoutSchedulingEnabled to its previous value.

    if (!subtree && !toRenderView(root)->printing())
        adjustViewSize();

    // Now update the positions of all layers.
    beginDeferredRepaints();
    if (m_doFullRepaint)
        root->view()->repaint(); // FIXME: This isn't really right, since the RenderView doesn't fully encompass the visibleContentRect(). It just happens
                                 // to work out most of the time, since first layouts and printing don't have you scrolled anywhere.

    layer->updateLayerPositionsAfterLayout(rootRenderer(this)->layer(),
                                (m_doFullRepaint ? 0 : RenderLayer::CheckForRepaint)
                                | RenderLayer::IsCompositingUpdateRoot
                                | RenderLayer::UpdateCompositingLayers);

    endDeferredRepaints();

#if USE(ACCELERATED_COMPOSITING)
    updateCompositingLayersAfterLayout();
#endif
    
    m_layoutCount++;

#if PLATFORM(MAC) || PLATFORM(CHROMIUM)
    if (AXObjectCache::accessibilityEnabled())
        root->document()->axObjectCache()->postNotification(root, AXObjectCache::AXLayoutComplete, true);
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    updateDashboardRegions();
#endif

    ASSERT(!root->needsLayout());

    updateCanBlitOnScrollRecursively();

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(layoutWidth() < contentsWidth(),
                             layoutHeight() < contentsHeight());

    if (!m_postLayoutTasksTimer.isActive()) {
        if (!m_inSynchronousPostLayout) {
            if (inChildFrameLayoutWithFrameFlattening) {
                if (RenderView* root = rootRenderer(this))
                    root->updateWidgetPositions();
            } else {
                m_inSynchronousPostLayout = true;
                // Calls resumeScheduledEvents()
                performPostLayoutTasks();
                m_inSynchronousPostLayout = false;
            }
        }
        
        if (!m_postLayoutTasksTimer.isActive() && (needsLayout() || m_inSynchronousPostLayout || inChildFrameLayoutWithFrameFlattening)) {
            // If we need layout or are already in a synchronous call to postLayoutTasks(), 
            // defer widget updates and event dispatch until after we return. postLayoutTasks()
            // can make us need to update again, and we can get stuck in a nasty cycle unless
            // we call it through the timer here.
            m_postLayoutTasksTimer.startOneShot(0);
            if (needsLayout()) {
                m_actionScheduler->pause();
                layout();
            }
        }
    } else {
        m_actionScheduler->resume();
    }

    InspectorInstrumentation::didLayout(cookie);

    m_nestedLayoutCount--;
    if (m_nestedLayoutCount)
        return;

    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return;

    page->chrome()->client()->layoutUpdated(frame());
}

RenderBox* FrameView::embeddedContentBox() const
{
#if ENABLE(SVG)
    RenderView* root = rootRenderer(this);
    if (!root)
        return 0;

    RenderObject* rootChild = root->firstChild();
    if (!rootChild || !rootChild->isBox())
        return 0;

    // Curently only embedded SVG documents participate in the size-negotiation logic.
    if (rootChild->isSVGRoot())
        return toRenderBox(rootChild);
#endif

    return 0;
}

void FrameView::addWidgetToUpdate(RenderEmbeddedObject* object)
{
    if (!m_widgetUpdateSet)
        m_widgetUpdateSet = adoptPtr(new RenderEmbeddedObjectSet);

    // Tell the DOM element that it needs a widget update.
    Node* node = object->node();
    if (node->hasTagName(objectTag) || node->hasTagName(embedTag)) {
        HTMLPlugInImageElement* pluginElement = static_cast<HTMLPlugInImageElement*>(node);
        pluginElement->setNeedsWidgetUpdate(true);
    }

    m_widgetUpdateSet->add(object);
}

void FrameView::removeWidgetToUpdate(RenderEmbeddedObject* object)
{
    if (!m_widgetUpdateSet)
        return;

    m_widgetUpdateSet->remove(object);
}

void FrameView::setMediaType(const String& mediaType)
{
    m_mediaType = mediaType;
}

String FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType = m_frame->loader()->client()->overrideMediaType();
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
    bool mustBeSlow = m_slowRepaintObjectCount > 0 || (platformWidget() && m_fixedObjectCount > 0);

    if (contentsInCompositedLayer())
        return mustBeSlow;

#if PLATFORM(CHROMIUM)
    // The chromium compositor does not support scrolling a non-composited frame within a composited page through
    // the fast scrolling path, so force slow scrolling in that case.
    if (m_frame->ownerElement() && !hasCompositedContent() && m_frame->page() && m_frame->page()->mainFrame()->view()->hasCompositedContent())
        return true;
#endif

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
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
        if (FrameView* view = frame->view())
            view->setCanBlitOnScroll(!view->useSlowRepaints());
    }
}

bool FrameView::contentsInCompositedLayer() const
{
#if USE(ACCELERATED_COMPOSITING)
    RenderView* root = rootRenderer(this);
    if (root && root->isComposited()) {
        GraphicsLayer* layer = root->layer()->backing()->graphicsLayer();
        if (layer && layer->drawsContent())
            return true;
    }
#endif
    return false;
}

void FrameView::setCannotBlitToWindow()
{
    m_cannotBlitToWindow = true;
    updateCanBlitOnScrollRecursively();
}

void FrameView::addSlowRepaintObject()
{
    if (!m_slowRepaintObjectCount++) {
        updateCanBlitOnScrollRecursively();

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
        }
    }
}

void FrameView::removeSlowRepaintObject()
{
    ASSERT(m_slowRepaintObjectCount > 0);
    m_slowRepaintObjectCount--;
    if (!m_slowRepaintObjectCount) {
        updateCanBlitOnScrollRecursively();

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
        }
    }
}

void FrameView::addFixedObject()
{
    if (!m_fixedObjectCount++) {
        if (platformWidget())
            updateCanBlitOnScrollRecursively();

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasFixedObjectsDidChange(this);
        }
    }
}

void FrameView::removeFixedObject()
{
    ASSERT(m_fixedObjectCount > 0);
    --m_fixedObjectCount;

    if (!m_fixedObjectCount) {
        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasFixedObjectsDidChange(this);
        }

        // FIXME: In addFixedObject() we only call this if there's a platform widget,
        // why isn't the same check being made here?
        updateCanBlitOnScrollRecursively();
    }
}

int FrameView::scrollXForFixedPosition() const
{
    int visibleContentWidth = visibleContentRect().width();
    int maxX = contentsWidth() - visibleContentWidth;

    if (maxX == 0)
        return 0;

    int x = scrollX();

    if (!scrollOrigin().x()) {
        if (x < 0)
            x = 0;
        else if (x > maxX)
            x = maxX;
    } else {
        if (x > 0)
            x = 0;
        else if (x < -maxX)
            x = -maxX;
    }

    if (!m_frame)
        return x;

    float frameScaleFactor = m_frame->frameScaleFactor();

    // When the page is scaled, the scaled "viewport" with respect to which fixed object are positioned
    // doesn't move as fast as the content view, so that when the content is scrolled all the way to the
    // end, the bottom of the scaled "viewport" touches the bottom of the real viewport.
    float dragFactor = fixedElementsLayoutRelativeToFrame() ? 1 : (contentsWidth() - visibleContentWidth * frameScaleFactor) / maxX;

    return x * dragFactor / frameScaleFactor;
}

int FrameView::scrollYForFixedPosition() const
{
    int visibleContentHeight = visibleContentRect().height();

    int maxY = contentsHeight() - visibleContentHeight;
    if (maxY == 0)
        return 0;

    int y = scrollY();

    if (!scrollOrigin().y()) {
        if (y < 0)
            y = 0;
        else if (y > maxY)
            y = maxY;
    } else {
        if (y > 0)
            y = 0;
        else if (y < -maxY)
            y = -maxY;
    }

    if (!m_frame)
        return y;

    float frameScaleFactor = m_frame->frameScaleFactor();
    float dragFactor = fixedElementsLayoutRelativeToFrame() ? 1 : (contentsHeight() - visibleContentHeight * frameScaleFactor) / maxY;
    return y * dragFactor / frameScaleFactor;
}

IntSize FrameView::scrollOffsetForFixedPosition() const
{
    return IntSize(scrollXForFixedPosition(), scrollYForFixedPosition());
}

bool FrameView::fixedElementsLayoutRelativeToFrame() const
{
    ASSERT(m_frame);
    if (!m_frame->settings())
        return false;

    return m_frame->settings()->fixedElementsLayoutRelativeToFrame();
}

IntPoint FrameView::currentMousePosition() const
{
    return m_frame ? m_frame->eventHandler()->currentMousePosition() : IntPoint();
}

bool FrameView::scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    TrackedRendererListHashSet* positionedObjects = 0;
    if (RenderView* root = rootRenderer(this))
        positionedObjects = root->positionedObjects();

    if (!positionedObjects || positionedObjects->isEmpty()) {
        hostWindow()->scroll(scrollDelta, rectToScroll, clipRect);
        return true;
    }

    const bool isCompositedContentLayer = contentsInCompositedLayer();

    // Get the rects of the fixed objects visible in the rectToScroll
    Region regionToUpdate;
    TrackedRendererListHashSet::const_iterator end = positionedObjects->end();
    for (TrackedRendererListHashSet::const_iterator it = positionedObjects->begin(); it != end; ++it) {
        RenderBox* renderBox = *it;
        if (renderBox->style()->position() != FixedPosition)
            continue;
#if USE(ACCELERATED_COMPOSITING)
        if (renderBox->isComposited())
            continue;
#endif
        IntRect updateRect = pixelSnappedIntRect(renderBox->layer()->repaintRectIncludingNonCompositingDescendants());
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
    size_t fixObjectsCount = subRectsToUpdate.size();
    for (size_t i = 0; i < fixObjectsCount; ++i) {
        IntRect updateRect = subRectsToUpdate[i];
        IntRect scrolledRect = updateRect;
        scrolledRect.move(scrollDelta);
        updateRect.unite(scrolledRect);
#if USE(ACCELERATED_COMPOSITING)
        if (isCompositedContentLayer) {
            updateRect = rootViewToContents(updateRect);
            RenderView* root = rootRenderer(this);
            ASSERT(root);
            root->layer()->setBackingNeedsRepaintInRect(updateRect);
            continue;
        }
#endif
        if (clipsRepaints())
            updateRect.intersect(rectToScroll);
        hostWindow()->invalidateContentsAndRootView(updateRect, false);
    }

    return true;
}

void FrameView::scrollContentsSlowPath(const IntRect& updateRect)
{
#if USE(ACCELERATED_COMPOSITING)
    if (contentsInCompositedLayer()) {
        RenderView* root = rootRenderer(this);
        ASSERT(root);

        IntRect updateRect = visibleContentRect();

        // Make sure to "apply" the scale factor here since we're converting from frame view
        // coordinates to layer backing coordinates.
        updateRect.scale(1 / m_frame->frameScaleFactor());

        root->layer()->setBackingNeedsRepaintInRect(updateRect);
    }
    if (RenderPart* frameRenderer = m_frame->ownerRenderer()) {
        if (isEnclosedInCompositingLayer()) {
            LayoutRect rect(frameRenderer->borderLeft() + frameRenderer->paddingLeft(),
                            frameRenderer->borderTop() + frameRenderer->paddingTop(),
                            visibleWidth(), visibleHeight());
            frameRenderer->repaintRectangle(rect);
            return;
        }
    }
#endif

    ScrollView::scrollContentsSlowPath(updateRect);
}

// Note that this gets called at painting time.
void FrameView::setIsOverlapped(bool isOverlapped)
{
    if (isOverlapped == m_isOverlapped)
        return;

    m_isOverlapped = isOverlapped;
    updateCanBlitOnScrollRecursively();
    
#if USE(ACCELERATED_COMPOSITING)
    if (hasCompositedContentIncludingDescendants()) {
        // Overlap can affect compositing tests, so if it changes, we need to trigger
        // a layer update in the parent document.
        if (Frame* parentFrame = m_frame->tree()->parent()) {
            if (RenderView* parentView = parentFrame->contentRenderer()) {
                RenderLayerCompositor* compositor = parentView->compositor();
                compositor->setCompositingLayersNeedRebuild();
                compositor->scheduleCompositingLayerUpdate();
            }
        }

        if (RenderLayerCompositor::allowsIndependentlyCompositedFrames(this)) {
            // We also need to trigger reevaluation for this and all descendant frames,
            // since a frame uses compositing if any ancestor is compositing.
            for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
                if (RenderView* view = frame->contentRenderer()) {
                    RenderLayerCompositor* compositor = view->compositor();
                    compositor->setCompositingLayersNeedRebuild();
                    compositor->scheduleCompositingLayerUpdate();
                }
            }
        }
    }
#endif
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

bool FrameView::scrollToFragment(const KURL& url)
{
    // If our URL has no ref, then we have no place we need to jump to.
    // OTOH If CSS target was set previously, we want to set it to 0, recalc
    // and possibly repaint because :target pseudo class may have been
    // set (see bug 11321).
    if (!url.hasFragmentIdentifier() && !m_frame->document()->cssTarget())
        return false;

    String fragmentIdentifier = url.fragmentIdentifier();
    if (scrollToAnchor(fragmentIdentifier))
        return true;

    // Try again after decoding the ref, based on the document's encoding.
    if (TextResourceDecoder* decoder = m_frame->document()->decoder())
        return scrollToAnchor(decodeURLEscapeSequences(fragmentIdentifier, decoder->encoding()));

    return false;
}

bool FrameView::scrollToAnchor(const String& name)
{
    ASSERT(m_frame->document());

    if (!m_frame->document()->haveStylesheetsLoaded()) {
        m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(true);
        return false;
    }

    m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(false);

    Element* anchorNode = m_frame->document()->findAnchor(name);

    // Setting to null will clear the current target.
    m_frame->document()->setCSSTarget(anchorNode);

#if ENABLE(SVG)
    if (m_frame->document()->isSVGDocument()) {
        if (SVGSVGElement* svg = static_cast<SVGDocument*>(m_frame->document())->rootElement()) {
            svg->setupInitialView(name, anchorNode);
            if (!anchorNode)
                return true;
        }
    }
#endif
  
    // Implement the rule that "" and "top" both mean top of page as in other browsers.
    if (!anchorNode && !(name.isEmpty() || equalIgnoringCase(name, "top")))
        return false;

    maintainScrollPositionAtAnchor(anchorNode ? static_cast<Node*>(anchorNode) : m_frame->document());
    return true;
}

void FrameView::maintainScrollPositionAtAnchor(Node* anchorNode)
{
    m_maintainScrollPositionAnchor = anchorNode;
    if (!m_maintainScrollPositionAnchor)
        return;

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    m_frame->document()->updateStyleIfNeeded();
    // Only do a layout if changes have occurred that make it necessary.
    RenderView* root = rootRenderer(this);
    if (root && root->needsLayout())
        layout();
    else
        scrollToAnchor();
}

void FrameView::scrollElementToRect(Element* element, const IntRect& rect)
{
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    LayoutRect bounds = element->getRect();
    int centeringOffsetX = (rect.width() - bounds.width()) / 2;
    int centeringOffsetY = (rect.height() - bounds.height()) / 2;
    setScrollPosition(IntPoint(bounds.x() - centeringOffsetX - rect.x(), bounds.y() - centeringOffsetY - rect.y()));
}

void FrameView::setScrollPosition(const IntPoint& scrollPoint)
{
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);
    m_maintainScrollPositionAnchor = 0;

    if (requestScrollPositionUpdate(scrollPoint))
        return;

    ScrollView::setScrollPosition(scrollPoint);
}

void FrameView::delegatesScrollingDidChange()
{
#if USE(ACCELERATED_COMPOSITING)
    // When we switch to delgatesScrolling mode, we should destroy the scrolling/clipping layers in RenderLayerCompositor.
    if (hasCompositedContent())
        clearBackingStores();
#endif
}

void FrameView::setFixedVisibleContentRect(const IntRect& visibleContentRect)
{
    if (visibleContentRect.size() != this->fixedVisibleContentRect().size()) {
        // When the viewport size changes or the content is scaled, we need to
        // reposition the fixed positioned elements.
        if (RenderView* root = rootRenderer(this))
            root->setFixedPositionedObjectsNeedLayout();
    }

    IntSize offset = scrollOffset();
    ScrollView::setFixedVisibleContentRect(visibleContentRect);
    if (offset != scrollOffset()) {
        if (m_frame->page()->settings()->acceleratedCompositingForFixedPositionEnabled())
            updateFixedElementsAfterScrolling();
        scrollPositionChanged();
    }
    frame()->loader()->client()->didChangeScrollOffset();
}

void FrameView::scrollPositionChangedViaPlatformWidget()
{
    repaintFixedElementsAfterScrolling();
    updateFixedElementsAfterScrolling();
    scrollPositionChanged();
}

void FrameView::scrollPositionChanged()
{
    frame()->eventHandler()->sendScrollEvent();
    frame()->eventHandler()->dispatchFakeMouseMoveEventSoon();

#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* root = rootRenderer(this)) {
        if (root->usesCompositing())
            root->compositor()->frameViewDidScroll();
    }
#endif
}

void FrameView::repaintFixedElementsAfterScrolling()
{
    // For fixed position elements, update widget positions and compositing layers after scrolling,
    // but only if we're not inside of layout.
    if (!m_nestedLayoutCount && hasFixedObjects()) {
        if (RenderView* root = rootRenderer(this)) {
            root->updateWidgetPositions();
            root->layer()->updateLayerPositionsAfterDocumentScroll();
        }
    }
}

void FrameView::updateFixedElementsAfterScrolling()
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_nestedLayoutCount && hasFixedObjects()) {
        if (RenderView* root = rootRenderer(this)) {
            root->compositor()->updateCompositingLayers(CompositingUpdateOnScroll);
        }
    }
#endif
}

bool FrameView::shouldRubberBandInDirection(ScrollDirection direction) const
{
    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return ScrollView::shouldRubberBandInDirection(direction);
    return page->chrome()->client()->shouldRubberBandInDirection(direction);
}

bool FrameView::requestScrollPositionUpdate(const IntPoint& position)
{
#if ENABLE(THREADED_SCROLLING)
    if (Page* page = m_frame->page()) {
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
    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return 0;
    return page->chrome();
}

const unsigned cRepaintRectUnionThreshold = 25;

void FrameView::repaintContentRectangle(const IntRect& r, bool immediate)
{
    ASSERT(!m_frame->ownerElement());

    if (m_isTrackingRepaints) {
        IntRect repaintRect = r;
        repaintRect.move(-scrollOffset());
        m_trackedRepaintRects.append(repaintRect);
    }

    double delay = m_deferringRepaints ? 0 : adjustedDeferredRepaintDelay();
    if ((m_deferringRepaints || m_deferredRepaintTimer.isActive() || delay) && !immediate) {
        IntRect paintRect = r;
        if (clipsRepaints() && !paintsEntireContents())
            paintRect.intersect(visibleContentRect());
        if (paintRect.isEmpty())
            return;
        if (m_repaintCount == cRepaintRectUnionThreshold) {
            IntRect unionedRect;
            for (unsigned i = 0; i < cRepaintRectUnionThreshold; ++i)
                unionedRect.unite(pixelSnappedIntRect(m_repaintRects[i]));
            m_repaintRects.clear();
            m_repaintRects.append(unionedRect);
        }
        if (m_repaintCount < cRepaintRectUnionThreshold)
            m_repaintRects.append(paintRect);
        else
            m_repaintRects[0].unite(paintRect);
        m_repaintCount++;

        if (!m_deferringRepaints)
            startDeferredRepaintTimer(delay);

        return;
    }
    
    if (!shouldUpdate(immediate))
        return;

#if USE(TILED_BACKING_STORE)
    if (frame()->tiledBackingStore()) {
        frame()->tiledBackingStore()->invalidate(r);
        return;
    }
#endif
    ScrollView::repaintContentRectangle(r, immediate);
}

void FrameView::contentsResized()
{
    ScrollView::contentsResized();
    setNeedsLayout();
}

void FrameView::visibleContentsResized()
{
    // We check to make sure the view is attached to a frame() as this method can
    // be triggered before the view is attached by Frame::createView(...) setting
    // various values such as setScrollBarModes(...) for example.  An ASSERT is
    // triggered when a view is layout before being attached to a frame().
    if (!frame()->view())
        return;

    if (needsLayout())
        layout();

#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* root = rootRenderer(this)) {
        if (root->usesCompositing())
            root->compositor()->frameViewDidChangeSize();
    }
#endif
}

void FrameView::beginDeferredRepaints()
{
    Page* page = m_frame->page();
    if (page->mainFrame() != m_frame) {
        page->mainFrame()->view()->beginDeferredRepaints();
        return;
    }

    m_deferringRepaints++;
}

void FrameView::endDeferredRepaints()
{
    Page* page = m_frame->page();
    if (page->mainFrame() != m_frame) {
        page->mainFrame()->view()->endDeferredRepaints();
        return;
    }

    ASSERT(m_deferringRepaints > 0);

    if (--m_deferringRepaints)
        return;

    if (m_deferredRepaintTimer.isActive())
        return;

    if (double delay = adjustedDeferredRepaintDelay()) {
        startDeferredRepaintTimer(delay);
        return;
    }
    
    doDeferredRepaints();
}

void FrameView::startDeferredRepaintTimer(double delay)
{
    if (m_deferredRepaintTimer.isActive())
        return;

    if (m_disableRepaints)
        return;

    m_deferredRepaintTimer.startOneShot(delay);
}

void FrameView::checkStopDelayingDeferredRepaints()
{
    if (!m_deferredRepaintTimer.isActive())
        return;

    Document* document = m_frame->document();
    if (document && (document->parsing() || document->cachedResourceLoader()->requestCount()))
        return;
    
    m_deferredRepaintTimer.stop();

    doDeferredRepaints();
}
    
void FrameView::doDeferredRepaints()
{
    if (m_disableRepaints)
        return;

    ASSERT(!m_deferringRepaints);
    if (!shouldUpdate()) {
        m_repaintRects.clear();
        m_repaintCount = 0;
        return;
    }
    unsigned size = m_repaintRects.size();
    for (unsigned i = 0; i < size; i++) {
#if USE(TILED_BACKING_STORE)
        if (frame()->tiledBackingStore()) {
            frame()->tiledBackingStore()->invalidate(pixelSnappedIntRect(m_repaintRects[i]));
            continue;
        }
#endif
        ScrollView::repaintContentRectangle(pixelSnappedIntRect(m_repaintRects[i]), false);
    }
    m_repaintRects.clear();
    m_repaintCount = 0;
    
    updateDeferredRepaintDelay();
}

void FrameView::updateDeferredRepaintDelay()
{
    Document* document = m_frame->document();
    if (!document || (!document->parsing() && !document->cachedResourceLoader()->requestCount())) {
        m_deferredRepaintDelay = s_deferredRepaintDelay;
        return;
    }
    if (m_deferredRepaintDelay < s_maxDeferredRepaintDelayDuringLoading) {
        m_deferredRepaintDelay += s_deferredRepaintDelayIncrementDuringLoading;
        if (m_deferredRepaintDelay > s_maxDeferredRepaintDelayDuringLoading)
            m_deferredRepaintDelay = s_maxDeferredRepaintDelayDuringLoading;
    }
}

void FrameView::resetDeferredRepaintDelay()
{
    m_deferredRepaintDelay = 0;
    if (m_deferredRepaintTimer.isActive()) {
        m_deferredRepaintTimer.stop();
        if (!m_deferringRepaints)
            doDeferredRepaints();
    }
}

double FrameView::adjustedDeferredRepaintDelay() const
{
    ASSERT(!m_deferringRepaints);
    if (!m_deferredRepaintDelay)
        return 0;
    double timeSinceLastPaint = currentTime() - m_lastPaintTime;
    return max(0., m_deferredRepaintDelay - timeSinceLastPaint);
}
    
void FrameView::deferredRepaintTimerFired(Timer<FrameView>*)
{
    doDeferredRepaints();
}

void FrameView::beginDisableRepaints()
{
    m_disableRepaints++;
}

void FrameView::endDisableRepaints()
{
    ASSERT(m_disableRepaints > 0);
    m_disableRepaints--;
}

void FrameView::layoutTimerFired(Timer<FrameView>*)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_frame->document()->ownerElement())
        printf("Layout timer fired at %d\n", m_frame->document()->elapsedTime());
#endif
    layout();
}

void FrameView::scheduleRelayout()
{
    // FIXME: We should assert the page is not in the page cache, but that is causing
    // too many false assertions.  See <rdar://problem/7218118>.
    ASSERT(m_frame->view() == this);

    if (m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }
    if (!m_layoutSchedulingEnabled)
        return;
    if (!needsLayout())
        return;
    if (!m_frame->document()->shouldScheduleLayout())
        return;

    // When frame flattening is enabled, the contents of the frame could affect the layout of the parent frames.
    // Also invalidate parent frame starting from the owner element of this frame.
    if (isInChildFrameWithFrameFlattening() && m_frame->ownerRenderer())
        m_frame->ownerRenderer()->setNeedsLayout(true, MarkContainingBlockChain);

    int delay = m_frame->document()->minimumLayoutDelay();
    if (m_layoutTimer.isActive() && m_delayedLayout && !delay)
        unscheduleRelayout();
    if (m_layoutTimer.isActive())
        return;

    m_delayedLayout = delay != 0;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_frame->document()->ownerElement())
        printf("Scheduling layout for %d\n", delay);
#endif

    m_layoutTimer.startOneShot(delay * 0.001);
}

static bool isObjectAncestorContainerOf(RenderObject* ancestor, RenderObject* descendant)
{
    for (RenderObject* r = descendant; r; r = r->container()) {
        if (r == ancestor)
            return true;
    }
    return false;
}

void FrameView::scheduleRelayoutOfSubtree(RenderObject* relayoutRoot)
{
    ASSERT(m_frame->view() == this);

    RenderView* root = rootRenderer(this);
    if (root && root->needsLayout()) {
        if (relayoutRoot)
            relayoutRoot->markContainingBlocksForLayout(false);
        return;
    }

    if (layoutPending() || !m_layoutSchedulingEnabled) {
        if (m_layoutRoot != relayoutRoot) {
            if (isObjectAncestorContainerOf(m_layoutRoot, relayoutRoot)) {
                // Keep the current root
                relayoutRoot->markContainingBlocksForLayout(false, m_layoutRoot);
                ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
            } else if (m_layoutRoot && isObjectAncestorContainerOf(relayoutRoot, m_layoutRoot)) {
                // Re-root at relayoutRoot
                m_layoutRoot->markContainingBlocksForLayout(false, relayoutRoot);
                m_layoutRoot = relayoutRoot;
                ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
            } else {
                // Just do a full relayout
                if (m_layoutRoot)
                    m_layoutRoot->markContainingBlocksForLayout(false);
                m_layoutRoot = 0;
                relayoutRoot->markContainingBlocksForLayout(false);
            }
        }
    } else if (m_layoutSchedulingEnabled) {
        int delay = m_frame->document()->minimumLayoutDelay();
        m_layoutRoot = relayoutRoot;
        ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
        m_delayedLayout = delay != 0;
        m_layoutTimer.startOneShot(delay * 0.001);
    }
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
    if (!m_frame)
        return false;

    RenderView* root = rootRenderer(this);
    return layoutPending()
        || (root && root->needsLayout())
        || m_layoutRoot
        || (m_deferSetNeedsLayouts && m_setNeedsLayoutWasDeferred);
}

void FrameView::setNeedsLayout()
{
    if (m_deferSetNeedsLayouts) {
        m_setNeedsLayoutWasDeferred = true;
        return;
    }

    if (RenderView* root = rootRenderer(this))
        root->setNeedsLayout(true);
}

void FrameView::unscheduleRelayout()
{
    if (!m_layoutTimer.isActive())
        return;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_frame->document()->ownerElement())
        printf("Layout timer unscheduled at %d\n", m_frame->document()->elapsedTime());
#endif
    
    m_layoutTimer.stop();
    m_delayedLayout = false;
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void FrameView::serviceScriptedAnimations(DOMTimeStamp time)
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext()) {
        frame->view()->serviceScrollAnimations();
        frame->animation()->serviceAnimations();
    }

    Vector<RefPtr<Document> > documents;
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext())
        documents.append(frame->document());

    for (size_t i = 0; i < documents.size(); ++i)
        documents[i]->serviceScriptedAnimations(time);
}
#endif

bool FrameView::isTransparent() const
{
    return m_isTransparent;
}

void FrameView::setTransparent(bool isTransparent)
{
    m_isTransparent = isTransparent;
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
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
        if (FrameView* view = frame->view()) {
            view->setTransparent(transparent);
            view->setBaseBackgroundColor(backgroundColor);
        }
    }
}

bool FrameView::shouldUpdateWhileOffscreen() const
{
    return m_shouldUpdateWhileOffscreen;
}

void FrameView::setShouldUpdateWhileOffscreen(bool shouldUpdateWhileOffscreen)
{
    m_shouldUpdateWhileOffscreen = shouldUpdateWhileOffscreen;
}

bool FrameView::shouldUpdate(bool immediateRequested) const
{
    if (!immediateRequested && isOffscreen() && !shouldUpdateWhileOffscreen())
        return false;
    return true;
}

void FrameView::scheduleEvent(PassRefPtr<Event> event, PassRefPtr<Node> eventTarget)
{
    m_actionScheduler->scheduleEvent(event, eventTarget);
}

void FrameView::pauseScheduledEvents()
{
    m_actionScheduler->pause();
}

void FrameView::resumeScheduledEvents()
{
    m_actionScheduler->resume();
}

void FrameView::scrollToAnchor()
{
    RefPtr<Node> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;

    if (!anchorNode->renderer())
        return;

    LayoutRect rect;
    if (anchorNode != m_frame->document())
        rect = anchorNode->getRect();

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    anchorNode->renderer()->scrollRectToVisible(rect, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);

    if (AXObjectCache::accessibilityEnabled())
        m_frame->document()->axObjectCache()->handleScrolledToAnchor(anchorNode.get());

    // scrollRectToVisible can call into setScrollPosition(), which resets m_maintainScrollPositionAnchor.
    m_maintainScrollPositionAnchor = anchorNode;
}

void FrameView::updateWidget(RenderEmbeddedObject* object)
{
    ASSERT(!object->node() || object->node()->isElementNode());
    Element* ownerElement = static_cast<Element*>(object->node());
    // The object may have already been destroyed (thus node cleared),
    // but FrameView holds a manual ref, so it won't have been deleted.
    ASSERT(m_widgetUpdateSet->contains(object));
    if (!ownerElement)
        return;

    // No need to update if it's already crashed or known to be missing.
    if (object->showsUnavailablePluginIndicator())
        return;

    // FIXME: This could turn into a real virtual dispatch if we defined
    // updateWidget(PluginCreationOption) on HTMLElement.
    if (ownerElement->hasTagName(objectTag) || ownerElement->hasTagName(embedTag) || ownerElement->hasTagName(appletTag)) {
        HTMLPlugInImageElement* pluginElement = static_cast<HTMLPlugInImageElement*>(ownerElement);
        if (pluginElement->needsWidgetUpdate())
            pluginElement->updateWidget(CreateAnyWidgetType);
    }
    // FIXME: It is not clear that Media elements need or want this updateWidget() call.
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    else if (ownerElement->isMediaElement())
        static_cast<HTMLMediaElement*>(ownerElement)->updateWidget(CreateAnyWidgetType);
#endif
    else
        ASSERT_NOT_REACHED();

    // Caution: it's possible the object was destroyed again, since loading a
    // plugin may run any arbitrary javascript.
    object->updateWidgetPosition();
}

bool FrameView::updateWidgets()
{
    if (m_nestedLayoutCount > 1 || !m_widgetUpdateSet || m_widgetUpdateSet->isEmpty())
        return true;
    
    size_t size = m_widgetUpdateSet->size();

    Vector<RenderEmbeddedObject*> objects;
    objects.reserveCapacity(size);

    RenderEmbeddedObjectSet::const_iterator end = m_widgetUpdateSet->end();
    for (RenderEmbeddedObjectSet::const_iterator it = m_widgetUpdateSet->begin(); it != end; ++it) {
        objects.uncheckedAppend(*it);
        (*it)->ref();
    }

    for (size_t i = 0; i < size; ++i) {
        RenderEmbeddedObject* object = objects[i];
        updateWidget(object);
        m_widgetUpdateSet->remove(object);
    }

    RenderArena* arena = m_frame->document()->renderArena();
    for (size_t i = 0; i < size; ++i)
        objects[i]->deref(arena);
    
    return m_widgetUpdateSet->isEmpty();
}

void FrameView::flushAnyPendingPostLayoutTasks()
{
    if (!m_postLayoutTasksTimer.isActive())
        return;

    performPostLayoutTasks();
}

void FrameView::performPostLayoutTasks()
{
    m_postLayoutTasksTimer.stop();

    m_frame->selection()->setCaretRectNeedsUpdate();
    m_frame->selection()->updateAppearance();

    if (m_nestedLayoutCount <= 1) {
        if (m_firstLayoutCallbackPending) {
            m_firstLayoutCallbackPending = false;
            m_frame->loader()->didFirstLayout();
            if (Page* page = m_frame->page()) {
                if (page->mainFrame() == m_frame)
                    page->startCountingRelevantRepaintedObjects();
            }
        }

        // Ensure that we always send this eventually.
        if (!m_frame->document()->parsing() && m_frame->loader()->stateMachine()->committedFirstRealDocumentLoad())
            m_isVisuallyNonEmpty = true;

        // If the layout was done with pending sheets, we are not in fact visually non-empty yet.
        if (m_isVisuallyNonEmpty && !m_frame->document()->didLayoutWithPendingStylesheets() && m_firstVisuallyNonEmptyLayoutCallbackPending) {
            m_firstVisuallyNonEmptyLayoutCallbackPending = false;
            m_frame->loader()->didFirstVisuallyNonEmptyLayout();
        }
    }

    m_frame->loader()->client()->dispatchDidLayout();

    RenderView* root = rootRenderer(this);
    root->updateWidgetPositions();
    
    for (unsigned i = 0; i < maxUpdateWidgetsIterations; i++) {
        if (updateWidgets())
            break;
    }

    Page* page = m_frame->page();
    if (page) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewLayoutUpdated(this);
    }

#if USE(ACCELERATED_COMPOSITING)
    if (TiledBacking* tiledBacking = this->tiledBacking()) {
        if (page) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator()) {
                bool shouldLimitTileCoverage = !canHaveScrollbars() || scrollingCoordinator->shouldUpdateScrollLayerPositionOnMainThread();
                tiledBacking->setTileCoverage(shouldLimitTileCoverage ? TiledBacking::CoverageForVisibleArea : TiledBacking::CoverageForScrolling);
            }
        }
    }
#endif

    scrollToAnchor();

    m_actionScheduler->resume();

    if (!root->printing()) {
        IntSize currentSize;
        if (useFixedLayout() && !fixedLayoutSize().isEmpty() && delegatesScrolling())
            currentSize = fixedLayoutSize();
        else
            currentSize = visibleContentRect(true /*includeScrollbars*/).size();
        float currentZoomFactor = root->style()->zoom();
        bool resized = !m_firstLayout && (currentSize != m_lastViewportSize || currentZoomFactor != m_lastZoomFactor);
        m_lastViewportSize = currentSize;
        m_lastZoomFactor = currentZoomFactor;
        if (resized) {
            m_frame->eventHandler()->sendResizeEvent();

#if ENABLE(INSPECTOR)
            if (InspectorInstrumentation::hasFrontends()) {
                if (Page* page = m_frame->page()) {
                    if (page->mainFrame() == m_frame) {
                        if (InspectorClient* inspectorClient = page->inspectorController()->inspectorClient())
                            inspectorClient->didResizeMainFrame(m_frame.get());
                    }
                }
            }
#endif
        }
    }
}

void FrameView::postLayoutTimerFired(Timer<FrameView>*)
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

    Document* document = frame()->document();
    if (!document)
        return;

    RenderView* documentView = document->renderView();
    Element* documentElement = document->documentElement();
    if (!documentView || !documentElement)
        return;

    RenderBox* documentRenderBox = documentElement->renderBox();
    if (!documentRenderBox)
        return;

    // If this is the first time we run autosize, start from small height and
    // allow it to grow.
    if (!m_didRunAutosize)
        resize(frameRect().width(), m_minAutoSize.height());

    IntSize size = frameRect().size();

    // Do the resizing twice. The first time is basically a rough calculation using the preferred width
    // which may result in a height change during the second iteration.
    for (int i = 0; i < 2; i++) {
        // Update various sizes including contentsSize, scrollHeight, etc.
        document->updateLayoutIgnorePendingStylesheets();
        int width = documentView->minPreferredLogicalWidth();
        int height = documentRenderBox->scrollHeight();
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

        // Avoid doing resizing to a smaller size while the frame is loading to avoid switching to a small size
        // during an intermediate state (and then changing back to a bigger size as the load progresses).
        if (!frame()->loader()->isComplete() && (newSize.height() < size.height() || newSize.width() < size.width()))
            break;
        else if (document->processingLoadEvent())
            newSize = newSize.expandedTo(size);
        resize(newSize.width(), newSize.height());
        // Force the scrollbar state to avoid the scrollbar code adding them and causing them to be needed. For example,
        // a vertical scrollbar may cause text to wrap and thus increase the height (which is the only reason the scollbar is needed).
        setVerticalScrollbarLock(false);
        setHorizontalScrollbarLock(false);
        setScrollbarModes(horizonalScrollbarMode, verticalScrollbarMode, true, true);
    }
    m_didRunAutosize = true;
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
        
        m_actionScheduler->scheduleEvent(OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow,
            verticalOverflowChanged, verticalOverflow),
            m_viewportRenderer->node());
    }
    
}

IntRect FrameView::windowClipRect(bool clipToContents) const
{
    ASSERT(m_frame->view() == this);

    if (paintsEntireContents())
        return IntRect(IntPoint(), contentsSize());

    // Set our clip rect to be our contents.
    IntRect clipRect = contentsToWindow(visibleContentRect(!clipToContents));
    if (!m_frame || !m_frame->ownerElement())
        return clipRect;

    // Take our owner element and get the clip rect from the enclosing layer.
    Element* elt = m_frame->ownerElement();
    // The renderer can sometimes be null when style="display:none" interacts
    // with external content and plugins.
    RenderLayer* layer = elt->renderer() ? elt->renderer()->enclosingLayer() : 0;
    if (!layer)
        return clipRect;
    FrameView* parentView = elt->document()->view();
    clipRect.intersect(parentView->windowClipRectForLayer(layer, true));
    return clipRect;
}

IntRect FrameView::windowClipRectForLayer(const RenderLayer* layer, bool clipToLayerContents) const
{
    // If we have no layer, just return our window clip rect.
    if (!layer)
        return windowClipRect();

    // Apply the clip from the layer.
    IntRect clipRect;
    if (clipToLayerContents)
        clipRect = pixelSnappedIntRect(layer->childrenClipRect());
    else
        clipRect = pixelSnappedIntRect(layer->selfClipRect());
    clipRect = contentsToWindow(clipRect); 
    return intersection(clipRect, windowClipRect());
}

bool FrameView::isActive() const
{
    Page* page = frame()->page();
    return page && page->focusController()->isActive();
}

void FrameView::scrollTo(const IntSize& newOffset)
{
    LayoutSize offset = scrollOffset();
    ScrollView::scrollTo(newOffset);
    if (offset != scrollOffset())
        scrollPositionChanged();
    frame()->loader()->client()->didChangeScrollOffset();
}

void FrameView::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    // Add in our offset within the FrameView.
    IntRect dirtyRect = rect;
    dirtyRect.moveBy(scrollbar->location());
    invalidateRect(dirtyRect);
}

void FrameView::getTickmarks(Vector<IntRect>& tickmarks) const
{
    tickmarks = frame()->document()->markers()->renderedRectsForMarkers(DocumentMarker::TextMatch);
}

IntRect FrameView::windowResizerRect() const
{
    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return IntRect();
    return page->chrome()->windowResizerRect();
}

void FrameView::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
{
    Page* page = m_frame->page();
    if (!page)
        return;
    if (page->mainFrame() != m_frame)
        return;
    page->chrome()->client()->notifyScrollerThumbIsVisibleInRect(scrollerThumb);
}

bool FrameView::isOnActivePage() const
{
    if (!m_frame)
        return false;
    if (m_frame->view() != this)
        return false;
    if (Document* document = m_frame->document())
        return !document->inPageCache();
    return false;
}

ScrollableArea* FrameView::enclosingScrollableArea() const
{
    // FIXME: Walk up the frame tree and look for a scrollable parent frame or RenderLayer.
    return 0;
}

IntRect FrameView::scrollableAreaBoundingBox() const
{
    // FIXME: This isn't correct for transformed frames. We probably need to ask the renderer instead.
    return frameRect();
}

void FrameView::updateScrollableAreaSet()
{
    // That ensures that only inner frames are cached.
    if (!parentFrameView())
        return;

    // Check for:
    // 1) display:none or visibility:hidden set to self or inherited.
    // 2) overflow{-x,-y}: hidden;
    // 3) scrolling: no;

    // Covers #1.
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    if (!owner || !owner->renderer() || !owner->renderer()->visibleToHitTesting()) {
        parentFrameView()->removeScrollableArea(this);
        return;
    }

    IntSize contentSize = contentsSize();
    IntSize visibleContentSize = visibleContentRect().size();
    if ((contentSize.height() <= visibleContentSize.height() && contentSize.width() <= visibleContentSize.width())) {
        parentFrameView()->removeScrollableArea(this);
        return;
    }

    // Cover #2 and #3.
    ScrollbarMode horizontalMode;
    ScrollbarMode verticalMode;
    calculateScrollbarModesForLayout(horizontalMode, verticalMode, RulesFromWebContentOnly);
    if (horizontalMode == ScrollbarAlwaysOff && verticalMode == ScrollbarAlwaysOff) {
        parentFrameView()->removeScrollableArea(this);
        return;
    }

    parentFrameView()->addScrollableArea(this);
}

bool FrameView::shouldSuspendScrollAnimations() const
{
    return m_frame->loader()->state() != FrameStateComplete;
}

void FrameView::scrollbarStyleChanged(int newStyle, bool forceUpdate)
{
    Page* page = m_frame->page();
    if (!page)
        return;
    if (page->mainFrame() != m_frame)
        return;
    page->chrome()->client()->recommendedScrollbarStyleDidChange(newStyle);

    if (forceUpdate)
        ScrollView::scrollbarStyleChanged(newStyle, forceUpdate);
}

void FrameView::setAnimatorsAreActive()
{
    Page* page = m_frame->page();
    if (!page)
        return;

    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->setIsActive();

    if (!m_scrollableAreas)
        return;

    for (HashSet<ScrollableArea*>::const_iterator it = m_scrollableAreas->begin(), end = m_scrollableAreas->end(); it != end; ++it) {
        ScrollableArea* scrollableArea = *it;

        ASSERT(scrollableArea->isOnActivePage());
        scrollableArea->scrollAnimator()->setIsActive();
    }
}

void FrameView::notifyPageThatContentAreaWillPaint() const
{
    Page* page = m_frame->page();
    if (!page)
        return;

    contentAreaWillPaint();

    if (!m_scrollableAreas)
        return;

    for (HashSet<ScrollableArea*>::const_iterator it = m_scrollableAreas->begin(), end = m_scrollableAreas->end(); it != end; ++it) {
        ScrollableArea* scrollableArea = *it;

        if (!scrollableArea->isOnActivePage())
            continue;

        scrollableArea->contentAreaWillPaint();
    }
}

bool FrameView::scrollAnimatorEnabled() const
{
#if ENABLE(SMOOTH_SCROLLING)
    if (Page* page = m_frame->page())
        return page->settings()->scrollAnimatorEnabled();
#endif

    return false;
}

#if ENABLE(DASHBOARD_SUPPORT)
void FrameView::updateDashboardRegions()
{
    Document* document = m_frame->document();
    if (!document->hasDashboardRegions())
        return;
    Vector<DashboardRegionValue> newRegions;
    document->renderBox()->collectDashboardRegions(newRegions);
    if (newRegions == document->dashboardRegions())
        return;
    document->setDashboardRegions(newRegions);
    Page* page = m_frame->page();
    if (!page)
        return;
    page->chrome()->client()->dashboardRegionsChanged();
}
#endif

void FrameView::updateScrollCorner()
{
    RenderObject* renderer = 0;
    RefPtr<RenderStyle> cornerStyle;
    IntRect cornerRect = scrollCornerRect();
    
    if (!cornerRect.isEmpty()) {
        // Try the <body> element first as a scroll corner source.
        Document* doc = m_frame->document();
        Element* body = doc ? doc->body() : 0;
        if (body && body->renderer()) {
            renderer = body->renderer();
            cornerStyle = renderer->getUncachedPseudoStyle(SCROLLBAR_CORNER, renderer->style());
        }
        
        if (!cornerStyle) {
            // If the <body> didn't have a custom style, then the root element might.
            Element* docElement = doc ? doc->documentElement() : 0;
            if (docElement && docElement->renderer()) {
                renderer = docElement->renderer();
                cornerStyle = renderer->getUncachedPseudoStyle(SCROLLBAR_CORNER, renderer->style());
            }
        }
        
        if (!cornerStyle) {
            // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
            if (RenderPart* renderer = m_frame->ownerRenderer())
                cornerStyle = renderer->getUncachedPseudoStyle(SCROLLBAR_CORNER, renderer->style());
        }
    }

    if (cornerStyle) {
        if (!m_scrollCorner)
            m_scrollCorner = new (renderer->renderArena()) RenderScrollbarPart(renderer->document());
        m_scrollCorner->setStyle(cornerStyle.release());
        invalidateScrollCorner(cornerRect);
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
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
        bool needsBackgorund = m_frame->page() && m_frame->page()->mainFrame() == m_frame;
        if (needsBackgorund)
            context->fillRect(cornerRect, baseBackgroundColor(), ColorSpaceDeviceRGB);
        m_scrollCorner->paintIntoRect(context, cornerRect.location(), cornerRect);
        return;
    }

    ScrollView::paintScrollCorner(context, cornerRect);
}

void FrameView::paintScrollbar(GraphicsContext* context, Scrollbar* bar, const IntRect& rect)
{
    bool needsBackgorund = bar->isCustomScrollbar() && (m_frame->page() && m_frame->page()->mainFrame() == m_frame);
    if (needsBackgorund) {
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
    if (!frame()->document())
        return Color();

    Element* htmlElement = frame()->document()->documentElement();
    Element* bodyElement = frame()->document()->body();

    // Start with invalid colors.
    Color htmlBackgroundColor;
    Color bodyBackgroundColor;
    if (htmlElement && htmlElement->renderer())
        htmlBackgroundColor = htmlElement->renderer()->style()->visitedDependentColor(CSSPropertyBackgroundColor);
    if (bodyElement && bodyElement->renderer())
        bodyBackgroundColor = bodyElement->renderer()->style()->visitedDependentColor(CSSPropertyBackgroundColor);

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
    const HashSet<RefPtr<Widget> >* viewChildren = children();
    HashSet<RefPtr<Widget> >::const_iterator end = viewChildren->end();
    for (HashSet<RefPtr<Widget> >::const_iterator current = viewChildren->begin(); current != end; ++current) {
        Widget* widget = current->get();
        if (widget->isFrameView()) {
            if (static_cast<FrameView*>(widget)->hasCustomScrollbars())
                return true;
        } else if (widget->isScrollbar()) {
            Scrollbar* scrollbar = static_cast<Scrollbar*>(widget);
            if (scrollbar->isCustomScrollbar())
                return true;
        }
    }

    return false;
}

FrameView* FrameView::parentFrameView() const
{
    if (Widget* parentView = parent()) {
        if (parentView->isFrameView())
            return static_cast<FrameView*>(parentView);
    }
    return 0;
}

bool FrameView::isInChildFrameWithFrameFlattening()
{
    if (!parent() || !m_frame->ownerElement() || !m_frame->settings() || !m_frame->settings()->frameFlatteningEnabled())
        return false;

    // Frame flattening applies when the owner element is either in a frameset or
    // an iframe with flattening parameters.
    if (m_frame->ownerElement()->hasTagName(iframeTag)) {
        RenderIFrame* iframeRenderer = toRenderIFrame(m_frame->ownerElement()->renderPart());

        if (iframeRenderer->flattenFrame())
            return true;

    } else if (m_frame->ownerElement()->hasTagName(frameTag))
        return true;

    return false;
}

bool FrameView::doLayoutWithFrameFlattening(bool allowSubtree)
{
    // Try initiating layout from the topmost parent.
    FrameView* parentView = parentFrameView();

    if (!parentView)
        return false;

    // In the middle of parent layout, no need to restart from topmost.
    if (parentView->m_nestedLayoutCount)
        return false;

    // Parent tree is clean. Starting layout from it would have no effect.
    if (!parentView->needsLayout())
        return false;

    while (parentView->parentFrameView())
        parentView = parentView->parentFrameView();

    parentView->layout(allowSubtree);

    RenderObject* root = m_layoutRoot ? m_layoutRoot : m_frame->document()->renderer();
    ASSERT_UNUSED(root, !root->needsLayout());

    return true;
}

void FrameView::updateControlTints()
{
    // This is called when control tints are changed from aqua/graphite to clear and vice versa.
    // We do a "fake" paint, and when the theme gets a paint call, it can then do an invalidate.
    // This is only done if the theme supports control tinting. It's up to the theme and platform
    // to define when controls get the tint and to call this function when that changes.
    
    // Optimize the common case where we bring a window to the front while it's still empty.
    if (!m_frame || m_frame->document()->url().isEmpty())
        return;

    RenderView* root = rootRenderer(this);
    if ((root && root->theme()->supportsControlTints()) || hasCustomScrollbars())
        paintControlTints();
}

void FrameView::paintControlTints()
{
    if (needsLayout())
        layout();
    PlatformGraphicsContext* const noContext = 0;
    GraphicsContext context(noContext);
    context.setUpdatingControlTints(true);
    if (platformWidget())
        paintContents(&context, visibleContentRect());
    else
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
    m_wasScrolledByUser = wasScrolledByUser;
}

void FrameView::paintContents(GraphicsContext* p, const IntRect& rect)
{
    if (!frame())
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willPaint(m_frame.get(), p, rect);

    Document* document = m_frame->document();

#ifndef NDEBUG
    bool fillWithRed;
    if (document->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (m_frame->ownerElement())
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
        p->fillRect(rect, Color(0xFF, 0, 0), ColorSpaceDeviceRGB);
#endif

    Page* page = m_frame->page();
    if (page->mainFrame() == m_frame && page->pagination().mode != Page::Pagination::Unpaginated)
        p->fillRect(rect, baseBackgroundColor(), ColorSpaceDeviceRGB);

    bool isTopLevelPainter = !sCurrentPaintTimeStamp;
    if (isTopLevelPainter)
        sCurrentPaintTimeStamp = currentTime();
    
    RenderView* root = rootRenderer(this);
    if (!root) {
        LOG_ERROR("called FrameView::paint with nil renderer");
        return;
    }

    ASSERT(!needsLayout());
    if (needsLayout())
        return;

    FontCachePurgePreventer fontCachePurgePreventer;

#if USE(ACCELERATED_COMPOSITING)
    if (!p->paintingDisabled() && !document->printing())
        syncCompositingStateForThisFrame(m_frame.get());
#endif

    PaintBehavior oldPaintBehavior = m_paintBehavior;
    
    if (FrameView* parentView = parentFrameView()) {
        if (parentView->paintBehavior() & PaintBehaviorFlattenCompositingLayers)
            m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;
    }
    
    if (m_paintBehavior == PaintBehaviorNormal)
        document->markers()->invalidateRenderedRectsForMarkersInRect(rect);

    if (document->printing())
        m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;

    bool flatteningPaint = m_paintBehavior & PaintBehaviorFlattenCompositingLayers;
    bool isRootFrame = !m_frame->ownerElement();
    if (flatteningPaint && isRootFrame)
        notifyWidgetsInAllFrames(WillPaintFlattened);

    ASSERT(!m_isPainting);
    m_isPainting = true;

    // m_nodeToDraw is used to draw only one element (and its descendants)
    RenderObject* eltRenderer = m_nodeToDraw ? m_nodeToDraw->renderer() : 0;
    RenderLayer* rootLayer = root->layer();

    rootLayer->paint(p, rect, m_paintBehavior, eltRenderer);

    if (rootLayer->containsDirtyOverlayScrollbars())
        rootLayer->paintOverlayScrollbars(p, rect, m_paintBehavior, eltRenderer);

    m_isPainting = false;

    if (flatteningPaint && isRootFrame)
        notifyWidgetsInAllFrames(DidPaintFlattened);

    m_paintBehavior = oldPaintBehavior;
    m_lastPaintTime = currentTime();

#if ENABLE(DASHBOARD_SUPPORT)
    // Regions may have changed as a result of the visibility/z-index of element changing.
    if (document->dashboardRegionsDirty())
        updateDashboardRegions();
#endif

    if (isTopLevelPainter)
        sCurrentPaintTimeStamp = 0;

    InspectorInstrumentation::didPaint(cookie);
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

void FrameView::setNodeToDraw(Node* node)
{
    m_nodeToDraw = node;
}

void FrameView::paintContentsForSnapshot(GraphicsContext* context, const IntRect& imageRect, SelectionInSnaphot shouldPaintSelection)
{
    updateLayoutAndStyleIfNeededRecursive();

    // Cache paint behavior and set a new behavior appropriate for snapshots.
    PaintBehavior oldBehavior = paintBehavior();
    setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);

    // If the snapshot should exclude selection, then we'll clear the current selection
    // in the render tree only. This will allow us to restore the selection from the DOM
    // after we paint the snapshot.
    if (shouldPaintSelection == ExcludeSelection) {
        for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
            if (RenderView* root = frame->contentRenderer())
                root->clearSelection();
        }
    }

    paintContents(context, imageRect);

    // Restore selection.
    if (shouldPaintSelection == ExcludeSelection) {
        for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get()))
            frame->selection()->updateAppearance();
    }

    // Restore cached paint behavior.
    setPaintBehavior(oldBehavior);
}

void FrameView::paintOverhangAreas(GraphicsContext* context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    if (context->paintingDisabled())
        return;

    if (m_frame->document()->printing())
        return;

    Page* page = m_frame->page();
    if (page->mainFrame() == m_frame) {
        if (page->chrome()->client()->paintCustomOverhangArea(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect))
            return;
    }

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

    m_frame->document()->updateStyleIfNeeded();

    if (needsLayout())
        layout();

    // Grab a copy of the children() set, as it may be mutated by the following updateLayoutAndStyleIfNeededRecursive
    // calls, as they can potentially re-enter a layout of the parent frame view, which may add/remove scrollbars
    // and thus mutates the children() set.
    Vector<RefPtr<FrameView> > frameViews;
    collectFrameViewChildren(this, frameViews);

    const Vector<RefPtr<FrameView> >::iterator end = frameViews.end();
    for (Vector<RefPtr<FrameView> >::iterator it = frameViews.begin(); it != end; ++it)
        (*it)->updateLayoutAndStyleIfNeededRecursive();

    // updateLayoutAndStyleIfNeededRecursive is called when we need to make sure style and layout are up-to-date before
    // painting, so we need to flush out any deferred repaints too.
    flushDeferredRepaints();

    // When frame flattening is on, child frame can mark parent frame dirty. In such case, child frame
    // needs to call layout on parent frame recursively.
    // This assert ensures that parent frames are clean, when child frames finished updating layout and style.
    ASSERT(!needsLayout());
}
    
void FrameView::flushDeferredRepaints()
{
    if (!m_deferredRepaintTimer.isActive())
        return;
    m_deferredRepaintTimer.stop();
    doDeferredRepaints();
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
    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    if (RenderView* root = rootRenderer(this)) {
        float pageLogicalWidth = root->style()->isHorizontalWritingMode() ? pageSize.width() : pageSize.height();
        float pageLogicalHeight = root->style()->isHorizontalWritingMode() ? pageSize.height() : pageSize.width();

        LayoutUnit flooredPageLogicalWidth = static_cast<LayoutUnit>(pageLogicalWidth);
        LayoutUnit flooredPageLogicalHeight = static_cast<LayoutUnit>(pageLogicalHeight);
        root->setLogicalWidth(flooredPageLogicalWidth);
        root->setPageLogicalHeight(flooredPageLogicalHeight);
        root->setNeedsLayoutAndPrefWidthsRecalc();
        forceLayout();

        // If we don't fit in the given page width, we'll lay out again. If we don't fit in the
        // page width when shrunk, we will lay out at maximum shrink and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        bool horizontalWritingMode = root->style()->isHorizontalWritingMode();
        const LayoutRect& documentRect = root->documentRect();
        LayoutUnit docLogicalWidth = horizontalWritingMode ? documentRect.width() : documentRect.height();
        if (docLogicalWidth > pageLogicalWidth) {
            int expectedPageWidth = std::min<float>(documentRect.width(), pageSize.width() * maximumShrinkFactor);
            int expectedPageHeight = std::min<float>(documentRect.height(), pageSize.height() * maximumShrinkFactor);
            FloatSize maxPageSize = m_frame->resizePageRectsKeepingRatio(FloatSize(originalPageSize.width(), originalPageSize.height()), FloatSize(expectedPageWidth, expectedPageHeight));
            pageLogicalWidth = horizontalWritingMode ? maxPageSize.width() : maxPageSize.height();
            pageLogicalHeight = horizontalWritingMode ? maxPageSize.height() : maxPageSize.width();

            flooredPageLogicalWidth = static_cast<LayoutUnit>(pageLogicalWidth);
            flooredPageLogicalHeight = static_cast<LayoutUnit>(pageLogicalHeight);
            root->setLogicalWidth(flooredPageLogicalWidth);
            root->setPageLogicalHeight(flooredPageLogicalHeight);
            root->setNeedsLayoutAndPrefWidthsRecalc();
            forceLayout();

            const LayoutRect& updatedDocumentRect = root->documentRect();
            LayoutUnit docLogicalHeight = horizontalWritingMode ? updatedDocumentRect.height() : updatedDocumentRect.width();
            LayoutUnit docLogicalTop = horizontalWritingMode ? updatedDocumentRect.y() : updatedDocumentRect.x();
            LayoutUnit docLogicalRight = horizontalWritingMode ? updatedDocumentRect.maxX() : updatedDocumentRect.maxY();
            LayoutUnit clippedLogicalLeft = 0;
            if (!root->style()->isLeftToRightDirection())
                clippedLogicalLeft = docLogicalRight - pageLogicalWidth;
            LayoutRect overflow(clippedLogicalLeft, docLogicalTop, pageLogicalWidth, docLogicalHeight);

            if (!horizontalWritingMode)
                overflow = overflow.transposedRect();
            root->clearLayoutOverflow();
            root->addLayoutOverflow(overflow); // This is how we clip in case we overflow again.
        }
    }

    if (shouldAdjustViewSize)
        adjustViewSize();
}

void FrameView::adjustPageHeightDeprecated(float *newBottom, float oldTop, float oldBottom, float /*bottomLimit*/)
{
    if (RenderView* root = rootRenderer(this)) {
        // Use a context with painting disabled.
        GraphicsContext context((PlatformGraphicsContext*)0);
        root->setTruncatedAt((int)floorf(oldBottom));
        IntRect dirtyRect(0, (int)floorf(oldTop), root->maxXLayoutOverflow(), (int)ceilf(oldBottom - oldTop));
        root->setPrintRect(dirtyRect);
        root->layer()->paint(&context, dirtyRect);
        *newBottom = root->bestTruncatedAt();
        if (*newBottom == 0)
            *newBottom = oldBottom;
        root->setPrintRect(IntRect());
    } else
        *newBottom = oldBottom;
}

IntRect FrameView::convertFromRenderer(const RenderObject* renderer, const IntRect& rendererRect) const
{
    IntRect rect = renderer->localToAbsoluteQuad(FloatRect(rendererRect)).enclosingBoundingBox();

    // Convert from page ("absolute") to FrameView coordinates.
    if (!delegatesScrolling())
        rect.moveBy(-scrollPosition());

    return rect;
}

IntRect FrameView::convertToRenderer(const RenderObject* renderer, const IntRect& viewRect) const
{
    IntRect rect = viewRect;
    
    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrolling())
        rect.moveBy(scrollPosition());

    // FIXME: we don't have a way to map an absolute rect down to a local quad, so just
    // move the rect for now.
    rect.setLocation(roundedIntPoint(renderer->absoluteToLocal(rect.location(), false, true /* use transforms */)));
    return rect;
}

IntPoint FrameView::convertFromRenderer(const RenderObject* renderer, const IntPoint& rendererPoint) const
{
    IntPoint point = roundedIntPoint(renderer->localToAbsolute(rendererPoint, false, true /* use transforms */));

    // Convert from page ("absolute") to FrameView coordinates.
    if (!delegatesScrolling())
        point.moveBy(-scrollPosition());
    return point;
}

IntPoint FrameView::convertToRenderer(const RenderObject* renderer, const IntPoint& viewPoint) const
{
    IntPoint point = viewPoint;

    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrolling())
        point += IntSize(scrollX(), scrollY());

    return roundedIntPoint(renderer->absoluteToLocal(point, false, true /* use transforms */));
}

IntRect FrameView::convertToContainingView(const IntRect& localRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = static_cast<const FrameView*>(parentScrollView);
            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return localRect;
                
            IntRect rect(localRect);
            // Add borders and padding??
            rect.move(renderer->borderLeft() + renderer->paddingLeft(),
                      renderer->borderTop() + renderer->paddingTop());
            return parentView->convertFromRenderer(renderer, rect);
        }
        
        return Widget::convertToContainingView(localRect);
    }
    
    return localRect;
}

IntRect FrameView::convertFromContainingView(const IntRect& parentRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = static_cast<const FrameView*>(parentScrollView);

            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return parentRect;

            IntRect rect = parentView->convertToRenderer(renderer, parentRect);
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
            const FrameView* parentView = static_cast<const FrameView*>(parentScrollView);

            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return localPoint;
                
            IntPoint point(localPoint);

            // Add borders and padding
            point.move(renderer->borderLeft() + renderer->paddingLeft(),
                       renderer->borderTop() + renderer->paddingTop());
            return parentView->convertFromRenderer(renderer, point);
        }
        
        return Widget::convertToContainingView(localPoint);
    }
    
    return localPoint;
}

IntPoint FrameView::convertFromContainingView(const IntPoint& parentPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = static_cast<const FrameView*>(parentScrollView);

            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return parentPoint;

            IntPoint point = parentView->convertToRenderer(renderer, parentPoint);
            // Subtract borders and padding
            point.move(-renderer->borderLeft() - renderer->paddingLeft(),
                       -renderer->borderTop() - renderer->paddingTop());
            return point;
        }
        
        return Widget::convertFromContainingView(parentPoint);
    }
    
    return parentPoint;
}

// Normal delay
void FrameView::setRepaintThrottlingDeferredRepaintDelay(double p)
{
    s_deferredRepaintDelay = p;
}

// Negative value would mean that first few repaints happen without a delay
void FrameView::setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(double p)
{
    s_initialDeferredRepaintDelayDuringLoading = p;
}

// The delay grows on each repaint to this maximum value
void FrameView::setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(double p)
{
    s_maxDeferredRepaintDelayDuringLoading = p;
}

// On each repaint the delay increases by this amount
void FrameView::setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(double p)
{
    s_deferredRepaintDelayIncrementDuringLoading = p;
}

void FrameView::setTracksRepaints(bool trackRepaints)
{
    if (trackRepaints == m_isTrackingRepaints)
        return;
    
    m_trackedRepaintRects.clear();
    m_isTrackingRepaints = trackRepaints;
}

void FrameView::addScrollableArea(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreas)
        m_scrollableAreas = adoptPtr(new ScrollableAreaSet);
    m_scrollableAreas->add(scrollableArea);
}

void FrameView::removeScrollableArea(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreas)
        return;
    m_scrollableAreas->remove(scrollableArea);
}

bool FrameView::containsScrollableArea(ScrollableArea* scrollableArea) const
{
    if (!m_scrollableAreas)
        return false;
    return m_scrollableAreas->contains(scrollableArea);
}

void FrameView::removeChild(Widget* widget)
{
    if (widget->isFrameView())
        removeScrollableArea(static_cast<FrameView*>(widget));

    ScrollView::removeChild(widget);
}

bool FrameView::wheelEvent(const PlatformWheelEvent& wheelEvent)
{
    // We don't allow mouse wheeling to happen in a ScrollView that has had its scrollbars explicitly disabled.
    if (!canHaveScrollbars())
        return false;

#if !PLATFORM(WX)
    if (platformWidget())
        return false;
#endif

#if ENABLE(THREADED_SCROLLING)
    if (Page* page = m_frame->page()) {
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
    RenderView* root = rootRenderer(this);
    if (!root)
        return true;

    return root->style()->isHorizontalWritingMode();
}

bool FrameView::isFlippedDocument() const
{
    RenderView* root = rootRenderer(this);
    if (!root)
        return false;

    return root->style()->isFlippedBlocksWritingMode();
}

void FrameView::notifyWidgetsInAllFrames(WidgetNotification notification)
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
        if (RenderView* root = frame->contentRenderer())
            root->notifyWidgets(notification);
    }
}
    
AXObjectCache* FrameView::axObjectCache() const
{
    if (frame() && frame()->document() && frame()->document()->axObjectCacheExists())
        return frame()->document()->axObjectCache();
    return 0;
}
    
} // namespace WebCore
