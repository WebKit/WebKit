/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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
#include "LocalFrameView.h"

#include "AXObjectCache.h"
#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ColorBlending.h"
#include "ContainerNode.h"
#include "ContentVisibilityDocumentState.h"
#include "DebugPageOverlays.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentSVG.h"
#include "Editor.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FragmentDirectiveParser.h"
#include "FragmentDirectiveRangeFinder.h"
#include "FragmentDirectiveUtilities.h"
#include "FrameLoader.h"
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
#include "HTMLPlugInImageElement.h"
#include "HighlightRegistry.h"
#include "ImageDocument.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "NullGraphicsContext.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "PerformanceLoggingClient.h"
#include "ProgressTracker.h"
#include "Quirks.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderEmbeddedObject.h"
#include "RenderFlexibleBox.h"
#include "RenderIFrame.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderSVGRoot.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderStyleSetters.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResizeObserver.h"
#include "SVGDocument.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "ScriptDisallowedScope.h"
#include "ScriptRunner.h"
#include "ScriptedAnimationController.h"
#include "ScrollAnchoringController.h"
#include "ScrollAnimator.h"
#include "ScrollSnapOffsetsInfo.h"
#include "ScrollbarTheme.h"
#include "ScrollbarsController.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "TextIndicator.h"
#include "TextIterator.h"
#include "TextResourceDecoder.h"
#include "TiledBacking.h"
#include "VelocityData.h"
#include "VisualViewport.h"
#include "WheelEventTestMonitor.h"
#include <wtf/HexNumber.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "DocumentLoader.h"
#include "LegacyTileCache.h"
#endif

#include "LayoutContext.h"

#define PAGE_ID (m_frame->pageID() ? m_frame->pageID()->toUInt64() : 0)
#define FRAME_ID m_frame->frameID().object().toUInt64()
#define FRAMEVIEW_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", isMainFrame=%d] LocalFrameView::" fmt, this, PAGE_ID, FRAME_ID, m_frame->isMainFrame(), ##__VA_ARGS__)

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(LocalFrameView);

MonotonicTime LocalFrameView::sCurrentPaintTimeStamp { };

// The maximum number of updateEmbeddedObjects iterations that should be done before returning.
static const unsigned maxUpdateEmbeddedObjectsIterations = 2;

static constexpr unsigned defaultSignificantRenderedTextCharacterThreshold = 3000;
static constexpr float defaultSignificantRenderedTextMeanLength = 50;
static constexpr unsigned mainArticleSignificantRenderedTextCharacterThreshold = 1500;
static constexpr float mainArticleSignificantRenderedTextMeanLength = 25;

Pagination::Mode paginationModeForRenderStyle(const RenderStyle& style)
{
    auto overflow = style.overflowY();
    if (overflow != Overflow::PagedX && overflow != Overflow::PagedY)
        return Pagination::Mode::Unpaginated;

    // paged-x always corresponds to LeftToRightPaginated or RightToLeftPaginated. If the WritingMode
    // is horizontal, then we use TextDirection to choose between those options. If the WritingMode
    // is vertical, then the block flow direction dictates the choice.
    if (overflow == Overflow::PagedX) {
        return style.writingMode().isAnyLeftToRight() ? Pagination::Mode::LeftToRightPaginated : Pagination::Mode::RightToLeftPaginated;
    }

    // paged-y always corresponds to TopToBottomPaginated or BottomToTopPaginated. If the WritingMode
    // is horizontal, then the block flow direction dictates the choice. If the WritingMode
    // is vertical, then we use TextDirection to choose between those options. 
    return style.writingMode().isAnyTopToBottom() ? Pagination::Mode::TopToBottomPaginated : Pagination::Mode::BottomToTopPaginated;
}

LocalFrameView::LocalFrameView(LocalFrame& frame)
    : m_frame(frame)
    , m_layoutContext(*this)
    , m_updateEmbeddedObjectsTimer(*this, &LocalFrameView::updateEmbeddedObjectsTimerFired)
    , m_updateWidgetPositionsTimer(*this, &LocalFrameView::updateWidgetPositionsTimerFired)
    , m_delayedScrollEventTimer(*this, &LocalFrameView::scheduleScrollEvent)
    , m_delayedScrollToFocusedElementTimer(*this, &LocalFrameView::scrollToFocusedElementTimerFired)
    , m_speculativeTilingEnableTimer(*this, &LocalFrameView::speculativeTilingEnableTimerFired)
    , m_delayedTextFragmentIndicatorTimer(*this, &LocalFrameView::textFragmentIndicatorTimerFired)
    , m_mediaType(screenAtom())
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
    if (m_frame->document() && m_frame->document()->settings().cssScrollAnchoringEnabled())
        m_scrollAnchoringController = WTF::makeUnique<ScrollAnchoringController>(*this);
}

Ref<LocalFrameView> LocalFrameView::create(LocalFrame& frame)
{
    Ref<LocalFrameView> view = adoptRef(*new LocalFrameView(frame));
    if (frame.page() && frame.page()->isVisible())
        view->show();
    return view;
}

Ref<LocalFrameView> LocalFrameView::create(LocalFrame& frame, const IntSize& initialSize)
{
    Ref<LocalFrameView> view = adoptRef(*new LocalFrameView(frame));
    view->Widget::setFrameRect(IntRect(view->location(), initialSize));
    if (frame.page() && frame.page()->isVisible())
        view->show();
    return view;
}

LocalFrameView::~LocalFrameView()
{
    removeFromAXObjectCache();
    resetScrollbars();

    // Custom scrollbars should already be destroyed at this point
    ASSERT(!horizontalScrollbar() || !horizontalScrollbar()->isCustomScrollbar());
    ASSERT(!verticalScrollbar() || !verticalScrollbar()->isCustomScrollbar());

    setHasHorizontalScrollbar(false); // Remove native scrollbars now before we lose the connection to the HostWindow.
    setHasVerticalScrollbar(false);
    
    ASSERT(!m_scrollCorner);

    ASSERT(m_frame->view() != this || !m_frame->contentRenderer());
}

void LocalFrameView::reset()
{
    m_cannotBlitToWindow = false;
    m_isOverlapped = false;
    m_contentIsOpaque = false;
    m_updateEmbeddedObjectsTimer.stop();
    m_wasScrolledByUser = false;
    m_delayedScrollEventTimer.stop();
    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();
    m_delayedTextFragmentIndicatorTimer.stop();
    m_pendingTextFragmentIndicatorRange.reset();
    m_pendingTextFragmentIndicatorText = String();
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

void LocalFrameView::resetLayoutMilestones()
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

void LocalFrameView::removeFromAXObjectCache()
{
    if (AXObjectCache* cache = axObjectCache()) {
        if (HTMLFrameOwnerElement* owner = m_frame->ownerElement())
            cache->childrenChanged(owner->renderer());
        cache->remove(this);
    }
}

void LocalFrameView::resetScrollbars()
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

void LocalFrameView::resetScrollbarsAndClearContentsSize()
{
    resetScrollbars();

    LOG(Layout, "LocalFrameView %p resetScrollbarsAndClearContentsSize", this);

    setScrollbarsSuppressed(true);
    setContentsSize(IntSize());
    setScrollbarsSuppressed(false);
}

void LocalFrameView::init()
{
    reset();

    m_lastUsedSizeForLayout = { };

    // Propagate the scrolling mode to the view.
    if (m_frame->scrollingMode() == ScrollbarMode::AlwaysOff)
        setCanHaveScrollbars(false);

    Page* page = m_frame->page();
    if (page && page->chrome().client().shouldPaintEntireContents())
        setPaintsEntireContents(true);
}

void LocalFrameView::prepareForDetach()
{
    detachCustomScrollbars();
    // When the view is no longer associated with a frame, it needs to be removed from the ax object cache
    // right now, otherwise it won't be able to reach the topDocument()'s axObject cache later.
    removeFromAXObjectCache();

    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->willDestroyScrollableArea(*this);
}

void LocalFrameView::detachCustomScrollbars()
{
    Scrollbar* horizontalBar = horizontalScrollbar();
    if (horizontalBar && horizontalBar->isCustomScrollbar())
        setHasHorizontalScrollbar(false);

    Scrollbar* verticalBar = verticalScrollbar();
    if (verticalBar && verticalBar->isCustomScrollbar())
        setHasVerticalScrollbar(false);

    m_scrollCorner = nullptr;
}

void LocalFrameView::willBeDestroyed()
{
    setHasHorizontalScrollbar(false);
    setHasVerticalScrollbar(false);
    m_scrollCorner = nullptr;
}

void LocalFrameView::recalculateScrollbarOverlayStyle()
{
    auto style = [this] {
        if (auto page = m_frame->page()) {
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

void LocalFrameView::clear()
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
void LocalFrameView::didReplaceMultipartContent()
{
    // Re-enable tile updates that were disabled in clear().
    if (LegacyTileCache* tileCache = legacyTileCache())
        tileCache->setTilingMode(LegacyTileCache::Normal);
}
#endif

bool LocalFrameView::didFirstLayout() const
{
    return layoutContext().didFirstLayout();
}

void LocalFrameView::setFrameRect(const IntRect& newRect)
{
    Ref<LocalFrameView> protectedThis(*this);
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

    if (m_frame->isMainFrame() && m_frame->page())
        m_frame->page()->pageOverlayController().didChangeViewSize();

    if (auto* document = m_frame->document())
        document->didChangeViewSize();

    viewportContentsChanged();
    setCurrentScrollType(oldScrollType);
}

void LocalFrameView::setCanHaveScrollbars(bool canHaveScrollbars)
{
    m_canHaveScrollbars = canHaveScrollbars;
    ScrollView::setCanHaveScrollbars(canHaveScrollbars);
}

void LocalFrameView::updateCanHaveScrollbars()
{
    ScrollbarMode hMode;
    ScrollbarMode vMode;
    scrollbarModes(hMode, vMode);
    if (hMode == ScrollbarMode::AlwaysOff && vMode == ScrollbarMode::AlwaysOff)
        setCanHaveScrollbars(false);
    else
        setCanHaveScrollbars(true);
}

RefPtr<Element> LocalFrameView::rootElementForCustomScrollbarPartStyle() const
{
    // FIXME: We need to update the scrollbar dynamically as documents change (or as doc elements and bodies get discovered that have custom styles).
    auto* document = m_frame->document();
    if (!document)
        return nullptr;

    // Try the <body> element first as a scrollbar source.
    auto* body = document->bodyOrFrameset();
    // scrollbar-width on root element should override custom scrollbars declared on body element, so check that here..
    if (body && body->renderer() && body->renderer()->style().usesLegacyScrollbarStyle() && scrollbarWidthStyle() == ScrollbarWidth::Auto)
        return body;

    // If the <body> didn't have a custom style, then the root element might.
    auto* docElement = document->documentElement();
    if (docElement && docElement->renderer() && docElement->renderer()->style().usesLegacyScrollbarStyle())
        return docElement;

    return nullptr;
}

Ref<Scrollbar> LocalFrameView::createScrollbar(ScrollbarOrientation orientation)
{
    if (auto element = rootElementForCustomScrollbarPartStyle())
        return RenderScrollbar::createCustomScrollbar(*this, orientation, element.get());
    
    // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
    // FIXME: Seems bad to do this for cross-origin frames.
    RenderWidget* frameRenderer = m_frame->ownerRenderer();
    if (frameRenderer && frameRenderer->style().usesLegacyScrollbarStyle())
        return RenderScrollbar::createCustomScrollbar(*this, orientation, nullptr, m_frame.ptr());

    // Nobody set a custom style, so we just use a native scrollbar.
    return Scrollbar::createNativeScrollbar(*this, orientation, scrollbarWidthStyle());
}

void LocalFrameView::didRestoreFromBackForwardCache()
{
    // When restoring from back/forward cache, the main frame stays in place while subframes get swapped in.
    // We update the scrollable area set to ensure that scrolling data structures get invalidated.
    updateScrollableAreaSet();
}

void LocalFrameView::willDestroyRenderTree()
{
    detachCustomScrollbars();
    layoutContext().clearSubtreeLayoutRoot();
}

void LocalFrameView::didDestroyRenderTree()
{
    ASSERT(!layoutContext().subtreeLayoutRoot());
    ASSERT(m_widgetsInRenderTree.isEmpty());

    ASSERT(!m_embeddedObjectsToUpdate || m_embeddedObjectsToUpdate->isEmpty());

    ASSERT(!m_viewportConstrainedObjects || m_viewportConstrainedObjects->isEmptyIgnoringNullReferences());
    ASSERT(!m_slowRepaintObjects || m_slowRepaintObjects->isEmptyIgnoringNullReferences());
}

void LocalFrameView::setContentsSize(const IntSize& size)
{
    if (size == contentsSize())
        return;

    layoutContext().disableSetNeedsLayout();

    ScrollView::setContentsSize(size);
    contentsResized();

    Page* page = m_frame->page();
    if (!page)
        return;

    updateScrollableAreaSet();

    page->chrome().contentsSizeChanged(m_frame.get(), size); // Notify only.

    if (m_frame->isMainFrame()) {
        page->pageOverlayController().didChangeDocumentSize();
        BackForwardCache::singleton().markPagesForContentsSizeChanged(*page);
    }
    layoutContext().enableSetNeedsLayout();
}

void LocalFrameView::adjustViewSize()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    ASSERT(m_frame->view() == this);

    const IntRect rect = renderView->documentRect();
    const IntSize& size = rect.size();
    ScrollView::setScrollOrigin(IntPoint(-rect.x(), -rect.y()), !m_frame->document()->printing(), size == contentsSize());

    LOG_WITH_STREAM(Layout, stream << "LocalFrameView " << this << " adjustViewSize: unscaled document rect changed to " << renderView->unscaledDocumentRect() << " (scaled to " << size << ")");

    setContentsSize(size);
}

void LocalFrameView::applyOverflowToViewport(const RenderElement& renderer, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.

    // To combat the inability to scroll on a page with overflow:hidden on the root when scaled, disregard hidden when
    // there is a frameScaleFactor that is greater than one on the main frame. Also disregard hidden if there is a
    // header or footer.

    bool overrideHidden = m_frame->isMainFrame() && ((m_frame->frameScaleFactor() > 1) || headerHeight() || footerHeight());

    Overflow overflowX = renderer.style().overflowX();
    Overflow overflowY = renderer.style().overflowY();

    if (CheckedPtr renderSVGRoot = dynamicDowncast<RenderSVGRoot>(renderer)) {
        // FIXME: evaluate if we can allow overflow for these cases too.
        // Overflow is always hidden when stand-alone SVG documents are embedded.
        if (renderSVGRoot->isEmbeddedThroughFrameContainingSVGDocument()) {
            overflowX = Overflow::Hidden;
            overflowY = Overflow::Hidden;
        }
    }

    if (CheckedPtr legacyRenderSVGRoot = dynamicDowncast<LegacyRenderSVGRoot>(renderer)) {
        // FIXME: evaluate if we can allow overflow for these cases too.
        // Overflow is always hidden when stand-alone SVG documents are embedded.
        if (legacyRenderSVGRoot->isEmbeddedThroughFrameContainingSVGDocument()) {
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

void LocalFrameView::applyPaginationToViewport()
{
    auto* document = m_frame->document();
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
            auto* renderBox = dynamicDowncast<RenderBox>(documentOrBodyRenderer);
            if (auto* containerForPaginationGap = renderBox ? renderBox : documentOrBodyRenderer->containingBlock())
                pagination.gap = valueForLength(columnGapLength.length(), containerForPaginationGap->availableLogicalWidth()).toUnsigned();
        }
    }
    setPagination(pagination);
}

void LocalFrameView::calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy strategy)
{
    m_viewportRendererType = ViewportRendererType::None;

    if (m_frame->scrollingMode() == ScrollbarMode::AlwaysOff) {
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
    
    auto* document = m_frame->document();
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
    
    if (is<HTMLFrameSetElement>(*bodyOrFrameset)) {
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

void LocalFrameView::willRecalcStyle()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    renderView->compositor().willRecalcStyle();
}

void LocalFrameView::styleAndRenderTreeDidChange()
{
    ScrollView::styleAndRenderTreeDidChange();
    if (!renderView())
        return;

    checkAndDispatchDidReachVisuallyNonEmptyState();
}

bool LocalFrameView::updateCompositingLayersAfterStyleChange()
{
    // If we expect to update compositing after an incipient layout, don't do so here.
    auto* renderView = this->renderView();
    if (!renderView)
        return false;
    if (needsLayout() || layoutContext().isInLayout())
        return false;
    renderView->layer()->updateLayerPositionsAfterStyleChange();
    return renderView->compositor().didRecalcStyleWithNoPendingLayout();
}

void LocalFrameView::updateCompositingLayersAfterLayout()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    renderView->compositor().updateCompositingLayers(CompositingUpdateType::AfterLayout);
    m_updateCompositingLayersIsPending = false;
}

bool LocalFrameView::updateCompositingLayersAfterLayoutIfNeeded()
{
    if (m_updateCompositingLayersIsPending) {
        updateCompositingLayersAfterLayout();
        return true;
    }

    return false;
}

void LocalFrameView::invalidateScrollbarsForAllScrollableAreas()
{
    if (!m_scrollableAreas)
        return;

    for (auto& area : *m_scrollableAreas) {
        CheckedPtr<ScrollableArea> scrollableArea(area);
        scrollableArea->invalidateScrollbars();
    }
}

GraphicsLayer* LocalFrameView::layerForHorizontalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForHorizontalScrollbar();
}

GraphicsLayer* LocalFrameView::layerForVerticalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForVerticalScrollbar();
}

GraphicsLayer* LocalFrameView::layerForScrollCorner() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForScrollCorner();
}

TiledBacking* LocalFrameView::tiledBacking() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return nullptr;

    return backing->tiledBacking();
}

std::optional<ScrollingNodeID> LocalFrameView::scrollingNodeID() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return std::nullopt;

    RenderLayerBacking* backing = renderView->layer()->backing();
    if (!backing)
        return std::nullopt;

    return backing->scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling);
}

ScrollableArea* LocalFrameView::scrollableAreaForScrollingNodeID(ScrollingNodeID nodeID) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    if (auto area = m_scrollingNodeIDToPluginScrollableAreaMap.get(nodeID))
        return area.get();

    return renderView->compositor().scrollableAreaForScrollingNodeID(nodeID);
}

#if HAVE(RUBBER_BANDING)
GraphicsLayer* LocalFrameView::layerForOverhangAreas() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;
    return renderView->compositor().layerForOverhangAreas();
}

GraphicsLayer* LocalFrameView::setWantsLayerForTopOverHangArea(bool wantsLayer) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    return renderView->compositor().updateLayerForTopOverhangArea(wantsLayer);
}

GraphicsLayer* LocalFrameView::setWantsLayerForBottomOverHangArea(bool wantsLayer) const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    return renderView->compositor().updateLayerForBottomOverhangArea(wantsLayer);
}

#endif // HAVE(RUBBER_BANDING)

void LocalFrameView::updateSnapOffsets()
{
    if (!m_frame->document())
        return;

    RefPtr documentElement = m_frame->document()->documentElement();
    CheckedPtr rootRenderer = documentElement ? documentElement->renderBox() : nullptr;

    const RenderStyle* styleToUse = nullptr;
    if (rootRenderer && rootRenderer->style().scrollSnapType().strictness != ScrollSnapStrictness::None)
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

    updateSnapOffsetsForScrollableArea(*this, *rootRenderer, *styleToUse, viewport, rootRenderer->style().writingMode(), m_frame->document()->protectedFocusedElement().get());
}

bool LocalFrameView::isScrollSnapInProgress() const
{
    if (scrollbarsSuppressed())
        return false;

    // If the scrolling thread updates the scroll position for this FrameView, then we should return
    // ScrollingCoordinator::isScrollSnapInProgress().
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator()) {
        if (scrollingCoordinator->isScrollSnapInProgress(scrollingNodeID()))
            return true;
    }

    // If the main thread updates the scroll position for this FrameView, we should return
    // ScrollAnimator::isScrollSnapInProgress().
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isScrollSnapInProgress();

    return false;
}

void LocalFrameView::updateScrollingCoordinatorScrollSnapProperties() const
{
    renderView()->compositor().updateScrollSnapPropertiesWithFrameView(*this);
}

bool LocalFrameView::flushCompositingStateForThisFrame(const LocalFrame& rootFrameForFlush)
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return true; // We don't want to keep trying to update layers if we have no renderer.

    ASSERT(m_frame->view() == this);

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

void LocalFrameView::setNeedsOneShotDrawingSynchronization()
{
    if (Page* page = m_frame->page())
        page->chrome().client().setNeedsOneShotDrawingSynchronization();
}

GraphicsLayer* LocalFrameView::graphicsLayerForPlatformWidget(PlatformWidget platformWidget)
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

