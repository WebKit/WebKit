/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include "DocumentEditingContext.h"
#include "EditorState.h"
#include "FindController.h"
#include "GestureTypes.h"
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
#include "WKAccessibilityWebPageObjectIOS.h"
#include "WKAccessibilityWebPageObjectMac.h"
#include "WebEventConversion.h"
#include "WebEventModifier.h"
#include "WebEventType.h"
#include "WebFrame.h"
#include "WebHitTestResultData.h"
#include "WebKeyboardEvent.h"
#include "WebMouseEvent.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <CoreGraphics/CoreGraphics.h>
#include <PDFKit/PDFKit.h>
#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <WebCore/AXCoreObject.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/AutoscrollController.h>
#include <WebCore/BitmapImage.h>
#include <WebCore/Chrome.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/ColorBlending.h>
#include <WebCore/ColorCocoa.h>
#include <WebCore/ContainerNodeInlines.h>
#include <WebCore/DataDetectorElementInfo.h>
#include <WebCore/DictionaryLookup.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/DocumentView.h>
#include <WebCore/Editor.h>
#include <WebCore/EditorClient.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImmediateActionStage.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/NodeDocument.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderTheme.h>
#include <WebCore/ScreenProperties.h>
#include <WebCore/ScrollAnimator.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollbarTheme.h>
#include <WebCore/ScrollbarsController.h>
#include <WebCore/Settings.h>
#include <WebCore/ShadowRoot.h>
#include <WebCore/StyleColorOptions.h>
#include <WebCore/VoidCallback.h>
#include <WebCore/WheelEventDeltaFilter.h>
#include <algorithm>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/cocoa/TypeCastsCocoa.h>
#include <wtf/spi/darwin/OSVariantSPI.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextStream.h>

#include "PDFKitSoftLink.h"

#if PLATFORM(IOS_FAMILY)
@interface NSObject (AXPriv)
- (id)accessibilityHitTest:(CGPoint)point withPlugin:(id)plugin;
@end
#endif

@interface WKPDFFormMutationObserver : NSObject {
    ThreadSafeWeakPtr<WebKit::UnifiedPDFPlugin> _plugin;
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
    RefPtr plugin = _plugin.get();
    plugin->didMutatePDFDocument();

    RetainPtr fieldName = checked_objc_cast<NSString>([[notification userInfo] objectForKey:@"PDFFormFieldName"]);
    plugin->repaintAnnotationsForFormField(fieldName.get());
}
@end

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIColor.h>
#endif

// FIXME: We should rationalize these with the values in ViewGestureController.
// For now, we'll leave them differing as they do in PDFPlugin.
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
    , m_dataDetectorOverlayController(PDFDataDetectorOverlayController::create(*this))
#endif
{
    this->setVerticalScrollElasticity(ScrollElasticity::Automatic);
    this->setHorizontalScrollElasticity(ScrollElasticity::Automatic);

    Ref document = element.document();
    Ref annotationContainer = document->createElement(HTMLNames::divTag, false);
    m_annotationContainer = annotationContainer.copyRef();
    annotationContainer->setAttributeWithoutSynchronization(HTMLNames::idAttr, "annotationContainer"_s);
    Ref annotationStyleElement = document->createElement(HTMLNames::styleTag, false);
    annotationStyleElement->setTextContent(annotationStyle());
    annotationContainer->appendChild(annotationStyleElement);
    installAnnotationContainer();

    setDisplayMode(PDFDocumentLayout::DisplayMode::SinglePageContinuous);

    lazyInitialize(m_accessibilityDocumentObject, adoptNS([[WKAccessibilityPDFDocumentObject alloc] initWithPDFDocument:m_pdfDocument andElement:&element]));
    [m_accessibilityDocumentObject setPDFPlugin:this];
    RefPtr frame = m_frame.get();
    if (isFullMainFramePlugin())
        [m_accessibilityDocumentObject setParent:frame->protectedPage()->protectedAccessibilityRemoteObject().get()];

    if (protectedPresentationController()->wantsWheelEvents())
        wantsWheelEventsChanged();

    if (shouldSizeToFitContent()) {
        if (RefPtr frameView = frame->coreLocalFrame()->view())
            m_prohibitScrollingDueToContentSizeChanges = frameView->prohibitScrollingWhenChangingContentSizeForScope();
    }
}

void UnifiedPDFPlugin::installAnnotationContainer()
{
    RefPtr annotationContainer = m_annotationContainer;
    if (!annotationContainer) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref element = *m_element;
    Ref document = element->document();

    if (supportsForms()) {
        RefPtr { document->bodyOrFrameset() }->appendChild(*annotationContainer);
        return;
    }

    if (RefPtr existingShadowRoot = element->userAgentShadowRoot())
        existingShadowRoot->removeChildren();

    Ref shadowRoot = element->ensureUserAgentShadowRoot();
    m_shadowRoot = shadowRoot.copyRef();
    shadowRoot->appendChild(*annotationContainer);
    if (CheckedPtr renderer = dynamicDowncast<RenderEmbeddedObject>(element->renderer()))
        renderer->setHasShadowContent();

    document->updateLayoutIgnorePendingStylesheets();
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

    bool wantedWheelEvents = protectedPresentationController()->wantsWheelEvents();
    setPresentationController(nullptr); // Breaks retain cycle.

    if (wantedWheelEvents)
        wantsWheelEventsChanged();

    RefPtr page = this->page();
    RefPtr frame = m_frame.get();
    if (m_scrollingNodeID && page) {
        RefPtr scrollingCoordinator = page->scrollingCoordinator();
        scrollingCoordinator->unparentChildrenAndDestroyNode(*m_scrollingNodeID);
        frame->coreLocalFrame()->protectedView()->removePluginScrollableAreaForScrollingNodeID(*m_scrollingNodeID);
    }

    [[NSNotificationCenter defaultCenter] removeObserver:m_pdfMutationObserver.get() name:mutationObserverNotificationString().createNSString().get() object:m_pdfDocument.get()];
    m_pdfMutationObserver = nullptr;

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    std::exchange(m_dataDetectorOverlayController, nullptr)->teardown();
#endif

    setActiveAnnotation({ nullptr, IsInPluginCleanup::Yes });
    m_annotationContainer = nullptr;

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    if (RefPtr webPage = frame->page())
        webPage->removePDFPageNumberIndicator(*this);
#endif
}

void UnifiedPDFPlugin::setPresentationController(RefPtr<PDFPresentationController>&& newPresentationController)
{
    if (RefPtr presentationController = m_presentationController)
        presentationController->teardown();

    m_presentationController = WTFMove(newPresentationController);
}

LocalFrameView* UnifiedPDFPlugin::frameView() const
{
    if (RefPtr frame = m_frame.get())
        return frame->coreLocalFrame()->view();

    return nullptr;
}

FrameView* UnifiedPDFPlugin::mainFrameView() const
{
    if (!m_frame)
        return nullptr;

    RefPtr webPage = protectedFrame()->page();
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

    auto handlePDFTestCallback = makeScopeExit([testCallback = WTFMove(m_pdfTestCallback)] {
        if (testCallback)
            testCallback->invoke();
    });

    m_documentLayout.setPDFDocument(m_pdfDocument.get());

#if HAVE(INCREMENTAL_PDF_APIS)
    maybeClearHighLatencyDataProviderFlag();
#endif

    updateLayout(AdjustScaleAfterLayout::Yes);

#if ENABLE(PDF_HUD)
    updateHUDVisibility();
#endif

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    updatePageNumberIndicator();
#endif

    if (isLocked())
        createPasswordEntryForm();

    if (RefPtr view = m_view.get())
        view->layerHostingStrategyDidChange();

    [[NSNotificationCenter defaultCenter] addObserver:m_pdfMutationObserver.get() selector:@selector(formChanged:) name:mutationObserverNotificationString().createNSString().get() object:m_pdfDocument.get()];

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    enableDataDetection();
#endif

    if (protectedPresentationController()->wantsWheelEvents())
        wantsWheelEventsChanged();

    revealFragmentIfNeeded();

    if (RefPtr element = m_element.get()) {
        if (RefPtr callback = element->takePendingPDFTestCallback())
            registerPDFTest(WTFMove(callback));
    }

    sizeToFitContentsIfNeeded();
}

bool UnifiedPDFPlugin::shouldSizeToFitContent() const
{
#if PLATFORM(IOS_FAMILY)
    return isFullMainFramePlugin();
#else
    return false;
#endif
}

void UnifiedPDFPlugin::sizeToFitContentsIfNeeded()
{
    if (!shouldSizeToFitContent())
        return;

    if (isLocked())
        return;

    auto size = contentsSize();
    Ref pluginElement = m_view->pluginElement();
    pluginElement->setInlineStyleProperty(CSSPropertyHeight, size.height(), CSSUnitType::CSS_PX);
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
    setNeedsRepaintForIncrementalLoad();
}

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)

void UnifiedPDFPlugin::enableDataDetection()
{
#if HAVE(PDFDOCUMENT_ENABLE_DATA_DETECTORS)
    if ([m_pdfDocument respondsToSelector:@selector(setEnableDataDetectors:)])
        [m_pdfDocument setEnableDataDetectors:YES];
#endif
}

Ref<PDFDataDetectorOverlayController> UnifiedPDFPlugin::protectedDataDetectorOverlayController()
{
    return dataDetectorOverlayController();
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
    auto pageIndex = protectedPresentationController()->pageIndexForDocumentPoint(lastKnownMousePositionInDocumentSpace);
    protectedDataDetectorOverlayController()->didInvalidateHighlightOverlayRects(pageIndex);
}

#endif

// FIXME: Disambiguate between supportsForms/supportsPasswordForm. The former is a slight misnomer now.
bool UnifiedPDFPlugin::supportsPasswordForm() const
{
    return supportsForms() || m_shadowRoot;
}

void UnifiedPDFPlugin::createPasswordEntryForm()
{
    if (!supportsPasswordForm())
        return;

    Ref passwordForm = PDFPluginPasswordForm::create(this);
    m_passwordForm = passwordForm.ptr();
    passwordForm->attach(m_annotationContainer.get());

    if (supportsForms()) {
        Ref passwordField = PDFPluginPasswordField::create(this);
        m_passwordField = passwordField.ptr();
        passwordField->attach(m_annotationContainer.get());
    }
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

    if (![m_pdfDocument unlockWithPassword:password.createNSString().get()]) {
        Ref { *m_passwordField }->resetField();
        Ref { *m_passwordForm }->unlockFailed();
        return;
    }

    m_passwordForm = nullptr;
    m_passwordField = nullptr;

    updateLayout(AdjustScaleAfterLayout::Yes, shouldUpdateAutoSizeScaleOverride);

#if ENABLE(PDF_HUD)
    updateHUDVisibility();
    updateHUDLocation();
#endif

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    updatePageNumberIndicator();
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

    auto selectionCoverage = PDFPageCoverage::from(PerPageInfo { *pageIndex, layoutBoundsForPageAtIndex(*pageIndex), [annotation bounds] });
    protectedPresentationController()->setNeedsRepaintForPageCoverage(repaintRequirements, selectionCoverage);
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

    RefPtr rootLayer = m_rootLayer;
    if (!rootLayer) {
        rootLayer = createGraphicsLayer("UnifiedPDFPlugin root"_s, GraphicsLayer::Type::Normal);
        m_rootLayer = rootLayer.copyRef();
        rootLayer->setAnchorPoint({ });
        rootLayer->setBackgroundColor(pluginBackgroundColor());
        rootLayer->setAppliesPageScale();
    }

    RefPtr scrollContainerLayer = m_scrollContainerLayer;
    if (!scrollContainerLayer) {
        scrollContainerLayer = createGraphicsLayer("UnifiedPDFPlugin scroll container"_s, GraphicsLayer::Type::ScrollContainer);
        m_scrollContainerLayer = scrollContainerLayer.copyRef();
        scrollContainerLayer->setAnchorPoint({ });
        scrollContainerLayer->setMasksToBounds(true);
        rootLayer->addChild(*scrollContainerLayer);
    }

    RefPtr scrolledContentsLayer = m_scrolledContentsLayer;
    if (!scrolledContentsLayer) {
        scrolledContentsLayer = createGraphicsLayer("UnifiedPDFPlugin scrolled contents"_s, GraphicsLayer::Type::ScrolledContents);
        m_scrolledContentsLayer = scrolledContentsLayer.copyRef();
        scrolledContentsLayer->setAnchorPoint({ });
        scrollContainerLayer->addChild(*scrolledContentsLayer);
    }

    if (!m_overflowControlsContainer) {
        RefPtr overflowControlsContainer = createGraphicsLayer("Overflow controls container"_s, GraphicsLayer::Type::Normal);
        m_overflowControlsContainer = overflowControlsContainer.copyRef();
        overflowControlsContainer->setAnchorPoint({ });
        rootLayer->addChild(*overflowControlsContainer);
    }

    protectedPresentationController()->setupLayers(*scrolledContentsLayer);
}

void UnifiedPDFPlugin::incrementalLoadingRepaintTimerFired()
{
    setNeedsRepaintForIncrementalLoad();
}

void UnifiedPDFPlugin::setNeedsRepaintForIncrementalLoad()
{
    if (!m_pdfDocument)
        return;

    PDFPageCoverage pageCoverage;
    pageCoverage.reserveCapacity(m_documentLayout.pageCount());
    for (PDFDocumentLayout::PageIndex i = 0; i < m_documentLayout.pageCount(); ++i) {
        auto pageBounds = layoutBoundsForPageAtIndex(i);
        pageCoverage.append({ i, pageBounds, pageBounds });
    }
    protectedPresentationController()->setNeedsRepaintForPageCoverage({ RepaintRequirement::PDFContent, RepaintRequirement::HoverOverlay, RepaintRequirement::Selection }, pageCoverage);
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
    scrollingCoordinator->createNode(protectedFrame()->coreLocalFrame()->rootFrame().frameID(), ScrollingNodeType::PluginScrolling, *m_scrollingNodeID);

    RefPtr scrollContainerLayer = m_scrollContainerLayer;
#if ENABLE(SCROLLING_THREAD)
    scrollContainerLayer->setScrollingNodeID(*m_scrollingNodeID);

    if (RefPtr layer = layerForHorizontalScrollbar())
        layer->setScrollingNodeID(*m_scrollingNodeID);

    if (RefPtr layer = layerForVerticalScrollbar())
        layer->setScrollingNodeID(*m_scrollingNodeID);

    if (RefPtr layer = layerForScrollCorner())
        layer->setScrollingNodeID(*m_scrollingNodeID);
#endif

    protectedFrame()->coreLocalFrame()->protectedView()->setPluginScrollableAreaForScrollingNodeID(*m_scrollingNodeID, *this);

    scrollingCoordinator->setScrollingNodeScrollableAreaGeometry(*m_scrollingNodeID, *this);

    WebCore::ScrollingCoordinator::NodeLayers nodeLayers;
    nodeLayers.layer = m_rootLayer.get();
    nodeLayers.scrollContainerLayer = scrollContainerLayer.get();
    nodeLayers.scrolledContentsLayer = m_scrolledContentsLayer.get();
    nodeLayers.horizontalScrollbarLayer = layerForHorizontalScrollbar();
    nodeLayers.verticalScrollbarLayer = layerForVerticalScrollbar();

    scrollingCoordinator->setNodeLayers(*m_scrollingNodeID, nodeLayers);
}

