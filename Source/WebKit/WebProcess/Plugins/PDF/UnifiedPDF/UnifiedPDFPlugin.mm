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
#include "PDFAnnotationTypeHelpers.h"
#include "PDFContextMenu.h"
#include "PDFDataDetectorOverlayController.h"
#include "PDFKitSPI.h"
#include "PDFPageCoverage.h"
#include "PDFPluginAnnotation.h"
#include "PDFPluginPasswordField.h"
#include "PDFPluginPasswordForm.h"
#include "PDFScrollingPresentationController.h"
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
#include <WebCore/EventHandler.h>
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
#include <WebCore/ImmediateActionStage.h>
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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextStream.h>

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
using namespace WebKit::PDFAnnotationTypeHelpers;

WTF_MAKE_TZONE_ALLOCATED_IMPL(UnifiedPDFPlugin);

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

    setDisplayMode(PDFDocumentLayout::DisplayMode::SinglePageContinuous);

#if PLATFORM(MAC)
    m_accessibilityDocumentObject = adoptNS([[WKAccessibilityPDFDocumentObject alloc] initWithPDFDocument:m_pdfDocument andElement:&element]);
    [m_accessibilityDocumentObject setPDFPlugin:this];
    if (this->isFullFramePlugin() && m_frame && m_frame->page() && m_frame->isMainFrame())
        [m_accessibilityDocumentObject setParent:dynamic_objc_cast<NSObject>(m_frame->protectedPage()->accessibilityRemoteObject())];
#endif

    if (m_presentationController->wantsWheelEvents())
        wantsWheelEventsChanged();
}

UnifiedPDFPlugin::~UnifiedPDFPlugin() = default;

static String mutationObserverNotificationString()
{
    static NeverDestroyed<String> notificationString = "PDFFormDidChangeValue"_s;
    return notificationString;
}

void UnifiedPDFPlugin::teardown()
{
    PDFPluginBase::teardown();

    GraphicsLayer::unparentAndClear(m_rootLayer);

    bool wantedWheelEvents = m_presentationController->wantsWheelEvents();
    setPresentationController(nullptr); // Breaks retain cycle.

    if (wantedWheelEvents)
        wantsWheelEventsChanged();

    RefPtr page = this->page();
    if (m_scrollingNodeID && page) {
        RefPtr scrollingCoordinator = page->scrollingCoordinator();
        scrollingCoordinator->unparentChildrenAndDestroyNode(*m_scrollingNodeID);
        m_frame->coreLocalFrame()->protectedView()->removePluginScrollableAreaForScrollingNodeID(*m_scrollingNodeID);
    }

    [[NSNotificationCenter defaultCenter] removeObserver:m_pdfMutationObserver.get() name:mutationObserverNotificationString() object:m_pdfDocument.get()];
    m_pdfMutationObserver = nullptr;

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    std::exchange(m_dataDetectorOverlayController, nullptr)->teardown();
#endif

    setActiveAnnotation({ nullptr, IsInPluginCleanup::Yes });
    m_annotationContainer = nullptr;
}

GraphicsLayer* UnifiedPDFPlugin::graphicsLayer() const
{
    return m_rootLayer.get();
}

void UnifiedPDFPlugin::setPresentationController(RefPtr<PDFPresentationController>&& presentationController)
{
    if (m_presentationController)
        m_presentationController->teardown();

    m_presentationController = WTFMove(presentationController);
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

    if (isLocked())
        createPasswordEntryForm();

    if (m_view) {
        m_view->layerHostingStrategyDidChange();
#if PLATFORM(IOS_FAMILY)
        m_view->pluginDidInstallPDFDocument(pageScaleFactor());
#endif
    }

    [[NSNotificationCenter defaultCenter] addObserver:m_pdfMutationObserver.get() selector:@selector(formChanged:) name:mutationObserverNotificationString() object:m_pdfDocument.get()];

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    enableDataDetection();
#endif

    if (m_presentationController->wantsWheelEvents())
        wantsWheelEventsChanged();

    revealFragmentIfNeeded();

    if (m_pdfTestCallback) {
        m_pdfTestCallback->handleEvent();
        m_pdfTestCallback = nullptr;
    }
}

void UnifiedPDFPlugin::incrementalLoadingDidProgress()
{
    static constexpr auto incrementalLoadRepaintInterval = 1_s;
    if (!m_incrementalLoadingRepaintTimer.isActive())
        m_incrementalLoadingRepaintTimer.startRepeating(incrementalLoadRepaintInterval);
}

void UnifiedPDFPlugin::incrementalLoadingDidCancel()
{
    m_incrementalLoadingRepaintTimer.stop();
}

void UnifiedPDFPlugin::incrementalLoadingDidFinish()
{
    m_incrementalLoadingRepaintTimer.stop();
    repaintForIncrementalLoad();
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

bool UnifiedPDFPlugin::canShowDataDetectorHighlightOverlays() const
{
    return !m_inMagnificationGesture;
}

void UnifiedPDFPlugin::didInvalidateDataDetectorHighlightOverlayRects()
{
    auto lastKnownMousePositionInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, lastKnownMousePositionInView());
    auto pageIndex = m_presentationController->pageIndexForDocumentPoint(lastKnownMousePositionInDocumentSpace);
    dataDetectorOverlayController().didInvalidateHighlightOverlayRects(pageIndex);
}

#endif

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

void UnifiedPDFPlugin::teardownPasswordEntryForm()
{
    m_passwordForm = nullptr;
    m_passwordField = nullptr;
}

