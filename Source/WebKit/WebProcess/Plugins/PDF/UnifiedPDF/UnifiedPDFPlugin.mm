/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "UnifiedPDFPlugin.h"

#if ENABLE(UNIFIED_PDF)

#include "PDFContextMenu.h"
#include "PDFKitSPI.h"
#include "PDFPluginAnnotation.h"
#include "PluginView.h"
#include "WebEventConversion.h"
#include "WebEventModifier.h"
#include "WebEventType.h"
#include "WebMouseEvent.h"
#include "WebPageProxyMessages.h"
#include <CoreGraphics/CoreGraphics.h>
#include <PDFKit/PDFKit.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/Chrome.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/ColorCocoa.h>
#include <WebCore/Editor.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/ScreenProperties.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollbarTheme.h>
#include <WebCore/ScrollbarsController.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

#include "PDFKitSoftLink.h"

@interface WKPDFFormMutationObserver : NSObject {
    WebKit::UnifiedPDFPlugin* _plugin;
}
@end

@implementation WKPDFFormMutationObserver

- (id)initWithPlguin:(WebKit::UnifiedPDFPlugin *)plugin
{
    if (!(self = [super init]))
        return nil;
    _plugin = plugin;
    return self;
}

- (void)formChanged:(NSNotification *)notification
{
    _plugin->didMutatePDFDocument();
}
@end

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIColor.h>
#endif

// FIXME: We should rationalize these with the values in ViewGestureController.
// For now, we'll leave them differing as they do in PDFPlugin.
static constexpr CGFloat minimumZoomScale = 0.2;
static constexpr CGFloat maximumZoomScale = 6.0;
static constexpr CGFloat zoomIncrement = 1.18920;

namespace WebKit {
using namespace WebCore;

Ref<UnifiedPDFPlugin> UnifiedPDFPlugin::create(HTMLPlugInElement& pluginElement)
{
    return adoptRef(*new UnifiedPDFPlugin(pluginElement));
}

UnifiedPDFPlugin::UnifiedPDFPlugin(HTMLPlugInElement& element)
    : PDFPluginBase(element)
    , m_pdfMutationObserver(adoptNS([[WKPDFFormMutationObserver alloc] initWithPlguin:this]))
{
    this->setVerticalScrollElasticity(ScrollElasticity::Automatic);
    this->setHorizontalScrollElasticity(ScrollElasticity::Automatic);

    if (supportsForms()) {
        Ref document = element.document();
        m_annotationContainer = document->createElement(HTMLNames::divTag, false);
        m_annotationContainer->setAttributeWithoutSynchronization(HTMLNames::idAttr, "annotationContainer"_s);

        auto annotationStyleElement = document->createElement(HTMLNames::styleTag, false);
        annotationStyleElement->setTextContent(annotationStyle);

        m_annotationContainer->appendChild(annotationStyleElement);
        RefPtr { document->bodyOrFrameset() }->appendChild(*m_annotationContainer);
    }
}

static String mutationObserverNotificationString()
{
    static NeverDestroyed<String> notificationString = "PDFFormDidChangeValue"_s;
    return notificationString;
}

void UnifiedPDFPlugin::teardown()
{
    PDFPluginBase::teardown();

    if (m_rootLayer)
        m_rootLayer->removeFromParent();

    RefPtr page = this->page();
    if (m_scrollingNodeID && page) {
        RefPtr scrollingCoordinator = page->scrollingCoordinator();
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_scrollingNodeID);
        m_frame->coreLocalFrame()->protectedView()->removePluginScrollableAreaForScrollingNodeID(m_scrollingNodeID);
    }
    [[NSNotificationCenter defaultCenter] removeObserver:m_pdfMutationObserver.get() name:mutationObserverNotificationString() object:m_pdfDocument.get()];
    m_pdfMutationObserver = nullptr;
}

GraphicsLayer* UnifiedPDFPlugin::graphicsLayer() const
{
    return m_rootLayer.get();
}

void UnifiedPDFPlugin::installPDFDocument()
{
    ASSERT(isMainRunLoop());

    if (m_hasBeenDestroyed)
        return;

    if (!m_pdfDocument)
        return;

    if (!m_view)
        return;

    m_documentLayout.setPDFDocument(m_pdfDocument.get());

    updateLayout();

    if (m_view)
        m_view->layerHostingStrategyDidChange();
    [[NSNotificationCenter defaultCenter] addObserver:m_pdfMutationObserver.get() selector:@selector(formChanged:) name:mutationObserverNotificationString() object:m_pdfDocument.get()];
}

RefPtr<GraphicsLayer> UnifiedPDFPlugin::createGraphicsLayer(const String& name, GraphicsLayer::Type layerType)
{
    RefPtr page = this->page();
    if (!page)
        return nullptr;

    auto* graphicsLayerFactory = page->chrome().client().graphicsLayerFactory();
    Ref graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, *this, layerType);
    graphicsLayer->setName(name);
    return graphicsLayer;
}

void UnifiedPDFPlugin::scheduleRenderingUpdate()
{
    RefPtr page = this->page();
    if (!page)
        return;

    page->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

void UnifiedPDFPlugin::ensureLayers()
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (!m_rootLayer) {
        m_rootLayer = createGraphicsLayer("UnifiedPDFPlugin root"_s, GraphicsLayer::Type::Normal);
        m_rootLayer->setAnchorPoint({ });
        m_rootLayer->setBackgroundColor(WebCore::roundAndClampToSRGBALossy([WebCore::CocoaColor grayColor].CGColor));
        if (handlesPageScaleFactor())
            m_rootLayer->setAppliesPageScale();
    }

    if (!m_scrollContainerLayer) {
        m_scrollContainerLayer = createGraphicsLayer("UnifiedPDFPlugin scroll container"_s, GraphicsLayer::Type::ScrollContainer);
        m_scrollContainerLayer->setAnchorPoint({ });
        m_scrollContainerLayer->setMasksToBounds(true);
        m_rootLayer->addChild(*m_scrollContainerLayer);
    }

    if (!m_scrolledContentsLayer) {
        m_scrolledContentsLayer = createGraphicsLayer("UnifiedPDFPlugin scrolled contents"_s, GraphicsLayer::Type::ScrolledContents);
        m_scrolledContentsLayer->setAnchorPoint({ });
        m_scrollContainerLayer->addChild(*m_scrolledContentsLayer);
    }

    if (!m_contentsLayer) {
        m_contentsLayer = createGraphicsLayer("UnifiedPDFPlugin contents"_s, GraphicsLayer::Type::TiledBacking);
        m_contentsLayer->setAnchorPoint({ });
        m_contentsLayer->setDrawsContent(true);
        m_scrolledContentsLayer->addChild(*m_contentsLayer);
    }

    if (!m_overflowControlsContainer) {
        m_overflowControlsContainer = createGraphicsLayer("UnifiedPDFPlugin overflow controls container"_s, GraphicsLayer::Type::Normal);
        m_overflowControlsContainer->setAnchorPoint({ });
        m_rootLayer->addChild(*m_overflowControlsContainer);
    }
}

ScrollingNodeID UnifiedPDFPlugin::scrollingNodeID() const
{
    if (!m_scrollingNodeID)
        const_cast<UnifiedPDFPlugin*>(this)->createScrollingNodeIfNecessary();

    return m_scrollingNodeID;
}