GraphicsLayer* LocalFrameView::graphicsLayerForPageScale()
{
    auto* page = m_frame->page();
    if (!page)
        return nullptr;

    if (page->delegatesScaling()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    if (!renderView->hasLayer() || !renderView->layer()->isComposited())
        return nullptr;

    auto* backing = renderView->layer()->backing();
    if (auto* contentsContainmentLayer = backing->contentsContainmentLayer())
        return contentsContainmentLayer;

    return backing->graphicsLayer();
}

GraphicsLayer* LocalFrameView::graphicsLayerForScrolledContents()
{
    if (auto* renderView = m_frame->contentRenderer())
        return renderView->compositor().scrolledContentsLayer();
    return nullptr;
}

#if HAVE(RUBBER_BANDING)
GraphicsLayer* LocalFrameView::graphicsLayerForTransientZoomShadow()
{
    auto* page = m_frame->page();
    if (!page)
        return nullptr;

    if (page->delegatesScaling()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    return renderView->compositor().layerForContentShadow();
}
#endif

LayoutRect LocalFrameView::fixedScrollableAreaBoundsInflatedForScrolling(const LayoutRect& uninflatedBounds) const
{
    LayoutPoint scrollPosition;
    LayoutSize topLeftExpansion;
    LayoutSize bottomRightExpansion;

    if (m_frame->settings().visualViewportEnabled()) {
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

LayoutPoint LocalFrameView::scrollPositionRespectingCustomFixedPosition() const
{
#if PLATFORM(IOS_FAMILY)
    if (!m_frame->settings().visualViewportEnabled())
        return useCustomFixedPositionLayoutRect() ? customFixedPositionLayoutRect().location() : scrollPosition();
#endif

    return scrollPositionForFixedPosition();
}

void LocalFrameView::topContentInsetDidChange(float newTopContentInset)
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    if (platformWidget())
        platformSetTopContentInset(newTopContentInset);
    
    renderView->setNeedsLayout();
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

void LocalFrameView::topContentDirectionDidChange()
{
    m_needsDeferredScrollbarsUpdate = true;
    m_needsDeferredPositionScrollbarLayers = true;
}

void LocalFrameView::handleDeferredScrollbarsUpdate()
{
    if (!m_needsDeferredScrollbarsUpdate)
        return;

    m_needsDeferredScrollbarsUpdate = false;
    updateScrollbars(scrollPosition());
}

void LocalFrameView::handleDeferredPositionScrollbarLayers()
{
    if (!m_needsDeferredPositionScrollbarLayers)
        return;

    m_needsDeferredPositionScrollbarLayers = false;
    positionScrollbarLayers();
}

// Sometimes (for plug-ins) we need to eagerly go into compositing mode.
void LocalFrameView::enterCompositingMode()
{
    if (RenderView* renderView = this->renderView()) {
        renderView->compositor().enableCompositingMode();
        if (!needsLayout())
            renderView->compositor().scheduleCompositingLayerUpdate();
    }
}

bool LocalFrameView::isEnclosedInCompositingLayer() const
{
    auto frameOwnerRenderer = m_frame->ownerRenderer();
    if (frameOwnerRenderer && frameOwnerRenderer->containerForRepaint().renderer)
        return true;

    if (auto* parentView = parentFrameView())
        return parentView->isEnclosedInCompositingLayer();
    return false;
}

bool LocalFrameView::flushCompositingStateIncludingSubframes()
{
    bool allFramesFlushed = flushCompositingStateForThisFrame(m_frame.get());

    for (RefPtr child = m_frame->tree().firstRenderedChild(); child; child = child->tree().traverseNextRendered(m_frame.ptr())) {
        RefPtr localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        RefPtr frameView = localChild->view();
        if (!frameView)
            continue;
        bool flushed = frameView->flushCompositingStateForThisFrame(m_frame.get());
        allFramesFlushed &= flushed;
    }
    return allFramesFlushed;
}

bool LocalFrameView::isSoftwareRenderable() const
{
    RenderView* renderView = this->renderView();
    return !renderView || !renderView->compositor().has3DContent();
}

void LocalFrameView::setIsInWindow(bool isInWindow)
{
    if (RenderView* renderView = this->renderView())
        renderView->setIsInWindow(isInWindow);
}

void LocalFrameView::forceLayoutParentViewIfNeeded()
{
    RenderWidget* ownerRenderer = m_frame->ownerRenderer();
    if (!ownerRenderer)
        return;

    RenderBox* contentBox = embeddedContentBox();
    if (!contentBox)
        return;

    if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(contentBox)) {
        if (svgRoot->everHadLayout() && !svgRoot->needsLayout())
            return;
    }

    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(contentBox)) {
        if (svgRoot->everHadLayout() && !svgRoot->needsLayout())
            return;
    }

    LOG(Layout, "LocalFrameView %p forceLayoutParentViewIfNeeded scheduling layout on parent LocalFrameView %p", this, &ownerRenderer->view().frameView());

    // If the embedded SVG document appears the first time, the ownerRenderer has already finished
    // layout without knowing about the existence of the embedded SVG document, because RenderReplaced
    // embeddedContentBox() returns nullptr, as long as the embedded document isn't loaded yet. Before
    // bothering to lay out the SVG document, mark the ownerRenderer needing layout and ask its
    // LocalFrameView for a layout. After that the RenderEmbeddedObject (ownerRenderer) carries the
    // correct size, which LegacyRenderSVGRoot::computeReplacedLogicalWidth/Height rely on, when laying
    // out for the first time, or when the LegacyRenderSVGRoot size has changed dynamically (eg. via <script>).

    ownerRenderer->setNeedsLayoutAndPrefWidthsRecalc();
    ownerRenderer->view().frameView().layoutContext().scheduleLayout();
}

void LocalFrameView::markRootOrBodyRendererDirty() const
{
    auto& document = *m_frame->document();
    RenderBox* rootRenderer = document.documentElement() ? document.documentElement()->renderBox() : nullptr;
    auto* body = document.bodyOrFrameset();
    RenderBox* bodyRenderer = rootRenderer && body ? body->renderBox() : nullptr;
    if (bodyRenderer && bodyRenderer->stretchesToViewport())
        bodyRenderer->setChildNeedsLayout();
    else if (rootRenderer && rootRenderer->stretchesToViewport())
        rootRenderer->setChildNeedsLayout();
}

void LocalFrameView::adjustScrollbarsForLayout(bool isFirstLayout)
{
    ScrollbarMode hMode;
    ScrollbarMode vMode;
    calculateScrollbarModesForLayout(hMode, vMode);

    LOG_WITH_STREAM(Layout, stream << "LocalFrameView " << this << " adjustScrollbarsForLayout - first layout " << isFirstLayout << " horizontal scrollbar mode:" << hMode << " vertical scrollbar mode:" << vMode);

    if (isFirstLayout && !layoutContext().isLayoutNested()) {
        setScrollbarsSuppressed(true);
        // Set the initial vMode to AlwaysOn if we're auto.
        if (vMode == ScrollbarMode::Auto)
            setVerticalScrollbarMode(ScrollbarMode::AlwaysOn); // This causes a vertical scrollbar to appear.
        // Set the initial hMode to AlwaysOff if we're auto.
        if (hMode == ScrollbarMode::Auto)
            setHorizontalScrollbarMode(ScrollbarMode::AlwaysOff); // This causes a horizontal scrollbar to disappear.
        ASSERT(m_frame->page());
        if (m_frame->page()->isMonitoringWheelEvents())
            scrollAnimator().setWheelEventTestMonitor(m_frame->page()->wheelEventTestMonitor());
        setScrollbarModes(hMode, vMode);
        setScrollbarsSuppressed(false, true);
    } else if (hMode != horizontalScrollbarMode() || vMode != verticalScrollbarMode())
        setScrollbarModes(hMode, vMode);
}

void LocalFrameView::willDoLayout(SingleThreadWeakPtr<RenderElement> layoutRoot)
{
    bool subtreeLayout = !is<RenderView>(*layoutRoot);
    if (subtreeLayout)
        return;
    
    if (auto* body = m_frame->document()->bodyOrFrameset()) {
        if (is<HTMLFrameSetElement>(*body) && body->renderer())
            body->renderer()->setChildNeedsLayout();
    }
    auto firstLayout = !layoutContext().didFirstLayout();
    if (firstLayout) {
        m_lastViewportSize = sizeForResizeEvent();
        m_lastZoomFactor = layoutRoot->style().zoom();
        m_firstLayoutCallbackPending = true;
    }
    adjustScrollbarsForLayout(firstLayout);

    auto oldSize = m_lastUsedSizeForLayout;
    auto newSize = layoutSize();
    if (oldSize != newSize) {
        m_lastUsedSizeForLayout = newSize;
        LOG_WITH_STREAM(Layout, stream << "  size for layout changed from " << oldSize << " to " << newSize);
        layoutContext().setNeedsFullRepaint();
        if (!firstLayout)
            markRootOrBodyRendererDirty();
    }
    forceLayoutParentViewIfNeeded();
}

bool LocalFrameView::hasPendingUpdateLayerPositions() const
{
    return !!m_pendingUpdateLayerPositions;
}

void LocalFrameView::flushUpdateLayerPositions()
{
    if (!m_pendingUpdateLayerPositions)
        return;

    UpdateLayerPositions updateLayerPositions = *std::exchange(m_pendingUpdateLayerPositions, std::nullopt);

    if (CheckedPtr view = renderView()) {
        view->layer()->updateLayerPositionsAfterLayout(updateLayerPositions.layoutIdentifier, updateLayerPositions.needsFullRepaint, updateLayerPositions.didRunSimplifiedLayout ? RenderLayer::CanUseSimplifiedRepaintPass::Yes : RenderLayer::CanUseSimplifiedRepaintPass::No);
        m_renderLayerPositionUpdateCount++;
    }
}

void LocalFrameView::didLayout(SingleThreadWeakPtr<RenderElement> layoutRoot, bool didRunSimplifiedLayout, bool canDeferUpdateLayerPositions)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    m_layoutUpdateCount++;

    UpdateLayerPositions updateLayerPositions { layoutContext().layoutIdentifier(), layoutContext().needsFullRepaint(), didRunSimplifiedLayout };
    if (m_pendingUpdateLayerPositions)
        m_pendingUpdateLayerPositions->merge(updateLayerPositions);
    else
        m_pendingUpdateLayerPositions = updateLayerPositions;

    if (!canDeferUpdateLayerPositions)
        flushUpdateLayerPositions();

    m_updateCompositingLayersIsPending = true;

    Ref document = *m_frame->document();

#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK)
    if (CheckedPtr cache = document->existingAXObjectCache())
        cache->postNotification(layoutRoot.get(), AXObjectCache::AXLayoutComplete);
#else
    UNUSED_PARAM(layoutRoot);
#endif

    m_frame->invalidateContentEventRegionsIfNeeded(LocalFrame::InvalidateContentEventRegionsReason::Layout);
    document->invalidateRenderingDependentRegions();

    updateCanBlitOnScrollRecursively();

    handleDeferredScrollUpdateAfterContentSizeChange();

    handleDeferredScrollbarsUpdate();
    handleDeferredPositionScrollbarLayers();

    if (CheckedPtr markers = document->markersIfExists())
        markers->invalidateRectsForAllMarkers();
}

bool LocalFrameView::shouldDeferScrollUpdateAfterContentSizeChange()
{
    return (layoutContext().layoutPhase() < LocalFrameViewLayoutContext::LayoutPhase::InPostLayout) && (layoutContext().layoutPhase() != LocalFrameViewLayoutContext::LayoutPhase::OutsideLayout);
}

RenderBox* LocalFrameView::embeddedContentBox() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return nullptr;

    RenderObject* firstChild = renderView->firstChild();

    // Curently only embedded SVG documents participate in the size-negotiation logic.
    if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(firstChild))
        return svgRoot;

    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(firstChild))
        return svgRoot;

    return nullptr;
}

void LocalFrameView::addEmbeddedObjectToUpdate(RenderEmbeddedObject& embeddedObject)
{
    if (!m_embeddedObjectsToUpdate)
        m_embeddedObjectsToUpdate = makeUnique<ListHashSet<SingleThreadWeakRef<RenderEmbeddedObject>>>();

    auto& element = embeddedObject.frameOwnerElement();
    if (RefPtr embedOrObject = dynamicDowncast<HTMLPlugInImageElement>(element))
        embedOrObject->setNeedsWidgetUpdate(true);

    m_embeddedObjectsToUpdate->add(embeddedObject);
}

void LocalFrameView::removeEmbeddedObjectToUpdate(RenderEmbeddedObject& embeddedObject)
{
    if (!m_embeddedObjectsToUpdate)
        return;

    m_embeddedObjectsToUpdate->remove(&embeddedObject);
}

void LocalFrameView::setMediaType(const AtomString& mediaType)
{
    m_mediaType = mediaType;
}

AtomString LocalFrameView::mediaType() const
{
    // See if we have an override type.
    auto overrideType = m_frame->loader().client().overrideMediaType();
    InspectorInstrumentation::applyEmulatedMedia(m_frame.get(), overrideType);
    if (!overrideType.isNull())
        return overrideType;
    return m_mediaType;
}

void LocalFrameView::adjustMediaTypeForPrinting(bool printing)
{
    if (printing) {
        if (m_mediaTypeWhenNotPrinting.isNull())
            m_mediaTypeWhenNotPrinting = mediaType();
        setMediaType(printAtom());
    } else {
        if (!m_mediaTypeWhenNotPrinting.isNull())
            setMediaType(m_mediaTypeWhenNotPrinting);
        m_mediaTypeWhenNotPrinting = nullAtom();
    }
}

bool LocalFrameView::useSlowRepaints(bool considerOverlap) const
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

    if (auto* parentView = parentFrameView())
        return parentView->useSlowRepaints(considerOverlap);

    return false;
}

bool LocalFrameView::useSlowRepaintsIfNotOverlapped() const
{
    return useSlowRepaints(false);
}

void LocalFrameView::updateCanBlitOnScrollRecursively()
{
    for (RefPtr<Frame> frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (RefPtr view = localFrame->view())
            view->setCanBlitOnScroll(!view->useSlowRepaints());
    }
}

bool LocalFrameView::usesCompositedScrolling() const
{
    RenderView* renderView = this->renderView();
    if (renderView && renderView->isComposited()) {
        GraphicsLayer* layer = renderView->layer()->backing()->graphicsLayer();
        if (layer && layer->drawsContent())
            return true;
    }

    return false;
}

bool LocalFrameView::usesAsyncScrolling() const
{
#if ENABLE(ASYNC_SCROLLING)
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->coordinatesScrollingForFrameView(*this);
#endif
    return false;
}

bool LocalFrameView::mockScrollbarsControllerEnabled() const
{
    return m_frame->settings().mockScrollbarsControllerEnabled();
}

void LocalFrameView::logMockScrollbarsControllerMessage(const String& message) const
{
    RefPtr document = m_frame->document();
    if (!document)
        return;
    document->addConsoleMessage(MessageSource::Other, MessageLevel::Debug,
        makeString(m_frame->isMainFrame() ? "Main"_s : ""_s, "LocalFrameView: "_s, message));
}

String LocalFrameView::debugDescription() const
{
    return makeString("LocalFrameView 0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase), ' ', m_frame->debugDescription());
}

bool LocalFrameView::canShowNonOverlayScrollbars() const
{
    auto usesLegacyScrollbarStyle = [&] {
        RefPtr element = rootElementForCustomScrollbarPartStyle();
        if (!element)
            return false;

        if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(element->renderer()))
            return renderBox->style().usesLegacyScrollbarStyle();

        return false;
    }();

    return canHaveScrollbars() && (usesLegacyScrollbarStyle || !ScrollbarTheme::theme().usesOverlayScrollbars());
}

bool LocalFrameView::styleHidesScrollbarWithOrientation(ScrollbarOrientation orientation) const
{
    auto element = rootElementForCustomScrollbarPartStyle();
    if (!element)
        return false;
    auto* renderer = element->renderer();
    ASSERT(renderer); // rootElementForCustomScrollbarPart assures that it's not null.

    StyleScrollbarState scrollbarState;
    scrollbarState.scrollbarPart = ScrollbarBGPart;
    scrollbarState.orientation = orientation;
    auto scrollbarStyle = renderer->getUncachedPseudoStyle({ PseudoId::WebKitScrollbar, scrollbarState }, &renderer->style());
    return scrollbarStyle && scrollbarStyle->display() == DisplayType::None;
}

NativeScrollbarVisibility LocalFrameView::horizontalNativeScrollbarVisibility() const
{
    if (managesScrollbars()) {
        auto* scrollbar = horizontalScrollbar();
        return Scrollbar::nativeScrollbarVisibility(scrollbar);
    }

    return styleHidesScrollbarWithOrientation(ScrollbarOrientation::Horizontal) ? NativeScrollbarVisibility::HiddenByStyle : NativeScrollbarVisibility::Visible;
}

NativeScrollbarVisibility LocalFrameView::verticalNativeScrollbarVisibility() const
{
    if (managesScrollbars()) {
        auto* scrollbar = verticalScrollbar();
        return Scrollbar::nativeScrollbarVisibility(scrollbar);
    }

    return styleHidesScrollbarWithOrientation(ScrollbarOrientation::Vertical) ? NativeScrollbarVisibility::HiddenByStyle : NativeScrollbarVisibility::Visible;
}

void LocalFrameView::setCannotBlitToWindow()
{
    m_cannotBlitToWindow = true;
    updateCanBlitOnScrollRecursively();
}

void LocalFrameView::addSlowRepaintObject(RenderElement& renderer)
{
    bool hadSlowRepaintObjects = hasSlowRepaintObjects();

    if (!m_slowRepaintObjects)
        m_slowRepaintObjects = makeUnique<SingleThreadWeakHashSet<RenderElement>>();

    auto addResult = m_slowRepaintObjects->add(renderer);
    if (addResult.isNewEntry) {
        if (auto layer = renderer.enclosingLayer())
            layer->setNeedsScrollingTreeUpdate();
    }

    if (hadSlowRepaintObjects)
        return;

    updateCanBlitOnScrollRecursively();
}

void LocalFrameView::removeSlowRepaintObject(RenderElement& renderer)
{
    if (!m_slowRepaintObjects)
        return;

    bool removed = m_slowRepaintObjects->remove(renderer);
    if (removed) {
        if (auto layer = renderer.enclosingLayer())
            layer->setNeedsScrollingTreeUpdate();
    }

    if (!m_slowRepaintObjects->isEmptyIgnoringNullReferences())
        return;

    m_slowRepaintObjects = nullptr;
    updateCanBlitOnScrollRecursively();
}

bool LocalFrameView::hasSlowRepaintObject(const RenderElement& renderer) const
{
    return m_slowRepaintObjects && m_slowRepaintObjects->contains(renderer);
}

bool LocalFrameView::hasSlowRepaintObjects() const
{
    return m_slowRepaintObjects && !m_slowRepaintObjects->isEmptyIgnoringNullReferences();
}

bool LocalFrameView::hasViewportConstrainedObjects() const
{
    return m_viewportConstrainedObjects && !m_viewportConstrainedObjects->isEmptyIgnoringNullReferences();
}

void LocalFrameView::addViewportConstrainedObject(RenderLayerModelObject& object)
{
    if (!m_viewportConstrainedObjects)
        m_viewportConstrainedObjects = makeUnique<SingleThreadWeakHashSet<RenderLayerModelObject>>();

    if (!m_viewportConstrainedObjects->contains(object)) {
        m_viewportConstrainedObjects->add(object);
        if (platformWidget())
            updateCanBlitOnScrollRecursively();

        if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewFixedObjectsDidChange(*this);

        if (RefPtr page = m_frame->page())
            page->chrome().client().didAddOrRemoveViewportConstrainedObjects();
    }
}

void LocalFrameView::removeViewportConstrainedObject(RenderLayerModelObject& object)
{
    if (m_viewportConstrainedObjects && m_viewportConstrainedObjects->remove(object)) {
        if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewFixedObjectsDidChange(*this);

        // FIXME: In addFixedObject() we only call this if there's a platform widget,
        // why isn't the same check being made here?
        updateCanBlitOnScrollRecursively();

        if (RefPtr<Page> page = m_frame->page())
            page->chrome().client().didAddOrRemoveViewportConstrainedObjects();
    }
}

LayoutSize LocalFrameView::expandedLayoutViewportSize(const LayoutSize& baseLayoutViewportSize, const LayoutSize& documentSize, double heightExpansionFactor)
{
    if (!heightExpansionFactor)
        return baseLayoutViewportSize;

    auto documentHeight = documentSize.height();
    auto layoutViewportHeight = baseLayoutViewportSize.height();
    if (layoutViewportHeight > documentHeight)
        return baseLayoutViewportSize;

    if (auto expandedHeight = LayoutUnit((1 + heightExpansionFactor) * layoutViewportHeight); expandedHeight < documentHeight)
        layoutViewportHeight = expandedHeight;

    return { baseLayoutViewportSize.width(), std::min(documentHeight, layoutViewportHeight) };
}

