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

#include "PluginView.h"
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
}

void UnifiedPDFPlugin::teardown()
{
    PDFPluginBase::teardown();

    if (m_rootLayer)
        m_rootLayer->removeFromParent();
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
    if (!m_rootLayer) {
        m_rootLayer = createGraphicsLayer("UnifiedPDFPlugin root"_s, GraphicsLayer::Type::Normal);
        m_rootLayer->setAnchorPoint({ });
    }

    if (!m_clippingLayer) {
        m_clippingLayer = createGraphicsLayer("UnifiedPDFPlugin clipping"_s, GraphicsLayer::Type::Normal);
        m_clippingLayer->setAnchorPoint({ });
        m_clippingLayer->setMasksToBounds(true);
        m_rootLayer->addChild(*m_clippingLayer);
    }

    if (!m_scrollingLayer) {
        m_scrollingLayer = createGraphicsLayer("UnifiedPDFPlugin scrolling"_s, GraphicsLayer::Type::Normal);
        m_scrollingLayer->setAnchorPoint({ });
        m_clippingLayer->addChild(*m_scrollingLayer);
    }

    if (!m_contentsLayer) {
        m_contentsLayer = createGraphicsLayer("UnifiedPDFPlugin contents"_s, GraphicsLayer::Type::TiledBacking);
        m_contentsLayer->setAnchorPoint({ });
        m_contentsLayer->setDrawsContent(true);
        m_scrollingLayer->addChild(*m_contentsLayer);
    }

    m_clippingLayer->setSize(size());

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
    propagateSettingsToLayer(*m_clippingLayer);
    propagateSettingsToLayer(*m_scrollingLayer);
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

void UnifiedPDFPlugin::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
{
    if (layer != m_contentsLayer.get())
        return;

    if (m_size.isEmpty() || contentsSize().isEmpty())
        return;

    auto drawingRect = IntRect { { }, contentsSize() };
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

        // Translate the context to the bottom of pageBounds and flip, so that CGPDFPageGetDrawingTransform operates
        // from this page's drawing origin.
        bufferContext.translate(destinationRect.minXMaxYCorner());
        bufferContext.scale({ 1, -1 });

        destinationRect.setLocation({ });
        auto transform = [page transformForBox:kPDFDisplayBoxCropBox];
        bufferContext.concatCTM(transform);

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
}

IntSize UnifiedPDFPlugin::contentsSize() const
{
    if (isLocked())
        return { 0, 0 };
    return expandedIntSize(m_documentLayout.scaledContentsSize());
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
    m_scrollingLayer->setPosition({ -static_cast<float>(m_scrollOffset.width()), -static_cast<float>(m_scrollOffset.height()) });
    scheduleRenderingUpdate();
}

void UnifiedPDFPlugin::invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&)
{
}

void UnifiedPDFPlugin::invalidateScrollCornerRect(const WebCore::IntRect&)
{
}

bool UnifiedPDFPlugin::handleMouseEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleWheelEvent(const WebWheelEvent& event)
{
    return ScrollableArea::handleWheelEventForScrolling(platform(event), { });
}

bool UnifiedPDFPlugin::handleMouseEnterEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleMouseLeaveEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleContextMenuEvent(const WebMouseEvent&)
{
    return false;
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

RefPtr<ShareableBitmap> UnifiedPDFPlugin::snapshot()
{
    return nullptr;
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
