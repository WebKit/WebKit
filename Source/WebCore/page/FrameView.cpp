/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ColorBlending.h"
#include "DOMWindow.h"
#include "DebugPageOverlays.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentSVG.h"
#include "Editor.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FragmentDirectiveParser.h"
#include "FragmentDirectiveRangeFinder.h"
#include "Frame.h"
#include "FrameFlattening.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLBodyElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPlugInImageElement.h"
#include "HighlightRegister.h"
#include "ImageDocument.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "LegacyRenderSVGRoot.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "ModalContainerObserver.h"
#include "NullGraphicsContext.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "PerformanceLoggingClient.h"
#include "ProgressTracker.h"
#include "Quirks.h"
#include "RenderEmbeddedObject.h"
#include "RenderFullScreen.h"
#include "RenderIFrame.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderSVGRoot.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResizeObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocument.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "ScriptRunner.h"
#include "ScriptedAnimationController.h"
#include "ScrollAnimator.h"
#include "ScrollSnapOffsetsInfo.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "TextResourceDecoder.h"
#include "TiledBacking.h"
#include "VelocityData.h"
#include "VisualViewport.h"
#include "WheelEventTestMonitor.h"
#include <wtf/HexNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/TextStream.h>

#if USE(COORDINATED_GRAPHICS)
#include "TiledBackingStore.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "DocumentLoader.h"
#include "LegacyTileCache.h"
#endif

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
#include "DisplayView.h"
#include "LayoutContext.h"
#endif

#define PAGE_ID valueOrDefault(frame().pageID()).toUInt64()
#define FRAME_ID valueOrDefault(frame().frameID()).toUInt64()
#define FRAMEVIEW_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", main=%d] FrameView::" fmt, this, PAGE_ID, FRAME_ID, frame().isMainFrame(), ##__VA_ARGS__)

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(FrameView);

MonotonicTime FrameView::sCurrentPaintTimeStamp { };

// The maximum number of updateEmbeddedObjects iterations that should be done before returning.
static const unsigned maxUpdateEmbeddedObjectsIterations = 2;

static constexpr unsigned defaultSignificantRenderedTextCharacterThreshold = 3000;
static constexpr float defaultSignificantRenderedTextMeanLength = 50;
static constexpr unsigned mainArticleSignificantRenderedTextCharacterThreshold = 1500;
static constexpr float mainArticleSignificantRenderedTextMeanLength = 25;

Pagination::Mode paginationModeForRenderStyle(const RenderStyle& style)
{
    Overflow overflow = style.overflowY();
    if (overflow != Overflow::PagedX && overflow != Overflow::PagedY)
        return Pagination::Unpaginated;

    bool isHorizontalWritingMode = style.isHorizontalWritingMode();
    TextDirection textDirection = style.direction();
    WritingMode writingMode = style.writingMode();

    // paged-x always corresponds to LeftToRightPaginated or RightToLeftPaginated. If the WritingMode
    // is horizontal, then we use TextDirection to choose between those options. If the WritingMode
    // is vertical, then the direction of the verticality dictates the choice.
    if (overflow == Overflow::PagedX) {
        if ((isHorizontalWritingMode && textDirection == TextDirection::LTR) || writingMode == WritingMode::LeftToRight)
            return Pagination::LeftToRightPaginated;
        return Pagination::RightToLeftPaginated;
    }

    // paged-y always corresponds to TopToBottomPaginated or BottomToTopPaginated. If the WritingMode
    // is horizontal, then the direction of the horizontality dictates the choice. If the WritingMode
    // is vertical, then we use TextDirection to choose between those options. 
    if (writingMode == WritingMode::TopToBottom || (!isHorizontalWritingMode && textDirection == TextDirection::RTL))
        return Pagination::TopToBottomPaginated;
    return Pagination::BottomToTopPaginated;
}

FrameView::FrameView(Frame& frame)
    : m_frame(frame)
    , m_layoutContext(*this)
    , m_updateEmbeddedObjectsTimer(*this, &FrameView::updateEmbeddedObjectsTimerFired)
    , m_updateWidgetPositionsTimer(*this, &FrameView::updateWidgetPositionsTimerFired)
    , m_delayedScrollEventTimer(*this, &FrameView::scheduleScrollEvent)
    , m_delayedScrollToFocusedElementTimer(*this, &FrameView::scrollToFocusedElementTimerFired)
    , m_speculativeTilingEnableTimer(*this, &FrameView::speculativeTilingEnableTimerFired)
{
    init();

#if HAVE(RUBBER_BANDING)
    auto verticalElasticity = ScrollElasticity::None;
    auto horizontalElasticity = ScrollElasticity::None;
    if (m_frame->isMainFrame()) {
        verticalElasticity = m_frame->page() ? m_frame->page()->verticalScrollElasticity() : ScrollElasticity::Allowed;
        horizontalElasticity = m_frame->page() ? m_frame->page()->horizontalScrollElasticity() : ScrollElasticity::Allowed;
    } else if (m_frame->settings().rubberBandingForSubScrollableRegionsEnabled()) {
        verticalElasticity = ScrollElasticity::Automatic;
        horizontalElasticity = ScrollElasticity::Automatic;
    }

    ScrollableArea::setVerticalScrollElasticity(verticalElasticity);
    ScrollableArea::setHorizontalScrollElasticity(horizontalElasticity);
#endif
}

Ref<FrameView> FrameView::create(Frame& frame)
{
    Ref<FrameView> view = adoptRef(*new FrameView(frame));
    if (frame.page() && frame.page()->isVisible())
        view->show();
    return view;
}

Ref<FrameView> FrameView::create(Frame& frame, const IntSize& initialSize)
{
    Ref<FrameView> view = adoptRef(*new FrameView(frame));
    view->Widget::setFrameRect(IntRect(view->location(), initialSize));
    if (frame.page() && frame.page()->isVisible())
        view->show();
    return view;
}

FrameView::~FrameView()
{
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
    m_updateEmbeddedObjectsTimer.stop();
    m_wasScrolledByUser = false;
    m_delayedScrollEventTimer.stop();
    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();
    m_lastViewportSize = IntSize();
    m_lastZoomFactor = 1.0f;
    m_isTrackingRepaints = false;
    m_trackedRepaintRects.clear();
    m_lastPaintTime = MonotonicTime();
    m_paintBehavior = PaintBehavior::Normal;
    m_isPainting = false;
    m_needsDeferredScrollbarsUpdate = false;
    m_needsDeferredPositionScrollbarLayers = false;
    m_maintainScrollPositionAnchor = nullptr;
    resetLayoutMilestones();
    layoutContext().reset();
}

void FrameView::resetLayoutMilestones()
{
    m_firstLayoutCallbackPending = false;
    m_firstVisuallyNonEmptyLayoutMilestoneIsPending = true;
    m_contentQualifiesAsVisuallyNonEmpty = false;
    m_hasReachedSignificantRenderedTextThreshold = false;
    m_renderedSignificantAmountOfText = false;
    m_visuallyNonEmptyCharacterCount = 0;
    m_visuallyNonEmptyPixelCount = 0;
    m_textRendererCountForVisuallyNonEmptyCharacters = 0;
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
    // FIXME: Do we really need this?
    layoutContext().resetFirstLayoutFlag();
    // Reset the document's scrollbars back to our defaults before we yield the floor.
    setScrollbarsSuppressed(true);
    if (m_canHaveScrollbars)
        setScrollbarModes(ScrollbarMode::Auto, ScrollbarMode::Auto);
    else
        setScrollbarModes(ScrollbarMode::AlwaysOff, ScrollbarMode::AlwaysOff);
    setScrollbarsSuppressed(false);
}

void FrameView::resetScrollbarsAndClearContentsSize()
{
    resetScrollbars();

    LOG(Layout, "FrameView %p resetScrollbarsAndClearContentsSize", this);

    setScrollbarsSuppressed(true);
    setContentsSize(IntSize());
    setScrollbarsSuppressed(false);
}

void FrameView::init()
{
    reset();

    m_size = LayoutSize();

    // Propagate the scrolling mode to the view.
    auto* ownerElement = frame().ownerElement();
    if (is<HTMLFrameElementBase>(ownerElement) && downcast<HTMLFrameElementBase>(*ownerElement).scrollingMode() == ScrollbarMode::AlwaysOff)
        setCanHaveScrollbars(false);

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

    if (auto scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->willDestroyScrollableArea(*this);
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

void FrameView::willBeDestroyed()
{
    setHasHorizontalScrollbar(false);
    setHasVerticalScrollbar(false);
}

void FrameView::recalculateScrollbarOverlayStyle()
{
    auto style = [this] {
        if (auto page = frame().page()) {
            if (auto clientStyle = page->chrome().client().preferredScrollbarOverlayStyle())
                return *clientStyle;
        }
        auto background = documentBackgroundColor();
        if (background.isVisible()) {
            if (background.lightness() <= .5f)
                return ScrollbarOverlayStyleLight;
        } else {
            if (useDarkAppearance())
                return ScrollbarOverlayStyleLight;
        }
        return ScrollbarOverlayStyleDefault;
    }();
    if (scrollbarOverlayStyle() != style)
        setScrollbarOverlayStyle(style);
}

#if ENABLE(DARK_MODE_CSS)

void FrameView::recalculateBaseBackgroundColor()
{
    auto styleColorOptions = this->styleColorOptions();
    if (m_styleColorOptions == styleColorOptions)
        return;

    m_styleColorOptions = styleColorOptions;
    std::optional<Color> backgroundColor;
    if (m_isTransparent)
        backgroundColor = Color(Color::transparentBlack);
    updateBackgroundRecursively(backgroundColor);
}

#endif

void FrameView::clear()
{
    setCanBlitOnScroll(true);
    
    reset();

    setScrollbarsSuppressed(true);

#if PLATFORM(IOS_FAMILY)
    // To avoid flashes of white, disable tile updates immediately when view is cleared at the beginning of a page load.
    // Tiling will be re-enabled from UIKit via [WAKWindow setTilingMode:] when we have content to draw.
    if (LegacyTileCache* tileCache = legacyTileCache())
        tileCache->setTilingMode(LegacyTileCache::Disabled);
#endif
}

#if PLATFORM(IOS_FAMILY)
void FrameView::didReplaceMultipartContent()
{
    // Re-enable tile updates that were disabled in clear().
    if (LegacyTileCache* tileCache = legacyTileCache())
        tileCache->setTilingMode(LegacyTileCache::Normal);
}
#endif

bool FrameView::didFirstLayout() const
{
    return layoutContext().didFirstLayout();
}

void FrameView::invalidateRect(const IntRect& rect)
{
    if (!parent()) {
        if (auto* page = frame().page())
            page->chrome().invalidateContentsAndRootView(rect);
        return;
    }

    auto* renderer = frame().ownerRenderer();
    if (!renderer)
        return;

    IntRect repaintRect = rect;
    repaintRect.moveBy(roundedIntPoint(renderer->contentBoxLocation()));
    renderer->repaintRectangle(repaintRect);
}

void FrameView::setFrameRect(const IntRect& newRect)
{
    Ref<FrameView> protectedThis(*this);
    IntRect oldRect = frameRect();
    if (newRect == oldRect)
        return;

    // Every scroll that happens as the result of frame size change is programmatic.
    auto oldScrollType = currentScrollType();
    setCurrentScrollType(ScrollType::Programmatic);

    ScrollView::setFrameRect(newRect);

    updateScrollableAreaSet();

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidChangeSize();
    }

    if (frame().isMainFrame() && frame().page())
        frame().page()->pageOverlayController().didChangeViewSize();

    viewportContentsChanged();
    setCurrentScrollType(oldScrollType);
}

FrameFlattening FrameView::effectiveFrameFlattening() const
{
#if PLATFORM(IOS_FAMILY)
    // On iOS when async frame scrolling is enabled, it does not make sense to use full frame flattening.
    // In that case, we just consider that frame flattening is disabled. This allows people to test
    // frame scrolling on iOS by enabling "Async Frame Scrolling" via the Safari menu.
    if (frame().settings().asyncFrameScrollingEnabled() && frame().settings().frameFlattening() == FrameFlattening::FullyEnabled)
        return FrameFlattening::Disabled;
#endif
    return frame().settings().frameFlattening();
}

bool FrameView::frameFlatteningEnabled() const
{
    return effectiveFrameFlattening() != FrameFlattening::Disabled;
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
    if (hMode == ScrollbarMode::AlwaysOff && vMode == ScrollbarMode::AlwaysOff)
        setCanHaveScrollbars(false);
    else
        setCanHaveScrollbars(true);
}

RefPtr<Element> FrameView::rootElementForCustomScrollbarPartStyle(PseudoId partPseudoId) const
{
    // FIXME: We need to update the scrollbar dynamically as documents change (or as doc elements and bodies get discovered that have custom styles).
    auto* document = frame().document();
    if (!document)
        return nullptr;

    // Try the <body> element first as a scrollbar source.
    auto* body = document->bodyOrFrameset();
    if (body && body->renderer() && body->renderer()->style().hasPseudoStyle(partPseudoId))
        return body;
    
    // If the <body> didn't have a custom style, then the root element might.
    auto* docElement = document->documentElement();
    if (docElement && docElement->renderer() && docElement->renderer()->style().hasPseudoStyle(partPseudoId))
        return docElement;

    return nullptr;
}

Ref<Scrollbar> FrameView::createScrollbar(ScrollbarOrientation orientation)
{
    if (auto element = rootElementForCustomScrollbarPartStyle(PseudoId::Scrollbar))
        return RenderScrollbar::createCustomScrollbar(*this, orientation, element.get());
    
    // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
    // FIXME: Seems bad to do this for cross-origin frames.
    RenderWidget* frameRenderer = frame().ownerRenderer();
    if (frameRenderer && frameRenderer->style().hasPseudoStyle(PseudoId::Scrollbar))
        return RenderScrollbar::createCustomScrollbar(*this, orientation, nullptr, &frame());

    // Nobody set a custom style, so we just use a native scrollbar.
    return ScrollView::createScrollbar(orientation);
}

void FrameView::didRestoreFromBackForwardCache()
{
    // When restoring from back/forward cache, the main frame stays in place while subframes get swapped in.
    // We update the scrollable area set to ensure that scrolling data structures get invalidated.
    updateScrollableAreaSet();
}

void FrameView::willDestroyRenderTree()
{
    detachCustomScrollbars();
    layoutContext().clearSubtreeLayoutRoot();
}

void FrameView::didDestroyRenderTree()
{
    ASSERT(!layoutContext().subtreeLayoutRoot());
    ASSERT(m_widgetsInRenderTree.isEmpty());

    // If the render tree is destroyed below FrameView::updateEmbeddedObjects(), there will still be a null sentinel in the set.
    // Everything else should have removed itself as the tree was felled.
    ASSERT(!m_embeddedObjectsToUpdate || m_embeddedObjectsToUpdate->isEmpty() || (m_embeddedObjectsToUpdate->size() == 1 && m_embeddedObjectsToUpdate->first() == nullptr));

    ASSERT(!m_viewportConstrainedObjects || m_viewportConstrainedObjects->computesEmpty());
    ASSERT(!m_slowRepaintObjects || m_slowRepaintObjects->computesEmpty());
}

void FrameView::setContentsSize(const IntSize& size)
{
    if (size == contentsSize())
        return;

    layoutContext().disableSetNeedsLayout();

    ScrollView::setContentsSize(size);
    contentsResized();
    
    Page* page = frame().page();
    if (!page)
        return;

    updateScrollableAreaSet();

    page->chrome().contentsSizeChanged(frame(), size); // Notify only.

    if (frame().isMainFrame()) {
        page->pageOverlayController().didChangeDocumentSize();
        BackForwardCache::singleton().markPagesForContentsSizeChanged(*page);
    }
    layoutContext().enableSetNeedsLayout();
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

    Overflow overflowX = renderer.effectiveOverflowX();
    Overflow overflowY = renderer.effectiveOverflowY();

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGRoot>(renderer)) {
        // FIXME: evaluate if we can allow overflow for these cases too.
        // Overflow is always hidden when stand-alone SVG documents are embedded.
        if (downcast<RenderSVGRoot>(renderer).isEmbeddedThroughFrameContainingSVGDocument()) {
            overflowX = Overflow::Hidden;
            overflowY = Overflow::Hidden;
        }
    }
#endif

    if (is<LegacyRenderSVGRoot>(renderer)) {
        // FIXME: evaluate if we can allow overflow for these cases too.
        // Overflow is always hidden when stand-alone SVG documents are embedded.
        if (downcast<LegacyRenderSVGRoot>(renderer).isEmbeddedThroughFrameContainingSVGDocument()) {
            overflowX = Overflow::Hidden;
            overflowY = Overflow::Hidden;
        }
    }

    switch (overflowX) {
    case Overflow::Hidden:
    case Overflow::Clip:
        if (overrideHidden)
            hMode = ScrollbarMode::Auto;
        else
            hMode = ScrollbarMode::AlwaysOff;
        break;
    case Overflow::Scroll:
        hMode = ScrollbarMode::AlwaysOn;
        break;
    case Overflow::Auto:
        hMode = ScrollbarMode::Auto;
        break;
    default:
        // Don't set it at all.
        ;
    }

    switch (overflowY) {
    case Overflow::Hidden:
    case Overflow::Clip:
        if (overrideHidden)
            vMode = ScrollbarMode::Auto;
        else
            vMode = ScrollbarMode::AlwaysOff;
        break;
    case Overflow::Scroll:
        vMode = ScrollbarMode::AlwaysOn;
        break;
    case Overflow::Auto:
        vMode = ScrollbarMode::Auto;
        break;
    default:
        // Don't set it at all. Values of Overflow::PagedX and Overflow::PagedY are handled by applyPaginationToViewPort().
        ;
    }
}