LayoutRect LocalFrameView::computeUpdatedLayoutViewportRect(const LayoutRect& layoutViewport, const LayoutRect& documentRect, const LayoutSize& unobscuredContentSize, const LayoutRect& unobscuredContentRect, const LayoutSize& baseLayoutViewportSize, const LayoutPoint& stableLayoutViewportOriginMin, const LayoutPoint& stableLayoutViewportOriginMax, LayoutViewportConstraint constraint)
{
    LayoutRect layoutViewportRect = layoutViewport;
    
    // The layout viewport is never smaller than baseLayoutViewportSize, and never be smaller than the unobscuredContentRect.
    LayoutSize constrainedSize = baseLayoutViewportSize;
    layoutViewportRect.setSize(constrainedSize.expandedTo(unobscuredContentSize));
        
    LayoutPoint layoutViewportOrigin = computeLayoutViewportOrigin(unobscuredContentRect, stableLayoutViewportOriginMin, stableLayoutViewportOriginMax, layoutViewportRect, ScrollBehaviorForFixedElements::StickToViewportBounds);

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
LayoutPoint LocalFrameView::computeLayoutViewportOrigin(const LayoutRect& visualViewport, const LayoutPoint& stableLayoutViewportOriginMin, const LayoutPoint& stableLayoutViewportOriginMax, const LayoutRect& layoutViewport, ScrollBehaviorForFixedElements fixedBehavior)
{
    LayoutPoint layoutViewportOrigin = layoutViewport.location();
    bool allowRubberBanding = fixedBehavior == ScrollBehaviorForFixedElements::StickToViewportBounds;

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

void LocalFrameView::setBaseLayoutViewportOrigin(LayoutPoint origin, TriggerLayoutOrNot layoutTriggering)
{
    ASSERT(m_frame->settings().visualViewportEnabled());

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

void LocalFrameView::setLayoutViewportOverrideRect(std::optional<LayoutRect> rect, TriggerLayoutOrNot layoutTriggering)
{
    if (rect == m_layoutViewportOverrideRect)
        return;

    LayoutRect oldRect = layoutViewportRect();
    m_layoutViewportOverrideRect = rect;
    LayoutRect newRect = layoutViewportRect();

    if (oldRect != newRect) {
        invalidateScrollAnchoringElement();
        updateScrollAnchoringElement();
    }

    // Triggering layout on height changes is necessary to make bottom-fixed elements behave correctly.
    if (oldRect.height() != newRect.height())
        layoutTriggering = TriggerLayoutOrNot::Yes;

    LOG_WITH_STREAM(Scrolling, stream << "\nFrameView " << this << " setLayoutViewportOverrideRect() - changing override layout viewport from " << oldRect << " to " << valueOrDefault(m_layoutViewportOverrideRect) << " layoutTriggering " << (layoutTriggering == TriggerLayoutOrNot::Yes ? "yes" : "no"));

    if (layoutTriggering == TriggerLayoutOrNot::Yes) {
        if (oldRect != newRect)
            setViewportConstrainedObjectsNeedLayout();

        if (oldRect.size() != newRect.size()) {
            if (auto* document = m_frame->document())
                document->updateViewportUnitsOnResize();
        }
    }
}

void LocalFrameView::setVisualViewportOverrideRect(std::optional<LayoutRect> rect)
{
    m_visualViewportOverrideRect = rect;
}

LayoutSize LocalFrameView::baseLayoutViewportSize() const
{
    return renderView() ? renderView()->size() : size();
}

void LocalFrameView::updateLayoutViewport()
{
    if (!m_frame->settings().visualViewportEnabled())
        return;
    
    // Don't update the layout viewport if we're in the middle of adjusting scrollbars. We'll get another call
    // as a post-layout task.
    if (layoutContext().layoutPhase() == LocalFrameViewLayoutContext::LayoutPhase::InViewSizeAdjust)
        return;

    LayoutRect layoutViewport = layoutViewportRect();

    LOG_WITH_STREAM(Scrolling, stream << "\nFrameView " << this << " updateLayoutViewport() totalContentSize " << totalContentsSize() << " unscaledDocumentRect " << (renderView() ? renderView()->unscaledDocumentRect() : IntRect()) << " header height " << headerHeight() << " footer height " << footerHeight() << " fixed behavior " << scrollBehaviorForFixedElements());
    LOG_WITH_STREAM(Scrolling, stream << "layoutViewport: " << layoutViewport);
    LOG_WITH_STREAM(Scrolling, stream << "visualViewport: " << visualViewportRect() << " (is override " << (bool)m_visualViewportOverrideRect << ")");
    LOG_WITH_STREAM(Scrolling, stream << "stable origins: min: " << minStableLayoutViewportOrigin() << " max: "<< maxStableLayoutViewportOrigin());
    
    if (m_layoutViewportOverrideRect) {
        if (currentScrollType() == ScrollType::Programmatic) {
            LOG_WITH_STREAM(Scrolling, stream << "computing new override layout viewport because of programmatic scrolling");
            LayoutPoint newOrigin = computeLayoutViewportOrigin(visualViewportRect(), minStableLayoutViewportOrigin(), maxStableLayoutViewportOrigin(), layoutViewport, ScrollBehaviorForFixedElements::StickToDocumentBounds);
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

LayoutPoint LocalFrameView::minStableLayoutViewportOrigin() const
{
    return unscaledMinimumScrollPosition();
}

LayoutPoint LocalFrameView::maxStableLayoutViewportOrigin() const
{
    LayoutPoint maxPosition = unscaledMaximumScrollPosition();
    maxPosition = (maxPosition - LayoutSize(0, headerHeight() + footerHeight())).expandedTo({ });
    return maxPosition;
}

IntPoint LocalFrameView::unscaledScrollOrigin() const
{
    if (RenderView* renderView = this->renderView())
        return -renderView->unscaledDocumentRect().location(); // Akin to code in adjustViewSize().

    return { };
}

LayoutRect LocalFrameView::layoutViewportRect() const
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
LayoutRect LocalFrameView::visibleDocumentRect(const FloatRect& visibleContentRect, float headerHeight, float footerHeight, const FloatSize& totalContentsSize, float pageScaleFactor)
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

LayoutRect LocalFrameView::visualViewportRect() const
{
    if (m_visualViewportOverrideRect)
        return m_visualViewportOverrideRect.value();

    FloatRect visibleContentRect = this->visibleContentRect(LegacyIOSDocumentVisibleRect);
    return visibleDocumentRect(visibleContentRect, headerHeight(), footerHeight(), totalContentsSize(), frameScaleFactor());
}

LayoutRect LocalFrameView::viewportConstrainedVisibleContentRect() const
{
    ASSERT(!m_frame->settings().visualViewportEnabled());

#if PLATFORM(IOS_FAMILY)
    if (useCustomFixedPositionLayoutRect())
        return customFixedPositionLayoutRect();
#endif
    LayoutRect viewportRect = visibleContentRect();

    viewportRect.setLocation(scrollPositionForFixedPosition());
    return viewportRect;
}

LayoutRect LocalFrameView::rectForFixedPositionLayout() const
{
    if (m_frame->settings().visualViewportEnabled())
        return layoutViewportRect();

    return viewportConstrainedVisibleContentRect();
}

float LocalFrameView::frameScaleFactor() const
{
    return m_frame->frameScaleFactor();
}

LayoutPoint LocalFrameView::scrollPositionForFixedPosition() const
{
    if (m_frame->settings().visualViewportEnabled())
        return layoutViewportRect().location();

    return scrollPositionForFixedPosition(visibleContentRect(), totalContentsSize(), scrollPosition(), scrollOrigin(), frameScaleFactor(), fixedElementsLayoutRelativeToFrame(), scrollBehaviorForFixedElements(), headerHeight(), footerHeight());
}

LayoutPoint LocalFrameView::scrollPositionForFixedPosition(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements behaviorForFixed, int headerHeight, int footerHeight)
{
    LayoutPoint position;
    if (behaviorForFixed == ScrollBehaviorForFixedElements::StickToDocumentBounds)
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

float LocalFrameView::yPositionForInsetClipLayer(const FloatPoint& scrollPosition, float topContentInset)
{
    if (!topContentInset)
        return 0;

    // The insetClipLayer should not move for negative scroll values.
    float scrollY = std::max<float>(0, scrollPosition.y());

    if (scrollY >= topContentInset)
        return 0;

    return topContentInset - scrollY;
}

float LocalFrameView::yPositionForHeaderLayer(const FloatPoint& scrollPosition, float topContentInset)
{
    if (!topContentInset)
        return 0;

    float scrollY = std::max<float>(0, scrollPosition.y());

    if (scrollY >= topContentInset)
        return topContentInset;

    return scrollY;
}

float LocalFrameView::yPositionForFooterLayer(const FloatPoint& scrollPosition, float topContentInset, float totalContentsHeight, float footerHeight)
{
    return yPositionForHeaderLayer(scrollPosition, topContentInset) + totalContentsHeight - footerHeight;
}

FloatPoint LocalFrameView::positionForRootContentLayer(const FloatPoint& scrollPosition, const FloatPoint& scrollOrigin, float topContentInset, float headerHeight)
{
    return FloatPoint(0, yPositionForHeaderLayer(scrollPosition, topContentInset) + headerHeight) - toFloatSize(scrollOrigin);
}

FloatPoint LocalFrameView::positionForRootContentLayer() const
{
    return positionForRootContentLayer(scrollPosition(), scrollOrigin(), topContentInset(), headerHeight());
}

#if PLATFORM(IOS_FAMILY)
LayoutRect LocalFrameView::rectForViewportConstrainedObjects(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, float frameScaleFactor, bool fixedElementsLayoutRelativeToFrame, ScrollBehaviorForFixedElements scrollBehavior)
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
        
        AffineTransform rescaleTransform = AffineTransform::makeTranslation(toFloatSize(scaleOrigin));
        rescaleTransform.scale(frameScaleFactor / maxPostionedObjectsRectScale, frameScaleFactor / maxPostionedObjectsRectScale);
        rescaleTransform = CGAffineTransformTranslate(rescaleTransform, -scaleOrigin.x(), -scaleOrigin.y());

        viewportConstrainedObjectsRect = enclosingLayoutRect(rescaleTransform.mapRect(visibleContentRect));
    }
    
    if (scrollBehavior == ScrollBehaviorForFixedElements::StickToDocumentBounds) {
        LayoutRect documentBounds(LayoutPoint(), totalContentsSize);
        viewportConstrainedObjectsRect.intersect(documentBounds);
    }

    return viewportConstrainedObjectsRect;
}
    
LayoutRect LocalFrameView::viewportConstrainedObjectsRect() const
{
    return rectForViewportConstrainedObjects(visibleContentRect(), totalContentsSize(), m_frame->frameScaleFactor(), fixedElementsLayoutRelativeToFrame(), scrollBehaviorForFixedElements());
}
#endif
    
ScrollPosition LocalFrameView::minimumScrollPosition() const
{
    ScrollPosition minimumPosition = ScrollView::minimumScrollPosition();

    if (m_frame->isMainFrame() && m_scrollPinningBehavior == ScrollPinningBehavior::PinToBottom)
        minimumPosition.setY(maximumScrollPosition().y());
    
    return minimumPosition;
}

ScrollPosition LocalFrameView::maximumScrollPosition() const
{
    ScrollPosition maximumPosition = ScrollView::maximumScrollPosition();

    if (m_frame->isMainFrame() && m_scrollPinningBehavior == ScrollPinningBehavior::PinToTop)
        maximumPosition.setY(minimumScrollPosition().y());
    
    return maximumPosition;
}

ScrollPosition LocalFrameView::unscaledMinimumScrollPosition() const
{
    if (RenderView* renderView = this->renderView()) {
        IntRect unscaledDocumentRect = renderView->unscaledDocumentRect();
        ScrollPosition minimumPosition = unscaledDocumentRect.location();

        if (m_frame->isMainFrame() && m_scrollPinningBehavior == ScrollPinningBehavior::PinToBottom)
            minimumPosition.setY(unscaledMaximumScrollPosition().y());

        return minimumPosition;
    }

    return minimumScrollPosition();
}

ScrollPosition LocalFrameView::unscaledMaximumScrollPosition() const
{
    if (RenderView* renderView = this->renderView()) {
        IntRect unscaledDocumentRect = renderView->unscaledDocumentRect();
        unscaledDocumentRect.expand(0, headerHeight() + footerHeight());
        ScrollPosition maximumPosition = ScrollPosition(unscaledDocumentRect.maxXMaxYCorner() - visibleSize()).expandedTo({ 0, 0 });
        if (m_frame->isMainFrame() && m_scrollPinningBehavior == ScrollPinningBehavior::PinToTop)
            maximumPosition.setY(unscaledMinimumScrollPosition().y());

        return maximumPosition;
    }

    return maximumScrollPosition();
}

void LocalFrameView::viewportContentsChanged()
{
    if (!m_frame->view()) {
        // The frame is being destroyed.
        return;
    }

    if (RefPtr page = m_frame->page())
        page->updateValidationBubbleStateIfNeeded();

    // When the viewport contents changes (scroll, resize, style recalc, layout, ...),
    // check if we should resume animated images or unthrottle DOM timers.
    applyRecursivelyWithVisibleRect([] (LocalFrameView& frameView, const IntRect& visibleRect) {
        frameView.resumeVisibleImageAnimations(visibleRect);
        frameView.updateScriptedAnimationsAndTimersThrottlingState(visibleRect);

        if (auto* renderView = frameView.m_frame->contentRenderer())
            renderView->updateVisibleViewportRect(visibleRect);
    });
}

IntRect LocalFrameView::viewRectExpandedByContentInsets() const
{
    FloatRect viewRect;
    if (delegatesScrollingToNativeView() && platformWidget())
        viewRect = unobscuredContentRect();
    else
        viewRect = visualViewportRect();

    if (auto* page = m_frame->page())
        viewRect.expand(page->contentInsets());

    return IntRect(viewRect);
}

bool LocalFrameView::fixedElementsLayoutRelativeToFrame() const
{
    return m_frame->settings().fixedElementsLayoutRelativeToFrame();
}

IntPoint LocalFrameView::lastKnownMousePositionInView() const
{
    return convertFromContainingWindow(m_frame->eventHandler().lastKnownMousePosition());
}

bool LocalFrameView::isHandlingWheelEvent() const
{
    return m_frame->eventHandler().isHandlingWheelEvent();
}

bool LocalFrameView::shouldSetCursor() const
{
    Page* page = m_frame->page();
    return page && page->isVisible() && page->focusController().isActive();
}

#if ENABLE(DARK_MODE_CSS)
RenderObject* LocalFrameView::rendererForColorScheme() const
{
    auto* document = m_frame->document();
    auto* documentElement = document ? document->documentElement() : nullptr;
    auto* documentElementRenderer = documentElement ? documentElement->renderer() : nullptr;
    if (documentElementRenderer && documentElementRenderer->style().hasExplicitlySetColorScheme())
        return documentElementRenderer;
    return nullptr;
}
#endif

bool LocalFrameView::useDarkAppearance() const
{
#if ENABLE(DARK_MODE_CSS)
    if (auto* renderer = rendererForColorScheme())
        return renderer->useDarkAppearance();
#endif
    if (auto* document = m_frame->document())
        return document->useDarkAppearance(nullptr);
    return false;
}

OptionSet<StyleColorOptions> LocalFrameView::styleColorOptions() const
{
#if ENABLE(DARK_MODE_CSS)
    if (auto* renderer = rendererForColorScheme())
        return renderer->styleColorOptions();
#endif
    if (auto* document = m_frame->document())
        return document->styleColorOptions(nullptr);
    return { };
}

bool LocalFrameView::scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    if (!m_viewportConstrainedObjects || m_viewportConstrainedObjects->isEmptyIgnoringNullReferences()) {
        m_frame->page()->chrome().scroll(scrollDelta, rectToScroll, clipRect);
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
    m_frame->page()->chrome().scroll(scrollDelta, rectToScroll, clipRect);

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
        m_frame->page()->chrome().invalidateContentsAndRootView(updateRect);
    }

    return true;
}

void LocalFrameView::scrollContentsSlowPath(const IntRect& updateRect)
{
    repaintSlowRepaintObjects();

    if (!usesCompositedScrolling() && isEnclosedInCompositingLayer()) {
        if (RenderWidget* frameRenderer = m_frame->ownerRenderer()) {
            LayoutRect rect(frameRenderer->borderLeft() + frameRenderer->paddingLeft(), frameRenderer->borderTop() + frameRenderer->paddingTop(),
                visibleWidth(), visibleHeight());
            frameRenderer->repaintRectangle(rect);
            return;
        }
    }

    ScrollView::scrollContentsSlowPath(updateRect);
}

void LocalFrameView::repaintSlowRepaintObjects()
{
    if (!m_slowRepaintObjects)
        return;

    // Renderers with fixed backgrounds may be in compositing layers, so we need to explicitly
    // repaint them after scrolling.
    for (auto& renderer : *m_slowRepaintObjects)
        renderer.repaintSlowRepaintObject();
}

// Note that this gets called at painting time.
void LocalFrameView::setIsOverlapped(bool isOverlapped)
{
    if (isOverlapped == m_isOverlapped)
        return;

    m_isOverlapped = isOverlapped;
    updateCanBlitOnScrollRecursively();
}

void LocalFrameView::setContentIsOpaque(bool contentIsOpaque)
{
    if (contentIsOpaque == m_contentIsOpaque)
        return;

    m_contentIsOpaque = contentIsOpaque;
    updateCanBlitOnScrollRecursively();
}

void LocalFrameView::restoreScrollbar()
{
    setScrollbarsSuppressed(false);
}

bool LocalFrameView::scrollToFragment(const URL& url)
{
    ASSERT(m_frame->document());
    Ref document = *m_frame->document();
    
    auto fragmentIdentifier = url.fragmentIdentifier();
    auto fragmentDirective = document->fragmentDirective();
    
    if (m_frame->isMainFrame() && document->settings().scrollToTextFragmentEnabled() && !fragmentDirective.isEmpty()) {
        FragmentDirectiveParser fragmentDirectiveParser(fragmentDirective);
        
        if (fragmentDirectiveParser.isValid()) {
            auto parsedTextDirectives = fragmentDirectiveParser.parsedTextDirectives();
            
            auto highlightRanges = FragmentDirectiveRangeFinder::findRangesFromTextDirectives(parsedTextDirectives, document);
            if (m_frame->settings().scrollToTextFragmentMarkingEnabled()) {
                for (auto range : highlightRanges)
                    document->fragmentHighlightRegistry().addAnnotationHighlightWithRange(StaticRange::create(range));
            }

            if (highlightRanges.size()) {
                auto range = highlightRanges.first();
                RefPtr commonAncestor = commonInclusiveAncestor<ComposedTree>(range);
                if (commonAncestor && !is<Element>(commonAncestor))
                    commonAncestor = commonAncestor->parentElement();
                if (commonAncestor)
                    document->setCSSTarget(downcast<Element>(commonAncestor.get()));
                // FIXME: <http://webkit.org/b/245262> (Scroll To Text Fragment should use DelegateMainFrameScroll)
                TemporarySelectionChange selectionChange(document, { range }, { TemporarySelectionOption::RevealSelection, TemporarySelectionOption::RevealSelectionBounds, TemporarySelectionOption::UserTriggered, TemporarySelectionOption::ForceCenterScroll });
                maintainScrollPositionAtScrollToTextFragmentRange(range);
                if (m_frame->settings().scrollToTextFragmentIndicatorEnabled() && !m_frame->page()->isControlledByAutomation())
                    m_delayedTextFragmentIndicatorTimer.startOneShot(100_ms);
                return true;
            }
        }
    }
    
    if (scrollToFragmentInternal(fragmentIdentifier))
        return true;

    if (scrollToFragmentInternal(PAL::decodeURLEscapeSequences(fragmentIdentifier)))
        return true;

    resetScrollAnchor();
    return false;
}

bool LocalFrameView::scrollToFragmentInternal(StringView fragmentIdentifier)
{
    // If our URL has no ref, then we have no place we need to jump to.
    if (fragmentIdentifier.isNull())
        return false;

    LOG_WITH_STREAM(Scrolling, stream << *this << " scrollToFragmentInternal " << fragmentIdentifier);

    ASSERT(m_frame->document());
    auto& document = *m_frame->document();
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
    } else if (!anchorElement && !(fragmentIdentifier.isEmpty() || equalLettersIgnoringASCIICase(fragmentIdentifier, "top"_s))) {
        // Implement the rule that "" and "top" both mean top of page as in other browsers.
        return false;
    }

    RefPtr<ContainerNode> scrollPositionAnchor = anchorElement;
    if (!scrollPositionAnchor)
        scrollPositionAnchor = m_frame->document();
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

void LocalFrameView::maintainScrollPositionAtAnchor(ContainerNode* anchorNode)
{
    LOG(Scrolling, "LocalFrameView::maintainScrollPositionAtAnchor at %p", anchorNode);

    m_maintainScrollPositionAnchor = anchorNode;
    if (!m_maintainScrollPositionAnchor)
        return;

    cancelScheduledScrolls();

    if (RefPtr element = dynamicDowncast<Element>(anchorNode))
        m_frame->document()->updateContentRelevancyForScrollIfNeeded(*element);

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    m_frame->document()->updateStyleIfNeeded();
    // Only do a layout if changes have occurred that make it necessary.
    RenderView* renderView = this->renderView();
    if (renderView && renderView->needsLayout())
        layoutContext().layout();
    else
        scheduleScrollToAnchorAndTextFragment();

    scrollToAnchorAndTextFragmentNowIfNeeded();
}

void LocalFrameView::maintainScrollPositionAtScrollToTextFragmentRange(SimpleRange& range)
{
    m_pendingTextFragmentIndicatorRange = range;
    m_pendingTextFragmentIndicatorText = plainText(range);
    if (!m_pendingTextFragmentIndicatorRange)
        return;

    scrollToTextFragmentRange();
}

void LocalFrameView::scrollElementToRect(const Element& element, const IntRect& rect)
{
    m_frame->protectedDocument()->updateLayoutIgnorePendingStylesheets();

    LayoutRect bounds;
    if (RenderElement* renderer = element.renderer())
        bounds = renderer->absoluteAnchorRect();
    int centeringOffsetX = (rect.width() - bounds.width()) / 2;
    int centeringOffsetY = (rect.height() - bounds.height()) / 2;
    setScrollPosition(IntPoint(bounds.x() - centeringOffsetX - rect.x(), bounds.y() - centeringOffsetY - rect.y()));
}

void LocalFrameView::setScrollPosition(const ScrollPosition& scrollPosition, const ScrollPositionChangeOptions& options)
{
    LOG_WITH_STREAM(Scrolling, stream << "LocalFrameView::setScrollPosition " << scrollPosition << " animated " << (options.animated == ScrollIsAnimated::Yes) << ", clearing anchor");

    auto oldScrollType = currentScrollType();
    setCurrentScrollType(options.type);

    m_maintainScrollPositionAnchor = nullptr;
    cancelScheduledScrolls();

    Page* page = m_frame->page();
    if (page && page->isMonitoringWheelEvents())
        scrollAnimator().setWheelEventTestMonitor(page->wheelEventTestMonitor());

    ScrollOffset snappedOffset = ceiledIntPoint(scrollAnimator().scrollOffsetAdjustedForSnapping(scrollOffsetFromPosition(scrollPosition), options.snapPointSelectionMethod));
    auto snappedPosition = scrollPositionFromOffset(snappedOffset);

    if (options.animated == ScrollIsAnimated::Yes)
        scrollToPositionWithAnimation(snappedPosition, options);
    else
        ScrollView::setScrollPosition(snappedPosition, options);

    setCurrentScrollType(oldScrollType);
}

void LocalFrameView::resetScrollAnchor()
{
    ASSERT(m_frame->document());
    auto& document = *m_frame->document();

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

void LocalFrameView::scheduleScrollToFocusedElement(SelectionRevealMode selectionRevealMode)
{
    if (selectionRevealMode == SelectionRevealMode::DoNotReveal)
        return;

    m_selectionRevealModeForFocusedElement = selectionRevealMode;
    if (m_shouldScrollToFocusedElement)
        return;
    m_shouldScrollToFocusedElement = true;
    m_delayedScrollToFocusedElementTimer.startOneShot(0_s);
}

void LocalFrameView::cancelScheduledScrolls()
{
    cancelScheduledScrollToFocusedElement();
    cancelScheduledTextFragmentIndicatorTimer();
}

void LocalFrameView::cancelScheduledScrollToFocusedElement()
{
    m_shouldScrollToFocusedElement = false;
    m_delayedScrollToFocusedElementTimer.stop();
}

void LocalFrameView::scrollToFocusedElementImmediatelyIfNeeded()
{
    if (!m_shouldScrollToFocusedElement)
        return;

    m_delayedScrollToFocusedElementTimer.stop();
    scrollToFocusedElementInternal();
}

void LocalFrameView::scrollToFocusedElementTimerFired()
{
    scrollToFocusedElementInternal();
}

void LocalFrameView::scrollToFocusedElementInternal()
{
    RELEASE_ASSERT(m_shouldScrollToFocusedElement);
    RefPtr document = m_frame->document();
    if (!document)
        return;

    document->updateLayoutIgnorePendingStylesheets();
    if (!m_shouldScrollToFocusedElement || m_delayedScrollToFocusedElementTimer.isActive())
        return; // Updating the layout may have ran scripts.

    m_shouldScrollToFocusedElement = false;

    RefPtr focusedElement = document->focusedElement();
    if (!focusedElement)
        return;
    auto updateTarget = focusedElement->focusAppearanceUpdateTarget();
    if (!updateTarget)
        return;
    
    // Get the scroll-margin of the shadow host when we're inside a user agent shadow root.
    if (updateTarget->containingShadowRoot() && updateTarget->containingShadowRoot()->mode() == ShadowRootMode::UserAgent)
        updateTarget = updateTarget->shadowHost();

    auto* renderer = updateTarget->renderer();
    if (!renderer || renderer->isRenderWidget())
        return;

    bool insideFixed;
    auto absoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed);
    auto anchorRectWithScrollMargin = absoluteBounds.marginRect;
    auto anchorRect = absoluteBounds.anchorRect;
    LocalFrameView::scrollRectToVisible(anchorRectWithScrollMargin, *renderer, insideFixed, { m_selectionRevealModeForFocusedElement, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, ShouldAllowCrossOriginScrolling::No, ScrollBehavior::Auto, anchorRect });
}

void LocalFrameView::textFragmentIndicatorTimerFired()
{
    ASSERT(m_frame->document());
    auto& document = *m_frame->document();
    
    m_delayedTextFragmentIndicatorTimer.stop();
    
    if (!m_pendingTextFragmentIndicatorRange)
        return;
    
    if (m_pendingTextFragmentIndicatorText != plainText(m_pendingTextFragmentIndicatorRange.value()))
        return;
    
    auto range = m_pendingTextFragmentIndicatorRange.value();

    // FIXME: <http://webkit.org/b/245262> (Scroll To Text Fragment should use DelegateMainFrameScroll)
    TemporarySelectionChange selectionChange(document, { range }, { TemporarySelectionOption::RevealSelection, TemporarySelectionOption::RevealSelectionBounds, TemporarySelectionOption::UserTriggered, TemporarySelectionOption::ForceCenterScroll });
    
    maintainScrollPositionAtScrollToTextFragmentRange(range);
    
    auto textIndicator = TextIndicator::createWithRange(range, { TextIndicatorOption::DoNotClipToVisibleRect }, WebCore::TextIndicatorPresentationTransition::Bounce);
    
    auto* page = m_frame->page();
    
    if (!page)
        return;
    
    if (textIndicator) {
        auto* localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
        if (!localMainFrame)
            return;

        auto textRects = RenderObject::absoluteTextRects(range);

        constexpr auto hitType = OptionSet { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
        auto result = localMainFrame->eventHandler().hitTestResultAtPoint(LayoutPoint(textRects.first().center()), hitType);
        if (!intersects(range, *result.protectedTargetNode()))
            return;
        
        if (textRects.size() >= 2) {
            result = localMainFrame->eventHandler().hitTestResultAtPoint(LayoutPoint(textRects[1].center()), hitType);
            if (!intersects(range, *result.protectedTargetNode()))
                return;
        }
        
        if (textRects.size() >= 4) {
            result = localMainFrame->eventHandler().hitTestResultAtPoint(LayoutPoint(textRects.last().center()), hitType);
            if (!intersects(range, *result.protectedTargetNode()))
                return;
            result = localMainFrame->eventHandler().hitTestResultAtPoint(LayoutPoint(textRects[textRects.size() - 2].center()), hitType);
            if (!intersects(range, *result.protectedTargetNode()))
                return;
        }
        document.protectedPage()->chrome().client().setTextIndicator(textIndicator->data());
    }
}

void LocalFrameView::cancelScheduledTextFragmentIndicatorTimer()
{
    if (m_skipScrollResetOfScrollToTextFragmentRange)
        return;
    m_pendingTextFragmentIndicatorRange.reset();
    m_pendingTextFragmentIndicatorText = String();
    m_delayedTextFragmentIndicatorTimer.stop();
}

bool LocalFrameView::scrollRectToVisible(const LayoutRect& absoluteRect, const RenderObject& renderer, bool insideFixed, const ScrollRectToVisibleOptions& options)
{
    if (options.revealMode == SelectionRevealMode::DoNotReveal)
        return false;

    if (renderer.isSkippedContent())
        return false;

    auto* layer = renderer.enclosingLayer();
    if (!layer)
        return false;

    // FIXME: It would be nice to use RenderLayer::enclosingScrollableLayer here, but that seems to skip overflow:hidden layers.
    auto adjustedRect = absoluteRect;
    for (; layer; layer = layer->enclosingContainingBlockLayer(CrossFrameBoundaries::No)) {
        if (layer->shouldTryToScrollForScrollIntoView())
            adjustedRect = layer->ensureLayerScrollableArea()->scrollRectToVisible(adjustedRect, options);
    }

    auto& frameView = renderer.view().frameView();
    RefPtr ownerElement = frameView.m_frame->document() ? frameView.m_frame->document()->ownerElement() : nullptr;
    if (ownerElement && ownerElement->renderer())
        frameView.scrollRectToVisibleInChildView(adjustedRect, insideFixed, options, ownerElement.get());
    else
        frameView.scrollRectToVisibleInTopLevelView(adjustedRect, insideFixed, options);
    return true;
}

static ScrollPositionChangeOptions scrollPositionChangeOptionsForElement(const LocalFrameView& frameView, Element* element, const ScrollRectToVisibleOptions& options)
{
    auto scrollPositionOptions = ScrollPositionChangeOptions::createProgrammatic();
    if (!frameView.frame().eventHandler().autoscrollInProgress()
        && element
        && useSmoothScrolling(options.behavior, element))
        scrollPositionOptions.animated = ScrollIsAnimated::Yes;
    return scrollPositionOptions;
};

void LocalFrameView::scrollRectToVisibleInChildView(const LayoutRect& absoluteRect, bool insideFixed, const ScrollRectToVisibleOptions& options, const HTMLFrameOwnerElement* ownerElement)
{
    // If scrollbars aren't explicitly forbidden, permit scrolling.
    if (m_frame->scrollingMode() == ScrollbarMode::AlwaysOff) {
        // If scrollbars are forbidden, user initiated scrolls should obviously be ignored.
        if (wasScrolledByUser())
            return;
        // Forbid autoscrolls when scrollbars are off, but permits other programmatic scrolls, like navigation to an anchor.
        if (m_frame->eventHandler().autoscrollInProgress())
            return;
    }

    // If this assertion fires we need to protect the ownerElement from being destroyed.
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    auto viewRect = visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);

    // scroll-padding applies to the scroll container, but expand the rectangle that we want to expose in order
    // simulate padding the scroll container. This rectangle is passed up the tree of scrolling elements to
    // ensure that the padding on this scroll container is maintained.
    auto targetRect = absoluteRect;
    auto* element = ownerElement->contentDocument() ? ownerElement->contentDocument()->documentElement() : nullptr;
    if (auto* renderer = element ? element->renderBox() : nullptr)
        targetRect.expand(renderer->scrollPaddingForViewportRect(viewRect));

    auto revealRect = getPossiblyFixedRectToExpose(viewRect, targetRect, insideFixed, options.alignX, options.alignY);
    auto scrollPosition = roundedIntPoint(revealRect.location());
    scrollPosition = constrainedScrollPosition(scrollPosition);

    // FIXME: Should we use contentDocument()->scrollingElement()?
    // See https://bugs.webkit.org/show_bug.cgi?id=205059
    setScrollPosition(scrollPosition, scrollPositionChangeOptionsForElement(*this, element, options));

    if (options.shouldAllowCrossOriginScrolling == ShouldAllowCrossOriginScrolling::No && !safeToPropagateScrollToParent()) 
        return;

    // FIXME: ideally need to determine if this <iframe> is inside position:fixed.
    if (auto* ownerRenderer = ownerElement->renderer())
        scrollRectToVisible(contentsToContainingViewContents(enclosingIntRect(targetRect)), *ownerRenderer, false /* insideFixed */, options);
}

void LocalFrameView::scrollRectToVisibleInTopLevelView(const LayoutRect& absoluteRect, bool insideFixed, const ScrollRectToVisibleOptions& options)
{
    if (options.revealMode == SelectionRevealMode::RevealUpToMainFrame && m_frame->isMainFrame())
        return;

    auto* page = m_frame->page();
    if (!page)
        return;

    if (options.revealMode == SelectionRevealMode::DelegateMainFrameScroll && m_frame->isMainFrame()) {
        page->chrome().scrollMainFrameToRevealRect(snappedIntRect(absoluteRect));
        return;
    }

    auto minScrollPosition = minimumScrollPosition();
    auto maxScrollPosition = maximumScrollPosition();

#if !PLATFORM(IOS_FAMILY)
    auto viewRect = visibleContentRect();
#else
    // FIXME: ContentInsets should be taken care of in UI process side. webkit.org/b/199682
    // To do that, getRectToExposeForScrollIntoView needs to return the additional scrolling to do beyond content rect.
    LayoutRect viewRect = viewRectExpandedByContentInsets();

    // FIXME: webkit.org/b/199683 LocalFrameView::visibleContentRect is wrong when content insets are present
    maxScrollPosition = scrollPositionFromOffset(ScrollPosition(totalContentsSize() - flooredIntSize(viewRect.size())));

    auto contentInsets = page->contentInsets();
    minScrollPosition.move(-contentInsets.left(), -contentInsets.top());
    maxScrollPosition.move(contentInsets.right(), contentInsets.bottom());
#endif
    // Move the target rect into "scrollView contents" coordinates.
    auto targetRect = absoluteRect;
    targetRect.move(0, headerHeight());

    // scroll-padding applies to the scroll container, but expand the rectangle that we want to expose in order
    // simulate padding the scroll container. This rectangle is passed up the tree of scrolling elements to
    // ensure that the padding on this scroll container is maintained.
    auto* element = m_frame->document() ? m_frame->document()->documentElement() : nullptr;
    if (auto* renderBox = element ? element->renderBox() : nullptr)
        targetRect.expand(renderBox->scrollPaddingForViewportRect(viewRect));

    LayoutRect revealRect = getPossiblyFixedRectToExpose(viewRect, targetRect, insideFixed, options.alignX, options.alignY);

    // Avoid scrolling to the rounded value of revealRect.location() if we don't actually need to scroll
    if (revealRect != viewRect) {
        // FIXME: Should we use document()->scrollingElement()?
        // See https://bugs.webkit.org/show_bug.cgi?id=205059
        ScrollOffset clampedScrollPosition = roundedIntPoint(revealRect.location()).constrainedBetween(minScrollPosition, maxScrollPosition);
        setScrollPosition(clampedScrollPosition, scrollPositionChangeOptionsForElement(*this, element, options));
    }

    // This is the outermost view of a web page, so after scrolling this view we
    // scroll its container by calling Page::scrollMainFrameToRevealRect.
    // This only has an effect on the Mac platform in applications
    // that put web views into scrolling containers, such as Mac OS X Mail.
    // The canAutoscroll function in EventHandler also knows about this.
    page->chrome().scrollContainingScrollViewsToRevealRect(snappedIntRect(absoluteRect));
}

void LocalFrameView::contentsResized()
{
    // For non-delegated scrolling, updateTiledBackingAdaptiveSizing() is called via addedOrRemovedScrollbar() which occurs less often.
    if (delegatesScrollingToNativeView())
        updateTiledBackingAdaptiveSizing();
}

void LocalFrameView::delegatedScrollingModeDidChange()
{
    auto* renderView = this->renderView();
    if (!renderView)
        return;

    RenderLayerCompositor& compositor = renderView->compositor();
    // When we switch to delegatesScrolling mode, we should destroy the scrolling/clipping layers in RenderLayerCompositor.
    // FIXME: Is this right? What turns compositing back on?
    if (compositor.usesCompositing()) {
        ASSERT(compositor.usesCompositing());
        compositor.enableCompositingMode(false);
        compositor.clearBackingForAllLayers();
    }
}

void LocalFrameView::setViewportConstrainedObjectsNeedLayout()
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

void LocalFrameView::didChangeScrollOffset()
{
    if (auto* page = m_frame->page()) {
        page->pageOverlayController().didScrollFrame(m_frame.get());
        InspectorInstrumentation::didScroll(*page);
    }

    m_frame->loader().client().didChangeScrollOffset();
}

void LocalFrameView::scrollOffsetChangedViaPlatformWidgetImpl(const ScrollOffset& oldOffset, const ScrollOffset& newOffset)
{
#if PLATFORM(COCOA)
    auto deferrer = WheelEventTestMonitorCompletionDeferrer { m_frame->page()->wheelEventTestMonitor().get(), scrollingNodeIDForTesting(), WheelEventTestMonitor::DeferReason::ContentScrollInProgress };
#endif

    updateLayerPositionsAfterScrolling();
    updateCompositingLayersAfterScrolling();
    repaintSlowRepaintObjects();
    scrollPositionChanged(scrollPositionFromOffset(oldOffset), scrollPositionFromOffset(newOffset));
    invalidateScrollAnchoringElement();
    updateScrollAnchoringElement();

    if (auto* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().didChangeVisibleRect();
    }
}

// These scroll positions are affected by zooming.
void LocalFrameView::scrollPositionChanged(const ScrollPosition& oldPosition, const ScrollPosition& newPosition)
{
    UNUSED_PARAM(oldPosition);
    UNUSED_PARAM(newPosition);

    Page* page = m_frame->page();
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

    LOG_WITH_STREAM(Scrolling, stream << "LocalFrameView " << this << " scrollPositionChanged from " << oldPosition << " to " << newPosition << " (scale " << frameScaleFactor() << " )");

    updateLayoutViewport();
    viewportContentsChanged();

    if (auto* renderView = this->renderView()) {
        if (auto* layer = renderView->layer())
            m_frame->editor().renderLayerDidScroll(*layer);
    }
}

void LocalFrameView::applyRecursivelyWithVisibleRect(const Function<void(LocalFrameView& frameView, const IntRect& visibleRect)>& apply)
{
    IntRect windowClipRect = this->windowClipRect();
    auto visibleRect = windowToContents(windowClipRect);
    apply(*this, visibleRect);

    // Recursive call for subframes. We cache the current LocalFrameView's windowClipRect to avoid recomputing it for every subframe.
    SetForScope windowClipRectCache(m_cachedWindowClipRect, &windowClipRect);
    for (Frame* childFrame = m_frame->tree().firstChild(); childFrame; childFrame = childFrame->tree().nextSibling()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(childFrame);
        if (!localFrame)
            continue;
        if (auto* childView = localFrame->view())
            childView->applyRecursivelyWithVisibleRect(apply);
    }
}

void LocalFrameView::resumeVisibleImageAnimations(const IntRect& visibleRect)
{
    if (visibleRect.isEmpty())
        return;

    if (auto* renderView = m_frame->contentRenderer())
        renderView->resumePausedImageAnimationsIfNeeded(visibleRect);
}

void LocalFrameView::updateScriptedAnimationsAndTimersThrottlingState(const IntRect& visibleRect)
{
    if (m_frame->isMainFrame())
        return;

    auto* document = m_frame->document();
    if (!document)
        return;

    // We don't throttle zero-size or display:none frames because those are usually utility frames.
    bool shouldThrottle = visibleRect.isEmpty() && !m_lastUsedSizeForLayout.isEmpty() && m_frame->ownerRenderer();
    document->setTimerThrottlingEnabled(shouldThrottle);

    auto* page = m_frame->page();
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


void LocalFrameView::resumeVisibleImageAnimationsIncludingSubframes()
{
    applyRecursivelyWithVisibleRect([] (LocalFrameView& frameView, const IntRect& visibleRect) {
        frameView.resumeVisibleImageAnimations(visibleRect);
    });
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void LocalFrameView::updatePlayStateForAllAnimations(const IntRect& visibleRect)
{
    if (auto* renderView = m_frame->contentRenderer())
        renderView->updatePlayStateForAllAnimations(visibleRect);
}

void LocalFrameView::updatePlayStateForAllAnimationsIncludingSubframes()
{
    applyRecursivelyWithVisibleRect([] (LocalFrameView& frameView, const IntRect& visibleRect) {
        frameView.updatePlayStateForAllAnimations(visibleRect);
    });
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

void LocalFrameView::updateLayerPositionsAfterScrolling()
{
    // If we're scrolling as a result of updating the view size after layout, we'll update widgets and layer positions soon anyway.
    if (layoutContext().layoutPhase() == LocalFrameViewLayoutContext::LayoutPhase::InViewSizeAdjust)
        return;

    if (!layoutContext().isLayoutNested() && hasViewportConstrainedObjects()) {
        if (auto* renderView = this->renderView()) {
            updateWidgetPositions();
            flushUpdateLayerPositions();
            renderView->layer()->updateLayerPositionsAfterDocumentScroll();
        }
    }
}

void LocalFrameView::updateLayerPositionsAfterOverflowScroll(RenderLayer& layer)
{
    flushUpdateLayerPositions();
    layer.updateLayerPositionsAfterOverflowScroll();
    scheduleUpdateWidgetPositions();
}

ScrollingCoordinator* LocalFrameView::scrollingCoordinator() const
{
    return m_frame->page() ? m_frame->page()->scrollingCoordinator() : nullptr;
}

bool LocalFrameView::shouldUpdateCompositingLayersAfterScrolling() const
{
#if ENABLE(ASYNC_SCROLLING)
    // If the scrolling thread is updating the fixed elements, then the LocalFrameView should not update them as well.

    Page* page = m_frame->page();
    if (!page)
        return true;

    if (&page->mainFrame() != m_frame.ptr())
        return true;

    RefPtr scrollingCoordinator = this->scrollingCoordinator();
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

void LocalFrameView::updateCompositingLayersAfterScrolling()
{
    ASSERT(layoutContext().layoutPhase() >= LocalFrameViewLayoutContext::LayoutPhase::InPostLayout || layoutContext().layoutPhase() == LocalFrameViewLayoutContext::LayoutPhase::OutsideLayout);

    if (!shouldUpdateCompositingLayersAfterScrolling())
        return;

    if (!layoutContext().isLayoutNested() && hasViewportConstrainedObjects()) {
        if (RenderView* renderView = this->renderView())
            renderView->compositor().updateCompositingLayers(CompositingUpdateType::OnScroll);
    }
}

bool LocalFrameView::isUserScrollInProgress() const
{
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator()) {
        if (scrollingCoordinator->isUserScrollInProgress(scrollingNodeID()))
            return true;
    }

    if (auto scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isUserScrollInProgress();

    return false;
}

bool LocalFrameView::isRubberBandInProgress() const
{
    if (scrollbarsSuppressed())
        return false;

    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->isRubberBandInProgress(scrollingNodeID());

    if (auto scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();

    return false;
}

bool LocalFrameView::isInStableState() const
{
    if (auto* page = m_frame->page())
        return page->chrome().client().isInStableState();
    return FrameView::isInStableState();
}

bool LocalFrameView::requestStartKeyboardScrollAnimation(const KeyboardScroll& scrollData)
{
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->requestStartKeyboardScrollAnimation(*this, scrollData);

    return false;
}

bool LocalFrameView::requestStopKeyboardScrollAnimation(bool immediate)
{
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->requestStopKeyboardScrollAnimation(*this, immediate);

    return false;
}

bool LocalFrameView::requestScrollToPosition(const ScrollPosition& position, const ScrollPositionChangeOptions& options)
{
    LOG_WITH_STREAM(Scrolling, stream << "LocalFrameView::requestScrollToPosition " << position << " options  " << options);

#if ENABLE(ASYNC_SCROLLING)
    if (auto* tiledBacking = this->tiledBacking(); tiledBacking && options.animated == ScrollIsAnimated::No) {
#if PLATFORM(IOS_FAMILY)
        auto contentSize = exposedContentRect().size();
#else
        auto contentSize = visibleContentRect().size();
#endif
        tiledBacking->prepopulateRect(FloatRect(position, contentSize));
    }
#endif

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->requestScrollToPosition(*this, position, options);
#else
    UNUSED_PARAM(position);
    UNUSED_PARAM(options);
#endif

    return false;
}

void LocalFrameView::stopAsyncAnimatedScroll()
{
#if ENABLE(ASYNC_SCROLLING)
    LOG_WITH_STREAM(Scrolling, stream << "LocalFrameView::stopAsyncAnimatedScroll");

    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->stopAnimatedScroll(*this);
#endif
}

void LocalFrameView::addTrackedRepaintRect(const FloatRect& r)
{
    if (!m_isTrackingRepaints || r.isEmpty())
        return;

    FloatRect repaintRect = r;
    repaintRect.moveBy(-scrollPosition());
    m_trackedRepaintRects.append(repaintRect);
}

void LocalFrameView::repaintContentRectangle(const IntRect& r)
{
    ASSERT(!m_frame->ownerElement());

    if (!shouldUpdate())
        return;

    ScrollView::repaintContentRectangle(r);
}

static unsigned countRenderedCharactersInRenderObjectWithThreshold(const RenderElement& renderer, unsigned threshold)
{
    unsigned count = 0;
    for (const RenderObject* descendant = &renderer; descendant; descendant = descendant->nextInPreOrder()) {
        if (CheckedPtr renderText = dynamicDowncast<RenderText>(*descendant)) {
            count += renderText->text().length();
            if (count >= threshold)
                break;
        }
    }
    return count;
}

bool LocalFrameView::renderedCharactersExceed(unsigned threshold)
{
    if (!m_frame->contentRenderer())
        return false;
    return countRenderedCharactersInRenderObjectWithThreshold(*m_frame->contentRenderer(), threshold) >= threshold;
}

void LocalFrameView::availableContentSizeChanged(AvailableSizeChangeReason reason)
{
    if (Document* document = m_frame->document()) {
        // FIXME: Merge this logic with m_setNeedsLayoutWasDeferred and find a more appropriate
        // way of handling potential recursive layouts when the viewport is resized to accomodate
        // the content but the content always overflows the viewport. See webkit.org/b/165781.
        if (layoutContext().layoutPhase() != LocalFrameViewLayoutContext::LayoutPhase::InViewSizeAdjust || !useFixedLayout())
            document->updateViewportUnitsOnResize();
    }

    updateLayoutViewport();
    setNeedsLayoutAfterViewConfigurationChange();
    ScrollView::availableContentSizeChanged(reason);
}

bool LocalFrameView::shouldLayoutAfterContentsResized() const
{
    return !useFixedLayout() || useCustomFixedPositionLayoutRect();
}

void LocalFrameView::updateContentsSize()
{
    // We check to make sure the view is attached to a frame() as this method can
    // be triggered before the view is attached by Frame::createView(...) setting
    // various values such as setScrollBarModes(...) for example.  An ASSERT is
    // triggered when a view is layout before being attached to a m_frame->
    if (!m_frame->view())
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

void LocalFrameView::addedOrRemovedScrollbar()
{
    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidAddOrRemoveScrollbars();
    }

    updateTiledBackingAdaptiveSizing();

    InspectorInstrumentation::didAddOrRemoveScrollbars(*this);
}

OptionSet<TiledBacking::Scrollability> LocalFrameView::computeScrollability() const
{
    auto* page = m_frame->page();

    // Use smaller square tiles if the Window is not active to facilitate app napping.
    if (!page || !page->isWindowActive())
        return { TiledBacking::Scrollability::HorizontallyScrollable, TiledBacking::Scrollability::VerticallyScrollable };

    bool horizontallyScrollable;
    bool verticallyScrollable;
    bool clippedByAncestorView = static_cast<bool>(m_viewExposedRect);

#if PLATFORM(IOS_FAMILY)
    if (page)
        clippedByAncestorView |= page->enclosedInScrollableAncestorView();
#endif

    if (delegatesScrollingToNativeView()) {
        IntSize documentSize = contentsSize();
        IntSize visibleSize = this->visibleSize();

        horizontallyScrollable = clippedByAncestorView || documentSize.width() > visibleSize.width();
        verticallyScrollable = clippedByAncestorView || documentSize.height() > visibleSize.height();
    } else {
        horizontallyScrollable = clippedByAncestorView || horizontalScrollbar();
        verticallyScrollable = clippedByAncestorView || verticalScrollbar();
    }

    OptionSet<TiledBacking::Scrollability> scrollability = TiledBacking::Scrollability::NotScrollable;
    if (horizontallyScrollable)
        scrollability.add(TiledBacking::Scrollability::HorizontallyScrollable);

    if (verticallyScrollable)
        scrollability.add(TiledBacking::Scrollability::VerticallyScrollable);

    return scrollability;
}

void LocalFrameView::updateTiledBackingAdaptiveSizing()
{
    auto* tiledBacking = this->tiledBacking();
    if (!tiledBacking)
        return;

    tiledBacking->setScrollability(computeScrollability());
}

// FIXME: This shouldn't be called from outside; LocalFrameView should call it when the relevant viewports change.
void LocalFrameView::layoutOrVisualViewportChanged()
{
    if (m_frame->settings().visualViewportAPIEnabled()) {
        if (auto* window = m_frame->window())
            window->visualViewport().update();

        if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewVisualViewportChanged(*this);
    }
}

void LocalFrameView::unobscuredContentSizeChanged()
{
#if PLATFORM(IOS_FAMILY)
    updateTiledBackingAdaptiveSizing();
#endif
}

void LocalFrameView::loadProgressingStatusChanged()
{
    if (m_firstVisuallyNonEmptyLayoutMilestoneIsPending && m_frame->loader().isComplete())
        fireLayoutRelatedMilestonesIfNeeded();
    adjustTiledBackingCoverage();
}

void LocalFrameView::adjustTiledBackingCoverage()
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

static bool shouldEnableSpeculativeTilingDuringLoading(const LocalFrameView& view)
{
    RefPtr page = view.frame().page();
    return page && view.isVisuallyNonEmpty() && !page->progress().isMainLoadProgressing();
}

void LocalFrameView::enableSpeculativeTilingIfNeeded()
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

void LocalFrameView::speculativeTilingEnableTimerFired()
{
    if (m_speculativeTilingEnabled)
        return;
    m_speculativeTilingEnabled = shouldEnableSpeculativeTilingDuringLoading(*this);
    adjustTiledBackingCoverage();
}

void LocalFrameView::show()
{
    ScrollView::show();

    if (m_frame->isMainFrame()) {
        // Turn off speculative tiling for a brief moment after a LocalFrameView appears on screen.
        // Note that adjustTiledBackingCoverage() kicks the (500ms) timer to re-enable it.
        m_speculativeTilingEnabled = false;
        m_wasScrolledByUser = false;
        adjustTiledBackingCoverage();
    }
}

void LocalFrameView::hide()
{
    ScrollView::hide();
    adjustTiledBackingCoverage();
}

bool LocalFrameView::needsLayout() const
{
    return layoutContext().needsLayout();
}

void LocalFrameView::setNeedsLayoutAfterViewConfigurationChange()
{
    layoutContext().setNeedsLayoutAfterViewConfigurationChange();
}

void LocalFrameView::setNeedsCompositingConfigurationUpdate()
{
    RenderView* renderView = this->renderView();
    if (renderView && renderView->usesCompositing()) {
        if (auto* rootLayer = renderView->layer())
            rootLayer->setNeedsCompositingConfigurationUpdate();
        renderView->compositor().scheduleCompositingLayerUpdate();
    }
}

void LocalFrameView::setNeedsCompositingGeometryUpdate()
{
    RenderView* renderView = this->renderView();
    if (renderView->usesCompositing()) {
        if (auto* rootLayer = renderView->layer())
            rootLayer->setNeedsCompositingGeometryUpdate();
        renderView->compositor().scheduleCompositingLayerUpdate();
    }
}

void LocalFrameView::setDescendantsNeedUpdateBackingAndHierarchyTraversal()
{
    RenderView* renderView = this->renderView();
    if (renderView->usesCompositing()) {
        if (auto* rootLayer = renderView->layer())
            rootLayer->setDescendantsNeedUpdateBackingAndHierarchyTraversal();
        renderView->compositor().scheduleCompositingLayerUpdate();
    }
}

void LocalFrameView::scheduleSelectionUpdate()
{
    if (needsLayout())
        return;
    // FIXME: We should not need to go through the layout process since selection update does not change dimension/geometry.
    // However we can't tell at this point if the tree is stable yet, so let's just schedule a root only layout for now.
    setNeedsLayoutAfterViewConfigurationChange();
}

bool LocalFrameView::isTransparent() const
{
    return m_isTransparent;
}

void LocalFrameView::setTransparent(bool isTransparent)
{
    if (m_isTransparent == isTransparent)
        return;

    m_isTransparent = isTransparent;

    // setTransparent can be called in the window between LocalFrameView initialization
    // and switching in the new Document; this means that the RenderView that we
    // retrieve is actually attached to the previous Document, which is going away,
    // and must not update compositing layers.
    if (!isViewForDocumentInFrame())
        return;

    setNeedsLayoutAfterViewConfigurationChange();
    setNeedsCompositingConfigurationUpdate();
}

bool LocalFrameView::hasOpaqueBackground() const
{
    return !m_isTransparent && m_baseBackgroundColor.isOpaque();
}

Color LocalFrameView::baseBackgroundColor() const
{
    return m_baseBackgroundColor;
}

void LocalFrameView::setBaseBackgroundColor(const Color& backgroundColor)
{
    Color newBaseBackgroundColor = backgroundColor.isValid() ? backgroundColor : Color::white;
    if (m_baseBackgroundColor == newBaseBackgroundColor)
        return;

#if ENABLE(DARK_MODE_CSS)
    m_styleColorOptions = styleColorOptions();
#endif
    m_baseBackgroundColor = newBaseBackgroundColor;

    if (!isViewForDocumentInFrame())
        return;

    recalculateScrollbarOverlayStyle();
    setNeedsLayoutAfterViewConfigurationChange();
    setNeedsCompositingConfigurationUpdate();
}

#if ENABLE(DARK_MODE_CSS)
void LocalFrameView::updateBaseBackgroundColorIfNecessary()
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

void LocalFrameView::updateBackgroundRecursively(const std::optional<Color>& backgroundColor)
{
    auto intrinsicBaseBackgroundColor = [](LocalFrameView& view) -> Color {
#if HAVE(OS_DARK_MODE_SUPPORT)
#if PLATFORM(COCOA)
        static const auto cssValueControlBackground = CSSValueAppleSystemControlBackground;
#else
        static const auto cssValueControlBackground = CSSValueWindow;
#endif
        return RenderTheme::singleton().systemColor(cssValueControlBackground, view.styleColorOptions());
#endif
        return Color::white;
    };

    for (Frame* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;

        if (RefPtr view = localFrame->view()) {
            auto baseBackgroundColor = backgroundColor.value_or(intrinsicBaseBackgroundColor(*view));
            view->setTransparent(!baseBackgroundColor.isVisible());
            view->setBaseBackgroundColor(baseBackgroundColor);
            if (view->needsLayout())
                view->layoutContext().scheduleLayout();
        }
    }
}

bool LocalFrameView::hasExtendedBackgroundRectForPainting() const
{
    TiledBacking* tiledBacking = this->tiledBacking();
    if (!tiledBacking)
        return false;

    return tiledBacking->hasMargins();
}

void LocalFrameView::updateExtendBackgroundIfNecessary()
{
    ExtendedBackgroundMode mode = calculateExtendedBackgroundMode();
    if (mode == ExtendedBackgroundModeNone)
        return;

    updateTilesForExtendedBackgroundMode(mode);
}

LocalFrameView::ExtendedBackgroundMode LocalFrameView::calculateExtendedBackgroundMode() const
{
#if PLATFORM(IOS_FAMILY)
    // <rdar://problem/16201373>
    return ExtendedBackgroundModeNone;
#else
    if (!m_frame->settings().backgroundShouldExtendBeyondPage())
        return ExtendedBackgroundModeNone;

    // Just because Settings::backgroundShouldExtendBeyondPage() is true does not necessarily mean
    // that the background rect needs to be extended for painting. Simple backgrounds can be extended
    // just with RenderLayerCompositor's rootExtendedBackgroundColor. More complicated backgrounds,
    // such as images, require extending the background rect to continue painting into the extended
    // region. This function finds out if it is necessary to extend the background rect for painting.

    if (!m_frame->isMainFrame())
        return ExtendedBackgroundModeNone;

    Document* document = m_frame->document();
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
    auto backgroundRepeat = rootBackgroundRenderer->style().backgroundRepeat();
    if (backgroundRepeat.x == FillRepeat::Repeat)
        mode |= ExtendedBackgroundModeHorizontal;
    if (backgroundRepeat.y == FillRepeat::Repeat)
        mode |= ExtendedBackgroundModeVertical;

    return mode;
#endif
}

void LocalFrameView::updateTilesForExtendedBackgroundMode(ExtendedBackgroundMode mode)
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

IntRect LocalFrameView::extendedBackgroundRectForPainting() const
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

bool LocalFrameView::shouldUpdateWhileOffscreen() const
{
    return m_shouldUpdateWhileOffscreen;
}

void LocalFrameView::setShouldUpdateWhileOffscreen(bool shouldUpdateWhileOffscreen)
{
    m_shouldUpdateWhileOffscreen = shouldUpdateWhileOffscreen;
}

bool LocalFrameView::shouldUpdate() const
{
    if (isOffscreen() && !shouldUpdateWhileOffscreen())
        return false;
    return true;
}

bool LocalFrameView::safeToPropagateScrollToParent() const
{
    RefPtr document = m_frame->document();
    if (!document)
        return false;

    RefPtr parentFrame = m_frame->tree().parent();
    if (!parentFrame)
        return false;
    
    RefPtr localParent = dynamicDowncast<LocalFrame>(parentFrame);
    if (!localParent)
        return false;

    RefPtr parentDocument = localParent->document();
    if (!parentDocument)
        return false;

    return document->protectedSecurityOrigin()->isSameOriginDomain(parentDocument->protectedSecurityOrigin());
}

void LocalFrameView::scheduleScrollToAnchorAndTextFragment()
{
    RefPtr<ContainerNode> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;
    m_scheduledMaintainScrollPositionAnchor = anchorNode;

    if (m_scheduledToScrollToAnchor)
        return;

    RefPtr document = m_frame->document();
    ASSERT(document);

    m_scheduledToScrollToAnchor = true;
    document->eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (!protectedThis->m_maintainScrollPositionAnchor) {
            SetForScope scrollPositionAnchor(protectedThis->m_maintainScrollPositionAnchor, protectedThis->m_scheduledMaintainScrollPositionAnchor);
            protectedThis->scrollToAnchorAndTextFragmentNowIfNeeded();
        } else
            protectedThis->scrollToAnchorAndTextFragmentNowIfNeeded();
        protectedThis->m_scheduledMaintainScrollPositionAnchor = nullptr;
    });
}

void LocalFrameView::scrollToAnchorAndTextFragmentNowIfNeeded()
{
    if (!m_scheduledToScrollToAnchor)
        return;

    m_scheduledToScrollToAnchor = false;
    scrollToAnchor();
    scrollToTextFragmentRange();
}

void LocalFrameView::scrollToAnchor()
{
    RefPtr<ContainerNode> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;

    LOG_WITH_STREAM(Scrolling, stream << *this << " scrollToAnchor() " << anchorNode.get());

    if (!anchorNode->renderer())
        return;

    cancelScheduledScrolls();

    LayoutRect rect;
    bool insideFixed = false;
    if (anchorNode != m_frame->document() && anchorNode->renderer())
        rect = anchorNode->renderer()->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;

    LOG_WITH_STREAM(Scrolling, stream << " anchor node rect " << rect);

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    if (anchorNode->renderer()->writingMode().isHorizontal())
        scrollRectToVisible(rect, *anchorNode->renderer(), insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways, ShouldAllowCrossOriginScrolling::No });
    else if (anchorNode->renderer()->writingMode().blockDirection() == FlowDirection::RightToLeft)
        scrollRectToVisible(rect, *anchorNode->renderer(), insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignRightAlways, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::No });
    else
        scrollRectToVisible(rect, *anchorNode->renderer(), insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignLeftAlways, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::No });

    if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
        cache->handleScrolledToAnchor(*anchorNode);

    // scrollRectToVisible can call into setScrollPosition(), which resets m_maintainScrollPositionAnchor.
    LOG_WITH_STREAM(Scrolling, stream << " restoring anchor node to " << anchorNode.get());
    m_maintainScrollPositionAnchor = anchorNode;
    cancelScheduledScrolls();
}

void LocalFrameView::scrollToTextFragmentRange()
{
    if (!m_pendingTextFragmentIndicatorRange)
        return;

    if (needsLayout())
        return;

    auto range = *m_pendingTextFragmentIndicatorRange;
    auto rangeText = plainText(range);
    if (m_pendingTextFragmentIndicatorText != plainText(range))
        return;

    LOG_WITH_STREAM(Scrolling, stream << *this << " scrollToTextFragmentRange() " << range);

    if (!range.startContainer().renderer() || !range.endContainer().renderer())
        return;

    ASSERT(m_frame->document());
    Ref document = *m_frame->document();

    SetForScope skipScrollResetOfScrollToTextFragmentRange(m_skipScrollResetOfScrollToTextFragmentRange, true);

    // FIXME: <http://webkit.org/b/245262> (Scroll To Text Fragment should use DelegateMainFrameScroll)
    TemporarySelectionChange selectionChange(document, { range }, { TemporarySelectionOption::RevealSelection, TemporarySelectionOption::RevealSelectionBounds, TemporarySelectionOption::UserTriggered, TemporarySelectionOption::ForceCenterScroll });
}

void LocalFrameView::updateEmbeddedObject(const SingleThreadWeakPtr<RenderEmbeddedObject>& embeddedObject)
{
    // The to-be-updated renderer is passed in as WeakPtr for convenience, but it will be alive at this point.
    ASSERT(embeddedObject);

    // No need to update if it's already crashed or known to be missing.
    if (embeddedObject->isPluginUnavailable())
        return;

    if (RefPtr embedOrObject = dynamicDowncast<HTMLPlugInImageElement>(embeddedObject->frameOwnerElement())) {
        if (embedOrObject->needsWidgetUpdate())
            embedOrObject->updateWidget(CreatePlugins::Yes);
    } else
        ASSERT_NOT_REACHED();

    // It's possible the renderer was destroyed below updateWidget() since loading a plugin may execute arbitrary JavaScript.
    if (!embeddedObject)
        return;

    auto ignoreWidgetState = embeddedObject->updateWidgetPosition();
    UNUSED_PARAM(ignoreWidgetState);
}

bool LocalFrameView::updateEmbeddedObjects()
{
    SetForScope inUpdateEmbeddedObjects(m_inUpdateEmbeddedObjects, true);
    if (layoutContext().isLayoutNested() || !m_embeddedObjectsToUpdate || m_embeddedObjectsToUpdate->isEmpty())
        return true;

    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    // Make sure we process at most numberOfObjectsToUpdate objects to avoid
    // potential infinite loops if objects get added to m_embeddedObjectsToUpdate
    // during the execution of this loop.
    unsigned numberOfObjectsToUpdate = m_embeddedObjectsToUpdate->size();
    for (unsigned i = 0; i < numberOfObjectsToUpdate; ++i) {
        if (m_embeddedObjectsToUpdate->isEmpty())
            break;
        WeakPtr embeddedObject = m_embeddedObjectsToUpdate->takeFirst().get();
        updateEmbeddedObject(embeddedObject);
    }

    return m_embeddedObjectsToUpdate->isEmpty();
}

void LocalFrameView::updateEmbeddedObjectsTimerFired()
{
    m_updateEmbeddedObjectsTimer.stop();
    for (unsigned i = 0; i < maxUpdateEmbeddedObjectsIterations; ++i) {
        if (updateEmbeddedObjects())
            break;
    }
}

void LocalFrameView::flushAnyPendingPostLayoutTasks()
{
    layoutContext().flushPostLayoutTasks();
    if (m_updateEmbeddedObjectsTimer.isActive())
        updateEmbeddedObjectsTimerFired();
}

CheckedRef<const LocalFrameViewLayoutContext> LocalFrameView::checkedLayoutContext() const
{
    return m_layoutContext;
}

CheckedRef<LocalFrameViewLayoutContext> LocalFrameView::checkedLayoutContext()
{
    return m_layoutContext;
}

void LocalFrameView::performPostLayoutTasks()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    // FIXME: We should not run any JavaScript code in this function.
    LOG(Layout, "LocalFrameView %p performPostLayoutTasks", this);
    updateHasReachedSignificantRenderedTextThreshold();

    if (!layoutContext().isLayoutNested() && m_frame->document()->documentElement())
        fireLayoutRelatedMilestonesIfNeeded();

#if PLATFORM(IOS_FAMILY)
    // Only send layout-related delegate callbacks synchronously for the main frame to
    // avoid re-entering layout for the main frame while delivering a layout-related delegate
    // callback for a subframe.
    if (m_frame->isMainFrame()) {
        if (Page* page = m_frame->page())
            page->chrome().client().didLayout();
    }
#endif
    
    // FIXME: We should consider adding DidLayout as a LayoutMilestone. That would let us merge this
    // with didLayout(LayoutMilestones).
    m_frame->loader().client().dispatchDidLayout();

    updateWidgetPositions();

    updateSnapOffsets();

    m_updateEmbeddedObjectsTimer.startOneShot(0_s);

    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewLayoutUpdated(*this);

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor().frameViewDidLayout();
    }

    scheduleScrollToAnchorAndTextFragment();

    scheduleResizeEventIfNeeded();

    updateLayoutViewport();
    viewportContentsChanged();

    resnapAfterLayout();

    m_frame->document()->scheduleDeferredAXObjectCacheUpdate();
}

void LocalFrameView::dequeueScrollableAreaForScrollAnchoringUpdate(ScrollableArea& scrollableArea)
{
    m_scrollableAreasWithScrollAnchoringControllersNeedingUpdate.remove(scrollableArea);
}

void LocalFrameView::queueScrollableAreaForScrollAnchoringUpdate(ScrollableArea& scrollableArea)
{
    m_scrollableAreasWithScrollAnchoringControllersNeedingUpdate.add(scrollableArea);
}

void LocalFrameView::updateScrollAnchoringElementsForScrollableAreas()
{
    updateScrollAnchoringElement();
    if (!m_scrollableAreas)
        return;
    for (auto& scrollableArea : *m_scrollableAreas)
        scrollableArea.updateScrollAnchoringElement();
}

void LocalFrameView::updateScrollAnchoringPositionForScrollableAreas()
{
    auto scrollableAreasNeedingUpdate = std::exchange(m_scrollableAreasWithScrollAnchoringControllersNeedingUpdate, { });
    for (auto& scrollableArea : scrollableAreasNeedingUpdate)
        scrollableArea.updateScrollPositionForScrollAnchoringController();
}

IntSize LocalFrameView::sizeForResizeEvent() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_customSizeForResizeEvent)
        return *m_customSizeForResizeEvent;
#endif
    if (useFixedLayout() && !fixedLayoutSize().isEmpty() && delegatesScrolling())
        return fixedLayoutSize();
    return visibleContentRectIncludingScrollbars().size();
}