void UnifiedPDFPlugin::updateLayerHierarchy()
{
    ensureLayers();

    // The protectedGraphicsLayer()'s position is set in RenderLayerBacking::updateAfterWidgetResize().
    protectedGraphicsLayer()->setSize(size());
    protectedOverflowControlsContainer()->setSize(size());

    auto scrollContainerRect = availableContentsRect();
    Ref scrollContainerLayer = *m_scrollContainerLayer;
    scrollContainerLayer->setPosition(scrollContainerRect.location());
    scrollContainerLayer->setSize(scrollContainerRect.size());

    protectedPresentationController()->updateLayersOnLayoutChange(documentSize(), centeringOffset(), m_scaleFactor);
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
    if (RefPtr rootLayer = m_rootLayer)
        propagateSettingsToLayer(*rootLayer);

    if (RefPtr scrollContainerLayer = m_scrollContainerLayer)
        propagateSettingsToLayer(*scrollContainerLayer);

    if (RefPtr scrolledContentsLayer = m_scrolledContentsLayer)
        propagateSettingsToLayer(*scrolledContentsLayer);

    if (RefPtr layerForHorizontalScrollbar = m_layerForHorizontalScrollbar)
        propagateSettingsToLayer(*layerForHorizontalScrollbar);

    if (RefPtr layerForVerticalScrollbar = m_layerForVerticalScrollbar)
        propagateSettingsToLayer(*layerForVerticalScrollbar);

    if (RefPtr layerForScrollCorner = m_layerForScrollCorner)
        propagateSettingsToLayer(*layerForScrollCorner);

    protectedPresentationController()->updateDebugBorders(showDebugBorders, showRepaintCounter);
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
    if (!m_pdfDocument)
        return;

    RefPtr page = this->page();
    if (!page)
        return;

    bool isInWindow = page->isInWindow();
    protectedPresentationController()->updateIsInWindow(isInWindow);

    if (!isInWindow) {
        RefPtr scrollingCoordinator = page->scrollingCoordinator();
        scrollingCoordinator->scrollableAreaWillBeDetached(*this);
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
    context.fillRect(clipRect, pluginBackgroundColor());
    context.scale(m_scaleFactor);

    auto paddingForCentering = centeringOffset();
    context.translate(paddingForCentering.width(), paddingForCentering.height());

    clipRect.scale(1.0f / m_scaleFactor);

    paintPDFContent(nullptr, context, clipRect, protectedPresentationController()->visibleRow());
}

void UnifiedPDFPlugin::paintContents(const GraphicsLayer& layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
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

    if (&layer == layerForHorizontalScrollbar()) {
        paintScrollbar(m_horizontalScrollbar.get(), context);
        return;
    }

    if (&layer == layerForVerticalScrollbar()) {
        paintScrollbar(m_verticalScrollbar.get(), context);
        return;
    }

    if (&layer == layerForScrollCorner()) {
        auto cornerRect = viewRelativeScrollCornerRect();

        GraphicsContextStateSaver stateSaver(context);
        context.translate(-cornerRect.location());
        ScrollbarTheme::theme().paintScrollCorner(*this, context, cornerRect);
        return;
    }

    // Other layers should be painted by the PDFPresentationController.
    ASSERT_NOT_REACHED();
}

void UnifiedPDFPlugin::paintPDFContent(const WebCore::GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, const std::optional<PDFLayoutRow>& row, AsyncPDFRenderer* asyncRenderer)
{
    if (visibleOrDocumentSizeIsEmpty())
        return;

    RefPtr presentationController = m_presentationController;
    if (!presentationController)
        return;

    auto stateSaver = GraphicsContextStateSaver(context);

    auto showDebugIndicators = shouldShowDebugIndicators();

    auto pageWithAnnotation = pageIndexWithHoveredAnnotation();

    auto tilingScaleFactor = 1.0f;
    if (layer) {
        if (CheckedPtr tiledBacking = layer->tiledBacking())
            tilingScaleFactor = tiledBacking->tilingScaleFactor();
    }

    auto pageCoverage = presentationController->pageCoverageAndScalesForContentsRect(clipRect, row, tilingScaleFactor);
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
        if (asyncRenderer && !currentPageHasAnnotation)
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
            [page drawWithBox:kPDFDisplayBoxCropBox toContext:context.protectedPlatformContext().get()];
        }

        if constexpr (hasFullAnnotationSupport) {
            if (currentPageHasAnnotation) {
                auto pageGeometry = m_documentLayout.geometryForPage(page);
                auto transformForBox = m_documentLayout.toPageTransform(*pageGeometry).inverse().value_or(AffineTransform { });
                GraphicsContextStateSaver stateSaver(context);
                context.concatCTM(transformForBox);
                paintHoveredAnnotationOnPage(pageInfo.pageIndex, context, clipRect);
            }
        }
    }
}

bool UnifiedPDFPlugin::hasSelection() const
{
    return m_currentSelection && ![m_currentSelection isEmpty];
}

void UnifiedPDFPlugin::paintPDFSelection(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, std::optional<PDFLayoutRow> row)
{
    if (!hasSelection())
        return;

    RefPtr presentationController = m_presentationController;
    if (!presentationController)
        return;

    bool isVisibleAndActive = false;
    if (RefPtr page = this->page())
        isVisibleAndActive = page->isVisibleAndActive();

    auto selectionColor = [renderer = CheckedPtr { m_element->renderer() }, isVisibleAndActive] {
        auto& renderTheme = renderer ? renderer->theme() : RenderTheme::singleton();
        OptionSet<StyleColorOptions> styleColorOptions;
        if (renderer)
            styleColorOptions = renderer->styleColorOptions();
        auto selectionColor = isVisibleAndActive ? renderTheme.activeSelectionBackgroundColor(styleColorOptions) : renderTheme.inactiveSelectionBackgroundColor(styleColorOptions);
        return blendSourceOver(Color::white, selectionColor);
    }();

    auto tilingScaleFactor = 1.0f;
    if (CheckedPtr tiledBacking = layer->tiledBacking())
        tilingScaleFactor = tiledBacking->tilingScaleFactor();

    auto pageCoverage = presentationController->pageCoverageAndScalesForContentsRect(clipRect, row, tilingScaleFactor);
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

#if HAVE(PDFSELECTION_ENUMERATE_RECTS_AND_TRANSFORMS)
        [protectedCurrentSelection() enumerateRectsAndTransformsForPage:page.get() usingBlock:[&context, &selectionColor](CGRect cgRect, CGAffineTransform cgTransform) mutable {
            // FIXME: Perf optimization -- consider coalescing rects by transform.
            GraphicsContextStateSaver individualRectTransformPairStateSaver { context, /* saveAndRestore */ false };

            if (AffineTransform transform { cgTransform }; !transform.isIdentity()) {
                individualRectTransformPairStateSaver.save();
                context.concatCTM(transform);
            }

            context.fillRect({ cgRect }, selectionColor);
        }];
#endif
    }
}

static const WebCore::Color textAnnotationHoverColor()
{
    static constexpr auto textAnnotationHoverAlpha = 0.12;
    static NeverDestroyed color = RenderTheme::singleton().systemColor(CSSValueAppleSystemBlue, { }).colorWithAlpha(textAnnotationHoverAlpha);
    return color.get();
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

    RefPtr frame = m_frame.get();
    if (!frame || !frame->coreLocalFrame())
        return 1;

    RefPtr webPage = frame->page();
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
    protectedDataDetectorOverlayController()->hideActiveHighlightOverlay();
#endif
}

void UnifiedPDFPlugin::didEndMagnificationGesture()
{
    m_inMagnificationGesture = false;
    m_magnificationOriginInContentCoordinates = { };
    m_magnificationOriginInPluginCoordinates = { };

    using enum CheckForMagnificationGesture;
    deviceOrPageScaleFactorChanged(handlesPageScaleFactor() ? Yes : No);
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

    protectedPresentationController()->updateLayersOnLayoutChange(documentSize(), centeringOffset(), m_scaleFactor);
    updateSnapOffsets();

#if PLATFORM(MAC)
    if (RefPtr activeAnnotation = m_activeAnnotation)
        activeAnnotation->updateGeometry();
#endif

    if (scrollingMode() == DelegatedScrollingMode::NotDelegated) {
        auto scrolledContentsPoint = roundedIntPoint(convertUp(CoordinateSpace::Contents, CoordinateSpace::ScrolledContents, FloatPoint { zoomContentsOrigin }));
        auto newScrollPosition = IntPoint { scrolledContentsPoint - originInPluginCoordinates };
        newScrollPosition = newScrollPosition.expandedTo({ 0, 0 });

        scrollToPointInContentsSpace(newScrollPosition);
    }

    scheduleRenderingUpdate();

    protectedView()->pluginScaleFactorDidChange();

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::setScaleFactor " << scale << " - new scale factor " << m_scaleFactor << " (exposed as normalized scale factor " << scaleFactor() << ") normalization factor " << m_scaleNormalizationFactor << " layout scale " << m_documentLayout.scale());
}

void UnifiedPDFPlugin::setPageScaleFactor(double scale, std::optional<WebCore::IntPoint> origin)
{
    deviceOrPageScaleFactorChanged(CheckForMagnificationGesture::Yes);
    if (!handlesPageScaleFactor()) {
        mainFramePageScaleFactorDidChange();
        return;
    }

    if (origin) {
        // Compensate for the subtraction of content insets that happens in ViewGestureController::handleMagnificationGestureEvent();
        // origin is not in root view coordinates.
        if (RefPtr frameView = protectedFrame()->coreLocalFrame()->view()) {
            auto obscuredContentInsets = frameView->obscuredContentInsets();
            origin->move(std::round(obscuredContentInsets.left()), std::round(obscuredContentInsets.top()));
        }
    }

    if (scale != 1.0 && !shouldSizeToFitContent())
        m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;

    // FIXME: Make the overlay scroll with the tiles instead of repainting constantly.
    updateFindOverlay(HideFindIndicator::Yes);

    auto internalScale = fromNormalizedScaleFactor(scale);
    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::setPageScaleFactor " << scale << " mapped to " << internalScale);
    setScaleFactor(internalScale, origin);
}

void UnifiedPDFPlugin::mainFramePageScaleFactorDidChange()
{
    if (handlesPageScaleFactor()) {
        ASSERT_NOT_REACHED();
        return;
    }
    updateScrollingExtents();
}

bool UnifiedPDFPlugin::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    bool sizeChanged = pluginSize != m_size;

#if PLATFORM(IOS_FAMILY)
    if (sizeChanged && hasSelection()) {
        if (RefPtr webPage = this->webPage())
            webPage->scheduleFullEditorStateUpdate();
    }
#endif

    if (!PDFPluginBase::geometryDidChange(pluginSize, pluginToRootViewTransform))
        return false;

#if PLATFORM(MAC)
    if (RefPtr activeAnnotation = m_activeAnnotation)
        activeAnnotation->updateGeometry();
#endif

    if (sizeChanged)
        updateLayout();

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    updatePageNumberIndicator();
#endif

    return true;
}

void UnifiedPDFPlugin::visibilityDidChange(bool)
{
    PDFPluginBase::visibilityDidChange(true);

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    updatePageNumberIndicator();
#endif
}

void UnifiedPDFPlugin::deviceOrPageScaleFactorChanged(CheckForMagnificationGesture checkForMagnificationGesture)
{
    bool gestureAllowsScaleUpdate = checkForMagnificationGesture == CheckForMagnificationGesture::No || !m_inMagnificationGesture;

    if (!handlesPageScaleFactor() || gestureAllowsScaleUpdate)
        protectedGraphicsLayer()->noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    if (gestureAllowsScaleUpdate)
        protectedPresentationController()->deviceOrPageScaleFactorChanged();
}

