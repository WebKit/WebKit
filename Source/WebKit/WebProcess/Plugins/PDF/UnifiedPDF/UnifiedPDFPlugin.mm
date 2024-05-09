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

#include "AsyncPDFRenderer.h"
#include "DataDetectionResult.h"
#include "FindController.h"
#include "MessageSenderInlines.h"
#include "PDFContextMenu.h"
#include "PDFDataDetectorOverlayController.h"
#include "PDFKitSPI.h"
#include "PDFPageCoverage.h"
#include "PDFPluginAnnotation.h"
#include "PDFPluginPasswordField.h"
#include "PDFPluginPasswordForm.h"
#include "PasteboardTypes.h"
#include "PluginView.h"
#include "WKAccessibilityPDFDocumentObject.h"
#include "WebEventConversion.h"
#include "WebEventModifier.h"
#include "WebEventType.h"
#include "WebHitTestResultData.h"
#include "WebKeyboardEvent.h"
#include "WebMouseEvent.h"
#include "WebPageProxyMessages.h"
#include <CoreGraphics/CoreGraphics.h>
#include <PDFKit/PDFKit.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/AutoscrollController.h>
#include <WebCore/BitmapImage.h>
#include <WebCore/Chrome.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/ColorBlending.h>
#include <WebCore/ColorCocoa.h>
#include <WebCore/DataDetectorElementInfo.h>
#include <WebCore/DictionaryLookup.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/Editor.h>
#include <WebCore/EditorClient.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/ScreenProperties.h>
#include <WebCore/ScrollAnimator.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollbarTheme.h>
#include <WebCore/ScrollbarsController.h>
#include <WebCore/VoidCallback.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/Algorithms.h>
#include <wtf/Scope.h>
#include <wtf/text/StringToIntegerConversion.h>

#include "PDFKitSoftLink.h"

@interface WKPDFFormMutationObserver : NSObject {
    WebKit::UnifiedPDFPlugin* _plugin;
}
@end

@implementation WKPDFFormMutationObserver

- (id)initWithPlugin:(WebKit::UnifiedPDFPlugin *)plugin
{
    if (!(self = [super init]))
        return nil;
    _plugin = plugin;
    return self;
}

- (void)formChanged:(NSNotification *)notification
{
    _plugin->didMutatePDFDocument();

    NSString *fieldName = (NSString *)[[notification userInfo] objectForKey:@"PDFFormFieldName"];
    _plugin->repaintAnnotationsForFormField(fieldName);
}
@end

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIColor.h>
#endif

// FIXME: We should rationalize these with the values in ViewGestureController.
// For now, we'll leave them differing as they do in PDFPlugin.
static constexpr double minimumZoomScale = 0.2;
static constexpr double maximumZoomScale = 6.0;
static constexpr double zoomIncrement = 1.18920;

namespace WebKit {
using namespace WebCore;

Ref<UnifiedPDFPlugin> UnifiedPDFPlugin::create(HTMLPlugInElement& pluginElement)
{
    return adoptRef(*new UnifiedPDFPlugin(pluginElement));
}

UnifiedPDFPlugin::UnifiedPDFPlugin(HTMLPlugInElement& element)
    : PDFPluginBase(element)
    , m_pdfMutationObserver(adoptNS([[WKPDFFormMutationObserver alloc] initWithPlugin:this]))
#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    , m_dataDetectorOverlayController { WTF::makeUnique<PDFDataDetectorOverlayController>(*this) }
#endif
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

#if PLATFORM(MAC)
    m_accessibilityDocumentObject = adoptNS([[WKAccessibilityPDFDocumentObject alloc] initWithPDFDocument:m_pdfDocument andElement:&element]);
    [m_accessibilityDocumentObject setPDFPlugin:this];
    if (this->isFullFramePlugin() && m_frame && m_frame->page() && m_frame->isMainFrame())
        [m_accessibilityDocumentObject setParent:dynamic_objc_cast<NSObject>(m_frame->protectedPage()->accessibilityRemoteObject())];
#endif
}

UnifiedPDFPlugin::~UnifiedPDFPlugin() = default;

static String mutationObserverNotificationString()
{
    static NeverDestroyed<String> notificationString = "PDFFormDidChangeValue"_s;
    return notificationString;
}

void UnifiedPDFPlugin::teardown()
{
    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->teardown();

    PDFPluginBase::teardown();

    GraphicsLayer::unparentAndClear(m_rootLayer);
    GraphicsLayer::unparentAndClear(m_contentsLayer);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    GraphicsLayer::unparentAndClear(m_selectionLayer);
#endif

    RefPtr page = this->page();
    if (m_scrollingNodeID && page) {
        RefPtr scrollingCoordinator = page->scrollingCoordinator();
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_scrollingNodeID);
        m_frame->coreLocalFrame()->protectedView()->removePluginScrollableAreaForScrollingNodeID(m_scrollingNodeID);
    }

    [[NSNotificationCenter defaultCenter] removeObserver:m_pdfMutationObserver.get() name:mutationObserverNotificationString() object:m_pdfDocument.get()];
    m_pdfMutationObserver = nullptr;

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    std::exchange(m_dataDetectorOverlayController, nullptr)->teardown();
#endif
}

GraphicsLayer* UnifiedPDFPlugin::graphicsLayer() const
{
    return m_rootLayer.get();
}

Ref<AsyncPDFRenderer> UnifiedPDFPlugin::asyncRenderer()
{
    if (m_asyncRenderer)
        return *m_asyncRenderer;

    m_asyncRenderer = AsyncPDFRenderer::create(*this);
    return *m_asyncRenderer;
}

RefPtr<AsyncPDFRenderer> UnifiedPDFPlugin::asyncRendererIfExists() const
{
    return m_asyncRenderer;
}

FrameView* UnifiedPDFPlugin::mainFrameView() const
{
    if (!m_frame)
        return nullptr;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return nullptr;

    return webPage->mainFrameView();
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

#if HAVE(INCREMENTAL_PDF_APIS)
    maybeClearHighLatencyDataProviderFlag();
#endif

    updateLayout(AdjustScaleAfterLayout::Yes);

#if ENABLE(PDF_HUD)
    updateHUDVisibility();
#endif

#if PLATFORM(MAC)
    if (isLocked())
        createPasswordEntryForm();
#endif

    if (m_view)
        m_view->layerHostingStrategyDidChange();

    [[NSNotificationCenter defaultCenter] addObserver:m_pdfMutationObserver.get() selector:@selector(formChanged:) name:mutationObserverNotificationString() object:m_pdfDocument.get()];

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    enableDataDetection();
#endif

    scrollToFragmentIfNeeded();

    if (m_pdfTestCallback) {
        m_pdfTestCallback->handleEvent();
        m_pdfTestCallback = nullptr;
    }

}

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)

void UnifiedPDFPlugin::enableDataDetection()
{
#if HAVE(PDFDOCUMENT_ENABLE_DATA_DETECTORS)
    if ([m_pdfDocument respondsToSelector:@selector(setEnableDataDetectors:)])
        [m_pdfDocument setEnableDataDetectors:YES];
#endif
}

void UnifiedPDFPlugin::handleClickForDataDetectionResult(const DataDetectorElementInfo& dataDetectorElementInfo, const IntPoint& clickPointInPluginSpace)
{
    RefPtr page = this->page();
    if (!page)
        return;

    page->chrome().client().handleClickForDataDetectionResult(dataDetectorElementInfo, clickPointInPluginSpace);
}

void UnifiedPDFPlugin::didInvalidateDataDetectorHighlightOverlayRects()
{
    auto lastKnownMousePositionInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, lastKnownMousePositionInView());
    auto pageIndex = pageIndexForDocumentPoint(lastKnownMousePositionInDocumentSpace);
    dataDetectorOverlayController().didInvalidateHighlightOverlayRects(pageIndex);
}

#endif

#if PLATFORM(MAC)

void UnifiedPDFPlugin::createPasswordEntryForm()
{
    if (!supportsForms())
        return;

    auto passwordForm = PDFPluginPasswordForm::create(this);
    m_passwordForm = passwordForm.ptr();
    passwordForm->attach(m_annotationContainer.get());

    auto passwordField = PDFPluginPasswordField::create(this);
    m_passwordField = passwordField.ptr();
    passwordField->attach(m_annotationContainer.get());
}

#endif

void UnifiedPDFPlugin::attemptToUnlockPDF(const String& password)
{
    if (![m_pdfDocument unlockWithPassword:password]) {
#if PLATFORM(MAC)
        m_passwordField->resetField();
        m_passwordForm->unlockFailed();
#endif
        return;
    }

#if PLATFORM(MAC)
    m_passwordForm = nullptr;
    m_passwordField = nullptr;
#endif

    updateLayout(AdjustScaleAfterLayout::Yes);

#if ENABLE(PDF_HUD)
    updateHUDVisibility();
    updateHUDLocation();
#endif

    scrollToFragmentIfNeeded();
}

RefPtr<GraphicsLayer> UnifiedPDFPlugin::createGraphicsLayer(const String& name, GraphicsLayer::Type layerType)
{
    if (RefPtr graphicsLayer = createGraphicsLayer(*this, layerType)) {
        graphicsLayer->setName(name);
        return graphicsLayer;
    }

    return nullptr;
}

RefPtr<GraphicsLayer> UnifiedPDFPlugin::createGraphicsLayer(GraphicsLayerClient& client)
{
    return createGraphicsLayer(client, GraphicsLayer::Type::Normal);
}

RefPtr<GraphicsLayer> UnifiedPDFPlugin::createGraphicsLayer(GraphicsLayerClient& client, GraphicsLayer::Type layerType)
{
    RefPtr page = this->page();
    if (!page)
        return nullptr;

    auto* graphicsLayerFactory = page->chrome().client().graphicsLayerFactory();
    Ref graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, client, layerType);
    return graphicsLayer;
}

void UnifiedPDFPlugin::setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement> repaintRequirements, const FloatRect& rectInDocumentCoordinates)
{
    if (!repaintRequirements)
        return;

    auto contentsRect = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::Contents, rectInDocumentCoordinates);
    if (repaintRequirements.contains(RepaintRequirement::PDFContent)) {
        if (RefPtr asyncRenderer = asyncRendererIfExists())
            asyncRenderer->pdfContentChangedInRect(m_scaleFactor, contentsRect);
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (repaintRequirements.contains(RepaintRequirement::Selection) && canPaintSelectionIntoOwnedLayer()) {
        RefPtr { m_selectionLayer }->setNeedsDisplayInRect(contentsRect);
        if (repaintRequirements.hasExactlyOneBitSet())
            return;
    }
#endif

    RefPtr { m_contentsLayer }->setNeedsDisplayInRect(contentsRect);
}

void UnifiedPDFPlugin::setNeedsRepaintInDocumentRects(OptionSet<RepaintRequirement> repaintRequirements, const Vector<FloatRect>& rectsInDocumentCoordinates)
{
    for (auto& rectInDocumentCoordinates : rectsInDocumentCoordinates)
        setNeedsRepaintInDocumentRect(repaintRequirements, rectInDocumentCoordinates);
}

void UnifiedPDFPlugin::scheduleRenderingUpdate(OptionSet<RenderingUpdateStep> requestedSteps)
{
    RefPtr page = this->page();
    if (!page)
        return;

    page->scheduleRenderingUpdate(requestedSteps);
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

    if (!m_pageBackgroundsContainerLayer) {
        m_pageBackgroundsContainerLayer = createGraphicsLayer("Page backgrounds"_s, GraphicsLayer::Type::Normal);
        m_pageBackgroundsContainerLayer->setAnchorPoint({ });
        m_scrolledContentsLayer->addChild(*m_pageBackgroundsContainerLayer);
    }

    if (!m_contentsLayer) {
        m_contentsLayer = createGraphicsLayer("PDF contents"_s, isFullMainFramePlugin() ? GraphicsLayer::Type::PageTiledBacking : GraphicsLayer::Type::TiledBacking);
        m_contentsLayer->setAnchorPoint({ });
        m_contentsLayer->setDrawsContent(true);
        m_contentsLayer->setAcceleratesDrawing(canPaintSelectionIntoOwnedLayer());
        m_scrolledContentsLayer->addChild(*m_contentsLayer);

        // This is the call that enables async rendering.
        asyncRenderer()->setupWithLayer(*m_contentsLayer);
    }

    if (!m_overflowControlsContainer) {
        m_overflowControlsContainer = createGraphicsLayer("Overflow controls container"_s, GraphicsLayer::Type::Normal);
        m_overflowControlsContainer->setAnchorPoint({ });
        m_rootLayer->addChild(*m_overflowControlsContainer);
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (!m_selectionLayer) {
        m_selectionLayer = createGraphicsLayer("PDF selections"_s, GraphicsLayer::Type::TiledBacking);
        m_selectionLayer->setAnchorPoint({ });
        m_selectionLayer->setDrawsContent(true);
        m_selectionLayer->setAcceleratesDrawing(true);
        m_selectionLayer->setBlendMode(BlendMode::Multiply);
        m_scrolledContentsLayer->addChild(*m_selectionLayer);
    }
#endif
}

void UnifiedPDFPlugin::updatePageBackgroundLayers()
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (isLocked())
        return;

    Vector<Ref<GraphicsLayer>> pageContainerLayers = m_pageBackgroundsContainerLayer->children();

    // pageContentsLayers are always the size of `layoutBoundsForPageAtIndex`; we generate a page preview
    // buffer of the same size. On zooming, this layer just gets scaled, to avoid repainting.

    for (PDFDocumentLayout::PageIndex i = 0; i < m_documentLayout.pageCount(); ++i) {
        auto pageBoundsRect = m_documentLayout.layoutBoundsForPageAtIndex(i);
        auto destinationRect = pageBoundsRect;
        destinationRect.scale(m_documentLayout.scale());

        auto addLayerShadow = [](GraphicsLayer& layer, IntPoint shadowOffset, const Color& shadowColor, int shadowStdDeviation) {
            Vector<RefPtr<FilterOperation>> filterOperations;
            filterOperations.append(DropShadowFilterOperation::create(shadowOffset, shadowStdDeviation, shadowColor));

            FilterOperations filters;
            filters.setOperations(WTFMove(filterOperations));
            layer.setFilters(filters);
        };

        const auto containerShadowOffset = IntPoint { 0, 1 };
        constexpr auto containerShadowColor = SRGBA<uint8_t> { 0, 0, 0, 46 };
        constexpr int containerShadowStdDeviation = 2;

        const auto shadowOffset = IntPoint { 0, 2 };
        constexpr auto shadowColor = SRGBA<uint8_t> { 0, 0, 0, 38 };
        constexpr int shadowStdDeviation = 6;

        auto pageContainerLayer = [&](PDFDocumentLayout::PageIndex pageIndex) {
            if (pageIndex < pageContainerLayers.size())
                return pageContainerLayers[pageIndex];

            auto pageContainerLayer = createGraphicsLayer(makeString("Page container "_s, pageIndex), GraphicsLayer::Type::Normal);
            auto pageBackgroundLayer = createGraphicsLayer(makeString("Page background "_s, pageIndex), GraphicsLayer::Type::Normal);
            // Can only be null if this->page() is null, which we checked above.
            ASSERT(pageContainerLayer);
            ASSERT(pageBackgroundLayer);

            pageContainerLayer->addChild(*pageBackgroundLayer);

            pageContainerLayer->setAnchorPoint({ });
            addLayerShadow(*pageContainerLayer, containerShadowOffset, containerShadowColor, containerShadowStdDeviation);

            pageBackgroundLayer->setAnchorPoint({ });
            pageBackgroundLayer->setBackgroundColor(Color::white);
            pageBackgroundLayer->setDrawsContent(true);
            pageBackgroundLayer->setAcceleratesDrawing(true);
            pageBackgroundLayer->setShouldUpdateRootRelativeScaleFactor(false);
            pageBackgroundLayer->setNeedsDisplay(); // We only need to paint this layer once.

            // Sure would be nice if we could just stuff data onto a GraphicsLayer.
            m_pageBackgroundLayers.add(pageBackgroundLayer, pageIndex);

            // FIXME: Need to add a 1px black border with alpha 0.0586.

            addLayerShadow(*pageBackgroundLayer, shadowOffset, shadowColor, shadowStdDeviation);

            auto containerLayer = pageContainerLayer.releaseNonNull();
            pageContainerLayers.append(WTFMove(containerLayer));

            return pageContainerLayers[pageIndex];
        }(i);

        pageContainerLayer->setPosition(destinationRect.location());
        pageContainerLayer->setSize(destinationRect.size());
        pageContainerLayer->setOpacity(shouldDisplayPage(i) ? 1 : 0);

        auto pageBackgroundLayer = pageContainerLayer->children()[0];
        pageBackgroundLayer->setSize(pageBoundsRect.size());

        TransformationMatrix documentScaleTransform;
        documentScaleTransform.scale(m_documentLayout.scale());
        pageBackgroundLayer->setTransform(documentScaleTransform);
    }

    m_pageBackgroundsContainerLayer->setChildren(WTFMove(pageContainerLayers));
}

void UnifiedPDFPlugin::paintBackgroundLayerForPage(const GraphicsLayer*, GraphicsContext& context, const FloatRect& clipRect, PDFDocumentLayout::PageIndex pageIndex)
{
    auto destinationRect = m_documentLayout.layoutBoundsForPageAtIndex(pageIndex);
    destinationRect.setLocation({ });

    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->paintPagePreview(context, clipRect, destinationRect, pageIndex);
}