void LocalFrameView::scheduleResizeEventIfNeeded()
{
    if (layoutContext().isInRenderTreeLayout() || needsLayout())
        return;

    RenderView* renderView = this->renderView();
    if (!renderView || renderView->printing())
        return;

    auto* page = m_frame->page();
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
    if (RefPtr documentLoader = m_frame->loader().documentLoader()) {
        if (documentLoader->isLoadingInAPISense())
            return;
    }
#endif

    RefPtr document = m_frame->document();
    if (document->quirks().shouldSilenceWindowResizeEvents()) {
        document->addConsoleMessage(MessageSource::Other, MessageLevel::Info, "Window resize events silenced due to: http://webkit.org/b/258597"_s);
        FRAMEVIEW_RELEASE_LOG(Events, "scheduleResizeEventIfNeeded: Not firing resize events because they are temporarily disabled for this page");
        return;
    }

    // TODO: move this to a method called for all scrollable areas
    invalidateScrollAnchoringElement();
    updateScrollAnchoringElement();

    LOG_WITH_STREAM(Events, stream << "LocalFrameView " << this << " scheduleResizeEventIfNeeded scheduling resize event for document" << document << ", size " << currentSize);
    document->setNeedsDOMWindowResizeEvent();

    bool isMainFrame = m_frame->isMainFrame();
    if (InspectorInstrumentation::hasFrontends() && isMainFrame) {
        if (InspectorClient* inspectorClient = page ? page->inspectorController().inspectorClient() : nullptr)
            inspectorClient->didResizeMainFrame(m_frame.ptr());
    }
}