IntRect UnifiedPDFPlugin::availableContentsRect() const
{
    auto availableRect = IntRect({ }, size());
    if (ScrollbarTheme::theme().usesOverlayScrollbars())
        return availableRect;

    int verticalScrollbarSpace = 0;
    if (RefPtr verticalScrollbar = m_verticalScrollbar)
        verticalScrollbarSpace = verticalScrollbar->width();

    int horizontalScrollbarSpace = 0;
    if (RefPtr horizontalScrollbar = m_horizontalScrollbar)
        horizontalScrollbarSpace = horizontalScrollbar->height();

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

    Ref presentationController = *m_presentationController;
    auto computeAnchoringInfo = [&] {
        return presentationController->pdfPositionForCurrentView(PDFPresentationController::AnchorPoint::TopLeft, shouldAdjustScale == AdjustScaleAfterLayout::Yes || autoSizeMode == ShouldUpdateAutoSizeScale::Yes);
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

        if (!shouldSizeToFitContent())
            m_shouldUpdateAutoSizeScale = ShouldUpdateAutoSizeScale::No;
    }

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::updateLayout - scale " << m_scaleFactor << " normalization factor " << m_scaleNormalizationFactor << " layout scale " << m_documentLayout.scale());

    constexpr OptionSet allLayoutChangeTypes {
        PDFDocumentLayout::LayoutUpdateChange::PageGeometries,
        PDFDocumentLayout::LayoutUpdateChange::DocumentBounds,
    };
    if (layoutUpdateChanges.containsAll(allLayoutChangeTypes)) {
        anchoringInfo = anchoringInfo.and_then([&](auto&) {
            return computeAnchoringInfo();
        });
    }

    if (anchoringInfo && !shouldSizeToFitContent())
        presentationController->restorePDFPosition(*anchoringInfo);

    sizeToFitContentsIfNeeded();
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

void UnifiedPDFPlugin::releaseMemory()
{
    if (RefPtr presentationController = m_presentationController)
        presentationController->releaseMemory();

    m_webFoundTextRangePDFDataSelectionMap.clear();
}

void UnifiedPDFPlugin::didChangeScrollOffset()
{
    if (this->currentScrollType() == ScrollType::User)
        protectedScrollContainerLayer()->syncBoundsOrigin(IntPoint(m_scrollOffset));
    else
        protectedScrollContainerLayer()->setBoundsOrigin(IntPoint(m_scrollOffset));

#if PLATFORM(MAC)
    if (RefPtr activeAnnotation = m_activeAnnotation)
        activeAnnotation->updateGeometry();
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

    protectedPresentationController()->updateForCurrentScrollability(computeScrollability());
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
            layer->setAcceleratesDrawing(!shouldUseInProcessBackingStore());

#if ENABLE(SCROLLING_THREAD)
            layer->setScrollingNodeID(m_scrollingNodeID);
#endif

            protectedOverflowControlsContainer()->addChild(*layer);
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

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    if (horizontalScrollbarLayerChanged)
        scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(*this, ScrollbarOrientation::Horizontal);
    if (verticalScrollbarLayerChanged)
        scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(*this, ScrollbarOrientation::Vertical);

    return layersChanged;
}

void UnifiedPDFPlugin::positionOverflowControlsLayers()
{
    auto overflowControlsPositioningRect = IntRect({ }, size());

    auto positionScrollbarLayer = [](GraphicsLayer& layer, const IntRect& scrollbarRect) {
        layer.setPosition(scrollbarRect.location());
        layer.setSize(scrollbarRect.size());
    };

    if (RefPtr layer = layerForHorizontalScrollbar())
        positionScrollbarLayer(*layer, viewRelativeHorizontalScrollbarRect());

    if (RefPtr layer = layerForVerticalScrollbar())
        positionScrollbarLayer(*layer, viewRelativeVerticalScrollbarRect());

    if (RefPtr layer = layerForScrollCorner()) {
        auto cornerRect = viewRelativeScrollCornerRect();
        layer->setPosition(cornerRect.location());
        layer->setSize(cornerRect.size());
        layer->setDrawsContent(!cornerRect.isEmpty());
        layer->setAcceleratesDrawing(!shouldUseInProcessBackingStore());
    }
}

void UnifiedPDFPlugin::invalidateScrollbarRect(WebCore::Scrollbar& scrollbar, const WebCore::IntRect& rect)
{
    if (&scrollbar == m_verticalScrollbar.get()) {
        if (RefPtr layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }

        return;
    }

    if (&scrollbar == m_horizontalScrollbar.get()) {
        if (RefPtr layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
        return;
    }
}

void UnifiedPDFPlugin::invalidateScrollCornerRect(const WebCore::IntRect& rect)
{
    if (RefPtr layer = layerForScrollCorner()) {
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
    if (!m_cachedIsFullMainFramePlugin) [[unlikely]] {
        m_cachedIsFullMainFramePlugin = [&] {
            RefPtr frame = m_frame.get();
            if (!frame)
                return false;

            return frame->isMainFrame() && isFullFramePlugin();
        }();
    }

    return *m_cachedIsFullMainFramePlugin;
}

bool UnifiedPDFPlugin::shouldCachePagePreviews() const
{
    // Only main frame plugins are hooked up to releaseMemory().
    return isFullFramePlugin();
}

OptionSet<TiledBackingScrollability> UnifiedPDFPlugin::computeScrollability() const
{
    if (shouldSizeToFitContent()) {
        RefPtr frameView = protectedFrame()->coreLocalFrame()->view();
        if (frameView)
            return frameView->computeScrollability();
    }

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

    RefPtr verticalScrollbar = m_verticalScrollbar;
    bool hasHorizontalScrollbar = !!m_horizontalScrollbar;
    bool hasVerticalScrollbar = !!verticalScrollbar.get();
    bool showsScrollCorner = hasHorizontalScrollbar && hasVerticalScrollbar && !verticalScrollbar->isOverlayScrollbar();
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

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    scrollingCoordinator->setScrollingNodeScrollableAreaGeometry(m_scrollingNodeID, *this);

    if (RefPtr presentationController = m_presentationController)
        presentationController->updateForCurrentScrollability(computeScrollability());

    CheckedPtr renderer = m_element->renderer();
    if (!renderer)
        return;

    RefPtr scrollContainerLayer = m_scrollContainerLayer;
    if (!scrollContainerLayer)
        return;

    EventRegion eventRegion;
    auto eventRegionContext = eventRegion.makeContext();
    eventRegionContext.unite(FloatRoundedRect(FloatRect({ }, size())), *renderer, renderer->checkedStyle().get());
    scrollContainerLayer->setEventRegion(WTFMove(eventRegion));
}

bool UnifiedPDFPlugin::requestScrollToPosition(const ScrollPosition& position, const ScrollPositionChangeOptions& options)
{
    RefPtr page = this->page();
    if (!page)
        return false;

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    return scrollingCoordinator->requestScrollToPosition(*this, position, options);
}

bool UnifiedPDFPlugin::requestStartKeyboardScrollAnimation(const KeyboardScroll& scrollData)
{
    RefPtr page = this->page();
    if (!page)
        return false;

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    return scrollingCoordinator->requestStartKeyboardScrollAnimation(*this, scrollData);
}

bool UnifiedPDFPlugin::requestStopKeyboardScrollAnimation(bool immediate)
{
    RefPtr page = this->page();
    if (!page)
        return false;

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    return scrollingCoordinator->requestStopKeyboardScrollAnimation(*this, immediate);
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
    return protectedPresentationController()->nearestPageIndexForDocumentPoint(centerInDocumentSpace);
}

RetainPtr<PDFAnnotation> UnifiedPDFPlugin::annotationForRootViewPoint(const IntPoint& point) const
{
    auto pointInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { convertFromRootViewToPlugin(point) });
    auto pageIndex = protectedPresentationController()->pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!pageIndex)
        return nullptr;

    auto page = m_documentLayout.pageAtIndex(pageIndex.value());
    return [page annotationAtPoint:convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex.value())];
}

FloatRect UnifiedPDFPlugin::convertFromContentsToPainting(const FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    return protectedPresentationController()->convertFromContentsToPainting(rect, pageIndex);
}

FloatRect UnifiedPDFPlugin::convertFromPaintingToContents(const FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    return protectedPresentationController()->convertFromPaintingToContents(rect, pageIndex);
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, UnifiedPDFPlugin::PDFElementType elementType)
{
    switch (elementType) {
    case UnifiedPDFPlugin::PDFElementType::Page: ts << "page"_s; break;
    case UnifiedPDFPlugin::PDFElementType::Text: ts << "text"_s; break;
    case UnifiedPDFPlugin::PDFElementType::Annotation: ts << "annotation"_s; break;
    case UnifiedPDFPlugin::PDFElementType::Link: ts << "link"_s; break;
    case UnifiedPDFPlugin::PDFElementType::Control: ts << "control"_s; break;
    case UnifiedPDFPlugin::PDFElementType::TextField: ts << "text field"_s; break;
    case UnifiedPDFPlugin::PDFElementType::Icon: ts << "icon"_s; break;
    case UnifiedPDFPlugin::PDFElementType::Popup: ts << "popup"_s; break;
    case UnifiedPDFPlugin::PDFElementType::Image: ts << "image"_s; break;
    }
    return ts;
}
#endif

static BOOL annotationIsExternalLink(PDFAnnotation *annotation)
{
    if (!annotationIsOfType(annotation, AnnotationType::Link))
        return NO;

    return !![annotation URL];
}

static BOOL annotationIsLinkWithDestination(PDFAnnotation *annotation)
{
    if (!annotationIsOfType(annotation, AnnotationType::Link))
        return NO;

    return [annotation URL] || [annotation destination];
}

auto UnifiedPDFPlugin::pdfElementTypesForPluginPoint(const IntPoint& point) const -> PDFElementTypes
{
    auto pointInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, point);
    auto hitPageIndex = protectedPresentationController()->pageIndexForDocumentPoint(pointInDocumentSpace);
    if (!hitPageIndex || *hitPageIndex >= m_documentLayout.pageCount())
        return { };

    auto pageIndex = *hitPageIndex;
    RetainPtr page = m_documentLayout.pageAtIndex(pageIndex);
    auto pointInPDFPageSpace = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex);

    return pdfElementTypesForPagePoint(roundedIntPoint(pointInPDFPageSpace), page.get());
}