void FrameView::applyPaginationToViewport()
{
    auto* document = frame().document();
    auto* documentElement = document ? document->documentElement() : nullptr;
    if (!documentElement || !documentElement->renderer()) {
        setPagination(Pagination());
        return;
    }

    auto& documentRenderer = *documentElement->renderer();
    auto* documentOrBodyRenderer = &documentRenderer;

    auto* body = document->body();
    if (body && body->renderer()) {
        documentOrBodyRenderer = documentRenderer.effectiveOverflowX() == Overflow::Visible && is<HTMLHtmlElement>(*documentElement) ?
            body->renderer() : &documentRenderer;
    }

    Pagination pagination;
    Overflow overflowY = documentOrBodyRenderer->effectiveOverflowY();
    if (overflowY == Overflow::PagedX || overflowY == Overflow::PagedY) {
        pagination.mode = WebCore::paginationModeForRenderStyle(documentOrBodyRenderer->style());
        GapLength columnGapLength = documentOrBodyRenderer->style().columnGap();
        pagination.gap = 0;
        if (!columnGapLength.isNormal()) {
            if (auto* containerForPaginationGap = is<RenderBox>(documentOrBodyRenderer) ? downcast<RenderBox>(documentOrBodyRenderer) : documentOrBodyRenderer->containingBlock())
                pagination.gap = valueForLength(columnGapLength.length(), containerForPaginationGap->availableLogicalWidth()).toUnsigned();
        }
    }
    setPagination(pagination);
}

void FrameView::calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy strategy)
{
    m_viewportRendererType = ViewportRendererType::None;

    const HTMLFrameOwnerElement* owner = frame().ownerElement();
    if (owner && (owner->scrollingMode() == ScrollbarMode::AlwaysOff)) {
        hMode = ScrollbarMode::AlwaysOff;
        vMode = ScrollbarMode::AlwaysOff;
        return;
    }  
    
    if (m_canHaveScrollbars || strategy == RulesFromWebContentOnly) {
        hMode = ScrollbarMode::Auto;
        vMode = ScrollbarMode::Auto;
    } else {
        hMode = ScrollbarMode::AlwaysOff;
        vMode = ScrollbarMode::AlwaysOff;
    }
    
    if (layoutContext().subtreeLayoutRoot())
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
        vMode = ScrollbarMode::AlwaysOff;
        hMode = ScrollbarMode::AlwaysOff;
        return;
    }

    if (is<HTMLBodyElement>(*bodyOrFrameset) && rootRenderer) {
        // It's sufficient to just check the X overflow,
        // since it's illegal to have visible in only one direction.
        if (rootRenderer->effectiveOverflowX() == Overflow::Visible && is<HTMLHtmlElement>(documentElement)) {
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

void FrameView::styleAndRenderTreeDidChange()
{
    ScrollView::styleAndRenderTreeDidChange();
    auto* renderView = this->renderView();
    if (!renderView)
        return;

    checkAndDispatchDidReachVisuallyNonEmptyState();
    auto* layerTreeMutationRoot = renderView->takeStyleChangeLayerTreeMutationRoot();
    if (layerTreeMutationRoot && !needsLayout())
        layerTreeMutationRoot->updateLayerPositionsAfterStyleChange();
}

bool FrameView::updateCompositingLayersAfterStyleChange()
{
    // If we expect to update compositing after an incipient layout, don't do so here.
    if (!renderView() || needsLayout() || layoutContext().isInLayout())
        return false;
    return renderView()->compositor().didRecalcStyleWithNoPendingLayout();
}

void FrameView::updateCompositingLayersAfterLayout()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    renderView->compositor().updateCompositingLayers(CompositingUpdateType::AfterLayout);
}

void FrameView::invalidateScrollbarsForAllScrollableAreas()
{
    if (!m_scrollableAreas)
        return;

    for (auto* scrollableArea : *m_scrollableAreas)
        scrollableArea->invalidateScrollbars();
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

    return backing->tiledBacking();
}

ScrollingNodeID FrameView::scrollingNodeID() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return 0;

    return backing->scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling);
}

ScrollableArea* FrameView::scrollableAreaForScrollingNodeID(ScrollingNodeID nodeID) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    return renderView->compositor().scrollableAreaForScrollingNodeID(nodeID);
}

#if HAVE(RUBBER_BANDING)
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

#endif // HAVE(RUBBER_BANDING)

void FrameView::updateSnapOffsets()
{
    if (!frame().document())
        return;

    auto& document = *frame().document();
    auto* documentElement = document.documentElement();
    RenderBox* bodyRenderer = document.bodyOrFrameset() ? document.bodyOrFrameset()->renderBox() : nullptr;
    RenderBox* rootRenderer = documentElement ? documentElement->renderBox() : nullptr;
    auto rendererSyleHasScrollSnap = [](const RenderObject* renderer) {
        return renderer && renderer->style().scrollSnapType().strictness != ScrollSnapStrictness::None;
    };

    const RenderStyle* styleToUse = nullptr;
    if (rendererSyleHasScrollSnap(bodyRenderer)) {
        //  The specification doesn't allow setting scroll-snap-type on the body, but
        //  we do this to ensure backwards compatibility with an earlier version of the
        //  specification: See webkit.org/b/200643.
        styleToUse = &bodyRenderer->style();
    } else if (rendererSyleHasScrollSnap(rootRenderer))
        styleToUse = &rootRenderer->style();

    if (!styleToUse || !documentElement) {
        clearSnapOffsets();
        return;
    }

    // updateSnapOffsetsForScrollableArea calculates scroll offsets with all rectangles having their origin at the
    // padding box rectangle of the scrollable element. Unlike for overflow:scroll, the FrameView viewport includes
    // the root element margins. This means that we need to offset the viewport rectangle to make it relative to
    // the padding box of the root element.
    LayoutRect viewport = LayoutRect(IntPoint(), baseLayoutViewportSize());
    viewport.move(-rootRenderer->marginLeft(), -rootRenderer->marginTop());

    updateSnapOffsetsForScrollableArea(*this, *rootRenderer, *styleToUse, viewport, rootRenderer->style().writingMode(), rootRenderer->style().direction());
}

bool FrameView::isScrollSnapInProgress() const
{
    if (scrollbarsSuppressed())
        return false;

    // If the scrolling thread updates the scroll position for this FrameView, then we should return
    // ScrollingCoordinator::isScrollSnapInProgress().
    if (auto scrollingCoordinator = this->scrollingCoordinator()) {
        if (scrollingCoordinator->isScrollSnapInProgress(scrollingNodeID()))
            return true;
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

bool FrameView::flushCompositingStateForThisFrame(const Frame& rootFrameForFlush)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextEnabled()) {
        if (auto* view = existingDisplayView())
            view->flushLayers();
        return true;
    }
#endif

    RenderView* renderView = this->renderView();
    if (!renderView)
        return true; // We don't want to keep trying to update layers if we have no renderer.

    ASSERT(frame().view() == this);

    // If we sync compositing layers when a layout is pending, we may cause painting of compositing
    // layer content to occur before layout has happened, which will cause paintContents() to bail.
    if (needsLayout())
        return false;

#if PLATFORM(IOS_FAMILY)
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
    const Widget* foundWidget = nullptr;
    for (auto& widget : children()) {
        if (widget->platformWidget() != platformWidget)
            continue;
        foundWidget = widget.ptr();
        break;
    }

    if (!foundWidget)
        return nullptr;

    auto* renderWidget = RenderWidget::find(*foundWidget);
    if (!renderWidget)
        return nullptr;

    auto* widgetLayer = renderWidget->layer();
    if (!widgetLayer || !widgetLayer->isComposited())
        return nullptr;

    return widgetLayer->backing()->parentForSublayers();
}

LayoutRect FrameView::fixedScrollableAreaBoundsInflatedForScrolling(const LayoutRect& uninflatedBounds) const
{
    LayoutPoint scrollPosition;
    LayoutSize topLeftExpansion;
    LayoutSize bottomRightExpansion;

    if (frame().settings().visualViewportEnabled()) {
        // FIXME: this is wrong under zooming; uninflatedBounds is scaled but the scroll positions are not.
        scrollPosition = layoutViewportRect().location();
        topLeftExpansion = scrollPosition - unscaledMinimumScrollPosition();
        bottomRightExpansion = unscaledMaximumScrollPosition() - scrollPosition;
    } else {
        scrollPosition = scrollPositionRespectingCustomFixedPosition();
        topLeftExpansion = scrollPosition - minimumScrollPosition();
        bottomRightExpansion = maximumScrollPosition() - scrollPosition;
    }

    return LayoutRect(uninflatedBounds.location() - topLeftExpansion, uninflatedBounds.size() + topLeftExpansion + bottomRightExpansion);
}

LayoutPoint FrameView::scrollPositionRespectingCustomFixedPosition() const
{
#if PLATFORM(IOS_FAMILY)
    if (!frame().settings().visualViewportEnabled())
        return useCustomFixedPositionLayoutRect() ? customFixedPositionLayoutRect().location() : scrollPosition();
#endif

    return scrollPositionForFixedPosition();
}

int FrameView::headerHeight() const
{
    if (!frame().isMainFrame())
        return 0;
    Page* page = frame().page();
    return page ? page->headerHeight() : 0;
}

int FrameView::footerHeight() const
{
    if (!frame().isMainFrame())
        return 0;
    Page* page = frame().page();
    return page ? page->footerHeight() : 0;
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
    
    layoutContext().layout();
    // Every scroll that happens as the result of content inset change is programmatic.
    auto oldScrollType = currentScrollType();
    setCurrentScrollType(ScrollType::Programmatic);

    updateScrollbars(scrollPosition());
    if (renderView->usesCompositing())
        renderView->compositor().frameViewDidChangeSize();

    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setTopContentInset(newTopContentInset);

    setCurrentScrollType(oldScrollType);
}

void FrameView::topContentDirectionDidChange()
{
    m_needsDeferredScrollbarsUpdate = true;
    m_needsDeferredPositionScrollbarLayers = true;
}

void FrameView::handleDeferredScrollbarsUpdate()
{
    if (!m_needsDeferredScrollbarsUpdate)
        return;

    m_needsDeferredScrollbarsUpdate = false;
    updateScrollbars(scrollPosition());
}

void FrameView::handleDeferredPositionScrollbarLayers()
{
    if (!m_needsDeferredPositionScrollbarLayers)
        return;

    m_needsDeferredPositionScrollbarLayers = false;
    positionScrollbarLayers();
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* view = existingDisplayView())
        view->setIsInWindow(isInWindow);
#endif
}

void FrameView::forceLayoutParentViewIfNeeded()
{
    RenderWidget* ownerRenderer = frame().ownerRenderer();
    if (!ownerRenderer)
        return;

    RenderBox* contentBox = embeddedContentBox();
    if (!contentBox)
        return;

    auto& svgRoot = downcast<LegacyRenderSVGRoot>(*contentBox);
    if (svgRoot.everHadLayout() && !svgRoot.needsLayout())
        return;

    LOG(Layout, "FrameView %p forceLayoutParentViewIfNeeded scheduling layout on parent FrameView %p", this, &ownerRenderer->view().frameView());

    // If the embedded SVG document appears the first time, the ownerRenderer has already finished
    // layout without knowing about the existence of the embedded SVG document, because RenderReplaced
    // embeddedContentBox() returns nullptr, as long as the embedded document isn't loaded yet. Before
    // bothering to lay out the SVG document, mark the ownerRenderer needing layout and ask its
    // FrameView for a layout. After that the RenderEmbeddedObject (ownerRenderer) carries the
    // correct size, which LegacyRenderSVGRoot::computeReplacedLogicalWidth/Height rely on, when laying
    // out for the first time, or when the LegacyRenderSVGRoot size has changed dynamically (eg. via <script>).

    ownerRenderer->setNeedsLayoutAndPrefWidthsRecalc();
    ownerRenderer->view().frameView().layoutContext().scheduleLayout();
}

void FrameView::markRootOrBodyRendererDirty() const
{
    auto& document = *frame().document();
    RenderBox* rootRenderer = document.documentElement() ? document.documentElement()->renderBox() : nullptr;
    auto* body = document.bodyOrFrameset();
    RenderBox* bodyRenderer = rootRenderer && body ? body->renderBox() : nullptr;
    if (bodyRenderer && bodyRenderer->stretchesToViewport())
        bodyRenderer->setChildNeedsLayout();
    else if (rootRenderer && rootRenderer->stretchesToViewport())
        rootRenderer->setChildNeedsLayout();
}

void FrameView::adjustScrollbarsForLayout(bool isFirstLayout)
{
    ScrollbarMode hMode;
    ScrollbarMode vMode;
    calculateScrollbarModesForLayout(hMode, vMode);
    if (isFirstLayout && !layoutContext().isLayoutNested()) {
        setScrollbarsSuppressed(true);
        // Set the initial vMode to AlwaysOn if we're auto.
        if (vMode == ScrollbarMode::Auto)
            setVerticalScrollbarMode(ScrollbarMode::AlwaysOn); // This causes a vertical scrollbar to appear.
        // Set the initial hMode to AlwaysOff if we're auto.
        if (hMode == ScrollbarMode::Auto)
            setHorizontalScrollbarMode(ScrollbarMode::AlwaysOff); // This causes a horizontal scrollbar to disappear.
        ASSERT(frame().page());
        if (frame().page()->isMonitoringWheelEvents())
            scrollAnimator().setWheelEventTestMonitor(frame().page()->wheelEventTestMonitor());
        setScrollbarModes(hMode, vMode);
        setScrollbarsSuppressed(false, true);
    } else if (hMode != horizontalScrollbarMode() || vMode != verticalScrollbarMode())
        setScrollbarModes(hMode, vMode);
}

void FrameView::willDoLayout(WeakPtr<RenderElement> layoutRoot)
{
    bool subtreeLayout = !is<RenderView>(*layoutRoot);
    if (subtreeLayout)
        return;
    
    if (auto* body = frame().document()->bodyOrFrameset()) {
        if (is<HTMLFrameSetElement>(*body) && !frameFlatteningEnabled() && body->renderer())
            body->renderer()->setChildNeedsLayout();
    }
    auto firstLayout = !layoutContext().didFirstLayout();
    if (firstLayout) {
        m_lastViewportSize = sizeForResizeEvent();
        m_lastZoomFactor = layoutRoot->style().zoom();
        m_firstLayoutCallbackPending = true;
    }
    adjustScrollbarsForLayout(firstLayout);
        
    auto oldSize = m_size;
    LayoutSize newSize = layoutSize();
    if (oldSize != newSize) {
        m_size = newSize;
        LOG(Layout, "  layout size changed from %.3fx%.3f to %.3fx%.3f", oldSize.width().toFloat(), oldSize.height().toFloat(),     newSize.width().toFloat(), newSize.height().toFloat());
        layoutContext().setNeedsFullRepaint();
        if (!firstLayout)
            markRootOrBodyRendererDirty();
    }
    forceLayoutParentViewIfNeeded();
}

void FrameView::didLayout(WeakPtr<RenderElement> layoutRoot)
{
    renderView()->releaseProtectedRenderWidgets();
    auto* layoutRootEnclosingLayer = layoutRoot->enclosingLayer();
    layoutRootEnclosingLayer->updateLayerPositionsAfterLayout(!is<RenderView>(*layoutRoot), layoutContext().needsFullRepaint());

    updateCompositingLayersAfterLayout();

    Ref document = *frame().document();

#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK)
    if (auto* cache = document->existingAXObjectCache())
        cache->postNotification(layoutRoot.get(), AXObjectCache::AXLayoutComplete);
#endif

    frame().invalidateContentEventRegionsIfNeeded(Frame::InvalidateContentEventRegionsReason::Layout);
    document->invalidateRenderingDependentRegions();

    updateCanBlitOnScrollRecursively();

    handleDeferredScrollUpdateAfterContentSizeChange();

    handleDeferredScrollbarsUpdate();
    handleDeferredPositionScrollbarLayers();

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(layoutWidth() < contentsWidth(), layoutHeight() < contentsHeight());

    document->markers().invalidateRectsForAllMarkers();
}

bool FrameView::shouldDeferScrollUpdateAfterContentSizeChange()
{
    return (layoutContext().layoutPhase() < FrameViewLayoutContext::LayoutPhase::InPostLayout) && (layoutContext().layoutPhase() != FrameViewLayoutContext::LayoutPhase::OutsideLayout);
}

RenderBox* FrameView::embeddedContentBox() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    RenderObject* firstChild = renderView->firstChild();

    // Curently only embedded SVG documents participate in the size-negotiation logic.
    if (is<LegacyRenderSVGRoot>(firstChild))
        return downcast<LegacyRenderSVGRoot>(firstChild);

    return nullptr;
}