void LocalFrameView::willStartLiveResize()
{
    ScrollView::willStartLiveResize();
    adjustTiledBackingCoverage();
}
    
void LocalFrameView::willEndLiveResize()
{
    ScrollView::willEndLiveResize();
    adjustTiledBackingCoverage();
}

void LocalFrameView::autoSizeIfEnabled()
{
    if (!m_shouldAutoSize)
        return;

    if (m_inAutoSize)
        return;

    auto* document = m_frame->document();
    if (!document)
        return;

    auto* renderView = document->renderView();
    if (!renderView)
        return;

    auto* firstChild = renderView->firstChild();
    if (!firstChild)
        return;

    SetForScope changeInAutoSize(m_inAutoSize, true);
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

    if (auto* page = m_frame->page(); page && m_frame->isMainFrame())
        page->chrome().client().intrinsicContentsSizeChanged(m_autoSizeContentSize);

    m_didRunAutosize = true;
}

void LocalFrameView::performFixedWidthAutoSize()
{
    LOG(Layout, "LocalFrameView %p performFixedWidthAutoSize", this);

    RefPtr document = m_frame->document();
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

    Ref<LocalFrameView> protectedThis(*this);
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

void LocalFrameView::performSizeToContentAutoSize()
{
    // Do the resizing twice. The first time is basically a rough calculation using the preferred width
    // which may result in a height change during the second iteration.
    // Let's ignore renderers with viewport units first and resolve these boxes during the second phase of the autosizing.
    LOG(Layout, "LocalFrameView %p performSizeToContentAutoSize", this);
    ASSERT(m_frame->document() && m_frame->document()->renderView());

    auto document = m_frame->protectedDocument();
    auto& renderView = *document->renderView();
    auto layoutWithAdjustedStyleIfNeeded = [&] {
        document->updateStyleIfNeeded();
        if (auto* documentRenderer = downcast<RenderElement>(renderView.firstChild())) {
            auto& style = documentRenderer->mutableStyle();
            if (style.logicalHeight().isPercent()) {
                // Percent height values on the document renderer when we don't really have a proper viewport size can
                // result incorrect rendering in certain layout contexts (e.g flex).
                style.setLogicalHeight({ });
            }
        }
        document->updateLayout();
    };

    resetOverriddenWidthForCSSDefaultViewportUnits();
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
            && !m_frame->loader().isComplete() && (newSize.height() < size.height() || newSize.width() < size.width()))
            break;

        // The first time around, resize to the minimum height again; otherwise,
        // on pages (e.g. quirks mode) where the body/document resize to the view size,
        // we'll end up not shrinking back down after resizing to the computed preferred width.
        resize(newSize.width(), i ? newSize.height() : minAutoSize.height());
        // Protect the content from evergrowing layout.
        auto preferredViewportWidth = std::min(newSize.width(), m_autoSizeConstraint.width());
        overrideWidthForCSSDefaultViewportUnits(preferredViewportWidth);
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

void LocalFrameView::setAutoSizeFixedMinimumHeight(int fixedMinimumHeight)
{
    if (m_autoSizeFixedMinimumHeight == fixedMinimumHeight)
        return;

    m_autoSizeFixedMinimumHeight = fixedMinimumHeight;

    setNeedsLayoutAfterViewConfigurationChange();
}

RenderElement* LocalFrameView::viewportRenderer() const
{
    if (m_viewportRendererType == ViewportRendererType::None)
        return nullptr;

    auto* document = m_frame->document();
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

const Pagination& LocalFrameView::pagination() const
{
    if (m_pagination != Pagination())
        return m_pagination;

    if (m_frame->isMainFrame()) {
        if (Page* page = m_frame->page())
            return page->pagination();
    }

    return m_pagination;
}

void LocalFrameView::setPagination(const Pagination& pagination)
{
    if (m_pagination == pagination)
        return;

    m_pagination = pagination;

    m_frame->document()->styleScope().didChangeStyleSheetEnvironment();
}

IntRect LocalFrameView::windowClipRect() const
{
    ASSERT(m_frame->view() == this);

    if (m_cachedWindowClipRect)
        return *m_cachedWindowClipRect;

    if (paintsEntireContents())
        return contentsToWindow(IntRect(IntPoint(), totalContentsSize()));

    // Set our clip rect to be our contents.
    IntRect clipRect = contentsToWindow(visibleContentRect(LegacyIOSDocumentVisibleRect));

    if (!m_frame->ownerElement())
        return clipRect;

    // Take our owner element and get its clip rect.
    RefPtr ownerElement = m_frame->ownerElement();
    if (auto* parentView = ownerElement->document().view())
        clipRect.intersect(parentView->windowClipRectForFrameOwner(ownerElement.get(), true));
    return clipRect;
}

IntRect LocalFrameView::windowClipRectForFrameOwner(const HTMLFrameOwnerElement* ownerElement, bool clipToLayerContents) const
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

void LocalFrameView::scrollTo(const ScrollPosition& newPosition)
{
    IntPoint oldPosition = scrollPosition();
    ScrollView::scrollTo(newPosition);
    if (oldPosition != scrollPosition())
        scrollPositionChanged(oldPosition, scrollPosition());

    didChangeScrollOffset();
}

float LocalFrameView::adjustVerticalPageScrollStepForFixedContent(float step)
{
    TrackedRendererListHashSet* positionedObjects = nullptr;
    if (RenderView* root = m_frame->contentRenderer()) {
        if (!root->hasPositionedObjects())
            return step;
        positionedObjects = root->positionedObjects();
    }

    FloatRect unobscuredContentRect = this->unobscuredContentRect();
    float topObscuredArea = 0;
    float bottomObscuredArea = 0;
    for (const auto& positionedObject : *positionedObjects) {
        const RenderStyle& style = positionedObject.style();
        if (style.position() != PositionType::Fixed || style.usedVisibility() == Visibility::Hidden || !style.opacity())
            continue;

        FloatQuad contentQuad = positionedObject.absoluteContentQuad();
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

void LocalFrameView::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    // Add in our offset within the LocalFrameView.
    IntRect dirtyRect = rect;
    dirtyRect.moveBy(scrollbar.location());
    invalidateRect(dirtyRect);
}

void LocalFrameView::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
{
    if (!m_frame->isMainFrame())
        return;

    if (Page* page = m_frame->page())
        page->chrome().client().notifyScrollerThumbIsVisibleInRect(scrollerThumb);
}

bool LocalFrameView::isScrollable(Scrollability definitionOfScrollable)
{
    // Check for:
    // 1) If there an actual overflow.
    // 2) display:none or visibility:hidden set to self or inherited.
    // 3) overflow{-x,-y}: hidden;
    // 4) scrolling: no;
    if (!didFirstLayout())
        return false;

#if HAVE(RUBBER_BANDING)
    bool requiresActualOverflowToBeConsideredScrollable = !m_frame->isMainFrame() || definitionOfScrollable != Scrollability::ScrollableOrRubberbandable;
#else
    UNUSED_PARAM(definitionOfScrollable);
    bool requiresActualOverflowToBeConsideredScrollable = true;
#endif

    // Covers #1
    if (requiresActualOverflowToBeConsideredScrollable) {
        IntSize totalContentsSize = this->totalContentsSize();
        IntSize visibleContentSize = visibleContentRect(LegacyIOSDocumentVisibleRect).size();
        if (totalContentsSize.height() <= visibleContentSize.height() && totalContentsSize.width() <= visibleContentSize.width())
            return false;
    }

    // Covers #2.
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
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

bool LocalFrameView::isScrollableOrRubberbandable()
{
    return isScrollable(Scrollability::ScrollableOrRubberbandable);
}

bool LocalFrameView::hasScrollableOrRubberbandableAncestor()
{
    if (m_frame->isMainFrame())
        return isScrollableOrRubberbandable();

    for (auto* parent = this->parentFrameView(); parent; parent = parent->parentFrameView()) {
        Scrollability frameScrollability = parent->m_frame->isMainFrame() ? Scrollability::ScrollableOrRubberbandable : Scrollability::Scrollable;
        if (parent->isScrollable(frameScrollability))
            return true;
    }

    return false;
}

void LocalFrameView::updateScrollableAreaSet()
{
    // That ensures that only inner frames are cached.
    auto* parentFrameView = this->parentFrameView();
    if (!parentFrameView)
        return;

    if (!isScrollable()) {
        parentFrameView->removeScrollableArea(this);
        return;
    }

    parentFrameView->addScrollableArea(this);
}

bool LocalFrameView::shouldSuspendScrollAnimations() const
{
    return m_frame->loader().state() != FrameState::Complete;
}

void LocalFrameView::notifyAllFramesThatContentAreaWillPaint() const
{
    notifyScrollableAreasThatContentAreaWillPaint();

    for (RefPtr child = m_frame->tree().firstRenderedChild(); child; child = child->tree().traverseNextRendered(m_frame.ptr())) {
        RefPtr localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        if (RefPtr frameView = localChild->view())
            frameView->notifyScrollableAreasThatContentAreaWillPaint();
    }
}

void LocalFrameView::notifyScrollableAreasThatContentAreaWillPaint() const
{
    Page* page = m_frame->page();
    if (!page)
        return;

    contentAreaWillPaint();

    if (!m_scrollableAreas)
        return;

    for (auto& area : *m_scrollableAreas) {
        CheckedPtr<ScrollableArea> scrollableArea(area);
        // ScrollView ScrollableAreas will be handled via the Frame tree traversal above.
        if (!is<ScrollView>(scrollableArea))
            scrollableArea->contentAreaWillPaint();
    }
}

void LocalFrameView::updateScrollCorner()
{
    CheckedPtr<RenderElement> renderer;
    std::unique_ptr<RenderStyle> cornerStyle;
    IntRect cornerRect = scrollCornerRect();
    RefPtr doc = m_frame->document();

    auto usesStandardScrollbarStyle = [](auto& doc) {
        if (!doc || !doc->documentElement())
            return false;
        if (!doc->documentElement()->renderer())
            return false;

        return doc->documentElement()->renderer()->style().usesStandardScrollbarStyle();
    };

    if (!(usesStandardScrollbarStyle(doc) || cornerRect.isEmpty())) {
        // Try the <body> element first as a scroll corner source.
        RefPtr body = doc ? doc->bodyOrFrameset() : nullptr;
        if (body && body->renderer()) {
            renderer = body->renderer();
            cornerStyle = renderer->getUncachedPseudoStyle({ PseudoId::WebKitScrollbarCorner }, &renderer->style());
        }
        
        if (!cornerStyle) {
            // If the <body> didn't have a custom style, then the root element might.
            RefPtr docElement = doc ? doc->documentElement() : nullptr;
            if (docElement && docElement->renderer()) {
                renderer = docElement->renderer();
                cornerStyle = renderer->getUncachedPseudoStyle({ PseudoId::WebKitScrollbarCorner }, &renderer->style());
            }
        }
        
        if (!cornerStyle) {
            // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
            // FIXME: Seems wrong to do this for cross-origin frames.
            if (RefPtr renderer = m_frame->ownerRenderer())
                cornerStyle = renderer->getUncachedPseudoStyle({ PseudoId::WebKitScrollbarCorner }, &renderer->style());
        }
    }

    if (!cornerStyle || !renderer)
        m_scrollCorner = nullptr;
    else {
        if (!m_scrollCorner) {
            m_scrollCorner = createRenderer<RenderScrollbarPart>(renderer->protectedDocument(), WTFMove(*cornerStyle));
            m_scrollCorner->initializeStyle();
        } else
            m_scrollCorner->setStyle(WTFMove(*cornerStyle));
        invalidateScrollCorner(cornerRect);
    }
}

void LocalFrameView::paintScrollCorner(GraphicsContext& context, const IntRect& cornerRect)
{
    if (context.invalidatingControlTints()) {
        updateScrollCorner();
        return;
    }

    if (m_scrollCorner) {
        if (m_frame->isMainFrame())
            context.fillRect(cornerRect, baseBackgroundColor());
        m_scrollCorner->paintIntoRect(context, cornerRect.location(), cornerRect);
        return;
    }

    ScrollView::paintScrollCorner(context, cornerRect);
}

void LocalFrameView::paintScrollbar(GraphicsContext& context, Scrollbar& bar, const IntRect& rect)
{
    if (bar.isCustomScrollbar() && m_frame->isMainFrame()) {
        IntRect toFill = bar.frameRect();
        toFill.intersect(rect);
        context.fillRect(toFill, baseBackgroundColor());
    }

    ScrollView::paintScrollbar(context, bar, rect);
}

Color LocalFrameView::documentBackgroundColor() const
{
    // <https://bugs.webkit.org/show_bug.cgi?id=59540> We blend the background color of
    // the document and the body against the base background color of the frame view.
    // Background images are unfortunately impractical to include.

    // Return invalid Color objects whenever there is insufficient information.
    auto* backgroundDocument = [&] {
#if ENABLE(FULLSCREEN_API)
        if (auto* page = m_frame->page()) {
            if (auto* fullscreenDocument = page->outermostFullscreenDocument())
                return fullscreenDocument;
        }
#endif
        return m_frame->document();
    }();

    if (!backgroundDocument)
        return Color();

    auto* htmlElement = backgroundDocument->documentElement();
    auto* bodyElement = backgroundDocument->bodyOrFrameset();

    // Start with invalid colors.
    Color htmlBackgroundColor;
    Color bodyBackgroundColor;
    if (htmlElement && htmlElement->renderer())
        htmlBackgroundColor = htmlElement->renderer()->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (bodyElement && bodyElement->renderer())
        bodyBackgroundColor = bodyElement->renderer()->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

#if ENABLE(FULLSCREEN_API)
    Color fullscreenBackgroundColor = [&] () -> Color {
        CheckedPtr fullscreenManager = backgroundDocument->fullscreenManagerIfExists();
        if (!fullscreenManager)
            return { };

        RefPtr fullscreenElement = fullscreenManager->fullscreenElement();
        if (!fullscreenElement)
            return { };

        auto* fullscreenRenderer = fullscreenElement->renderer();
        if (!fullscreenRenderer)
            return { };

        auto fullscreenElementColor = fullscreenRenderer->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

        WeakPtr backdropRenderer = fullscreenRenderer->backdropRenderer();
        if (!backdropRenderer)
            return fullscreenElementColor;

        // Do not blend the fullscreenElementColor atop the backdrop color. The backdrop should
        // intentionally be visible underneath (and around) the fullscreen element.
        return backdropRenderer->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    }();

    // Replace or blend the fullscreen background color with the body background color, if present.
    if (fullscreenBackgroundColor.isValid()) {
        if (!bodyBackgroundColor.isValid())
            bodyBackgroundColor = fullscreenBackgroundColor;
        else
            bodyBackgroundColor = blendSourceOver(bodyBackgroundColor, fullscreenBackgroundColor);
    }
#endif

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

bool LocalFrameView::hasCustomScrollbars() const
{
    for (auto& widget : children()) {
        if (RefPtr frame = dynamicDowncast<LocalFrameView>(widget)) {
            if (frame->hasCustomScrollbars())
                return true;
        } else if (RefPtr scrollbar = dynamicDowncast<Scrollbar>(widget)) {
            if (scrollbar->isCustomScrollbar())
                return true;
        }
    }
    return false;
}

LocalFrameView* LocalFrameView::parentFrameView() const
{
    if (!parent())
        return nullptr;
    auto* parentFrame = dynamicDowncast<LocalFrame>(m_frame->tree().parent());
    if (!parentFrame)
        return nullptr;
    return parentFrame->view();
}

void LocalFrameView::updateControlTints()
{
    // This is called when control tints are changed from aqua/graphite to clear and vice versa.
    // We do a "fake" paint, and when the theme gets a paint call, it can then do an invalidate.
    // This is only done if the theme supports control tinting. It's up to the theme and platform
    // to define when controls get the tint and to call this function when that changes.
    
    // Optimize the common case where we bring a window to the front while it's still empty.
    if (m_frame->document()->url().isEmpty())
        return;

    // As noted above, this is a "fake" paint, so we should pause counting relevant repainted objects.
    Page* page = m_frame->page();
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

void LocalFrameView::invalidateControlTints()
{
    traverseForPaintInvalidation(NullGraphicsContext::PaintInvalidationReasons::InvalidatingControlTints);
}

void LocalFrameView::invalidateImagesWithAsyncDecodes()
{
    traverseForPaintInvalidation(NullGraphicsContext::PaintInvalidationReasons::InvalidatingImagesWithAsyncDecodes);
}

void LocalFrameView::updateAccessibilityObjectRegions()
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!AXObjectCache::accessibilityEnabled() || !AXObjectCache::isIsolatedTreeEnabled())
        return;

    NullGraphicsContext graphicsContext;
    AccessibilityRegionContext accessibilityRegionContext;
    paint(graphicsContext, frameRect(), SecurityOriginPaintPolicy::AnyOrigin, &accessibilityRegionContext);
#endif
}

void LocalFrameView::traverseForPaintInvalidation(NullGraphicsContext::PaintInvalidationReasons paintInvalidationReasons)
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

bool LocalFrameView::wasScrolledByUser() const
{
    return m_wasScrolledByUser;
}

void LocalFrameView::setWasScrolledByUser(bool wasScrolledByUser)
{
    LOG(Scrolling, "LocalFrameView::setWasScrolledByUser at %d", wasScrolledByUser);

    cancelScheduledScrolls();
    if (currentScrollType() == ScrollType::Programmatic)
        return;

    RefPtr document = m_frame->document();
    if (wasScrolledByUser && document)
        document->setGotoAnchorNeededAfterStylesheetsLoad(false);

    m_maintainScrollPositionAnchor = nullptr;
    if (m_wasScrolledByUser == wasScrolledByUser)
        return;
    m_wasScrolledByUser = wasScrolledByUser;
    adjustTiledBackingCoverage();
}

void LocalFrameView::willPaintContents(GraphicsContext& context, const IntRect&, PaintingState& paintingState, RegionContext* regionContext)
{
    Document* document = m_frame->document();

    if (!context.paintingDisabled())
        InspectorInstrumentation::willPaint(*renderView());

    paintingState.isTopLevelPainter = !sCurrentPaintTimeStamp;

    if (paintingState.isTopLevelPainter)
        sCurrentPaintTimeStamp = MonotonicTime::now();

    paintingState.paintBehavior = m_paintBehavior;
    
    if (auto* parentView = parentFrameView()) {
        constexpr OptionSet<PaintBehavior> flagsToCopy { PaintBehavior::FlattenCompositingLayers, PaintBehavior::Snapshotting, PaintBehavior::DefaultAsynchronousImageDecode, PaintBehavior::ForceSynchronousImageDecode, PaintBehavior::ExcludeReplacedContent };
        m_paintBehavior.add(parentView->paintBehavior() & flagsToCopy);
    }

    if (document->printing()) {
        m_paintBehavior.add(PaintBehavior::FlattenCompositingLayers);
        m_paintBehavior.add(PaintBehavior::Snapshotting);
    }

    if (is<AccessibilityRegionContext>(regionContext))
        m_paintBehavior.add(PaintBehavior::FlattenCompositingLayers);

    paintingState.isFlatteningPaintOfRootFrame = (m_paintBehavior & PaintBehavior::FlattenCompositingLayers) && !m_frame->ownerElement() && !context.detectingContentfulPaint();
    if (paintingState.isFlatteningPaintOfRootFrame)
        notifyWidgetsInAllFrames(WillPaintFlattened);

    ASSERT(!m_isPainting);
    m_isPainting = true;
}

void LocalFrameView::didPaintContents(GraphicsContext& context, const IntRect& dirtyRect, PaintingState& paintingState)
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

void LocalFrameView::paintContents(GraphicsContext& context, const IntRect& dirtyRect, SecurityOriginPaintPolicy securityOriginPaintPolicy, RegionContext* regionContext)
{
#ifndef NDEBUG
    bool fillWithWarningColor;
    if (m_frame->document()->printing())
        fillWithWarningColor = false; // Printing, don't fill with red (can't remember why).
    else if (m_frame->ownerElement())
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
        LOG_ERROR("called LocalFrameView::paint with nil renderer");
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
    willPaintContents(context, dirtyRect, paintingState, regionContext);

    // m_nodeToDraw is used to draw only one element (and its descendants)
    RenderObject* renderer = m_nodeToDraw ? m_nodeToDraw->renderer() : nullptr;
    RenderLayer* rootLayer = renderView->layer();

    RenderObject::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(rootLayer->renderer());

    rootLayer->paint(context, dirtyRect, LayoutSize(), m_paintBehavior, renderer, { }, securityOriginPaintPolicy == SecurityOriginPaintPolicy::AnyOrigin ? RenderLayer::SecurityOriginPaintPolicy::AnyOrigin : RenderLayer::SecurityOriginPaintPolicy::AccessibleOriginOnly, regionContext);
    if (auto* scrollableRootLayer = rootLayer->scrollableArea()) {
        if (scrollableRootLayer->containsDirtyOverlayScrollbars() && !regionContext)
            scrollableRootLayer->paintOverlayScrollbars(context, dirtyRect, m_paintBehavior, renderer);
    }

    didPaintContents(context, dirtyRect, paintingState);
}

void LocalFrameView::setPaintBehavior(OptionSet<PaintBehavior> behavior)
{
    m_paintBehavior = behavior;
}

OptionSet<PaintBehavior> LocalFrameView::paintBehavior() const
{
    return m_paintBehavior;
}

bool LocalFrameView::isPainting() const
{
    return m_isPainting;
}

// FIXME: change this to use the subtreePaint terminology.
void LocalFrameView::setNodeToDraw(Node* node)
{
    m_nodeToDraw = node;
}

void LocalFrameView::paintContentsForSnapshot(GraphicsContext& context, const IntRect& imageRect, SelectionInSnapshot shouldPaintSelection, CoordinateSpaceForSnapshot coordinateSpace)
{
    updateLayoutAndStyleIfNeededRecursive();

    // Cache paint behavior and set a new behavior appropriate for snapshots.
    auto oldBehavior = paintBehavior();
    setPaintBehavior(oldBehavior | PaintBehavior::FlattenCompositingLayers | PaintBehavior::Snapshotting);

    // If the snapshot should exclude selection, then we'll clear the current selection
    // in the render tree only. This will allow us to restore the selection from the DOM
    // after we paint the snapshot.
    if (shouldPaintSelection == ExcludeSelection) {
        for (Frame* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame);
            if (!localFrame)
                continue;
            if (auto* renderView = localFrame->contentRenderer())
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
        for (Frame* frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame);
            if (!localFrame)
                continue;
            localFrame->selection().updateAppearance();
        }
    }

    // Restore cached paint behavior.
    setPaintBehavior(oldBehavior);
}

void LocalFrameView::paintOverhangAreas(GraphicsContext& context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    if (context.paintingDisabled())
        return;

    if (m_frame->document()->printing())
        return;

    ScrollView::paintOverhangAreas(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect);
}

void LocalFrameView::updateLayoutAndStyleIfNeededRecursive(OptionSet<LayoutOptions> layoutOptions)
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

    // Style updates can trigger script, which can cause this LocalFrameView to be destroyed.
    Ref<LocalFrameView> protectedThis(*this);

    using DescendantsDeque = Deque<Ref<LocalFrameView>, 16>;
    auto nextRenderedDescendant = [this] (DescendantsDeque& descendantsDeque) -> RefPtr<LocalFrameView> {
        if (descendantsDeque.isEmpty())
            descendantsDeque.append(*this);
        else {
            // Append renderered children after processing the parent, in case the processing
            // affects the set of rendered children.
            auto previousView = descendantsDeque.takeFirst();
            for (auto* frame = previousView->m_frame->tree().firstRenderedChild(); frame; frame = frame->tree().nextRenderedSibling()) {
                auto* localFrame = dynamicDowncast<LocalFrame>(frame);
                if (!localFrame)
                    continue;
                if (auto* view = localFrame->view())
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
            if (view->m_frame->document()->updateLayout(layoutOptions | LayoutOptions::DoNotLayoutAncestorDocuments) == Document::UpdateLayoutResult::ChangesDone)
                didWork = true;
        }
        if (!didWork)
            break;
    }

#if ASSERT_ENABLED
    auto needsStyleRecalc = [&] {
        DescendantsDeque deque;
        while (auto view = nextRenderedDescendant(deque)) {
            RefPtr document = view->m_frame->document();
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

#include <span>

template<typename CharacterType>
static size_t nonWhitespaceLength(std::span<const CharacterType> characters)
{
    size_t result = characters.size();
    for (auto character : characters) {
        if (isASCIIWhitespace(character))
            --result;
    }
    return result;
}

void LocalFrameView::incrementVisuallyNonEmptyCharacterCountSlowCase(const String& inlineText)
{
    if (inlineText.is8Bit())
        m_visuallyNonEmptyCharacterCount += nonWhitespaceLength(inlineText.span8());
    else
        m_visuallyNonEmptyCharacterCount += nonWhitespaceLength(inlineText.span16());
    ++m_textRendererCountForVisuallyNonEmptyCharacters;
}

void LocalFrameView::updateHasReachedSignificantRenderedTextThreshold()
{
    if (m_hasReachedSignificantRenderedTextThreshold)
        return;

    auto* page = m_frame->page();
    if (!page || !page->requestedLayoutMilestones().contains(LayoutMilestone::DidRenderSignificantAmountOfText))
        return;

    auto* document = m_frame->document();
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

bool LocalFrameView::qualifiesAsSignificantRenderedText() const
{
    ASSERT(!m_renderedSignificantAmountOfText);
    auto* document = m_frame->document();
    if (!document || document->styleScope().hasPendingSheetsBeforeBody())
        return false;
    return m_hasReachedSignificantRenderedTextThreshold;
}

void LocalFrameView::checkAndDispatchDidReachVisuallyNonEmptyState()
{
    auto qualifiesAsVisuallyNonEmpty = [&] {
        // No content yet.
        auto& document = *m_frame->document();
        auto* documentElement = document.documentElement();
        if (!documentElement || !documentElement->renderer())
            return false;

        if (document.hasVisuallyNonEmptyCustomContent())
            return true;

        // FIXME: We should also ignore renderers with non-final style.
        if (document.styleScope().hasPendingSheetsBeforeBody())
            return false;

        auto finishedParsingMainDocument = m_frame->loader().stateMachine().committedFirstRealDocumentLoad()
            && (document.readyState() == Document::ReadyState::Interactive || document.readyState() == Document::ReadyState::Complete);
        // Ensure that we always fire visually non-empty milestone eventually.
        if (finishedParsingMainDocument && m_frame->loader().isComplete())
            return true;

        auto isVisible = [](const Element* element) {
            if (!element || !element->renderer())
                return false;
            if (!element->renderer()->opacity())
                return false;
            return element->renderer()->style().usedVisibility() == Visibility::Visible;
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
            auto* documentLoader = m_frame->loader().documentLoader();
            if (!documentLoader)
                return false;

            auto& resourceLoader = documentLoader->cachedResourceLoader();
            if (!resourceLoader.requestCount())
                return false;

            auto& resources = resourceLoader.allCachedResources();
            for (auto& resource : resources) {
                if (resource.value->isLoaded())
                    continue;
                // ResourceLoadPriority::VeryLow is used for resources that are not needed to render.
                if (resource.value->loadPriority() == ResourceLoadPriority::VeryLow)
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
    if (m_frame->isRootFrame())
        m_frame->loader().didReachVisuallyNonEmptyState();
}

bool LocalFrameView::hasContentfulDescendants() const
{
    return m_visuallyNonEmptyCharacterCount || m_visuallyNonEmptyPixelCount;
}

bool LocalFrameView::isViewForDocumentInFrame() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    return &renderView->frameView() == this;
}

void LocalFrameView::enableFixedWidthAutoSizeMode(bool enable, const IntSize& viewSize)
{
    // FIXME: Figure out if this flavor of the autosizing should run only on the mainframe.
    enableAutoSizeMode(enable, viewSize, AutoSizeMode::FixedWidth);
}

void LocalFrameView::enableSizeToContentAutoSizeMode(bool enable, const IntSize& viewSize)
{
    ASSERT(m_frame->isMainFrame());
    enableAutoSizeMode(enable, viewSize, AutoSizeMode::SizeToContent);
}

void LocalFrameView::enableAutoSizeMode(bool enable, const IntSize& viewSize, AutoSizeMode mode)
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
        overrideWidthForCSSDefaultViewportUnits(m_autoSizeConstraint.width());
        overrideWidthForCSSSmallViewportUnits(m_autoSizeConstraint.width());
        overrideWidthForCSSLargeViewportUnits(m_autoSizeConstraint.width());
        return;
    }

    clearSizeOverrideForCSSDefaultViewportUnits();
    clearSizeOverrideForCSSSmallViewportUnits();
    clearSizeOverrideForCSSLargeViewportUnits();
    // Since autosize mode forces the scrollbar mode, change them to being auto.
    setVerticalScrollbarLock(false);
    setHorizontalScrollbarLock(false);
    setScrollbarModes(ScrollbarMode::Auto, ScrollbarMode::Auto);
}

void LocalFrameView::forceLayout(bool allowSubtreeLayout)
{
    if (!allowSubtreeLayout && layoutContext().subtreeLayoutRoot())
        layoutContext().convertSubtreeLayoutToFullLayout();
    layoutContext().layout();
}

void LocalFrameView::forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor, AdjustViewSizeOrNot shouldAdjustViewSize)
{
    if (!renderView())
        return;

    Ref<LocalFrameView> protectedThis(*this);
    auto& renderView = *this->renderView();

    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    bool isHorizontalWritingMode = renderView.writingMode().isHorizontal();
    float pageLogicalWidth = isHorizontalWritingMode ? pageSize.width() : pageSize.height();
    float pageLogicalHeight = isHorizontalWritingMode ? pageSize.height() : pageSize.width();

    renderView.setPageLogicalSize({ floor(pageLogicalWidth), floor(pageLogicalHeight) });
    renderView.setNeedsLayoutAndPrefWidthsRecalc();
    forceLayout();
    if (hasOneRef())
        return;

    // If we don't fit in the given page width, we'll lay out again. If we don't fit in the
    // page width when shrunk, we will lay out at maximum shrink and clip extra content.
    // FIXME: We are assuming a shrink-to-fit printing implementation. A cropping
    // implementation should not do this!
    const LayoutRect& documentRect = renderView.documentRect();
    LayoutUnit docLogicalWidth = isHorizontalWritingMode ? documentRect.width() : documentRect.height();
    if (docLogicalWidth > pageLogicalWidth) {
        int expectedPageWidth = std::min<float>(documentRect.width(), pageSize.width() * maximumShrinkFactor);
        int expectedPageHeight = std::min<float>(documentRect.height(), pageSize.height() * maximumShrinkFactor);
        FloatSize maxPageSize = m_frame->resizePageRectsKeepingRatio(FloatSize(originalPageSize.width(), originalPageSize.height()), FloatSize(expectedPageWidth, expectedPageHeight));
        pageLogicalWidth = isHorizontalWritingMode ? maxPageSize.width() : maxPageSize.height();
        pageLogicalHeight = isHorizontalWritingMode ? maxPageSize.height() : maxPageSize.width();

        renderView.setPageLogicalSize({ floor(pageLogicalWidth), floor(pageLogicalHeight) });
        renderView.setNeedsLayoutAndPrefWidthsRecalc();
        forceLayout();
        if (hasOneRef())
            return;

        const LayoutRect& updatedDocumentRect = renderView.documentRect();
        LayoutUnit docLogicalHeight = isHorizontalWritingMode ? updatedDocumentRect.height() : updatedDocumentRect.width();
        LayoutUnit docLogicalTop = isHorizontalWritingMode ? updatedDocumentRect.y() : updatedDocumentRect.x();
        LayoutUnit docLogicalRight = isHorizontalWritingMode ? updatedDocumentRect.maxX() : updatedDocumentRect.maxY();
        LayoutUnit clippedLogicalLeft;
        if (!renderView.writingMode().isAnyLeftToRight())
            clippedLogicalLeft = docLogicalRight - pageLogicalWidth;
        LayoutRect overflow { clippedLogicalLeft, docLogicalTop, LayoutUnit(pageLogicalWidth), docLogicalHeight };

        if (!isHorizontalWritingMode)
            overflow = overflow.transposedRect();
        renderView.clearLayoutOverflow();
        renderView.addLayoutOverflow(overflow); // This is how we clip in case we overflow again.
    }

    if (shouldAdjustViewSize)
        adjustViewSize();
}

void LocalFrameView::adjustPageHeightDeprecated(float *newBottom, float oldTop, float oldBottom, float /*bottomLimit*/)
{
    RenderView* renderView = this->renderView();
    if (!renderView) {
        *newBottom = oldBottom;
        return;

    }
    // Use a context with painting disabled.
    NullGraphicsContext context;
    renderView->setTruncatedAt(static_cast<int>(floorf(oldBottom)));
    IntRect dirtyRect(0, static_cast<int>(floorf(oldTop)), renderView->layoutOverflowRect().maxX(), static_cast<int>(ceilf(oldBottom - oldTop)));
    renderView->setPrintRect(dirtyRect);
    renderView->layer()->paint(context, dirtyRect);
    *newBottom = renderView->bestTruncatedAt();
    if (!*newBottom)
        *newBottom = oldBottom;
    renderView->setPrintRect(IntRect());
}

float LocalFrameView::documentToAbsoluteScaleFactor(std::optional<float> usedZoom) const
{
    // If usedZoom is passed, it already factors in pageZoomFactor().
    return usedZoom.value_or(m_frame->pageZoomFactor()) * m_frame->frameScaleFactor();
}

float LocalFrameView::absoluteToDocumentScaleFactor(std::optional<float> usedZoom) const
{
    // If usedZoom is passed, it already factors in pageZoomFactor().
    return 1 / documentToAbsoluteScaleFactor(usedZoom);
}

FloatRect LocalFrameView::absoluteToDocumentRect(FloatRect rect, std::optional<float> usedZoom) const
{
    rect.scale(absoluteToDocumentScaleFactor(usedZoom));
    return rect;
}

FloatPoint LocalFrameView::absoluteToDocumentPoint(FloatPoint p, std::optional<float> usedZoom) const
{
    return p.scaled(absoluteToDocumentScaleFactor(usedZoom));
}

FloatRect LocalFrameView::absoluteToClientRect(FloatRect rect, std::optional<float> usedZoom) const
{
    return documentToClientRect(absoluteToDocumentRect(rect, usedZoom));
}

FloatSize LocalFrameView::documentToClientOffset() const
{
    FloatSize clientOrigin = -toFloatSize(visibleContentRect().location());

    // Layout and visual viewports are affected by page zoom, so we need to factor that out.
    return clientOrigin.scaled(1 / (m_frame->pageZoomFactor() * m_frame->frameScaleFactor()));
}

FloatRect LocalFrameView::documentToClientRect(FloatRect rect) const
{
    rect.move(documentToClientOffset());
    return rect;
}

FloatPoint LocalFrameView::documentToClientPoint(FloatPoint p) const
{
    p.move(documentToClientOffset());
    return p;
}

FloatRect LocalFrameView::clientToDocumentRect(FloatRect rect) const
{
    rect.move(-documentToClientOffset());
    return rect;
}

FloatPoint LocalFrameView::clientToDocumentPoint(FloatPoint point) const
{
    point.move(-documentToClientOffset());
    return point;
}

FloatPoint LocalFrameView::absoluteToLayoutViewportPoint(FloatPoint p) const
{
    ASSERT(m_frame->settings().visualViewportEnabled());
    p.scale(1 / m_frame->frameScaleFactor());
    p.moveBy(-layoutViewportRect().location());
    return p;
}

FloatPoint LocalFrameView::layoutViewportToAbsolutePoint(FloatPoint p) const
{
    ASSERT(m_frame->settings().visualViewportEnabled());
    p.moveBy(layoutViewportRect().location());
    return p.scaled(m_frame->frameScaleFactor());
}

FloatRect LocalFrameView::layoutViewportToAbsoluteRect(FloatRect rect) const
{
    ASSERT(m_frame->settings().visualViewportEnabled());
    rect.moveBy(layoutViewportRect().location());
    rect.scale(m_frame->frameScaleFactor());
    return rect;
}

FloatRect LocalFrameView::absoluteToLayoutViewportRect(FloatRect rect) const
{
    ASSERT(m_frame->settings().visualViewportEnabled());
    rect.scale(1 / m_frame->frameScaleFactor());
    rect.moveBy(-layoutViewportRect().location());
    return rect;
}

FloatRect LocalFrameView::clientToLayoutViewportRect(FloatRect rect) const
{
    ASSERT(m_frame->settings().visualViewportEnabled());
    rect.scale(m_frame->pageZoomFactor());
    return rect;
}

FloatPoint LocalFrameView::clientToLayoutViewportPoint(FloatPoint p) const
{
    ASSERT(m_frame->settings().visualViewportEnabled());
    return p.scaled(m_frame->pageZoomFactor());
}

void LocalFrameView::setTracksRepaints(bool trackRepaints)
{
    if (trackRepaints == m_isTrackingRepaints)
        return;

    // Force layout to flush out any pending repaints.
    if (trackRepaints) {
        if (RefPtr document = m_frame->document())
            document->updateLayout(LayoutOptions::UpdateCompositingLayers);
    }

    for (Frame* frame = &m_frame->tree().top(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (auto* renderView = localFrame->contentRenderer())
            renderView->compositor().setTracksRepaints(trackRepaints);
    }

    resetTrackedRepaints();
    m_isTrackingRepaints = trackRepaints;
}

void LocalFrameView::resetTrackedRepaints()
{
    m_trackedRepaintRects.clear();
    if (RenderView* renderView = this->renderView())
        renderView->compositor().resetTrackedRepaintRects();
}

String LocalFrameView::trackedRepaintRectsAsText() const
{
    auto& frame = this->m_frame.get();
    Ref protectedFrame { frame };

    if (RefPtr document = frame.document())
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

void LocalFrameView::startTrackingLayoutUpdates()
{
    m_layoutUpdateCount = 0;
}

unsigned LocalFrameView::layoutUpdateCount()
{
    return m_layoutUpdateCount;
}

void LocalFrameView::startTrackingRenderLayerPositionUpdates()
{
    m_renderLayerPositionUpdateCount = 0;
}

unsigned LocalFrameView::renderLayerPositionUpdateCount()
{
    return m_renderLayerPositionUpdateCount;
}

void LocalFrameView::addScrollableAreaForAnimatedScroll(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreasForAnimatedScroll)
        m_scrollableAreasForAnimatedScroll = makeUnique<ScrollableAreaSet>();
    
    m_scrollableAreasForAnimatedScroll->add(*scrollableArea);
}

void LocalFrameView::removeScrollableAreaForAnimatedScroll(ScrollableArea* scrollableArea)
{
    if (m_scrollableAreasForAnimatedScroll)
        m_scrollableAreasForAnimatedScroll->remove(*scrollableArea);
}

bool LocalFrameView::addScrollableArea(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreas)
        m_scrollableAreas = makeUnique<ScrollableAreaSet>();
    
    if (m_scrollableAreas->add(*scrollableArea).isNewEntry) {
        scrollableAreaSetChanged();
        return true;
    }

    return false;
}

bool LocalFrameView::removeScrollableArea(ScrollableArea* scrollableArea)
{
    if (m_scrollableAreas && m_scrollableAreas->remove(*scrollableArea)) {
        scrollableAreaSetChanged();
        return true;
    }
    return false;
}

bool LocalFrameView::containsScrollableArea(ScrollableArea* scrollableArea) const
{
    return m_scrollableAreas && m_scrollableAreas->contains(*scrollableArea);
}

void LocalFrameView::scrollableAreaSetChanged()
{
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewEventTrackingRegionsChanged(*this);
}

void LocalFrameView::scheduleScrollEvent()
{
    m_frame->eventHandler().scheduleScrollEvent();
    m_frame->eventHandler().dispatchFakeMouseMoveEventSoon();
}

void LocalFrameView::addChild(Widget& widget)
{
    if (auto* childFrameView = dynamicDowncast<LocalFrameView>(widget)) {
        if (childFrameView->isScrollable())
            addScrollableArea(childFrameView);
    }

    ScrollView::addChild(widget);
}

void LocalFrameView::removeChild(Widget& widget)
{
    if (auto* childFrameView = dynamicDowncast<LocalFrameView>(widget))
        removeScrollableArea(childFrameView);

    ScrollView::removeChild(widget);
}

bool LocalFrameView::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState> gestureState)
{
    // Note that to allow for rubber-band over-scroll behavior, even non-scrollable views
    // should handle wheel events.
#if !HAVE(RUBBER_BANDING)
    if (!isScrollable())
        return false;
#endif

    if (delegatesScrollingToNativeView()) {
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
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator()) {
        if (scrollingCoordinator->coordinatesScrollingForFrameView(*this)) {
            auto result = scrollingCoordinator->handleWheelEventForScrolling(wheelEvent, *scrollingNodeID(), gestureState);
            if (!result.needsMainThreadProcessing())
                return result.wasHandled;
        }
    }
#endif

    return ScrollableArea::handleWheelEventForScrolling(wheelEvent, gestureState);
}

bool LocalFrameView::isVerticalDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return true;

    return renderView->writingMode().isHorizontal();
}

bool LocalFrameView::isFlippedDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    return renderView->writingMode().isBlockFlipped();
}

void LocalFrameView::notifyWidgetsInAllFrames(WidgetNotification notification)
{
    for (RefPtr<Frame> frame = m_frame.ptr(); frame; frame = frame->tree().traverseNext(m_frame.ptr())) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (RefPtr view = localFrame->view())
            view->notifyWidgets(notification);
    }
}
    
AXObjectCache* LocalFrameView::axObjectCache() const
{
    AXObjectCache* cache = nullptr;
    if (m_frame->document())
        cache = m_frame->document()->existingAXObjectCache();

    // FIXME: We should generally always be using the main-frame cache rather than
    // using it as a fallback as we do here.
    if (!cache && !m_frame->isMainFrame()) {
        auto localMainFrame = dynamicDowncast<LocalFrame>(m_frame->mainFrame());
        if (localMainFrame) {
            if (auto* mainFrameDocument = localMainFrame->document())
                cache = mainFrameDocument->existingAXObjectCache();
        }
    }
    return cache;
}

#if PLATFORM(IOS_FAMILY)
bool LocalFrameView::useCustomFixedPositionLayoutRect() const
{
    return !m_frame->settings().visualViewportEnabled() && m_useCustomFixedPositionLayoutRect;
}

void LocalFrameView::setCustomFixedPositionLayoutRect(const IntRect& rect)
{
    if (m_useCustomFixedPositionLayoutRect && m_customFixedPositionLayoutRect == rect)
        return;
    m_useCustomFixedPositionLayoutRect = true;
    m_customFixedPositionLayoutRect = rect;
    updateContentsSize();
}

bool LocalFrameView::updateFixedPositionLayoutRect()
{
    if (!m_useCustomFixedPositionLayoutRect)
        return false;

    IntRect newRect;
    Page* page = m_frame->page();
    if (!page || !page->chrome().client().fetchCustomFixedPositionLayoutRect(newRect))
        return false;

    if (newRect != m_customFixedPositionLayoutRect) {
        m_customFixedPositionLayoutRect = newRect;
        setViewportConstrainedObjectsNeedLayout();
        return true;
    }
    return false;
}

void LocalFrameView::setCustomSizeForResizeEvent(IntSize customSize)
{
    m_customSizeForResizeEvent = customSize;
    scheduleResizeEventIfNeeded();
}

void LocalFrameView::setScrollVelocity(const VelocityData& velocityData)
{
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setVelocity(velocityData);
}
#endif // PLATFORM(IOS_FAMILY)

void LocalFrameView::setScrollingPerformanceTestingEnabled(bool scrollingPerformanceTestingEnabled)
{
    if (scrollingPerformanceTestingEnabled) {
        auto* page = m_frame->page();
        if (page && page->performanceLoggingClient())
            page->performanceLoggingClient()->logScrollingEvent(PerformanceLoggingClient::ScrollingEvent::LoggingEnabled, MonotonicTime::now(), 0);
    }

    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setScrollingPerformanceTestingEnabled(scrollingPerformanceTestingEnabled);
}

void LocalFrameView::createScrollbarsController()
{
    auto* page = m_frame->page();
    if (!page) {
        ScrollView::createScrollbarsController();
        return;
    }

    page->chrome().client().ensureScrollbarsController(*page, *this);
}

void LocalFrameView::didAddScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    auto* page = m_frame->page();

    ScrollableArea::didAddScrollbar(scrollbar, orientation);

    if (page && page->isMonitoringWheelEvents())
        scrollAnimator().setWheelEventTestMonitor(page->wheelEventTestMonitor());
    if (AXObjectCache* cache = axObjectCache())
        cache->onScrollbarUpdate(*this);
}

void LocalFrameView::willRemoveScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    ScrollableArea::willRemoveScrollbar(scrollbar, orientation);
    if (AXObjectCache* cache = axObjectCache()) {
        cache->remove(scrollbar);
        cache->onScrollbarUpdate(*this);
    }
}