float UnifiedPDFPlugin::scaleForPagePreviews() const
{
    // The scale for page previews is a half of the normal tile resolution at 1x page scale.
    // pageCoverage.pdfDocumentScale is here because page previews draw into a buffer sized using layoutBoundsForPageAtIndex().
    static constexpr float pagePreviewScale = 0.5;
    return deviceScaleFactor() * m_documentLayout.scale() * pagePreviewScale;
}

void UnifiedPDFPlugin::didGeneratePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    if (RefPtr layer = backgroundLayerForPage(pageIndex))
        layer->setNeedsDisplay();
}

GraphicsLayer* UnifiedPDFPlugin::backgroundLayerForPage(PDFDocumentLayout::PageIndex pageIndex) const
{
    if (!m_pageBackgroundsContainerLayer)
        return nullptr;

    auto pageContainerLayers = m_pageBackgroundsContainerLayer->children();
    if (pageContainerLayers.size() <= pageIndex)
        return nullptr;

    Ref pageContainerLayer = pageContainerLayers[pageIndex];
    if (!pageContainerLayer->children().size())
        return nullptr;

    return pageContainerLayer->children()[0].ptr();
}

std::optional<PDFDocumentLayout::PageIndex> UnifiedPDFPlugin::pageIndexForPageBackgroundLayer(const GraphicsLayer* layer) const
{
    auto it = m_pageBackgroundLayers.find(layer);
    if (it == m_pageBackgroundLayers.end())
        return { };

    return it->value;
}

void UnifiedPDFPlugin::willAttachScrollingNode()
{
    createScrollingNodeIfNecessary();
}

void UnifiedPDFPlugin::didAttachScrollingNode()
{
    m_didAttachScrollingTreeNode = true;
    scrollToFragmentIfNeeded();
}

void UnifiedPDFPlugin::didSameDocumentNavigationForFrame(WebFrame& frame)
{
    if (&frame != m_frame)
        return;
    m_didScrollToFragment = false;
    scrollToFragmentIfNeeded();
}

ScrollingNodeID UnifiedPDFPlugin::scrollingNodeID() const
{
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
    scrollingCoordinator->createNode(m_frame->coreLocalFrame()->rootFrame().frameID(), ScrollingNodeType::PluginScrolling, m_scrollingNodeID);

#if ENABLE(SCROLLING_THREAD)
    m_scrollContainerLayer->setScrollingNodeID(m_scrollingNodeID);

    if (auto* layer = layerForHorizontalScrollbar())
        layer->setScrollingNodeID(m_scrollingNodeID);

    if (auto* layer = layerForVerticalScrollbar())
        layer->setScrollingNodeID(m_scrollingNodeID);

    if (auto* layer = layerForScrollCorner())
        layer->setScrollingNodeID(m_scrollingNodeID);
#endif

    m_frame->coreLocalFrame()->protectedView()->setPluginScrollableAreaForScrollingNodeID(m_scrollingNodeID, *this);

    scrollingCoordinator->setScrollingNodeScrollableAreaGeometry(m_scrollingNodeID, *this);

    WebCore::ScrollingCoordinator::NodeLayers nodeLayers;
    nodeLayers.layer = m_rootLayer.get();
    nodeLayers.scrollContainerLayer = m_scrollContainerLayer.get();
    nodeLayers.scrolledContentsLayer = m_scrolledContentsLayer.get();
    nodeLayers.horizontalScrollbarLayer = layerForHorizontalScrollbar();
    nodeLayers.verticalScrollbarLayer = layerForVerticalScrollbar();

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

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    m_selectionLayer->setSize(documentSize());
    m_selectionLayer->setNeedsDisplay();
#endif

    updatePageBackgroundLayers();
    updateSnapOffsets();

    didChangeSettings();
    didChangeIsInWindow();
}

void UnifiedPDFPlugin::updateLayerPositions()
{
    TransformationMatrix transform;
    transform.scale(m_scaleFactor);
    auto padding = centeringOffset();
    transform.translate(padding.width(), padding.height());

    m_contentsLayer->setTransform(transform);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    m_selectionLayer->setTransform(transform);
#endif
    m_pageBackgroundsContainerLayer->setTransform(transform);
}

bool UnifiedPDFPlugin::shouldShowDebugIndicators() const
{
    RefPtr page = this->page();
    return page && page->settings().showDebugBorders();
}

void UnifiedPDFPlugin::didChangeSettings()
{
    RefPtr page = this->page();
    if (!page)
        return;

    auto showDebugBorders = shouldShowDebugIndicators();
    auto showRepaintCounter = page->settings().showRepaintCounter();

    auto propagateSettingsToLayer = [&] (GraphicsLayer& layer) {
        layer.setShowDebugBorder(showDebugBorders);
        layer.setShowRepaintCounter(showRepaintCounter);
    };
    propagateSettingsToLayer(*m_rootLayer);
    propagateSettingsToLayer(*m_scrollContainerLayer);
    propagateSettingsToLayer(*m_scrolledContentsLayer);
    propagateSettingsToLayer(*m_pageBackgroundsContainerLayer);
    propagateSettingsToLayer(*m_contentsLayer);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    propagateSettingsToLayer(*m_selectionLayer);
#endif

    for (auto& pageLayer : m_pageBackgroundsContainerLayer->children()) {
        propagateSettingsToLayer(pageLayer);
        if (pageLayer->children().size())
            propagateSettingsToLayer(pageLayer->children()[0]);
    }

    if (m_layerForHorizontalScrollbar)
        propagateSettingsToLayer(*m_layerForHorizontalScrollbar);

    if (m_layerForVerticalScrollbar)
        propagateSettingsToLayer(*m_layerForVerticalScrollbar);

    if (m_layerForScrollCorner)
        propagateSettingsToLayer(*m_layerForScrollCorner);

    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->setShowDebugBorders(showDebugBorders);
}

void UnifiedPDFPlugin::notifyFlushRequired(const GraphicsLayer*)
{
    scheduleRenderingUpdate();
}

std::optional<float> UnifiedPDFPlugin::customContentsScale(const GraphicsLayer* layer) const
{
    if (pageIndexForPageBackgroundLayer(layer))
        return scaleForPagePreviews();

    return { };
}

bool UnifiedPDFPlugin::layerNeedsPlatformContext(const WebCore::GraphicsLayer* layer) const
{
    // We need a platform context if the plugin can not paint selections into its own layer,
    // since we would then have to vend a platform context that PDFKit can paint into.
    // However, this constraint only applies for the contents layer. No other layer needs to be WP-backed.
    return layer == m_contentsLayer.get() && !canPaintSelectionIntoOwnedLayer();
}

void UnifiedPDFPlugin::tiledBackingUsageChanged(const GraphicsLayer* layer, bool usingTiledBacking)
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (usingTiledBacking)
        layer->tiledBacking()->setIsInWindow(page->isInWindow());
}

void UnifiedPDFPlugin::didChangeIsInWindow()
{
    RefPtr page = this->page();
    if (!page || !m_contentsLayer)
        return;

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (!m_selectionLayer)
        return;
#endif

    bool isInWindow = page->isInWindow();
    m_contentsLayer->setIsInWindow(isInWindow);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    m_selectionLayer->setIsInWindow(isInWindow);
#endif

    for (auto& pageLayer : m_pageBackgroundsContainerLayer->children()) {
        if (pageLayer->children().size()) {
            Ref pageContentsLayer = pageLayer->children()[0];
            pageContentsLayer->setIsInWindow(isInWindow);
        }
    }
}

void UnifiedPDFPlugin::windowActivityDidChange()
{
    repaintOnSelectionActiveStateChangeIfNeeded(ActiveStateChangeReason::WindowActivityChanged);
}

void UnifiedPDFPlugin::paint(GraphicsContext& context, const IntRect&)
{
    // Only called for snapshotting.
    if (size().isEmpty())
        return;

    context.translate(-m_scrollOffset.width(), -m_scrollOffset.height());

    FloatRect clipRect { FloatPoint(m_scrollOffset), size() };

    context.clip(clipRect);
    context.fillRect(clipRect, WebCore::roundAndClampToSRGBALossy([WebCore::CocoaColor grayColor].CGColor));
    context.scale(m_scaleFactor);

    auto paddingForCentering = centeringOffset();
    context.translate(paddingForCentering.width(), paddingForCentering.height());

    clipRect.scale(1.0f / m_scaleFactor);

    paintPDFContent(context, clipRect);
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

    if (layer == m_contentsLayer.get()) {
        paintPDFContent(context, clipRect, PaintingBehavior::All, AllowsAsyncRendering::Yes);
        return;
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (layer == m_selectionLayer.get())
        return paintPDFSelection(context, clipRect);
#endif

    if (auto backgroundLayerPageIndex = pageIndexForPageBackgroundLayer(layer)) {
        paintBackgroundLayerForPage(layer, context, clipRect, *backgroundLayerPageIndex);
        return;
    }

}

PDFPageCoverage UnifiedPDFPlugin::pageCoverageForRect(const FloatRect& clipRect) const
{
    if (m_size.isEmpty() || documentSize().isEmpty())
        return { };

    auto tilingScaleFactor = 1.0f;
    if (auto* tiledBacking = m_contentsLayer->tiledBacking())
        tilingScaleFactor = tiledBacking->tilingScaleFactor();

    auto documentLayoutScale = m_documentLayout.scale();

    auto pageCoverage = PDFPageCoverage { };
    pageCoverage.deviceScaleFactor = deviceScaleFactor();
    pageCoverage.pdfDocumentScale = documentLayoutScale;
    pageCoverage.tilingScaleFactor = tilingScaleFactor;

    auto drawingRect = IntRect { { }, documentSize() };
    drawingRect.intersect(enclosingIntRect(clipRect));

    auto inverseScale = 1.0f / documentLayoutScale;
    auto scaleTransform = AffineTransform::makeScale({ inverseScale, inverseScale });
    auto drawingRectInPDFLayoutCoordinates = scaleTransform.mapRect(FloatRect { drawingRect });

    for (PDFDocumentLayout::PageIndex i = 0; i < m_documentLayout.pageCount(); ++i) {
        auto page = m_documentLayout.pageAtIndex(i);
        if (!page)
            continue;

        auto pageBounds = m_documentLayout.layoutBoundsForPageAtIndex(i);
        if (!pageBounds.intersects(drawingRectInPDFLayoutCoordinates))
            continue;

        pageCoverage.pages.append(PerPageInfo { i, pageBounds });
    }

    return pageCoverage;
}

void UnifiedPDFPlugin::paintPDFContent(GraphicsContext& context, const FloatRect& clipRect, PaintingBehavior behavior, AllowsAsyncRendering allowsAsyncRendering)
{
    if (m_size.isEmpty() || documentSize().isEmpty())
        return;

    auto stateSaver = GraphicsContextStateSaver(context);

    auto showDebugIndicators = shouldShowDebugIndicators();

    bool haveSelection = false;
    bool isVisibleAndActive = false;
    bool shouldPaintSelection = behavior == PaintingBehavior::All && !canPaintSelectionIntoOwnedLayer();
    if (m_currentSelection && ![m_currentSelection isEmpty] && shouldPaintSelection) {
        haveSelection = true;
        if (RefPtr page = this->page())
            isVisibleAndActive = page->isVisibleAndActive();
    }

    auto pageWithAnnotation = pageIndexWithHoveredAnnotation();
    auto pageCoverage = pageCoverageForRect(clipRect);
    auto documentScale = pageCoverage.pdfDocumentScale;

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin: paintPDFContent " << pageCoverage);

    for (auto& pageInfo : pageCoverage.pages) {
        auto page = m_documentLayout.pageAtIndex(pageInfo.pageIndex);
        if (!page)
            continue;

        if (!shouldDisplayPage(pageInfo.pageIndex))
            continue;

        auto pageDestinationRect = pageInfo.pageBounds;

        RefPtr<AsyncPDFRenderer> asyncRenderer;
        if (allowsAsyncRendering == AllowsAsyncRendering::Yes)
            asyncRenderer = asyncRendererIfExists();
        if (asyncRenderer) {
            auto pageBoundsInPaintingCoordinates = pageDestinationRect;
            pageBoundsInPaintingCoordinates.scale(documentScale);

            auto pageStateSaver = GraphicsContextStateSaver(context);
            context.clip(pageBoundsInPaintingCoordinates);

            bool paintedPageContent = asyncRenderer->paintTilesForPage(context, documentScale, clipRect, pageBoundsInPaintingCoordinates, pageInfo.pageIndex);
            LOG_WITH_STREAM(PDFAsyncRendering, stream << "UnifiedPDFPlugin::paintPDFContent - painting tiles for page " << pageInfo.pageIndex << " dest rect " << pageBoundsInPaintingCoordinates << " clip " << clipRect << " - painted cached tile " << paintedPageContent);

            if (!paintedPageContent && showDebugIndicators)
                context.fillRect(pageBoundsInPaintingCoordinates, Color::yellow.colorWithAlphaByte(128));
        }

        bool currentPageHasAnnotation = pageWithAnnotation && *pageWithAnnotation == pageInfo.pageIndex;
        if (asyncRenderer && !haveSelection && !currentPageHasAnnotation)
            continue;

        auto pageStateSaver = GraphicsContextStateSaver(context);
        context.scale(documentScale);
        context.clip(pageDestinationRect);

        if (!asyncRenderer)
            context.fillRect(pageDestinationRect, Color::white);

        // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
        // from this page's drawing origin.
        context.translate(pageDestinationRect.minXMaxYCorner());
        context.scale({ 1, -1 });

        if (!asyncRenderer) {
            LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin: painting PDF page " << pageInfo.pageIndex << " into rect " << pageDestinationRect << " with clip " << clipRect);

            [page drawWithBox:kPDFDisplayBoxCropBox toContext:context.platformContext()];
        }

        if (haveSelection || currentPageHasAnnotation) {
            auto pageGeometry = m_documentLayout.geometryForPage(page);
            auto transformForBox = m_documentLayout.toPageTransform(*pageGeometry).inverse().value_or(AffineTransform { });
            GraphicsContextStateSaver stateSaver(context);
            context.concatCTM(transformForBox);

            if (haveSelection)
                [m_currentSelection drawForPage:page.get() withBox:kCGPDFCropBox active:isVisibleAndActive inContext:context.platformContext()];

            if (currentPageHasAnnotation)
                paintHoveredAnnotationOnPage(pageInfo.pageIndex, context, clipRect);
        }
    }
}

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
void UnifiedPDFPlugin::paintPDFSelection(GraphicsContext& context, const FloatRect& clipRect)
{
    if (!m_currentSelection || [m_currentSelection isEmpty] || !canPaintSelectionIntoOwnedLayer())
        return;

    bool isVisibleAndActive = false;
    if (RefPtr page = this->page())
        isVisibleAndActive = page->isVisibleAndActive();

    auto selectionColor = [renderer = m_element->renderer(), isVisibleAndActive] {
        auto& renderTheme = renderer->theme();
        auto styleColorOptions = renderer->styleColorOptions();
        auto selectionColor = isVisibleAndActive ? renderTheme.activeSelectionBackgroundColor(styleColorOptions) : renderTheme.inactiveSelectionBackgroundColor(styleColorOptions);
        return blendSourceOver(Color::white, selectionColor);
    }();

    auto pageCoverage = pageCoverageForRect(clipRect);
    auto documentScale = pageCoverage.pdfDocumentScale;
    for (auto& pageInfo : pageCoverage.pages) {
        auto page = m_documentLayout.pageAtIndex(pageInfo.pageIndex);
        if (!page || !shouldDisplayPage(pageInfo.pageIndex))
            continue;

        auto pageDestinationRect = pageInfo.pageBounds;

        GraphicsContextStateSaver pageStateSaver { context };
        context.scale(documentScale);
        context.clip(pageDestinationRect);
        // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
        // from this page's drawing origin.
        context.translate(pageDestinationRect.minXMaxYCorner());
        context.scale({ 1, -1 });

        auto pageGeometry = m_documentLayout.geometryForPage(page);
        auto transformForBox = m_documentLayout.toPageTransform(*pageGeometry).inverse().value_or(AffineTransform { });
        context.concatCTM(transformForBox);

        if ([m_currentSelection respondsToSelector:@selector(enumerateRectsAndTransformsForPage:usingBlock:)]) {
            [protectedCurrentSelection() enumerateRectsAndTransformsForPage:page.get() usingBlock:[&context, &selectionColor](CGRect cgRect, CGAffineTransform cgTransform) mutable {
                // FIXME: Perf optimization -- consider coalescing rects by transform.
                GraphicsContextStateSaver individualRectTransformPairStateSaver { context, /* saveAndRestore */ false };

                if (AffineTransform transform { cgTransform }; !transform.isIdentity()) {
                    individualRectTransformPairStateSaver.save();
                    context.concatCTM(transform);
                }

                context.fillRect({ cgRect }, selectionColor);
            }];
        }
    }
}
#endif

bool UnifiedPDFPlugin::canPaintSelectionIntoOwnedLayer() const
{
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    return [getPDFSelectionClass() instancesRespondToSelector:@selector(enumerateRectsAndTransformsForPage:usingBlock:)];
#endif
    return false;
}

static const WebCore::Color textAnnotationHoverColor()
{
    static constexpr auto textAnnotationHoverAlpha = 0.12;
    static NeverDestroyed color = WebCore::Color::createAndPreserveColorSpace([[[WebCore::CocoaColor systemBlueColor] colorWithAlphaComponent:textAnnotationHoverAlpha] CGColor]);
    return color;
}

void UnifiedPDFPlugin::paintHoveredAnnotationOnPage(PDFDocumentLayout::PageIndex indexOfPaintedPage, WebCore::GraphicsContext& context, const WebCore::FloatRect& clipRect)
{
    ASSERT(pageIndexWithHoveredAnnotation());
    ASSERT(supportsForms());

    RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    auto pageIndex = [m_pdfDocument indexForPage:[trackedAnnotation page]];
    if (pageIndex != indexOfPaintedPage)
        return;

    // FIXME: Share code with documentRectForAnnotation().
    auto annotationRect = FloatRect { [trackedAnnotation bounds] };
    context.fillRect(annotationRect, textAnnotationHoverColor());
}

std::optional<PDFDocumentLayout::PageIndex> UnifiedPDFPlugin::pageIndexWithHoveredAnnotation() const
{
    RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    if (!trackedAnnotation)
        return { };

    if (![trackedAnnotation isKindOfClass:getPDFAnnotationTextWidgetClass()])
        return { };

    if (!m_annotationTrackingState.isBeingHovered())
        return { };

    auto annotationRect = FloatRect { [trackedAnnotation bounds] };
    auto pageIndex = [m_pdfDocument indexForPage:[trackedAnnotation page]];
    if (pageIndex == NSNotFound)
        return { };

    return pageIndex;
}

double UnifiedPDFPlugin::scaleForActualSize() const
{
#if PLATFORM(MAC)
    if (size().isEmpty())
        return 1;

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

    auto firstPageBounds = m_documentLayout.layoutBoundsForPageAtIndex(0);
    if (firstPageBounds.isEmpty())
        return 1;

    constexpr auto pdfDotsPerInch = 72.0;
    auto screenDPI = screenData->screenDPI();
    float pixelSize = screenDPI * firstPageBounds.width() / pdfDotsPerInch;

    return pixelSize / size().width();
#endif
    return 1;
}

double UnifiedPDFPlugin::scaleForFitToView() const
{
    auto contentsSize = m_documentLayout.scaledContentsSize();
    auto availableSize = FloatSize { availableContentsRect().size() };

    if (contentsSize.isEmpty() || availableSize.isEmpty())
        return 1;

    auto aspectRatioFitRect = largestRectWithAspectRatioInsideRect(contentsSize.aspectRatio(), FloatRect { { }, availableSize });
    return aspectRatioFitRect.width() / size().width();
}

double UnifiedPDFPlugin::initialScale() const
{
    auto actualSizeScale = scaleForActualSize();
    auto fitToViewScale = scaleForFitToView();
    auto initialScale = std::max(actualSizeScale, fitToViewScale);
    // Only let actual size scaling scale down, not up.
    initialScale = std::min(initialScale, 1.0);
    return initialScale;
}

void UnifiedPDFPlugin::computeNormalizationFactor()
{
    auto actualSizeScale = scaleForActualSize();
    m_scaleNormalizationFactor = 1.0 / actualSizeScale;
}

double UnifiedPDFPlugin::fromNormalizedScaleFactor(double normalizedScale) const
{
    return normalizedScale / m_scaleNormalizationFactor;
}

double UnifiedPDFPlugin::toNormalizedScaleFactor(double scale) const
{
    return scale * m_scaleNormalizationFactor;
}

double UnifiedPDFPlugin::scaleFactor() const
{
    // The return value is mapped to `pageScaleFactor`, so we want a value of 1 to match "actual size".
    return toNormalizedScaleFactor(m_scaleFactor);
}

// This is a GraphicsLayerClient function. The return value is used to compute layer contentsScale, so we don't
// want to use the normalized scale factor.
float UnifiedPDFPlugin::pageScaleFactor() const
{
    return m_scaleFactor;
}

double UnifiedPDFPlugin::contentScaleFactor() const
{
    return m_scaleFactor * m_documentLayout.scale();
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
    m_magnificationOriginInContentCoordinates = { };
    m_magnificationOriginInPluginCoordinates = { };
    m_rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
}

void UnifiedPDFPlugin::setScaleFactor(double scale, std::optional<WebCore::IntPoint> originInRootViewCoordinates)
{
    RefPtr page = this->page();
    if (!page)
        return;

    IntPoint originInPluginCoordinates;
    if (originInRootViewCoordinates)
        originInPluginCoordinates = convertFromRootViewToPlugin(*originInRootViewCoordinates);
    else
        originInPluginCoordinates = IntRect({ }, size()).center();

    auto computeOriginInContentsCoordinates = [&]() {
        if (m_magnificationOriginInContentCoordinates) {
            ASSERT(m_magnificationOriginInPluginCoordinates);
            originInPluginCoordinates = *m_magnificationOriginInPluginCoordinates;
            return *m_magnificationOriginInContentCoordinates;
        }

        auto originInContentsCoordinates = roundedIntPoint(convertDown(CoordinateSpace::Plugin, CoordinateSpace::Contents, FloatPoint { originInPluginCoordinates }));

        if (m_inMagnificationGesture && !m_magnificationOriginInContentCoordinates) {
            m_magnificationOriginInPluginCoordinates = originInPluginCoordinates;
            m_magnificationOriginInContentCoordinates = originInContentsCoordinates;
        }

        return originInContentsCoordinates;
    };

    auto zoomContentsOrigin = computeOriginInContentsCoordinates();

    std::exchange(m_scaleFactor, scale);

    updateScrollbars();
    updateScrollingExtents();

    if (!m_inMagnificationGesture || !handlesPageScaleFactor())
        m_rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    updateLayerPositions();
    updatePageBackgroundLayers();
    updateSnapOffsets();

#if PLATFORM(MAC)
    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();
#endif

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    didInvalidateDataDetectorHighlightOverlayRects();
#endif

    auto scrolledContentsPoint = roundedIntPoint(convertUp(CoordinateSpace::Contents, CoordinateSpace::ScrolledContents, FloatPoint { zoomContentsOrigin }));
    auto newScrollPosition = IntPoint { scrolledContentsPoint - originInPluginCoordinates };
    newScrollPosition = newScrollPosition.expandedTo({ 0, 0 });

    scrollToPointInContentsSpace(newScrollPosition);

    scheduleRenderingUpdate();

    m_view->pluginScaleFactorDidChange();

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::setScaleFactor " << scale << " - new scale factor " << m_scaleFactor << " (exposed as normalized scale factor " << scaleFactor() << ") normalization factor " << m_scaleNormalizationFactor << " layout scale " << m_documentLayout.scale());
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

    if (origin) {
        // Compensate for the subtraction of topContentInset that happens in ViewGestureController::handleMagnificationGestureEvent();
        // origin is not in root view coordinates.
        RefPtr frameView = m_frame->coreLocalFrame()->view();
        if (frameView)
            origin->move(0, std::round(frameView->topContentInset()));
    }

    if (scale != 1.0)
        m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;

    // FIXME: Make the overlay scroll with the tiles instead of repainting constantly.
    updateFindOverlay(HideFindIndicator::Yes);

    auto internalScale = fromNormalizedScaleFactor(scale);
    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::setPageScaleFactor " << scale << " mapped to " << internalScale);
    setScaleFactor(internalScale, origin);
}

bool UnifiedPDFPlugin::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    bool sizeChanged = pluginSize != m_size;

    if (!PDFPluginBase::geometryDidChange(pluginSize, pluginToRootViewTransform))
        return false;

#if PLATFORM(MAC)
    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();
#endif

    if (sizeChanged)
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

void UnifiedPDFPlugin::updateLayout(AdjustScaleAfterLayout shouldAdjustScale)
{
    auto layoutSize = availableContentsRect().size();
    auto autoSizeMode = m_didLayoutWithValidDocument ? m_shouldUpdateAutoSizeScale : ShouldUpdateAutoSizeScale::Yes;

    auto scrollAnchoringInfo = scrollAnchoringForCurrentScrollPosition(shouldAdjustScale == AdjustScaleAfterLayout::Yes || autoSizeMode == ShouldUpdateAutoSizeScale::Yes);

    m_documentLayout.updateLayout(layoutSize, autoSizeMode);
    updateScrollbars();

    // Do a second layout pass if the first one changed scrollbars.
    auto newLayoutSize = availableContentsRect().size();
    if (layoutSize != newLayoutSize) {
        m_documentLayout.updateLayout(newLayoutSize, autoSizeMode);
        updateScrollbars();
    }

    m_didLayoutWithValidDocument = m_documentLayout.hasPDFDocument();

    updateLayerHierarchy();
    updateLayerPositions();
    updateScrollingExtents();
    computeNormalizationFactor();

    if (shouldAdjustScale == AdjustScaleAfterLayout::Yes && m_view) {
        auto initialScaleFactor = initialScale();
        LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::updateLayout - on first layout, chose scale for actual size " << initialScaleFactor);
        setScaleFactor(initialScaleFactor);

        m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;
    }

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::updateLayout - scale " << m_scaleFactor << " normalization factor " << m_scaleNormalizationFactor << " layout scale " << m_documentLayout.scale());

    if (scrollAnchoringInfo)
        restoreScrollPositionWithInfo(*scrollAnchoringInfo);
}

auto UnifiedPDFPlugin::scrollAnchoringForCurrentScrollPosition(bool preserveScrollPosition) const -> std::optional<ScrollAnchoringInfo>
{
    if (!preserveScrollPosition)
        return { };

    if (!m_documentLayout.hasLaidOutPDFDocument())
        return { };

    auto topLeftInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { });
    auto [pageIndex, pointInPDFPageSpace] = m_documentLayout.pageIndexAndPagePointForDocumentYOffset(topLeftInDocumentSpace.y());

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::scrollAnchoringForCurrentScrollPosition - point " << pointInPDFPageSpace << " in page " << pageIndex);

    return ScrollAnchoringInfo { pageIndex, pointInPDFPageSpace };
}