void FrameView::addEmbeddedObjectToUpdate(RenderEmbeddedObject& embeddedObject)
{
    if (!m_embeddedObjectsToUpdate)
        m_embeddedObjectsToUpdate = makeUnique<ListHashSet<RenderEmbeddedObject*>>();

    auto& element = embeddedObject.frameOwnerElement();
    if (is<HTMLPlugInImageElement>(element))
        downcast<HTMLPlugInImageElement>(element).setNeedsWidgetUpdate(true);

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
    if (auto scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->coordinatesScrollingForFrameView(*this);
#endif
    return false;
}

bool FrameView::mockScrollbarsControllerEnabled() const
{
    return frame().settings().mockScrollbarsControllerEnabled();
}

void FrameView::logMockScrollbarsControllerMessage(const String& message) const
{
    auto document = frame().document();
    if (!document)
        return;
    document->addConsoleMessage(MessageSource::Other, MessageLevel::Debug,
        makeString(frame().isMainFrame() ? "Main" : "", "FrameView: ", message));
}

String FrameView::debugDescription() const
{
    return makeString("FrameView 0x", hex(reinterpret_cast<uintptr_t>(this), Lowercase), ' ', frame().debugDescription());
}

bool FrameView::styleHidesScrollbarWithOrientation(ScrollbarOrientation orientation) const
{
    auto element = rootElementForCustomScrollbarPartStyle(PseudoId::Scrollbar);
    if (!element)
        return false;
    auto* renderer = element->renderer();
    ASSERT(renderer); // rootElementForCustomScrollbarPart assures that it's not null.

    StyleScrollbarState scrollbarState;
    scrollbarState.scrollbarPart = ScrollbarBGPart;
    scrollbarState.orientation = orientation;
    auto scrollbarStyle = renderer->getUncachedPseudoStyle({ PseudoId::Scrollbar, scrollbarState }, &renderer->style());
    return scrollbarStyle && scrollbarStyle->display() == DisplayType::None;
}

bool FrameView::horizontalScrollbarHiddenByStyle() const
{
    if (managesScrollbars()) {
        auto* scrollbar = horizontalScrollbar();
        return scrollbar && scrollbar->isHiddenByStyle();
    }

    return styleHidesScrollbarWithOrientation(ScrollbarOrientation::Horizontal);
}

bool FrameView::verticalScrollbarHiddenByStyle() const
{
    if (managesScrollbars()) {
        auto* scrollbar = verticalScrollbar();
        return scrollbar && scrollbar->isHiddenByStyle();
    }
    
    return styleHidesScrollbarWithOrientation(ScrollbarOrientation::Vertical);
}

void FrameView::setCannotBlitToWindow()
{
    m_cannotBlitToWindow = true;
    updateCanBlitOnScrollRecursively();
}

void FrameView::addSlowRepaintObject(RenderElement& renderer)
{
    bool hadSlowRepaintObjects = hasSlowRepaintObjects();

    if (!m_slowRepaintObjects)
        m_slowRepaintObjects = makeUnique<WeakHashSet<RenderElement>>();

    auto addResult = m_slowRepaintObjects->add(renderer);
    if (addResult.isNewEntry) {
        if (auto layer = renderer.enclosingLayer())
            layer->setNeedsScrollingTreeUpdate();
    }

    if (hadSlowRepaintObjects)
        return;

    updateCanBlitOnScrollRecursively();
}

void FrameView::removeSlowRepaintObject(RenderElement& renderer)
{
    if (!m_slowRepaintObjects)
        return;

    bool removed = m_slowRepaintObjects->remove(renderer);
    if (removed) {
        if (auto layer = renderer.enclosingLayer())
            layer->setNeedsScrollingTreeUpdate();
    }

    if (!m_slowRepaintObjects->computesEmpty())
        return;

    m_slowRepaintObjects = nullptr;
    updateCanBlitOnScrollRecursively();
}

void FrameView::addViewportConstrainedObject(RenderLayerModelObject& object)
{
    if (!m_viewportConstrainedObjects)
        m_viewportConstrainedObjects = makeUnique<WeakHashSet<RenderLayerModelObject>>();

    if (!m_viewportConstrainedObjects->contains(object)) {
        m_viewportConstrainedObjects->add(object);
        if (platformWidget())
            updateCanBlitOnScrollRecursively();

        if (auto scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewFixedObjectsDidChange(*this);
    }
}

void FrameView::removeViewportConstrainedObject(RenderLayerModelObject& object)
{
    if (m_viewportConstrainedObjects && m_viewportConstrainedObjects->remove(object)) {
        if (auto scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewFixedObjectsDidChange(*this);

        // FIXME: In addFixedObject() we only call this if there's a platform widget,
        // why isn't the same check being made here?
        updateCanBlitOnScrollRecursively();
    }
}

LayoutSize FrameView::expandedLayoutViewportSize(const LayoutSize& baseLayoutViewportSize, const LayoutSize& documentSize, double heightExpansionFactor)
{
    if (!heightExpansionFactor)
        return baseLayoutViewportSize;

    auto documentHeight = documentSize.height();
    auto layoutViewportHeight = baseLayoutViewportSize.height();
    if (layoutViewportHeight > documentHeight)
        return baseLayoutViewportSize;

    return { baseLayoutViewportSize.width(), std::min(documentHeight, LayoutUnit((1 + heightExpansionFactor) * layoutViewportHeight)) };
}

LayoutRect FrameView::computeUpdatedLayoutViewportRect(const LayoutRect& layoutViewport, const LayoutRect& documentRect, const LayoutSize& unobscuredContentSize, const LayoutRect& unobscuredContentRect, const LayoutSize& baseLayoutViewportSize, const LayoutPoint& stableLayoutViewportOriginMin, const LayoutPoint& stableLayoutViewportOriginMax, LayoutViewportConstraint constraint)
{
    LayoutRect layoutViewportRect = layoutViewport;
    
    // The layout viewport is never smaller than baseLayoutViewportSize, and never be smaller than the unobscuredContentRect.
    LayoutSize constrainedSize = baseLayoutViewportSize;
    layoutViewportRect.setSize(constrainedSize.expandedTo(unobscuredContentSize));
        
    LayoutPoint layoutViewportOrigin = computeLayoutViewportOrigin(unobscuredContentRect, stableLayoutViewportOriginMin, stableLayoutViewportOriginMax, layoutViewportRect, StickToViewportBounds);

    // FIXME: Is this equivalent to calling computeLayoutViewportOrigin() with StickToDocumentBounds?
    if (constraint == LayoutViewportConstraint::ConstrainedToDocumentRect) {
        // The max stable layout viewport origin really depends on the size of the layout viewport itself, so we need to adjust the location of the layout viewport one final time to make sure it does not end up out of bounds of the document.
        // Without this adjustment (and with using the non-constrained unobscuredContentRect's size as the size of the layout viewport) the layout viewport can be pushed past the bounds of the document during rubber-banding, and cannot be pushed
        // back in until the user scrolls back in the other direction.
        layoutViewportOrigin.setX(clampTo<float>(layoutViewportOrigin.x().toFloat(), 0, documentRect.width() - layoutViewportRect.width()));
        layoutViewportOrigin.setY(clampTo<float>(layoutViewportOrigin.y().toFloat(), 0, documentRect.height() - layoutViewportRect.height()));
    }
    layoutViewportRect.setLocation(layoutViewportOrigin);
    
    return layoutViewportRect;
}

// visualViewport and layoutViewport are both in content coordinates (unzoomed).
LayoutPoint FrameView::computeLayoutViewportOrigin(const LayoutRect& visualViewport, const LayoutPoint& stableLayoutViewportOriginMin, const LayoutPoint& stableLayoutViewportOriginMax, const LayoutRect& layoutViewport, ScrollBehaviorForFixedElements fixedBehavior)
{
    LayoutPoint layoutViewportOrigin = layoutViewport.location();
    bool allowRubberBanding = fixedBehavior == StickToViewportBounds;

    if (visualViewport.width() > layoutViewport.width()) {
        layoutViewportOrigin.setX(visualViewport.x());
        if (!allowRubberBanding) {
            if (layoutViewportOrigin.x() < stableLayoutViewportOriginMin.x())
                layoutViewportOrigin.setX(stableLayoutViewportOriginMin.x());
            else if (layoutViewportOrigin.x() > stableLayoutViewportOriginMax.x())
                layoutViewportOrigin.setX(stableLayoutViewportOriginMax.x());
        }
    } else {
        bool rubberbandingAtLeft = allowRubberBanding && visualViewport.x() < stableLayoutViewportOriginMin.x();
        bool rubberbandingAtRight = allowRubberBanding && (visualViewport.maxX() - layoutViewport.width()) > stableLayoutViewportOriginMax.x();

        if (visualViewport.x() < layoutViewport.x() || rubberbandingAtLeft)
            layoutViewportOrigin.setX(visualViewport.x());

        if (visualViewport.maxX() > layoutViewport.maxX() || rubberbandingAtRight)
            layoutViewportOrigin.setX(visualViewport.maxX() - layoutViewport.width());

        if (!rubberbandingAtLeft && layoutViewportOrigin.x() < stableLayoutViewportOriginMin.x())
            layoutViewportOrigin.setX(stableLayoutViewportOriginMin.x());
        
        if (!rubberbandingAtRight && layoutViewportOrigin.x() > stableLayoutViewportOriginMax.x())
            layoutViewportOrigin.setX(stableLayoutViewportOriginMax.x());
    }

    if (visualViewport.height() > layoutViewport.height()) {
        layoutViewportOrigin.setY(visualViewport.y());
        if (!allowRubberBanding) {
            if (layoutViewportOrigin.y() < stableLayoutViewportOriginMin.y())
                layoutViewportOrigin.setY(stableLayoutViewportOriginMin.y());
            else if (layoutViewportOrigin.y() > stableLayoutViewportOriginMax.y())
                layoutViewportOrigin.setY(stableLayoutViewportOriginMax.y());
        }
    } else {
        bool rubberbandingAtTop = allowRubberBanding && visualViewport.y() < stableLayoutViewportOriginMin.y();
        bool rubberbandingAtBottom = allowRubberBanding && (visualViewport.maxY() - layoutViewport.height()) > stableLayoutViewportOriginMax.y();

        if (visualViewport.y() < layoutViewport.y() || rubberbandingAtTop)
            layoutViewportOrigin.setY(visualViewport.y());

        if (visualViewport.maxY() > layoutViewport.maxY() || rubberbandingAtBottom)
            layoutViewportOrigin.setY(visualViewport.maxY() - layoutViewport.height());

        if (!rubberbandingAtTop && layoutViewportOrigin.y() < stableLayoutViewportOriginMin.y())
            layoutViewportOrigin.setY(stableLayoutViewportOriginMin.y());

        if (!rubberbandingAtBottom && layoutViewportOrigin.y() > stableLayoutViewportOriginMax.y())
            layoutViewportOrigin.setY(stableLayoutViewportOriginMax.y());
    }

    return layoutViewportOrigin;
}

void FrameView::setBaseLayoutViewportOrigin(LayoutPoint origin, TriggerLayoutOrNot layoutTriggering)
{
    ASSERT(frame().settings().visualViewportEnabled());

    if (origin == m_layoutViewportOrigin)
        return;

    m_layoutViewportOrigin = origin;
    if (layoutTriggering == TriggerLayoutOrNot::Yes)
        setViewportConstrainedObjectsNeedLayout();
    
    if (TiledBacking* tiledBacking = this->tiledBacking()) {
        FloatRect layoutViewport = layoutViewportRect();
        layoutViewport.moveBy(unscaledScrollOrigin()); // tiledBacking deals in top-left relative coordinates.
        tiledBacking->setLayoutViewportRect(layoutViewport);
    }
}

void FrameView::setLayoutViewportOverrideRect(std::optional<LayoutRect> rect, TriggerLayoutOrNot layoutTriggering)
{
    if (rect == m_layoutViewportOverrideRect)
        return;

    LayoutRect oldRect = layoutViewportRect();
    m_layoutViewportOverrideRect = rect;

    // Triggering layout on height changes is necessary to make bottom-fixed elements behave correctly.
    if (oldRect.height() != layoutViewportRect().height())
        layoutTriggering = TriggerLayoutOrNot::Yes;

    LOG_WITH_STREAM(Scrolling, stream << "\nFrameView " << this << " setLayoutViewportOverrideRect() - changing override layout viewport from " << oldRect << " to " << valueOrDefault(m_layoutViewportOverrideRect) << " layoutTriggering " << (layoutTriggering == TriggerLayoutOrNot::Yes ? "yes" : "no"));

    if (oldRect != layoutViewportRect() && layoutTriggering == TriggerLayoutOrNot::Yes)
        setViewportConstrainedObjectsNeedLayout();
}

void FrameView::setVisualViewportOverrideRect(std::optional<LayoutRect> rect)
{
    m_visualViewportOverrideRect = rect;
}

LayoutSize FrameView::baseLayoutViewportSize() const
{
    return renderView() ? renderView()->size() : size();
}

void FrameView::updateLayoutViewport()
{
    if (!frame().settings().visualViewportEnabled())
        return;
    
    // Don't update the layout viewport if we're in the middle of adjusting scrollbars. We'll get another call
    // as a post-layout task.
    if (layoutContext().layoutPhase() == FrameViewLayoutContext::LayoutPhase::InViewSizeAdjust)
        return;

    LayoutRect layoutViewport = layoutViewportRect();

    LOG_WITH_STREAM(Scrolling, stream << "\nFrameView " << this << " updateLayoutViewport() totalContentSize " << totalContentsSize() << " unscaledDocumentRect " << (renderView() ? renderView()->unscaledDocumentRect() : IntRect()) << " header height " << headerHeight() << " footer height " << footerHeight() << " fixed behavior " << scrollBehaviorForFixedElements());
    LOG_WITH_STREAM(Scrolling, stream << "layoutViewport: " << layoutViewport);
    LOG_WITH_STREAM(Scrolling, stream << "visualViewport: " << visualViewportRect() << " (is override " << (bool)m_visualViewportOverrideRect << ")");
    LOG_WITH_STREAM(Scrolling, stream << "stable origins: min: " << minStableLayoutViewportOrigin() << " max: "<< maxStableLayoutViewportOrigin());
    
    if (m_layoutViewportOverrideRect) {
        if (currentScrollType() == ScrollType::Programmatic) {
            LOG_WITH_STREAM(Scrolling, stream << "computing new override layout viewport because of programmatic scrolling");
            LayoutPoint newOrigin = computeLayoutViewportOrigin(visualViewportRect(), minStableLayoutViewportOrigin(), maxStableLayoutViewportOrigin(), layoutViewport, StickToDocumentBounds);
            setLayoutViewportOverrideRect(LayoutRect(newOrigin, m_layoutViewportOverrideRect.value().size()));
        }
        layoutOrVisualViewportChanged();
        return;
    }

    LayoutPoint newLayoutViewportOrigin = computeLayoutViewportOrigin(visualViewportRect(), minStableLayoutViewportOrigin(), maxStableLayoutViewportOrigin(), layoutViewport, scrollBehaviorForFixedElements());
    if (newLayoutViewportOrigin != m_layoutViewportOrigin) {
        setBaseLayoutViewportOrigin(newLayoutViewportOrigin);
        LOG_WITH_STREAM(Scrolling, stream << "layoutViewport changed to " << layoutViewportRect());
    }
    layoutOrVisualViewportChanged();
}

LayoutPoint FrameView::minStableLayoutViewportOrigin() const
{
    return unscaledMinimumScrollPosition();
}

LayoutPoint FrameView::maxStableLayoutViewportOrigin() const
{
    LayoutPoint maxPosition = unscaledMaximumScrollPosition();
    maxPosition = (maxPosition - LayoutSize(0, headerHeight() + footerHeight())).expandedTo({ });
    return maxPosition;
}

IntPoint FrameView::unscaledScrollOrigin() const
{
    if (RenderView* renderView = this->renderView())
        return -renderView->unscaledDocumentRect().location(); // Akin to code in adjustViewSize().

    return { };
}

LayoutRect FrameView::layoutViewportRect() const
{
    if (m_layoutViewportOverrideRect)
        return m_layoutViewportOverrideRect.value();

    // Size of initial containing block, anchored at scroll position, in document coordinates (unchanged by scale factor).
    return LayoutRect(m_layoutViewportOrigin, baseLayoutViewportSize());
}

// visibleContentRect is in the bounds of the scroll view content. That consists of an
// optional header, the document, and an optional footer. Only the document is scaled,
// so we have to compute the visible part of the document in unscaled document coordinates.
// On iOS, pageScaleFactor is always 1 here, and we never have headers and footers.
LayoutRect FrameView::visibleDocumentRect(const FloatRect& visibleContentRect, float headerHeight, float footerHeight, const FloatSize& totalContentsSize, float pageScaleFactor)
{
    float contentsHeight = totalContentsSize.height() - headerHeight - footerHeight;

    float rubberBandTop = std::min<float>(visibleContentRect.y(), 0);
    float visibleScaledDocumentTop = std::max<float>(visibleContentRect.y() - headerHeight, 0) + rubberBandTop;
    
    float rubberBandBottom = std::min<float>((totalContentsSize.height() - visibleContentRect.y()) - visibleContentRect.height(), 0);
    float visibleScaledDocumentBottom = std::min<float>(visibleContentRect.maxY() - headerHeight, contentsHeight) - rubberBandBottom;

    FloatRect visibleDocumentRect = visibleContentRect;
    visibleDocumentRect.setY(visibleScaledDocumentTop);
    visibleDocumentRect.setHeight(std::max<float>(visibleScaledDocumentBottom - visibleScaledDocumentTop, 0));
    visibleDocumentRect.scale(1 / pageScaleFactor);
    
    return LayoutRect(visibleDocumentRect);
}

LayoutRect FrameView::visualViewportRect() const
{
    if (m_visualViewportOverrideRect)
        return m_visualViewportOverrideRect.value();

    FloatRect visibleContentRect = this->visibleContentRect(LegacyIOSDocumentVisibleRect);
    return visibleDocumentRect(visibleContentRect, headerHeight(), footerHeight(), totalContentsSize(), frameScaleFactor());
}

LayoutRect FrameView::viewportConstrainedVisibleContentRect() const
{
    ASSERT(!frame().settings().visualViewportEnabled());

#if PLATFORM(IOS_FAMILY)
    if (useCustomFixedPositionLayoutRect())
        return customFixedPositionLayoutRect();
#endif
    LayoutRect viewportRect = visibleContentRect();

    viewportRect.setLocation(scrollPositionForFixedPosition());
    return viewportRect;
}

LayoutRect FrameView::rectForFixedPositionLayout() const
{
    if (frame().settings().visualViewportEnabled())
        return layoutViewportRect();

    return viewportConstrainedVisibleContentRect();
}

float FrameView::frameScaleFactor() const
{
    return frame().frameScaleFactor();
}

LayoutPoint FrameView::scrollPositionForFixedPosition() const
{
    if (frame().settings().visualViewportEnabled())
        return layoutViewportRect().location();

    return scrollPositionForFixedPosition(visibleContentRect(), totalContentsSize(), scrollPosition(), scrollOrigin(), frameScaleFactor(), fixedElementsLayoutRelativeToFrame(), scrollBehaviorForFixedElements(), headerHeight(), footerHeight());
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

#if PLATFORM(IOS_FAMILY)
LayoutRect FrameView::rectForViewportConstrainedObjects(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements scrollBehavior)
{
    if (fixedElementsLayoutRelativeToFrame)
        return visibleContentRect;
    
    if (totalContentsSize.isEmpty())
        return visibleContentRect;

    // We impose an lower limit on the size (so an upper limit on the scale) of
    // the rect used to position fixed objects so that they don't crowd into the
    // center of the screen at larger scales.
    const LayoutUnit maxContentWidthForZoomThreshold = 1024_lu;
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

ScrollPosition FrameView::unscaledMinimumScrollPosition() const
{
    if (RenderView* renderView = this->renderView()) {
        IntRect unscaledDocumentRect = renderView->unscaledDocumentRect();
        ScrollPosition minimumPosition = unscaledDocumentRect.location();

        if (frame().isMainFrame() && m_scrollPinningBehavior == PinToBottom)
            minimumPosition.setY(unscaledMaximumScrollPosition().y());

        return minimumPosition;
    }

    return minimumScrollPosition();
}

ScrollPosition FrameView::unscaledMaximumScrollPosition() const
{
    if (RenderView* renderView = this->renderView()) {
        IntRect unscaledDocumentRect = renderView->unscaledDocumentRect();
        unscaledDocumentRect.expand(0, headerHeight() + footerHeight());
        ScrollPosition maximumPosition = ScrollPosition(unscaledDocumentRect.maxXMaxYCorner() - visibleSize()).expandedTo({ 0, 0 });
        if (frame().isMainFrame() && m_scrollPinningBehavior == PinToTop)
            maximumPosition.setY(unscaledMinimumScrollPosition().y());

        return maximumPosition;
    }

    return maximumScrollPosition();
}

void FrameView::viewportContentsChanged()
{
    if (!frame().view()) {
        // The frame is being destroyed.
        return;
    }

    if (auto* page = frame().page())
        page->updateValidationBubbleStateIfNeeded();

    // When the viewport contents changes (scroll, resize, style recalc, layout, ...),
    // check if we should resume animated images or unthrottle DOM timers.
    applyRecursivelyWithVisibleRect([] (FrameView& frameView, const IntRect& visibleRect) {
        frameView.resumeVisibleImageAnimations(visibleRect);
        frameView.updateScriptedAnimationsAndTimersThrottlingState(visibleRect);

        if (auto* renderView = frameView.frame().contentRenderer())
            renderView->updateVisibleViewportRect(visibleRect);
    });
}

IntRect FrameView::viewRectExpandedByContentInsets() const
{
    FloatRect viewRect;
    if (delegatesScrolling() && platformWidget())
        viewRect = unobscuredContentRect();
    else
        viewRect = visualViewportRect();

    if (auto* page = frame().page())
        viewRect.expand(page->contentInsets());

    return IntRect(viewRect);
}

bool FrameView::fixedElementsLayoutRelativeToFrame() const
{
    return frame().settings().fixedElementsLayoutRelativeToFrame();
}

IntPoint FrameView::lastKnownMousePositionInView() const
{
    return convertFromContainingWindow(frame().eventHandler().lastKnownMousePosition());
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

#if ENABLE(DARK_MODE_CSS)
RenderObject* FrameView::rendererForColorScheme() const
{
    auto* document = frame().document();
    auto* documentElement = document ? document->documentElement() : nullptr;
    auto* documentElementRenderer = documentElement ? documentElement->renderer() : nullptr;
    if (documentElementRenderer && documentElementRenderer->style().hasExplicitlySetColorScheme())
        return documentElementRenderer;
    auto* bodyElement = document ? document->bodyOrFrameset() : nullptr;
    return bodyElement ? bodyElement->renderer() : nullptr;
}
#endif

bool FrameView::useDarkAppearance() const
{
#if ENABLE(DARK_MODE_CSS)
    if (auto* renderer = rendererForColorScheme())
        return renderer->useDarkAppearance();
#endif
    if (auto* document = frame().document())
        return document->useDarkAppearance(nullptr);
    return false;
}

OptionSet<StyleColorOptions> FrameView::styleColorOptions() const
{
#if ENABLE(DARK_MODE_CSS)
    if (auto* renderer = rendererForColorScheme())
        return renderer->styleColorOptions();
#endif
    if (auto* document = frame().document())
        return document->styleColorOptions(nullptr);
    return { };
}

bool FrameView::scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    if (!m_viewportConstrainedObjects || m_viewportConstrainedObjects->computesEmpty()) {
        frame().page()->chrome().scroll(scrollDelta, rectToScroll, clipRect);
        return true;
    }

    bool isCompositedContentLayer = usesCompositedScrolling();

    // Get the rects of the fixed objects visible in the rectToScroll
    Region regionToUpdate;
    for (auto& renderer : *m_viewportConstrainedObjects) {
        if (!renderer.style().hasViewportConstrainedPosition())
            continue;
        if (renderer.isComposited())
            continue;

        // Fixed items should always have layers.
        ASSERT(renderer.hasLayer());
        RenderLayer* layer = downcast<RenderBoxModelObject>(renderer).layer();

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
        if (!isCompositedContentLayer)
            updateRect.intersect(rectToScroll);
        if (!updateRect.isEmpty())
            regionToUpdate.unite(updateRect);
    }

    // 1) scroll
    frame().page()->chrome().scroll(scrollDelta, rectToScroll, clipRect);

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
        updateRect.intersect(rectToScroll);
        frame().page()->chrome().invalidateContentsAndRootView(updateRect);
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
        renderer.repaintSlowRepaintObject();
}

// Note that this gets called at painting time.
void FrameView::setIsOverlapped(bool isOverlapped)
{
    if (isOverlapped == m_isOverlapped)
        return;

    m_isOverlapped = isOverlapped;
    updateCanBlitOnScrollRecursively();
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
    ASSERT(frame().document());
    Ref document = *frame().document();
    
    auto fragmentIdentifier = url.fragmentIdentifier();
    
    if (document->settings().scrollToTextFragmentEnabled()) {
        FragmentDirectiveParser fragmentDirectiveParser(url);
        
        if (fragmentDirectiveParser.isValid()) {
            auto fragmentDirective = fragmentDirectiveParser.fragmentDirective().toString();
            document->setFragmentDirective(fragmentDirective);
            
            auto parsedTextDirectives = fragmentDirectiveParser.parsedTextDirectives();
            
            auto highlightRanges = FragmentDirectiveRangeFinder::rangesForFragments(parsedTextDirectives, document);
            for (auto range : highlightRanges)
                document->fragmentHighlightRegister().addAnnotationHighlightWithRange(StaticRange::create(range));
            
            if (highlightRanges.size()) {
                TemporarySelectionChange selectionChange(document, { highlightRanges.first() }, { TemporarySelectionOption::DelegateMainFrameScroll, TemporarySelectionOption::SmoothScroll, TemporarySelectionOption::RevealSelectionBounds });
                // FIXME: add a textIndicator after the scroll has completed.
            }
            
        } else
            fragmentIdentifier = fragmentDirectiveParser.remainingURLFragment();
    }
    
    if (scrollToFragmentInternal(fragmentIdentifier))
        return true;

    if (scrollToFragmentInternal(PAL::decodeURLEscapeSequences(fragmentIdentifier)))
        return true;

    resetScrollAnchor();
    return false;
}

bool FrameView::scrollToFragmentInternal(StringView fragmentIdentifier)
{
    // If our URL has no ref, then we have no place we need to jump to.
    if (fragmentIdentifier.isNull())
        return false;

    LOG_WITH_STREAM(Scrolling, stream << *this << " scrollToFragmentInternal " << fragmentIdentifier);

    ASSERT(frame().document());
    auto& document = *frame().document();
    RELEASE_ASSERT(document.haveStylesheetsLoaded());

    RefPtr anchorElement = document.findAnchor(fragmentIdentifier);

    LOG(Scrolling, " anchorElement is %p", anchorElement.get());

    // Setting to null will clear the current target.
    document.setCSSTarget(anchorElement.get());

    if (is<SVGDocument>(document)) {
        if (fragmentIdentifier.isEmpty())
            return false;
        if (auto rootElement = DocumentSVG::rootElement(document)) {
            if (rootElement->scrollToFragment(fragmentIdentifier))
                return true;
            // If SVG failed to scrollToAnchor() and anchorElement is null, no other scrolling will be possible.
            if (!anchorElement)
                return false;
        }
    } else if (!anchorElement && !(fragmentIdentifier.isEmpty() || equalLettersIgnoringASCIICase(fragmentIdentifier, "top"))) {
        // Implement the rule that "" and "top" both mean top of page as in other browsers.
        return false;
    }

    RefPtr<ContainerNode> scrollPositionAnchor = anchorElement;
    if (!scrollPositionAnchor)
        scrollPositionAnchor = frame().document();
    maintainScrollPositionAtAnchor(scrollPositionAnchor.get());
    
    // If the anchor accepts keyboard focus, move focus there to aid users relying on keyboard navigation.
    if (anchorElement) {
        if (anchorElement->isFocusable())
            document.setFocusedElement(anchorElement.get(), { { }, { }, { }, { }, FocusVisibility::Visible });
        else {
            document.setFocusedElement(nullptr);
            document.setFocusNavigationStartingNode(anchorElement.get());
        }
    }
    
    return true;
}

void FrameView::maintainScrollPositionAtAnchor(ContainerNode* anchorNode)
{
    LOG(Scrolling, "FrameView::maintainScrollPositionAtAnchor at %p", anchorNode);

    m_maintainScrollPositionAnchor = anchorNode;
    if (!m_maintainScrollPositionAnchor)
        return;
    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    frame().document()->updateStyleIfNeeded();
    // Only do a layout if changes have occurred that make it necessary.
    RenderView* renderView = this->renderView();
    if (renderView && renderView->needsLayout())
        layoutContext().layout();
    else
        scrollToAnchor();
}

void FrameView::scrollElementToRect(const Element& element, const IntRect& rect)
{
    frame().document()->updateLayoutIgnorePendingStylesheets();

    LayoutRect bounds;
    if (RenderElement* renderer = element.renderer())
        bounds = renderer->absoluteAnchorRect();
    int centeringOffsetX = (rect.width() - bounds.width()) / 2;
    int centeringOffsetY = (rect.height() - bounds.height()) / 2;
    setScrollPosition(IntPoint(bounds.x() - centeringOffsetX - rect.x(), bounds.y() - centeringOffsetY - rect.y()));
}

void FrameView::setScrollPosition(const ScrollPosition& scrollPosition, const ScrollPositionChangeOptions& options)
{
    LOG_WITH_STREAM(Scrolling, stream << "FrameView::setScrollPosition " << scrollPosition << " animated " << (options.animated == ScrollIsAnimated::Yes) << ", clearing anchor");

    auto oldScrollType = currentScrollType();
    setCurrentScrollType(options.type);

    m_maintainScrollPositionAnchor = nullptr;
    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();
    Page* page = frame().page();
    if (page && page->isMonitoringWheelEvents())
        scrollAnimator().setWheelEventTestMonitor(page->wheelEventTestMonitor());

    ScrollOffset snappedOffset = ceiledIntPoint(scrollAnimator().scrollOffsetAdjustedForSnapping(scrollOffsetFromPosition(scrollPosition), options.snapPointSelectionMethod));
    auto snappedPosition = scrollPositionFromOffset(snappedOffset);

    if (options.animated == ScrollIsAnimated::Yes)
        scrollToPositionWithAnimation(snappedPosition, currentScrollType(), options.clamping);
    else
        ScrollView::setScrollPosition(snappedPosition, options);

    setCurrentScrollType(oldScrollType);
}

void FrameView::resetScrollAnchor()
{
    ASSERT(frame().document());
    auto& document = *frame().document();

    // If CSS target was set previously, we want to set it to 0, recalc
    // and possibly repaint because :target pseudo class may have been
    // set (see bug 11321).
    document.setCSSTarget(nullptr);

    if (is<SVGDocument>(document)) {
        if (auto rootElement = DocumentSVG::rootElement(document)) {
            // We need to update the layout before resetScrollAnchor(), otherwise we
            // could really mess things up if resetting the anchor comes at a bad moment.
            document.updateStyleIfNeeded();
            rootElement->resetScrollAnchor();
        }
    }
}

void FrameView::scheduleScrollToFocusedElement(SelectionRevealMode selectionRevealMode)
{
    if (selectionRevealMode == SelectionRevealMode::DoNotReveal)
        return;

    m_selectionRevealModeForFocusedElement = selectionRevealMode;
    if (m_shouldScrollToFocusedElement)
        return;
    m_shouldScrollToFocusedElement = true;
    m_delayedScrollToFocusedElementTimer.startOneShot(0_s);
}

void FrameView::scrollToFocusedElementImmediatelyIfNeeded()
{
    if (!m_shouldScrollToFocusedElement)
        return;

    m_delayedScrollToFocusedElementTimer.stop();
    scrollToFocusedElementInternal();
}

void FrameView::scrollToFocusedElementTimerFired()
{
    Ref protectedThis { *this };
    scrollToFocusedElementInternal();
}

void FrameView::scrollToFocusedElementInternal()
{
    RELEASE_ASSERT(m_shouldScrollToFocusedElement);
    RefPtr document = frame().document();
    if (!document)
        return;

    document->updateLayoutIgnorePendingStylesheets();
    if (!m_shouldScrollToFocusedElement)
        return; // Updating the layout may have ran scripts.
    m_shouldScrollToFocusedElement = false;

    RefPtr focusedElement = document->focusedElement();
    if (!focusedElement)
        return;
    auto updateTarget = focusedElement->focusAppearanceUpdateTarget();
    if (!updateTarget)
        return;

    auto* renderer = updateTarget->renderer();
    if (!renderer || renderer->isWidget())
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed);
    renderer->scrollRectToVisible(absoluteBounds, insideFixed, { m_selectionRevealModeForFocusedElement, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, ShouldAllowCrossOriginScrolling::No });
}

void FrameView::contentsResized()
{
    // For non-delegated scrolling, updateTiledBackingAdaptiveSizing() is called via addedOrRemovedScrollbar() which occurs less often.
    if (delegatesScrolling())
        updateTiledBackingAdaptiveSizing();
}

void FrameView::delegatesScrollingDidChange()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    RenderLayerCompositor& compositor = renderView->compositor();
    // When we switch to delegatesScrolling mode, we should destroy the scrolling/clipping layers in RenderLayerCompositor.
    if (compositor.usesCompositing()) {
        ASSERT(compositor.usesCompositing());
        compositor.enableCompositingMode(false);
        compositor.clearBackingForAllLayers();
    }
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

    for (auto& renderer : *m_viewportConstrainedObjects) {
        renderer.setNeedsLayout();
        if (renderer.hasLayer()) {
            auto* layer = downcast<RenderBoxModelObject>(renderer).layer();
            layer->setNeedsCompositingGeometryUpdate();
        }
    }
}

void FrameView::didChangeScrollOffset()
{
    if (auto* page = frame().page())
        page->pageOverlayController().didScrollFrame(frame());
    frame().loader().client().didChangeScrollOffset();
}

void FrameView::scrollOffsetChangedViaPlatformWidgetImpl(const ScrollOffset& oldOffset, const ScrollOffset& newOffset)
{
#if PLATFORM(COCOA)
    auto deferrer = WheelEventTestMonitorCompletionDeferrer { frame().page()->wheelEventTestMonitor().get(), this, WheelEventTestMonitor::DeferReason::ContentScrollInProgress };
#endif

    updateLayerPositionsAfterScrolling();
    updateCompositingLayersAfterScrolling();
    repaintSlowRepaintObjects();
    scrollPositionChanged(scrollPositionFromOffset(oldOffset), scrollPositionFromOffset(newOffset));
    
    if (auto* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().didChangeVisibleRect();
    }
}

// These scroll positions are affected by zooming.
void FrameView::scrollPositionChanged(const ScrollPosition& oldPosition, const ScrollPosition& newPosition)
{
    UNUSED_PARAM(oldPosition);
    UNUSED_PARAM(newPosition);

    Page* page = frame().page();
    Seconds throttlingDelay = page ? page->chrome().client().eventThrottlingDelay() : 0_s;

    if (throttlingDelay == 0_s) {
        m_delayedScrollEventTimer.stop();
        scheduleScrollEvent();
    } else if (!m_delayedScrollEventTimer.isActive())
        m_delayedScrollEventTimer.startOneShot(throttlingDelay);

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidScroll();
    }

    LOG_WITH_STREAM(Scrolling, stream << "FrameView " << this << " scrollPositionChanged from " << oldPosition << " to " << newPosition << " (scale " << frameScaleFactor() << " )");
    updateLayoutViewport();
    viewportContentsChanged();

    if (auto* renderView = this->renderView()) {
        if (auto* layer = renderView->layer())
            frame().editor().renderLayerDidScroll(*layer);
    }
}

void FrameView::applyRecursivelyWithVisibleRect(const Function<void(FrameView& frameView, const IntRect& visibleRect)>& apply)
{
    IntRect windowClipRect = this->windowClipRect();
    auto visibleRect = windowToContents(windowClipRect);
    apply(*this, visibleRect);

    // Recursive call for subframes. We cache the current FrameView's windowClipRect to avoid recomputing it for every subframe.
    SetForScope<IntRect*> windowClipRectCache(m_cachedWindowClipRect, &windowClipRect);
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
    document->setTimerThrottlingEnabled(shouldThrottle);

    auto* page = frame().page();
    if (!page || !page->canUpdateThrottlingReason(ThrottlingReason::OutsideViewport))
        return;
    
    auto* scriptedAnimationController = document->scriptedAnimationController();
    if (!scriptedAnimationController)
        return;

    if (shouldThrottle)
        scriptedAnimationController->addThrottlingReason(ThrottlingReason::OutsideViewport);
    else
        scriptedAnimationController->removeThrottlingReason(ThrottlingReason::OutsideViewport);
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
    if (layoutContext().layoutPhase() == FrameViewLayoutContext::LayoutPhase::InViewSizeAdjust)
        return;

    if (!layoutContext().isLayoutNested() && hasViewportConstrainedObjects()) {
        if (auto* renderView = this->renderView()) {
            updateWidgetPositions();
            renderView->layer()->updateLayerPositionsAfterDocumentScroll();
        }
    }
}

ScrollingCoordinator* FrameView::scrollingCoordinator() const
{
    return frame().page() ? frame().page()->scrollingCoordinator() : nullptr;
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

    auto scrollingCoordinator = this->scrollingCoordinator();
    if (!scrollingCoordinator)
        return true;

    if (scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously(*this))
        return true;

    if (currentScrollType() == ScrollType::Programmatic)
        return true;

    return false;
#endif
    return true;
}

void FrameView::updateCompositingLayersAfterScrolling()
{
    ASSERT(layoutContext().layoutPhase() >= FrameViewLayoutContext::LayoutPhase::InPostLayout || layoutContext().layoutPhase() == FrameViewLayoutContext::LayoutPhase::OutsideLayout);

    if (!shouldUpdateCompositingLayersAfterScrolling())
        return;

    if (!layoutContext().isLayoutNested() && hasViewportConstrainedObjects()) {
        if (RenderView* renderView = this->renderView())
            renderView->compositor().updateCompositingLayers(CompositingUpdateType::OnScroll);
    }
}

bool FrameView::isUserScrollInProgress() const
{
    if (auto scrollingCoordinator = this->scrollingCoordinator()) {
        if (scrollingCoordinator->isUserScrollInProgress(scrollingNodeID()))
            return true;
    }

    if (auto scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isUserScrollInProgress();

    return false;
}

bool FrameView::isRubberBandInProgress() const
{
    if (scrollbarsSuppressed())
        return false;

    if (auto scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->isRubberBandInProgress(scrollingNodeID());

    if (auto scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();

    return false;
}

bool FrameView::requestScrollPositionUpdate(const ScrollPosition& position, ScrollType scrollType, ScrollClamping clamping)
{
    LOG_WITH_STREAM(Scrolling, stream << "FrameView::requestScrollPositionUpdate " << position);

#if ENABLE(ASYNC_SCROLLING)
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->prepopulateRect(FloatRect(position, visibleContentRect().size()));
#endif

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
    if (auto scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->requestScrollPositionUpdate(*this, position, scrollType, clamping);
#else
    UNUSED_PARAM(position);
#endif

    return false;
}

bool FrameView::requestAnimatedScrollToPosition(const ScrollPosition& destinationPosition, ScrollClamping clamping)
{
    LOG_WITH_STREAM(Scrolling, stream << "FrameView::requestAnimatedScrollToPosition " << destinationPosition);

#if ENABLE(ASYNC_SCROLLING)
    if (auto scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->requestAnimatedScrollToPosition(*this, destinationPosition, clamping);
#else
    UNUSED_PARAM(destinationPosition);
#endif

    return false;
}

void FrameView::stopAsyncAnimatedScroll()
{
#if ENABLE(ASYNC_SCROLLING)
    LOG_WITH_STREAM(Scrolling, stream << "FrameView::stopAsyncAnimatedScroll");

    if (auto scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->stopAnimatedScroll(*this);
#endif
}

HostWindow* FrameView::hostWindow() const
{
    auto* page = frame().page();
    if (!page)
        return nullptr;
    return &page->chrome();
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
            count += downcast<RenderText>(*descendant).text().length();
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
    if (Document* document = frame().document()) {
        // FIXME: Merge this logic with m_setNeedsLayoutWasDeferred and find a more appropriate
        // way of handling potential recursive layouts when the viewport is resized to accomodate
        // the content but the content always overflows the viewport. See webkit.org/b/165781.
        if (layoutContext().layoutPhase() == FrameViewLayoutContext::LayoutPhase::InViewSizeAdjust)
            document->updateViewportUnitsOnResize();
    }

    updateLayoutViewport();
    setNeedsLayoutAfterViewConfigurationChange();
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

#if PLATFORM(IOS_FAMILY)
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
        layoutContext().layout();

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

    updateTiledBackingAdaptiveSizing();
}

TiledBacking::Scrollability FrameView::computeScrollability() const
{
    auto* page = frame().page();

    // Use smaller square tiles if the Window is not active to facilitate app napping.
    if (!page || !page->isWindowActive())
        return TiledBacking::HorizontallyScrollable | TiledBacking::VerticallyScrollable;

    bool horizontallyScrollable;
    bool verticallyScrollable;
    bool clippedByAncestorView = static_cast<bool>(m_viewExposedRect);

#if PLATFORM(IOS_FAMILY)
    if (page)
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

    return scrollability;
}

void FrameView::updateTiledBackingAdaptiveSizing()
{
    auto* tiledBacking = this->tiledBacking();
    if (!tiledBacking)
        return;

    tiledBacking->setScrollability(computeScrollability());
}

// FIXME: This shouldn't be called from outside; FrameView should call it when the relevant viewports change.
void FrameView::layoutOrVisualViewportChanged()
{
    if (frame().settings().visualViewportAPIEnabled()) {
        if (auto* window = frame().window())
            window->visualViewport().update();

        if (auto scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewVisualViewportChanged(*this);
    }

    auto layoutViewportSize = layoutViewportRect().size();
    if (layoutViewportSize != m_lastLayoutViewportSize) {
        if (auto* document = frame().document())
            document->updateViewportUnitsOnResize();

        m_lastLayoutViewportSize = layoutViewportSize;
    }
}

void FrameView::unobscuredContentSizeChanged()
{
#if PLATFORM(IOS_FAMILY)
    updateTiledBackingAdaptiveSizing();
#endif
}

void FrameView::loadProgressingStatusChanged()
{
    if (m_firstVisuallyNonEmptyLayoutMilestoneIsPending && frame().loader().isComplete())
        fireLayoutRelatedMilestonesIfNeeded();
    adjustTiledBackingCoverage();
}

void FrameView::adjustTiledBackingCoverage()
{
    if (!m_speculativeTilingEnabled)
        enableSpeculativeTilingIfNeeded();

    RenderView* renderView = this->renderView();
    if (renderView && renderView->layer() && renderView->layer()->backing())
        renderView->layer()->backing()->adjustTiledBackingCoverage();
#if PLATFORM(IOS_FAMILY)
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

    if (m_speculativeTilingDelayDisabledForTesting) {
        speculativeTilingEnableTimerFired();
        return;
    }

    if (m_speculativeTilingEnableTimer.isActive())
        return;
    // Delay enabling a bit as load completion may trigger further loading from scripts.
    static const Seconds speculativeTilingEnableDelay { 500_ms };
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

void FrameView::hide()
{
    ScrollView::hide();
    adjustTiledBackingCoverage();
}

bool FrameView::needsLayout() const
{
    return layoutContext().needsLayout();
}

void FrameView::setNeedsLayoutAfterViewConfigurationChange()
{
    layoutContext().setNeedsLayoutAfterViewConfigurationChange();
}

void FrameView::setNeedsCompositingConfigurationUpdate()
{
    RenderView* renderView = this->renderView();
    if (renderView && renderView->usesCompositing()) {
        if (auto* rootLayer = renderView->layer())
            rootLayer->setNeedsCompositingConfigurationUpdate();
        renderView->compositor().scheduleCompositingLayerUpdate();
    }
}

void FrameView::setNeedsCompositingGeometryUpdate()
{
    RenderView* renderView = this->renderView();
    if (renderView->usesCompositing()) {
        if (auto* rootLayer = renderView->layer())
            rootLayer->setNeedsCompositingGeometryUpdate();
        renderView->compositor().scheduleCompositingLayerUpdate();
    }
}

void FrameView::scheduleSelectionUpdate()
{
    if (needsLayout())
        return;
    // FIXME: We should not need to go through the layout process since selection update does not change dimension/geometry.
    // However we can't tell at this point if the tree is stable yet, so let's just schedule a root only layout for now.
    setNeedsLayoutAfterViewConfigurationChange();
}

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

    setNeedsLayoutAfterViewConfigurationChange();
    setNeedsCompositingConfigurationUpdate();
}

bool FrameView::hasOpaqueBackground() const
{
    return !m_isTransparent && m_baseBackgroundColor.isOpaque();
}

Color FrameView::baseBackgroundColor() const
{
    return m_baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(const Color& backgroundColor)
{
    Color newBaseBackgroundColor = backgroundColor.isValid() ? backgroundColor : Color::white;
    if (m_baseBackgroundColor == newBaseBackgroundColor)
        return;

    m_baseBackgroundColor = newBaseBackgroundColor;

    if (!isViewForDocumentInFrame())
        return;

    recalculateScrollbarOverlayStyle();
    setNeedsLayoutAfterViewConfigurationChange();
    setNeedsCompositingConfigurationUpdate();
}

void FrameView::updateBackgroundRecursively(const std::optional<Color>& backgroundColor)
{
#if HAVE(OS_DARK_MODE_SUPPORT)
#if PLATFORM(COCOA)
    static const auto cssValueControlBackground = CSSValueAppleSystemControlBackground;
#else
    static const auto cssValueControlBackground = CSSValueWindow;
#endif
#endif

    for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        if (auto* view = frame->view()) {
#if HAVE(OS_DARK_MODE_SUPPORT)
            auto baseBackgroundColor = backgroundColor.value_or(RenderTheme::singleton().systemColor(cssValueControlBackground, view->styleColorOptions()));
#else
            auto baseBackgroundColor = backgroundColor.value_or(Color::white);
#endif
            view->setTransparent(!baseBackgroundColor.isVisible());
            view->setBaseBackgroundColor(baseBackgroundColor);
            if (view->needsLayout())
                view->layoutContext().scheduleLayout();
        }
    }
}

bool FrameView::hasExtendedBackgroundRectForPainting() const
{
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
#if PLATFORM(IOS_FAMILY)
    // <rdar://problem/16201373>
    return ExtendedBackgroundModeNone;
#else
    if (!frame().settings().backgroundShouldExtendBeyondPage())
        return ExtendedBackgroundModeNone;

    // Just because Settings::backgroundShouldExtendBeyondPage() is true does not necessarily mean
    // that the background rect needs to be extended for painting. Simple backgrounds can be extended
    // just with RenderLayerCompositor's rootExtendedBackgroundColor. More complicated backgrounds,
    // such as images, require extending the background rect to continue painting into the extended
    // region. This function finds out if it is necessary to extend the background rect for painting.

    if (!frame().isMainFrame())
        return ExtendedBackgroundModeNone;

    Document* document = frame().document();
    if (!document)
        return ExtendedBackgroundModeNone;

    if (!renderView())
        return ExtendedBackgroundModeNone;
    
    auto* rootBackgroundRenderer = renderView()->rendererForRootBackground();
    if (!rootBackgroundRenderer)
        return ExtendedBackgroundModeNone;

    if (!rootBackgroundRenderer->style().hasBackgroundImage())
        return ExtendedBackgroundModeNone;

    ExtendedBackgroundMode mode = ExtendedBackgroundModeNone;
    if (rootBackgroundRenderer->style().backgroundRepeatX() == FillRepeat::Repeat)
        mode |= ExtendedBackgroundModeHorizontal;
    if (rootBackgroundRenderer->style().backgroundRepeatY() == FillRepeat::Repeat)
        mode |= ExtendedBackgroundModeVertical;

    return mode;
#endif
}

void FrameView::updateTilesForExtendedBackgroundMode(ExtendedBackgroundMode mode)
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return;

    TiledBacking* tiledBacking = backing->tiledBacking();
    if (!tiledBacking)
        return;

    ExtendedBackgroundMode existingMode = ExtendedBackgroundModeNone;
    if (tiledBacking->hasVerticalMargins())
        existingMode |= ExtendedBackgroundModeVertical;
    if (tiledBacking->hasHorizontalMargins())
        existingMode |= ExtendedBackgroundModeHorizontal;

    if (existingMode == mode)
        return;

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

bool FrameView::safeToPropagateScrollToParent() const
{
    auto* document = frame().document();
    if (!document)
        return false;

    auto* parentFrame = frame().tree().parent();
    if (!parentFrame)
        return false;

    auto* parentDocument = parentFrame->document();
    if (!parentDocument)
        return false;

    return document->securityOrigin().isSameOriginDomain(parentDocument->securityOrigin());
}

void FrameView::scrollToAnchor()
{
    RefPtr<ContainerNode> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;

    LOG_WITH_STREAM(Scrolling, stream << *this << " scrollToAnchor() " << anchorNode.get());

    if (!anchorNode->renderer())
        return;

    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();

    LayoutRect rect;
    bool insideFixed = false;
    if (anchorNode != frame().document() && anchorNode->renderer())
        rect = anchorNode->renderer()->absoluteAnchorRectWithScrollMargin(&insideFixed);

    LOG_WITH_STREAM(Scrolling, stream << " anchor node rect " << rect);

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    if (anchorNode->renderer()->style().isHorizontalWritingMode())
        anchorNode->renderer()->scrollRectToVisible(rect, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways, ShouldAllowCrossOriginScrolling::No });
    else if (anchorNode->renderer()->style().isFlippedBlocksWritingMode())
        anchorNode->renderer()->scrollRectToVisible(rect, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignRightAlways, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::No });
    else
        anchorNode->renderer()->scrollRectToVisible(rect, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignLeftAlways, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::No });

    if (AXObjectCache* cache = frame().document()->existingAXObjectCache())
        cache->handleScrolledToAnchor(anchorNode.get());

    // scrollRectToVisible can call into setScrollPosition(), which resets m_maintainScrollPositionAnchor.
    LOG_WITH_STREAM(Scrolling, stream << " restoring anchor node to " << anchorNode.get());
    m_maintainScrollPositionAnchor = anchorNode;
    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();
}

void FrameView::updateEmbeddedObject(RenderEmbeddedObject& embeddedObject)
{
    // No need to update if it's already crashed or known to be missing.
    if (embeddedObject.isPluginUnavailable())
        return;

    auto& element = embeddedObject.frameOwnerElement();

    WeakPtr weakRenderer { embeddedObject };

    if (is<HTMLPlugInImageElement>(element)) {
        auto& pluginElement = downcast<HTMLPlugInImageElement>(element);
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
    SetForScope<bool> inUpdateEmbeddedObjects(m_inUpdateEmbeddedObjects, true);
    if (layoutContext().isLayoutNested() || !m_embeddedObjectsToUpdate || m_embeddedObjectsToUpdate->isEmpty())
        return true;

    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    // Insert a marker for where we should stop.
    ASSERT(!m_embeddedObjectsToUpdate->contains(nullptr));
    m_embeddedObjectsToUpdate->add(nullptr);

    while (!m_embeddedObjectsToUpdate->isEmpty()) {
        auto embeddedObject = m_embeddedObjectsToUpdate->takeFirst();
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
    layoutContext().flushAsynchronousTasks();
    if (m_updateEmbeddedObjectsTimer.isActive())
        updateEmbeddedObjectsTimerFired();
}

void FrameView::queuePostLayoutCallback(Function<void()>&& callback)
{
    m_postLayoutCallbackQueue.append(WTFMove(callback));
}

void FrameView::flushPostLayoutTasksQueue()
{
    if (layoutContext().isLayoutNested())
        return;

    if (!m_postLayoutCallbackQueue.size())
        return;

    Vector<Function<void()>> queue = WTFMove(m_postLayoutCallbackQueue);
    for (auto& task : queue)
        task();
}

void FrameView::performPostLayoutTasks()
{
    // FIXME: We should not run any JavaScript code in this function.
    LOG(Layout, "FrameView %p performPostLayoutTasks", this);
    updateHasReachedSignificantRenderedTextThreshold();
    frame().selection().updateAppearanceAfterLayout();

    flushPostLayoutTasksQueue();

    if (!layoutContext().isLayoutNested() && frame().document()->documentElement())
        fireLayoutRelatedMilestonesIfNeeded();

#if PLATFORM(IOS_FAMILY)
    // Only send layout-related delegate callbacks synchronously for the main frame to
    // avoid re-entering layout for the main frame while delivering a layout-related delegate
    // callback for a subframe.
    if (frame().isMainFrame()) {
        if (Page* page = frame().page())
            page->chrome().client().didLayout();
    }
#endif
    
    // FIXME: We should consider adding DidLayout as a LayoutMilestone. That would let us merge this
    // with didLayout(LayoutMilestones).
    frame().loader().client().dispatchDidLayout();

    updateWidgetPositions();

    updateSnapOffsets();

    m_updateEmbeddedObjectsTimer.startOneShot(0_s);

    if (auto scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewLayoutUpdated(*this);

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidLayout();
    }

    scrollToAnchor();

    scheduleResizeEventIfNeeded();
    
    updateLayoutViewport();
    viewportContentsChanged();

    resnapAfterLayout();

    if (AXObjectCache* cache = frame().document()->existingAXObjectCache())
        cache->performDeferredCacheUpdate();

    if (auto* observer = frame().document()->modalContainerObserver())
        observer->updateModalContainerIfNeeded(*this);
}

IntSize FrameView::sizeForResizeEvent() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_customSizeForResizeEvent)
        return *m_customSizeForResizeEvent;
#endif
    if (useFixedLayout() && !fixedLayoutSize().isEmpty() && delegatesScrolling())
        return fixedLayoutSize();
    return visibleContentRectIncludingScrollbars().size();
}

void FrameView::scheduleResizeEventIfNeeded()
{
    if (layoutContext().isInRenderTreeLayout() || needsLayout())
        return;

    RenderView* renderView = this->renderView();
    if (!renderView || renderView->printing())
        return;

    auto* page = frame().page();
    if (page && page->chrome().client().isSVGImageChromeClient())
        return;

    IntSize currentSize = sizeForResizeEvent();
    float currentZoomFactor = renderView->style().zoom();

    if (currentSize == m_lastViewportSize && currentZoomFactor == m_lastZoomFactor)
        return;

    m_lastViewportSize = currentSize;
    m_lastZoomFactor = currentZoomFactor;

    if (!layoutContext().didFirstLayout())
        return;

#if PLATFORM(IOS_FAMILY)
    // Don't send the resize event if the document is loading. Some pages automatically reload
    // when the window is resized; Safari on iOS often resizes the window while setting up its
    // viewport. This obviously can cause problems.
    if (DocumentLoader* documentLoader = frame().loader().documentLoader()) {
        if (documentLoader->isLoadingInAPISense())
            return;
    }
#endif

    auto* document = frame().document();
    if (document->quirks().shouldSilenceWindowResizeEvents()) {
        FRAMEVIEW_RELEASE_LOG(Events, "scheduleResizeEventIfNeeded: Not firing resize events because they are temporarily disabled for this page");
        return;
    }

    LOG_WITH_STREAM(Events, stream << "FrameView " << this << " scheduleResizeEventIfNeeded scheduling resize event for document" << document << ", size " << currentSize);
    document->setNeedsDOMWindowResizeEvent();

    bool isMainFrame = frame().isMainFrame();
    if (InspectorInstrumentation::hasFrontends() && isMainFrame) {
        if (InspectorClient* inspectorClient = page ? page->inspectorController().inspectorClient() : nullptr)
            inspectorClient->didResizeMainFrame(&frame());
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

    auto* document = frame().document();
    if (!document)
        return;

    auto* renderView = document->renderView();
    if (!renderView)
        return;

    auto* firstChild = renderView->firstChild();
    if (!firstChild)
        return;

    SetForScope<bool> changeInAutoSize(m_inAutoSize, true);
    if (layoutContext().subtreeLayoutRoot())
        layoutContext().convertSubtreeLayoutToFullLayout();

    switch (m_autoSizeMode) {
    case AutoSizeMode::SizeToContent:
        performSizeToContentAutoSize();
        break;
    case AutoSizeMode::FixedWidth:
        performFixedWidthAutoSize();
        break;
    }

    if (auto* page = frame().page(); page && frame().isMainFrame())
        page->chrome().client().intrinsicContentsSizeChanged(m_autoSizeContentSize);

    m_didRunAutosize = true;
}

void FrameView::performFixedWidthAutoSize()
{
    LOG(Layout, "FrameView %p performFixedWidthAutoSize", this);

    auto* document = frame().document();
    auto* renderView = document->renderView();
    auto* firstChild = renderView->firstChild();

    auto horizonalScrollbarMode = ScrollbarMode::AlwaysOff;
    auto verticalScrollbarMode = ScrollbarMode::AlwaysOff;
    setVerticalScrollbarLock(false);
    setHorizontalScrollbarLock(false);
    setScrollbarModes(horizonalScrollbarMode, verticalScrollbarMode, true, true);

    ASSERT(is<RenderElement>(*firstChild));
    auto& documentRenderer = downcast<RenderElement>(*firstChild);
    documentRenderer.mutableStyle().setMaxWidth(Length(m_autoSizeConstraint.width(), LengthType::Fixed));
    resize(m_autoSizeConstraint.width(), m_autoSizeConstraint.height());

    Ref<FrameView> protectedThis(*this);
    document->updateStyleIfNeeded();
    document->updateLayoutIgnorePendingStylesheets();
    // While the final content size could slightly be different after the next resize/layout (see below), we intentionally save and report
    // the current value to avoid unstable layout (e.g. content "height: 100%").
    // See also webkit.org/b/173561
    m_autoSizeContentSize = contentsSize();

    auto finalWidth = std::max(m_autoSizeConstraint.width(), m_autoSizeContentSize.width());
    auto finalHeight = m_autoSizeFixedMinimumHeight ? std::max(m_autoSizeFixedMinimumHeight, m_autoSizeContentSize.height()) : m_autoSizeContentSize.height();
    resize(finalWidth, finalHeight);
    document->updateLayoutIgnorePendingStylesheets();
}

void FrameView::performSizeToContentAutoSize()
{
    // Do the resizing twice. The first time is basically a rough calculation using the preferred width
    // which may result in a height change during the second iteration.
    // Let's ignore renderers with viewport units first and resolve these boxes during the second phase of the autosizing.
    LOG(Layout, "FrameView %p performSizeToContentAutoSize", this);
    ASSERT(frame().document() && frame().document()->renderView());

    auto& document = *frame().document();
    auto& renderView = *document.renderView();
    auto layoutWithAdjustedStyleIfNeeded = [&] {
        document.updateStyleIfNeeded();
        if (auto* documentRenderer = downcast<RenderElement>(renderView.firstChild())) {
            auto& style = documentRenderer->mutableStyle();
            if (style.logicalHeight().isPercent()) {
                // Percent height values on the document renderer when we don't really have a proper viewport size can
                // result incorrect rendering in certain layout contexts (e.g flex).
                style.setLogicalHeight({ });
            }
        }
        document.updateLayout();
    };

    resetOverriddenWidthForCSSSmallViewportUnits();
    resetOverriddenWidthForCSSLargeViewportUnits();

    // Start from the minimum size and allow it to grow.
    auto minAutoSize = IntSize { 1, 1 };
    resize(minAutoSize);
    auto size = frameRect().size();
    for (int i = 0; i < 2; i++) {
        layoutWithAdjustedStyleIfNeeded();
        // Update various sizes including contentsSize, scrollHeight, etc.
        auto newSize = IntSize { renderView.minPreferredLogicalWidth(), renderView.documentRect().height() };

        // Check to see if a scrollbar is needed for a given dimension and
        // if so, increase the other dimension to account for the scrollbar.
        // Since the dimensions are only for the view rectangle, once a
        // dimension exceeds the maximum, there is no need to increase it further.
        if (newSize.width() > m_autoSizeConstraint.width()) {
            RefPtr<Scrollbar> localHorizontalScrollbar = horizontalScrollbar();
            if (!localHorizontalScrollbar)
                localHorizontalScrollbar = createScrollbar(ScrollbarOrientation::Horizontal);
            newSize.expand(0, localHorizontalScrollbar->occupiedHeight());
            // Don't bother checking for a vertical scrollbar because the width is at
            // already greater the maximum.
        } else if (newSize.height() > m_autoSizeConstraint.height()) {
            RefPtr<Scrollbar> localVerticalScrollbar = verticalScrollbar();
            if (!localVerticalScrollbar)
                localVerticalScrollbar = createScrollbar(ScrollbarOrientation::Vertical);
            newSize.expand(localVerticalScrollbar->occupiedWidth(), 0);
            // Don't bother checking for a horizontal scrollbar because the height is
            // already greater the maximum.
        }

        // Ensure the size is at least the min bounds.
        newSize = newSize.expandedTo(minAutoSize);

        // Bound the dimensions by the max bounds and determine what scrollbars to show.
        ScrollbarMode horizonalScrollbarMode = ScrollbarMode::AlwaysOff;
        if (newSize.width() > m_autoSizeConstraint.width()) {
            newSize.setWidth(m_autoSizeConstraint.width());
            horizonalScrollbarMode = ScrollbarMode::AlwaysOn;
        }
        ScrollbarMode verticalScrollbarMode = ScrollbarMode::AlwaysOff;
        if (newSize.height() > m_autoSizeConstraint.height()) {
            newSize.setHeight(m_autoSizeConstraint.height());
            verticalScrollbarMode = ScrollbarMode::AlwaysOn;
        }

        if (newSize == size)
            continue;

        // While loading only allow the size to increase (to avoid twitching during intermediate smaller states)
        // unless autoresize has just been turned on or the maximum size is smaller than the current size.
        if (m_didRunAutosize && size.height() <= m_autoSizeConstraint.height() && size.width() <= m_autoSizeConstraint.width()
            && !frame().loader().isComplete() && (newSize.height() < size.height() || newSize.width() < size.width()))
            break;

        // The first time around, resize to the minimum height again; otherwise,
        // on pages (e.g. quirks mode) where the body/document resize to the view size,
        // we'll end up not shrinking back down after resizing to the computed preferred width.
        resize(newSize.width(), i ? newSize.height() : minAutoSize.height());
        // Protect the content from evergrowing layout.
        auto preferredViewportWidth = std::min(newSize.width(), m_autoSizeConstraint.width());
        overrideWidthForCSSSmallViewportUnits(preferredViewportWidth);
        overrideWidthForCSSLargeViewportUnits(preferredViewportWidth);
        // Force the scrollbar state to avoid the scrollbar code adding them and causing them to be needed. For example,
        // a vertical scrollbar may cause text to wrap and thus increase the height (which is the only reason the scollbar is needed).
        setVerticalScrollbarLock(false);
        setHorizontalScrollbarLock(false);
        setScrollbarModes(horizonalScrollbarMode, verticalScrollbarMode, true, true);
    }
    // All the resizing above may have invalidated style (for example if viewport units are being used).
    // FIXME: Use the final layout's result as the content size (webkit.org/b/173561).
    layoutWithAdjustedStyleIfNeeded();
    m_autoSizeContentSize = contentsSize();
}

void FrameView::setAutoSizeFixedMinimumHeight(int fixedMinimumHeight)
{
    if (m_autoSizeFixedMinimumHeight == fixedMinimumHeight)
        return;

    m_autoSizeFixedMinimumHeight = fixedMinimumHeight;

    setNeedsLayoutAfterViewConfigurationChange();
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

    frame().document()->styleScope().didChangeStyleSheetEnvironment();
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
    return page && page->settings().scrollingPerformanceTestingEnabled();
}

void FrameView::scrollTo(const ScrollPosition& newPosition)
{
    IntPoint oldPosition = scrollPosition();
    ScrollView::scrollTo(newPosition);
    if (oldPosition != scrollPosition())
        scrollPositionChanged(oldPosition, scrollPosition());

    didChangeScrollOffset();
}

void FrameView::scrollToPositionWithAnimation(const ScrollPosition& position, ScrollType scrollType, ScrollClamping)
{
    // FIXME: Why isn't all this in ScrollableArea?
    auto previousScrollType = currentScrollType();
    setCurrentScrollType(scrollType);

    if (scrollAnimationStatus() == ScrollAnimationStatus::Animating)
        scrollAnimator().cancelAnimations();

    if (position != scrollPosition())
        ScrollableArea::scrollToPositionWithAnimation(position);

    setCurrentScrollType(previousScrollType);
}

float FrameView::adjustVerticalPageScrollStepForFixedContent(float step)
{
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
        if (style.position() != PositionType::Fixed || style.visibility() == Visibility::Hidden || !style.opacity())
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
    if (!frame().isMainFrame())
        return 1;

    Page* page = frame().page();
    // FIXME: This !delegatesScaling() is confusing, and the opposite behavior to Frame::frameScaleFactor().
    // This function should probably be renamed to delegatedPageScaleFactor().
    if (!page || !page->delegatesScaling())
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
    if (frame().isMainFrame())
        return nullptr;

    auto* ownerElement = frame().ownerElement();
    if (!ownerElement)
        return nullptr;

    auto* ownerRenderer = ownerElement->renderer();
    if (!ownerRenderer)
        return nullptr;

    auto* layer = ownerRenderer->enclosingLayer();
    if (!layer)
        return nullptr;

    auto* enclosingScrollableLayer = layer->enclosingScrollableLayer(IncludeSelfOrNot::IncludeSelf, CrossFrameBoundaries::No);
    if (!enclosingScrollableLayer)
        return nullptr;

    return enclosingScrollableLayer->scrollableArea();
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
    if (!didFirstLayout())
        return false;

    bool requiresActualOverflowToBeConsideredScrollable = !frame().isMainFrame() || definitionOfScrollable != Scrollability::ScrollableOrRubberbandable;
#if !HAVE(RUBBER_BANDING)
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
    if (horizontalMode == ScrollbarMode::AlwaysOff && verticalMode == ScrollbarMode::AlwaysOff)
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
    return frame().loader().state() != FrameState::Complete;
}

void FrameView::scrollbarStyleChanged(ScrollbarStyle newStyle, bool forceUpdate)
{
    if (!frame().isMainFrame())
        return;

    if (Page* page = frame().page())
        page->chrome().client().recommendedScrollbarStyleDidChange(newStyle);

    ScrollView::scrollbarStyleChanged(newStyle, forceUpdate);
}

void FrameView::notifyAllFramesThatContentAreaWillPaint() const
{
    notifyScrollableAreasThatContentAreaWillPaint();

    for (auto* child = frame().tree().firstRenderedChild(); child; child = child->tree().traverseNextRendered(m_frame.ptr())) {
        if (auto* frameView = child->view())
            frameView->notifyScrollableAreasThatContentAreaWillPaint();
    }
}

void FrameView::notifyScrollableAreasThatContentAreaWillPaint() const
{
    Page* page = frame().page();
    if (!page)
        return;

    contentAreaWillPaint();

    if (!m_scrollableAreas)
        return;

    for (auto& scrollableArea : *m_scrollableAreas) {
        // ScrollView ScrollableAreas will be handled via the Frame tree traversal above.
        if (!is<ScrollView>(scrollableArea))
            scrollableArea->contentAreaWillPaint();
    }
}

bool FrameView::scrollAnimatorEnabled() const
{
    if (auto* page = frame().page())
        return page->settings().scrollAnimatorEnabled();

    return false;
}

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
            cornerStyle = renderer->getUncachedPseudoStyle({ PseudoId::ScrollbarCorner }, &renderer->style());
        }
        
        if (!cornerStyle) {
            // If the <body> didn't have a custom style, then the root element might.
            Element* docElement = doc ? doc->documentElement() : nullptr;
            if (docElement && docElement->renderer()) {
                renderer = docElement->renderer();
                cornerStyle = renderer->getUncachedPseudoStyle({ PseudoId::ScrollbarCorner }, &renderer->style());
            }
        }
        
        if (!cornerStyle) {
            // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
            // FIXME: Seems wrong to do this for cross-origin frames.
            if (RenderWidget* renderer = frame().ownerRenderer())
                cornerStyle = renderer->getUncachedPseudoStyle({ PseudoId::ScrollbarCorner }, &renderer->style());
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
}

void FrameView::paintScrollCorner(GraphicsContext& context, const IntRect& cornerRect)
{
    if (context.invalidatingControlTints()) {
        updateScrollCorner();
        return;
    }

    if (m_scrollCorner) {
        if (frame().isMainFrame())
            context.fillRect(cornerRect, baseBackgroundColor());
        m_scrollCorner->paintIntoRect(context, cornerRect.location(), cornerRect);
        return;
    }

#if PLATFORM(MAC)
    // Keep this in sync with ScrollAnimatorMac's effectiveAppearanceForScrollerImp:.
    LocalDefaultSystemAppearance localAppearance(useDarkAppearanceForScrollbars());
#endif

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
        htmlBackgroundColor = htmlElement->renderer()->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (bodyElement && bodyElement->renderer())
        bodyBackgroundColor = bodyElement->renderer()->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

    if (!bodyBackgroundColor.isValid()) {
        if (!htmlBackgroundColor.isValid())
            return Color();
        return blendSourceOver(baseBackgroundColor(), htmlBackgroundColor);
    }

    if (!htmlBackgroundColor.isValid())
        return blendSourceOver(baseBackgroundColor(), bodyBackgroundColor);

    // We take the aggregate of the base background color
    // the <html> background color, and the <body>
    // background color to find the document color. The
    // addition of the base background color is not
    // technically part of the document background, but it
    // otherwise poses problems when the aggregate is not
    // fully opaque.
    return blendSourceOver(blendSourceOver(baseBackgroundColor(), htmlBackgroundColor), bodyBackgroundColor);
}

bool FrameView::hasCustomScrollbars() const
{
    for (auto& widget : children()) {
        if (is<FrameView>(widget)) {
            if (downcast<FrameView>(widget.get()).hasCustomScrollbars())
                return true;
        } else if (is<Scrollbar>(widget)) {
            if (downcast<Scrollbar>(widget.get()).isCustomScrollbar())
                return true;
        }
    }
    return false;
}

FrameView* FrameView::parentFrameView() const
{
    if (!parent())
        return nullptr;
    auto* parentFrame = frame().tree().parent();
    if (!parentFrame)
        return nullptr;
    return parentFrame->view();
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
        invalidateControlTints();

    if (page)
        page->setIsCountingRelevantRepaintedObjects(isCurrentlyCountingRelevantRepaintedObject);
}

void FrameView::invalidateControlTints()
{
    traverseForPaintInvalidation(NullGraphicsContext::PaintInvalidationReasons::InvalidatingControlTints);
}

void FrameView::invalidateImagesWithAsyncDecodes()
{
    traverseForPaintInvalidation(NullGraphicsContext::PaintInvalidationReasons::InvalidatingImagesWithAsyncDecodes);
}

void FrameView::traverseForPaintInvalidation(NullGraphicsContext::PaintInvalidationReasons paintInvalidationReasons)
{
    if (needsLayout())
        layoutContext().layout();

    NullGraphicsContext context(paintInvalidationReasons);
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
    LOG(Scrolling, "FrameView::setWasScrolledByUser at %d", wasScrolledByUser);

    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();
    if (currentScrollType() == ScrollType::Programmatic)
        return;
    m_maintainScrollPositionAnchor = nullptr;
    if (m_wasScrolledByUser == wasScrolledByUser)
        return;
    m_wasScrolledByUser = wasScrolledByUser;
    adjustTiledBackingCoverage();
}

void FrameView::willPaintContents(GraphicsContext& context, const IntRect&, PaintingState& paintingState)
{
    Document* document = frame().document();

    if (!context.paintingDisabled())
        InspectorInstrumentation::willPaint(*renderView());

    paintingState.isTopLevelPainter = !sCurrentPaintTimeStamp;

    if (paintingState.isTopLevelPainter)
        sCurrentPaintTimeStamp = MonotonicTime::now();

    paintingState.paintBehavior = m_paintBehavior;
    
    if (FrameView* parentView = parentFrameView()) {
        if (parentView->paintBehavior() & PaintBehavior::FlattenCompositingLayers)
            m_paintBehavior.add(PaintBehavior::FlattenCompositingLayers);
        
        if (parentView->paintBehavior() & PaintBehavior::Snapshotting)
            m_paintBehavior.add(PaintBehavior::Snapshotting);
        
        if (parentView->paintBehavior() & PaintBehavior::TileFirstPaint)
            m_paintBehavior.add(PaintBehavior::TileFirstPaint);
    }

    if (document->printing()) {
        m_paintBehavior.add(PaintBehavior::FlattenCompositingLayers);
        m_paintBehavior.add(PaintBehavior::Snapshotting);
    }

    paintingState.isFlatteningPaintOfRootFrame = (m_paintBehavior & PaintBehavior::FlattenCompositingLayers) && !frame().ownerElement() && !context.detectingContentfulPaint();
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
    m_lastPaintTime = MonotonicTime::now();

    if (paintingState.isTopLevelPainter)
        sCurrentPaintTimeStamp = MonotonicTime();

    if (!context.paintingDisabled()) {
        InspectorInstrumentation::didPaint(*renderView(), dirtyRect);
        // FIXME: should probably not fire milestones for snapshot painting. https://bugs.webkit.org/show_bug.cgi?id=117623
        firePaintRelatedMilestonesIfNeeded();
    }
}

void FrameView::paintContents(GraphicsContext& context, const IntRect& dirtyRect, SecurityOriginPaintPolicy securityOriginPaintPolicy, EventRegionContext* eventRegionContext)
{
#ifndef NDEBUG
    bool fillWithWarningColor;
    if (frame().document()->printing())
        fillWithWarningColor = false; // Printing, don't fill with red (can't remember why).
    else if (frame().ownerElement())
        fillWithWarningColor = false; // Subframe, don't fill with red.
    else if (isTransparent())
        fillWithWarningColor = false; // Transparent, don't fill with red.
    else if (m_paintBehavior & PaintBehavior::SelectionOnly)
        fillWithWarningColor = false; // Selections are transparent, don't fill with red.
    else if (m_nodeToDraw)
        fillWithWarningColor = false; // Element images are transparent, don't fill with red.
    else
        fillWithWarningColor = true;

    if (fillWithWarningColor) {
        IntRect debugRect = frameRect();
        debugRect.intersect(dirtyRect);
        context.fillRect(debugRect, SRGBA<uint8_t> { 255, 64, 255 });
    }
#endif

    RenderView* renderView = this->renderView();
    if (!renderView) {
        LOG_ERROR("called FrameView::paint with nil renderer");
        return;
    }

    if (!layoutContext().inPaintableState())
        return;

    ASSERT(!needsLayout());
    if (needsLayout()) {
        FRAMEVIEW_RELEASE_LOG(Layout, "paintContents: Not painting because render tree needs layout");
        return;
    }

    PaintingState paintingState;
    willPaintContents(context, dirtyRect, paintingState);

    // m_nodeToDraw is used to draw only one element (and its descendants)
    RenderObject* renderer = m_nodeToDraw ? m_nodeToDraw->renderer() : nullptr;
    RenderLayer* rootLayer = renderView->layer();

    RenderObject::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(rootLayer->renderer());

    rootLayer->paint(context, dirtyRect, LayoutSize(), m_paintBehavior, renderer, { }, securityOriginPaintPolicy == SecurityOriginPaintPolicy::AnyOrigin ? RenderLayer::SecurityOriginPaintPolicy::AnyOrigin : RenderLayer::SecurityOriginPaintPolicy::AccessibleOriginOnly, eventRegionContext);
    if (auto* scrollableRootLayer = rootLayer->scrollableArea()) {
        if (scrollableRootLayer->containsDirtyOverlayScrollbars() && !eventRegionContext)
            scrollableRootLayer->paintOverlayScrollbars(context, dirtyRect, m_paintBehavior, renderer);
    }

    didPaintContents(context, dirtyRect, paintingState);
}

void FrameView::setPaintBehavior(OptionSet<PaintBehavior> behavior)
{
    m_paintBehavior = behavior;
}

OptionSet<PaintBehavior> FrameView::paintBehavior() const
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
    auto oldBehavior = paintBehavior();
    setPaintBehavior(oldBehavior | PaintBehavior::FlattenCompositingLayers | PaintBehavior::Snapshotting);

    // If the snapshot should exclude selection, then we'll clear the current selection
    // in the render tree only. This will allow us to restore the selection from the DOM
    // after we paint the snapshot.
    if (shouldPaintSelection == ExcludeSelection) {
        for (auto* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
            if (auto* renderView = frame->contentRenderer())
                renderView->selection().clear();
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

void FrameView::updateLayoutAndStyleIfNeededRecursive()
{
    // Style updating, render tree creation, and layout needs to be done multiple times
    // for more than one reason. But one reason is that when an <object> element determines
    // what it needs to load a subframe, a second pass is needed. That requires update
    // passes equal to the number of levels of DOM nesting. That is why this number is large.
    // There are test cases where we have roughly 10 levels of DOM nesting, so this needs to
    // be greater than that. We have a limit to avoid the possibility of an infinite loop.
    // Typical calls will run the loop 2 times (once to do work, once to detect no further work
    // is needed).
    // FIXME: We should find an approach that does not require a loop at all.
    const unsigned maxUpdatePasses = 25;

    // Style updates can trigger script, which can cause this FrameView to be destroyed.
    Ref<FrameView> protectedThis(*this);

    using DescendantsDeque = Deque<Ref<FrameView>, 16>;
    auto nextRenderedDescendant = [this] (DescendantsDeque& descendantsDeque) -> RefPtr<FrameView> {
        if (descendantsDeque.isEmpty())
            descendantsDeque.append(*this);
        else {
            // Append renderered children after processing the parent, in case the processing
            // affects the set of rendered children.
            auto previousView = descendantsDeque.takeFirst();
            for (auto* frame = previousView->frame().tree().firstRenderedChild(); frame; frame = frame->tree().nextRenderedSibling()) {
                if (auto* view = frame->view())
                    descendantsDeque.append(*view);
            }
            if (descendantsDeque.isEmpty())
                return nullptr;
        }
        return descendantsDeque.first().ptr();
    };

    for (unsigned i = 0; i < maxUpdatePasses; ++i) {
        bool didWork = false;
        DescendantsDeque deque;
        while (auto view = nextRenderedDescendant(deque)) {
            if (view->frame().document()->updateStyleIfNeeded())
                didWork = true;
            if (view->needsLayout()) {
                view->layoutContext().layout();
                view->frame().document()->styleScope().updateQueryContainerState();
                didWork = true;
            }
        }
        if (!didWork)
            break;
    }

#if ASSERT_ENABLED
    auto needsStyleRecalc = [&] {
        DescendantsDeque deque;
        while (auto view = nextRenderedDescendant(deque)) {
            auto* document = view->frame().document();
            if (document && document->childNeedsStyleRecalc())
                return true;
        }
        return false;
    };

    auto needsLayout = [&] {
        DescendantsDeque deque;
        while (auto view = nextRenderedDescendant(deque)) {
            if (view->needsLayout())
                return true;
        }
        return false;
    };
#endif // ASSERT_ENABLED

    ASSERT(!needsStyleRecalc());
    ASSERT(!needsLayout());
}

void FrameView::incrementVisuallyNonEmptyCharacterCount(const String& inlineText)
{
    if (m_visuallyNonEmptyCharacterCount > visualCharacterThreshold && m_hasReachedSignificantRenderedTextThreshold)
        return;

    auto nonWhitespaceLength = [](auto& inlineText) {
        auto length = inlineText.length();
        for (unsigned i = 0; i < inlineText.length(); ++i) {
            if (isNotHTMLSpace(inlineText[i]))
                continue;
            --length;
        }
        return length;
    };
    m_visuallyNonEmptyCharacterCount += nonWhitespaceLength(inlineText);
    ++m_textRendererCountForVisuallyNonEmptyCharacters;
}

void FrameView::updateHasReachedSignificantRenderedTextThreshold()
{
    if (m_hasReachedSignificantRenderedTextThreshold)
        return;

    auto* page = frame().page();
    if (!page || !page->requestedLayoutMilestones().contains(DidRenderSignificantAmountOfText))
        return;

    auto* document = frame().document();
    if (!document)
        return;

    document->updateMainArticleElementAfterLayout();
    auto hasMainArticleElement = document->hasMainArticleElement();
    auto characterThreshold = hasMainArticleElement ? mainArticleSignificantRenderedTextCharacterThreshold : defaultSignificantRenderedTextCharacterThreshold;
    if (m_visuallyNonEmptyCharacterCount < characterThreshold)
        return;

    auto meanLength = hasMainArticleElement ? mainArticleSignificantRenderedTextMeanLength : defaultSignificantRenderedTextMeanLength;
    if (!m_textRendererCountForVisuallyNonEmptyCharacters || m_visuallyNonEmptyCharacterCount / static_cast<float>(m_textRendererCountForVisuallyNonEmptyCharacters) < meanLength)
        return;

    m_hasReachedSignificantRenderedTextThreshold = true;
}

bool FrameView::qualifiesAsSignificantRenderedText() const
{
    ASSERT(!m_renderedSignificantAmountOfText);
    auto* document = frame().document();
    if (!document || document->styleScope().hasPendingSheetsBeforeBody())
        return false;
    return m_hasReachedSignificantRenderedTextThreshold;
}

void FrameView::checkAndDispatchDidReachVisuallyNonEmptyState()
{
    auto qualifiesAsVisuallyNonEmpty = [&] {
        // No content yet.
        auto& document = *frame().document();
        auto* documentElement = document.documentElement();
        if (!documentElement || !documentElement->renderer())
            return false;

        if (document.hasVisuallyNonEmptyCustomContent())
            return true;

        // FIXME: We should also ignore renderers with non-final style.
        if (document.styleScope().hasPendingSheetsBeforeBody())
            return false;

        auto finishedParsingMainDocument = frame().loader().stateMachine().committedFirstRealDocumentLoad() && (document.readyState() == Document::Interactive || document.readyState() == Document::Complete);
        // Ensure that we always fire visually non-empty milestone eventually.
        if (finishedParsingMainDocument && frame().loader().isComplete())
            return true;

        auto isVisible = [](const Element* element) {
            if (!element || !element->renderer())
                return false;
            if (!element->renderer()->opacity())
                return false;
            return element->renderer()->style().visibility() == Visibility::Visible;
        };

        if (!isVisible(documentElement))
            return false;

        if (!isVisible(document.body()))
            return false;

        // The first few hundred characters rarely contain the interesting content of the page.
        if (m_visuallyNonEmptyCharacterCount > visualCharacterThreshold)
            return true;

        // Use a threshold value to prevent very small amounts of visible content from triggering didFirstVisuallyNonEmptyLayout
        if (m_visuallyNonEmptyPixelCount > visualPixelThreshold)
            return true;

        auto isMoreContentExpected = [&]() {
            ASSERT(finishedParsingMainDocument);
            // Pending css/font loading means we should wait a little longer. Classic non-async, non-defer scripts are all processed by now.
            auto* documentLoader = frame().loader().documentLoader();
            if (!documentLoader)
                return false;

            auto& resourceLoader = documentLoader->cachedResourceLoader();
            if (!resourceLoader.requestCount())
                return false;

            auto& resources = resourceLoader.allCachedResources();
            for (auto& resource : resources) {
                if (resource.value->isLoaded())
                    continue;
                if (resource.value->type() == CachedResource::Type::CSSStyleSheet || resource.value->type() == CachedResource::Type::FontResource)
                    return true;
            }
            return false;
        };

        // Finished parsing the main document and we still don't yet have enough content. Check if we might be getting some more.
        if (finishedParsingMainDocument)
            return !isMoreContentExpected();

        return false;
    };

    if (m_contentQualifiesAsVisuallyNonEmpty)
        return;

    if (!qualifiesAsVisuallyNonEmpty())
        return;

    m_contentQualifiesAsVisuallyNonEmpty = true;
    if (frame().isMainFrame())
        frame().loader().didReachVisuallyNonEmptyState();
}

bool FrameView::hasContentfulDescendants() const
{
    return m_visuallyNonEmptyCharacterCount || m_visuallyNonEmptyPixelCount;
}

bool FrameView::isViewForDocumentInFrame() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    return &renderView->frameView() == this;
}

void FrameView::enableFixedWidthAutoSizeMode(bool enable, const IntSize& viewSize)
{
    // FIXME: Figure out if this flavor of the autosizing should run only on the mainframe.
    enableAutoSizeMode(enable, viewSize, AutoSizeMode::FixedWidth);
}

void FrameView::enableSizeToContentAutoSizeMode(bool enable, const IntSize& viewSize)
{
    ASSERT(frame().isMainFrame());
    enableAutoSizeMode(enable, viewSize, AutoSizeMode::SizeToContent);
}

void FrameView::enableAutoSizeMode(bool enable, const IntSize& viewSize, AutoSizeMode mode)
{
    ASSERT(!enable || !viewSize.isEmpty());
    if (m_shouldAutoSize == enable && m_autoSizeConstraint == viewSize)
        return;

    m_autoSizeMode = mode;
    m_shouldAutoSize = enable;
    m_autoSizeConstraint = viewSize;
    m_autoSizeContentSize = contentsSize();
    m_didRunAutosize = false;

    setNeedsLayoutAfterViewConfigurationChange();
    layoutContext().scheduleLayout();
    if (m_shouldAutoSize) {
        overrideWidthForCSSSmallViewportUnits(m_autoSizeConstraint.width());
        overrideWidthForCSSLargeViewportUnits(m_autoSizeConstraint.width());
        return;
    }

    clearSizeOverrideForCSSSmallViewportUnits();
    clearSizeOverrideForCSSLargeViewportUnits();
    // Since autosize mode forces the scrollbar mode, change them to being auto.
    setVerticalScrollbarLock(false);
    setHorizontalScrollbarLock(false);
    setScrollbarModes(ScrollbarMode::Auto, ScrollbarMode::Auto);
}

void FrameView::forceLayout(bool allowSubtreeLayout)
{
    if (!allowSubtreeLayout && layoutContext().subtreeLayoutRoot())
        layoutContext().convertSubtreeLayoutToFullLayout();
    layoutContext().layout();
}

void FrameView::forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor, AdjustViewSizeOrNot shouldAdjustViewSize)
{
    if (!renderView())
        return;

    Ref<FrameView> protectedThis(*this);
    auto& renderView = *this->renderView();

    // Dumping externalRepresentation(frame().renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    float pageLogicalWidth = renderView.style().isHorizontalWritingMode() ? pageSize.width() : pageSize.height();
    float pageLogicalHeight = renderView.style().isHorizontalWritingMode() ? pageSize.height() : pageSize.width();

    renderView.setPageLogicalSize({ floor(pageLogicalWidth), floor(pageLogicalHeight) });
    renderView.setNeedsLayoutAndPrefWidthsRecalc();
    forceLayout();
    if (hasOneRef())
        return;

    // If we don't fit in the given page width, we'll lay out again. If we don't fit in the
    // page width when shrunk, we will lay out at maximum shrink and clip extra content.
    // FIXME: We are assuming a shrink-to-fit printing implementation. A cropping
    // implementation should not do this!
    bool horizontalWritingMode = renderView.style().isHorizontalWritingMode();
    const LayoutRect& documentRect = renderView.documentRect();
    LayoutUnit docLogicalWidth = horizontalWritingMode ? documentRect.width() : documentRect.height();
    if (docLogicalWidth > pageLogicalWidth) {
        int expectedPageWidth = std::min<float>(documentRect.width(), pageSize.width() * maximumShrinkFactor);
        int expectedPageHeight = std::min<float>(documentRect.height(), pageSize.height() * maximumShrinkFactor);
        FloatSize maxPageSize = frame().resizePageRectsKeepingRatio(FloatSize(originalPageSize.width(), originalPageSize.height()), FloatSize(expectedPageWidth, expectedPageHeight));
        pageLogicalWidth = horizontalWritingMode ? maxPageSize.width() : maxPageSize.height();
        pageLogicalHeight = horizontalWritingMode ? maxPageSize.height() : maxPageSize.width();

        renderView.setPageLogicalSize({ floor(pageLogicalWidth), floor(pageLogicalHeight) });
        renderView.setNeedsLayoutAndPrefWidthsRecalc();
        forceLayout();
        if (hasOneRef())
            return;

        const LayoutRect& updatedDocumentRect = renderView.documentRect();
        LayoutUnit docLogicalHeight = horizontalWritingMode ? updatedDocumentRect.height() : updatedDocumentRect.width();
        LayoutUnit docLogicalTop = horizontalWritingMode ? updatedDocumentRect.y() : updatedDocumentRect.x();
        LayoutUnit docLogicalRight = horizontalWritingMode ? updatedDocumentRect.maxX() : updatedDocumentRect.maxY();
        LayoutUnit clippedLogicalLeft;
        if (!renderView.style().isLeftToRightDirection())
            clippedLogicalLeft = docLogicalRight - pageLogicalWidth;
        LayoutRect overflow { clippedLogicalLeft, docLogicalTop, LayoutUnit(pageLogicalWidth), docLogicalHeight };

        if (!horizontalWritingMode)
            overflow = overflow.transposedRect();
        renderView.clearLayoutOverflow();
        renderView.addLayoutOverflow(overflow); // This is how we clip in case we overflow again.
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
    NullGraphicsContext context(NullGraphicsContext::PaintInvalidationReasons::None);
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

    return contentsToView(rect);
}

IntRect FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const IntRect& viewRect) const
{
    IntRect rect = viewToContents(viewRect);

    // FIXME: we don't have a way to map an absolute rect down to a local quad, so just
    // move the rect for now.
    rect.setLocation(roundedIntPoint(renderer->absoluteToLocal(rect.location(), UseTransforms)));
    return rect;
}

FloatRect FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const FloatRect& viewRect) const
{
    FloatRect rect = viewToContents(viewRect);

    return (renderer->absoluteToLocalQuad(rect)).boundingBox();
}

IntPoint FrameView::convertFromRendererToContainingView(const RenderElement* renderer, const IntPoint& rendererPoint) const
{
    IntPoint point = roundedIntPoint(renderer->localToAbsolute(rendererPoint, UseTransforms));

    return contentsToView(point);
}

FloatPoint FrameView::convertFromRendererToContainingView(const RenderElement* renderer, const FloatPoint& rendererPoint) const
{
    return contentsToView(renderer->localToAbsolute(rendererPoint, UseTransforms));
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
                
            auto rect = localRect;
            rect.moveBy(roundedIntPoint(renderer->contentBoxLocation()));
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

            auto rect = parentView.convertFromContainingViewToRenderer(renderer, parentRect);
            rect.moveBy(-roundedIntPoint(renderer->contentBoxLocation()));
            return rect;
        }
        
        return Widget::convertFromContainingView(parentRect);
    }
    
    return parentRect;
}

FloatRect FrameView::convertFromContainingView(const FloatRect& parentRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (is<FrameView>(*parentScrollView)) {
            const FrameView& parentView = downcast<FrameView>(*parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return parentRect;

            auto rect = parentView.convertFromContainingViewToRenderer(renderer, parentRect);
            rect.moveBy(-renderer->contentBoxLocation());
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
                
            auto point = localPoint;
            point.moveBy(roundedIntPoint(renderer->contentBoxLocation()));
            return parentView.convertFromRendererToContainingView(renderer, point);
        }
        
        return Widget::convertToContainingView(localPoint);
    }
    
    return localPoint;
}

FloatPoint FrameView::convertToContainingView(const FloatPoint& localPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (is<FrameView>(*parentScrollView)) {
            const FrameView& parentView = downcast<FrameView>(*parentScrollView);

            // Get our renderer in the parent view
            RenderWidget* renderer = frame().ownerRenderer();
            if (!renderer)
                return localPoint;

            auto point = localPoint;
            point.moveBy(renderer->contentBoxLocation());
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

            auto point = parentView.convertFromContainingViewToRenderer(renderer, parentPoint);
            point.moveBy(-roundedIntPoint(renderer->contentBoxLocation()));
            return point;
        }
        
        return Widget::convertFromContainingView(parentPoint);
    }
    
    return parentPoint;
}

float FrameView::documentToAbsoluteScaleFactor(std::optional<float> effectiveZoom) const
{
    // If effectiveZoom is passed, it already factors in pageZoomFactor(). 
    return effectiveZoom.value_or(frame().pageZoomFactor()) * frame().frameScaleFactor();
}

float FrameView::absoluteToDocumentScaleFactor(std::optional<float> effectiveZoom) const
{
    // If effectiveZoom is passed, it already factors in pageZoomFactor(). 
    return 1 / documentToAbsoluteScaleFactor(effectiveZoom);
}

FloatRect FrameView::absoluteToDocumentRect(FloatRect rect, std::optional<float> effectiveZoom) const
{
    rect.scale(absoluteToDocumentScaleFactor(effectiveZoom));
    return rect;
}

FloatPoint FrameView::absoluteToDocumentPoint(FloatPoint p, std::optional<float> effectiveZoom) const
{
    return p.scaled(absoluteToDocumentScaleFactor(effectiveZoom));
}

FloatRect FrameView::absoluteToClientRect(FloatRect rect, std::optional<float> effectiveZoom) const
{
    return documentToClientRect(absoluteToDocumentRect(rect, effectiveZoom));
}

FloatSize FrameView::documentToClientOffset() const
{
    FloatSize clientOrigin = -toFloatSize(visibleContentRect().location());

    // Layout and visual viewports are affected by page zoom, so we need to factor that out.
    return clientOrigin.scaled(1 / (frame().pageZoomFactor() * frame().frameScaleFactor()));
}

FloatRect FrameView::documentToClientRect(FloatRect rect) const
{
    rect.move(documentToClientOffset());
    return rect;
}

FloatPoint FrameView::documentToClientPoint(FloatPoint p) const
{
    p.move(documentToClientOffset());
    return p;
}

FloatRect FrameView::clientToDocumentRect(FloatRect rect) const
{
    rect.move(-documentToClientOffset());
    return rect;
}

FloatPoint FrameView::clientToDocumentPoint(FloatPoint point) const
{
    point.move(-documentToClientOffset());
    return point;
}

FloatPoint FrameView::absoluteToLayoutViewportPoint(FloatPoint p) const
{
    ASSERT(frame().settings().visualViewportEnabled());
    p.scale(1 / frame().frameScaleFactor());
    p.moveBy(-layoutViewportRect().location());
    return p;
}

FloatPoint FrameView::layoutViewportToAbsolutePoint(FloatPoint p) const
{
    ASSERT(frame().settings().visualViewportEnabled());
    p.moveBy(layoutViewportRect().location());
    return p.scaled(frame().frameScaleFactor());
}

FloatRect FrameView::layoutViewportToAbsoluteRect(FloatRect rect) const
{
    ASSERT(frame().settings().visualViewportEnabled());
    rect.moveBy(layoutViewportRect().location());
    rect.scale(frame().frameScaleFactor());
    return rect;
}

FloatRect FrameView::absoluteToLayoutViewportRect(FloatRect rect) const
{
    ASSERT(frame().settings().visualViewportEnabled());
    rect.scale(1 / frame().frameScaleFactor());
    rect.moveBy(-layoutViewportRect().location());
    return rect;
}

FloatRect FrameView::clientToLayoutViewportRect(FloatRect rect) const
{
    ASSERT(frame().settings().visualViewportEnabled());
    rect.scale(frame().pageZoomFactor());
    return rect;
}

FloatPoint FrameView::clientToLayoutViewportPoint(FloatPoint p) const
{
    ASSERT(frame().settings().visualViewportEnabled());
    return p.scaled(frame().pageZoomFactor());
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
    Frame& frame = this->frame();
    Ref<Frame> protector(frame);

    if (auto* document = frame.document())
        document->updateLayout();

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
        m_scrollableAreas = makeUnique<ScrollableAreaSet>();
    
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
    if (auto scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewEventTrackingRegionsChanged(*this);
}

void FrameView::scheduleScrollEvent()
{
    frame().eventHandler().scheduleScrollEvent();
    frame().eventHandler().dispatchFakeMouseMoveEventSoon();
}

void FrameView::addChild(Widget& widget)
{
    if (is<FrameView>(widget)) {
        auto& childFrameView = downcast<FrameView>(widget);
        if (childFrameView.isScrollable())
            addScrollableArea(&childFrameView);
    }

    ScrollView::addChild(widget);
}

void FrameView::removeChild(Widget& widget)
{
    if (is<FrameView>(widget))
        removeScrollableArea(&downcast<FrameView>(widget));

    ScrollView::removeChild(widget);
}

bool FrameView::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState> gestureState)
{
    // Note that to allow for rubber-band over-scroll behavior, even non-scrollable views
    // should handle wheel events.
#if !HAVE(RUBBER_BANDING)
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
    if (auto scrollingCoordinator = this->scrollingCoordinator()) {
        if (scrollingCoordinator->coordinatesScrollingForFrameView(*this))
            return scrollingCoordinator->handleWheelEventForScrolling(wheelEvent, scrollingNodeID(), gestureState);
    }
#endif

    return ScrollableArea::handleWheelEventForScrolling(wheelEvent, gestureState);
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

#if PLATFORM(IOS_FAMILY)
bool FrameView::useCustomFixedPositionLayoutRect() const
{
    return !frame().settings().visualViewportEnabled() && m_useCustomFixedPositionLayoutRect;
}

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
    m_customSizeForResizeEvent = customSize;
    scheduleResizeEventIfNeeded();
}

void FrameView::setScrollVelocity(const VelocityData& velocityData)
{
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setVelocity(velocityData);
}
#endif // PLATFORM(IOS_FAMILY)

void FrameView::setScrollingPerformanceTestingEnabled(bool scrollingPerformanceTestingEnabled)
{
    if (scrollingPerformanceTestingEnabled) {
        auto* page = frame().page();
        if (page && page->performanceLoggingClient())
            page->performanceLoggingClient()->logScrollingEvent(PerformanceLoggingClient::ScrollingEvent::LoggingEnabled, MonotonicTime::now(), 0);
    }

    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setScrollingPerformanceTestingEnabled(scrollingPerformanceTestingEnabled);
}

void FrameView::didAddScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    ScrollableArea::didAddScrollbar(scrollbar, orientation);
    Page* page = frame().page();
    if (page && page->isMonitoringWheelEvents())
        scrollAnimator().setWheelEventTestMonitor(page->wheelEventTestMonitor());
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

void FrameView::addPaintPendingMilestones(OptionSet<LayoutMilestone> milestones)
{
    m_milestonesPendingPaint.add(milestones);
}

void FrameView::fireLayoutRelatedMilestonesIfNeeded()
{
    OptionSet<LayoutMilestone> requestedMilestones;
    OptionSet<LayoutMilestone> milestonesAchieved;
    Page* page = frame().page();
    if (page)
        requestedMilestones = page->requestedLayoutMilestones();

    if (m_firstLayoutCallbackPending) {
        m_firstLayoutCallbackPending = false;
        frame().loader().didFirstLayout();
        if (requestedMilestones & DidFirstLayout)
            milestonesAchieved.add(DidFirstLayout);
        if (frame().isMainFrame())
            page->startCountingRelevantRepaintedObjects();
    }

    if (m_firstVisuallyNonEmptyLayoutMilestoneIsPending) {
        checkAndDispatchDidReachVisuallyNonEmptyState();
        if (m_contentQualifiesAsVisuallyNonEmpty) {
            m_firstVisuallyNonEmptyLayoutMilestoneIsPending = false;

            addPaintPendingMilestones(DidFirstMeaningfulPaint);
            if (requestedMilestones & DidFirstVisuallyNonEmptyLayout)
                milestonesAchieved.add(DidFirstVisuallyNonEmptyLayout);
        }
    }

    if (!m_renderedSignificantAmountOfText && qualifiesAsSignificantRenderedText()) {
        m_renderedSignificantAmountOfText = true;
        if (requestedMilestones & DidRenderSignificantAmountOfText)
            milestonesAchieved.add(DidRenderSignificantAmountOfText);
    }

    if (milestonesAchieved && frame().isMainFrame()) {
        if (milestonesAchieved.contains(DidFirstVisuallyNonEmptyLayout))
            FRAMEVIEW_RELEASE_LOG(Layout, "fireLayoutRelatedMilestonesIfNeeded: Firing first visually non-empty layout milestone on the main frame");
        frame().loader().didReachLayoutMilestone(milestonesAchieved);
    }
}

void FrameView::firePaintRelatedMilestonesIfNeeded()
{
    Page* page = frame().page();
    if (!page)
        return;

    OptionSet<LayoutMilestone> milestonesAchieved;

    // Make sure the pending paint milestones have actually been requested before we send them.
    if (m_milestonesPendingPaint & DidFirstFlushForHeaderLayer) {
        if (page->requestedLayoutMilestones() & DidFirstFlushForHeaderLayer)
            milestonesAchieved.add(DidFirstFlushForHeaderLayer);
    }

    if (m_milestonesPendingPaint & DidFirstPaintAfterSuppressedIncrementalRendering) {
        if (page->requestedLayoutMilestones() & DidFirstPaintAfterSuppressedIncrementalRendering)
            milestonesAchieved.add(DidFirstPaintAfterSuppressedIncrementalRendering);
    }

    if (m_milestonesPendingPaint & DidFirstMeaningfulPaint) {
        if (page->requestedLayoutMilestones() & DidFirstMeaningfulPaint)
            milestonesAchieved.add(DidFirstMeaningfulPaint);
    }

    m_milestonesPendingPaint = { };

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
    
    if (auto scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->setScrollPinningBehavior(pinning);
    
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
Display::View* FrameView::existingDisplayView() const
{
    return m_displayView.get();
}

Display::View& FrameView::displayView()
{
    if (!m_displayView)
        m_displayView = makeUnique<Display::View>(*this);
    return *m_displayView;
}
#endif

int FrameView::mapFromLayoutToCSSUnits(LayoutUnit value) const
{
    return value / (frame().pageZoomFactor() * frame().frameScaleFactor());
}

LayoutUnit FrameView::mapFromCSSToLayoutUnits(int value) const
{
    return LayoutUnit(value * frame().pageZoomFactor() * frame().frameScaleFactor());
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
    return copyToVectorOf<RefPtr<Widget>>(set);
}

void FrameView::updateWidgetPositions()
{
    m_updateWidgetPositionsTimer.stop();
    // updateWidgetPosition() can possibly cause layout to be re-entered (via plug-ins running
    // scripts in response to NPP_SetWindow, for example), so we need to keep the Widgets
    // alive during enumeration.
    for (auto& widget : collectAndProtectWidgets(m_widgetsInRenderTree)) {
        if (auto* renderer = RenderWidget::find(*widget)) {
            auto ignoreWidgetState = renderer->updateWidgetPosition();
            UNUSED_PARAM(ignoreWidgetState);
        }
    }
}

void FrameView::scheduleUpdateWidgetPositions()
{
    if (!m_updateWidgetPositionsTimer.isActive())
        m_updateWidgetPositionsTimer.startOneShot(0_s);
}

void FrameView::updateWidgetPositionsTimerFired()
{
    updateWidgetPositions();
}

void FrameView::notifyWidgets(WidgetNotification notification)
{
    for (auto& widget : collectAndProtectWidgets(m_widgetsInRenderTree))
        widget->notifyWidget(notification);
}

void FrameView::setViewExposedRect(std::optional<FloatRect> viewExposedRect)
{
    if (m_viewExposedRect == viewExposedRect)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "FrameView " << this << " setViewExposedRect " << (viewExposedRect ? viewExposedRect.value() : FloatRect()));

    bool hasRectExistenceChanged = !m_viewExposedRect == !viewExposedRect;
    m_viewExposedRect = viewExposedRect;

    // FIXME: We should support clipping to the exposed rect for subframes as well.
    if (!frame().isMainFrame())
        return;

    if (TiledBacking* tiledBacking = this->tiledBacking()) {
        if (hasRectExistenceChanged)
            updateTiledBackingAdaptiveSizing();
        adjustTiledBackingCoverage();
        tiledBacking->setTiledScrollingIndicatorPosition(m_viewExposedRect ? m_viewExposedRect.value().location() : FloatPoint());
    }

    if (auto* page = frame().page()) {
        page->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
        page->pageOverlayController().didChangeViewExposedRect();
    }
}

void FrameView::clearSizeOverrideForCSSSmallViewportUnits()
{
    if (!m_smallViewportSizeOverride)
        return;

    m_smallViewportSizeOverride = std::nullopt;
    if (auto* document = frame().document())
        document->styleScope().didChangeStyleSheetEnvironment();
}

void FrameView::setSizeForCSSSmallViewportUnits(FloatSize size)
{
    overrideSizeForCSSSmallViewportUnits({ size.width(), size.height() });
}

void FrameView::overrideWidthForCSSSmallViewportUnits(float width)
{
    overrideSizeForCSSSmallViewportUnits({ width, m_smallViewportSizeOverride ? m_smallViewportSizeOverride->height : std::nullopt });
}

void FrameView::resetOverriddenWidthForCSSSmallViewportUnits()
{
    overrideSizeForCSSSmallViewportUnits({ { }, m_smallViewportSizeOverride ? m_smallViewportSizeOverride->height : std::nullopt });
}

void FrameView::overrideSizeForCSSSmallViewportUnits(OverrideViewportSize size)
{
    if (m_smallViewportSizeOverride && *m_smallViewportSizeOverride == size)
        return;

    m_smallViewportSizeOverride = size;

    if (auto* document = frame().document())
        document->styleScope().didChangeStyleSheetEnvironment();
}

FloatSize FrameView::sizeForCSSSmallViewportUnits() const
{
    return calculateSizeForCSSViewportUnitsOverride(m_smallViewportSizeOverride);
}

void FrameView::clearSizeOverrideForCSSLargeViewportUnits()
{
    if (!m_largeViewportSizeOverride)
        return;

    m_largeViewportSizeOverride = std::nullopt;
    if (auto* document = frame().document())
        document->styleScope().didChangeStyleSheetEnvironment();
}

void FrameView::setSizeForCSSLargeViewportUnits(FloatSize size)
{
    overrideSizeForCSSLargeViewportUnits({ size.width(), size.height() });
}

void FrameView::overrideWidthForCSSLargeViewportUnits(float width)
{
    overrideSizeForCSSLargeViewportUnits({ width, m_largeViewportSizeOverride ? m_largeViewportSizeOverride->height : std::nullopt });
}

void FrameView::resetOverriddenWidthForCSSLargeViewportUnits()
{
    overrideSizeForCSSLargeViewportUnits({ { }, m_largeViewportSizeOverride ? m_largeViewportSizeOverride->height : std::nullopt });
}

void FrameView::overrideSizeForCSSLargeViewportUnits(OverrideViewportSize size)
{
    if (m_largeViewportSizeOverride && *m_largeViewportSizeOverride == size)
        return;

    m_largeViewportSizeOverride = size;

    if (auto* document = frame().document())
        document->styleScope().didChangeStyleSheetEnvironment();
}

FloatSize FrameView::sizeForCSSLargeViewportUnits() const
{
    return calculateSizeForCSSViewportUnitsOverride(m_largeViewportSizeOverride);
}

FloatSize FrameView::calculateSizeForCSSViewportUnitsOverride(std::optional<OverrideViewportSize> override) const
{
    OverrideViewportSize viewportSize;

    if (override) {
        viewportSize = *override;

        // auto-size overrides the width only, so we can't always bail out early here.
        if (viewportSize.width && viewportSize.height)
            return { *viewportSize.width, *viewportSize.height };
    }

    if (useFixedLayout()) {
        auto fixedLayoutSize = this->fixedLayoutSize();
        viewportSize.width = viewportSize.width.value_or(fixedLayoutSize.width());
        viewportSize.height = viewportSize.height.value_or(fixedLayoutSize.height());
        return { *viewportSize.width, *viewportSize.height };
    }
    
    // FIXME: the value returned should take into account the value of the overflow
    // property on the root element.
    auto visibleContentSizeIncludingScrollbars = visibleContentRectIncludingScrollbars().size();
    viewportSize.width = viewportSize.width.value_or(visibleContentSizeIncludingScrollbars.width());
    viewportSize.height = viewportSize.height.value_or(visibleContentSizeIncludingScrollbars.height());
    return { *viewportSize.width, *viewportSize.height };
}

FloatSize FrameView::sizeForCSSDynamicViewportUnits() const
{
    return rectForFixedPositionLayout().size();
}

FloatSize FrameView::sizeForCSSDefaultViewportUnits() const
{
    return sizeForCSSLargeViewportUnits();
}

bool FrameView::shouldPlaceVerticalScrollbarOnLeft() const
{
    return renderView() && renderView()->shouldPlaceVerticalScrollbarOnLeft();
}

TextStream& operator<<(TextStream& ts, const FrameView& view)
{
    ts << view.debugDescription();
    return ts;
}

void FrameView::didFinishProhibitingScrollingWhenChangingContentSize()
{
    if (auto* document = frame().document()) {
        if (!document->needsStyleRecalc() && !needsLayout() && !layoutContext().isInLayout())
            updateScrollbars(scrollPosition());
        else
            m_needsDeferredScrollbarsUpdate = true;
    }
}

float FrameView::pageScaleFactor() const
{
    return frame().frameScaleFactor();
}

void FrameView::didStartScrollAnimation()
{
    if (auto* page = frame().page())
        page->scheduleRenderingUpdate({ RenderingUpdateStep::Scroll });
}

void FrameView::updateScrollbarSteps()
{
    auto* documentElement = frame().document() ? frame().document()->documentElement() : nullptr;
    auto* renderer = documentElement ? documentElement->renderBox() : nullptr;
    if (!renderer) {
        ScrollView::updateScrollbarSteps();
        return;
    }

    LayoutRect paddedViewRect(LayoutPoint(), visibleSize());
    paddedViewRect.contract(renderer->scrollPaddingForViewportRect(paddedViewRect));

    if (horizontalScrollbar()) {
        int pageStep = Scrollbar::pageStep(paddedViewRect.width());
        horizontalScrollbar()->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);

    }
    if (verticalScrollbar()) {
        int pageStep = Scrollbar::pageStep(paddedViewRect.height());
        verticalScrollbar()->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
    }
}

OverscrollBehavior FrameView::horizontalOverscrollBehavior() const
{
    auto* document = frame().document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject && renderView() && renderView()->canBeScrolledAndHasScrollableArea())
        return scrollingObject->style().overscrollBehaviorX();
    return OverscrollBehavior::Auto;
}

OverscrollBehavior FrameView::verticalOverscrollBehavior()  const
{
    auto* document = frame().document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject && renderView() && renderView()->canBeScrolledAndHasScrollableArea())
        return scrollingObject->style().overscrollBehaviorY();
    return OverscrollBehavior::Auto;
}
} // namespace WebCore

#undef PAGE_ID
#undef FRAME_ID
