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
#include "PluginView.h"
#include "WebEventConversion.h"
#include "WebEventType.h"
#include "WebMouseEvent.h"
#include "WebPageProxyMessages.h"
#include <CoreGraphics/CoreGraphics.h>
#include <PDFKit/PDFKit.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/Chrome.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/ColorCocoa.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/ScrollTypes.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

#include "PDFKitSoftLink.h"

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIColor.h>
#endif

namespace WebKit {
using namespace WebCore;

Ref<UnifiedPDFPlugin> UnifiedPDFPlugin::create(HTMLPlugInElement& pluginElement)
{
    return adoptRef(*new UnifiedPDFPlugin(pluginElement));
}

UnifiedPDFPlugin::UnifiedPDFPlugin(HTMLPlugInElement& element)
    : PDFPluginBase(element)
{
    this->setVerticalScrollElasticity(ScrollElasticity::Automatic);
    this->setHorizontalScrollElasticity(ScrollElasticity::Automatic);
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

void UnifiedPDFPlugin::updateLayerHierarchy()
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (!m_rootLayer) {
        m_rootLayer = createGraphicsLayer("UnifiedPDFPlugin root"_s, GraphicsLayer::Type::Normal);
        m_rootLayer->setAnchorPoint({ });
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
        m_scrolledContentsLayer->setDrawsContent(true);
        m_scrollContainerLayer->addChild(*m_scrolledContentsLayer);
    }

    if (!m_contentsLayer) {
        m_contentsLayer = createGraphicsLayer("UnifiedPDFPlugin contents"_s, GraphicsLayer::Type::TiledBacking);
        m_contentsLayer->setAnchorPoint({ });
        m_contentsLayer->setDrawsContent(true);
        m_scrolledContentsLayer->addChild(*m_contentsLayer);
    }

    RefPtr scrollingCoordinator = page->scrollingCoordinator();
    if (!m_scrollingNodeID && scrollingCoordinator) {
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

    m_scrollContainerLayer->setSize(size());

    m_contentsLayer->setSize(contentsSize());
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

void UnifiedPDFPlugin::paint(WebCore::GraphicsContext& context, const WebCore::IntRect&)
{
    // Only called for snapshotting.

    if (size().isEmpty())
        return;

    context.translate(-m_scrollOffset.width(), -m_scrollOffset.height());
    paintContents(m_contentsLayer.get(), context, { FloatPoint(m_scrollOffset), size() }, { });
}

void UnifiedPDFPlugin::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
{
    if (layer != m_contentsLayer.get())
        return;

    if (m_size.isEmpty() || documentSize().isEmpty())
        return;

    auto drawingRect = IntRect { { }, documentSize() };
    drawingRect.intersect(enclosingIntRect(clipRect));

    auto imageBuffer = ImageBuffer::create(drawingRect.size(), RenderingPurpose::Unspecified, context.scaleFactor().width(), DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return;

    auto& bufferContext = imageBuffer->context();
    auto stateSaver = GraphicsContextStateSaver(bufferContext);

    bufferContext.translate(FloatPoint { -drawingRect.location() });
    auto scale = m_documentLayout.scale();
    bufferContext.scale(scale);

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

        auto pageStateSaver = GraphicsContextStateSaver(bufferContext);
        bufferContext.clip(destinationRect);
        bufferContext.fillRect(destinationRect, Color::white);

        // Translate the context to the bottom of pageBounds and flip, so that PDFKit operates
        // from this page's drawing origin.
        bufferContext.translate(destinationRect.minXMaxYCorner());
        bufferContext.scale({ 1, -1 });

        [page drawWithBox:kPDFDisplayBoxCropBox toContext:imageBuffer->context().platformContext()];
    }

    context.fillRect(clipRect, WebCore::roundAndClampToSRGBALossy([WebCore::CocoaColor grayColor].CGColor));
    context.drawImageBuffer(*imageBuffer, drawingRect.location());
}

CGFloat UnifiedPDFPlugin::scaleFactor() const
{
    return m_scaleFactor;
}

float UnifiedPDFPlugin::deviceScaleFactor() const
{
    return PDFPluginBase::deviceScaleFactor();
}

float UnifiedPDFPlugin::pageScaleFactor() const
{
    if (RefPtr page = this->page())
        return m_view->pageScaleFactor();
    return 1;
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
    m_contentsLayer->setTransform(transform);

    if (!origin)
        origin = IntRect({ }, size()).center();

    auto gestureOriginInContentsCoordinates = convertFromRootViewToPlugin(*origin);
    gestureOriginInContentsCoordinates.moveBy(oldScrollPosition);

    auto gestureOriginInNewContentsCoordinates = gestureOriginInContentsCoordinates;
    gestureOriginInNewContentsCoordinates.scale(m_scaleFactor / oldScale);

    auto delta = gestureOriginInNewContentsCoordinates - gestureOriginInContentsCoordinates;
    auto newPosition = oldScrollPosition + delta;

    auto options = ScrollPositionChangeOptions::createUser();
    options.originalScrollDelta = delta;
    page->protectedScrollingCoordinator()->requestScrollToPosition(*this, newPosition, options);

    scheduleRenderingUpdate();
}

void UnifiedPDFPlugin::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    if (size() == pluginSize)
        return;

    PDFPluginBase::geometryDidChange(pluginSize, pluginToRootViewTransform);

    updateLayout();
}

void UnifiedPDFPlugin::updateLayout()
{
    m_documentLayout.updateLayout(size());
    updateLayerHierarchy();
    updateScrollbars();
    updateScrollingExtents();
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

RetainPtr<PDFDocument> UnifiedPDFPlugin::pdfDocumentForPrinting() const
{
    return nil;
}

FloatSize UnifiedPDFPlugin::pdfDocumentSizeForPrinting() const
{
    return { };
}

RefPtr<FragmentedSharedBuffer> UnifiedPDFPlugin::liveResourceData() const
{
    return nullptr;
}

void UnifiedPDFPlugin::didChangeScrollOffset()
{
    if (this->currentScrollType() == ScrollType::User)
        m_scrollContainerLayer->syncBoundsOrigin(IntPoint(m_scrollOffset));
    else
        m_scrollContainerLayer->setBoundsOrigin(IntPoint(m_scrollOffset));

    scheduleRenderingUpdate();
}

void UnifiedPDFPlugin::invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&)
{
}

void UnifiedPDFPlugin::invalidateScrollCornerRect(const WebCore::IntRect&)
{
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
    return WebCore::AffineTransform { }.scale(1.0f / m_documentLayout.scale()).translate(m_scrollOffset).mapPoint(point);
}

std::optional<PDFDocumentLayout::PageIndex> UnifiedPDFPlugin::nearestPageIndexForDocumentPoint(const WebCore::IntPoint& point) const
{
    switch (m_documentLayout.displayMode()) {
    case PDFDocumentLayout::DisplayMode::TwoUp:
    case PDFDocumentLayout::DisplayMode::TwoUpContinuous:
        // FIXME: Implement page index resolution for TwoUp display modes.
        notImplemented();
        break;
    case PDFDocumentLayout::DisplayMode::SinglePage:
    case PDFDocumentLayout::DisplayMode::Continuous: {
        // Walk down the document, find the first page bound that contains the position.
        // Note that we don't consider display direction (vertical vs horizontal).
        for (PDFDocumentLayout::PageIndex index = 0; index < m_documentLayout.pageCount(); ++index) {
            auto pageBounds = m_documentLayout.boundsForPageAtIndex(index);
            pageBounds.expand(PDFDocumentLayout::documentMargin);
            pageBounds.contract(PDFDocumentLayout::pageMargin);
            // At this point, this page index necessarily corresponds to the
            // nearest page to the point (so far), so we always keep track of it.
            if (point.y() <= pageBounds.maxY())
                return index;
        }
    }
    }

    return std::nullopt;
}

WebCore::IntPoint UnifiedPDFPlugin::convertFromDocumentToPage(const WebCore::IntPoint& point, PDFDocumentLayout::PageIndex pageIndex) const
{
    ASSERT(pageIndex < m_documentLayout.pageCount());

    auto pageBounds = m_documentLayout.boundsForPageAtIndex(pageIndex);
    auto pageRotation = m_documentLayout.rotationForPageAtIndex(pageIndex);
    auto pointInPDFPageSpace = IntPoint { point - WebCore::flooredIntPoint(pageBounds.location()) };
    auto documentSpaceToPageSpaceTransform = AffineTransform::makeRotation(pageRotation).translate([&pageBounds, pageRotation] () -> FloatPoint {
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

    auto pointWithTopLeftOrigin = documentSpaceToPageSpaceTransform.mapPoint(pointInPDFPageSpace);
    return IntPoint { pointWithTopLeftOrigin.x(), static_cast<int>(pageBounds.height()) - pointWithTopLeftOrigin.y() };
}

auto UnifiedPDFPlugin::pdfElementTypesForPluginPoint(const WebCore::IntPoint& point) const -> PDFElementTypes
{
    auto pointInDocumentSpace = convertFromPluginToDocument(point);
    auto maybeNearestPageIndex = nearestPageIndexForDocumentPoint(pointInDocumentSpace);
    if (!maybeNearestPageIndex || *maybeNearestPageIndex >= m_documentLayout.pageCount())
        return { };
    auto nearestPageIndex = *maybeNearestPageIndex;
    auto nearestPage = m_documentLayout.pageAtIndex(nearestPageIndex);
    auto pointInPDFPageSpace = convertFromDocumentToPage(pointInDocumentSpace, nearestPageIndex);

    PDFElementTypes pdfElementTypes { PDFElementType::Page };

    if (auto annotation = [nearestPage annotationAtPoint:pointInPDFPageSpace]) {
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

    if (!isTaggedPDF())
        return pdfElementTypes;

    if (auto pageLayout = [nearestPage pageLayout]) {
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
    switch (event.type()) {
    case WebEventType::MouseMove:
        mouseMovedInContentArea();
        switch (event.button()) {
        case WebMouseEventButton::None: {
            auto altKeyIsActive = event.altKey() ? AltKeyIsActive::Yes : AltKeyIsActive::No;
            auto mousePositionInPluginCoordinates = convertFromRootViewToPlugin(event.position());
            auto pdfElementTypes = pdfElementTypesForPluginPoint(mousePositionInPluginCoordinates);
            notifyCursorChanged(toWebCoreCursorType(pdfElementTypes, altKeyIsActive));
            return true;
        }
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

#if PLATFORM(MAC)
PDFContextMenu UnifiedPDFPlugin::createContextMenu(const IntPoint& contextMenuPoint) const
{
    Vector<PDFContextMenuItem> menuItems;

    // FIXME: We should also set the openInPreviewIndex when UnifiedPdfPlugin::openWithPreview is implemented.
    menuItems.append({ WebCore::contextMenuItemPDFOpenWithPreview(), 0,
        enumToUnderlyingType(ContextMenuItemTag::OpenWithPreview),
        ContextMenuItemEnablement::Enabled,
        ContextMenuItemHasAction::Yes,
        ContextMenuItemIsSeparator::No
    });

    menuItems.append({ String(), 0, invalidContextMenuItemTag, ContextMenuItemEnablement::Disabled, ContextMenuItemHasAction::No, ContextMenuItemIsSeparator::Yes });

    auto currentDisplayMode = contextMenuItemTagFromDisplyMode(m_documentLayout.displayMode());
    menuItems.append({ WebCore::contextMenuItemPDFSinglePage(), currentDisplayMode == ContextMenuItemTag::SinglePage, enumToUnderlyingType(ContextMenuItemTag::SinglePage), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });

    menuItems.append({ WebCore::contextMenuItemPDFSinglePageContinuous(), currentDisplayMode == ContextMenuItemTag::SinglePageContinuous, enumToUnderlyingType(ContextMenuItemTag::SinglePageContinuous), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });

    menuItems.append({ WebCore::contextMenuItemPDFTwoPages(), currentDisplayMode == ContextMenuItemTag::TwoPages, enumToUnderlyingType(ContextMenuItemTag::TwoPages), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });

    menuItems.append({ WebCore::contextMenuItemPDFTwoPagesContinuous(), currentDisplayMode == ContextMenuItemTag::TwoPagesContinuous, enumToUnderlyingType(ContextMenuItemTag::TwoPagesContinuous), ContextMenuItemEnablement::Enabled, ContextMenuItemHasAction::Yes, ContextMenuItemIsSeparator::No });

    return { contextMenuPoint, WTFMove(menuItems), { } };
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
        if (tag != contextMenuItemTagFromDisplyMode(m_documentLayout.displayMode())) {
            m_documentLayout.setDisplayMode(displayModeFromContextMenuItemTag(tag));
            updateLayout();
        }
    }
}
#endif // PLATFORM(MAC)

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

    auto sendResult = webPage->sendSync(Messages::WebPageProxy::ShowPDFContextMenu(contextMenu, m_identifier));
    auto [selectedItemTag] = sendResult.takeReplyOr(-1);

    if (selectedItemTag >= 0)
        performContextMenuAction(static_cast<ContextMenuItemTag>(selectedItemTag.value()));
    return true;
#else
    return false;
#endif // PLATFORM(MAC)
}

bool UnifiedPDFPlugin::handleKeyboardEvent(const WebKeyboardEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleEditingCommand(StringView commandName)
{
    return false;
}

bool UnifiedPDFPlugin::isEditingCommandEnabled(StringView commandName)
{
    return false;
}

String UnifiedPDFPlugin::getSelectionString() const
{
    return emptyString();
}

bool UnifiedPDFPlugin::existingSelectionContainsPoint(const FloatPoint&) const
{
    return false;
}

FloatRect UnifiedPDFPlugin::rectForSelectionInRootView(PDFSelection *) const
{
    return { };
}

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
}

void UnifiedPDFPlugin::zoomOut()
{
}

void UnifiedPDFPlugin::save(CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&& completionHandler)
{
}

void UnifiedPDFPlugin::openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&& completionHandler)
{
}

#endif // ENABLE(PDF_HUD)

bool UnifiedPDFPlugin::isTaggedPDF() const
{
    return CGPDFDocumentIsTaggedPDF([m_pdfDocument documentRef]);
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
