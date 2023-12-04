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
#include "PluginView.h"
#include "WebEventType.h"
#include "WebMouseEvent.h"
#include "WebPageProxyMessages.h"
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/Chrome.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/ColorCocoa.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/ScrollTypes.h>

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

    CheckedPtr page = this->page();
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
    CheckedPtr page = this->page();
    if (!page)
        return nullptr;

    auto* graphicsLayerFactory = page->chrome().client().graphicsLayerFactory();
    Ref graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, *this, layerType);
    graphicsLayer->setName(name);
    return graphicsLayer;
}

void UnifiedPDFPlugin::scheduleRenderingUpdate()
{
    CheckedPtr page = this->page();
    if (!page)
        return;

    page->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

void UnifiedPDFPlugin::updateLayerHierarchy()
{
    CheckedPtr page = this->page();
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
    CheckedPtr page = this->page();
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
    CheckedPtr page = this->page();
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
    return 1;
}

float UnifiedPDFPlugin::deviceScaleFactor() const
{
    return PDFPluginBase::deviceScaleFactor();
}

float UnifiedPDFPlugin::pageScaleFactor() const
{
    if (CheckedPtr page = this->page())
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

    CheckedPtr page = this->page();
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
    CheckedPtr page = this->page();
    if (!page)
        return;
    auto& scrollingCoordinator = *page->scrollingCoordinator();
    scrollingCoordinator.setScrollingNodeScrollableAreaGeometry(m_scrollingNodeID, *this);

    EventRegion eventRegion;
    auto eventRegionContext = eventRegion.makeContext();
    eventRegionContext.unite(enclosingIntRect(FloatRect({ }, size())), *m_element->renderer(), m_element->renderer()->style());
    m_scrollContainerLayer->setEventRegion(WTFMove(eventRegion));
}

enum class AltKeyIsActive : bool { No, Yes };

static WebCore::Cursor::Type toWebCoreCursorType(UnifiedPDFPlugin::PDFElementTypes pdfElementTypes, AltKeyIsActive altKeyIsActive = AltKeyIsActive::No)
{
    if (pdfElementTypes.containsAny({ UnifiedPDFPlugin::PDFElementType::Link, UnifiedPDFPlugin::PDFElementType::Control, UnifiedPDFPlugin::PDFElementType::Icon }) || altKeyIsActive == AltKeyIsActive::Yes)
        return WebCore::Cursor::Type::Hand;

    if (pdfElementTypes.containsAny({ UnifiedPDFPlugin::PDFElementType::Text, UnifiedPDFPlugin::PDFElementType::TextField }))
        return WebCore::Cursor::Type::IBeam;

    return WebCore::Cursor::Type::Pointer;
}

auto UnifiedPDFPlugin::pdfElementTypesForPluginPoint(const WebCore::IntPoint&) const -> PDFElementTypes
{
    return { };
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

    // FIXME: We should also set the openInPreviewIndex when UnifiedPdfPlugin::openWithPreview is implemented
    menuItems.append({ WebCore::contextMenuItemPDFOpenWithPreview(), true, false, 0, true, static_cast<uint8_t>(ContextMenuItemTag::OpenWithPreview) });

    return { contextMenuPoint, WTFMove(menuItems), { } };
}

void UnifiedPDFPlugin::performContextMenuAction(ContextMenuItemTag tag) const
{
    switch (tag) {
    // The OpenWithPreviewAction is handled in the UI Process
    case ContextMenuItemTag::OpenWithPreview: return;
    }
}
#endif

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
#endif
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

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