void UnifiedPDFPlugin::restoreScrollPositionWithInfo(const ScrollAnchoringInfo& info)
{
    scrollToPointInPage(info.pagePoint, info.pageIndex);
}

FloatSize UnifiedPDFPlugin::centeringOffset() const
{
    auto availableSize = FloatSize { availableContentsRect().size() };
    auto documentPresentationSize = m_documentLayout.scaledContentsSize() * m_scaleFactor;
    if (availableSize.isEmpty() || documentPresentationSize.isEmpty())
        return { };

    auto offset = FloatSize {
        std::floor(std::max<float>(availableSize.width() - documentPresentationSize.width(), 0) / 2),
        std::floor(std::max<float>(availableSize.height() - documentPresentationSize.height(), 0) / 2)
    };

    offset.scale(1 / m_scaleFactor);
    return offset;
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

unsigned UnifiedPDFPlugin::heightForPageAtIndex(PDFDocumentLayout::PageIndex pageIndex) const
{
    if (isLocked() || pageIndex >= m_documentLayout.pageCount())
        return 0;

    return std::ceil<unsigned>(m_documentLayout.layoutBoundsForPageAtIndex(pageIndex).height());
}

unsigned UnifiedPDFPlugin::firstPageHeight() const
{
    return heightForPageAtIndex(0);
}

FloatRect UnifiedPDFPlugin::layoutBoundsForPageAtIndex(PDFDocumentLayout::PageIndex pageIndex) const
{
    if (isLocked() || pageIndex >= m_documentLayout.pageCount())
        return { };

    return m_documentLayout.layoutBoundsForPageAtIndex(pageIndex);
}

RetainPtr<PDFPage> UnifiedPDFPlugin::pageAtIndex(PDFDocumentLayout::PageIndex pageIndex) const
{
    return m_documentLayout.pageAtIndex(pageIndex);
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

    if (m_animatedKeyboardScrollingDirection && !m_isScrollingWithAnimationToPageExtent)
        snapToNearbyPageExtentForKeyboardScrolling(*m_animatedKeyboardScrollingDirection);
#endif // PLATFORM(MAC)

    determineCurrentlySnappedPage();

    // FIXME: Make the overlay scroll with the tiles instead of repainting constantly.
    updateFindOverlay(HideFindIndicator::Yes);

    // FIXME: Make the overlay scroll with the tiles instead of repainting constantly.
#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    didInvalidateDataDetectorHighlightOverlayRects();
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
            if (!layer)
                return false;

            layer->setAllowsBackingStoreDetaching(false);
            layer->setAllowsTiling(false);
            layer->setDrawsContent(true);
            layer->setAcceleratesDrawing(true);

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
        layer->setAcceleratesDrawing(true);
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

    page->chrome().client().ensureScrollbarsController(*page, *this);
}

DelegatedScrollingMode UnifiedPDFPlugin::scrollingMode() const
{
#if PLATFORM(IOS_FAMILY)
    return DelegatedScrollingMode::DelegatedToNativeScrollView;
#else
    return DelegatedScrollingMode::NotDelegated;
#endif
}

bool UnifiedPDFPlugin::isFullMainFramePlugin() const
{
    return m_frame->isMainFrame() && isFullFramePlugin();
}

OptionSet<TiledBackingScrollability> UnifiedPDFPlugin::computeScrollability() const
{
    OptionSet<TiledBacking::Scrollability> scrollability = TiledBacking::Scrollability::NotScrollable;
    if (allowsHorizontalScrolling())
        scrollability.add(TiledBacking::Scrollability::HorizontallyScrollable);

    if (allowsVerticalScrolling())
        scrollability.add(TiledBacking::Scrollability::VerticallyScrollable);

    return scrollability;
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

    if (auto* tiledBacking = m_contentsLayer->tiledBacking())
        tiledBacking->setScrollability(computeScrollability());

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

void UnifiedPDFPlugin::populateScrollSnapIdentifiers()
{
    ASSERT(m_scrollSnapIdentifiers.isEmpty());

    // Create fake ElementIdentifiers for each page.
    // FIXME: Ideally we would not use DOM Element identifiers here, but scroll
    // snap code wants them (but never uses them to look the element back up).
    // We should consider making it generic.
    for (PDFDocumentLayout::PageIndex i = 0; i < m_documentLayout.pageCount(); ++i)
        m_scrollSnapIdentifiers.append(ElementIdentifier::generate());
}

PDFDocumentLayout::PageIndex UnifiedPDFPlugin::pageForScrollSnapIdentifier(ElementIdentifier scrollSnapIdentifier) const
{
    auto index = m_scrollSnapIdentifiers.find(scrollSnapIdentifier);
    RELEASE_ASSERT(index != notFound);
    return index;
}

bool UnifiedPDFPlugin::isInDiscreteDisplayMode() const
{
    return m_documentLayout.displayMode() == PDFDocumentLayout::DisplayMode::SinglePageDiscrete || m_documentLayout.displayMode() == PDFDocumentLayout::DisplayMode::TwoUpDiscrete;
}

bool UnifiedPDFPlugin::isShowingTwoPages() const
{
    return m_documentLayout.displayMode() == PDFDocumentLayout::DisplayMode::TwoUpContinuous || m_documentLayout.displayMode() == PDFDocumentLayout::DisplayMode::TwoUpDiscrete;
}

FloatRect UnifiedPDFPlugin::pageBoundsInContentsSpace(PDFDocumentLayout::PageIndex index) const
{
    auto bounds = m_documentLayout.layoutBoundsForPageAtIndex(index);
    bounds.inflate(PDFDocumentLayout::pageMargin);
    bounds.scale(contentScaleFactor());
    return bounds;
}

void UnifiedPDFPlugin::updateSnapOffsets()
{
    if (m_scrollSnapIdentifiers.isEmpty())
        populateScrollSnapIdentifiers();

    if (!isInDiscreteDisplayMode()) {
        clearSnapOffsets();
        return;
    }

    Vector<SnapOffset<LayoutUnit>> verticalSnapOffsets;
    Vector<LayoutRect> snapAreas;

    for (PDFDocumentLayout::PageIndex i = 0; i < m_documentLayout.pageCount(); ++i) {
        auto pageBoundsInDocumentSpace = m_documentLayout.layoutBoundsForPageAtIndex(i);
        auto destinationRect = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::Contents, pageBoundsInDocumentSpace);

        snapAreas.append(LayoutRect { destinationRect });

        bool isTallerThanViewport = destinationRect.height() > m_size.height();

        verticalSnapOffsets.append(SnapOffset<LayoutUnit> {
            LayoutUnit { destinationRect.y() },
            ScrollSnapStop::Always,
            isTallerThanViewport,
            m_scrollSnapIdentifiers[i],
            false,
            { i },
        });

        if (isTallerThanViewport) {
            verticalSnapOffsets.append(SnapOffset<LayoutUnit> {
                LayoutUnit { destinationRect.maxY() - m_size.height() },
                ScrollSnapStop::Always,
                isTallerThanViewport,
                m_scrollSnapIdentifiers[i],
                false,
                { i },
            });
        }
    }

    setScrollSnapOffsetInfo({
        ScrollSnapStrictness::Mandatory,
        { },
        verticalSnapOffsets,
        snapAreas,
        m_scrollSnapIdentifiers,
    });

    determineCurrentlySnappedPage();
}

void UnifiedPDFPlugin::determineCurrentlySnappedPage()
{
    std::optional<PDFDocumentLayout::PageIndex> newSnappedPage;

    if (isInDiscreteDisplayMode() && snapOffsetsInfo() && snapOffsetsInfo()->verticalSnapOffsets.size()) {
        std::optional<ElementIdentifier> newSnapIdentifier = snapOffsetsInfo()->verticalSnapOffsets[0].snapTargetID;

        FloatPoint currentScrollPosition = scrollPosition();
        float scrollPositionYInContentsSpace = convertDown(CoordinateSpace::ScrolledContents, CoordinateSpace::Contents, currentScrollPosition).y();

        auto closestDistanceToSnapOffset = std::numeric_limits<float>::max();
        for (const auto& offsetInfo : snapOffsetsInfo()->verticalSnapOffsets) {
            // FIXME: Can this padding be derived from something?
            auto distance = std::abs(scrollPositionYInContentsSpace + 10 - offsetInfo.offset);
            if (distance < closestDistanceToSnapOffset) {
                closestDistanceToSnapOffset = distance;
                newSnapIdentifier = offsetInfo.snapTargetID;
            }
        }

        if (newSnapIdentifier)
            newSnappedPage = pageForScrollSnapIdentifier(*newSnapIdentifier);
    }

    if (m_currentlySnappedPage != newSnappedPage) {
        m_currentlySnappedPage = newSnappedPage;
        updatePageBackgroundLayers();
        m_contentsLayer->setNeedsDisplay();
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        m_selectionLayer->setNeedsDisplay();
#endif
    }
}

bool UnifiedPDFPlugin::shouldDisplayPage(PDFDocumentLayout::PageIndex pageIndex)
{
    if (!isInDiscreteDisplayMode())
        return true;

    if (!m_currentlySnappedPage)
        return true;
    auto currentlySnappedPage = *m_currentlySnappedPage;

    if (pageIndex == currentlySnappedPage)
        return true;

    if (m_documentLayout.displayMode() == PDFDocumentLayout::DisplayMode::TwoUpDiscrete) {
        if (currentlySnappedPage % 2)
            return pageIndex == currentlySnappedPage - 1;
        return pageIndex == currentlySnappedPage + 1;
    }

    return false;
}

#pragma mark -

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

std::optional<PDFDocumentLayout::PageIndex> UnifiedPDFPlugin::pageIndexForDocumentPoint(const FloatPoint& point) const
{
    for (PDFDocumentLayout::PageIndex index = 0; index < m_documentLayout.pageCount(); ++index) {
        auto pageBounds = m_documentLayout.layoutBoundsForPageAtIndex(index);
        if (pageBounds.contains(point))
            return index;
    }

    return { };
}

PDFDocumentLayout::PageIndex UnifiedPDFPlugin::indexForCurrentPageInView() const
{
    auto centerInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { flooredIntPoint(size() / 2) });
    return m_documentLayout.nearestPageIndexForDocumentPoint(centerInDocumentSpace);
}