void UnifiedPDFPlugin::createScrollingNodeIfNecessary()
{
    if (m_scrollingNodeID)
        return;

    RefPtr page = this->page();
    if (!page)
        return;

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    if (!scrollingCoordinator)
        return;

    m_scrollingNodeID = scrollingCoordinator->uniqueScrollingNodeID();
    scrollingCoordinator->createNode(ScrollingNodeType::PluginScrolling, m_scrollingNodeID);

#if ENABLE(SCROLLING_THREAD)
    m_scrollContainerLayer->setScrollingNodeID(m_scrollingNodeID);
#endif

    m_frame->coreLocalFrame()->protectedView()->setPluginScrollableAreaForScrollingNodeID(m_scrollingNodeID, *this);

    WebCore::ScrollingCoordinator::NodeLayers nodeLayers;
    nodeLayers.layer = m_rootLayer.get();
    nodeLayers.scrollContainerLayer = m_scrollContainerLayer.get();
    nodeLayers.scrolledContentsLayer = m_scrolledContentsLayer.get();

    scrollingCoordinator->setNodeLayers(m_scrollingNodeID, nodeLayers);
}

void UnifiedPDFPlugin::updateLayerHierarchy()
{
    ensureLayers();

    // The m_rootLayer's position is set in RenderLayerBacking::updateAfterWidgetResize().
    m_rootLayer->setSize(size());
    m_overflowControlsContainer->setSize(size());

    auto scrollContainerRect = availableContentsRect();
    m_scrollContainerLayer->setPosition(scrollContainerRect.location());
    m_scrollContainerLayer->setSize(scrollContainerRect.size());

    m_contentsLayer->setSize(documentSize());
    m_contentsLayer->setNeedsDisplay();

    didChangeSettings();
    didChangeIsInWindow();
}

void UnifiedPDFPlugin::didChangeSettings()
{
    RefPtr page = this->page();
    if (!page)
        return;

    Settings& settings = page->settings();
    bool showDebugBorders = settings.showDebugBorders();
    bool showRepaintCounter = settings.showRepaintCounter();

    auto propagateSettingsToLayer = [&] (GraphicsLayer& layer) {
        layer.setShowDebugBorder(showDebugBorders);
        layer.setShowRepaintCounter(showRepaintCounter);
    };
    propagateSettingsToLayer(*m_rootLayer);
    propagateSettingsToLayer(*m_scrollContainerLayer);
    propagateSettingsToLayer(*m_scrolledContentsLayer);
    propagateSettingsToLayer(*m_contentsLayer);

    if (m_layerForHorizontalScrollbar)
        propagateSettingsToLayer(*m_layerForHorizontalScrollbar);

    if (m_layerForVerticalScrollbar)
        propagateSettingsToLayer(*m_layerForVerticalScrollbar);

    if (m_layerForScrollCorner)
        propagateSettingsToLayer(*m_layerForScrollCorner);
}

void UnifiedPDFPlugin::notifyFlushRequired(const GraphicsLayer*)
{
    scheduleRenderingUpdate();
}

void UnifiedPDFPlugin::didChangeIsInWindow()
{
    RefPtr page = this->page();
    if (!page || !m_contentsLayer)
        return;
    m_contentsLayer->tiledBacking()->setIsInWindow(page->isInWindow());
}

void UnifiedPDFPlugin::windowActivityDidChange()
{
    // FIXME: <https://webkit.org/b/268927> Selection painting requests should be optimized by specifying dirty rects.
    m_contentsLayer->setNeedsDisplay();
}

void UnifiedPDFPlugin::paint(WebCore::GraphicsContext& context, const WebCore::IntRect&)
{
    // Only called for snapshotting.
    if (size().isEmpty())
        return;

    context.translate(-m_scrollOffset.width(), -m_scrollOffset.height());
    paintPDFContent(context, { FloatPoint(m_scrollOffset), size() });
}

void UnifiedPDFPlugin::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
{
    // This scrollbar painting code is used in the non-UI-side compositing configuration.
    auto paintScrollbar = [](Scrollbar* scrollbar, GraphicsContext& context) {
        if (!scrollbar)
            return;

        GraphicsContextStateSaver stateSaver(context);
        auto scrollbarRect = scrollbar->frameRect();
        context.translate(-scrollbarRect.location());
        scrollbar->paint(context, scrollbarRect);
    };

    if (layer == layerForHorizontalScrollbar()) {
        paintScrollbar(m_horizontalScrollbar.get(), context);
        return;
    }

    if (layer == layerForVerticalScrollbar()) {
        paintScrollbar(m_verticalScrollbar.get(), context);
        return;
    }

    if (layer == layerForScrollCorner()) {
        auto cornerRect = viewRelativeScrollCornerRect();

        GraphicsContextStateSaver stateSaver(context);
        context.translate(-cornerRect.location());
        ScrollbarTheme::theme().paintScrollCorner(*this, context, cornerRect);
        return;
    }

    if (layer != m_contentsLayer.get())
        return;

    paintPDFContent(context, clipRect);
}

void UnifiedPDFPlugin::paintPDFContent(WebCore::GraphicsContext& context, const WebCore::FloatRect& clipRect)
{
    if (m_size.isEmpty() || documentSize().isEmpty())
        return;

    auto drawingRect = IntRect { { }, documentSize() };
    drawingRect.intersect(enclosingIntRect(clipRect));

    auto stateSaver = GraphicsContextStateSaver(context);

    auto scale = m_documentLayout.scale();
    context.scale(scale);

    auto inverseScale = 1.0f / scale;
    auto scaleTransform = AffineTransform::makeScale({ inverseScale, inverseScale });
    auto drawingRectInPDFLayoutCoordinates = scaleTransform.mapRect(FloatRect { drawingRect });

    for (PDFDocumentLayout::PageIndex i = 0; i < m_documentLayout.pageCount(); ++i) {
        auto page = m_documentLayout.pageAtIndex(i);
        if (!page)
            continue;

        auto destinationRect = m_documentLayout.boundsForPageAtIndex(i);

        // FIXME: If we draw page shadows we'll have to inflate destinationRect here.
        if (!destinationRect.intersects(drawingRectInPDFLayoutCoordinates))
            continue;

        auto pageStateSaver = GraphicsContextStateSaver(context);
        context.clip(destinationRect);
        context.fillRect(destinationRect, Color::white);

        // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
        // from this page's drawing origin.
        context.translate(destinationRect.minXMaxYCorner());
        context.scale({ 1, -1 });

        [page drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];

        bool isVisibleAndActive = true;
        if (RefPtr page = this->page())
            isVisibleAndActive = page->isVisibleAndActive();
        [m_currentSelection drawForPage:page.get() withBox:kCGPDFCropBox active:isVisibleAndActive inContext:context.platformContext()];
    }
    paintPDFOverlays(context);
}

float UnifiedPDFPlugin::scaleForActualSize() const
{
#if PLATFORM(MAC)
    if (!m_frame || !m_frame->coreLocalFrame())
        return 1;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return 1;

    auto* screenData = WebCore::screenData(webPage->corePage()->displayID());
    if (!screenData)
        return 1;

    if (!m_documentLayout.pageCount() || documentSize().isEmpty())
        return 1;

    auto firstPageBounds = m_documentLayout.boundsForPageAtIndex(0);
    if (firstPageBounds.isEmpty())
        return 1;

    constexpr auto pdfDotsPerInch = 72.0;
    auto screenDPI = screenData->screenDPI();
    float pixelSize = screenDPI * firstPageBounds.width() / pdfDotsPerInch;

    return pixelSize / size().width();
#endif
    return 1;
}

static const WebCore::Color textAnnotationHoverColor()
{
    static constexpr auto textAnnotationHoverAlpha = 0.12;
    static NeverDestroyed color = WebCore::Color::createAndPreserveColorSpace([[[WebCore::CocoaColor systemBlueColor] colorWithAlphaComponent:textAnnotationHoverAlpha] CGColor]);
    return color;
}