auto UnifiedPDFPlugin::pdfElementTypesForPagePoint(const IntPoint& pointInPDFPageSpace, PDFPage *page) const -> PDFElementTypes
{
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

    if (RetainPtr annotation = [page annotationAtPoint:pointInPDFPageSpace]) {
        pdfElementTypes.add(PDFElementType::Annotation);

        if (annotationIsLinkWithDestination(annotation.get()))
            pdfElementTypes.add(PDFElementType::Link);

        if (annotationIsOfType(annotation.get(), AnnotationType::Popup))
            pdfElementTypes.add(PDFElementType::Popup);

        if (annotationIsOfType(annotation.get(), AnnotationType::Text))
            pdfElementTypes.add(PDFElementType::Icon);

        if (![annotation isReadOnly]) {
            if (annotationIsWidgetOfType(annotation.get(), WidgetType::Text))
                pdfElementTypes.add(PDFElementType::TextField);
            if (annotationIsWidgetOfType(annotation.get(), WidgetType::Button))
                pdfElementTypes.add(PDFElementType::Control);
        }
    }

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::pdfElementTypesForPage " << pointInPDFPageSpace << " - elements " << pdfElementTypes);

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
    auto stopStateTrackingIfNeeded = makeScopeExit([this, protectedThis = Ref { *this }, isMouseUp = event.type() == WebEventType::MouseUp] {
        if (isMouseUp) {
            stopTrackingSelection();
            stopAutoscroll();
        }
    });

    auto pointInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, lastKnownMousePositionInView());
    Ref presentationController = *m_presentationController;
    auto pageIndex = presentationController->nearestPageIndexForDocumentPoint(pointInDocumentSpace);
    auto pointInPageSpace = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocumentSpace, pageIndex);

    auto mouseEventButton = event.button();
    auto mouseEventType = event.type();
    // Context menu events always call handleContextMenuEvent as well.
    if (mouseEventType == WebEventType::MouseDown && isContextMenuEvent(event)) {
        bool contextMenuEventIsInsideDocumentBounds = presentationController->pageIndexForDocumentPoint(pointInDocumentSpace).has_value();
        if (contextMenuEventIsInsideDocumentBounds)
            beginTrackingSelection(pageIndex, pointInPageSpace, event);
        return true;
    }

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    if (protectedDataDetectorOverlayController()->handleMouseEvent(event, pageIndex))
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

            RetainPtr annotationUnderMouse = annotationForRootViewPoint(flooredIntPoint(event.position()));
            if (RetainPtr currentTrackedAnnotation = m_annotationTrackingState.trackedAnnotation(); (currentTrackedAnnotation && currentTrackedAnnotation.get() != annotationUnderMouse) || (currentTrackedAnnotation.get() && !m_annotationTrackingState.isBeingHovered()))
                finishTrackingAnnotation(annotationUnderMouse.get(), mouseEventType, mouseEventButton, RepaintRequirement::HoverOverlay);

            if (!m_annotationTrackingState.trackedAnnotation() && annotationUnderMouse && annotationIsWidgetOfType(annotationUnderMouse.get(), WidgetType::Text) && supportsForms())
                startTrackingAnnotation(WTFMove(annotationUnderMouse), mouseEventType, mouseEventButton);

            return true;
        }
        case WebMouseEventButton::Left: {
            if (RetainPtr trackedAnnotation = m_annotationTrackingState.trackedAnnotation()) {
                RetainPtr annotationUnderMouse = annotationForRootViewPoint(flooredIntPoint(event.position()));
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
            if (RetainPtr<PDFAnnotation> annotation = annotationForRootViewPoint(flooredIntPoint(event.position()))) {
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
                RetainPtr annotationUnderMouse = annotationForRootViewPoint(flooredIntPoint(event.position()));
                finishTrackingAnnotation(annotationUnderMouse.get(), mouseEventType, mouseEventButton);

                bool shouldFollowLinkAnnotation = [frame = m_frame] {
                    if (!frame || !frame->coreLocalFrame())
                        return true;
#if USE(UICONTEXTMENU)
                    if (RefPtr webPage = frame->page(); webPage && webPage->hasActiveContextMenuInteraction())
                        return false;
#endif
                    auto immediateActionStage = frame->protectedCoreLocalFrame()->eventHandler().immediateActionStage();
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

    RefPtr presentationController = m_presentationController;
    if (!presentationController)
        return false;

    return presentationController->wantsWheelEvents();
}

bool UnifiedPDFPlugin::handleWheelEvent(const WebWheelEvent& wheelEvent)
{
    auto handledByPresentationController = protectedPresentationController()->handleWheelEvent(wheelEvent);

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin::handleWheelEvent " << platform(wheelEvent) << " - handledByPresentationController " << handledByPresentationController);

    if (handledByPresentationController)
        return true;

    return handleWheelEventForScrolling(platform(wheelEvent), { });
}

PlatformWheelEvent UnifiedPDFPlugin::wheelEventCopyWithVelocity(const PlatformWheelEvent& wheelEvent) const
{
    if (!isFullMainFramePlugin())
        return wheelEvent;

    RefPtr webPage = protectedFrame()->page();
    if (!webPage)
        return wheelEvent;

    return webPage->corePage()->wheelEventDeltaFilter()->eventCopyWithVelocity(wheelEvent);
}

bool UnifiedPDFPlugin::handleContextMenuEvent(const WebMouseEvent& event)
{
#if ENABLE(CONTEXT_MENUS)
    RefPtr frame = m_frame.get();
    RefPtr webPage = frame ? frame->page() : nullptr;
    if (!webPage)
        return false;

    auto contextMenu = createContextMenu(event);
    if (!contextMenu)
        return false;

    webPage->sendWithAsyncReply(Messages::WebPageProxy::ShowPDFContextMenu { *contextMenu, identifier(), frame->frameID() }, [eventPosition = event.position(), weakThis = WeakPtr { *this }](std::optional<int32_t>&& selectedItemTag) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (selectedItemTag)
            protectedThis->performContextMenuAction(toContextMenuItemTag(selectedItemTag.value()), flooredIntPoint(eventPosition));
        protectedThis->stopTrackingSelection();
    });

    return true;
#else
    return false;
#endif // ENABLE(CONTEXT_MENUS)
}

bool UnifiedPDFPlugin::handleKeyboardEvent(const WebKeyboardEvent& event)
{
    return protectedPresentationController()->handleKeyboardEvent(event);
}

void UnifiedPDFPlugin::followLinkAnnotation(PDFAnnotation *annotation, std::optional<PlatformMouseEvent>&& event)
{
    ASSERT(annotationIsLinkWithDestination(annotation));
    if (RetainPtr<NSURL> url = [annotation URL])
        navigateToURL(url.get(), WTFMove(event));
    else if (RetainPtr<PDFDestination> destination = [annotation destination])
        revealPDFDestination(destination.get());
}

RepaintRequirements UnifiedPDFPlugin::repaintRequirementsForAnnotation(PDFAnnotation *annotation, IsAnnotationCommit isAnnotationCommit)
{
    if (annotationIsWidgetOfType(annotation, WidgetType::Button))
        return RepaintRequirement::PDFContent;

    if (annotationIsOfType(annotation, AnnotationType::Popup))
        return RepaintRequirement::PDFContent;

    if (annotationIsWidgetOfType(annotation, WidgetType::Choice))
        return RepaintRequirement::PDFContent;

    if (annotationIsOfType(annotation, AnnotationType::Text))
        return RepaintRequirement::PDFContent;

    if (annotationIsWidgetOfType(annotation, WidgetType::Text))
        return isAnnotationCommit == IsAnnotationCommit::Yes ? RepaintRequirement::PDFContent : RepaintRequirement::HoverOverlay;

    // No visual feedback for PDFAnnotationSubtypeLink at this time.

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
    setNeedsRepaintForAnnotation(m_annotationTrackingState.protectedTrackedAnnotation().get(), repaintRequirements);
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

void UnifiedPDFPlugin::finishTrackingAnnotation(PDFAnnotation *annotationUnderMouse, WebEventType mouseEventType, WebMouseEventButton mouseEventButton, RepaintRequirements repaintRequirements)
{
    // AnnotationTrackingState::finishAnnotationTracking() will clear this, so hold on to it.
    RetainPtr previouslyTrackedAnnotation = m_annotationTrackingState.trackedAnnotation();
    repaintRequirements.add(m_annotationTrackingState.finishAnnotationTracking(annotationUnderMouse, mouseEventType, mouseEventButton));
    setNeedsRepaintForAnnotation(previouslyTrackedAnnotation.get(), repaintRequirements);
}

// FIXME: <https://webkit.org/b/276981>  Assumes scrolling.

void UnifiedPDFPlugin::revealPDFDestination(PDFDestination *destination)
{
    auto unspecifiedValue = get_PDFKit_kPDFDestinationUnspecifiedValueSingleton();

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
    protectedPresentationController()->ensurePageIsVisible(pageIndex);
    auto contentsRect = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::ScrolledContents, pageRect, pageIndex);

    auto pluginRectInContentsCoordinates = convertDown(CoordinateSpace::Plugin, CoordinateSpace::ScrolledContents, FloatRect { { 0, 0 }, size() });
    auto rectToExpose = getRectToExposeForScrollIntoView(LayoutRect(pluginRectInContentsCoordinates), LayoutRect(contentsRect), ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, std::nullopt);

    scrollToPointInContentsSpace(rectToExpose.location());
}

void UnifiedPDFPlugin::revealPointInPage(FloatPoint pointInPDFPageSpace, PDFDocumentLayout::PageIndex pageIndex)
{
    protectedPresentationController()->ensurePageIsVisible(pageIndex);
    auto contentsPoint = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::ScrolledContents, pointInPDFPageSpace, pageIndex);
    scrollToPointInContentsSpace(contentsPoint);
}

bool UnifiedPDFPlugin::revealPage(PDFDocumentLayout::PageIndex pageIndex)
{
    ASSERT(pageIndex < m_documentLayout.pageCount());
    protectedPresentationController()->ensurePageIsVisible(pageIndex);

    auto pageBounds = m_documentLayout.layoutBoundsForPageAtIndex(pageIndex);
    auto boundsInScrolledContents = convertUp(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::ScrolledContents, pageBounds);
    return scrollToPointInContentsSpace(boundsInScrolledContents.location());
}

bool UnifiedPDFPlugin::scrollToPointInContentsSpace(FloatPoint pointInContentsSpace)
{
    if (shouldSizeToFitContent()) {
        RefPtr webPage = protectedFrame()->page();
        if (!webPage)
            return false;

        auto pluginPoint = convertUp(CoordinateSpace::ScrolledContents, CoordinateSpace::Plugin, pointInContentsSpace);
        auto rootViewPoint = convertFromPluginToRootView(pluginPoint);
        webPage->scrollToRect({ rootViewPoint, FloatSize { } }, { });
        return true;
    }

    auto oldScrollType = currentScrollType();
    setCurrentScrollType(ScrollType::Programmatic);
    bool success = scrollToPositionWithoutAnimation(roundedIntPoint(pointInContentsSpace));
    setCurrentScrollType(oldScrollType);
    // We assume that callers have ensured the correct page is visible,
    // so this should always return true for discrete display modes.
    return isInDiscreteDisplayMode() || success;
}

void UnifiedPDFPlugin::revealFragmentIfNeeded()
{
    if (!m_pdfDocument || !m_didAttachScrollingTreeNode || isLocked())
        return;

    if (m_didScrollToFragment)
        return;

    m_didScrollToFragment = true;

    RefPtr frame = m_frame.get();
    if (!frame)
        return;

    auto frameURL = frame->url();
    auto fragmentView = frameURL.fragmentIdentifier();
    if (!fragmentView)
        return;

    // Only respect the first fragment component.
    if (auto endOfFirstComponentLocation = fragmentView.find('&'); endOfFirstComponentLocation != notFound)
        fragmentView = fragmentView.left(endOfFirstComponentLocation);

    // Ignore leading hashes.
    auto isNotHash = [](char16_t character) {
        return character != '#';
    };
    if (auto firstNonHashLocation = fragmentView.find(isNotHash); firstNonHashLocation != notFound)
        fragmentView = fragmentView.substring(firstNonHashLocation);
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
        if (RetainPtr destination = [m_pdfDocument namedDestination:remainder->createNSString().get()])
            revealPDFDestination(destination.get());
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

ContextMenuAction UnifiedPDFPlugin::contextMenuActionFromTag(ContextMenuItemTag tag) const
{
    switch (tag) {
    case ContextMenuItemTag::ActualSize:
        return ContextMenuItemPDFActualSize;
    case ContextMenuItemTag::AutoSize:
        return ContextMenuItemPDFAutoSize;
    case ContextMenuItemTag::Copy:
        return ContextMenuItemTagCopy;
    case ContextMenuItemTag::CopyLink:
        return ContextMenuItemTagCopyLinkToClipboard;
    case ContextMenuItemTag::DictionaryLookup:
        return ContextMenuItemTagLookUpInDictionary;
    case ContextMenuItemTag::Invalid:
    case ContextMenuItemTag::Unknown:
        return ContextMenuItemTagNoAction;
    case ContextMenuItemTag::NextPage:
        return ContextMenuItemPDFNextPage;
    case ContextMenuItemTag::OpenWithDefaultViewer:
        return ContextMenuItemTagOpenWithDefaultApplication;
    case ContextMenuItemTag::PreviousPage:
        return ContextMenuItemPDFPreviousPage;
    case ContextMenuItemTag::SinglePage:
        return ContextMenuItemPDFSinglePage;
    case ContextMenuItemTag::SinglePageContinuous:
        return ContextMenuItemPDFSinglePageContinuous;
    case ContextMenuItemTag::TwoPages:
        return ContextMenuItemPDFTwoPages;
    case ContextMenuItemTag::TwoPagesContinuous:
        return ContextMenuItemPDFTwoPagesContinuous;
    case ContextMenuItemTag::WebSearch:
        return ContextMenuItemTagSearchWeb;
    case ContextMenuItemTag::ZoomIn:
        return ContextMenuItemPDFZoomIn;
    case ContextMenuItemTag::ZoomOut:
        return ContextMenuItemPDFZoomOut;
    }

    return ContextMenuItemTagNoAction;
}

auto UnifiedPDFPlugin::toContextMenuItemTag(int tagValue) -> ContextMenuItemTag
{
    static constexpr std::array regularContextMenuItemTags {
        ContextMenuItemTag::AutoSize,
        ContextMenuItemTag::WebSearch,
        ContextMenuItemTag::DictionaryLookup,
        ContextMenuItemTag::Copy,
        ContextMenuItemTag::CopyLink,
        ContextMenuItemTag::NextPage,
        ContextMenuItemTag::OpenWithDefaultViewer,
        ContextMenuItemTag::PreviousPage,
        ContextMenuItemTag::SinglePage,
        ContextMenuItemTag::SinglePageContinuous,
        ContextMenuItemTag::TwoPages,
        ContextMenuItemTag::TwoPagesContinuous,
        ContextMenuItemTag::ZoomIn,
        ContextMenuItemTag::ZoomOut,
        ContextMenuItemTag::ActualSize,
    };
    const auto isKnownContextMenuItemTag = std::ranges::any_of(regularContextMenuItemTags, [tagValue](ContextMenuItemTag tag) {
        return tagValue == enumToUnderlyingType(tag);
    });
    return isKnownContextMenuItemTag ? static_cast<ContextMenuItemTag>(tagValue) : ContextMenuItemTag::Unknown;
}

static bool isInRecoveryOS()
{
    return os_variant_is_basesystem("WebKit");
}

std::optional<PDFContextMenu> UnifiedPDFPlugin::createContextMenu(const WebMouseEvent& contextMenuEvent) const
{
    ASSERT(isContextMenuEvent(contextMenuEvent));

    RefPtr frame = m_frame.get();
    if (!frame || !frame->coreLocalFrame())
        return std::nullopt;

    RefPtr frameView = frame->coreLocalFrame()->view();
    if (!frameView)
        return std::nullopt;

    auto contextMenuEventRootViewPoint = flooredIntPoint(contextMenuEvent.position());

    Vector<PDFContextMenuItem> menuItems;

    auto addSeparator = [item = separatorContextMenuItem(), &menuItems] {
        menuItems.append(item);
    };

    if ([m_pdfDocument allowsCopying] && hasSelection()) {
        bool shouldPresentLookupAndSearchOptions = !isInRecoveryOS();
        menuItems.appendVector(selectionContextMenuItems(contextMenuEventRootViewPoint, shouldPresentLookupAndSearchOptions));
        addSeparator();
    }

    std::optional<int> openInDefaultViewerTag;
    bool shouldPresentOpenWithDefaultViewerOption = !isInRecoveryOS();
    if (shouldPresentOpenWithDefaultViewerOption) {
        menuItems.append(contextMenuItem(ContextMenuItemTag::OpenWithDefaultViewer));
        openInDefaultViewerTag = enumToUnderlyingType(ContextMenuItemTag::OpenWithDefaultViewer);
    }

    addSeparator();

    menuItems.appendVector(scaleContextMenuItems());

    addSeparator();

    menuItems.appendVector(displayModeContextMenuItems());

    addSeparator();

    auto contextMenuEventPluginPoint = convertFromRootViewToPlugin(contextMenuEventRootViewPoint);
    // FIXME: <https://webkit.org/b/276981> Fix for rows.
    auto contextMenuEventDocumentPoint = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, contextMenuEventPluginPoint);
    menuItems.appendVector(navigationContextMenuItemsForPageAtIndex(protectedPresentationController()->nearestPageIndexForDocumentPoint(contextMenuEventDocumentPoint)));

    auto contextMenuPoint = frameView->contentsToScreen(IntRect(frameView->windowToContents(contextMenuEventRootViewPoint), IntSize())).location();

    return PDFContextMenu { contextMenuPoint, WTFMove(menuItems), WTFMove(openInDefaultViewerTag) };
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
    // The title for the OpenWithDefaultViewer item is determined in the UI Process.
    case ContextMenuItemTag::OpenWithDefaultViewer:
        return ""_s;
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

        bool disableItemDueToLockedDocument = isLocked() && tag != ContextMenuItemTag::OpenWithDefaultViewer;
        auto itemEnabled = disableItemDueToLockedDocument ? ContextMenuItemEnablement::Disabled : ContextMenuItemEnablement::Enabled;
        auto itemHasAction = hasAction && !disableItemDueToLockedDocument ? ContextMenuItemHasAction::Yes : ContextMenuItemHasAction::No;

        return { titleForContextMenuItemTag(tag), state, enumToUnderlyingType(tag), contextMenuActionFromTag(tag), itemEnabled, itemHasAction, ContextMenuItemIsSeparator::No };
    }
    }
}

PDFContextMenuItem UnifiedPDFPlugin::separatorContextMenuItem() const
{
    return { { }, 0, enumToUnderlyingType(ContextMenuItemTag::Invalid), ContextMenuItemTagNoAction, ContextMenuItemEnablement::Disabled, ContextMenuItemHasAction::No, ContextMenuItemIsSeparator::Yes };
}