void UnifiedPDFPlugin::attemptToUnlockPDF(const String& password)
{
    std::optional<ShouldUpdateAutoSizeScale> shouldUpdateAutoSizeScaleOverride;
    if (isLocked())
        shouldUpdateAutoSizeScaleOverride = ShouldUpdateAutoSizeScale::Yes;

    if (![m_pdfDocument unlockWithPassword:password]) {
        m_passwordField->resetField();
        m_passwordForm->unlockFailed();
        return;
    }

    m_passwordForm = nullptr;
    m_passwordField = nullptr;

    updateLayout(AdjustScaleAfterLayout::Yes, shouldUpdateAutoSizeScaleOverride);

#if ENABLE(PDF_HUD)
    updateHUDVisibility();
    updateHUDLocation();
#endif

    revealFragmentIfNeeded();
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

GraphicsLayerFactory* UnifiedPDFPlugin::graphicsLayerFactory() const
{
    RefPtr page = this->page();
    if (!page)
        return nullptr;

    return page->chrome().client().graphicsLayerFactory();
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

void UnifiedPDFPlugin::setNeedsRepaintForAnnotation(PDFAnnotation *annotation, RepaintRequirements repaintRequirements)
{
    if (!repaintRequirements)
        return;

    auto pageIndex = pageIndexForAnnotation(annotation);
    if (!pageIndex)
        return;

    auto annotationBounds = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::PDFDocumentLayout, FloatRect { [annotation bounds] }, pageIndex);
    auto layoutRow = m_documentLayout.rowForPageIndex(*pageIndex);
    setNeedsRepaintInDocumentRect(repaintRequirements, annotationBounds, layoutRow);
}

void UnifiedPDFPlugin::setNeedsRepaintInDocumentRect(RepaintRequirements repaintRequirements, const FloatRect& rectInDocumentCoordinates, std::optional<PDFLayoutRow> layoutRow)
{
    if (!repaintRequirements)
        return;

    m_presentationController->setNeedsRepaintInDocumentRect(repaintRequirements, rectInDocumentCoordinates, layoutRow);
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

    if (!m_overflowControlsContainer) {
        m_overflowControlsContainer = createGraphicsLayer("Overflow controls container"_s, GraphicsLayer::Type::Normal);
        m_overflowControlsContainer->setAnchorPoint({ });
        m_rootLayer->addChild(*m_overflowControlsContainer);
    }

    m_presentationController->setupLayers(*m_scrollContainerLayer);
}

void UnifiedPDFPlugin::incrementalLoadingRepaintTimerFired()
{
    repaintForIncrementalLoad();
}

void UnifiedPDFPlugin::repaintForIncrementalLoad()
{
    if (!m_pdfDocument)
        return;

    m_presentationController->repaintForIncrementalLoad();
}

float UnifiedPDFPlugin::scaleForPagePreviews() const
{
    // The scale for page previews is a half of the normal tile resolution at 1x page scale.
    // pageCoverage.pdfDocumentScale is here because page previews draw into a buffer sized using layoutBoundsForPageAtIndex().
    static constexpr float pagePreviewScale = 0.5;
    return deviceScaleFactor() * m_documentLayout.scale() * pagePreviewScale;
}

void UnifiedPDFPlugin::willAttachScrollingNode()
{
    createScrollingNodeIfNecessary();
}

void UnifiedPDFPlugin::didAttachScrollingNode()
{
    m_didAttachScrollingTreeNode = true;
    revealFragmentIfNeeded();
}

void UnifiedPDFPlugin::didSameDocumentNavigationForFrame(WebFrame& frame)
{
    if (&frame != m_frame)
        return;

    m_didScrollToFragment = false;
    revealFragmentIfNeeded();
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
    scrollingCoordinator->createNode(m_frame->coreLocalFrame()->rootFrame().frameID(), ScrollingNodeType::PluginScrolling, *m_scrollingNodeID);

#if ENABLE(SCROLLING_THREAD)
    m_scrollContainerLayer->setScrollingNodeID(*m_scrollingNodeID);

    if (auto* layer = layerForHorizontalScrollbar())
        layer->setScrollingNodeID(*m_scrollingNodeID);

    if (auto* layer = layerForVerticalScrollbar())
        layer->setScrollingNodeID(*m_scrollingNodeID);

    if (auto* layer = layerForScrollCorner())
        layer->setScrollingNodeID(*m_scrollingNodeID);
#endif

    m_frame->coreLocalFrame()->protectedView()->setPluginScrollableAreaForScrollingNodeID(*m_scrollingNodeID, *this);

    scrollingCoordinator->setScrollingNodeScrollableAreaGeometry(*m_scrollingNodeID, *this);

    WebCore::ScrollingCoordinator::NodeLayers nodeLayers;
    nodeLayers.layer = m_rootLayer.get();
    nodeLayers.scrollContainerLayer = m_scrollContainerLayer.get();
    nodeLayers.scrolledContentsLayer = m_scrolledContentsLayer.get();
    nodeLayers.horizontalScrollbarLayer = layerForHorizontalScrollbar();
    nodeLayers.verticalScrollbarLayer = layerForVerticalScrollbar();

    scrollingCoordinator->setNodeLayers(*m_scrollingNodeID, nodeLayers);
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

    m_presentationController->updateLayersOnLayoutChange(documentSize(), centeringOffset(), m_scaleFactor);
    updateSnapOffsets();

    didChangeSettings();
    didChangeIsInWindow();
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
    if (m_rootLayer)
        propagateSettingsToLayer(*m_rootLayer);

    if (m_scrollContainerLayer)
        propagateSettingsToLayer(*m_scrollContainerLayer);

    if (m_scrolledContentsLayer)
        propagateSettingsToLayer(*m_scrolledContentsLayer);

    if (m_layerForHorizontalScrollbar)
        propagateSettingsToLayer(*m_layerForHorizontalScrollbar);

    if (m_layerForVerticalScrollbar)
        propagateSettingsToLayer(*m_layerForVerticalScrollbar);

    if (m_layerForScrollCorner)
        propagateSettingsToLayer(*m_layerForScrollCorner);

    m_presentationController->updateDebugBorders(showDebugBorders, showRepaintCounter);
}

void UnifiedPDFPlugin::notifyFlushRequired(const GraphicsLayer*)
{
    scheduleRenderingUpdate();
}

bool UnifiedPDFPlugin::isInWindow() const
{
    RefPtr page = this->page();
    return page ? page->isInWindow() : false;
}

void UnifiedPDFPlugin::didChangeIsInWindow()
{
    RefPtr page = this->page();
    if (!page)
        return;

    bool isInWindow = page->isInWindow();
    m_presentationController->updateIsInWindow(isInWindow);

    if (!isInWindow) {
        auto& scrollingCoordinator = *page->scrollingCoordinator();
        scrollingCoordinator.scrollableAreaWillBeDetached(*this);
    }
}

void UnifiedPDFPlugin::windowActivityDidChange()
{
    repaintOnSelectionChange(ActiveStateChangeReason::WindowActivityChanged);
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

    paintPDFContent(nullptr, context, clipRect, m_presentationController->visibleRow());
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

    // Other layers should be painted by the PDFPresentationController.
    ASSERT_NOT_REACHED();
}

void UnifiedPDFPlugin::paintPDFContent(const WebCore::GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, const std::optional<PDFLayoutRow>& row, PaintingBehavior behavior, AsyncPDFRenderer* asyncRenderer)
{
    if (visibleOrDocumentSizeIsEmpty() || !m_presentationController)
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

    auto tilingScaleFactor = 1.0f;
    if (layer) {
        if (auto* tiledBacking = layer->tiledBacking())
            tilingScaleFactor = tiledBacking->tilingScaleFactor();
    }

    auto pageCoverage = m_presentationController->pageCoverageAndScalesForContentsRect(clipRect, row, tilingScaleFactor);
    auto documentScale = pageCoverage.pdfDocumentScale;

    for (auto& pageInfo : pageCoverage.pages) {
        auto page = m_documentLayout.pageAtIndex(pageInfo.pageIndex);
        if (!page)
            continue;

        auto pageDestinationRect = pageInfo.pageBounds;

        if (asyncRenderer) {
            auto pageBoundsInContentCoordinates = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::Contents, pageDestinationRect, pageInfo.pageIndex);
            auto pageBoundsInPaintingCoordinates = convertFromContentsToPainting(pageBoundsInContentCoordinates, pageInfo.pageIndex);

            auto pageStateSaver = GraphicsContextStateSaver(context);
            context.clip(pageBoundsInPaintingCoordinates);

            ASSERT(layer);

            if (showDebugIndicators)
                context.fillRect(pageBoundsInPaintingCoordinates, Color::yellow.colorWithAlphaByte(128));

            asyncRenderer->paintTilesForPage(layer, context, documentScale, clipRect, pageInfo.rectInPageLayoutCoordinates, pageBoundsInPaintingCoordinates, pageInfo.pageIndex);
        }

        bool currentPageHasAnnotation = pageWithAnnotation && *pageWithAnnotation == pageInfo.pageIndex;
        if (asyncRenderer && !haveSelection && !currentPageHasAnnotation)
            continue;

        auto pageStateSaver = GraphicsContextStateSaver(context);
        if (layer) {
            auto contentsOffset = convertFromContentsToPainting({ }, pageInfo.pageIndex);
            context.translate(contentsOffset.location());
        }

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
void UnifiedPDFPlugin::paintPDFSelection(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, std::optional<PDFLayoutRow> row)
{
    if (!m_currentSelection || [m_currentSelection isEmpty] || !canPaintSelectionIntoOwnedLayer() || !m_presentationController)
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

    auto tilingScaleFactor = 1.0f;
    if (auto* tiledBacking = layer->tiledBacking())
        tilingScaleFactor = tiledBacking->tilingScaleFactor();

    auto pageCoverage = m_presentationController->pageCoverageAndScalesForContentsRect(clipRect, row, tilingScaleFactor);
    auto documentScale = pageCoverage.pdfDocumentScale;
    for (auto& pageInfo : pageCoverage.pages) {
        auto page = m_documentLayout.pageAtIndex(pageInfo.pageIndex);
        if (!page)
            continue;

        auto pageDestinationRect = pageInfo.pageBounds;

        GraphicsContextStateSaver pageStateSaver { context };
        if (layer) {
            auto contentsOffset = convertFromContentsToPainting({ }, pageInfo.pageIndex);
            context.translate(contentsOffset.location());
        }

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

void UnifiedPDFPlugin::paintHoveredAnnotationOnPage(PDFDocumentLayout::PageIndex indexOfPaintedPage, GraphicsContext& context, const FloatRect& clipRect)
{
    ASSERT(pageIndexWithHoveredAnnotation());
    ASSERT(supportsForms());

    RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    auto pageIndex = pageIndexForAnnotation(trackedAnnotation.get());
    if (!pageIndex)
        return;

    // The GraphicsContext is in PDFPage coordinates here.
    auto annotationRect = FloatRect { [trackedAnnotation bounds] };
    context.fillRect(annotationRect, textAnnotationHoverColor());
}

std::optional<PDFDocumentLayout::PageIndex> UnifiedPDFPlugin::pageIndexForAnnotation(PDFAnnotation *annotation) const
{
    auto pageIndex = [m_pdfDocument indexForPage:[annotation page]];
    if (pageIndex == NSNotFound)
        return { };

    return pageIndex;
}

std::optional<PDFDocumentLayout::PageIndex> UnifiedPDFPlugin::pageIndexWithHoveredAnnotation() const
{
    RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    if (!trackedAnnotation)
        return { };

    if (!annotationIsWidgetOfType(trackedAnnotation.get(), WidgetType::Text))
        return { };

    if (!m_annotationTrackingState.isBeingHovered())
        return { };

    return pageIndexForAnnotation(trackedAnnotation.get());
}

double UnifiedPDFPlugin::minScaleFactor() const
{
    return minimumZoomScale;
}

double UnifiedPDFPlugin::maxScaleFactor() const
{
    return maximumZoomScale;
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
#if PLATFORM(MAC)
    auto actualSizeScale = scaleForActualSize();
    auto fitToViewScale = scaleForFitToView();
    auto initialScale = std::max(actualSizeScale, fitToViewScale);
    // Only let actual size scaling scale down, not up.
    initialScale = std::min(initialScale, 1.0);
    return initialScale;
#else
    return 1.0;
#endif
}

void UnifiedPDFPlugin::computeNormalizationFactor()
{
    auto actualSizeScale = scaleForActualSize();
    m_scaleNormalizationFactor = std::max(1.0, actualSizeScale) / actualSizeScale;
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
    return nonNormalizedScaleFactor();
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

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    dataDetectorOverlayController().hideActiveHighlightOverlay();
#endif
}

void UnifiedPDFPlugin::didEndMagnificationGesture()
{
    m_inMagnificationGesture = false;
    m_magnificationOriginInContentCoordinates = { };
    m_magnificationOriginInPluginCoordinates = { };

    deviceOrPageScaleFactorChanged();
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

    deviceOrPageScaleFactorChanged(CheckForMagnificationGesture::Yes);

    m_presentationController->updateLayersOnLayoutChange(documentSize(), centeringOffset(), m_scaleFactor);
    updateSnapOffsets();

#if PLATFORM(MAC)
    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();
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
    deviceOrPageScaleFactorChanged(CheckForMagnificationGesture::Yes);
    if (!handlesPageScaleFactor())
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

void UnifiedPDFPlugin::deviceOrPageScaleFactorChanged(CheckForMagnificationGesture checkForMagnificationGesture)
{
    bool gestureAllowsScaleUpdate = checkForMagnificationGesture == CheckForMagnificationGesture::No || !m_inMagnificationGesture;

    if (!handlesPageScaleFactor() || gestureAllowsScaleUpdate)
        m_rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    if (gestureAllowsScaleUpdate)
        m_presentationController->deviceOrPageScaleFactorChanged();
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

void UnifiedPDFPlugin::updateLayout(AdjustScaleAfterLayout shouldAdjustScale, std::optional<ShouldUpdateAutoSizeScale> shouldUpdateAutoSizeScaleOverride)
{
    auto layoutSize = availableContentsRect().size();
    auto autoSizeMode = shouldUpdateAutoSizeScaleOverride.value_or(m_didLayoutWithValidDocument ? m_shouldUpdateAutoSizeScale : ShouldUpdateAutoSizeScale::Yes);

    auto computeAnchoringInfo = [&] {
        return m_presentationController->pdfPositionForCurrentView(shouldAdjustScale == AdjustScaleAfterLayout::Yes || autoSizeMode == ShouldUpdateAutoSizeScale::Yes);
    };
    auto anchoringInfo = computeAnchoringInfo();

    auto layoutUpdateChanges = m_documentLayout.updateLayout(layoutSize, autoSizeMode);
    updateScrollbars();

    // Do a second layout pass if the first one changed scrollbars.
    auto newLayoutSize = availableContentsRect().size();
    if (layoutSize != newLayoutSize) {
        layoutUpdateChanges |= m_documentLayout.updateLayout(newLayoutSize, autoSizeMode);
        updateScrollbars();
    }

    m_didLayoutWithValidDocument = m_documentLayout.hasPDFDocument();

    updateLayerHierarchy();
    updateScrollingExtents();
    computeNormalizationFactor();

    if (shouldAdjustScale == AdjustScaleAfterLayout::Yes && m_view) {
        auto initialScaleFactor = initialScale();
        LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::updateLayout - on first layout, chose scale for actual size " << initialScaleFactor);
        setScaleFactor(initialScaleFactor);

        m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;
    }

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::updateLayout - scale " << m_scaleFactor << " normalization factor " << m_scaleNormalizationFactor << " layout scale " << m_documentLayout.scale());

    constexpr OptionSet allLayoutChangeTypes {
        PDFDocumentLayout::LayoutUpdateChange::PageGeometries,
        PDFDocumentLayout::LayoutUpdateChange::DocumentBounds,
    };
    if (layoutUpdateChanges.containsAll(allLayoutChangeTypes)) {
        anchoringInfo = anchoringInfo.and_then([&](const PDFPresentationController::VisiblePDFPosition&) {
            return computeAnchoringInfo();
        });
    }

    if (anchoringInfo)
        m_presentationController->restorePDFPosition(*anchoringInfo);
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

void UnifiedPDFPlugin::releaseMemory()
{
    if (m_presentationController)
        m_presentationController->releaseMemory();
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
#endif // PLATFORM(MAC)

    // FIXME: Make the overlay scroll with the tiles instead of repainting constantly.
    updateFindOverlay(HideFindIndicator::Yes);

    // FIXME: Make the overlay scroll with the tiles instead of repainting constantly.
#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    didInvalidateDataDetectorHighlightOverlayRects();
#endif

    scheduleRenderingUpdate();
}

void UnifiedPDFPlugin::willChangeVisibleRow()
{
    setActiveAnnotation({ nullptr });
}

void UnifiedPDFPlugin::didChangeVisibleRow()
{
    // FIXME: <https://webkit.org/b/276981> Make the overlay scroll with the tiles instead of repainting constantly.
    updateFindOverlay(HideFindIndicator::Yes);

    // FIXME: <https://webkit.org/b/276981> Make the overlay scroll with the tiles instead of repainting constantly.
#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    didInvalidateDataDetectorHighlightOverlayRects();
#endif

    m_presentationController->updateForCurrentScrollability(computeScrollability());
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

bool UnifiedPDFPlugin::shouldCachePagePreviews() const
{
    // Only main frame plugins are hooked up to releaseMemory().
    return isFullFramePlugin();
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

    m_presentationController->updateForCurrentScrollability(computeScrollability());

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

WebCore::OverscrollBehavior UnifiedPDFPlugin::overscrollBehavior() const
{
    return isInDiscreteDisplayMode() ? WebCore::OverscrollBehavior::None : WebCore::OverscrollBehavior::Auto;
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

PDFDocumentLayout::PageIndex UnifiedPDFPlugin::indexForCurrentPageInView() const
{
    // FIXME: <https://webkit.org/b/276981> This is not correct for discrete presentation mode.
    auto centerInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { flooredIntPoint(size() / 2) });
    return m_presentationController->nearestPageIndexForDocumentPoint(centerInDocumentSpace);
}

RetainPtr<PDFAnnotation> UnifiedPDFPlugin::annotationForRootViewPoint(const IntPoint& point) const
{
    auto pointInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { convertFromRootViewToPlugin(point) });
    auto pageIndex = m_presentationController->pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!pageIndex)
        return nullptr;

    auto page = m_documentLayout.pageAtIndex(pageIndex.value());
    return [page annotationAtPoint:convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex.value())];
}

FloatRect UnifiedPDFPlugin::convertFromContentsToPainting(const FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    return m_presentationController->convertFromContentsToPainting(rect, pageIndex);
}

FloatRect UnifiedPDFPlugin::convertFromPaintingToContents(const FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    return m_presentationController->convertFromPaintingToContents(rect, pageIndex);
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
    auto hitPageIndex = m_presentationController->pageIndexForDocumentPoint(pointInDocumentSpace);
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

        if (![annotation isReadOnly]) {
            if (annotationIsWidgetOfType(annotation, WidgetType::Text))
                pdfElementTypes.add(PDFElementType::TextField);
            if (annotationIsWidgetOfType(annotation, WidgetType::Button))
                pdfElementTypes.add(PDFElementType::Control);
        }
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
    auto pageIndex = m_presentationController->nearestPageIndexForDocumentPoint(pointInDocumentSpace);
    auto pointInPageSpace = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex);

    auto mouseEventButton = event.button();
    auto mouseEventType = event.type();
    // Context menu events always call handleContextMenuEvent as well.
    if (mouseEventType == WebEventType::MouseDown && isContextMenuEvent(event)) {
        bool contextMenuEventIsInsideDocumentBounds = m_presentationController->pageIndexForDocumentPoint(pointInDocumentSpace).has_value();
        if (contextMenuEventIsInsideDocumentBounds)
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

            if (!m_annotationTrackingState.trackedAnnotation() && annotationUnderMouse && annotationIsWidgetOfType(annotationUnderMouse.get(), WidgetType::Text) && supportsForms())
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
                if ([annotation isReadOnly]
                    && annotationIsWidgetOfType(annotation.get(), { WidgetType::Button, WidgetType::Text, WidgetType::Choice }))
                    return true;

                if (annotationIsWidgetOfType(annotation.get(), { WidgetType::Text, WidgetType::Choice })) {
                    setActiveAnnotation({ WTFMove(annotation) });
                    return true;
                }

                if (annotationIsWidgetOfType(annotation.get(), WidgetType::Button)) {
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
            if (RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation(); trackedAnnotation && !annotationIsWidgetOfType(trackedAnnotation.get(), WidgetType::Text)) {
                RetainPtr annotationUnderMouse = annotationForRootViewPoint(event.position());
                finishTrackingAnnotation(annotationUnderMouse.get(), mouseEventType, mouseEventButton);

                bool shouldFollowLinkAnnotation = [frame = m_frame] {
                    if (!frame || !frame->coreLocalFrame())
                        return true;
                    auto immediateActionStage = frame->protectedCoreLocalFrame()->checkedEventHandler()->immediateActionStage();
                    return !immediateActionBeganOrWasCompleted(immediateActionStage);
                }();

                if (shouldFollowLinkAnnotation && annotationIsLinkWithDestination(trackedAnnotation.get()))
                    followLinkAnnotation(trackedAnnotation.get());

#if PLATFORM(MAC)
                if (RetainPtr pdfAction = [trackedAnnotation action])
                    handlePDFActionForAnnotation(trackedAnnotation.get(), pageIndex, shouldFollowLinkAnnotation ? ShouldPerformGoToAction::Yes : ShouldPerformGoToAction::No);
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

bool UnifiedPDFPlugin::wantsWheelEvents() const
{
    if (!m_pdfDocument)
        return false;

    if (!m_presentationController)
        return false;

    return m_presentationController->wantsWheelEvents();
}

bool UnifiedPDFPlugin::handleWheelEvent(const WebWheelEvent& wheelEvent)
{
    auto handledByPresentationController = m_presentationController->handleWheelEvent(wheelEvent);

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::handleWheelEvent " << platform(wheelEvent) << " - handledByPresentationController " << handledByPresentationController);

    if (handledByPresentationController)
        return true;

    return handleWheelEventForScrolling(platform(wheelEvent), { });
}

PlatformWheelEvent UnifiedPDFPlugin::wheelEventCopyWithVelocity(const PlatformWheelEvent& wheelEvent) const
{
    if (!isFullMainFramePlugin())
        return wheelEvent;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return wheelEvent;

    return webPage->corePage()->wheelEventDeltaFilter()->eventCopyWithVelocity(wheelEvent);
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

bool UnifiedPDFPlugin::handleKeyboardEvent(const WebKeyboardEvent& event)
{
    return m_presentationController->handleKeyboardEvent(event);
}

void UnifiedPDFPlugin::followLinkAnnotation(PDFAnnotation *annotation)
{
    ASSERT(annotationIsLinkWithDestination(annotation));
    if (NSURL *url = [annotation URL])
        navigateToURL(url);
    else if (PDFDestination *destination = [annotation destination])
        revealPDFDestination(destination);
}

RepaintRequirements UnifiedPDFPlugin::repaintRequirementsForAnnotation(PDFAnnotation *annotation, IsAnnotationCommit isAnnotationCommit)
{
    if (annotationIsWidgetOfType(annotation, WidgetType::Button))
        return RepaintRequirement::PDFContent;

    if ([annotation isKindOfClass:getPDFAnnotationPopupClass()])
        return RepaintRequirement::PDFContent;

    if (annotationIsWidgetOfType(annotation, WidgetType::Choice))
        return RepaintRequirement::PDFContent;

    if ([annotation isKindOfClass:getPDFAnnotationTextClass()])
        return RepaintRequirement::PDFContent;

    if (annotationIsWidgetOfType(annotation, WidgetType::Text))
        return isAnnotationCommit == IsAnnotationCommit::Yes ? RepaintRequirement::PDFContent : RepaintRequirement::HoverOverlay;

    // No visual feedback for getPDFAnnotationLinkClass at this time.

    return { };
}

void UnifiedPDFPlugin::repaintAnnotationsForFormField(NSString *fieldName)
{
#if HAVE(PDFDOCUMENT_ANNOTATIONS_FOR_FIELD_NAME)
    RetainPtr annotations = [m_pdfDocument annotationsForFieldName:fieldName];
    for (PDFAnnotation *annotation in annotations.get())
        setNeedsRepaintForAnnotation(annotation, repaintRequirementsForAnnotation(annotation));
#else
    UNUSED_PARAM(fieldName);
#endif
}

void UnifiedPDFPlugin::startTrackingAnnotation(RetainPtr<PDFAnnotation>&& annotation, WebEventType mouseEventType, WebMouseEventButton mouseEventButton)
{
    auto repaintRequirements = m_annotationTrackingState.startAnnotationTracking(WTFMove(annotation), mouseEventType, mouseEventButton);
    setNeedsRepaintForAnnotation(m_annotationTrackingState.trackedAnnotation(), repaintRequirements);
}

void UnifiedPDFPlugin::updateTrackedAnnotation(PDFAnnotation *annotationUnderMouse)
{
    RetainPtr currentTrackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    bool isHighlighted = [currentTrackedAnnotation isHighlighted];
    RepaintRequirements repaintRequirements;

    if (isHighlighted && currentTrackedAnnotation != annotationUnderMouse) {
        [currentTrackedAnnotation setHighlighted:NO];
        repaintRequirements.add(UnifiedPDFPlugin::repaintRequirementsForAnnotation(currentTrackedAnnotation.get()));
    } else if (!isHighlighted && currentTrackedAnnotation == annotationUnderMouse) {
        [currentTrackedAnnotation setHighlighted:YES];
        repaintRequirements.add(UnifiedPDFPlugin::repaintRequirementsForAnnotation(currentTrackedAnnotation.get()));
    }

    setNeedsRepaintForAnnotation(currentTrackedAnnotation.get(), repaintRequirements);
}

void UnifiedPDFPlugin::finishTrackingAnnotation(PDFAnnotation* annotationUnderMouse, WebEventType mouseEventType, WebMouseEventButton mouseEventButton, RepaintRequirements repaintRequirements)
{
    RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    repaintRequirements.add(m_annotationTrackingState.finishAnnotationTracking(annotationUnderMouse, mouseEventType, mouseEventButton));
    setNeedsRepaintForAnnotation(trackedAnnotation.get(), repaintRequirements);
}

// FIXME: <https://webkit.org/b/276981>  Assumes scrolling.

void UnifiedPDFPlugin::revealPDFDestination(PDFDestination *destination)
{
    auto unspecifiedValue = get_PDFKit_kPDFDestinationUnspecifiedValue();

    auto pageIndex = [m_pdfDocument indexForPage:[destination page]];
    auto pointInPDFPageSpace = [destination point];
    if (pointInPDFPageSpace.x == unspecifiedValue)
        pointInPDFPageSpace.x = 0;
    if (pointInPDFPageSpace.y == unspecifiedValue)
        pointInPDFPageSpace.y = heightForPageAtIndex(pageIndex);

    revealPointInPage(pointInPDFPageSpace, pageIndex);
}

void UnifiedPDFPlugin::revealRectInPage(const FloatRect& pageRect, PDFDocumentLayout::PageIndex pageIndex)
{
    m_presentationController->ensurePageIsVisible(pageIndex);
    auto contentsRect = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::ScrolledContents, pageRect, pageIndex);

    auto pluginRectInContentsCoordinates = convertDown(CoordinateSpace::Plugin, CoordinateSpace::ScrolledContents, FloatRect { { 0, 0 }, size() });
    auto rectToExpose = getRectToExposeForScrollIntoView(LayoutRect(pluginRectInContentsCoordinates), LayoutRect(contentsRect), ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, std::nullopt);

    scrollToPointInContentsSpace(rectToExpose.location());
}

void UnifiedPDFPlugin::revealPointInPage(FloatPoint pointInPDFPageSpace, PDFDocumentLayout::PageIndex pageIndex)
{
    m_presentationController->ensurePageIsVisible(pageIndex);
    auto contentsPoint = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::ScrolledContents, pointInPDFPageSpace, pageIndex);
    scrollToPointInContentsSpace(contentsPoint);
}

void UnifiedPDFPlugin::revealPage(PDFDocumentLayout::PageIndex pageIndex)
{
    ASSERT(pageIndex < m_documentLayout.pageCount());
    m_presentationController->ensurePageIsVisible(pageIndex);

    auto pageBounds = m_documentLayout.layoutBoundsForPageAtIndex(pageIndex);
    auto boundsInScrolledContents = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::ScrolledContents, pageBounds);
    scrollToPointInContentsSpace(boundsInScrolledContents.location());
}

void UnifiedPDFPlugin::scrollToPointInContentsSpace(FloatPoint pointInContentsSpace)
{
    auto oldScrollType = currentScrollType();
    setCurrentScrollType(ScrollType::Programmatic);
    scrollToPositionWithoutAnimation(roundedIntPoint(pointInContentsSpace));
    setCurrentScrollType(oldScrollType);
}

void UnifiedPDFPlugin::revealFragmentIfNeeded()
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
            revealPage(*pageNumber - 1);
        return;
    }

    if (auto remainder = remainderForPrefix("nameddest="_s)) {
        if (auto destination = [m_pdfDocument namedDestination:remainder->createNSString().get()])
            revealPDFDestination(destination);
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
    // FIXME: <https://webkit.org/b/276981> Fix for rows.
    auto contextMenuEventDocumentPoint = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, contextMenuEventPluginPoint);
    menuItems.appendVector(navigationContextMenuItemsForPageAtIndex(m_presentationController->nearestPageIndexForDocumentPoint(contextMenuEventDocumentPoint)));

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

        bool disableItemDueToLockedDocument = isLocked() && tag != ContextMenuItemTag::OpenWithPreview;
        auto itemEnabled = disableItemDueToLockedDocument ? ContextMenuItemEnablement::Disabled : ContextMenuItemEnablement::Enabled;
        auto itemHasAction = hasAction && !disableItemDueToLockedDocument ? ContextMenuItemHasAction::Yes : ContextMenuItemHasAction::No;

        return { titleForContextMenuItemTag(tag), state, enumToUnderlyingType(tag), itemEnabled, itemHasAction, ContextMenuItemIsSeparator::No };
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
            revealPage(currentPageIndex + 1);
        else if (pageCount > pagesPerRow && currentPageIndex < pageCount - pagesPerRow)
            revealPage(currentPageIndex + pagesPerRow);
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
            revealPage(currentPageIndex - 1);
        else if (currentPageIndex > 1)
            revealPage(currentPageIndex - pagesPerRow);
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

    NSMutableArray *types = [NSMutableArray array];
    NSMutableArray *items = [NSMutableArray array];

    if (NSData *webArchiveData = [m_currentSelection webArchive]) {
        [types addObject:PasteboardTypes::WebArchivePboardType];
        [items addObject:webArchiveData];
    }

    if (NSData *plainStringData = [[m_currentSelection string] dataUsingEncoding:NSUTF8StringEncoding]) {
        [types addObject:NSPasteboardTypeString];
        [items addObject:plainStringData];
    }

    if (NSData *htmlStringData = [[m_currentSelection html] dataUsingEncoding:NSUTF8StringEncoding]) {
        [types addObject:NSPasteboardTypeHTML];
        [items addObject:htmlStringData];
    }

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

void UnifiedPDFPlugin::freezeCursorDuringSelectionDragIfNeeded(IsDraggingSelection isDraggingSelection, IsMarqueeSelection isMarqueeSelection)
{
    if (isDraggingSelection == IsDraggingSelection::No)
        return;

    if (!m_currentSelection || [m_currentSelection isEmpty])
        return;

    if (!std::exchange(m_selectionTrackingData.cursorIsFrozenForSelectionDrag, true))
        notifyCursorChanged(isMarqueeSelection == IsMarqueeSelection::Yes ? PlatformCursorType::Cross : PlatformCursorType::IBeam);
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
    freezeCursorDuringSelectionDragIfNeeded(isDraggingSelection, m_selectionTrackingData.shouldMakeMarqueeSelection ? IsMarqueeSelection::Yes : IsMarqueeSelection::No);

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

PDFPageCoverage UnifiedPDFPlugin::pageCoverageForSelection(PDFSelection *selection, FirstPageOnly firstPageOnly) const
{
    if (!selection || [selection isEmpty])
        return { };

    auto pageCoverage = PDFPageCoverage { };

    for (PDFPage *page in [selection pages]) {
        auto pageIndex = m_documentLayout.indexForPage(page);
        if (!pageIndex)
            continue;

        // FIXME: <https://webkit.org/b/276981> This needs per-row adjustment via the presentation controller.
        auto selectionBounds = FloatRect { [selection boundsForPage:page] };
        pageCoverage.append(PerPageInfo { *pageIndex, selectionBounds, selectionBounds });
        if (firstPageOnly == FirstPageOnly::Yes)
            break;
    }

    return pageCoverage;
}

void UnifiedPDFPlugin::repaintOnSelectionChange(ActiveStateChangeReason reason, PDFSelection* previousSelection)
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

    auto repaintSelection = [&](PDFSelection* selection) {
        auto selectionPageCoverage = pageCoverageForSelection(selection);
        for (auto& page : selectionPageCoverage) {
            auto layoutRow = m_documentLayout.rowForPageIndex(page.pageIndex);
            auto pageSelectionBounds = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::PDFDocumentLayout, page.pageBounds, page.pageIndex);
            setNeedsRepaintInDocumentRect(RepaintRequirement::Selection, pageSelectionBounds, layoutRow);
        }
    };

    if (previousSelection)
        repaintSelection(previousSelection);

    repaintSelection(protectedCurrentSelection().get());
}

RetainPtr<PDFSelection> UnifiedPDFPlugin::protectedCurrentSelection() const
{
    return m_currentSelection;
}

void UnifiedPDFPlugin::setCurrentSelection(RetainPtr<PDFSelection>&& selection)
{
    RetainPtr previousSelection = std::exchange(m_currentSelection, WTFMove(selection));

    // FIXME: <https://webkit.org/b/268980> Selection painting requests should be only be made if the current selection has changed.
    // FIXME: <https://webkit.org/b/270070> Selection painting should be optimized by only repainting diff between old and new selection.
    repaintOnSelectionChange(ActiveStateChangeReason::SetCurrentSelection, previousSelection.get());
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
    auto pageIndex = m_presentationController->pageIndexForDocumentPoint(documentPoint);
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
    auto pageIndex = m_presentationController->nearestPageIndexForDocumentPoint(lastKnownMousePositionInDocumentSpace);
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
        m_findMatchRects.clear();
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

    revealRectInPage([selection boundsForPage:firstPageForSelection.get()], *firstPageIndex);

    setCurrentSelection(WTFMove(selection));
    return true;
}

void UnifiedPDFPlugin::collectFindMatchRects(const String& target, WebCore::FindOptions options)
{
    m_findMatchRects.clear();

    RetainPtr foundSelections = [m_pdfDocument findString:target withOptions:compareOptionsForFindOptions(options)];
    for (PDFSelection *selection in foundSelections.get()) {
        for (PDFPage *page in selection.pages) {
            auto pageIndex = m_documentLayout.indexForPage(page);
            if (!pageIndex)
                continue;

            auto perPageInfo = PerPageInfo { *pageIndex, [selection boundsForPage:page] };
            m_findMatchRects.append(WTFMove(perPageInfo));
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
    return visibleRectsForFindMatchRects(m_findMatchRects);
}

Vector<WebFoundTextRange::PDFData> UnifiedPDFPlugin::findTextMatches(const String& target, WebCore::FindOptions options)
{
    Vector<WebFoundTextRange::PDFData> matches;
    if (!target.length())
        return matches;

    RetainPtr foundSelections = [m_pdfDocument findString:target withOptions:compareOptionsForFindOptions(options)];
    for (PDFSelection *selection in foundSelections.get()) {
        RetainPtr startPage = [[selection pages] firstObject];
        NSRange startPageRange = [selection rangeAtIndex:0 onPage:startPage.get()];
        NSUInteger startPageIndex = [m_pdfDocument indexForPage:startPage.get()];
        NSUInteger startPageOffset = startPageRange.location;

        RetainPtr endPage = [[selection pages] lastObject];
        NSUInteger endPageTextRangeCount = [selection numberOfTextRangesOnPage:endPage.get()];
        NSRange endPageRange = [selection rangeAtIndex:(endPageTextRangeCount - 1) onPage:endPage.get()];
        NSUInteger endPageIndex = [m_pdfDocument indexForPage:endPage.get()];
        NSUInteger endPageOffset = endPageRange.location + endPageRange.length;

        matches.append(WebFoundTextRange::PDFData { startPageIndex, startPageOffset, endPageIndex, endPageOffset });
    }

    return matches;
}

Vector<FloatRect> UnifiedPDFPlugin::rectsForTextMatch(const WebFoundTextRange::PDFData& data)
{
    RetainPtr selection = selectionFromWebFoundTextRangePDFData(data);
    if (!selection)
        return { };

    PDFPageCoverage findMatchRects;
    for (PDFPage *page in [selection pages]) {
        auto pageIndex = m_documentLayout.indexForPage(page);
        if (!pageIndex)
            continue;

        auto selectionBounds = FloatRect { [selection boundsForPage:page] };
        findMatchRects.append(PerPageInfo { *pageIndex, selectionBounds, selectionBounds });
    }

    return visibleRectsForFindMatchRects(findMatchRects);
}

Vector<WebCore::FloatRect> UnifiedPDFPlugin::visibleRectsForFindMatchRects(PDFPageCoverage findMatchRects) const
{
    auto visibleRow = m_presentationController->visibleRow();

    Vector<FloatRect> rectsInPluginCoordinates;
    if (!visibleRow)
        rectsInPluginCoordinates.reserveCapacity(findMatchRects.size());

    for (auto& perPageInfo : findMatchRects) {
        if (visibleRow && !visibleRow->containsPage(perPageInfo.pageIndex))
            continue;

        auto pluginRect = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Plugin, perPageInfo.pageBounds, perPageInfo.pageIndex);
        rectsInPluginCoordinates.append(pluginRect);
    }

    return rectsInPluginCoordinates;
}

PDFSelection *UnifiedPDFPlugin::selectionFromWebFoundTextRangePDFData(const WebFoundTextRange::PDFData& data) const
{
    RetainPtr startPage = [m_pdfDocument pageAtIndex:data.startPage];
    if (!startPage)
        return nil;

    RetainPtr endPage = [m_pdfDocument pageAtIndex:data.endPage];
    if (!endPage)
        return nil;

    return [m_pdfDocument selectionFromPage:startPage.get() atCharacterIndex:data.startOffset toPage:endPage.get() atCharacterIndex:(data.endOffset - 1)];
}

void UnifiedPDFPlugin::scrollToRevealTextMatch(const WebFoundTextRange::PDFData& data)
{
    RetainPtr selection = selectionFromWebFoundTextRangePDFData(data);
    if (!selection)
        return;

    RetainPtr firstPageForSelection = [[selection pages] firstObject];
    if (!firstPageForSelection)
        return;

    auto firstPageIndex = m_documentLayout.indexForPage(firstPageForSelection);
    if (!firstPageIndex)
        return;

    if (scrollingMode() == DelegatedScrollingMode::DelegatedToNativeScrollView) {
        auto rect = rectForSelectionInMainFrameContentsSpace(selection.get());
        if (RefPtr page = this->page())
            page->chrome().scrollMainFrameToRevealRect(enclosingIntRect(rect));
    } else
        revealRectInPage([selection boundsForPage:firstPageForSelection.get()], *firstPageIndex);

    setCurrentSelection(WTFMove(selection));
}

RefPtr<WebCore::TextIndicator> UnifiedPDFPlugin::textIndicatorForTextMatch(const WebFoundTextRange::PDFData& data, WebCore::TextIndicatorPresentationTransition transition)
{
    RetainPtr selection = selectionFromWebFoundTextRangePDFData(data);
    if (!selection)
        return { };

    return textIndicatorForSelection(selection.get(), { WebCore::TextIndicatorOption::IncludeMarginIfRangeMatchesSelection }, transition);
}

RefPtr<TextIndicator> UnifiedPDFPlugin::textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption> options, WebCore::TextIndicatorPresentationTransition transition)
{
    RetainPtr selection = m_currentSelection;
    return textIndicatorForSelection(selection.get(), options, transition);
}

RefPtr<TextIndicator> UnifiedPDFPlugin::textIndicatorForSelection(PDFSelection *selection, OptionSet<WebCore::TextIndicatorOption> options, WebCore::TextIndicatorPresentationTransition transition)
{
    auto selectionPageCoverage = pageCoverageForSelection(selection, FirstPageOnly::Yes);
    if (selectionPageCoverage.isEmpty())
        return nullptr;

    auto rectInContentsCoordinates = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Contents, selectionPageCoverage[0].pageBounds, selectionPageCoverage[0].pageIndex);
    auto rectInPluginCoordinates = convertUp(CoordinateSpace::Contents, CoordinateSpace::Plugin, rectInContentsCoordinates);
    auto rectInRootViewCoordinates = convertFromPluginToRootView(enclosingIntRect(rectInPluginCoordinates));

    float deviceScaleFactor = this->deviceScaleFactor();

    auto buffer = ImageBuffer::create(rectInRootViewCoordinates.size(), RenderingPurpose::ShareableSnapshot, deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, { }, nullptr);
    if (!buffer)
        return nullptr;

    auto& context = buffer->context();

    {
        GraphicsContextStateSaver saver(context);

        context.scale(m_scaleFactor);
        context.translate(-rectInContentsCoordinates.location());

        auto layoutRow = m_documentLayout.rowForPageIndex(selectionPageCoverage[0].pageIndex);
        paintPDFContent(nullptr, context, rectInContentsCoordinates, layoutRow, PaintingBehavior::PageContentsOnly);
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

WebCore::DictionaryPopupInfo UnifiedPDFPlugin::dictionaryPopupInfoForSelection(PDFSelection *selection, WebCore::TextIndicatorPresentationTransition presentationTransition)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    if (!selection.string.length)
        return dictionaryPopupInfo;

    NSAttributedString *nsAttributedString = [selection] {
        static constexpr unsigned maximumSelectionLength = 250;
        if (selection.string.length > maximumSelectionLength)
            return [selection.attributedString attributedSubstringFromRange:NSMakeRange(0, maximumSelectionLength)];
        return selection.attributedString;
    }();

    dictionaryPopupInfo.origin = rectForSelectionInRootView(selection).location();
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(nsAttributedString);

    if (auto textIndicator = textIndicatorForSelection(selection, { }, presentationTransition))
        dictionaryPopupInfo.textIndicator = textIndicator->data();

    return dictionaryPopupInfo;
}

bool UnifiedPDFPlugin::performDictionaryLookupAtLocation(const FloatPoint& rootViewPoint)
{
    auto pluginPoint = convertFromRootViewToPlugin(roundedIntPoint(rootViewPoint));
    auto documentPoint = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { pluginPoint });
    auto pageIndex = m_presentationController->pageIndexForDocumentPoint(documentPoint);
    if (!pageIndex)
        return false;

    RetainPtr page = m_documentLayout.pageAtIndex(*pageIndex);
    auto pagePoint = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, documentPoint, *pageIndex);
    RetainPtr lookupSelection = [page selectionForWordAtPoint:pagePoint];
    return showDefinitionForSelection(lookupSelection.get());
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
    auto pageIndex = m_presentationController->pageIndexForDocumentPoint(documentPoint);

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
        FloatRect annotationBoundsInPageSpace = [annotation bounds];

        if (!annotationBoundsInPageSpace.contains(pagePoint))
            continue;

#if PLATFORM(MAC)
        if (m_activeAnnotation && m_activeAnnotation->annotation() == annotation)
            data.isActivePDFAnnotation = true;
#endif

        if (!annotationIsExternalLink(annotation))
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
    revealPage(pageIndex);
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
        auto hitPageIndex = m_presentationController->pageIndexForDocumentPoint(pointInDocumentSpace);
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
        return annotationIsWidgetOfType(annotation, WidgetType::Text) && ![annotation isReadOnly] && [annotation shouldDisplay];
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
    setActiveAnnotation({ WTFMove(nextTextAnnotation) });
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
    setActiveAnnotation({ WTFMove(previousTextAnnotation) });
#endif
}

void UnifiedPDFPlugin::setActiveAnnotation(SetActiveAnnotationParams&& setActiveAnnotationParams)
{
#if PLATFORM(MAC)
    callOnMainRunLoopAndWait([annotation = WTFMove(setActiveAnnotationParams.annotation), isInPluginCleanup = WTFMove(setActiveAnnotationParams.isInPluginCleanup), this] {

        ASSERT(isInPluginCleanup != IsInPluginCleanup::Yes || !annotation, "Must pass a null annotation when cleaning up the plugin");

        if (isInPluginCleanup != IsInPluginCleanup::Yes && !supportsForms())
            return;

        if (isInPluginCleanup != IsInPluginCleanup::Yes && m_activeAnnotation) {
            m_activeAnnotation->commit();
            setNeedsRepaintForAnnotation(m_activeAnnotation->annotation(), repaintRequirementsForAnnotation(m_activeAnnotation->annotation(), IsAnnotationCommit::Yes));
        }

        if (annotation) {
            if (annotationIsWidgetOfType(annotation.get(), WidgetType::Text) && [annotation isReadOnly]) {
                m_activeAnnotation = nullptr;
                return;
            }

            m_activeAnnotation = PDFPluginAnnotation::create(annotation.get(), this);
            protectedActiveAnnotation()->attach(m_annotationContainer.get());
            revealAnnotation(protectedActiveAnnotation()->annotation());
        } else
            m_activeAnnotation = nullptr;
    });
#endif
}

void UnifiedPDFPlugin::revealAnnotation(PDFAnnotation *annotation)
{
    auto pageIndex = pageIndexForAnnotation(annotation);
    if (!pageIndex)
        return;

    revealRectInPage([annotation bounds], *pageIndex);
}

#if PLATFORM(MAC)
void UnifiedPDFPlugin::handlePDFActionForAnnotation(PDFAnnotation *annotation, PDFDocumentLayout::PageIndex currentPageIndex, ShouldPerformGoToAction shouldPerformGoToAction)
{
    if (!annotation)
        return;

    RetainPtr firstAction = [annotation action];
    ASSERT(firstAction);
    if (!firstAction)
        return;

    using PDFActionList = Vector<RetainPtr<PDFAction>>;
    auto performPDFAction = [this, currentPageIndex, annotation, shouldPerformGoToAction](PDFAction *action) {
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
                    revealPage(currentPageIndex + 1);
                break;
            case kPDFActionNamedPreviousPage:
                if (currentPageIndex)
                    revealPage(currentPageIndex - 1);
                break;
            case kPDFActionNamedFirstPage:
                revealPage(0);
                break;
            case kPDFActionNamedLastPage:
                revealPage(m_documentLayout.pageCount() - 1);
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
        } else if ([actionType isEqualToString:@"GoTo"] && shouldPerformGoToAction == ShouldPerformGoToAction::Yes)
            revealPDFDestination([annotation destination]);
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

RepaintRequirements AnnotationTrackingState::startAnnotationTracking(RetainPtr<PDFAnnotation>&& annotation, WebEventType mouseEventType, WebMouseEventButton mouseEventButton)
{
    ASSERT(!m_trackedAnnotation);
    m_trackedAnnotation = WTFMove(annotation);

    auto repaintRequirements = RepaintRequirements { };

    if (annotationIsWidgetOfType(m_trackedAnnotation.get(), WidgetType::Button)) {
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

RepaintRequirements AnnotationTrackingState::finishAnnotationTracking(PDFAnnotation* annotationUnderMouse, WebEventType mouseEventType, WebMouseEventButton mouseEventButton)
{
    ASSERT(m_trackedAnnotation);
    auto repaintRequirements = RepaintRequirements { };

    if (annotationUnderMouse == m_trackedAnnotation && mouseEventType == WebEventType::MouseUp && mouseEventButton == WebMouseEventButton::Left) {
        if ([m_trackedAnnotation isHighlighted]) {
            [m_trackedAnnotation setHighlighted:NO];
            repaintRequirements.add(UnifiedPDFPlugin::repaintRequirementsForAnnotation(m_trackedAnnotation.get()));
        }

        if (annotationIsWidgetOfType(m_trackedAnnotation.get(), WidgetType::Button) && [m_trackedAnnotation widgetControlType] != kPDFWidgetPushButtonControl) {
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

    for (PDFDocumentLayout::PageIndex pageIndex = 0; pageIndex < m_documentLayout.pageCount(); ++pageIndex) {
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


void UnifiedPDFPlugin::setTextAnnotationValueForTesting(unsigned pageIndex, unsigned annotationIndex, const String& value)
{
    if (pageIndex >= m_documentLayout.pageCount())
        return;

    RetainPtr page = m_documentLayout.pageAtIndex(pageIndex);
    RetainPtr annotationsOnPage = [page annotations];
    if (annotationIndex >= [annotationsOnPage count])
        return;

    RetainPtr annotation = [annotationsOnPage objectAtIndex:annotationIndex];
    if (!annotationIsWidgetOfType(annotation.get(), WidgetType::Text))
        return;

    [annotation setWidgetStringValue:value];
    setNeedsRepaintForAnnotation(annotation.get(), repaintRequirementsForAnnotation(annotation.get(), IsAnnotationCommit::Yes));
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

void UnifiedPDFPlugin::setDisplayMode(PDFDocumentLayout::DisplayMode mode)
{
    m_documentLayout.setDisplayMode(mode);

    if (m_presentationController && m_presentationController->supportsDisplayMode(mode)) {
        m_presentationController->willChangeDisplayMode(mode);
        return;
    }

    setPresentationController(PDFPresentationController::createForMode(mode, *this));
}

void UnifiedPDFPlugin::setDisplayModeAndUpdateLayout(PDFDocumentLayout::DisplayMode mode)
{
    auto shouldAdjustPageScale = m_shouldUpdateAutoSizeScale == ShouldUpdateAutoSizeScale::Yes ? AdjustScaleAfterLayout::No : AdjustScaleAfterLayout::Yes;
    bool didWantWheelEvents = m_presentationController->wantsWheelEvents();
    auto anchoringInfo = m_presentationController->pdfPositionForCurrentView();

    setDisplayMode(mode);
    {
        SetForScope scope(m_shouldUpdateAutoSizeScale, ShouldUpdateAutoSizeScale::Yes);
        updateLayout(shouldAdjustPageScale);
    }

    if (anchoringInfo)
        m_presentationController->restorePDFPosition(*anchoringInfo);

    bool wantsWheelEvents = m_presentationController->wantsWheelEvents();
    if (didWantWheelEvents != wantsWheelEvents)
        wantsWheelEventsChanged();
}

TextStream& operator<<(TextStream& ts, RepaintRequirement requirement)
{
    switch (requirement) {
    case RepaintRequirement::PDFContent:
        ts << "PDFContent";
        break;
    case RepaintRequirement::Selection:
        ts << "Selection";
        break;
    case RepaintRequirement::HoverOverlay:
        ts << "HoverOverlay";
        break;
    }
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