void UnifiedPDFPlugin::paintPDFOverlays(WebCore::GraphicsContext& context)
{
    if (auto* trackedAnnotation = m_annotationTrackingState.trackedAnnotation(); trackedAnnotation && [trackedAnnotation isKindOfClass:getPDFAnnotationTextWidgetClass()] && m_annotationTrackingState.isBeingHovered())
        context.fillRect(IntRect { [trackedAnnotation bounds] }, textAnnotationHoverColor());
}

CGFloat UnifiedPDFPlugin::scaleFactor() const
{
    return m_scaleFactor;
}

CGSize UnifiedPDFPlugin::contentSizeRespectingZoom() const
{
    return { };
}

float UnifiedPDFPlugin::deviceScaleFactor() const
{
    return PDFPluginBase::deviceScaleFactor();
}

void UnifiedPDFPlugin::didBeginMagnificationGesture()
{
    m_inMagnificationGesture = true;
}

void UnifiedPDFPlugin::didEndMagnificationGesture()
{
    m_inMagnificationGesture = false;
    m_rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
}

void UnifiedPDFPlugin::setPageScaleFactor(double scale, std::optional<WebCore::IntPoint> origin)
{
    if (!handlesPageScaleFactor()) {
        m_rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
        return;
    }

    RefPtr page = this->page();
    if (!page)
        return;

    auto oldScrollPosition = this->scrollPosition();
    auto oldScale = std::exchange(m_scaleFactor, scale);

    updateScrollbars();
    updateScrollingExtents();

    if (!m_inMagnificationGesture)
        m_rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    TransformationMatrix transform;
    transform.scale(m_scaleFactor);
    transform.translate(sidePaddingWidth(), 0);

    m_contentsLayer->setTransform(transform);

    if (!origin)
        origin = IntRect({ }, size()).center();

    auto gestureOriginInContentsCoordinates = convertFromRootViewToPlugin(*origin);
    gestureOriginInContentsCoordinates.moveBy(oldScrollPosition);

    auto gestureOriginInNewContentsCoordinates = gestureOriginInContentsCoordinates;
    gestureOriginInNewContentsCoordinates.scale(m_scaleFactor / oldScale);

    auto delta = gestureOriginInNewContentsCoordinates - gestureOriginInContentsCoordinates;
    auto newPosition = oldScrollPosition + delta;
    newPosition = newPosition.expandedTo({ 0, 0 });

    auto options = ScrollPositionChangeOptions::createUser();
    options.originalScrollDelta = delta;
    page->protectedScrollingCoordinator()->requestScrollToPosition(*this, newPosition, options);

    scheduleRenderingUpdate();
}

bool UnifiedPDFPlugin::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    if (!PDFPluginBase::geometryDidChange(pluginSize, pluginToRootViewTransform))
        return false;

    updateLayout();
    return true;
}

IntRect UnifiedPDFPlugin::availableContentsRect() const
{
    auto availableRect = IntRect({ }, size());
    if (ScrollbarTheme::theme().usesOverlayScrollbars())
        return availableRect;

    int verticalScrollbarSpace = 0;
    if (m_verticalScrollbar)
        verticalScrollbarSpace = m_verticalScrollbar->width();

    int horizontalScrollbarSpace = 0;
    if (m_horizontalScrollbar)
        horizontalScrollbarSpace = m_horizontalScrollbar->height();

    availableRect.contract(verticalScrollbarSpace, horizontalScrollbarSpace);

    // Don't allow negative sizes
    availableRect.setWidth(std::max(0, availableRect.width()));
    availableRect.setHeight(std::max(0, availableRect.height()));

    return availableRect;
}

void UnifiedPDFPlugin::updateLayout()
{
    auto layoutSize = availableContentsRect().size();
    m_documentLayout.updateLayout(layoutSize);
    updateScrollbars();

    // Do a second layout pass if the first one changed scrollbars.
    auto newLayoutSize = availableContentsRect().size();
    if (layoutSize != newLayoutSize) {
        m_documentLayout.updateLayout(newLayoutSize);
        updateScrollbars();
    }

    updateLayerHierarchy();
    updateScrollingExtents();
}

float UnifiedPDFPlugin::sidePaddingWidth() const
{
    auto sidePadding = 0.0f;
    if (m_scaleFactor < 1) {
        auto availableWidth = availableContentsRect().size().width();
        auto documentPresentationWidth = m_documentLayout.scaledContentsSize().width() * m_scaleFactor;

        sidePadding = std::floor(std::max<float>(availableWidth - documentPresentationWidth, 0) / 2);
        sidePadding /= m_scaleFactor;
    }

    return sidePadding;
}

IntSize UnifiedPDFPlugin::documentSize() const
{
    if (isLocked())
        return { 0, 0 };

    auto size = m_documentLayout.scaledContentsSize();
    return expandedIntSize(size);
}

IntSize UnifiedPDFPlugin::contentsSize() const
{
    if (isLocked())
        return { 0, 0 };

    auto size = m_documentLayout.scaledContentsSize();
    size.scale(m_scaleFactor);
    return expandedIntSize(size);
}

unsigned UnifiedPDFPlugin::firstPageHeight() const
{
    if (isLocked() || !m_documentLayout.pageCount())
        return 0;
    return static_cast<unsigned>(CGCeiling(m_documentLayout.boundsForPageAtIndex(0).height()));
}

RefPtr<FragmentedSharedBuffer> UnifiedPDFPlugin::liveResourceData() const
{
    NSData *pdfData = liveData();

    if (!pdfData)
        return nullptr;

    return SharedBuffer::create(pdfData);
}

NSData *UnifiedPDFPlugin::liveData() const
{
#if PLATFORM(MAC)
    if (m_activeAnnotation)
        m_activeAnnotation->commit();
#endif
    // Save data straight from the resource instead of PDFKit if the document is
    // untouched by the user, so that PDFs which PDFKit can't display will still be downloadable.
    if (m_pdfDocumentWasMutated)
        return [m_pdfDocument dataRepresentation];

    return originalData();
}

void UnifiedPDFPlugin::didChangeScrollOffset()
{
    if (this->currentScrollType() == ScrollType::User)
        m_scrollContainerLayer->syncBoundsOrigin(IntPoint(m_scrollOffset));
    else
        m_scrollContainerLayer->setBoundsOrigin(IntPoint(m_scrollOffset));

#if PLATFORM(MAC)
    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();
#endif

    scheduleRenderingUpdate();
}

bool UnifiedPDFPlugin::updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer)
{
    if (scrollingMode() == DelegatedScrollingMode::DelegatedToNativeScrollView)
        return false;

    RefPtr page = this->page();
    if (!page)
        return false;

    auto createOrDestroyLayer = [&](RefPtr<GraphicsLayer>& layer, bool needLayer, ASCIILiteral layerName) {
        if (needLayer == !!layer)
            return false;

        if (needLayer) {
            layer = createGraphicsLayer(layerName, GraphicsLayer::Type::Normal);
            layer->setAllowsBackingStoreDetaching(false);
            layer->setAllowsTiling(false);
            layer->setDrawsContent(true);

#if ENABLE(SCROLLING_THREAD)
            layer->setScrollingNodeID(m_scrollingNodeID);
#endif

            m_overflowControlsContainer->addChild(*layer);
        } else
            GraphicsLayer::unparentAndClear(layer);

        return true;
    };

    ensureLayers();

    bool layersChanged = false;

    bool horizontalScrollbarLayerChanged = createOrDestroyLayer(m_layerForHorizontalScrollbar, needsHorizontalScrollbarLayer, "horizontal scrollbar"_s);
    layersChanged |= horizontalScrollbarLayerChanged;

    bool verticalScrollbarLayerChanged = createOrDestroyLayer(m_layerForVerticalScrollbar, needsVerticalScrollbarLayer, "vertical scrollbar"_s);
    layersChanged |= verticalScrollbarLayerChanged;

    layersChanged |= createOrDestroyLayer(m_layerForScrollCorner, needsScrollCornerLayer, "scroll corner"_s);

    auto& scrollingCoordinator = *page->scrollingCoordinator();
    if (horizontalScrollbarLayerChanged)
        scrollingCoordinator.scrollableAreaScrollbarLayerDidChange(*this, ScrollbarOrientation::Horizontal);
    if (verticalScrollbarLayerChanged)
        scrollingCoordinator.scrollableAreaScrollbarLayerDidChange(*this, ScrollbarOrientation::Vertical);

    return layersChanged;
}