RetainPtr<PDFAnnotation> UnifiedPDFPlugin::annotationForRootViewPoint(const IntPoint& point) const
{
    auto pointInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { convertFromRootViewToPlugin(point) });
    auto pageIndex = pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!pageIndex)
        return nullptr;

    auto page = m_documentLayout.pageAtIndex(pageIndex.value());
    return [page annotationAtPoint:convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex.value())];
}

template <typename T>
T UnifiedPDFPlugin::convertUp(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    static_assert(std::is_same<T, FloatPoint>::value || std::is_same<T, FloatRect>::value, "Coordinate conversion should use float types");
    auto mappedValue = sourceValue;

    switch (sourceSpace) {
    case CoordinateSpace::PDFPage:
        if (destinationSpace == CoordinateSpace::PDFPage)
            return mappedValue;

        ASSERT(pageIndex);
        ASSERT(*pageIndex < m_documentLayout.pageCount());
        mappedValue = m_documentLayout.pdfPageToDocument(mappedValue, *pageIndex);
        FALLTHROUGH;

    case CoordinateSpace::PDFDocumentLayout:
        if (destinationSpace == CoordinateSpace::PDFDocumentLayout)
            return mappedValue;

        mappedValue.scale(m_documentLayout.scale());
        FALLTHROUGH;

    case CoordinateSpace::Contents:
        if (destinationSpace == CoordinateSpace::Contents)
            return mappedValue;

        mappedValue.move(centeringOffset());
        mappedValue.scale(m_scaleFactor);
        FALLTHROUGH;

    case CoordinateSpace::ScrolledContents:
        if (destinationSpace == CoordinateSpace::ScrolledContents)
            return mappedValue;

        mappedValue.moveBy(-WebCore::FloatPoint { m_scrollOffset });
        FALLTHROUGH;

    case CoordinateSpace::Plugin:
        if (destinationSpace == CoordinateSpace::Plugin)
            return mappedValue;
    }

    ASSERT_NOT_REACHED();
    return mappedValue;
}

template <typename T>
T UnifiedPDFPlugin::convertDown(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    static_assert(std::is_same<T, FloatPoint>::value || std::is_same<T, FloatRect>::value, "Coordinate conversion should use float types");
    auto mappedValue = sourceValue;

    switch (sourceSpace) {
    case CoordinateSpace::Plugin:
        if (destinationSpace == CoordinateSpace::Plugin)
            return mappedValue;

        mappedValue.moveBy(WebCore::FloatPoint { m_scrollOffset });
        FALLTHROUGH;

    case CoordinateSpace::ScrolledContents:
        if (destinationSpace == CoordinateSpace::ScrolledContents)
            return mappedValue;

        mappedValue.scale(1 / m_scaleFactor);
        mappedValue.move(-centeringOffset());
        FALLTHROUGH;

    case CoordinateSpace::Contents:
        if (destinationSpace == CoordinateSpace::Contents)
            return mappedValue;

        mappedValue.scale(1 / m_documentLayout.scale());
        FALLTHROUGH;

    case CoordinateSpace::PDFDocumentLayout:
        if (destinationSpace == CoordinateSpace::PDFDocumentLayout)
            return mappedValue;

        ASSERT(pageIndex);
        ASSERT(*pageIndex < m_documentLayout.pageCount());
        mappedValue = m_documentLayout.documentToPDFPage(mappedValue, *pageIndex);
        FALLTHROUGH;

    case CoordinateSpace::PDFPage:
        if (destinationSpace == CoordinateSpace::PDFPage)
            return mappedValue;
    }

    ASSERT_NOT_REACHED();
    return mappedValue;
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

static BOOL annotationIsExternalLink(PDFAnnotation *annotation)
{
    if (![annotation isKindOfClass:getPDFAnnotationLinkClass()])
        return NO;

    return !![annotation URL];
}

static BOOL annotationIsLinkWithDestination(PDFAnnotation *annotation)
{
    if (![annotation isKindOfClass:getPDFAnnotationLinkClass()])
        return NO;

    return [annotation URL] || [annotation destination];
}

auto UnifiedPDFPlugin::pdfElementTypesForPluginPoint(const IntPoint& point) const -> PDFElementTypes
{
    auto pointInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, point);
    auto hitPageIndex = pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!hitPageIndex || *hitPageIndex >= m_documentLayout.pageCount())
        return { };

    auto pageIndex = *hitPageIndex;
    RetainPtr page = m_documentLayout.pageAtIndex(pageIndex);
    auto pointInPDFPageSpace = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex);

    PDFElementTypes pdfElementTypes { PDFElementType::Page };

#if HAVE(PDFPAGE_AREA_OF_INTEREST_AT_POINT)
    if ([page respondsToSelector:@selector(areaOfInterestAtPoint:)]) {
        PDFAreaOfInterest areaOfInterest = [page areaOfInterestAtPoint:pointInPDFPageSpace];

        if (areaOfInterest & kPDFTextArea)
            pdfElementTypes.add(PDFElementType::Text);

        if (areaOfInterest & kPDFAnnotationArea)
            pdfElementTypes.add(PDFElementType::Annotation);

        if ((areaOfInterest & kPDFLinkArea) && annotationIsLinkWithDestination([page annotationAtPoint:pointInPDFPageSpace]))
            pdfElementTypes.add(PDFElementType::Link);

        if (areaOfInterest & kPDFControlArea)
            pdfElementTypes.add(PDFElementType::Control);

        if (areaOfInterest & kPDFTextFieldArea)
            pdfElementTypes.add(PDFElementType::TextField);

        if (areaOfInterest & kPDFIconArea)
            pdfElementTypes.add(PDFElementType::Icon);

        if (areaOfInterest & kPDFPopupArea)
            pdfElementTypes.add(PDFElementType::Popup);

        if (areaOfInterest & kPDFImageArea)
            pdfElementTypes.add(PDFElementType::Image);

        return pdfElementTypes;
    }
#endif

    if (auto annotation = [page annotationAtPoint:pointInPDFPageSpace]) {
        pdfElementTypes.add(PDFElementType::Annotation);

        if (annotationIsLinkWithDestination(annotation))
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

#if HAVE(COREGRAPHICS_WITH_PDF_AREA_OF_INTEREST_SUPPORT)
    if (auto pageLayout = [page pageLayout]) {
        CGPDFAreaOfInterest areaOfInterest = CGPDFPageLayoutGetAreaOfInterestAtPoint(pageLayout, pointInPDFPageSpace);
        if (areaOfInterest & kCGPDFAreaText)
            pdfElementTypes.add(PDFElementType::Text);
        if (areaOfInterest & kCGPDFAreaImage)
            pdfElementTypes.add(PDFElementType::Image);
    }
#endif

    return pdfElementTypes;
}

#pragma mark Events

static bool isContextMenuEvent(const WebMouseEvent& event)
{
#if PLATFORM(MAC)
    return event.menuTypeForEvent();
#else
    UNUSED_PARAM(event);
    return false;
#endif
}

bool UnifiedPDFPlugin::handleMouseEvent(const WebMouseEvent& event)
{
    m_lastMouseEvent = event;

    if (!m_pdfDocument)
        return false;

    // Even if the mouse event isn't handled (e.g. because the event is over a page we shouldn't
    // display in Single Page mode), we should stop tracking selections (and soon autoscrolling) on MouseUp.
    auto stopStateTrackingIfNeeded = makeScopeExit([this, isMouseUp = event.type() == WebEventType::MouseUp] {
        if (isMouseUp) {
            stopTrackingSelection();
            stopAutoscroll();
        }
    });

    auto pointInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, lastKnownMousePositionInView());
    auto pageIndex = m_documentLayout.nearestPageIndexForDocumentPoint(pointInDocumentSpace);

    if (!shouldDisplayPage(pageIndex)) {
        notifyCursorChanged(toWebCoreCursorType({ }));
        return false;
    }

    auto pointInPageSpace = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex);

    auto mouseEventButton = event.button();
    auto mouseEventType = event.type();
    // Context menu events always call handleContextMenuEvent as well.
    if (mouseEventType == WebEventType::MouseDown && isContextMenuEvent(event)) {
        beginTrackingSelection(pageIndex, pointInPageSpace, event);
        return true;
    }

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    if (dataDetectorOverlayController().handleMouseEvent(event, pageIndex))
        return true;
#endif

    switch (mouseEventType) {
    case WebEventType::MouseMove:
        mouseMovedInContentArea();
        switch (mouseEventButton) {
        case WebMouseEventButton::None: {
            auto altKeyIsActive = event.altKey() ? AltKeyIsActive::Yes : AltKeyIsActive::No;
            auto pdfElementTypes = pdfElementTypesForPluginPoint(lastKnownMousePositionInView());
            notifyCursorChanged(toWebCoreCursorType(pdfElementTypes, altKeyIsActive));

            RetainPtr annotationUnderMouse = annotationForRootViewPoint(event.position());
            if (auto* currentTrackedAnnotation = m_annotationTrackingState.trackedAnnotation(); (currentTrackedAnnotation && currentTrackedAnnotation != annotationUnderMouse) || (currentTrackedAnnotation && !m_annotationTrackingState.isBeingHovered()))
                finishTrackingAnnotation(annotationUnderMouse.get(), mouseEventType, mouseEventButton, RepaintRequirement::HoverOverlay);

            if (!m_annotationTrackingState.trackedAnnotation() && annotationUnderMouse && [annotationUnderMouse isKindOfClass:getPDFAnnotationTextWidgetClass()] && supportsForms())
                startTrackingAnnotation(WTFMove(annotationUnderMouse), mouseEventType, mouseEventButton);

            return true;
        }
        case WebMouseEventButton::Left: {
            if (RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation()) {
                RetainPtr annotationUnderMouse = annotationForRootViewPoint(event.position());
                updateTrackedAnnotation(annotationUnderMouse.get());
                return true;
            }

            if (m_selectionTrackingData.isActivelyTrackingSelection)
                continueTrackingSelection(pageIndex, pointInPageSpace, IsDraggingSelection::Yes);

            return true;
        }
        default:
            return false;
        }
    case WebEventType::MouseDown:
        switch (mouseEventButton) {
        case WebMouseEventButton::Left: {
            if (RetainPtr<PDFAnnotation> annotation = annotationForRootViewPoint(event.position())) {
                if (([annotation isKindOfClass:getPDFAnnotationButtonWidgetClass()] || [annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] || [annotation isKindOfClass:getPDFAnnotationChoiceWidgetClass()]) && [annotation isReadOnly])
                    return true;

                if ([annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] || [annotation isKindOfClass:getPDFAnnotationChoiceWidgetClass()]) {
                    setActiveAnnotation(WTFMove(annotation));
                    return true;
                }

                if ([annotation isKindOfClass:getPDFAnnotationButtonWidgetClass()]) {
                    startTrackingAnnotation(WTFMove(annotation), mouseEventType, mouseEventButton);
                    return true;
                }

                if (annotationIsLinkWithDestination(annotation.get())) {
                    startTrackingAnnotation(WTFMove(annotation), mouseEventType, mouseEventButton);
                    return true;
                }
            }

            beginTrackingSelection(pageIndex, pointInPageSpace, event);
            return false;
        }
        default:
            return false;
        }
    case WebEventType::MouseUp:
        switch (mouseEventButton) {
        case WebMouseEventButton::Left:
            if (RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation(); trackedAnnotation && ![trackedAnnotation isKindOfClass:getPDFAnnotationTextWidgetClass()]) {
                RetainPtr annotationUnderMouse = annotationForRootViewPoint(event.position());
                finishTrackingAnnotation(annotationUnderMouse.get(), mouseEventType, mouseEventButton);

                if (annotationIsLinkWithDestination(trackedAnnotation.get()))
                    followLinkAnnotation(trackedAnnotation.get());

#if PLATFORM(MAC)
                if (RetainPtr pdfAction = [trackedAnnotation action])
                    handlePDFActionForAnnotation(trackedAnnotation.get(), pageIndex);
#endif
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
#if ENABLE(CONTEXT_MENUS)
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return false;

    auto contextMenu = createContextMenu(event);
    if (!contextMenu)
        return false;

    webPage->sendWithAsyncReply(Messages::WebPageProxy::ShowPDFContextMenu { *contextMenu, identifier() }, [eventPosition = event.position(), this, weakThis = WeakPtr { *this }](std::optional<int32_t>&& selectedItemTag) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (selectedItemTag)
            performContextMenuAction(toContextMenuItemTag(selectedItemTag.value()), eventPosition);
        stopTrackingSelection();
    });

    return true;
#else
    return false;
#endif // ENABLE(CONTEXT_MENUS)
}

#if PLATFORM(MAC)

CheckedPtr<KeyboardScrollingAnimator> UnifiedPDFPlugin::checkedKeyboardScrollingAnimator() const
{
    return scrollAnimator().keyboardScrollingAnimator();
}

bool UnifiedPDFPlugin::handleKeyboardCommand(const WebKeyboardEvent& event)
{
    auto& commands = event.commands();
    if (commands.size() != 1)
        return false;

    auto commandName = commands[0].commandName;
    if (commandName == "scrollToBeginningOfDocument:"_s)
        return checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollUp, ScrollGranularity::Document, false);

    if (commandName == "scrollToEndOfDocument:"_s)
        return checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollDown, ScrollGranularity::Document, false);

    return false;
}

bool UnifiedPDFPlugin::handleKeyboardEventForDiscreteDisplayMode(const WebKeyboardEvent& event)
{
    if (!m_currentlySnappedPage || !isInDiscreteDisplayMode())
        return false;

    if (event.type() == WebEventType::KeyUp) {
        checkedKeyboardScrollingAnimator()->handleKeyUpEvent();
        return false;
    }

    if (event.type() != WebEventType::KeyDown || m_isScrollingWithAnimationToPageExtent)
        return false;

    auto key = event.key();
    if (key == "ArrowLeft"_s) {
        if (checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollLeft, ScrollGranularity::Line, false)) {
            m_animatedKeyboardScrollingDirection = ScrollDirection::ScrollLeft;
            return true;
        }
        return false;
    }

    if (key == "ArrowRight"_s) {
        if (checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollRight, ScrollGranularity::Line, false)) {
            m_animatedKeyboardScrollingDirection = ScrollDirection::ScrollRight;
            return true;
        }
        return false;
    }

    auto currentPage = *m_currentlySnappedPage;
    auto currentPageContentRect = pageBoundsInContentsSpace(currentPage);
    bool isTallerThanViewport = currentPageContentRect.height() > m_size.height();
    if (isTallerThanViewport) {
        if (key == "ArrowUp"_s) {
            if (snapToNearbyPageExtentForKeyboardScrolling(ScrollDirection::ScrollUp))
                return true;

            if (checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollUp, ScrollGranularity::Line, false)) {
                m_animatedKeyboardScrollingDirection = ScrollDirection::ScrollUp;
                return true;
            }
        }

        if (key == "ArrowDown"_s) {
            if (snapToNearbyPageExtentForKeyboardScrolling(ScrollDirection::ScrollDown))
                return true;

            if (checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollDown, ScrollGranularity::Line, false)) {
                m_animatedKeyboardScrollingDirection = ScrollDirection::ScrollDown;
                return true;
            }
        }

        return false;
    }

    if (key == "ArrowUp"_s && currentPage) {
        auto previousPageContentRect = pageBoundsInContentsSpace(currentPage - 1);
        return scrollAnimator().scrollToPositionWithoutAnimation({ static_cast<float>(m_scrollOffset.width()), previousPageContentRect.y() });
    }

    auto pagesToSkip = isShowingTwoPages() ? 2 : 1;
    if (key == "ArrowDown"_s && currentPage < m_documentLayout.pageCount() - pagesToSkip) {
        auto nextPageContentRect = pageBoundsInContentsSpace(currentPage + pagesToSkip);
        return scrollAnimator().scrollToPositionWithoutAnimation({ static_cast<float>(m_scrollOffset.width()), nextPageContentRect.y() });
    }

    return false;
}