void LocalFrameView::scrollbarFrameRectChanged(const Scrollbar& scrollbar) const
{
    if (auto* cache = axObjectCache())
        cache->onScrollbarFrameRectChange(scrollbar);
}

void LocalFrameView::addPaintPendingMilestones(OptionSet<LayoutMilestone> milestones)
{
    m_milestonesPendingPaint.add(milestones);
}

void LocalFrameView::fireLayoutRelatedMilestonesIfNeeded()
{
    OptionSet<LayoutMilestone> requestedMilestones;
    OptionSet<LayoutMilestone> milestonesAchieved;
    Page* page = m_frame->page();
    if (page)
        requestedMilestones = page->requestedLayoutMilestones();

    if (m_firstLayoutCallbackPending) {
        m_firstLayoutCallbackPending = false;
        m_frame->loader().didFirstLayout();
        if (requestedMilestones & LayoutMilestone::DidFirstLayout)
            milestonesAchieved.add(LayoutMilestone::DidFirstLayout);
        if (m_frame->isMainFrame())
            page->startCountingRelevantRepaintedObjects();
    }

    if (m_firstVisuallyNonEmptyLayoutMilestoneIsPending) {
        checkAndDispatchDidReachVisuallyNonEmptyState();
        if (m_contentQualifiesAsVisuallyNonEmpty) {
            m_firstVisuallyNonEmptyLayoutMilestoneIsPending = false;

            addPaintPendingMilestones(LayoutMilestone::DidFirstMeaningfulPaint);
            if (requestedMilestones & LayoutMilestone::DidFirstVisuallyNonEmptyLayout)
                milestonesAchieved.add(LayoutMilestone::DidFirstVisuallyNonEmptyLayout);
        }
    }

    if (!m_renderedSignificantAmountOfText && qualifiesAsSignificantRenderedText()) {
        m_renderedSignificantAmountOfText = true;
        if (requestedMilestones & LayoutMilestone::DidRenderSignificantAmountOfText)
            milestonesAchieved.add(LayoutMilestone::DidRenderSignificantAmountOfText);
    }

    if (milestonesAchieved && m_frame->isMainFrame()) {
        if (milestonesAchieved.contains(LayoutMilestone::DidFirstVisuallyNonEmptyLayout))
            FRAMEVIEW_RELEASE_LOG(Layout, "fireLayoutRelatedMilestonesIfNeeded: Firing first visually non-empty layout milestone on the main frame");
        m_frame->loader().didReachLayoutMilestone(milestonesAchieved);
    }
}