void UnifiedPDFPlugin::positionOverflowControlsLayers()
{
    auto overflowControlsPositioningRect = IntRect({ }, size());

    auto positionScrollbarLayer = [](GraphicsLayer& layer, const IntRect& scrollbarRect) {
        layer.setPosition(scrollbarRect.location());
        layer.setSize(scrollbarRect.size());
    };

    if (auto* layer = layerForHorizontalScrollbar())
        positionScrollbarLayer(*layer, viewRelativeHorizontalScrollbarRect());

    if (auto* layer = layerForVerticalScrollbar())
        positionScrollbarLayer(*layer, viewRelativeVerticalScrollbarRect());

    if (auto* layer = layerForScrollCorner()) {
        auto cornerRect = viewRelativeScrollCornerRect();
        layer->setPosition(cornerRect.location());
        layer->setSize(cornerRect.size());
        layer->setDrawsContent(!cornerRect.isEmpty());
    }
}

void UnifiedPDFPlugin::invalidateScrollbarRect(WebCore::Scrollbar& scrollbar, const WebCore::IntRect& rect)
{
    if (&scrollbar == m_verticalScrollbar.get()) {
        if (auto* layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }

        return;
    }

    if (&scrollbar == m_horizontalScrollbar.get()) {
        if (auto* layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
        return;
    }
}

void UnifiedPDFPlugin::invalidateScrollCornerRect(const WebCore::IntRect& rect)
{
    if (auto* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }
}

GraphicsLayer* UnifiedPDFPlugin::layerForHorizontalScrollbar() const
{
    return m_layerForHorizontalScrollbar.get();
}

GraphicsLayer* UnifiedPDFPlugin::layerForVerticalScrollbar() const
{
    return m_layerForVerticalScrollbar.get();
}

GraphicsLayer* UnifiedPDFPlugin::layerForScrollCorner() const
{
    return m_layerForScrollCorner.get();
}

void UnifiedPDFPlugin::createScrollbarsController()
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (auto scrollbarController = page->chrome().client().createScrollbarsController(*page, *this)) {
        setScrollbarsController(WTFMove(scrollbarController));
        return;
    }

    PDFPluginBase::createScrollbarsController();
}

DelegatedScrollingMode UnifiedPDFPlugin::scrollingMode() const
{
#if PLATFORM(IOS_FAMILY)
    return DelegatedScrollingMode::DelegatedToNativeScrollView;
#else
    return DelegatedScrollingMode::NotDelegated;
#endif
}

void UnifiedPDFPlugin::scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate)
{
    if (!forceUpdate)
        return;

    if (m_hasBeenDestroyed)
        return;

    updateLayout();
}

void UnifiedPDFPlugin::updateScrollbars()
{
    PDFPluginBase::updateScrollbars();

    bool hasHorizontalScrollbar = !!m_horizontalScrollbar;
    bool hasVerticalScrollbar = !!m_verticalScrollbar;
    bool showsScrollCorner = hasHorizontalScrollbar && hasVerticalScrollbar && !m_verticalScrollbar->isOverlayScrollbar();
    updateOverflowControlsLayers(hasHorizontalScrollbar, hasVerticalScrollbar, showsScrollCorner);
    positionOverflowControlsLayers();
}

void UnifiedPDFPlugin::updateScrollingExtents()
{
    RefPtr page = this->page();
    if (!page)
        return;

    // FIXME: It would be good to adjust the scroll to reveal the current page when changing view modes.
    auto scrollPosition = this->scrollPosition();
    auto constrainedPosition = constrainedScrollPosition(scrollPosition);
    if (scrollPosition != constrainedPosition) {
        auto oldScrollType = currentScrollType();
        setCurrentScrollType(ScrollType::Programmatic); // It's silly that we have to do this to avoid an AsyncScrollingCoordinator assertion.
        requestScrollToPosition(constrainedPosition);
        setCurrentScrollType(oldScrollType);
    }

    auto& scrollingCoordinator = *page->scrollingCoordinator();
    scrollingCoordinator.setScrollingNodeScrollableAreaGeometry(m_scrollingNodeID, *this);

    // FIXME: Use setEventRegionToLayerBounds().
    EventRegion eventRegion;
    auto eventRegionContext = eventRegion.makeContext();
    eventRegionContext.unite(FloatRoundedRect(FloatRect({ }, size())), *m_element->renderer(), m_element->renderer()->style());
    m_scrollContainerLayer->setEventRegion(WTFMove(eventRegion));
}

bool UnifiedPDFPlugin::requestScrollToPosition(const ScrollPosition& position, const ScrollPositionChangeOptions& options)
{
    RefPtr page = this->page();
    if (!page)
        return false;

    auto& scrollingCoordinator = *page->scrollingCoordinator();
    return scrollingCoordinator.requestScrollToPosition(*this, position, options);
}

bool UnifiedPDFPlugin::requestStartKeyboardScrollAnimation(const KeyboardScroll& scrollData)
{
    RefPtr page = this->page();
    if (!page)
        return false;

    auto& scrollingCoordinator = *page->scrollingCoordinator();
    return scrollingCoordinator.requestStartKeyboardScrollAnimation(*this, scrollData);
}

bool UnifiedPDFPlugin::requestStopKeyboardScrollAnimation(bool immediate)
{
    RefPtr page = this->page();
    if (!page)
        return false;

    auto& scrollingCoordinator = *page->scrollingCoordinator();
    return scrollingCoordinator.requestStopKeyboardScrollAnimation(*this, immediate);
}

enum class AltKeyIsActive : bool { No, Yes };

static WebCore::Cursor::Type toWebCoreCursorType(UnifiedPDFPlugin::PDFElementTypes pdfElementTypes, AltKeyIsActive altKeyIsActive = AltKeyIsActive::No)
{
    using PDFElementType = UnifiedPDFPlugin::PDFElementType;

    if (pdfElementTypes.containsAny({ PDFElementType::Link, PDFElementType::Control, PDFElementType::Icon }) || altKeyIsActive == AltKeyIsActive::Yes)
        return WebCore::Cursor::Type::Hand;

    if (pdfElementTypes.containsAny({ PDFElementType::Text, PDFElementType::TextField }))
        return WebCore::Cursor::Type::IBeam;

    return WebCore::Cursor::Type::Pointer;
}

WebCore::IntPoint UnifiedPDFPlugin::convertFromPluginToDocument(const WebCore::IntPoint& point) const
{
    auto transformedPoint = FloatPoint { point };
    transformedPoint.moveBy(FloatPoint(m_scrollOffset));

    transformedPoint.scale(1.0f / m_scaleFactor);
    transformedPoint.move(-sidePaddingWidth(), 0);
    transformedPoint.scale(1.0f / m_documentLayout.scale());

    return roundedIntPoint(transformedPoint);
}