bool UnifiedPDFPlugin::snapToNearbyPageExtentForKeyboardScrolling(ScrollDirection direction)
{
    if (!m_currentlySnappedPage)
        return false;

    auto currentPage = *m_currentlySnappedPage;
    auto currentPageContentRect = pageBoundsInContentsSpace(currentPage);
    static constexpr auto distanceBeforeSnappingToPageExtents = 100;
    if (direction == ScrollDirection::ScrollUp && currentPage && m_scrollOffset.height() < currentPageContentRect.y() + distanceBeforeSnappingToPageExtents) {
        checkedKeyboardScrollingAnimator()->stopScrollingImmediately();
        m_isScrollingWithAnimationToPageExtent |= scrollAnimator().scrollToPositionWithAnimation({ static_cast<float>(m_scrollOffset.width()), currentPageContentRect.y() });
        return true;
    }

    float scrollExtentForCurrentPage = currentPageContentRect.maxY() - m_size.height();
    auto pagesToSkip = isShowingTwoPages() ? 2 : 1;
    if (direction == ScrollDirection::ScrollDown && currentPage < m_documentLayout.pageCount() - pagesToSkip && m_scrollOffset.height() > scrollExtentForCurrentPage - distanceBeforeSnappingToPageExtents) {
        checkedKeyboardScrollingAnimator()->stopScrollingImmediately();
        m_isScrollingWithAnimationToPageExtent |= scrollAnimator().scrollToPositionWithAnimation({ static_cast<float>(m_scrollOffset.width()), scrollExtentForCurrentPage });
        return true;
    }

    return false;
}

#endif // PLATFORM(MAC)

void UnifiedPDFPlugin::animatedScrollDidEnd()
{
#if PLATFORM(MAC)
    m_isScrollingWithAnimationToPageExtent = false;
    m_animatedKeyboardScrollingDirection = std::nullopt;
#endif
}

bool UnifiedPDFPlugin::handleKeyboardEvent(const WebKeyboardEvent& event)
{
#if PLATFORM(MAC)
    if (handleKeyboardCommand(event))
        return true;

    if (handleKeyboardEventForDiscreteDisplayMode(event))
        return true;
#endif

    return false;
}

void UnifiedPDFPlugin::followLinkAnnotation(PDFAnnotation *annotation)
{
    ASSERT(annotationIsLinkWithDestination(annotation));
    if (NSURL *url = [annotation URL])
        navigateToURL(url);
    else if (PDFDestination *destination = [annotation destination])
        scrollToPDFDestination(destination);
}

OptionSet<RepaintRequirement> UnifiedPDFPlugin::repaintRequirementsForAnnotation(PDFAnnotation *annotation, IsAnnotationCommit isAnnotationCommit)
{
    if ([annotation isKindOfClass:getPDFAnnotationButtonWidgetClass()])
        return RepaintRequirement::PDFContent;

    if ([annotation isKindOfClass:getPDFAnnotationPopupClass()])
        return RepaintRequirement::PDFContent;

    if ([annotation isKindOfClass:getPDFAnnotationChoiceWidgetClass()])
        return RepaintRequirement::PDFContent;

    if ([annotation isKindOfClass:getPDFAnnotationTextClass()])
        return RepaintRequirement::PDFContent;

    if ([annotation isKindOfClass:getPDFAnnotationTextWidgetClass()])
        return isAnnotationCommit == IsAnnotationCommit::Yes ? RepaintRequirement::PDFContent : RepaintRequirement::HoverOverlay;

    // No visual feedback for getPDFAnnotationLinkClass at this time.

    return { };
}

void UnifiedPDFPlugin::repaintAnnotationsForFormField(NSString *fieldName)
{
    RetainPtr annotations = [m_pdfDocument annotationsForFieldName:fieldName];
    for (PDFAnnotation *annotation in annotations.get())
        setNeedsRepaintInDocumentRect(repaintRequirementsForAnnotation(annotation), documentRectForAnnotation(annotation));
}

WebCore::FloatRect UnifiedPDFPlugin::documentRectForAnnotation(PDFAnnotation *annotation) const
{
    if (!annotation)
        return { };

    auto pageIndex = [m_pdfDocument indexForPage:[annotation page]];
    if (pageIndex == NSNotFound)
        return { };

    auto annotationPageBounds = FloatRect { [annotation bounds] };
    return convertUp(CoordinateSpace::PDFPage, CoordinateSpace::PDFDocumentLayout, annotationPageBounds, pageIndex);
}

void UnifiedPDFPlugin::startTrackingAnnotation(RetainPtr<PDFAnnotation>&& annotation, WebEventType mouseEventType, WebMouseEventButton mouseEventButton)
{
    auto repaintRequirements = m_annotationTrackingState.startAnnotationTracking(WTFMove(annotation), mouseEventType, mouseEventButton);
    if (repaintRequirements) {
        auto annotationRect = documentRectForAnnotation(m_annotationTrackingState.trackedAnnotation());
        setNeedsRepaintInDocumentRect(repaintRequirements, annotationRect);
    }
}

void UnifiedPDFPlugin::updateTrackedAnnotation(PDFAnnotation *annotationUnderMouse)
{
    RetainPtr currentTrackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    bool isHighlighted = [currentTrackedAnnotation isHighlighted];
    OptionSet<RepaintRequirement> repaintRequirements;

    if (isHighlighted && currentTrackedAnnotation != annotationUnderMouse) {
        [currentTrackedAnnotation setHighlighted:NO];
        repaintRequirements.add(UnifiedPDFPlugin::repaintRequirementsForAnnotation(currentTrackedAnnotation.get()));
    } else if (!isHighlighted && currentTrackedAnnotation == annotationUnderMouse) {
        [currentTrackedAnnotation setHighlighted:YES];
        repaintRequirements.add(UnifiedPDFPlugin::repaintRequirementsForAnnotation(currentTrackedAnnotation.get()));
    }

    if (!repaintRequirements.isEmpty())
        setNeedsRepaintInDocumentRect(repaintRequirements, documentRectForAnnotation(currentTrackedAnnotation.get()));
}

void UnifiedPDFPlugin::finishTrackingAnnotation(PDFAnnotation* annotationUnderMouse, WebEventType mouseEventType, WebMouseEventButton mouseEventButton, OptionSet<RepaintRequirement> repaintRequirements)
{
    auto annotationRect = documentRectForAnnotation(m_annotationTrackingState.trackedAnnotation());
    repaintRequirements.add(m_annotationTrackingState.finishAnnotationTracking(annotationUnderMouse, mouseEventType, mouseEventButton));
    setNeedsRepaintInDocumentRect(repaintRequirements, annotationRect);
}

void UnifiedPDFPlugin::scrollToPointInContentsSpace(FloatPoint pointInContentsSpace)
{
    auto oldScrollType = currentScrollType();
    setCurrentScrollType(ScrollType::Programmatic);
    scrollToPositionWithoutAnimation(roundedIntPoint(pointInContentsSpace));
    setCurrentScrollType(oldScrollType);
}

void UnifiedPDFPlugin::revealRectInContentsSpace(FloatRect rectInContentsSpace)
{
    auto pluginRectInContentsCoordinates = convertDown(CoordinateSpace::Plugin, CoordinateSpace::ScrolledContents, FloatRect { { 0, 0 }, size() });
    auto rectToExpose = getRectToExposeForScrollIntoView(LayoutRect(pluginRectInContentsCoordinates), LayoutRect(rectInContentsSpace), ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, std::nullopt);
    scrollToPointInContentsSpace(rectToExpose.location());
}

void UnifiedPDFPlugin::scrollToPDFDestination(PDFDestination *destination)
{
    auto unspecifiedValue = get_PDFKit_kPDFDestinationUnspecifiedValue();

    auto pageIndex = [m_pdfDocument indexForPage:[destination page]];
    auto pointInPDFPageSpace = [destination point];
    if (pointInPDFPageSpace.x == unspecifiedValue)
        pointInPDFPageSpace.x = 0;
    if (pointInPDFPageSpace.y == unspecifiedValue)
        pointInPDFPageSpace.y = heightForPageAtIndex(pageIndex);

    scrollToPointInPage(pointInPDFPageSpace, pageIndex);
}

void UnifiedPDFPlugin::scrollToPointInPage(FloatPoint pointInPDFPageSpace, PDFDocumentLayout::PageIndex pageIndex)
{
    scrollToPointInContentsSpace(convertUp(CoordinateSpace::PDFPage, CoordinateSpace::ScrolledContents, pointInPDFPageSpace, pageIndex));
}

void UnifiedPDFPlugin::scrollToPage(PDFDocumentLayout::PageIndex pageIndex)
{
    ASSERT(pageIndex < m_documentLayout.pageCount());

    auto pageBounds = m_documentLayout.layoutBoundsForPageAtIndex(pageIndex);
    auto boundsInScrolledContents = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::ScrolledContents, pageBounds);

    scrollToPointInContentsSpace(boundsInScrolledContents.location());
}

void UnifiedPDFPlugin::scrollToFragmentIfNeeded()
{
    if (!m_pdfDocument || !m_didAttachScrollingTreeNode || isLocked())
        return;

    if (m_didScrollToFragment)
        return;

    m_didScrollToFragment = true;

    if (!m_frame)
        return;

    auto fragment = m_frame->url().fragmentIdentifier();
    if (!fragment)
        return;

    auto fragmentView = StringView(fragment);

    // Only respect the first fragment component.
    if (auto endOfFirstComponentLocation = fragmentView.find('&'); endOfFirstComponentLocation != notFound)
        fragmentView = fragment.left(endOfFirstComponentLocation);

    // Ignore leading hashes.
    auto isNotHash = [](UChar character) {
        return character != '#';
    };
    if (auto firstNonHashLocation = fragmentView.find(isNotHash); firstNonHashLocation != notFound)
        fragmentView = fragment.substring(firstNonHashLocation);
    else
        return;

    auto remainderForPrefix = [&](ASCIILiteral prefix) -> std::optional<StringView> {
        if (fragmentView.startsWith(prefix))
            return fragmentView.substring(prefix.length());
        return std::nullopt;
    };

    if (auto remainder = remainderForPrefix("page="_s)) {
        if (auto pageNumber = parseInteger<PDFDocumentLayout::PageIndex>(*remainder); pageNumber)
            scrollToPage(*pageNumber - 1);
        return;
    }

    if (auto remainder = remainderForPrefix("nameddest="_s)) {
        if (auto destination = [m_pdfDocument namedDestination:remainder->createNSString().get()])
            scrollToPDFDestination(destination);
        return;
    }
}

#pragma mark Context Menu

#if ENABLE(CONTEXT_MENUS)
UnifiedPDFPlugin::ContextMenuItemTag UnifiedPDFPlugin::contextMenuItemTagFromDisplayMode(const PDFDocumentLayout::DisplayMode& displayMode) const
{
    switch (displayMode) {
    case PDFDocumentLayout::DisplayMode::SinglePageDiscrete: return ContextMenuItemTag::SinglePage;
    case PDFDocumentLayout::DisplayMode::SinglePageContinuous: return ContextMenuItemTag::SinglePageContinuous;
    case PDFDocumentLayout::DisplayMode::TwoUpDiscrete: return ContextMenuItemTag::TwoPages;
    case PDFDocumentLayout::DisplayMode::TwoUpContinuous: return ContextMenuItemTag::TwoPagesContinuous;
    }
}

PDFDocumentLayout::DisplayMode UnifiedPDFPlugin::displayModeFromContextMenuItemTag(const ContextMenuItemTag& tag) const
{
    switch (tag) {
    case ContextMenuItemTag::SinglePage: return PDFDocumentLayout::DisplayMode::SinglePageDiscrete;
    case ContextMenuItemTag::SinglePageContinuous: return PDFDocumentLayout::DisplayMode::SinglePageContinuous;
    case ContextMenuItemTag::TwoPages: return PDFDocumentLayout::DisplayMode::TwoUpDiscrete;
    case ContextMenuItemTag::TwoPagesContinuous: return PDFDocumentLayout::DisplayMode::TwoUpContinuous;
    default:
        ASSERT_NOT_REACHED();
        return PDFDocumentLayout::DisplayMode::SinglePageContinuous;
    }
}