void LocalFrameView::firePaintRelatedMilestonesIfNeeded()
{
    Page* page = m_frame->page();
    if (!page)
        return;

    OptionSet<LayoutMilestone> milestonesAchieved;

    if (m_milestonesPendingPaint & LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering) {
        if (page->requestedLayoutMilestones() & LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering)
            milestonesAchieved.add(LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering);
    }

    if (m_milestonesPendingPaint & LayoutMilestone::DidFirstMeaningfulPaint) {
        if (page->requestedLayoutMilestones() & LayoutMilestone::DidFirstMeaningfulPaint)
            milestonesAchieved.add(LayoutMilestone::DidFirstMeaningfulPaint);
    }

    m_milestonesPendingPaint = { };

    auto* localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (milestonesAchieved && localMainFrame)
        localMainFrame->loader().didReachLayoutMilestone(milestonesAchieved);
}

void LocalFrameView::setVisualUpdatesAllowedByClient(bool visualUpdatesAllowed)
{
    if (m_visualUpdatesAllowedByClient == visualUpdatesAllowed)
        return;

    m_visualUpdatesAllowedByClient = visualUpdatesAllowed;

    m_frame->document()->setVisualUpdatesAllowedByClient(visualUpdatesAllowed);
}
    
void LocalFrameView::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    m_scrollPinningBehavior = pinning;
    
    if (RefPtr scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->setScrollPinningBehavior(pinning);
    
    updateScrollbars(scrollPosition());
}