WebCore::IntPoint UnifiedPDFPlugin::convertFromDocumentToPlugin(const WebCore::IntPoint& point) const
{
    auto transformedPoint = FloatPoint { point };

    transformedPoint.scale(m_documentLayout.scale());
    transformedPoint.move(sidePaddingWidth(), 0);
    transformedPoint.scale(m_scaleFactor);
    transformedPoint.moveBy(-FloatPoint(m_scrollOffset));

    return roundedIntPoint(transformedPoint);
}

std::optional<PDFDocumentLayout::PageIndex> UnifiedPDFPlugin::pageIndexForDocumentPoint(const WebCore::IntPoint& point) const
{
    for (PDFDocumentLayout::PageIndex index = 0; index < m_documentLayout.pageCount(); ++index) {
        auto pageBounds = m_documentLayout.boundsForPageAtIndex(index);
        if (pageBounds.contains(point))
            return index;
    }

    return std::nullopt;
}

RetainPtr<PDFAnnotation> UnifiedPDFPlugin::annotationForRootViewPoint(const WebCore::IntPoint& point) const
{
    auto pointInDocumentSpace = convertFromPluginToDocument(convertFromRootViewToPlugin(point));
    auto pageIndex = pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!pageIndex)
        return nullptr;

    auto page = m_documentLayout.pageAtIndex(pageIndex.value());
    return [page annotationAtPoint:convertFromDocumentToPage(pointInDocumentSpace, pageIndex.value())];
}

static AffineTransform documentSpaceToPageSpaceTransform(const IntDegrees& pageRotation, const FloatRect& pageBounds)
{
    return AffineTransform::makeRotation(pageRotation).translate([&pageBounds, pageRotation] () -> FloatPoint {
        auto width = pageBounds.width();
        auto height = pageBounds.height();
        switch (pageRotation) {
        case 0:
            return { };
        case 90:
            return { 0, height };
        case 180:
            return { height, width };
        case 270:
            return { width, 0 };
        default:
            ASSERT_NOT_REACHED();
            return { };
        }
    }());
}

WebCore::IntPoint UnifiedPDFPlugin::convertFromDocumentToPage(const WebCore::IntPoint& point, PDFDocumentLayout::PageIndex pageIndex) const
{
    ASSERT(pageIndex < m_documentLayout.pageCount());

    auto pageBounds = m_documentLayout.boundsForPageAtIndex(pageIndex);
    auto pageRotation = m_documentLayout.rotationForPageAtIndex(pageIndex);
    auto pointInPDFPageSpace = IntPoint { point - WebCore::flooredIntPoint(pageBounds.location()) };

    auto pointWithTopLeftOrigin = documentSpaceToPageSpaceTransform(pageRotation, pageBounds).mapPoint(pointInPDFPageSpace);
    return IntPoint { pointWithTopLeftOrigin.x(), static_cast<int>(pageBounds.height()) - pointWithTopLeftOrigin.y() };
}

WebCore::IntPoint UnifiedPDFPlugin::convertFromPageToDocument(const WebCore::IntPoint& pageSpacePoint, PDFDocumentLayout::PageIndex pageIndex) const
{
    auto pageBounds = m_documentLayout.boundsForPageAtIndex(pageIndex);
    auto pageRotation = m_documentLayout.rotationForPageAtIndex(pageIndex);
    auto pageSpacePointWithFlippedYOrigin = IntPoint { pageSpacePoint.x(), static_cast<int>(pageBounds.height()) - pageSpacePoint.y() };
    auto documentSpacePoint = WebCore::flooredIntPoint(pageBounds.location()) + pageSpacePointWithFlippedYOrigin;
    return documentSpaceToPageSpaceTransform(pageRotation, pageBounds).inverse()->mapPoint(documentSpacePoint);
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, UnifiedPDFPlugin::PDFElementType elementType)
{
    switch (elementType) {
    case UnifiedPDFPlugin::PDFElementType::Page: ts << "page"; break;
    case UnifiedPDFPlugin::PDFElementType::Text: ts << "text"; break;
    case UnifiedPDFPlugin::PDFElementType::Annotation: ts << "annotation"; break;
    case UnifiedPDFPlugin::PDFElementType::Link: ts << "link"; break;
    case UnifiedPDFPlugin::PDFElementType::Control: ts << "control"; break;
    case UnifiedPDFPlugin::PDFElementType::TextField: ts << "text field"; break;
    case UnifiedPDFPlugin::PDFElementType::Icon: ts << "icon"; break;
    case UnifiedPDFPlugin::PDFElementType::Popup: ts << "popup"; break;
    case UnifiedPDFPlugin::PDFElementType::Image: ts << "image"; break;
    }
    return ts;
}
#endif

auto UnifiedPDFPlugin::pdfElementTypesForPluginPoint(const WebCore::IntPoint& point) const -> PDFElementTypes
{
    auto pointInDocumentSpace = convertFromPluginToDocument(point);
    auto hitPageIndex = pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!hitPageIndex || *hitPageIndex >= m_documentLayout.pageCount())
        return { };

    auto pageIndex = *hitPageIndex;
    auto page = m_documentLayout.pageAtIndex(pageIndex);
    auto pointInPDFPageSpace = convertFromDocumentToPage(pointInDocumentSpace, pageIndex);

    PDFElementTypes pdfElementTypes { PDFElementType::Page };

    if (auto annotation = [page annotationAtPoint:pointInPDFPageSpace]) {
        pdfElementTypes.add(PDFElementType::Annotation);

        if ([annotation isKindOfClass:getPDFAnnotationLinkClass()])
            pdfElementTypes.add(PDFElementType::Link);

        if ([annotation isKindOfClass:getPDFAnnotationPopupClass()])
            pdfElementTypes.add(PDFElementType::Popup);

        if ([annotation isKindOfClass:getPDFAnnotationTextClass()])
            pdfElementTypes.add(PDFElementType::Icon);

        if ([annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] && ![annotation isReadOnly])
            pdfElementTypes.add(PDFElementType::TextField);

        if ([annotation isKindOfClass:getPDFAnnotationButtonWidgetClass()] && ![annotation isReadOnly])
            pdfElementTypes.add(PDFElementType::Control);
    }

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::pdfElementTypesForPluginPoint " << point << " document point " << pointInDocumentSpace << " found page " << pageIndex << " point in page " << pointInPDFPageSpace << " - elements " << pdfElementTypes);

    if (!isTaggedPDF())
        return pdfElementTypes;

    if (auto pageLayout = [page pageLayout]) {
        CGPDFAreaOfInterest areaOfInterest = CGPDFPageLayoutGetAreaOfInterestAtPoint(pageLayout, pointInPDFPageSpace);
        if (areaOfInterest & kCGPDFAreaText)
            pdfElementTypes.add(PDFElementType::Text);
        if (areaOfInterest & kCGPDFAreaImage)
            pdfElementTypes.add(PDFElementType::Image);
    }

    // FIXME: <https://webkit.org/b/265908> Cursor updates are incorrect over text/image elements for untagged PDFs.

    return pdfElementTypes;
}