auto UnifiedPDFPlugin::toContextMenuItemTag(int tagValue) const -> ContextMenuItemTag
{
    static constexpr std::array regularContextMenuItemTags {
        ContextMenuItemTag::AutoSize,
        ContextMenuItemTag::WebSearch,
        ContextMenuItemTag::DictionaryLookup,
        ContextMenuItemTag::Copy,
        ContextMenuItemTag::CopyLink,
        ContextMenuItemTag::NextPage,
        ContextMenuItemTag::OpenWithPreview,
        ContextMenuItemTag::PreviousPage,
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

std::optional<PDFContextMenu> UnifiedPDFPlugin::createContextMenu(const WebMouseEvent& contextMenuEvent) const
{
    ASSERT(isContextMenuEvent(contextMenuEvent));

    if (!m_frame || !m_frame->coreLocalFrame())
        return std::nullopt;

    RefPtr frameView = m_frame->coreLocalFrame()->view();
    if (!frameView)
        return std::nullopt;

    auto contextMenuEventRootViewPoint = contextMenuEvent.position();

    Vector<PDFContextMenuItem> menuItems;

    auto addSeparator = [item = separatorContextMenuItem(), &menuItems] {
        menuItems.append(item);
    };

    if ([m_pdfDocument allowsCopying] && m_currentSelection) {
        menuItems.appendVector(selectionContextMenuItems(contextMenuEventRootViewPoint));
        addSeparator();
    }

    menuItems.append(contextMenuItem(ContextMenuItemTag::OpenWithPreview));

    addSeparator();

    menuItems.appendVector(scaleContextMenuItems());

    addSeparator();

    menuItems.appendVector(displayModeContextMenuItems());

    addSeparator();

    auto contextMenuEventPluginPoint = convertFromRootViewToPlugin(contextMenuEventRootViewPoint);
    auto contextMenuEventDocumentPoint = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, contextMenuEventPluginPoint);
    menuItems.appendVector(navigationContextMenuItemsForPageAtIndex(m_documentLayout.nearestPageIndexForDocumentPoint(contextMenuEventDocumentPoint)));

    auto contextMenuPoint = frameView->contentsToScreen(IntRect(frameView->windowToContents(contextMenuEventRootViewPoint), IntSize())).location();

    return PDFContextMenu { contextMenuPoint, WTFMove(menuItems), { enumToUnderlyingType(ContextMenuItemTag::OpenWithPreview) } };
}

bool UnifiedPDFPlugin::isDisplayModeContextMenuItemTag(ContextMenuItemTag tag) const
{
    return tag == ContextMenuItemTag::SinglePage || tag == ContextMenuItemTag::SinglePageContinuous || tag == ContextMenuItemTag::TwoPages || tag == ContextMenuItemTag::TwoPagesContinuous;
}

String UnifiedPDFPlugin::titleForContextMenuItemTag(ContextMenuItemTag tag) const
{
    switch (tag) {
    case ContextMenuItemTag::Invalid:
        return { };
    case ContextMenuItemTag::AutoSize:
        return contextMenuItemPDFAutoSize();
    case ContextMenuItemTag::WebSearch:
        return contextMenuItemTagSearchWeb();
    case ContextMenuItemTag::DictionaryLookup:
        return contextMenuItemTagLookUpInDictionary(selectionString());
    case ContextMenuItemTag::Copy:
        return contextMenuItemTagCopy();
    case ContextMenuItemTag::CopyLink:
        return contextMenuItemTagCopyLinkToClipboard();
    case ContextMenuItemTag::NextPage:
        return contextMenuItemPDFNextPage();
    case ContextMenuItemTag::OpenWithPreview:
        return contextMenuItemPDFOpenWithPreview();
    case ContextMenuItemTag::PreviousPage:
        return contextMenuItemPDFPreviousPage();
    case ContextMenuItemTag::SinglePage:
        return contextMenuItemPDFSinglePage();
    case ContextMenuItemTag::SinglePageContinuous:
        return contextMenuItemPDFSinglePageContinuous();
    case ContextMenuItemTag::TwoPages:
        return contextMenuItemPDFTwoPages();
    case ContextMenuItemTag::TwoPagesContinuous:
        return contextMenuItemPDFTwoPagesContinuous();
    case ContextMenuItemTag::ZoomIn:
        return contextMenuItemPDFZoomIn();
    case ContextMenuItemTag::ZoomOut:
        return contextMenuItemPDFZoomOut();
    case ContextMenuItemTag::ActualSize:
        return contextMenuItemPDFActualSize();
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

PDFContextMenuItem UnifiedPDFPlugin::contextMenuItem(ContextMenuItemTag tag, bool hasAction) const
{
    switch (tag) {
    case ContextMenuItemTag::Unknown:
    case ContextMenuItemTag::Invalid:
        return separatorContextMenuItem();
    default: {
        int state = 0;

        if (isDisplayModeContextMenuItemTag(tag)) {
            auto currentDisplayMode = contextMenuItemTagFromDisplayMode(m_documentLayout.displayMode());
            state = currentDisplayMode == tag;
        } else if (tag == ContextMenuItemTag::AutoSize)
            state = m_shouldUpdateAutoSizeScale == ShouldUpdateAutoSizeScale::Yes;

        return { titleForContextMenuItemTag(tag), state, enumToUnderlyingType(tag), ContextMenuItemEnablement::Enabled, hasAction ? ContextMenuItemHasAction::Yes : ContextMenuItemHasAction::No, ContextMenuItemIsSeparator::No };
    }
    }
}

PDFContextMenuItem UnifiedPDFPlugin::separatorContextMenuItem() const
{
    return { { }, 0, enumToUnderlyingType(ContextMenuItemTag::Invalid), ContextMenuItemEnablement::Disabled, ContextMenuItemHasAction::No, ContextMenuItemIsSeparator::Yes };
}

Vector<PDFContextMenuItem> UnifiedPDFPlugin::selectionContextMenuItems(const IntPoint& contextMenuEventRootViewPoint) const
{
    if (![m_pdfDocument allowsCopying] || !m_currentSelection || [m_currentSelection isEmpty])
        return { };

    Vector<PDFContextMenuItem> items {
        contextMenuItem(ContextMenuItemTag::WebSearch),
        separatorContextMenuItem(),
        contextMenuItem(ContextMenuItemTag::DictionaryLookup),
        separatorContextMenuItem(),
        contextMenuItem(ContextMenuItemTag::Copy),
    };

    if (RetainPtr annotation = annotationForRootViewPoint(contextMenuEventRootViewPoint); annotation && annotationIsExternalLink(annotation.get()))
        items.append(contextMenuItem(ContextMenuItemTag::CopyLink));

    return items;
}

Vector<PDFContextMenuItem> UnifiedPDFPlugin::displayModeContextMenuItems() const
{
    return {
        contextMenuItem(ContextMenuItemTag::SinglePage),
        contextMenuItem(ContextMenuItemTag::SinglePageContinuous),
        contextMenuItem(ContextMenuItemTag::TwoPages),
        contextMenuItem(ContextMenuItemTag::TwoPagesContinuous),
    };
}

Vector<PDFContextMenuItem> UnifiedPDFPlugin::scaleContextMenuItems() const
{
    return {
        { contextMenuItem(ContextMenuItemTag::AutoSize) },
        contextMenuItem(ContextMenuItemTag::ZoomIn),
        contextMenuItem(ContextMenuItemTag::ZoomOut),
        contextMenuItem(ContextMenuItemTag::ActualSize),
    };
}

Vector<PDFContextMenuItem> UnifiedPDFPlugin::navigationContextMenuItemsForPageAtIndex(PDFDocumentLayout::PageIndex pageIndex) const
{
    auto pageIncrement = m_documentLayout.pagesPerRow();
    auto effectiveLastPageIndex = [pageCount = m_documentLayout.pageCount(), pageIncrement] {
        if (pageCount % 2)
            return pageCount - 1;
        return pageCount < pageIncrement ? 0 : pageCount - pageIncrement;
    }();

    return {
        contextMenuItem(ContextMenuItemTag::NextPage, pageIndex < effectiveLastPageIndex),
        contextMenuItem(ContextMenuItemTag::PreviousPage, pageIndex > pageIncrement - 1)
    };
}

void UnifiedPDFPlugin::performContextMenuAction(ContextMenuItemTag tag, const IntPoint& contextMenuEventRootViewPoint)
{
    switch (tag) {
    case ContextMenuItemTag::AutoSize:
        if (m_shouldUpdateAutoSizeScale == ShouldUpdateAutoSizeScale::No) {
            m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::Yes;
            setScaleFactor(1.0);
            updateLayout();
        } else
            m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;
        break;
    case ContextMenuItemTag::WebSearch:
        performWebSearch(selectionString());
        break;
    case ContextMenuItemTag::DictionaryLookup: {
        RetainPtr selection = m_currentSelection;
        showDefinitionForSelection(selection.get());
        break;
    } case ContextMenuItemTag::Copy:
        performCopyEditingOperation();
        break;
    case ContextMenuItemTag::CopyLink:
        performCopyLinkOperation(contextMenuEventRootViewPoint);
        break;
    // The OpenWithPreview action is handled in the UI Process.
    case ContextMenuItemTag::OpenWithPreview: return;
    case ContextMenuItemTag::SinglePage:
    case ContextMenuItemTag::SinglePageContinuous:
    case ContextMenuItemTag::TwoPagesContinuous:
    case ContextMenuItemTag::TwoPages:
        if (tag != contextMenuItemTagFromDisplayMode(m_documentLayout.displayMode())) {
            // FIXME: Scroll to the first page that was visible after the layout.
            setDisplayModeAndUpdateLayout(displayModeFromContextMenuItemTag(tag));
        }
        break;
    case ContextMenuItemTag::NextPage: {
        auto currentPageIndex = indexForCurrentPageInView();
        auto pageCount = m_documentLayout.pageCount();
        auto pagesPerRow = m_documentLayout.pagesPerRow();

        auto nextPageIsOnNextRow = [currentPageIndex, &documentLayout = m_documentLayout] {
            if (documentLayout.isSinglePageDisplayMode())
                return true;
            return documentLayout.isRightPageIndex(currentPageIndex);
        };

        if (!m_documentLayout.isLastPageIndex(currentPageIndex) && nextPageIsOnNextRow())
            scrollToPage(currentPageIndex + 1);
        else if (pageCount > pagesPerRow && currentPageIndex < pageCount - pagesPerRow)
            scrollToPage(currentPageIndex + pagesPerRow);
        break;
    }
    case ContextMenuItemTag::PreviousPage: {
        auto currentPageIndex = indexForCurrentPageInView();
        auto pagesPerRow = m_documentLayout.pagesPerRow();

        auto previousPageIsOnPreviousRow = [currentPageIndex, &documentLayout = m_documentLayout]  {
            if (documentLayout.isSinglePageDisplayMode())
                return true;
            return documentLayout.isLeftPageIndex(currentPageIndex);
        };

        if (currentPageIndex && previousPageIsOnPreviousRow())
            scrollToPage(currentPageIndex - 1);
        else if (currentPageIndex > 1)
            scrollToPage(currentPageIndex - pagesPerRow);
        break;
    }
    case ContextMenuItemTag::ZoomIn:
        zoomIn();
        break;
    case ContextMenuItemTag::ZoomOut:
        zoomOut();
        break;
    case ContextMenuItemTag::ActualSize:
        setScaleFactor(scaleForActualSize());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}
#endif // ENABLE(CONTEXT_MENUS)

void UnifiedPDFPlugin::performCopyLinkOperation(const IntPoint& contextMenuEventRootViewPoint) const
{
    if (![m_pdfDocument allowsCopying]) {
        [[NSNotificationCenter defaultCenter] postNotificationName:get_PDFKit_PDFViewCopyPermissionNotification() object:nil];
        return;
    }

    RetainPtr annotation = annotationForRootViewPoint(contextMenuEventRootViewPoint);
    if (!annotation)
        return;

    if (!annotationIsExternalLink(annotation.get()))
        return;

    RetainPtr url = [annotation URL];

    if (!url)
        return;

#if PLATFORM(MAC)
    NSString *urlAbsoluteString = [url absoluteString];
    NSArray *types = @[ NSPasteboardTypeString, NSPasteboardTypeHTML ];
    NSArray *items = @[ [urlAbsoluteString dataUsingEncoding:NSUTF8StringEncoding], [urlAbsoluteString dataUsingEncoding:NSUTF8StringEncoding] ];

    writeItemsToPasteboard(NSPasteboardNameGeneral, items, types);
#else
    // FIXME: Implement.
#endif
}

#pragma mark Editing Commands

bool UnifiedPDFPlugin::handleEditingCommand(const String& commandName, const String& argument)
{
    if (commandName == "ScrollPageBackward"_s || commandName == "ScrollPageForward"_s)
        return forwardEditingCommandToEditor(commandName, argument);

    if (commandName == "copy"_s)
        return performCopyEditingOperation();

    if (commandName == "selectAll"_s) {
        selectAll();
        return true;
    }

    if (commandName == "takeFindStringFromSelection"_s)
        return takeFindStringFromSelection();

    return false;
}

bool UnifiedPDFPlugin::isEditingCommandEnabled(const String& commandName)
{
    if (commandName == "ScrollPageBackward"_s || commandName == "ScrollPageForward"_s)
        return true;

    if (commandName == "selectAll"_s)
        return true;

    if (commandName == "copy"_s || commandName == "takeFindStringFromSelection"_s)
        return m_currentSelection;

    return false;
}

bool UnifiedPDFPlugin::performCopyEditingOperation() const
{
#if PLATFORM(MAC)
    if (!m_currentSelection)
        return false;

    if (![m_pdfDocument allowsCopying]) {
        [[NSNotificationCenter defaultCenter] postNotificationName:get_PDFKit_PDFViewCopyPermissionNotification() object:nil];
        return false;
    }

    NSString *plainString = [m_currentSelection string];
    NSString *htmlString = [m_currentSelection html];
    NSData *webArchiveData = [m_currentSelection webArchive];

    NSArray *types = @[ PasteboardTypes::WebArchivePboardType, NSPasteboardTypeString, NSPasteboardTypeHTML ];

    NSArray *items = @[ webArchiveData, [plainString dataUsingEncoding:NSUTF8StringEncoding], [htmlString dataUsingEncoding:NSUTF8StringEncoding] ];

    writeItemsToPasteboard(NSPasteboardNameGeneral, items, types);
    return true;
#endif
    return false;
}


bool UnifiedPDFPlugin::takeFindStringFromSelection()
{
    if (!m_currentSelection)
        return false;

    String findString { m_currentSelection.get().string };
    if (findString.isEmpty())
        return false;

#if PLATFORM(MAC)
    writeItemsToPasteboard(NSPasteboardNameFind, @[ [nsStringNilIfEmpty(findString) dataUsingEncoding:NSUTF8StringEncoding] ], @[ NSPasteboardTypeString ]);
#else
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;

    if (CheckedPtr client = m_frame->coreLocalFrame()->checkedEditor()->client())
        client->updateStringForFind(findString);
    else
        return false;
#endif

    return true;
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

    RetainPtr selection = [m_pdfDocument selectionFromPage:firstPageOfCurrentSelection atPoint:m_selectionTrackingData.startPagePoint toPage:m_documentLayout.pageAtIndex(oldStartPageIndex).get() atPoint:oldStartPagePoint];
    [selection addSelection:m_selectionTrackingData.selectionToExtendWith.get()];
    setCurrentSelection(WTFMove(selection));
}

void UnifiedPDFPlugin::beginTrackingSelection(PDFDocumentLayout::PageIndex pageIndex, const WebCore::FloatPoint& pagePoint, const WebMouseEvent& event)
{
    auto modifiers = event.modifiers();

    m_selectionTrackingData.isActivelyTrackingSelection = true;
    m_selectionTrackingData.granularity = selectionGranularityForMouseEvent(event);
    m_selectionTrackingData.startPageIndex = pageIndex;
    m_selectionTrackingData.startPagePoint = pagePoint;
    m_selectionTrackingData.marqueeSelectionRect = { };
    m_selectionTrackingData.shouldMakeMarqueeSelection = modifiers.contains(WebEventModifier::AltKey);
    m_selectionTrackingData.shouldExtendCurrentSelection = modifiers.contains(WebEventModifier::ShiftKey);
    m_selectionTrackingData.selectionToExtendWith = nullptr;

    // Context menu events can only generate a word selection under the event, so we bail out of the rest of our selection tracking logic.
    if (isContextMenuEvent(event))
        return updateCurrentSelectionForContextMenuEventIfNeeded();

    if (m_selectionTrackingData.shouldExtendCurrentSelection)
        extendCurrentSelectionIfNeeded();

    continueTrackingSelection(pageIndex, pagePoint, IsDraggingSelection::No);
}

void UnifiedPDFPlugin::updateCurrentSelectionForContextMenuEventIfNeeded()
{
    auto page = m_documentLayout.pageAtIndex(m_selectionTrackingData.startPageIndex);
    if (!m_currentSelection || !(FloatRect([m_currentSelection boundsForPage:page.get()]).contains(m_selectionTrackingData.startPagePoint)))
        setCurrentSelection([page selectionForWordAtPoint:m_selectionTrackingData.startPagePoint]);
}

static FloatRect computeMarqueeSelectionRect(const WebCore::FloatPoint& point1, const WebCore::FloatPoint& point2)
{
    auto marqueeRectLocation = point1.shrunkTo(point2);
    auto marqueeRectSize = FloatSize { point1 - point2 };
    return { marqueeRectLocation.x(), marqueeRectLocation.y(), std::abs(marqueeRectSize.width()), std::abs(marqueeRectSize.height()) };
}

void UnifiedPDFPlugin::freezeCursorDuringSelectionDragIfNeeded(IsDraggingSelection isDraggingSelection)
{
    if (isDraggingSelection == IsDraggingSelection::Yes && !std::exchange(m_selectionTrackingData.cursorIsFrozenForSelectionDrag, true))
        notifyCursorChanged(PlatformCursorType::IBeam);
}

void UnifiedPDFPlugin::unfreezeCursorAfterSelectionDragIfNeeded()
{
    if (std::exchange(m_selectionTrackingData.cursorIsFrozenForSelectionDrag, false) && m_lastMouseEvent) {
        auto altKeyIsActive = m_lastMouseEvent->altKey() ? AltKeyIsActive::Yes : AltKeyIsActive::No;
        auto pdfElementTypes = pdfElementTypesForPluginPoint(lastKnownMousePositionInView());
        notifyCursorChanged(toWebCoreCursorType(pdfElementTypes, altKeyIsActive));
    }
}

void UnifiedPDFPlugin::continueTrackingSelection(PDFDocumentLayout::PageIndex pageIndex, const WebCore::FloatPoint& pagePoint, IsDraggingSelection isDraggingSelection)
{
    freezeCursorDuringSelectionDragIfNeeded(isDraggingSelection);

    auto beginAutoscrollIfNecessary = makeScopeExit([&] {
        if (isDraggingSelection == IsDraggingSelection::Yes)
            beginAutoscroll();
    });

    if (m_selectionTrackingData.shouldMakeMarqueeSelection) {
        if (m_selectionTrackingData.startPageIndex != pageIndex)
            return;

        m_selectionTrackingData.marqueeSelectionRect = computeMarqueeSelectionRect(pagePoint, m_selectionTrackingData.startPagePoint);
        auto page = m_documentLayout.pageAtIndex(pageIndex);
        return setCurrentSelection([page selectionForRect:m_selectionTrackingData.marqueeSelectionRect]);
    }

    auto fromPage = m_documentLayout.pageAtIndex(m_selectionTrackingData.startPageIndex);
    auto toPage = m_documentLayout.pageAtIndex(pageIndex);

    RetainPtr<PDFSelection> selection;

#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)
    auto toPDFSelectionGranularity = [](SelectionGranularity granularity) {
        switch (granularity) {
        case SelectionGranularity::Character:
            return (PDFSelectionGranularity)PDFSelectionGranularityCharacter;
        case SelectionGranularity::Word:
            return (PDFSelectionGranularity)PDFSelectionGranularityWord;
        case SelectionGranularity::Line:
            return (PDFSelectionGranularity)PDFSelectionGranularityLine;
        }
        ASSERT_NOT_REACHED();
        return (PDFSelectionGranularity)PDFSelectionGranularityCharacter;
    };

    if ([m_pdfDocument respondsToSelector:@selector(selectionFromPage:atPoint:toPage:atPoint:withGranularity:)])
        selection = [m_pdfDocument selectionFromPage:fromPage.get() atPoint:m_selectionTrackingData.startPagePoint toPage:toPage.get() atPoint:pagePoint withGranularity:toPDFSelectionGranularity(m_selectionTrackingData.granularity)];
    else
#endif
        selection = [m_pdfDocument selectionFromPage:fromPage.get() atPoint:m_selectionTrackingData.startPagePoint toPage:toPage.get() atPoint:pagePoint];

    if (m_selectionTrackingData.granularity == SelectionGranularity::Character && m_selectionTrackingData.shouldExtendCurrentSelection)
        [selection addSelection:m_selectionTrackingData.selectionToExtendWith.get()];

    setCurrentSelection(WTFMove(selection));
}

void UnifiedPDFPlugin::stopTrackingSelection()
{
    m_selectionTrackingData.selectionToExtendWith = nullptr;
    m_selectionTrackingData.isActivelyTrackingSelection = false;
    unfreezeCursorAfterSelectionDragIfNeeded();
}

Vector<FloatRect> UnifiedPDFPlugin::boundsForSelection(const PDFSelection *selection, CoordinateSpace inSpace) const
{
    if (!selection || [selection isEmpty])
        return { };

    return makeVector([selection pages], [this, selection, inSpace](PDFPage *page) -> std::optional<FloatRect> {
        auto pageIndex = m_documentLayout.indexForPage(page);
        if (!pageIndex)
            return { };
        return convertUp(CoordinateSpace::PDFPage, inSpace, FloatRect { [selection boundsForPage:page] }, *pageIndex);
    });
}

void UnifiedPDFPlugin::repaintOnSelectionActiveStateChangeIfNeeded(ActiveStateChangeReason reason, const Vector<FloatRect>& additionalDocumentRectsToRepaint)
{
    switch (reason) {
    case ActiveStateChangeReason::WindowActivityChanged:
        if (!m_currentSelection || [m_currentSelection isEmpty])
            return;
        break;
    case ActiveStateChangeReason::SetCurrentSelection:
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    auto selectionDocumentRectsToRepaint = boundsForSelection(protectedCurrentSelection().get(), CoordinateSpace::PDFDocumentLayout);
    selectionDocumentRectsToRepaint.appendVector(additionalDocumentRectsToRepaint);

    setNeedsRepaintInDocumentRects(RepaintRequirement::Selection, selectionDocumentRectsToRepaint);
}

RetainPtr<PDFSelection> UnifiedPDFPlugin::protectedCurrentSelection() const
{
    return m_currentSelection;
}

void UnifiedPDFPlugin::setCurrentSelection(RetainPtr<PDFSelection>&& selection)
{
    auto staleSelectionDocumentRectsToRepaint = boundsForSelection(protectedCurrentSelection().get(), CoordinateSpace::PDFDocumentLayout);

    m_currentSelection = WTFMove(selection);
    // FIXME: <https://webkit.org/b/268980> Selection painting requests should be only be made if the current selection has changed.
    // FIXME: <https://webkit.org/b/270070> Selection painting should be optimized by only repainting diff between old and new selection.
    repaintOnSelectionActiveStateChangeIfNeeded(ActiveStateChangeReason::SetCurrentSelection, staleSelectionDocumentRectsToRepaint);
    notifySelectionChanged();
}

String UnifiedPDFPlugin::selectionString() const
{
    if (!m_currentSelection)
        return { };
    return m_currentSelection.get().string;
}

bool UnifiedPDFPlugin::existingSelectionContainsPoint(const FloatPoint& rootViewPoint) const
{
    auto pluginPoint = convertFromRootViewToPlugin(roundedIntPoint(rootViewPoint));
    auto documentPoint = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { pluginPoint });
    auto pageIndex = pageIndexForDocumentPoint(documentPoint);
    if (!pageIndex)
        return false;

    RetainPtr page = m_documentLayout.pageAtIndex(*pageIndex);
    auto pagePoint = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, documentPoint, *pageIndex);
    return FloatRect { [m_currentSelection boundsForPage:page.get()] }.contains(pagePoint);
}

FloatRect UnifiedPDFPlugin::rectForSelectionInMainFrameContentsSpace(PDFSelection *selection) const
{
    RefPtr mainFrameView = this->mainFrameView();
    if (!mainFrameView)
        return { };

    if (!m_frame->coreLocalFrame())
        return { };

    RefPtr localFrameView = m_frame->coreLocalFrame()->view();
    if (!localFrameView)
        return { };

    auto rectForSelectionInRootView = this->rectForSelectionInRootView(selection);
    auto rectForSelectionInContents = localFrameView->rootViewToContents(rectForSelectionInRootView);
    return mainFrameView->windowToContents(localFrameView->contentsToWindow(roundedIntRect(rectForSelectionInContents)));
}

FloatRect UnifiedPDFPlugin::rectForSelectionInRootView(PDFSelection *selection) const
{
    if (!selection || !selection.pages)
        return { };

    RetainPtr page = [selection.pages firstObject];
    auto pageIndex = m_documentLayout.indexForPage(page);
    if (!pageIndex)
        return { };

    auto pluginRect = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Plugin, FloatRect { [selection boundsForPage:page.get()] }, *pageIndex);
    return convertFromPluginToRootView(enclosingIntRect(pluginRect));
}

#pragma mark Autoscroll

void UnifiedPDFPlugin::beginAutoscroll()
{
    if (!std::exchange(m_inActiveAutoscroll, true))
        m_autoscrollTimer.startRepeating(WebCore::autoscrollInterval);
}

void UnifiedPDFPlugin::autoscrollTimerFired()
{
    if (!m_inActiveAutoscroll)
        return m_autoscrollTimer.stop();

    continueAutoscroll();
}

void UnifiedPDFPlugin::continueAutoscroll()
{
    if (!m_inActiveAutoscroll || !m_currentSelection || [m_currentSelection isEmpty])
        return;

    auto lastKnownMousePositionInPluginSpace = lastKnownMousePositionInView();

    // FIXME: If the window is on a screen boundary, the user can't drag-scroll with this delta. Implement something like autoscrollAdjustmentFactorForScreenBoundaries.
    auto scrollDelta = [&lastKnownMousePositionInPluginSpace, pluginBounds = FloatRect { { }, size() }]() -> IntSize {
        auto scrollDeltaLength = [](auto position, auto limit) -> int {
            if (position > limit)
                return position - limit;
            return std::min(position, 0);
        };

        int scrollDeltaHeight = scrollDeltaLength(lastKnownMousePositionInPluginSpace.y(), pluginBounds.height());
        int scrollDeltaWidth = scrollDeltaLength(lastKnownMousePositionInPluginSpace.x(), pluginBounds.width());

        return { scrollDeltaWidth, scrollDeltaHeight };
    }();

    if (scrollDelta.isZero())
        return;

    scrollWithDelta(scrollDelta);

    auto lastKnownMousePositionInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, lastKnownMousePositionInPluginSpace);
    auto pageIndex = m_documentLayout.nearestPageIndexForDocumentPoint(lastKnownMousePositionInDocumentSpace);
    auto lastKnownMousePositionInPageSpace = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, lastKnownMousePositionInDocumentSpace, pageIndex);

    continueTrackingSelection(pageIndex, lastKnownMousePositionInPageSpace, IsDraggingSelection::Yes);
}