Vector<PDFContextMenuItem> UnifiedPDFPlugin::selectionContextMenuItems(const IntPoint& contextMenuEventRootViewPoint, bool shouldPresentLookupAndSearchOptions) const
{
    if (![m_pdfDocument allowsCopying] || !hasSelection())
        return { };

    Vector<PDFContextMenuItem> items { contextMenuItem(ContextMenuItemTag::Copy) };

    if (shouldPresentLookupAndSearchOptions) {
        items.insertVector(0, Vector<PDFContextMenuItem> {
            contextMenuItem(ContextMenuItemTag::DictionaryLookup),
            separatorContextMenuItem(),
            contextMenuItem(ContextMenuItemTag::WebSearch),
            separatorContextMenuItem(),
        });
    }

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
    // The OpenWithDefaultViewer action is handled in the UI Process.
    case ContextMenuItemTag::OpenWithDefaultViewer: return;
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
        Ref presentationController = *m_presentationController;
        auto maybePageIndex = presentationController->pageIndexForCurrentView(PDFPresentationController::AnchorPoint::TopLeft);
        if (!maybePageIndex)
            break;

        auto currentPageIndex = *maybePageIndex;
        auto bottomRightInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { size() });
        auto bottomRightPageIndex = presentationController->nearestPageIndexForDocumentPoint(bottomRightInDocumentSpace);
        auto pagesPerRow = m_documentLayout.pagesPerRow();

        auto nextPageIsOnNextRow = [currentPageIndex, &documentLayout = m_documentLayout] {
            if (documentLayout.isSinglePageDisplayMode())
                return true;
            return documentLayout.isRightPageIndex(currentPageIndex);
        };


        if (currentPageIndex >= m_documentLayout.lastPageIndex())
            break;

        auto landingPageIndex = std::max(bottomRightPageIndex, currentPageIndex + (nextPageIsOnNextRow() ?: pagesPerRow));

        while (landingPageIndex <= m_documentLayout.lastPageIndex()) {
            if (revealPage(landingPageIndex))
                break;
            if (landingPageIndex + pagesPerRow > m_documentLayout.lastPageIndex())
                break;
            landingPageIndex += pagesPerRow;
        }
        break;
    }
    case ContextMenuItemTag::PreviousPage: {
        Ref presentationController = *m_presentationController;
        auto maybePageIndex = presentationController->pageIndexForCurrentView(PDFPresentationController::AnchorPoint::Center);
        if (!maybePageIndex)
            break;

        auto currentPageIndex = *maybePageIndex;
        auto topLeftInDocumentSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { });
        auto topLeftPageIndex = presentationController->nearestPageIndexForDocumentPoint(topLeftInDocumentSpace);
        auto pagesPerRow = m_documentLayout.pagesPerRow();

        auto previousPageIsOnPreviousRow = [currentPageIndex, &documentLayout = m_documentLayout]  {
            if (documentLayout.isSinglePageDisplayMode())
                return true;
            return documentLayout.isLeftPageIndex(currentPageIndex);
        };

        if (!currentPageIndex)
            break;

        auto landingPageIndex = std::min(topLeftPageIndex, currentPageIndex - (previousPageIsOnPreviousRow() ?: pagesPerRow));

        while (landingPageIndex >= 0) {
            if (revealPage(landingPageIndex))
                break;
            if (landingPageIndex < pagesPerRow)
                break;
            landingPageIndex -= pagesPerRow;
        }

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
        [[NSNotificationCenter defaultCenter] postNotificationName:get_PDFKit_PDFViewCopyPermissionNotificationSingleton() object:nil];
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

    RetainPtr urlData = [[url absoluteString] dataUsingEncoding:NSUTF8StringEncoding];
    Vector<PasteboardItem> pasteboardItems {
        { urlData, urlPasteboardType() },
        { urlData, stringPasteboardType() },
    };
    writeItemsToGeneralPasteboard(WTFMove(pasteboardItems));
}

#pragma mark Editing Commands

bool UnifiedPDFPlugin::handleEditingCommand(const String& commandName, const String& argument)
{
    if (equalLettersIgnoringASCIICase(commandName, "scrollpagebackward"_s) || equalLettersIgnoringASCIICase(commandName, "scrollpageforward"_s))
        return forwardEditingCommandToEditor(commandName, argument);

    if (equalLettersIgnoringASCIICase(commandName, "copy"_s))
        return performCopyEditingOperation();

    if (equalLettersIgnoringASCIICase(commandName, "selectall"_s)) {
        selectAll();
        return true;
    }

    if (equalLettersIgnoringASCIICase(commandName, "takefindstringfromselection"_s))
        return takeFindStringFromSelection();

    return false;
}

bool UnifiedPDFPlugin::isEditingCommandEnabled(const String& commandName)
{
    if (equalLettersIgnoringASCIICase(commandName, "scrollpagebackward"_s) || equalLettersIgnoringASCIICase(commandName, "scrollpageforward"_s))
        return true;

    if (equalLettersIgnoringASCIICase(commandName, "selectall"_s))
        return true;

    if (equalLettersIgnoringASCIICase(commandName, "copy"_s) || equalLettersIgnoringASCIICase(commandName, "takefindstringfromselection"_s))
        return hasSelection();

    return false;
}

static NSData *htmlDataFromSelection(PDFSelection *selection)
{
    if (!selection)
        return nil;
#if HAVE(PDFSELECTION_HTMLDATA_RTFDATA)
    if ([selection respondsToSelector:@selector(htmlData)])
        return [selection htmlData];
#endif
    RetainPtr<NSAttributedString> attributedString = selection.attributedString;
    return [attributedString dataFromRange:NSMakeRange(0, attributedString.get().length)
                        documentAttributes:@{ NSDocumentTypeDocumentAttribute : NSHTMLTextDocumentType }
                                     error:nil];
}

bool UnifiedPDFPlugin::performCopyEditingOperation() const
{
    if (!hasSelection())
        return false;

    if (![m_pdfDocument allowsCopying]) {
        [[NSNotificationCenter defaultCenter] postNotificationName:get_PDFKit_PDFViewCopyPermissionNotificationSingleton() object:nil];
        return false;
    }

    Vector<PasteboardItem> pasteboardItems;

    if (RetainPtr htmlData = htmlDataFromSelection(m_currentSelection.get()))
        pasteboardItems.append({ WTFMove(htmlData), htmlPasteboardType() });

#if HAVE(PDFSELECTION_HTMLDATA_RTFDATA)
    if ([m_currentSelection respondsToSelector:@selector(rtfData)]) {
        if (RetainPtr<NSData> rtfData = [m_currentSelection rtfData])
            pasteboardItems.append({ rtfData.get(), rtfPasteboardType() });
    }
#endif

    if (RetainPtr<NSData> plainStringData = [[m_currentSelection string] dataUsingEncoding:NSUTF8StringEncoding])
        pasteboardItems.append({ plainStringData.get(), stringPasteboardType() });

    writeItemsToGeneralPasteboard(WTFMove(pasteboardItems));
    return true;
}


bool UnifiedPDFPlugin::takeFindStringFromSelection()
{
    if (!hasSelection())
        return false;

    String findString { m_currentSelection.get().string };
    if (findString.isEmpty())
        return false;

#if PLATFORM(MAC)
    writeStringToFindPasteboard(findString);
#else
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;

    if (CheckedPtr client = m_frame->coreLocalFrame()->protectedEditor()->client())
        client->updateStringForFind(findString);
    else
        return false;
#endif

    return true;
}

bool UnifiedPDFPlugin::forwardEditingCommandToEditor(const String& commandName, const String& argument) const
{
    RefPtr frame = m_frame.get();
    if (!frame)
        return false;

    RefPtr localFrame = frame->coreLocalFrame();
    if (!localFrame)
        return false;

    return localFrame->protectedEditor()->command(commandName).execute(argument);
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
    if (!hasSelection())
        return;

    RetainPtr<PDFPage> firstPageOfCurrentSelection = [[m_currentSelection pages] firstObject];

    auto oldStartPageIndex = std::exchange(m_selectionTrackingData.startPageIndex, [m_pdfDocument indexForPage:firstPageOfCurrentSelection.get()]);
    auto oldStartPagePoint = std::exchange(m_selectionTrackingData.startPagePoint, IntPoint { [m_currentSelection firstCharCenter] });
    m_selectionTrackingData.selectionToExtendWith = WTFMove(m_currentSelection);

    RetainPtr selection = [m_pdfDocument selectionFromPage:firstPageOfCurrentSelection.get() atPoint:m_selectionTrackingData.startPagePoint toPage:m_documentLayout.pageAtIndex(oldStartPageIndex).get() atPoint:oldStartPagePoint];
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
    if (!hasSelection() || !(FloatRect([m_currentSelection boundsForPage:page.get()]).contains(m_selectionTrackingData.startPagePoint)))
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

    if (!hasSelection())
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

    auto beginAutoscrollIfNecessary = makeScopeExit([protectedThis = Ref { *this }, isDraggingSelection] {
        if (isDraggingSelection == IsDraggingSelection::Yes)
            protectedThis->beginAutoscroll();
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
            return PDFSelectionGranularityCharacter;
        case SelectionGranularity::Word:
            return PDFSelectionGranularityWord;
        case SelectionGranularity::Line:
            return PDFSelectionGranularityLine;
        }
        ASSERT_NOT_REACHED();
        return PDFSelectionGranularityCharacter;
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
        if (!hasSelection())
            return;
        break;
    case ActiveStateChangeReason::SetCurrentSelection:
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    auto repaintCoverage = unite(pageCoverageForSelection(previousSelection), pageCoverageForSelection(protectedCurrentSelection().get()));
    protectedPresentationController()->setNeedsRepaintForPageCoverage(RepaintRequirement::Selection, repaintCoverage);
}

RetainPtr<PDFSelection> UnifiedPDFPlugin::protectedCurrentSelection() const
{
    return m_currentSelection;
}

void UnifiedPDFPlugin::setCurrentSelection(RetainPtr<PDFSelection>&& selection)
{
    if (!selection && !m_currentSelection)
        return;

    RetainPtr previousSelection = std::exchange(m_currentSelection, WTFMove(selection));

#if ENABLE(TEXT_SELECTION)
    // FIXME: <https://webkit.org/b/268980> Selection painting requests should be only be made if the current selection has changed.
    // FIXME: <https://webkit.org/b/270070> Selection painting should be optimized by only repainting diff between old and new selection.
    protectedPresentationController()->setSelectionLayerEnabled(hasSelection());
    repaintOnSelectionChange(ActiveStateChangeReason::SetCurrentSelection, previousSelection.get());
#endif
    notifySelectionChanged();
}

String UnifiedPDFPlugin::fullDocumentString() const
{
    return [pdfDocument() string];
}

String UnifiedPDFPlugin::selectionString() const
{
    if (!hasSelection())
        return { };
    return m_currentSelection.get().string;
}

std::pair<String, String> UnifiedPDFPlugin::stringsBeforeAndAfterSelection(int characterCount) const
{
    RetainPtr selection = m_currentSelection;
    if (!selection)
        return { };

    auto selectionLength = [selection string].length;
    auto stringBeforeSelection = [&] -> String {
        RetainPtr beforeSelection = adoptNS([selection copy]);
        [beforeSelection extendSelectionAtStart:characterCount];

        RetainPtr result = [beforeSelection string];
        if (selectionLength > [result length]) {
            ASSERT_NOT_REACHED();
            return { };
        }

        auto targetIndex = [result length] - selectionLength;
        if (targetIndex > [result length]) {
            ASSERT_NOT_REACHED();
            return { };
        }

        return [result substringToIndex:targetIndex];
    }();

    auto stringAfterSelection = [&] -> String {
        RetainPtr afterSelection = adoptNS([selection copy]);
        [afterSelection extendSelectionAtEnd:characterCount];

        RetainPtr result = [afterSelection string];
        if (selectionLength > [result length]) {
            ASSERT_NOT_REACHED();
            return { };
        }

        return [result substringFromIndex:selectionLength];
    }();

    return { WTFMove(stringBeforeSelection), WTFMove(stringAfterSelection) };
}

bool UnifiedPDFPlugin::existingSelectionContainsPoint(const FloatPoint& rootViewPoint) const
{
    auto pluginPoint = convertFromRootViewToPlugin(roundedIntPoint(rootViewPoint));
    auto documentPoint = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { pluginPoint });
    auto pageIndex = protectedPresentationController()->pageIndexForDocumentPoint(documentPoint);
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

    Ref frame = *m_frame;
    if (!frame->coreLocalFrame())
        return { };

    RefPtr localFrameView = frame->coreLocalFrame()->view();
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
    if (!m_inActiveAutoscroll || !hasSelection())
        return;

    auto lastKnownMousePositionInPluginSpace = lastKnownMousePositionInView();
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

#if PLATFORM(MAC)
    if (RefPtr page = this->page()) {
        auto frame = toUserSpaceForPrimaryScreen(screenRectForDisplay(page->chrome().displayID()));
        auto screenPoint = toUserSpaceForPrimaryScreen(page->chrome().rootViewToScreen(convertFromPluginToRootView(lastKnownMousePositionInPluginSpace)));
        auto scrollAdjustmentBasedOnScreenBoundaries = EventHandler::autoscrollAdjustmentFactorForScreenBoundaries(screenPoint, frame);
        scrollDelta += scrollAdjustmentBasedOnScreenBoundaries;
    }
#endif // PLATFORM(MAC)

    if (scrollDelta.isZero())
        return;

    scrollWithDelta(scrollDelta);

    auto lastKnownMousePositionInDocumentSpace = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, lastKnownMousePositionInPluginSpace);
    auto pageIndex = protectedPresentationController()->nearestPageIndexForDocumentPoint(lastKnownMousePositionInDocumentSpace);
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
        m_webFoundTextRangePDFDataSelectionMap.clear();
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
        RetainPtr nsTarget = target.createNSString();
        RetainPtr foundSelection = [m_pdfDocument findString:nsTarget.get() fromSelection:m_currentSelection.get() withOptions:compareOptions];
        if (!foundSelection && wrapSearch) {
            RetainPtr emptySelection = adoptNS([allocPDFSelectionInstance() initWithDocument:m_pdfDocument.get()]);
            foundSelection = [m_pdfDocument findString:nsTarget.get() fromSelection:emptySelection.get() withOptions:compareOptions];
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

    RetainPtr foundSelections = [m_pdfDocument findString:target.createNSString().get() withOptions:compareOptionsForFindOptions(options)];
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
    Ref frame = *m_frame;
    frame->protectedPage()->findController().didInvalidateFindRects();

    if (hideFindIndicator == HideFindIndicator::Yes)
        frame->protectedPage()->findController().hideFindIndicator();
}

Vector<FloatRect> UnifiedPDFPlugin::rectsForTextMatchesInRect(const IntRect& clipRect) const
{
    return visibleRectsForFindMatchRects(m_findMatchRects, clipRect);
}