bool UnifiedPDFPlugin::handleMouseEvent(const WebMouseEvent& event)
{
    m_lastMouseEvent = event;

    auto pointInDocumentSpace = convertFromPluginToDocument(lastKnownMousePositionInView());
    auto pageIndex = pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!pageIndex)
        return false;

    auto pointInPageSpace = convertFromDocumentToPage(pointInDocumentSpace, *pageIndex);

    switch (auto mouseEventType = event.type()) {
    case WebEventType::MouseMove:
        mouseMovedInContentArea();
        switch (auto mouseEventButton = event.button()) {
        case WebMouseEventButton::None: {
            auto altKeyIsActive = event.altKey() ? AltKeyIsActive::Yes : AltKeyIsActive::No;
            auto pdfElementTypes = pdfElementTypesForPluginPoint(lastKnownMousePositionInView());
            notifyCursorChanged(toWebCoreCursorType(pdfElementTypes, altKeyIsActive));

            RetainPtr annotationUnderMouse = annotationForRootViewPoint(event.position());
            bool annotationTrackingStateChanged = false;
            if (auto* currentTrackedAnnotation = m_annotationTrackingState.trackedAnnotation(); (currentTrackedAnnotation && currentTrackedAnnotation != annotationUnderMouse) || (currentTrackedAnnotation && !m_annotationTrackingState.isBeingHovered())) {
                m_annotationTrackingState.finishAnnotationTracking(mouseEventType, mouseEventButton);
                annotationTrackingStateChanged = true;
            }
            if (!m_annotationTrackingState.trackedAnnotation() && annotationUnderMouse && [annotationUnderMouse isKindOfClass:getPDFAnnotationTextWidgetClass()]) {
                m_annotationTrackingState.startAnnotationTracking(WTFMove(annotationUnderMouse), mouseEventType, mouseEventButton);
                annotationTrackingStateChanged = true;
            }
            if (annotationTrackingStateChanged)
                updateLayerHierarchy();
            return true;
        }
        case WebMouseEventButton::Left: {
            if (RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation(); trackedAnnotation && trackedAnnotation != annotationForRootViewPoint(event.position())) {
                notifyCursorChanged(toWebCoreCursorType({ }));
                m_annotationTrackingState.finishAnnotationTracking(mouseEventType, mouseEventButton);
                updateLayerHierarchy();
                return true;
            }

            continueTrackingSelection(*pageIndex, pointInPageSpace);
            return true;
        }
        default:
            return false;
        }
    case WebEventType::MouseDown:
        switch (auto mouseEventButton = event.button()) {
        case WebMouseEventButton::Left: {
            if (RetainPtr<PDFAnnotation> annotation = annotationForRootViewPoint(event.position())) {
                if (([annotation isKindOfClass:getPDFAnnotationButtonWidgetClass()] || [annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] || [annotation isKindOfClass:getPDFAnnotationChoiceWidgetClass()]) && [annotation isReadOnly])
                    return true;

                if ([annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] || [annotation isKindOfClass:getPDFAnnotationChoiceWidgetClass()]) {
                    setActiveAnnotation(WTFMove(annotation));
                    return true;
                }

                if ([annotation isKindOfClass:getPDFAnnotationButtonWidgetClass()]) {
                    m_annotationTrackingState.startAnnotationTracking(WTFMove(annotation), mouseEventType, mouseEventButton);
                    updateLayerHierarchy();
                    return true;
                }

                if ([annotation isKindOfClass:getPDFAnnotationLinkClass()]) {
                    m_annotationTrackingState.startAnnotationTracking(WTFMove(annotation), mouseEventType, mouseEventButton);
                    return true;
                }

                return false;
            }

            beginTrackingSelection(*pageIndex, pointInPageSpace, selectionGranularityForMouseEvent(event), event.modifiers());

            return false;
        }
        default:
            return false;
        }
    case WebEventType::MouseUp:
        m_selectionTrackingData.selectionToExtendWith = nullptr;

        switch (auto mouseEventButton = event.button()) {
        case WebMouseEventButton::Left:
            if (RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation(); trackedAnnotation && ![trackedAnnotation isKindOfClass:getPDFAnnotationTextWidgetClass()]) {
                m_annotationTrackingState.finishAnnotationTracking(mouseEventType, mouseEventButton);

                if ([trackedAnnotation isKindOfClass:getPDFAnnotationLinkClass()])
                    didClickLinkAnnotation(trackedAnnotation.get());

                updateLayerHierarchy();
            }

            return false;
        default:
            return false;
        }
    default:
        return false;
    }
}

bool UnifiedPDFPlugin::handleMouseEnterEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleMouseLeaveEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleContextMenuEvent(const WebMouseEvent& event)
{
#if PLATFORM(MAC)
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return false;
    RefPtr frameView = m_frame->coreLocalFrame()->view();
    if (!frameView)
        return false;

    auto contextMenu = createContextMenu(frameView->contentsToScreen(IntRect(frameView->windowToContents(event.position()), IntSize())).location());

    webPage->sendWithAsyncReply(Messages::WebPageProxy::ShowPDFContextMenu { contextMenu, m_identifier }, [this, weakThis = WeakPtr { *this }](std::optional<int32_t>&& selectedItemTag) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (selectedItemTag)
            performContextMenuAction(toContextMenuItemTag(selectedItemTag.value()));
    });

    return true;
#else
    return false;
#endif // PLATFORM(MAC)
}

bool UnifiedPDFPlugin::handleKeyboardEvent(const WebKeyboardEvent&)
{
    return false;
}

void UnifiedPDFPlugin::didClickLinkAnnotation(const PDFAnnotation *annotation)
{
    // FIXME: Handle links with in-document destinations.

    if (NSURL *url = [annotation URL])
        navigateToURL(url);
}

#pragma mark Context Menu

#if PLATFORM(MAC)
UnifiedPDFPlugin::ContextMenuItemTag UnifiedPDFPlugin::contextMenuItemTagFromDisplayMode(const PDFDocumentLayout::DisplayMode& displayMode) const
{
    switch (displayMode) {
    case PDFDocumentLayout::DisplayMode::SinglePage: return ContextMenuItemTag::SinglePage;
    case PDFDocumentLayout::DisplayMode::Continuous: return ContextMenuItemTag::SinglePageContinuous;
    case PDFDocumentLayout::DisplayMode::TwoUp: return ContextMenuItemTag::TwoPages;
    case PDFDocumentLayout::DisplayMode::TwoUpContinuous: return ContextMenuItemTag::TwoPagesContinuous;
    }
}

PDFDocumentLayout::DisplayMode UnifiedPDFPlugin::displayModeFromContextMenuItemTag(const ContextMenuItemTag& tag) const
{
    switch (tag) {
    case ContextMenuItemTag::SinglePage: return PDFDocumentLayout::DisplayMode::SinglePage;
    case ContextMenuItemTag::SinglePageContinuous: return PDFDocumentLayout::DisplayMode::Continuous;
    case ContextMenuItemTag::TwoPages: return PDFDocumentLayout::DisplayMode::TwoUp;
    case ContextMenuItemTag::TwoPagesContinuous: return PDFDocumentLayout::DisplayMode::TwoUpContinuous;
    default:
        ASSERT_NOT_REACHED();
        return PDFDocumentLayout::DisplayMode::Continuous;
    }
}

auto UnifiedPDFPlugin::toContextMenuItemTag(int tagValue) const -> ContextMenuItemTag
{
    static constexpr std::array regularContextMenuItemTags {
        ContextMenuItemTag::OpenWithPreview,
        ContextMenuItemTag::SinglePage,
        ContextMenuItemTag::SinglePageContinuous,
        ContextMenuItemTag::TwoPages,
        ContextMenuItemTag::TwoPagesContinuous,
        ContextMenuItemTag::ZoomIn,
        ContextMenuItemTag::ZoomOut,
        ContextMenuItemTag::ActualSize,
    };
    const auto isKnownContextMenuItemTag = WTF::anyOf(regularContextMenuItemTags, [tagValue](ContextMenuItemTag tag) {
        return tagValue == enumToUnderlyingType(tag);
    });
    return isKnownContextMenuItemTag ? static_cast<ContextMenuItemTag>(tagValue) : ContextMenuItemTag::Unknown;
}