void UnifiedPDFPlugin::stopAutoscroll()
{
    m_inActiveAutoscroll = false;
}

void UnifiedPDFPlugin::scrollWithDelta(const IntSize& scrollDelta)
{
    if (isLocked())
        return;

    // FIXME: For discrete page modes, should we snap to the next/previous page immediately?

    setScrollOffset(constrainedScrollPosition(ScrollPosition { m_scrollOffset + scrollDelta }));
    scrollToPointInContentsSpace(scrollPosition());
}

#pragma mark -

unsigned UnifiedPDFPlugin::countFindMatches(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    // FIXME: Why is it OK to ignore the passed-in maximum match count?
    if (!target.length())
        return 0;

    NSStringCompareOptions nsOptions = options.contains(FindOption::CaseInsensitive) ? NSCaseInsensitiveSearch : 0;
    return [[m_pdfDocument findString:target withOptions:nsOptions] count];
}

static NSStringCompareOptions compareOptionsForFindOptions(WebCore::FindOptions options)
{
    bool searchForward = !options.contains(FindOption::Backwards);
    bool isCaseSensitive = !options.contains(FindOption::CaseInsensitive);

    NSStringCompareOptions compareOptions = 0;
    if (!searchForward)
        compareOptions |= NSBackwardsSearch;
    if (!isCaseSensitive)
        compareOptions |= NSCaseInsensitiveSearch;

    return compareOptions;
}

bool UnifiedPDFPlugin::findString(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (target.isEmpty()) {
        m_lastFindString = target;
        setCurrentSelection(nullptr);
        m_findMatchRectsInDocumentCoordinates.clear();
        return false;
    }

    if (options.contains(FindOption::DoNotSetSelection)) {
        // If the max was zero, any result means we exceeded the max, so we can skip computing the actual count.
        // FIXME: How can always returning true without searching if passed a max of 0 be right?
        // Even if it is right, why not put that special case inside countFindMatches instead of here?
        return !target.isEmpty() && (!maxMatchCount || countFindMatches(target, options, maxMatchCount));
    }

    bool wrapSearch = options.contains(FindOption::WrapAround);
    auto compareOptions = compareOptionsForFindOptions(options);

    auto nextMatchForString = [&]() -> RetainPtr<PDFSelection> {
        if (!target.length())
            return nullptr;
        RetainPtr foundSelection = [m_pdfDocument findString:target fromSelection:m_currentSelection.get() withOptions:compareOptions];
        if (!foundSelection && wrapSearch) {
            auto emptySelection = adoptNS([allocPDFSelectionInstance() initWithDocument:m_pdfDocument.get()]);
            foundSelection = [m_pdfDocument findString:target fromSelection:emptySelection.get() withOptions:compareOptions];
        }
        return foundSelection;
    };

    if (m_lastFindString != target) {
        setCurrentSelection(nullptr);
        m_lastFindString = target;

        collectFindMatchRects(target, options);
    }

    RetainPtr selection = nextMatchForString();
    if (!selection)
        return false;

    RetainPtr firstPageForSelection = [[selection pages] firstObject];
    if (!firstPageForSelection)
        return false;

    auto firstPageIndex = m_documentLayout.indexForPage(firstPageForSelection);
    if (!firstPageIndex)
        return false;

    auto selectionBoundsInContentsCoordinates = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::ScrolledContents, FloatRect { [selection boundsForPage:firstPageForSelection.get()] }, *firstPageIndex);
    revealRectInContentsSpace(selectionBoundsInContentsCoordinates);

    setCurrentSelection(WTFMove(selection));
    return true;
}

void UnifiedPDFPlugin::collectFindMatchRects(const String& target, WebCore::FindOptions options)
{
    m_findMatchRectsInDocumentCoordinates.clear();

    RetainPtr foundSelections = [m_pdfDocument findString:target withOptions:compareOptionsForFindOptions(options)];
    for (PDFSelection *selection in foundSelections.get()) {
        for (PDFPage *page in selection.pages) {
            auto pageIndex = m_documentLayout.indexForPage(page);
            if (!pageIndex)
                continue;

            auto bounds = FloatRect { [selection boundsForPage:page] };
            auto boundsInDocumentSpace = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::PDFDocumentLayout, bounds, *pageIndex);
            m_findMatchRectsInDocumentCoordinates.append(boundsInDocumentSpace);
        }
    }

    updateFindOverlay();
}

void UnifiedPDFPlugin::updateFindOverlay(HideFindIndicator hideFindIndicator)
{
    m_frame->protectedPage()->findController().didInvalidateFindRects();

    if (hideFindIndicator == HideFindIndicator::Yes)
        m_frame->protectedPage()->findController().hideFindIndicator();
}

Vector<FloatRect> UnifiedPDFPlugin::rectsForTextMatchesInRect(const IntRect&) const
{
    return m_findMatchRectsInDocumentCoordinates.map([&](FloatRect rect) {
        return convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::Plugin, rect);
    });
}

RefPtr<TextIndicator> UnifiedPDFPlugin::textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption> options, WebCore::TextIndicatorPresentationTransition transition)
{
    RetainPtr selection = m_currentSelection;
    return textIndicatorForSelection(selection.get(), options, transition);
}

RefPtr<TextIndicator> UnifiedPDFPlugin::textIndicatorForSelection(PDFSelection *selection, OptionSet<WebCore::TextIndicatorOption> options, WebCore::TextIndicatorPresentationTransition transition)
{
    auto selectionBounds = selectionBoundsForFirstPageInDocumentSpace(selection);
    if (!selectionBounds)
        return nullptr;

    auto rectInDocumentCoordinates = *selectionBounds;
    auto rectInContentsCoordinates = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::Contents, rectInDocumentCoordinates);
    auto rectInPluginCoordinates = convertUp(CoordinateSpace::Contents, CoordinateSpace::Plugin, rectInContentsCoordinates);
    auto rectInRootViewCoordinates = convertFromPluginToRootView(enclosingIntRect(rectInPluginCoordinates));

    float deviceScaleFactor = this->deviceScaleFactor();

    auto buffer = ImageBuffer::create(rectInRootViewCoordinates.size(), RenderingPurpose::ShareableSnapshot, deviceScaleFactor, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, { }, nullptr);
    if (!buffer)
        return nullptr;

    auto& context = buffer->context();

    {
        GraphicsContextStateSaver saver(context);

        context.scale(m_scaleFactor);
        context.translate(-rectInContentsCoordinates.location());

        paintPDFContent(context, rectInContentsCoordinates, PaintingBehavior::PageContentsOnly);
    }

    // FIXME: Figure out how to share this with WebTextIndicatorLayer.
#if PLATFORM(MAC)
    Color highlightColor = *roundAndClampToSRGBALossy([NSColor findHighlightColor].CGColor);
#else
    Color highlightColor = SRGBA<float>(.99, .89, 0.22, 1.0);
#endif
    context.fillRect({ { 0, 0 }, rectInRootViewCoordinates.size() }, highlightColor, CompositeOperator::SourceOver, BlendMode::Multiply);

    TextIndicatorData data;
    data.contentImage = BitmapImage::create(ImageBuffer::sinkIntoNativeImage(WTFMove(buffer)));
    data.contentImageScaleFactor = deviceScaleFactor;
    data.contentImageWithoutSelection = data.contentImage;
    data.contentImageWithoutSelectionRectInRootViewCoordinates = rectInRootViewCoordinates;
    data.selectionRectInRootViewCoordinates = rectInRootViewCoordinates;
    data.textBoundingRectInRootViewCoordinates = rectInRootViewCoordinates;
    data.textRectsInBoundingRectCoordinates = { FloatRect { 0, 0, static_cast<float>(rectInRootViewCoordinates.width()), static_cast<float>(rectInRootViewCoordinates.height()) } };
    data.presentationTransition = transition;
    data.options = options;

    return TextIndicator::create(data);
}

bool UnifiedPDFPlugin::performDictionaryLookupAtLocation(const FloatPoint& rootViewPoint)
{
    auto pluginPoint = convertFromRootViewToPlugin(roundedIntPoint(rootViewPoint));
    auto documentPoint = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { pluginPoint });
    auto pageIndex = pageIndexForDocumentPoint(documentPoint);
    if (!pageIndex)
        return false;

    RetainPtr page = m_documentLayout.pageAtIndex(*pageIndex);
    auto pagePoint = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, documentPoint, *pageIndex);
    RetainPtr lookupSelection = [page selectionForWordAtPoint:pagePoint];
    return showDefinitionForSelection(lookupSelection.get());
}

std::optional<FloatRect> UnifiedPDFPlugin::selectionBoundsForFirstPageInDocumentSpace(const RetainPtr<PDFSelection>& selection) const
{
    if (!selection)
        return { };

    for (PDFPage *page in [selection pages]) {
        auto pageIndex = m_documentLayout.indexForPage(page);
        if (!pageIndex)
            continue;
        auto rectForPage = FloatRect { [selection boundsForPage:page] };
        return convertUp(CoordinateSpace::PDFPage, CoordinateSpace::PDFDocumentLayout, rectForPage, *pageIndex);
    }

    return { };
}

WebCore::DictionaryPopupInfo UnifiedPDFPlugin::dictionaryPopupInfoForSelection(PDFSelection *selection, WebCore::TextIndicatorPresentationTransition transition)
{
    DictionaryPopupInfo dictionaryPopupInfo = PDFPluginBase::dictionaryPopupInfoForSelection(selection, transition);
    if (auto textIndicator = textIndicatorForSelection(selection, { }, transition))
        dictionaryPopupInfo.textIndicator = textIndicator->data();

    return dictionaryPopupInfo;
}

bool UnifiedPDFPlugin::showDefinitionForSelection(PDFSelection *selection)
{
    if (!m_frame || !m_frame->page())
        return false;
    RefPtr page = m_frame->page();

    auto dictionaryPopupInfo = dictionaryPopupInfoForSelection(selection, TextIndicatorPresentationTransition::Bounce);
    page->send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfo));
    return true;
}

std::pair<String, RetainPtr<PDFSelection>> UnifiedPDFPlugin::textForImmediateActionHitTestAtPoint(const WebCore::FloatPoint& rootViewPoint, WebHitTestResultData& data)
{
    if (existingSelectionContainsPoint(rootViewPoint))
        return { selectionString(), m_currentSelection };

    auto pluginPoint = convertFromRootViewToPlugin(roundedIntPoint(rootViewPoint));
    auto documentPoint = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { pluginPoint });
    auto pageIndex = pageIndexForDocumentPoint(documentPoint);

    if (!pageIndex)
        return { { }, m_currentSelection };

    auto pagePoint = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, documentPoint, *pageIndex);
    RetainPtr page = m_documentLayout.pageAtIndex(*pageIndex);
    RetainPtr wordSelection = [page selectionForWordAtPoint:pagePoint];
    if (!wordSelection)
        return { { }, nil };

    RetainPtr annotationsForCurrentPage = [page annotations];
    if (!annotationsForCurrentPage)
        return { { }, nil };

    for (PDFAnnotation *annotation in annotationsForCurrentPage.get()) {
        if (!annotationIsExternalLink(annotation))
            continue;

        if (!FloatRect { [annotation bounds] }.contains(pagePoint))
            continue;

        RetainPtr url = [annotation URL];
        if (!url)
            continue;

        data.absoluteLinkURL = [url absoluteString];
        data.linkLabel = [wordSelection string];
        return { [wordSelection string], wordSelection };
    }

    NSString *lookupText = DictionaryLookup::stringForPDFSelection(wordSelection.get());
    if (!lookupText || !lookupText.length)
        return { { }, wordSelection };

    return { lookupText, wordSelection };
}