Vector<WebFoundTextRange::PDFData> UnifiedPDFPlugin::findTextMatches(const String& target, WebCore::FindOptions options)
{
    Vector<WebFoundTextRange::PDFData> matches;
    if (!target.length())
        return matches;

    RetainPtr foundSelections = [m_pdfDocument findString:target.createNSString().get() withOptions:compareOptionsForFindOptions(options)];
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

Vector<WebCore::FloatRect> UnifiedPDFPlugin::rectsForTextMatchesInRect(const Vector<WebFoundTextRange::PDFData>& matches, const WebCore::IntRect& clipRect)
{

    RefPtr frame = m_frame.get();
    if (!frame || !frame->coreLocalFrame())
        return { };
    RefPtr view = frame->coreLocalFrame()->view();
    if (!view)
        return { };

    Ref presentationController = *m_presentationController;
    auto pageCoverage = presentationController->pageCoverageAndScalesForContentsRect(clipRect, presentationController->visibleRow(), 1.0);
    auto coveredPages = pageCoverage.pages.map([] (const auto& pageInfo) {
        return pageInfo.pageIndex;
    });
    if (coveredPages.isEmpty())
        return { };

    auto firstCoveredPage = std::ranges::min(coveredPages);
    auto lastCoveredPage = std::ranges::max(coveredPages);

    PDFPageCoverage findMatchRects;
    for (auto& match : matches) {
        if (match.startPage > lastCoveredPage || firstCoveredPage > match.endPage)
            continue;

        RetainPtr selection = selectionFromWebFoundTextRangePDFData(match);
        if (!selection)
            continue;

        for (PDFPage *page in [selection pages]) {
            auto pageIndex = m_documentLayout.indexForPage(page);
            if (!pageIndex)
                continue;

            auto selectionBounds = FloatRect { [selection boundsForPage:page] };
            findMatchRects.append(PerPageInfo { *pageIndex, selectionBounds, selectionBounds });
        }
    }

    return visibleRectsForFindMatchRects(findMatchRects, clipRect);
}

Vector<WebCore::FloatRect> UnifiedPDFPlugin::visibleRectsForFindMatchRects(const PDFPageCoverage& findMatchRects, const WebCore::IntRect& clipRect) const
{
    auto visibleRow = protectedPresentationController()->visibleRow();

    Vector<FloatRect> rectsInPluginCoordinates;
    if (!visibleRow)
        rectsInPluginCoordinates.reserveCapacity(findMatchRects.size());

    for (auto& perPageInfo : findMatchRects) {
        if (visibleRow && !visibleRow->containsPage(perPageInfo.pageIndex))
            continue;

        auto pluginRect = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Plugin, perPageInfo.pageBounds, perPageInfo.pageIndex);
        if (pluginRect.intersects(clipRect))
            rectsInPluginCoordinates.append(pluginRect);
    }

    return rectsInPluginCoordinates;
}

PDFSelection *UnifiedPDFPlugin::selectionFromWebFoundTextRangePDFData(const WebFoundTextRange::PDFData& data)
{
    RetainPtr startPage = [m_pdfDocument pageAtIndex:data.startPage];
    if (!startPage)
        return nil;

    RetainPtr endPage = [m_pdfDocument pageAtIndex:data.endPage];
    if (!endPage)
        return nil;

    return m_webFoundTextRangePDFDataSelectionMap.ensure(data, [&] {
        return [m_pdfDocument selectionFromPage:startPage.get() atCharacterIndex:data.startOffset toPage:endPage.get() atCharacterIndex:(data.endOffset - 1)];
    }).iterator->value.get();
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

    return textIndicatorForSelection(selection.get(), WebCore::TextIndicatorOption::IncludeMarginIfRangeMatchesSelection, transition);
}

RefPtr<TextIndicator> UnifiedPDFPlugin::textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption> options, WebCore::TextIndicatorPresentationTransition transition)
{
    RetainPtr selection = m_currentSelection;
    return textIndicatorForSelection(selection.get(), options, transition);
}

std::optional<TextIndicatorData> UnifiedPDFPlugin::textIndicatorDataForPageRect(FloatRect pageRect, PDFDocumentLayout::PageIndex pageIndex, const std::optional<Color>& highlightColor)
{
    auto mainFrameScaleForTextIndicator = [this] {
        if (handlesPageScaleFactor())
            return 1.0;
        RefPtr frame = m_frame.get();
        if (!frame || !frame->page())
            return 1.0;
        return frame->protectedPage()->pageScaleFactor();
    }();
    float deviceScaleFactor = this->deviceScaleFactor();

    auto rectInContentsCoordinates = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Contents, pageRect, pageIndex);
    auto rectInPluginCoordinates = convertUp(CoordinateSpace::Contents, CoordinateSpace::Plugin, rectInContentsCoordinates);
    auto rectInRootViewCoordinates = convertFromPluginToRootView(encloseRectToDevicePixels(rectInPluginCoordinates, deviceScaleFactor));
    auto bufferSize = rectInRootViewCoordinates.size().scaled(mainFrameScaleForTextIndicator);

    auto buffer { ImageBuffer::create(bufferSize, RenderingMode::Unaccelerated, RenderingPurpose::ShareableSnapshot, deviceScaleFactor, DestinationColorSpace::SRGB(), PixelFormat::BGRA8) };
    if (!buffer)
        return { };

    auto& context = buffer->context();

    {
        GraphicsContextStateSaver saver(context);

        context.scale(nonNormalizedScaleFactor() * mainFrameScaleForTextIndicator);
        context.translate(-rectInContentsCoordinates.location());

        auto layoutRow { m_documentLayout.rowForPageIndex(pageIndex) };
        paintPDFContent(nullptr, context, rectInContentsCoordinates, layoutRow);
    }

    if (highlightColor)
        context.fillRect({ { 0, 0 }, bufferSize }, *highlightColor, CompositeOperator::SourceOver, BlendMode::Multiply);

    TextIndicatorData data;
    data.contentImage = BitmapImage::create(ImageBuffer::sinkIntoNativeImage(WTFMove(buffer)));
    data.contentImageScaleFactor = deviceScaleFactor;
    data.contentImageWithoutSelection = data.contentImage;
    data.contentImageWithoutSelectionRectInRootViewCoordinates = rectInRootViewCoordinates;
    data.selectionRectInRootViewCoordinates = rectInRootViewCoordinates;
    data.textBoundingRectInRootViewCoordinates = rectInRootViewCoordinates;
    data.textRectsInBoundingRectCoordinates = { { { 0, 0, }, rectInRootViewCoordinates.size() } };

    return data;
}

Color UnifiedPDFPlugin::selectionTextIndicatorHighlightColor()
{
#if PLATFORM(MAC)
    static NeverDestroyed color = roundAndClampToSRGBALossy(RetainPtr { [NSColor findHighlightColor].CGColor }.get());
#else
    static NeverDestroyed color = SRGBA<float> { .99, .89, .22, 1.0 };
#endif
    return color.get();
}

RefPtr<TextIndicator> UnifiedPDFPlugin::textIndicatorForSelection(PDFSelection *selection, OptionSet<TextIndicatorOption> options, TextIndicatorPresentationTransition transition)
{
    auto selectionPageCoverage { pageCoverageForSelection(selection, FirstPageOnly::Yes) };
    if (selectionPageCoverage.isEmpty())
        return nullptr;

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin: creating text indicator for selection with page coverage " << selectionPageCoverage);

    auto data { textIndicatorDataForPageRect(selectionPageCoverage[0].pageBounds, selectionPageCoverage[0].pageIndex, selectionTextIndicatorHighlightColor()) };
    if (!data)
        return nullptr;

    data->presentationTransition = transition;
    data->options = options;
    return TextIndicator::create(*data);
}

RefPtr<TextIndicator> UnifiedPDFPlugin::textIndicatorForAnnotation(PDFAnnotation *annotation)
{
    if (!annotation)
        return nullptr;

    auto maybePageIndex { m_documentLayout.indexForPage(annotation.page) };
    if (!maybePageIndex)
        return nullptr;

    auto pageIndex { *maybePageIndex };
    FloatRect pageBounds { annotation.bounds };

    LOG_WITH_STREAM(PDF, stream << "UnifiedPDFPlugin: creating text indicator for annotation at page index " << pageIndex << " with bounds " << pageBounds);

    auto data { textIndicatorDataForPageRect(pageBounds, pageIndex) };
    if (!data)
        return nullptr;

    return TextIndicator::create(*data);
}

WebCore::DictionaryPopupInfo UnifiedPDFPlugin::dictionaryPopupInfoForSelection(PDFSelection *selection, WebCore::TextIndicatorPresentationTransition presentationTransition)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    if (!selection.string.length)
        return dictionaryPopupInfo;

    RetainPtr nsAttributedString = [selection] {
        static constexpr unsigned maximumSelectionLength = 250;
        if (selection.string.length > maximumSelectionLength)
            return [selection.attributedString attributedSubstringFromRange:NSMakeRange(0, maximumSelectionLength)];
        return selection.attributedString;
    }();

    dictionaryPopupInfo.origin = rectForSelectionInRootView(selection).location();
#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(nsAttributedString.get());
#else
    dictionaryPopupInfo.text = [nsAttributedString string];
#endif

    if (auto textIndicator = textIndicatorForSelection(selection, { }, presentationTransition))
        dictionaryPopupInfo.textIndicator = textIndicator;

    return dictionaryPopupInfo;
}

bool UnifiedPDFPlugin::performDictionaryLookupAtLocation(const FloatPoint& rootViewPoint)
{
    auto pluginPoint = convertFromRootViewToPlugin(roundedIntPoint(rootViewPoint));
    auto documentPoint = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { pluginPoint });
    auto pageIndex = protectedPresentationController()->pageIndexForDocumentPoint(documentPoint);
    if (!pageIndex)
        return false;

    RetainPtr page = m_documentLayout.pageAtIndex(*pageIndex);
    auto pagePoint = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, documentPoint, *pageIndex);
    RetainPtr lookupSelection = [page selectionForWordAtPoint:pagePoint];
    return showDefinitionForSelection(lookupSelection.get());
}

bool UnifiedPDFPlugin::showDefinitionForSelection(PDFSelection *selection)
{
    RefPtr frame = m_frame.get();
    if (!frame)
        return false;
    RefPtr page = frame->page();
    if (!page)
        return false;

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
    auto pageIndex = protectedPresentationController()->pageIndexForDocumentPoint(documentPoint);

    if (!pageIndex)
        return { { }, m_currentSelection };

    auto pagePoint = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, documentPoint, *pageIndex);
    RetainPtr page = m_documentLayout.pageAtIndex(*pageIndex);
    RetainPtr wordSelection = [page selectionForWordAtPoint:pagePoint];
    if (!wordSelection || [wordSelection isEmpty])
        return { { }, wordSelection };

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

#if ENABLE(REVEAL)
    RetainPtr lookupText = DictionaryLookup::stringForPDFSelection(wordSelection.get());
    if (lookupText && lookupText.get().length)
        return { lookupText.get(), wordSelection };
#endif

    return { { }, wordSelection };
}

#if PLATFORM(MAC)
void UnifiedPDFPlugin::accessibilityScrollToPage(PDFDocumentLayout::PageIndex pageIndex)
{
    revealPage(pageIndex);
}

id UnifiedPDFPlugin::accessibilityHitTestIntPoint(const WebCore::IntPoint& point) const
{
    Ref protectedThis { *this };
    return WebCore::Accessibility::retrieveValueFromMainThread<id>([&protectedThis, point] () -> id {
        IntPoint pluginPoint = point + (-protectedThis->protectedView()->location());
        auto pointInScreenSpaceCoordinate = protectedThis->convertFromPluginToScreenForAccessibility(pluginPoint);
        return [protectedThis->m_accessibilityDocumentObject accessibilityHitTest:pointInScreenSpaceCoordinate];
    });
}

IntPoint UnifiedPDFPlugin::convertFromPluginToScreenForAccessibility(const IntPoint& pointInPluginCoordinate) const
{
    Ref protectedThis { *this };
    return WebCore::Accessibility::retrieveValueFromMainThread<IntPoint>([&protectedThis, pointInPluginCoordinate] () -> IntPoint {
        auto pointInRootView = protectedThis->convertFromPluginToRootView(pointInPluginCoordinate);
        RefPtr page = protectedThis->page();
        if (!page)
            return { };
        return page->chrome().rootViewToScreen(IntPoint(pointInRootView));
    });
}

#endif

FloatRect UnifiedPDFPlugin::convertFromPDFPageToScreenForAccessibility(const FloatRect& rectInPageCoordinates, PDFDocumentLayout::PageIndex pageIndex) const
{
    Ref protectedThis { *this };
    return WebCore::Accessibility::retrieveValueFromMainThread<FloatRect>([&protectedThis, rectInPageCoordinates, pageIndex] -> FloatRect {
        auto rectInPluginCoordinates = protectedThis->pageToRootView(rectInPageCoordinates, pageIndex);
        RefPtr page = protectedThis->page();
        if (!page)
            return { };
        return page->chrome().rootViewToScreen(enclosingIntRect(rectInPluginCoordinates));
    });
}

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
    return m_accessibilityDocumentObject.get();
}

#if PLATFORM(IOS_FAMILY)
id UnifiedPDFPlugin::accessibilityHitTestInPageForIOS(WebCore::FloatPoint point)
{
    RefPtr corePage = this->page();
    if (!corePage)
        return nil;

    auto [page, pointInPage] = rootViewToPage(corePage->chrome().screenToRootView(WebCore::IntPoint(point)));
    if ([page respondsToSelector:@selector(accessibilityHitTest:withPlugin:)])
        return [page accessibilityHitTest:point withPlugin:m_accessibilityDocumentObject.get()];
    return nil;
}

WebCore::AXCoreObject* UnifiedPDFPlugin::accessibilityCoreObject()
{
    if (CheckedPtr cache = axObjectCache())
        return cache->exportedGetOrCreate(m_element.get());
    return nullptr;
}
#endif // PLATFORM(IOS_FAMILY)

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

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)

IntRect UnifiedPDFPlugin::frameForPageNumberIndicatorInRootViewCoordinates() const
{
    return convertFromPluginToRootView(IntRect { { }, size() });
}

bool UnifiedPDFPlugin::pageNumberIndicatorEnabled() const
{
    if (RefPtr page = this->page())
        return page->settings().pdfPluginPageNumberIndicatorEnabled();
    return false;
}