PDFContextMenu UnifiedPDFPlugin::createContextMenu(const IntPoint& contextMenuPoint) const
{
    Vector<PDFContextMenuItem> menuItems;

    auto addSeparator = [&] {
        menuItems.append({ String(), 0, enumToUnderlyingType(ContextMenuItemTag::Invalid), ContextMenuItemEnablement::Disabled, ContextMenuItemHasAction::No, ContextMenuItemIsSeparator::Yes });
    };

    menuItems.append({ WebCore::contextMenuItemPDFOpenWithPreview(), 0,
        enumToUnderlyingType(ContextMenuItemTag::OpenWithPreview),
        ContextMenuItemEnablement::Enabled,
        ContextMenuItemHasAction::Yes,
        ContextMenuItemIsSeparator::No
    });

    addSeparator();

    auto currentDisplayMode = contextMenuItemTagFromDisplayMode(m_documentLayout.displayMode());
    menuItems.append({ WebCore::contextMenuItemPDFSinglePage(), currentDisplayMode == ContextMenuItemTag::SinglePage, enumToUnderlyingType(ContextMenuItemTag::SinglePage), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });
    menuItems.append({ WebCore::contextMenuItemPDFSinglePageContinuous(), currentDisplayMode == ContextMenuItemTag::SinglePageContinuous, enumToUnderlyingType(ContextMenuItemTag::SinglePageContinuous), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });
    menuItems.append({ WebCore::contextMenuItemPDFTwoPages(), currentDisplayMode == ContextMenuItemTag::TwoPages, enumToUnderlyingType(ContextMenuItemTag::TwoPages), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });
    menuItems.append({ WebCore::contextMenuItemPDFTwoPagesContinuous(), currentDisplayMode == ContextMenuItemTag::TwoPagesContinuous, enumToUnderlyingType(ContextMenuItemTag::TwoPagesContinuous), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });

    addSeparator();

    menuItems.append({ WebCore::contextMenuItemPDFZoomIn(), 0, enumToUnderlyingType(ContextMenuItemTag::ZoomIn), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });
    menuItems.append({ WebCore::contextMenuItemPDFZoomOut(), 0, enumToUnderlyingType(ContextMenuItemTag::ZoomOut), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });
    menuItems.append({ WebCore::contextMenuItemPDFActualSize(), 0, enumToUnderlyingType(ContextMenuItemTag::ActualSize), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });

    return { contextMenuPoint, WTFMove(menuItems), { enumToUnderlyingType(ContextMenuItemTag::OpenWithPreview) } };
}

void UnifiedPDFPlugin::performContextMenuAction(ContextMenuItemTag tag)
{
    switch (tag) {
    // The OpenWithPreview action is handled in the UI Process.
    case ContextMenuItemTag::OpenWithPreview: return;
    case ContextMenuItemTag::SinglePage:
    case ContextMenuItemTag::SinglePageContinuous:
    case ContextMenuItemTag::TwoPagesContinuous:
    case ContextMenuItemTag::TwoPages:
        if (tag != contextMenuItemTagFromDisplayMode(m_documentLayout.displayMode())) {
            m_documentLayout.setDisplayMode(displayModeFromContextMenuItemTag(tag));
            updateLayout();
        }
        break;
    case ContextMenuItemTag::ZoomIn:
        zoomIn();
        break;
    case ContextMenuItemTag::ZoomOut:
        zoomOut();
        break;
    case ContextMenuItemTag::ActualSize:
        m_view->setPageScaleFactor(scaleForActualSize(), std::nullopt);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}
#endif // PLATFORM(MAC)

#pragma mark Editing Commands

bool UnifiedPDFPlugin::handleEditingCommand(const String& commandName, const String& argument)
{
    if (commandName == "ScrollPageBackward"_s || commandName == "ScrollPageForward"_s)
        return forwardEditingCommandToEditor(commandName, argument);

    if (commandName == "selectAll"_s) {
        selectAll();
        return true;
    }
    return false;
}

bool UnifiedPDFPlugin::isEditingCommandEnabled(const String& commandName)
{
    if (commandName == "ScrollPageBackward"_s || commandName == "ScrollPageForward"_s)
        return true;
    if (commandName == "selectAll"_s)
        return true;
    return false;
}

bool UnifiedPDFPlugin::forwardEditingCommandToEditor(const String& commandName, const String& argument) const
{
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;
    return m_frame->coreLocalFrame()->checkedEditor()->command(commandName).execute(argument);
}

void UnifiedPDFPlugin::selectAll()
{
    setCurrentSelection([m_pdfDocument selectionForEntireDocument]);
}

#pragma mark Selections

auto UnifiedPDFPlugin::selectionGranularityForMouseEvent(const WebMouseEvent& event) const -> SelectionGranularity
{
    if (event.clickCount() == 2)
        return SelectionGranularity::Word;
    if (event.clickCount() == 3)
        return SelectionGranularity::Line;
    return SelectionGranularity::Character;
}

void UnifiedPDFPlugin::extendCurrentSelectionIfNeeded()
{
    if (!m_currentSelection)
        return;
    PDFPage *firstPageOfCurrentSelection = [[m_currentSelection pages] firstObject];

    auto oldStartPageIndex = std::exchange(m_selectionTrackingData.startPageIndex, [m_pdfDocument indexForPage:firstPageOfCurrentSelection]);
    auto oldStartPagePoint = std::exchange(m_selectionTrackingData.startPagePoint, IntPoint { [m_currentSelection firstCharCenter] });
    m_selectionTrackingData.selectionToExtendWith = WTFMove(m_currentSelection);

    RetainPtr<PDFSelection> selection = [m_pdfDocument selectionFromPage:firstPageOfCurrentSelection atPoint:m_selectionTrackingData.startPagePoint toPage:m_documentLayout.pageAtIndex(oldStartPageIndex).get() atPoint:oldStartPagePoint];
    [selection addSelection:m_selectionTrackingData.selectionToExtendWith.get()];
    setCurrentSelection(WTFMove(selection));
}

void UnifiedPDFPlugin::beginTrackingSelection(PDFDocumentLayout::PageIndex pageIndex, const WebCore::IntPoint& pagePoint, SelectionGranularity selectionGranularity, OptionSet<WebEventModifier> mouseEventModifiers)
{
    m_selectionTrackingData.granularity = selectionGranularity;
    m_selectionTrackingData.startPageIndex = pageIndex;
    m_selectionTrackingData.startPagePoint = pagePoint;
    m_selectionTrackingData.marqueeSelectionRect = { };
    m_selectionTrackingData.shouldMakeMarqueeSelection = mouseEventModifiers.contains(WebEventModifier::AltKey);
    m_selectionTrackingData.shouldExtendCurrentSelection = mouseEventModifiers.contains(WebEventModifier::ShiftKey);

    if (m_selectionTrackingData.shouldExtendCurrentSelection)
        extendCurrentSelectionIfNeeded();

    continueTrackingSelection(pageIndex, pagePoint);
}

static IntRect computeMarqueeSelectionRect(const WebCore::IntPoint& point1, const WebCore::IntPoint& point2)
{
    auto marqueeRectLocation { point1.shrunkTo(point2) };
    IntSize marqueeRectSize { point1 - point2 };
    return { marqueeRectLocation.x(), marqueeRectLocation.y(), std::abs(marqueeRectSize.width()), std::abs(marqueeRectSize.height()) };
}

void UnifiedPDFPlugin::continueTrackingSelection(PDFDocumentLayout::PageIndex pageIndex, const WebCore::IntPoint& pagePoint)
{
    if (m_selectionTrackingData.shouldMakeMarqueeSelection) {
        if (m_selectionTrackingData.startPageIndex != pageIndex)
            return;

        m_selectionTrackingData.marqueeSelectionRect = computeMarqueeSelectionRect(pagePoint, m_selectionTrackingData.startPagePoint);
        auto page = m_documentLayout.pageAtIndex(pageIndex);
        return setCurrentSelection([page selectionForRect:m_selectionTrackingData.marqueeSelectionRect]);
    }

    switch (m_selectionTrackingData.granularity) {
    case SelectionGranularity::Character: {
        auto fromPage = m_documentLayout.pageAtIndex(m_selectionTrackingData.startPageIndex);
        auto toPage = m_documentLayout.pageAtIndex(pageIndex);
        RetainPtr<PDFSelection> selection = [m_pdfDocument selectionFromPage:fromPage.get() atPoint:m_selectionTrackingData.startPagePoint toPage:toPage.get() atPoint:pagePoint];
        if (m_selectionTrackingData.shouldExtendCurrentSelection)
            [selection addSelection:m_selectionTrackingData.selectionToExtendWith.get()];
        setCurrentSelection(WTFMove(selection));
        break;
    }
    // FIXME: <https://webkit.org/b/268616> Selection tracking should be able to reason at word/line granularity.
    case SelectionGranularity::Word:
    case SelectionGranularity::Line:
        notImplemented();
    }
}

void UnifiedPDFPlugin::setCurrentSelection(RetainPtr<PDFSelection>&& selection)
{
    m_currentSelection = WTFMove(selection);
    // FIXME: <https://webkit.org/b/268927> Selection painting requests should be optimized by specifying dirty rects.
    m_contentsLayer->setNeedsDisplay();
    notifySelectionChanged();
}

String UnifiedPDFPlugin::getSelectionString() const
{
    return m_currentSelection.get().string;
}

bool UnifiedPDFPlugin::existingSelectionContainsPoint(const FloatPoint&) const
{
    return false;
}

FloatRect UnifiedPDFPlugin::rectForSelectionInRootView(PDFSelection *) const
{
    return { };
}

#pragma mark -

unsigned UnifiedPDFPlugin::countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount)
{
    return 0;
}