#if PLATFORM(MAC)
void UnifiedPDFPlugin::accessibilityScrollToPage(PDFDocumentLayout::PageIndex pageIndex)
{
    scrollToPage(pageIndex);
}

FloatRect UnifiedPDFPlugin::convertFromPDFPageToScreenForAccessibility(const FloatRect& rectInPageCoordinate, PDFDocumentLayout::PageIndex pageIndex) const
{
    return WebCore::Accessibility::retrieveValueFromMainThread<FloatRect>([&]() ->FloatRect {
        auto rectInPluginCoordinates = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Plugin, rectInPageCoordinate, pageIndex);
        rectInPluginCoordinates.setLocation(convertFromPluginToRootView(IntPoint(rectInPluginCoordinates.location())));
        RefPtr page = this->page();
        if (!page)
            return { };
        return page->chrome().rootViewToScreen(enclosingIntRect(rectInPluginCoordinates));
    });
}

id UnifiedPDFPlugin::accessibilityHitTestIntPoint(const WebCore::IntPoint& point) const
{
    return WebCore::Accessibility::retrieveValueFromMainThread<id>([&] () -> id {
        auto floatPoint = FloatPoint { point };
        auto pointInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, floatPoint);
        auto hitPageIndex = pageIndexForDocumentPoint(pointInDocumentSpace);
        if (!hitPageIndex || *hitPageIndex >= m_documentLayout.pageCount())
            return { };
        auto pageIndex = *hitPageIndex;
        auto pointInPDFPageSpace = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex);

        return [m_accessibilityDocumentObject accessibilityHitTest:pointInPDFPageSpace];
    });
}
#endif

id UnifiedPDFPlugin::accessibilityHitTest(const WebCore::IntPoint& point) const
{
#if PLATFORM(MAC)
    return accessibilityHitTestIntPoint(point);
#endif
    UNUSED_PARAM(point);
    return nil;
}

id UnifiedPDFPlugin::accessibilityObject() const
{
#if PLATFORM(MAC)
    return m_accessibilityDocumentObject.get();
#endif
    return nil;
}

#if ENABLE(PDF_HUD)

void UnifiedPDFPlugin::zoomIn()
{
    m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;
    setScaleFactor(std::clamp(m_scaleFactor * zoomIncrement, minimumZoomScale, maximumZoomScale));
}

void UnifiedPDFPlugin::zoomOut()
{
    m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;
    setScaleFactor(std::clamp(m_scaleFactor / zoomIncrement, minimumZoomScale, maximumZoomScale));
}

void UnifiedPDFPlugin::resetZoom()
{
    setScaleFactor(initialScale());
}

#endif // ENABLE(PDF_HUD)

CGRect UnifiedPDFPlugin::pluginBoundsForAnnotation(RetainPtr<PDFAnnotation>& annotation) const
{
    auto pageSpaceBounds = FloatRect { [annotation bounds] };
    if (auto pageIndex = m_documentLayout.indexForPage([annotation page]))
        return convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Plugin, pageSpaceBounds, pageIndex.value());

    ASSERT_NOT_REACHED();
    return pageSpaceBounds;
}

#if PLATFORM(MAC)
static RetainPtr<PDFAnnotation> findFirstTextAnnotationStartingAtIndex(const RetainPtr<NSArray>& annotations, unsigned startingIndex, AnnotationSearchDirection searchDirection)
{
    ASSERT(annotations);
    if (!annotations || startingIndex >= [annotations count])
        return nullptr;

    auto indexRange = [&] {
        if (searchDirection == AnnotationSearchDirection::Forward)
            return [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(startingIndex, [annotations count] - startingIndex)];
        return [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, startingIndex + 1)];
    }();

    auto searchResult = [annotations indexOfObjectAtIndexes:indexRange options:searchDirection == AnnotationSearchDirection::Forward ? 0 : NSEnumerationReverse passingTest:^BOOL(PDFAnnotation* annotation, NSUInteger, BOOL *) {
        return [annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] && ![annotation isReadOnly] && [annotation shouldDisplay];
    }];

    return searchResult != NSNotFound ? [annotations objectAtIndex:searchResult] : nullptr;
}

RetainPtr<PDFAnnotation> UnifiedPDFPlugin::nextTextAnnotation(AnnotationSearchDirection searchDirection) const
{
    ASSERT(m_activeAnnotation);
    RetainPtr currentAnnotation = m_activeAnnotation->annotation();
    RetainPtr currentPage = [currentAnnotation page];
    if (!currentPage)
        return nullptr;

    RetainPtr annotationsForCurrentPage = [currentPage annotations];
    auto indexOfCurrentAnnotation = [annotationsForCurrentPage indexOfObject:currentAnnotation.get()];
    ASSERT(indexOfCurrentAnnotation != NSNotFound);
    if (indexOfCurrentAnnotation == NSNotFound)
        return nullptr;

    bool isForwardSearchDirection = searchDirection == AnnotationSearchDirection::Forward;
    if ((isForwardSearchDirection && indexOfCurrentAnnotation + 1 < [annotationsForCurrentPage count]) || (!isForwardSearchDirection && indexOfCurrentAnnotation)) {
        auto startingIndexForSearch = isForwardSearchDirection ? indexOfCurrentAnnotation + 1 : indexOfCurrentAnnotation - 1;
        if (RetainPtr nextTextAnnotationOnCurrentPage = findFirstTextAnnotationStartingAtIndex(annotationsForCurrentPage, startingIndexForSearch, searchDirection))
            return nextTextAnnotationOnCurrentPage;
    }

    auto indexForCurrentPage = m_documentLayout.indexForPage(currentPage);
    if (!indexForCurrentPage)
        return nullptr;

    RetainPtr<PDFAnnotation> nextAnnotation;
    auto nextPageToSearchIndex = indexForCurrentPage.value();
    while (!nextAnnotation) {
        auto computeNextPageToSearchIndex = [this, isForwardSearchDirection](unsigned currentPageIndex) -> unsigned {
            auto pageCount = m_documentLayout.pageCount();
            if (!isForwardSearchDirection && !currentPageIndex)
                return pageCount - 1;
            return isForwardSearchDirection ? ((currentPageIndex + 1) % pageCount) : currentPageIndex - 1;
        };
        nextPageToSearchIndex = computeNextPageToSearchIndex(nextPageToSearchIndex);
        RetainPtr nextPage = m_documentLayout.pageAtIndex(nextPageToSearchIndex);
        if (!nextPage)
            return nullptr;
        if (RetainPtr nextPageAnnotations = [nextPage annotations]; nextPageAnnotations && [nextPageAnnotations count])
            nextAnnotation = findFirstTextAnnotationStartingAtIndex(nextPageAnnotations, isForwardSearchDirection ? 0 : [nextPageAnnotations count] - 1, searchDirection);
    }
    return nextAnnotation;
}
#endif

void UnifiedPDFPlugin::focusNextAnnotation()
{
#if PLATFORM(MAC)
    if (!m_activeAnnotation)
        return;
    RetainPtr nextTextAnnotation = this->nextTextAnnotation(AnnotationSearchDirection::Forward);
    if (!nextTextAnnotation || nextTextAnnotation == m_activeAnnotation->annotation())
        return;
    setActiveAnnotation(WTFMove(nextTextAnnotation));
#endif
}

void UnifiedPDFPlugin::focusPreviousAnnotation()
{
#if PLATFORM(MAC)
    if (!m_activeAnnotation)
        return;
    RetainPtr previousTextAnnotation = this->nextTextAnnotation(AnnotationSearchDirection::Backward);
    if (!previousTextAnnotation || previousTextAnnotation == m_activeAnnotation->annotation())
        return;
    setActiveAnnotation(WTFMove(previousTextAnnotation));
#endif
}

void UnifiedPDFPlugin::setActiveAnnotation(RetainPtr<PDFAnnotation>&& annotation)
{
#if PLATFORM(MAC)
    if (!supportsForms())
        return;

    if (m_activeAnnotation) {
        m_activeAnnotation->commit();

        auto annotationRect = documentRectForAnnotation(m_activeAnnotation->annotation());
        setNeedsRepaintInDocumentRect(repaintRequirementsForAnnotation(m_activeAnnotation->annotation(), IsAnnotationCommit::Yes), annotationRect);
    }

    if (annotation) {
        if ([annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] && [annotation isReadOnly]) {
            m_activeAnnotation = nullptr;
            return;
        }

        m_activeAnnotation = PDFPluginAnnotation::create(annotation.get(), this);
        protectedActiveAnnotation()->attach(m_annotationContainer.get());

        auto newAnnotationRectInContentsCoordinates = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::ScrolledContents, documentRectForAnnotation(protectedActiveAnnotation()->annotation()));
        revealRectInContentsSpace(newAnnotationRectInContentsCoordinates);
    } else
        m_activeAnnotation = nullptr;
#endif
}

#if PLATFORM(MAC)
void UnifiedPDFPlugin::handlePDFActionForAnnotation(PDFAnnotation *annotation, unsigned currentPageIndex)
{
    if (!annotation)
        return;

    RetainPtr firstAction = [annotation action];
    ASSERT(firstAction);
    if (!firstAction)
        return;

    using PDFActionList = Vector<RetainPtr<PDFAction>>;
    auto performPDFAction = [this, currentPageIndex, annotation](PDFAction *action) {
        if (!action)
            return;

        if ([action isKindOfClass:getPDFActionResetFormClass()] && [m_pdfDocument respondsToSelector:@selector(resetFormFields:)])
            [m_pdfDocument resetFormFields:static_cast<PDFActionResetForm *>(action)];

        RetainPtr actionType = [action type];
        if ([actionType isEqualToString:@"Named"]) {
            auto actionName = [static_cast<PDFActionNamed *>(action) name];
            switch (actionName) {
            case kPDFActionNamedNextPage:
                if (currentPageIndex + 1 < m_documentLayout.pageCount())
                    scrollToPage(currentPageIndex + 1);
                break;
            case kPDFActionNamedPreviousPage:
                if (currentPageIndex)
                    scrollToPage(currentPageIndex - 1);
                break;
            case kPDFActionNamedFirstPage:
                scrollToPage(0);
                break;
            case kPDFActionNamedLastPage:
                scrollToPage(m_documentLayout.pageCount() - 1);
                break;
            case kPDFActionNamedZoomIn:
                zoomIn();
                break;
            case kPDFActionNamedZoomOut:
                zoomOut();
                break;
            case kPDFActionNamedPrint:
                print();
                break;
            default:
                LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin: unhandled action " << actionName);
                break;
            }
        } else if ([actionType isEqualToString:@"GoTo"])
            scrollToPDFDestination([annotation destination]);
    };

    PDFActionList actionsForAnnotation;
    actionsForAnnotation.append(firstAction);
    while (!actionsForAnnotation.isEmpty()) {
        RetainPtr currentAction = actionsForAnnotation.takeLast().get();
        performPDFAction(currentAction.get());

#if HAVE(PDFKIT_WITH_NEXT_ACTIONS)
        if ([currentAction respondsToSelector:@selector(nextActions)]) {
            RetainPtr reversedNextActions = [[currentAction nextActions] reverseObjectEnumerator];
            while (RetainPtr nextAction = [reversedNextActions nextObject]) {
                actionsForAnnotation.append(WTFMove(nextAction));
                nextAction = [reversedNextActions nextObject];
            }
        }
#endif // HAVE(PDFKIT_WITH_NEXT_ACTIONS)
    }
}
#endif

OptionSet<RepaintRequirement> AnnotationTrackingState::startAnnotationTracking(RetainPtr<PDFAnnotation>&& annotation, WebEventType mouseEventType, WebMouseEventButton mouseEventButton)
{
    ASSERT(!m_trackedAnnotation);
    m_trackedAnnotation = WTFMove(annotation);

    auto repaintRequirements = OptionSet<RepaintRequirement> { };

    if ([m_trackedAnnotation isKindOfClass:getPDFAnnotationButtonWidgetClass()]) {
        [m_trackedAnnotation setHighlighted:YES];
        repaintRequirements.add(UnifiedPDFPlugin::repaintRequirementsForAnnotation(m_trackedAnnotation.get()));
    }

    if (mouseEventType == WebEventType::MouseMove && mouseEventButton == WebMouseEventButton::None) {
        if (!m_isBeingHovered)
            repaintRequirements.add(RepaintRequirement::HoverOverlay);

        m_isBeingHovered = true;
    }

    return repaintRequirements;
}

OptionSet<RepaintRequirement> AnnotationTrackingState::finishAnnotationTracking(PDFAnnotation* annotationUnderMouse, WebEventType mouseEventType, WebMouseEventButton mouseEventButton)
{
    ASSERT(m_trackedAnnotation);
    auto repaintRequirements = OptionSet<RepaintRequirement> { };

    if (annotationUnderMouse == m_trackedAnnotation && mouseEventType == WebEventType::MouseUp && mouseEventButton == WebMouseEventButton::Left) {
        if ([m_trackedAnnotation isHighlighted]) {
            [m_trackedAnnotation setHighlighted:NO];
            repaintRequirements.add(UnifiedPDFPlugin::repaintRequirementsForAnnotation(m_trackedAnnotation.get()));
        }

        if ([m_trackedAnnotation isKindOfClass:getPDFAnnotationButtonWidgetClass()] && [m_trackedAnnotation widgetControlType] != kPDFWidgetPushButtonControl) {
            auto currentButtonState = [m_trackedAnnotation buttonWidgetState];
            if (currentButtonState == PDFWidgetCellState::kPDFWidgetOnState && [m_trackedAnnotation allowsToggleToOff]) {
                [m_trackedAnnotation setButtonWidgetState:PDFWidgetCellState::kPDFWidgetOffState];
                repaintRequirements.add(RepaintRequirement::PDFContent);
            } else if (currentButtonState == PDFWidgetCellState::kPDFWidgetOffState) {
                [m_trackedAnnotation setButtonWidgetState:PDFWidgetCellState::kPDFWidgetOnState];
                repaintRequirements.add(RepaintRequirement::PDFContent);
            }
        }
    }

    resetAnnotationTrackingState();
    return repaintRequirements;
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

bool UnifiedPDFPlugin::isTaggedPDF() const
{
    return CGPDFDocumentIsTaggedPDF([m_pdfDocument documentRef]);
}

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)

void UnifiedPDFPlugin::installDataDetectorOverlay(PageOverlay& overlay)
{
    if (!m_frame || !m_frame->coreLocalFrame())
        return;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->corePage()->pageOverlayController().installPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);
}

void UnifiedPDFPlugin::uninstallDataDetectorOverlay(PageOverlay& overlay)
{
    if (!m_frame || !m_frame->coreLocalFrame())
        return;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->corePage()->pageOverlayController().uninstallPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);
}

#endif

Vector<WebCore::FloatRect> UnifiedPDFPlugin::annotationRectsForTesting() const
{
    Vector<WebCore::FloatRect> annotationRects;

    for (size_t pageIndex = 0; pageIndex < m_documentLayout.pageCount(); ++pageIndex) {
        RetainPtr currentPage = m_documentLayout.pageAtIndex(pageIndex);
        if (!currentPage)
            break;

        RetainPtr annotationsOnPage = [currentPage annotations];
        if (!annotationsOnPage)
            continue;

        for (unsigned annotationIndex = 0; annotationIndex < [annotationsOnPage count]; ++annotationIndex) {
            auto pageSpaceBounds = [[annotationsOnPage objectAtIndex:annotationIndex] bounds];
            annotationRects.append(convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Plugin, FloatRect { pageSpaceBounds }, pageIndex));
        }
    }

    return annotationRects;
}

void UnifiedPDFPlugin::setPDFDisplayModeForTesting(const String& mode)
{
    setDisplayModeAndUpdateLayout([mode] {
        if (mode == "SinglePageDiscrete"_s)
            return PDFDocumentLayout::DisplayMode::SinglePageDiscrete;

        if (mode == "SinglePageContinuous"_s)
            return PDFDocumentLayout::DisplayMode::SinglePageContinuous;

        if (mode == "TwoUpDiscrete"_s)
            return PDFDocumentLayout::DisplayMode::TwoUpDiscrete;

        if (mode == "TwoUpContinuous"_s)
            return PDFDocumentLayout::DisplayMode::TwoUpContinuous;

        ASSERT_NOT_REACHED();
        return PDFDocumentLayout::DisplayMode::SinglePageContinuous;
    }());
}

void UnifiedPDFPlugin::setDisplayModeAndUpdateLayout(PDFDocumentLayout::DisplayMode mode)
{
    auto shouldAdjustPageScale = m_shouldUpdateAutoSizeScale == ShouldUpdateAutoSizeScale::Yes ? AdjustScaleAfterLayout::No : AdjustScaleAfterLayout::Yes;
    m_documentLayout.setDisplayMode(mode);
    {
        SetForScope scope(m_shouldUpdateAutoSizeScale, ShouldUpdateAutoSizeScale::Yes);
        updateLayout(shouldAdjustPageScale);
    }

    if (isInDiscreteDisplayMode()) {
        ASSERT(m_currentlySnappedPage);
        if (!m_currentlySnappedPage)
            return;

        auto pageBoundsInDocumentSpace = layoutBoundsForPageAtIndex(m_currentlySnappedPage.value());
        auto pageBoundsInScrolledContents = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::ScrolledContents, pageBoundsInDocumentSpace);

        scrollToPointInContentsSpace(pageBoundsInScrolledContents.location());
    }
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