bool UnifiedPDFPlugin::shouldShowPageNumberIndicator() const
{
    if (!pageNumberIndicatorEnabled())
        return false;

    if (!isFullMainFramePlugin())
        return false;

    if (!m_view->isVisible())
        return false;

    if (isLocked())
        return false;

    if (!m_documentLayout.hasPDFDocument())
        return false;

    return true;
}

auto UnifiedPDFPlugin::updatePageNumberIndicatorVisibility() -> IndicatorVisible
{
    if (!m_frame || !m_frame->page())
        return IndicatorVisible::No;

    if (shouldShowPageNumberIndicator()) {
        m_frame->protectedPage()->createPDFPageNumberIndicator(*this, frameForPageNumberIndicatorInRootViewCoordinates(), m_documentLayout.pageCount());
        return IndicatorVisible::Yes;
    }

    m_frame->protectedPage()->removePDFPageNumberIndicator(*this);
    return IndicatorVisible::No;
}

void UnifiedPDFPlugin::updatePageNumberIndicatorLocation()
{
    if (!m_frame || !m_frame->page())
        return;

    m_frame->protectedPage()->updatePDFPageNumberIndicatorLocation(*this, frameForPageNumberIndicatorInRootViewCoordinates());
}

void UnifiedPDFPlugin::updatePageNumberIndicatorCurrentPage(const std::optional<IntRect>& maybeUnobscuredContentRectInRootView)
{
    if (!m_frame || !m_frame->page())
        return;

    auto unobscuredContentRectInRootView = maybeUnobscuredContentRectInRootView.or_else([this] -> std::optional<IntRect> {
        if (!m_frame || !m_frame->coreLocalFrame())
            return { };
        RefPtr view = m_frame->coreLocalFrame()->view();
        if (!view)
            return { };
        return view->unobscuredContentRect();
    });

    if (!unobscuredContentRectInRootView)
        return;

    auto scrollPositionInPluginSpace = convertFromRootViewToPlugin(FloatPoint { unobscuredContentRectInRootView->center() });
    auto scrollPositionInDocumentLayoutSpace = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, scrollPositionInPluginSpace);
    auto currentPageIndex = m_presentationController->nearestPageIndexForDocumentPoint(scrollPositionInDocumentLayoutSpace);
    m_frame->protectedPage()->updatePDFPageNumberIndicatorCurrentPage(*this, currentPageIndex + 1);
}


void UnifiedPDFPlugin::updatePageNumberIndicator(const std::optional<IntRect>& maybeUnobscuredContentRectInRootView)
{
    if (updatePageNumberIndicatorVisibility() == IndicatorVisible::No)
        return;
    updatePageNumberIndicatorLocation();
    updatePageNumberIndicatorCurrentPage(maybeUnobscuredContentRectInRootView);
}

#endif

void UnifiedPDFPlugin::frameViewLayoutOrVisualViewportChanged(const IntRect& unobscuredContentRectInRootView)
{
#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    updatePageNumberIndicator(unobscuredContentRectInRootView);
#endif
}

CGRect UnifiedPDFPlugin::pluginBoundsForAnnotation(PDFAnnotation *annotation) const
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

    RetainPtr indexRange = [&] {
        if (searchDirection == AnnotationSearchDirection::Forward)
            return adoptNS([[NSIndexSet alloc] initWithIndexesInRange:NSMakeRange(startingIndex, [annotations count] - startingIndex)]);
        return adoptNS([[NSIndexSet alloc] initWithIndexesInRange:NSMakeRange(0, startingIndex + 1)]);
    }();

    auto searchResult = [annotations indexOfObjectAtIndexes:indexRange.get() options:searchDirection == AnnotationSearchDirection::Forward ? 0 : NSEnumerationReverse passingTest:^BOOL(PDFAnnotation* annotation, NSUInteger, BOOL *) {
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

        if (isInPluginCleanup != IsInPluginCleanup::Yes) {
            if (RefPtr activeAnnotation = m_activeAnnotation) {
                activeAnnotation->commit();
                RetainPtr pdfAnnotation = activeAnnotation->annotation();
                setNeedsRepaintForAnnotation(pdfAnnotation.get(), repaintRequirementsForAnnotation(pdfAnnotation.get(), IsAnnotationCommit::Yes));
            }
        }

        if (annotation) {
            if (annotationIsWidgetOfType(annotation.get(), WidgetType::Text) && [annotation isReadOnly]) {
                m_activeAnnotation = nullptr;
                return;
            }

            RefPtr newActiveAnnotation = PDFPluginAnnotation::create(annotation.get(), this);
            newActiveAnnotation->attach(m_annotationContainer.get());
            m_activeAnnotation = WTFMove(newActiveAnnotation);
            revealAnnotation(protectedActiveAnnotation()->protectedAnnotation().get());
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

#if HAVE(PDFDOCUMENT_RESET_FORM_FIELDS)
        if (RetainPtr resetAction = dynamic_objc_cast<PDFActionResetForm>(action))
            [m_pdfDocument resetFormFields:resetAction.get()];
#endif

        RetainPtr actionType = [action type];
        if ([actionType isEqualToString:@"Named"]) {
            auto actionName = [checked_objc_cast<PDFActionNamed>(action) name];
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

RepaintRequirements AnnotationTrackingState::finishAnnotationTracking(PDFAnnotation *annotationUnderMouse, WebEventType mouseEventType, WebMouseEventButton mouseEventButton)
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
    return CGPDFDocumentIsTaggedPDF(RetainPtr { [m_pdfDocument documentRef] }.get());
}

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)

void UnifiedPDFPlugin::installDataDetectorOverlay(PageOverlay& overlay)
{
    RefPtr frame = m_frame.get();
    if (!frame || !frame->coreLocalFrame())
        return;

    RefPtr webPage = frame->page();
    if (!webPage)
        return;

    webPage->corePage()->pageOverlayController().installPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);
}

void UnifiedPDFPlugin::uninstallDataDetectorOverlay(PageOverlay& overlay)
{
    RefPtr frame = m_frame.get();
    if (!frame || !frame->coreLocalFrame())
        return;

    RefPtr webPage = frame->page();
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

    [annotation setWidgetStringValue:value.createNSString().get()];
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

    if (RefPtr presentationController = m_presentationController; presentationController && presentationController->supportsDisplayMode(mode)) {
        presentationController->willChangeDisplayMode(mode);
        return;
    }

    setPresentationController(PDFPresentationController::createForMode(mode, *this));
}

void UnifiedPDFPlugin::setDisplayModeAndUpdateLayout(PDFDocumentLayout::DisplayMode mode)
{
    auto shouldAdjustPageScale = m_shouldUpdateAutoSizeScale == ShouldUpdateAutoSizeScale::Yes ? AdjustScaleAfterLayout::No : AdjustScaleAfterLayout::Yes;
    Ref presentationController = *m_presentationController;
    bool didWantWheelEvents = presentationController->wantsWheelEvents();
    auto anchoringInfo = presentationController->pdfPositionForCurrentView(PDFPresentationController::AnchorPoint::Center);

    setDisplayMode(mode);
    {
        SetForScope scope(m_shouldUpdateAutoSizeScale, ShouldUpdateAutoSizeScale::Yes);
        updateLayout(shouldAdjustPageScale);
    }

    if (anchoringInfo)
        presentationController->restorePDFPosition(*anchoringInfo);

    bool wantsWheelEvents = presentationController->wantsWheelEvents();
    if (didWantWheelEvents != wantsWheelEvents)
        wantsWheelEventsChanged();
}

#if PLATFORM(IOS_FAMILY)

std::pair<URL, FloatRect> UnifiedPDFPlugin::linkURLAndBoundsForAnnotation(PDFAnnotation *annotation) const
{
    if (!annotation)
        return { };

    if (!annotationIsLinkWithDestination(annotation))
        return { };

    return { [annotation URL], pageToRootView([annotation bounds], [annotation page]) };
}

std::pair<URL, FloatRect> UnifiedPDFPlugin::linkURLAndBoundsAtPoint(FloatPoint pointInRootView) const
{
    RetainPtr annotation = annotationForRootViewPoint(roundedIntPoint(pointInRootView));
    return linkURLAndBoundsForAnnotation(annotation.get());
}

std::tuple<URL, FloatRect, RefPtr<TextIndicator>> UnifiedPDFPlugin::linkDataAtPoint(FloatPoint pointInRootView)
{
    RetainPtr annotation = annotationForRootViewPoint(roundedIntPoint(pointInRootView));
    auto [linkURL, bounds] = linkURLAndBoundsForAnnotation(annotation.get());
    return { linkURL, bounds, textIndicatorForAnnotation(annotation.get()) };
}

std::optional<FloatRect> UnifiedPDFPlugin::highlightRectForTapAtPoint(FloatPoint pointInRootView) const
{
    // FIXME: We only support tapping on links at the moment. In the future, we might want to
    // support more types of annotations.
    auto [url, rect] = linkURLAndBoundsAtPoint(pointInRootView);
    if (rect.isEmpty())
        return std::nullopt;

    return rect;
}

void UnifiedPDFPlugin::handleSyntheticClick(PlatformMouseEvent&& event)
{
#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)
    auto pointInRootView = event.position();
    if (RetainPtr annotation = annotationForRootViewPoint(IntPoint(pointInRootView))) {
        if (annotationIsLinkWithDestination(annotation.get()))
            followLinkAnnotation(annotation.get(), { WTFMove(event) });
        clearSelection();
        return;
    }

    RetainPtr selection = m_currentSelection;
    if (selection && event.shiftKey()) {
        auto [page, pointInPage] = rootViewToPage(FloatPoint(pointInRootView));
        if (!page)
            return;

        [selection addSelection:selectionAtPoint(pointInPage, page.get(), TextGranularity::WordGranularity)];

        auto [startPage, startPointInPage] = selectionCaretPointInPage(selection.get(), SelectionEndpoint::Start);
        if (!startPage)
            return;

        auto [endPage, endPointInPage] = selectionCaretPointInPage(selection.get(), SelectionEndpoint::End);
        if (!endPage)
            return;

        setCurrentSelection(selectionBetweenPoints(startPointInPage, startPage.get(), endPointInPage, endPage.get()));
        return;
    }
#else
    UNUSED_PARAM(event);
#endif

    clearSelection();
}

void UnifiedPDFPlugin::clearSelection()
{
    resetInitialSelection();
    setCurrentSelection({ });
}

#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)

static bool areVisuallyDistinct(FloatPoint a, FloatPoint b)
{
    static constexpr auto maxDistanceSquared = 0.1 * 0.1;
    return (a - b).diagonalLengthSquared() > maxDistanceSquared;
}

static bool isEmpty(PDFSelection *selection)
{
    return !selection.pages.count;
}

#endif // HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)

void UnifiedPDFPlugin::setSelectionRange(FloatPoint pointInRootView, TextGranularity granularity)
{
#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)
    RetainPtr pdfDocument = m_pdfDocument;
    if (!pdfDocument)
        return;

    auto [page, pointInPage] = rootViewToPage(pointInRootView);
    if (!page)
        return;

    m_initialSelection = selectionAtPoint(pointInPage, page.get(), granularity);
    m_initialSelectionStart = selectionCaretPointInPage(m_initialSelection.get(), SelectionEndpoint::Start);
    setCurrentSelection(m_initialSelection.get());
#else
    UNUSED_PARAM(pointInRootView);
    UNUSED_PARAM(granularity);
#endif
}

SelectionWasFlipped UnifiedPDFPlugin::moveSelectionEndpoint(FloatPoint pointInRootView, SelectionEndpoint extentEndpoint)
{
    auto flipped = SelectionWasFlipped::No;
#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)
    RetainPtr pdfDocument = m_pdfDocument;
    if (!pdfDocument)
        return flipped;

    bool baseIsStart = extentEndpoint == SelectionEndpoint::End;
    auto baseEndpoint = baseIsStart ? SelectionEndpoint::Start : SelectionEndpoint::End;
    auto [basePage, basePointInPage] = selectionCaretPointInPage(baseEndpoint);
    if (!basePage)
        return flipped;

    auto [extentPage, extentPointInPage] = rootViewToPage(pointInRootView);
    if (!extentPage)
        return flipped;

    RetainPtr newSelection = selectionBetweenPoints(
        baseIsStart ? basePointInPage : extentPointInPage,
        baseIsStart ? basePage.get() : extentPage.get(),
        baseIsStart ? extentPointInPage : basePointInPage,
        baseIsStart ? extentPage.get() : basePage.get()
    );

    if (isEmpty(newSelection.get())) {
        // The selection became collapsed; maintain the existing selection.
        return flipped;
    }

    auto [newExtentPage, newExtentPointInPage] = selectionCaretPointInPage(newSelection.get(), extentEndpoint);
    auto [newBasePage, newBasePointInPage] = selectionCaretPointInPage(newSelection.get(), baseEndpoint);
    if (newExtentPage && newBasePage) {
        if (basePage != newBasePage || areVisuallyDistinct(basePointInPage, newBasePointInPage)) {
            // Canonicalize the selection (i.e. swap the start and end points) if needed.
            [newSelection addSelection:newSelection.get()];
            flipped = SelectionWasFlipped::Yes;
        }
    }

    resetInitialSelection();
    setCurrentSelection(WTFMove(newSelection));
#else
    UNUSED_PARAM(pointInRootView);
    UNUSED_PARAM(extentEndpoint);
#endif
    return flipped;
}

void UnifiedPDFPlugin::resetInitialSelection()
{
    m_initialSelection = nil;
    m_initialSelectionStart = { nil, { } };
}

#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)

PDFSelection *UnifiedPDFPlugin::selectionBetweenPoints(FloatPoint fromPoint, PDFPage *fromPage, FloatPoint toPoint, PDFPage *toPage) const
{
    return [pdfDocument() selectionFromPage:fromPage atPoint:fromPoint toPage:toPage atPoint:toPoint withGranularity:PDFSelectionGranularityCharacter];
}

PDFSelection *UnifiedPDFPlugin::selectionAtPoint(FloatPoint pointInPage, PDFPage *page, TextGranularity granularity) const
{
    if (granularity == TextGranularity::DocumentGranularity)
        return [pdfDocument() selectionForEntireDocument];

    return [pdfDocument() selectionFromPage:page atPoint:pointInPage toPage:page atPoint:pointInPage withGranularity:[&] {
        switch (granularity) {
        case TextGranularity::CharacterGranularity:
            return PDFSelectionGranularityCharacter;
        case TextGranularity::WordGranularity:
            return PDFSelectionGranularityWord;
        case TextGranularity::LineGranularity:
            return PDFSelectionGranularityLine;
        default:
            ASSERT_NOT_REACHED();
            return PDFSelectionGranularityCharacter;
        }
    }()];
}

#endif // HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)

SelectionEndpoint UnifiedPDFPlugin::extendInitialSelection(FloatPoint pointInRootView, TextGranularity granularity)
{
#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)
    auto [page, pointInPage] = rootViewToPage(pointInRootView);
    if (!page)
        return SelectionEndpoint::Start;

    auto [startPage, startPointInPage] = m_initialSelectionStart;
    if (!startPage)
        return SelectionEndpoint::Start;

    RetainPtr newSelection = selectionAtPoint(pointInPage, page.get(), granularity);
    if (isEmpty(newSelection.get()))
        return SelectionEndpoint::Start;

    [newSelection addSelection:m_initialSelection.get()];
    // The selection at this point only includes the initial selection, and the new hit-tested selection, and may be discontiguous.

    auto [newStartPage, newStartPointInPage] = selectionCaretPointInPage(newSelection.get(), SelectionEndpoint::Start);
    if (!newStartPage)
        return SelectionEndpoint::Start;

    auto [newEndPage, newEndPointInPage] = selectionCaretPointInPage(newSelection.get(), SelectionEndpoint::End);
    if (!newEndPage)
        return SelectionEndpoint::Start;

    newSelection = selectionBetweenPoints(newStartPointInPage, newStartPage.get(), newEndPointInPage, newEndPage.get());
    if (!newSelection)
        return SelectionEndpoint::Start;

    setCurrentSelection(WTFMove(newSelection));

    if (startPage == newStartPage && !areVisuallyDistinct(startPointInPage, newStartPointInPage))
        return SelectionEndpoint::End;
#else
    UNUSED_PARAM(granularity);
    UNUSED_PARAM(pointInRootView);
#endif
    return SelectionEndpoint::Start;
}

auto UnifiedPDFPlugin::selectionCaretPointInPage(PDFSelection *selection, SelectionEndpoint endpoint) const -> PageAndPoint
{
    bool isStart = endpoint == SelectionEndpoint::Start;
    RetainPtr pages = [selection pages];
    RetainPtr page = isStart ? [pages firstObject] : [pages lastObject];
    if (!page)
        return { nil, { } };

    RetainPtr selectionsByLine = [selection selectionsByLine];
    RetainPtr selectedLine = isStart ? [selectionsByLine firstObject] : [selectionsByLine lastObject];
    FloatRect boundsInRootView;

    AffineTransform cumulativeTransform = [page transformForBox:kPDFDisplayBoxMediaBox];
    bool appliedLineTransform = false;
    [selectedLine enumerateRectsAndTransformsForPage:page.get() usingBlock:[&](CGRect rect, CGAffineTransform transform) {
        if (std::exchange(appliedLineTransform, true)) {
            ASSERT_NOT_REACHED();
            return;
        }

        boundsInRootView = pageToRootView({ CGRectApplyAffineTransform(rect, transform) }, page.get());
        cumulativeTransform *= transform;
    }];

    if (boundsInRootView.isEmpty())
        return { nil, { } };

    if (!appliedLineTransform)
        return { nil, { } };

    auto rotationInRadians = atan2(cumulativeTransform.b(), cumulativeTransform.a());
    if (!std::isfinite(rotationInRadians))
        return { nil, { } };

    // FIXME: Account for RTL text and vertical writing mode.
    return rootViewToPage([&] -> FloatPoint {
        int clockwiseRotationAngle = static_cast<int>(360 + 90 * std::round(-rad2deg(rotationInRadians) / 90)) % 360;
        switch (clockwiseRotationAngle) {
        case 0:
            // The start/end points are along the left/right edges, respectively.
            return { isStart ? boundsInRootView.x() : boundsInRootView.maxX(), boundsInRootView.y() + (boundsInRootView.height() / 2) };
        case 90:
            // The start/end points are along the top/bottom edges, respectively.
            return { boundsInRootView.x() + (boundsInRootView.width() / 2), isStart ? boundsInRootView.y() : boundsInRootView.maxY() };
        case 180:
            // The start/end points are along the right/left edges, respectively.
            return { isStart ? boundsInRootView.maxX() : boundsInRootView.x(), boundsInRootView.y() + (boundsInRootView.height() / 2) };
        case 270:
            // The start/end points are along the bottom/top edges, respectively.
            return { boundsInRootView.x() + (boundsInRootView.width() / 2), isStart ? boundsInRootView.maxY() : boundsInRootView.y() };
        }
        ASSERT_NOT_REACHED();
        return boundsInRootView.center();
    }());
}

auto UnifiedPDFPlugin::selectionCaretPointInPage(SelectionEndpoint endpoint) const -> PageAndPoint
{
    return selectionCaretPointInPage(RetainPtr { m_currentSelection }.get(), endpoint);
}

bool UnifiedPDFPlugin::platformPopulateEditorStateIfNeeded(EditorState& state) const
{
    RetainPtr selection = m_currentSelection;
    if (!selection) {
        state.visualData = EditorState::VisualData { };
        state.postLayoutData = EditorState::PostLayoutData { };
        state.postLayoutData->isStableStateUpdate = true;
        return true;
    }

    Vector<FloatRect> selectionRects;
#if HAVE(PDFSELECTION_ENUMERATE_RECTS_AND_TRANSFORMS)
    for (PDFPage *page in [selection pages]) {
        auto pageIndex = m_documentLayout.indexForPage(page);
        [selection enumerateRectsAndTransformsForPage:page usingBlock:[&](CGRect rect, CGAffineTransform transform) {
            auto transformedRectInPage = CGRectApplyAffineTransform(rect, transform);
            auto rectInRootView = pageToRootView(FloatRect { transformedRectInPage }, pageIndex);
            if (rectInRootView.isEmpty())
                return;

            selectionRects.append(WTFMove(rectInRootView));
        }];
    }
#endif // HAVE(PDFSELECTION_ENUMERATE_RECTS_AND_TRANSFORMS)

    auto selectionGeometries = selectionRects.map([](auto& rectInRootView) {
        return SelectionGeometry {
            rectInRootView,
            SelectionRenderingBehavior::CoalesceBoundingRects,
            TextDirection::LTR,
            0, // minX
            0, // maxX
            0, // maxY
            0, // lineNumber
            false, // isLineBreak
            false, // isFirstOnLine
            false, // isLastOnLine
            false, // containsStart
            false, // containsEnd
            true, // isHorizontal
        };
    });

    if (selectionGeometries.size()) {
        selectionGeometries.first().setContainsStart(true);
        selectionGeometries.last().setContainsEnd(true);
    }

    state.isInPlugin = true;
    state.selectionIsNone = false;
    state.selectionIsRange = selectionGeometries.size();

    auto selectedString = String { [selection string] };
    state.postLayoutData = EditorState::PostLayoutData { };
    state.postLayoutData->isStableStateUpdate = true;
    state.postLayoutData->selectedTextLength = selectedString.length();
    state.postLayoutData->canCopy = !selectedString.isEmpty();
    state.postLayoutData->wordAtSelection = WTFMove(selectedString);

    state.visualData = EditorState::VisualData { };
    state.visualData->selectionGeometries = WTFMove(selectionGeometries);

    if (m_presentationController)
        state.visualData->enclosingLayerID = m_presentationController->contentsLayerIdentifier();

    if (m_scrollingNodeID) {
        state.visualData->enclosingScrollingNodeID = *m_scrollingNodeID;
        state.visualData->enclosingScrollOffset = scrollOffset();
    }

    return true;
}

CursorContext UnifiedPDFPlugin::cursorContext(FloatPoint pointInRootView) const
{
    CursorContext context;
#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)
    auto [page, pointInPage] = rootViewToPage(pointInRootView);
    if (!page)
        return context;

    auto elementTypes = pdfElementTypesForPagePoint(roundedIntPoint(pointInPage), page.get());
    if (toWebCoreCursorType(elementTypes) == Cursor::Type::IBeam)
        context.cursor = Cursor::fromType(Cursor::Type::IBeam);

    RetainPtr lineUnderCursor = selectionAtPoint(pointInPage, page.get(), TextGranularity::LineGranularity);
    auto pageRectForLine = FloatRect { [lineUnderCursor boundsForPage:page.get()] };
    if (pageRectForLine.contains(pointInPage))
        context.lineCaretExtent = pageToRootView(pageRectForLine, page.get());
#else
    UNUSED_PARAM(pointInRootView);
#endif
    return context;
}

DocumentEditingContext UnifiedPDFPlugin::documentEditingContext(DocumentEditingContextRequest&& request) const
{
    using enum DocumentEditingContextRequest::Options;

    static constexpr OptionSet unsupportedOptions { SpatialAndCurrentSelection, Spatial, Rects };

    if (request.options.containsAny(unsupportedOptions)) {
        // FIXME: Consider implementing support for these in the future, if needed.
        // At the moment, these are only used to drive specific interactions in editable content.
        return { };
    }

    bool wantsAttributedText = request.options.contains(AttributedText);
    bool wantsPlainText = request.options.contains(Text);
    if (!wantsAttributedText && !wantsPlainText)
        return { };

    RetainPtr selection = m_currentSelection;
    if (!selection)
        return { };

    DocumentEditingContext context;
    context.selectedText = [&] {
        if (wantsAttributedText)
            return AttributedString::fromNSAttributedString({ [selection attributedString] });

        ASSERT(wantsPlainText);
        return AttributedString { String { [selection string] }, { }, { } };
    }();

    // FIXME: We should populate `contextBefore` and `contextAfter` as well, but PDFKit currently doesn't expose
    // any APIs to (efficiently) extend the selection by word, sentence or paragraph granularity.
    return context;
}

#endif // PLATFORM(IOS_FAMILY)

FloatRect UnifiedPDFPlugin::pageToRootView(FloatRect rectInPage, PDFPage *page) const
{
    return pageToRootView(rectInPage, m_documentLayout.indexForPage(page));
}

FloatRect UnifiedPDFPlugin::pageToRootView(FloatRect rectInPage, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    auto rectInPlugin = convertUp(CoordinateSpace::PDFPage, CoordinateSpace::Plugin, rectInPage, pageIndex);
    return convertFromPluginToRootView(rectInPlugin);
}

auto UnifiedPDFPlugin::rootViewToPage(FloatPoint pointInRootView) const -> PageAndPoint
{
    auto pointInPlugin = convertFromRootViewToPlugin(pointInRootView);
    auto pointInDocument = convertDown<FloatPoint>(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, pointInPlugin);
    auto pageIndex = protectedPresentationController()->nearestPageIndexForDocumentPoint(pointInDocument);
    auto pointInPage = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, pointInDocument, pageIndex);
    return { m_documentLayout.pageAtIndex(pageIndex), pointInPage };
}