ScrollBehaviorForFixedElements LocalFrameView::scrollBehaviorForFixedElements() const
{
    return m_frame->settings().backgroundShouldExtendBeyondPage() ? ScrollBehaviorForFixedElements::StickToViewportBounds : ScrollBehaviorForFixedElements::StickToDocumentBounds;
}

LocalFrame& LocalFrameView::frame() const
{
    return m_frame;
}

Ref<LocalFrame> LocalFrameView::protectedFrame() const
{
    return m_frame;
}

RenderView* LocalFrameView::renderView() const
{
    return m_frame->contentRenderer();
}

CheckedPtr<RenderView> LocalFrameView::checkedRenderView() const
{
    return renderView();
}

int LocalFrameView::mapFromLayoutToCSSUnits(LayoutUnit value) const
{
    Ref frame = m_frame;
    return value / (frame->pageZoomFactor() * frame->frameScaleFactor());
}

LayoutUnit LocalFrameView::mapFromCSSToLayoutUnits(int value) const
{
    Ref frame = m_frame;
    return LayoutUnit(value * frame->pageZoomFactor() * frame->frameScaleFactor());
}

void LocalFrameView::didAddWidgetToRenderTree(Widget& widget)
{
    ASSERT(!m_widgetsInRenderTree.contains(widget));
    m_widgetsInRenderTree.add(widget);
}

void LocalFrameView::willRemoveWidgetFromRenderTree(Widget& widget)
{
    ASSERT(m_widgetsInRenderTree.contains(widget));
    m_widgetsInRenderTree.remove(widget);
}

static Vector<Ref<Widget>> collectAndProtectWidgets(const HashSet<SingleThreadWeakRef<Widget>>& set)
{
    return WTF::map(set, [](auto& widget) -> Ref<Widget> {
        return widget.get();
    });
}

void LocalFrameView::updateWidgetPositions()
{
    m_updateWidgetPositionsTimer.stop();
    // updateWidgetPosition() can possibly cause layout to be re-entered (via plug-ins running
    // scripts in response to NPP_SetWindow, for example), so we need to keep the Widgets
    // alive during enumeration.
    for (auto& widget : collectAndProtectWidgets(m_widgetsInRenderTree)) {
        if (auto* renderer = RenderWidget::find(widget)) {
            auto ignoreWidgetState = renderer->updateWidgetPosition();
            UNUSED_PARAM(ignoreWidgetState);
        }
    }
}

void LocalFrameView::scheduleUpdateWidgetPositions()
{
    if (!m_updateWidgetPositionsTimer.isActive())
        m_updateWidgetPositionsTimer.startOneShot(0_s);
}

void LocalFrameView::updateWidgetPositionsTimerFired()
{
    updateWidgetPositions();
}

void LocalFrameView::notifyWidgets(WidgetNotification notification)
{
    for (auto& widget : collectAndProtectWidgets(m_widgetsInRenderTree))
        widget->notifyWidget(notification);
}

void LocalFrameView::setViewExposedRect(std::optional<FloatRect> viewExposedRect)
{
    if (m_viewExposedRect == viewExposedRect)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "LocalFrameView " << this << " setViewExposedRect " << (viewExposedRect ? viewExposedRect.value() : FloatRect()));

    bool hasRectExistenceChanged = !m_viewExposedRect == !viewExposedRect;
    m_viewExposedRect = viewExposedRect;

    // FIXME: We should support clipping to the exposed rect for subframes as well.
    if (!m_frame->isMainFrame())
        return;

    if (TiledBacking* tiledBacking = this->tiledBacking()) {
        if (hasRectExistenceChanged)
            updateTiledBackingAdaptiveSizing();
        adjustTiledBackingCoverage();
        tiledBacking->setTiledScrollingIndicatorPosition(m_viewExposedRect ? m_viewExposedRect.value().location() : FloatPoint());
    }

    if (RefPtr page = m_frame->page()) {
        page->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
        page->pageOverlayController().didChangeViewExposedRect();
    }
}

void LocalFrameView::clearSizeOverrideForCSSDefaultViewportUnits()
{
    if (!m_defaultViewportSizeOverride)
        return;

    m_defaultViewportSizeOverride = std::nullopt;
    if (auto* document = m_frame->document())
        document->styleScope().didChangeStyleSheetEnvironment();
}

void LocalFrameView::setSizeForCSSDefaultViewportUnits(FloatSize size)
{
    setOverrideSizeForCSSDefaultViewportUnits({ size.width(), size.height() });
}

void LocalFrameView::overrideWidthForCSSDefaultViewportUnits(float width)
{
    setOverrideSizeForCSSDefaultViewportUnits({ width, m_defaultViewportSizeOverride ? m_defaultViewportSizeOverride->height : std::nullopt });
}

void LocalFrameView::resetOverriddenWidthForCSSDefaultViewportUnits()
{
    setOverrideSizeForCSSDefaultViewportUnits({ { }, m_defaultViewportSizeOverride ? m_defaultViewportSizeOverride->height : std::nullopt });
}

void LocalFrameView::setOverrideSizeForCSSDefaultViewportUnits(OverrideViewportSize size)
{
    if (m_defaultViewportSizeOverride == size)
        return;

    m_defaultViewportSizeOverride = size;

    if (auto* document = m_frame->document())
        document->styleScope().didChangeStyleSheetEnvironment();
}

FloatSize LocalFrameView::sizeForCSSDefaultViewportUnits() const
{
    return calculateSizeForCSSViewportUnitsOverride(m_defaultViewportSizeOverride);
}

void LocalFrameView::clearSizeOverrideForCSSSmallViewportUnits()
{
    if (!m_smallViewportSizeOverride)
        return;

    m_smallViewportSizeOverride = std::nullopt;
    if (auto* document = m_frame->document())
        document->updateViewportUnitsOnResize();
}

void LocalFrameView::setSizeForCSSSmallViewportUnits(FloatSize size)
{
    setOverrideSizeForCSSSmallViewportUnits({ size.width(), size.height() });
}

void LocalFrameView::overrideWidthForCSSSmallViewportUnits(float width)
{
    setOverrideSizeForCSSSmallViewportUnits({ width, m_smallViewportSizeOverride ? m_smallViewportSizeOverride->height : std::nullopt });
}

void LocalFrameView::resetOverriddenWidthForCSSSmallViewportUnits()
{
    setOverrideSizeForCSSSmallViewportUnits({ { }, m_smallViewportSizeOverride ? m_smallViewportSizeOverride->height : std::nullopt });
}

void LocalFrameView::setOverrideSizeForCSSSmallViewportUnits(OverrideViewportSize size)
{
    if (m_smallViewportSizeOverride && *m_smallViewportSizeOverride == size)
        return;

    m_smallViewportSizeOverride = size;

    if (auto* document = m_frame->document())
        document->updateViewportUnitsOnResize();
}

FloatSize LocalFrameView::sizeForCSSSmallViewportUnits() const
{
    return calculateSizeForCSSViewportUnitsOverride(m_smallViewportSizeOverride);
}

void LocalFrameView::clearSizeOverrideForCSSLargeViewportUnits()
{
    if (!m_largeViewportSizeOverride)
        return;

    m_largeViewportSizeOverride = std::nullopt;
    if (auto* document = m_frame->document())
        document->updateViewportUnitsOnResize();
}

void LocalFrameView::setSizeForCSSLargeViewportUnits(FloatSize size)
{
    setOverrideSizeForCSSLargeViewportUnits({ size.width(), size.height() });
}

void LocalFrameView::overrideWidthForCSSLargeViewportUnits(float width)
{
    setOverrideSizeForCSSLargeViewportUnits({ width, m_largeViewportSizeOverride ? m_largeViewportSizeOverride->height : std::nullopt });
}

void LocalFrameView::resetOverriddenWidthForCSSLargeViewportUnits()
{
    setOverrideSizeForCSSLargeViewportUnits({ { }, m_largeViewportSizeOverride ? m_largeViewportSizeOverride->height : std::nullopt });
}

void LocalFrameView::setOverrideSizeForCSSLargeViewportUnits(OverrideViewportSize size)
{
    if (m_largeViewportSizeOverride && *m_largeViewportSizeOverride == size)
        return;

    m_largeViewportSizeOverride = size;

    if (auto* document = m_frame->document())
        document->updateViewportUnitsOnResize();
}

FloatSize LocalFrameView::sizeForCSSLargeViewportUnits() const
{
    return calculateSizeForCSSViewportUnitsOverride(m_largeViewportSizeOverride);
}

FloatSize LocalFrameView::calculateSizeForCSSViewportUnitsOverride(std::optional<OverrideViewportSize> override) const
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

FloatSize LocalFrameView::sizeForCSSDynamicViewportUnits() const
{
    if (m_layoutViewportOverrideRect)
        return m_layoutViewportOverrideRect->size();

    if (useFixedLayout())
        return fixedLayoutSize();

    if (m_frame->settings().visualViewportEnabled())
        return unobscuredContentRectIncludingScrollbars().size();

    return viewportConstrainedVisibleContentRect().size();
}

bool LocalFrameView::shouldPlaceVerticalScrollbarOnLeft() const
{
    return renderView() && renderView()->shouldPlaceVerticalScrollbarOnLeft();
}

bool LocalFrameView::isHorizontalWritingMode() const
{
    return renderView() && renderView()->writingMode().isHorizontal();
}

TextStream& operator<<(TextStream& ts, const LocalFrameView& view)
{
    ts << view.debugDescription();
    return ts;
}

void LocalFrameView::didFinishProhibitingScrollingWhenChangingContentSize()
{
    if (auto* document = m_frame->document()) {
        if (!document->needsStyleRecalc() && !needsLayout() && !layoutContext().isInLayout())
            updateScrollbars(scrollPosition());
        else
            m_needsDeferredScrollbarsUpdate = true;
    }
}

float LocalFrameView::pageScaleFactor() const
{
    return m_frame->frameScaleFactor();
}

void LocalFrameView::didStartScrollAnimation()
{
    if (RefPtr page = m_frame->page())
        page->scheduleRenderingUpdate({ RenderingUpdateStep::Scroll });
}

void LocalFrameView::updateScrollbarSteps()
{
    auto* documentElement = m_frame->document() ? m_frame->document()->documentElement() : nullptr;
    auto* renderer = documentElement ? documentElement->renderBox() : nullptr;
    if (!renderer) {
        ScrollView::updateScrollbarSteps();
        return;
    }

    LayoutRect paddedViewRect(LayoutPoint(), visibleSize());
    paddedViewRect.contract(renderer->scrollPaddingForViewportRect(paddedViewRect));

    if (horizontalScrollbar()) {
        int pageStep = Scrollbar::pageStep(paddedViewRect.width());
        horizontalScrollbar()->setSteps(Scrollbar::pixelsPerLineStep(paddedViewRect.width()), pageStep);

    }
    if (verticalScrollbar()) {
        int pageStep = Scrollbar::pageStep(paddedViewRect.height());
        verticalScrollbar()->setSteps(Scrollbar::pixelsPerLineStep(paddedViewRect.height()), pageStep);
    }
}

OverscrollBehavior LocalFrameView::horizontalOverscrollBehavior() const
{
    auto* document = m_frame->document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject && renderView())
        return scrollingObject->style().overscrollBehaviorX();
    return OverscrollBehavior::Auto;
}

OverscrollBehavior LocalFrameView::verticalOverscrollBehavior()  const
{
    auto* document = m_frame->document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject && renderView())
        return scrollingObject->style().overscrollBehaviorY();
    return OverscrollBehavior::Auto;
}

Color LocalFrameView::scrollbarThumbColorStyle() const
{
    auto* document = m_frame->document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject)
        return scrollingObject->style().usedScrollbarThumbColor();
    return { };
}

Color LocalFrameView::scrollbarTrackColorStyle() const
{
    auto* document = m_frame->document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject)
        return scrollingObject->style().usedScrollbarTrackColor();
    return { };
}

ScrollbarGutter LocalFrameView::scrollbarGutterStyle()  const
{
    auto* document = m_frame->document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject)
        return scrollingObject->style().scrollbarGutter();
    return { };
}

ScrollbarWidth LocalFrameView::scrollbarWidthStyle()  const
{
    auto* document = m_frame->document();
    auto scrollingObject = document && document->documentElement() ? document->documentElement()->renderer() : nullptr;
    if (scrollingObject && renderView())
        return scrollingObject->style().scrollbarWidth();
    return ScrollbarWidth::Auto;
}

bool LocalFrameView::isVisibleToHitTesting() const
{
    bool isVisibleToHitTest = true;
    if (HTMLFrameOwnerElement* owner = m_frame->ownerElement())
        isVisibleToHitTest = owner->renderer() && owner->renderer()->visibleToHitTesting();
    return isVisibleToHitTest;
}

LayoutRect LocalFrameView::getPossiblyFixedRectToExpose(const LayoutRect& visibleRect, const LayoutRect& exposeRect, bool insideFixed, const ScrollAlignment& alignX, const ScrollAlignment& alignY) const
{
    if (!insideFixed)
        return getRectToExposeForScrollIntoView(visibleRect, exposeRect, alignX, alignY);

    // If the element is inside position:fixed and we're not scaled, no amount of scrolling is going to move things around.
    if (frameScaleFactor() == 1)
        return visibleRect;

    // FIXME: Shouldn't this return visibleRect as well?
    if (!m_frame->settings().visualViewportEnabled())
        return getRectToExposeForScrollIntoView(visibleRect, exposeRect, alignX, alignY);

    // exposeRect is in absolute coords, affected by page scale. Unscale it.
    auto unscaledExposeRect = exposeRect;
    unscaledExposeRect.scale(1 / frameScaleFactor());
    unscaledExposeRect.move(0, -headerHeight());

    // These are both in unscaled coordinates.
    auto layoutViewport = layoutViewportRect();
    auto visualViewport = visualViewportRect();

    // The rect to expose may be partially offscreen, which we can't do anything about with position:fixed.
    unscaledExposeRect.intersect(layoutViewport);

    // Make sure it's not larger than the visual viewport; if so, we'll just move to the top left.
    unscaledExposeRect.setSize(unscaledExposeRect.size().shrunkTo(visualViewport.size()));

    // Compute how much we have to move the visualViewport to reveal the part of the layoutViewport that contains exposeRect.
    auto requiredVisualViewport = getRectToExposeForScrollIntoView(visualViewport, unscaledExposeRect, alignX, alignY);

    // Scale it back up.
    requiredVisualViewport.scale(frameScaleFactor());
    requiredVisualViewport.move(0, headerHeight());
    return requiredVisualViewport;
}

float LocalFrameView::deviceScaleFactor() const
{
    if (auto* page = m_frame->page())
        return page->deviceScaleFactor();
    return 1;
}

void LocalFrameView::writeRenderTreeAsText(TextStream& ts, OptionSet<RenderAsTextFlag> behavior)
{
    externalRepresentationForLocalFrame(ts, frame(), behavior);
}

void LocalFrameView::updateScrollAnchoringElement()
{
    if (m_scrollAnchoringController)
        m_scrollAnchoringController->updateAnchorElement();
}

void LocalFrameView::updateScrollPositionForScrollAnchoringController()
{
    if (m_scrollAnchoringController)
        m_scrollAnchoringController->adjustScrollPositionForAnchoring();
}

void LocalFrameView::invalidateScrollAnchoringElement()
{
    if (m_scrollAnchoringController)
        m_scrollAnchoringController->invalidateAnchorElement();
}

void LocalFrameView::scrollbarStyleDidChange()
{
    if (auto scrollableAreas = this->scrollableAreas()) {
        for (auto& scrollableArea : *scrollableAreas)
            scrollableArea.scrollbarsController().updateScrollbarStyle();
    }
    scrollbarsController().updateScrollbarStyle();
}

std::optional<FrameIdentifier> LocalFrameView::rootFrameID() const
{
    return m_frame->rootFrame().frameID();
}

void LocalFrameView::scrollbarWidthChanged(ScrollbarWidth width)
{
    scrollbarsController().scrollbarWidthChanged(width);
    m_needsDeferredScrollbarsUpdate = true;
}

int LocalFrameView::scrollbarGutterWidth(bool isHorizontalWritingMode) const
{
    if (verticalScrollbar() && verticalScrollbar()->isOverlayScrollbar())
        return 0;

    if (!verticalScrollbar() && !(scrollbarGutterStyle().isAuto || ScrollbarTheme::theme().usesOverlayScrollbars()) && isHorizontalWritingMode)
        return ScrollbarTheme::theme().scrollbarThickness(scrollbarWidthStyle());

    if (!verticalScrollbar())
        return 0;

    return verticalScrollbar()->width();
}

IntSize LocalFrameView::totalScrollbarSpace() const
{
    IntSize scrollbarGutter = { horizontalScrollbarIntrusion(), verticalScrollbarIntrusion() };

    if (isHorizontalWritingMode()) {
        if (scrollbarGutterStyle().bothEdges)
            scrollbarGutter.setWidth(scrollbarGutterWidth() * 2);
        else
            scrollbarGutter.setWidth(scrollbarGutterWidth());
    }
    return scrollbarGutter;
}

int LocalFrameView::insetForLeftScrollbarSpace() const
{
    if (scrollbarGutterStyle().bothEdges)
        return scrollbarGutterWidth();
    if (shouldPlaceVerticalScrollbarOnLeft())
        return verticalScrollbar() ? verticalScrollbar()->occupiedWidth() : 0;
    return 0;
}

} // namespace WebCore

#undef PAGE_ID
#undef FRAME_ID