bool UnifiedPDFPlugin::findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount)
{
    return false;
}

bool UnifiedPDFPlugin::performDictionaryLookupAtLocation(const FloatPoint&)
{
    return false;
}

std::tuple<String, PDFSelection *, NSDictionary *> UnifiedPDFPlugin::lookupTextAtLocation(const FloatPoint&, WebHitTestResultData&) const
{
    return { };
}

id UnifiedPDFPlugin::accessibilityHitTest(const IntPoint&) const
{
    return nil;
}

id UnifiedPDFPlugin::accessibilityObject() const
{
    return nil;
}

id UnifiedPDFPlugin::accessibilityAssociatedPluginParentForElement(Element*) const
{
    return nil;
}

#if ENABLE(PDF_HUD)

void UnifiedPDFPlugin::zoomIn()
{
    m_view->setPageScaleFactor(std::clamp(m_scaleFactor * zoomIncrement, minimumZoomScale, maximumZoomScale), std::nullopt);
}

void UnifiedPDFPlugin::zoomOut()
{
    m_view->setPageScaleFactor(std::clamp(m_scaleFactor / zoomIncrement, minimumZoomScale, maximumZoomScale), std::nullopt);
}

#endif // ENABLE(PDF_HUD)

CGRect UnifiedPDFPlugin::pluginBoundsForAnnotation(RetainPtr<PDFAnnotation>& annotation) const
{
    auto pageSpaceBounds = IntRect([annotation bounds]);
    if (auto pageIndex = m_documentLayout.indexForPage([annotation page])) {
        auto documentSpacePoint = convertFromPageToDocument({ pageSpaceBounds.x(), pageSpaceBounds.y() }, pageIndex.value());

        // The origin of an annotation in page space and document space are opposite
        // in the Y axis so we need to perform a flip
        documentSpacePoint.setY(documentSpacePoint.y() - pageSpaceBounds.height());

        pageSpaceBounds.scale(m_documentLayout.scale() * m_scaleFactor);
        return { convertFromDocumentToPlugin(documentSpacePoint), pageSpaceBounds.size() };
    }
    ASSERT_NOT_REACHED();
    return pageSpaceBounds;
}


void UnifiedPDFPlugin::focusNextAnnotation()
{
}

void UnifiedPDFPlugin::focusPreviousAnnotation()
{
}

void UnifiedPDFPlugin::setActiveAnnotation(RetainPtr<PDFAnnotation>&& annotation)
{
#if PLATFORM(MAC)
    if (!supportsForms())
        return;

    if (m_activeAnnotation)
        m_activeAnnotation->commit();

    if (annotation) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] && static_cast<PDFAnnotationTextWidget *>(annotation).isReadOnly) {
            m_activeAnnotation = nullptr;
            return;
        }
ALLOW_DEPRECATED_DECLARATIONS_END

        auto activeAnnotation = PDFPluginAnnotation::create(annotation.get(), this);
        m_activeAnnotation = activeAnnotation.get();
        activeAnnotation->attach(m_annotationContainer.get());
    } else
        m_activeAnnotation = nullptr;
    updateLayerHierarchy();
#endif
}

void AnnotationTrackingState::startAnnotationTracking(RetainPtr<PDFAnnotation>&& annotation, const WebEventType& mouseEventType, const WebMouseEventButton& mouseEventButton)
{
    ASSERT(!m_trackedAnnotation);
    m_trackedAnnotation = WTFMove(annotation);

    if ([m_trackedAnnotation isKindOfClass:getPDFAnnotationButtonWidgetClass()])
        [m_trackedAnnotation setHighlighted:YES];
    if (mouseEventType == WebEventType::MouseMove && mouseEventButton == WebMouseEventButton::None)
        m_isBeingHovered = true;
}

void AnnotationTrackingState::finishAnnotationTracking(const WebEventType& mouseEventType, const WebMouseEventButton& mouseEventButton)
{
    ASSERT(m_trackedAnnotation);
    if (mouseEventType == WebEventType::MouseUp && mouseEventButton == WebMouseEventButton::Left) {
        if ([m_trackedAnnotation isHighlighted])
            [m_trackedAnnotation setHighlighted:NO];

        if ([m_trackedAnnotation isKindOfClass:getPDFAnnotationButtonWidgetClass()] && [m_trackedAnnotation widgetControlType] != kPDFWidgetPushButtonControl) {
            auto currentButtonState = [m_trackedAnnotation buttonWidgetState];
            if (currentButtonState == PDFWidgetCellState::kPDFWidgetOnState && [m_trackedAnnotation allowsToggleToOff])
                [m_trackedAnnotation setButtonWidgetState:PDFWidgetCellState::kPDFWidgetOffState];
            else if (currentButtonState == PDFWidgetCellState::kPDFWidgetOffState)
                [m_trackedAnnotation setButtonWidgetState:PDFWidgetCellState::kPDFWidgetOnState];
        }
    } else if (mouseEventType == WebEventType::MouseMove && mouseEventButton == WebMouseEventButton::Left)
        handleMouseDraggedOffTrackedAnnotation();
    resetAnnotationTrackingState();
}

void AnnotationTrackingState::handleMouseDraggedOffTrackedAnnotation()
{
    ASSERT(m_trackedAnnotation);

    [m_trackedAnnotation setHighlighted:NO];
}

bool AnnotationTrackingState::isBeingHovered() const
{
    ASSERT(m_trackedAnnotation);
    return m_isBeingHovered;
}

void AnnotationTrackingState::resetAnnotationTrackingState()
{
    ASSERT(m_trackedAnnotation);
    m_trackedAnnotation = nullptr;
    m_isBeingHovered = false;
}

void UnifiedPDFPlugin::attemptToUnlockPDF(const String& password)
{
}

bool UnifiedPDFPlugin::isTaggedPDF() const
{
    return CGPDFDocumentIsTaggedPDF([m_pdfDocument documentRef]);
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