FloatRect UnifiedPDFPlugin::absoluteBoundingRectForSmartMagnificationAtPoint(FloatPoint rootViewPoint) const
{
    auto pluginPoint = convertFromRootViewToPlugin(roundedIntPoint(rootViewPoint));
    auto documentPoint = convertDown(CoordinateSpace::Plugin, CoordinateSpace::PDFDocumentLayout, FloatPoint { pluginPoint });
    auto pageIndex = protectedPresentationController()->pageIndexForDocumentPoint(documentPoint);
    if (!pageIndex)
        return { };

    RetainPtr page = m_documentLayout.pageAtIndex(*pageIndex);
    auto pagePoint = convertDown(CoordinateSpace::PDFDocumentLayout, CoordinateSpace::PDFPage, documentPoint, pageIndex);
    FloatRect pageColumnFrame = [page columnFrameAtPoint:pagePoint];

    return pageToRootView(pageColumnFrame, pageIndex);
}

bool UnifiedPDFPlugin::shouldUseInProcessBackingStore() const
{
    return false;
}

bool UnifiedPDFPlugin::layerNeedsPlatformContext(const GraphicsLayer& layer) const
{
    return shouldUseInProcessBackingStore() && (&layer == layerForHorizontalScrollbar() || &layer == layerForVerticalScrollbar() || &layer == layerForScrollCorner());
}

bool UnifiedPDFPlugin::delegatesScrollingToMainFrame() const
{
    return !handlesPageScaleFactor() && isFullFramePlugin() && scrollingMode() == DelegatedScrollingMode::DelegatedToNativeScrollView;
}

RefPtr<PDFPresentationController> UnifiedPDFPlugin::protectedPresentationController() const
{
    return m_presentationController;
}

RefPtr<WebCore::GraphicsLayer> UnifiedPDFPlugin::protectedScrollContainerLayer() const
{
    return m_scrollContainerLayer;
}

RefPtr<WebCore::GraphicsLayer> UnifiedPDFPlugin::protectedOverflowControlsContainer() const
{
    return m_overflowControlsContainer;
}

ViewportConfiguration::Parameters UnifiedPDFPlugin::viewportParameters()
{
    ViewportConfiguration::Parameters parameters;
    parameters.width = ViewportArguments::ValueDeviceWidth;
    parameters.widthIsSet = true;
    parameters.allowsUserScaling = true;
    parameters.allowsShrinkToFit = false;
    parameters.minimumScale = minimumZoomScale;
    parameters.maximumScale = maximumZoomScale;
    parameters.initialScale = 1;
    parameters.initialScaleIgnoringLayoutScaleFactor = 1;
    parameters.initialScaleIsSet = true;
    parameters.shouldHonorMinimumEffectiveDeviceWidthFromClient = false;
    parameters.minimumScaleDoesNotAdaptToContent = true;
    return parameters;
}

TextStream& operator<<(TextStream& ts, RepaintRequirement requirement)
{
    switch (requirement) {
    case RepaintRequirement::PDFContent:
        ts << "PDFContent"_s;
        break;
    case RepaintRequirement::Selection:
        ts << "Selection"_s;
        break;
    case RepaintRequirement::HoverOverlay:
        ts << "HoverOverlay"_s;
        break;
    }
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
