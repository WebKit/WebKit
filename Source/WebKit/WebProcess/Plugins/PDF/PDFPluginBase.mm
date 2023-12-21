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

#import "config.h"
#import "PDFPluginBase.h"

#if ENABLE(PDF_PLUGIN)

#import "MessageSenderInlines.h"
#import "PluginView.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <CoreFoundation/CoreFoundation.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/ArchiveResource.h>
#import <WebCore/Chrome.h>
#import <WebCore/Cursor.h>
#import <WebCore/Document.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/ScrollAnimator.h>
#import <WebCore/SharedBuffer.h>

#import "PDFKitSoftLink.h"

namespace WebKit {
using namespace WebCore;

PluginInfo PDFPluginBase::pluginInfo()
{
    PluginInfo info;

    // Note: HTML specification requires that the WebKit built-in PDF name
    // is presented in plain English text.
    // https://html.spec.whatwg.org/multipage/system-state.html#pdf-viewing-support
    info.name = "WebKit built-in PDF"_s;
    info.desc = pdfDocumentTypeDescription();
    info.file = "internal-pdf-viewer"_s;
    info.isApplicationPlugin = true;
    info.clientLoadPolicy = PluginLoadClientPolicy::Undefined;
    info.bundleIdentifier = "com.apple.webkit.builtinpdfplugin"_s;

    MimeClassInfo pdfMimeClassInfo;
    pdfMimeClassInfo.type = "application/pdf"_s;
    pdfMimeClassInfo.desc = pdfDocumentTypeDescription();
    pdfMimeClassInfo.extensions.append("pdf"_s);
    info.mimes.append(pdfMimeClassInfo);

    MimeClassInfo textPDFMimeClassInfo;
    textPDFMimeClassInfo.type = "text/pdf"_s;
    textPDFMimeClassInfo.desc = pdfDocumentTypeDescription();
    textPDFMimeClassInfo.extensions.append("pdf"_s);
    info.mimes.append(textPDFMimeClassInfo);

    return info;
}

PDFPluginBase::PDFPluginBase(HTMLPlugInElement& element)
    : m_frame(*WebFrame::fromCoreFrame(*element.document().frame()))
    , m_element(element)
    , m_identifier(PDFPluginIdentifier::generate())
{
}

PDFPluginBase::~PDFPluginBase()
{
#if ENABLE(PDF_HUD)
    if (auto* page = m_frame ? m_frame->page() : nullptr)
        page->removePDFHUD(*this);
#endif
}

void PDFPluginBase::teardown()
{
    destroyScrollbar(ScrollbarOrientation::Horizontal);
    destroyScrollbar(ScrollbarOrientation::Vertical);
}

Page* PDFPluginBase::page() const
{
    if (RefPtr coreFrame = m_frame ? m_frame->coreLocalFrame() : nullptr)
        return coreFrame->page();
    return nullptr;
}

void PDFPluginBase::setView(PluginView& view)
{
    ASSERT(!m_view);
    m_view = view;
}

void PDFPluginBase::destroy()
{
    ASSERT(!m_isBeingDestroyed);
    SetForScope scope { m_isBeingDestroyed, true };

    m_hasBeenDestroyed = true;
    m_documentFinishedLoading = true;

    teardown();

    m_view = nullptr;
}

void PDFPluginBase::createPDFDocument()
{
    m_pdfDocument = adoptNS([allocPDFDocumentInstance() initWithData:rawData()]);
}

bool PDFPluginBase::isFullFramePlugin() const
{
    // <object> or <embed> plugins will appear to be in their parent frame, so we have to
    // check whether our frame's widget is exactly our PluginView.
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;

    RefPtr document = m_frame->coreLocalFrame()->document();
    if (!is<PluginDocument>(document))
        return false;
    return downcast<PluginDocument>(*document).pluginWidget() == m_view;
}

bool PDFPluginBase::handlesPageScaleFactor() const
{
    return m_frame && m_frame->isMainFrame() && isFullFramePlugin();
}

bool PDFPluginBase::isLocked() const
{
    return [m_pdfDocument isLocked];
}

NSData *PDFPluginBase::rawData() const
{
    return (__bridge NSData *)m_data.get();
}

void PDFPluginBase::ensureDataBufferLength(uint64_t targetLength)
{
    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    auto currentLength = CFDataGetLength(m_data.get());
    ASSERT(currentLength >= 0);
    if (targetLength > (uint64_t)currentLength)
        CFDataIncreaseLength(m_data.get(), targetLength - currentLength);
}

void PDFPluginBase::streamDidReceiveResponse(const ResourceResponse& response)
{
    m_suggestedFilename = response.suggestedFilename();
    if (m_suggestedFilename.isEmpty())
        m_suggestedFilename = suggestedFilenameWithMIMEType(nil, "application/pdf"_s);
    if (!m_suggestedFilename.endsWithIgnoringASCIICase(".pdf"_s))
        m_suggestedFilename = makeString(m_suggestedFilename, ".pdf"_s);
}

void PDFPluginBase::streamDidReceiveData(const SharedBuffer& buffer)
{
    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    ensureDataBufferLength(m_streamedBytes + buffer.size());
    memcpy(CFDataGetMutableBytePtr(m_data.get()) + m_streamedBytes, buffer.data(), buffer.size());
    m_streamedBytes += buffer.size();

    incrementalPDFStreamDidReceiveData(buffer);
}

void PDFPluginBase::streamDidFinishLoading()
{
    if (m_hasBeenDestroyed)
        return;

    addArchiveResource();

    m_documentFinishedLoading = true;

    if (!incrementalPDFStreamDidFinishLoading()) {
        createPDFDocument();
        installPDFDocument();
    }

    tryRunScriptsInPDFDocument();
}

void PDFPluginBase::streamDidFail()
{
    m_data = nullptr;
    incrementalPDFStreamDidFail();
}

void PDFPluginBase::addArchiveResource()
{
    // FIXME: It's a hack to force add a resource to DocumentLoader. PDF documents should just be fetched as CachedResources.

    // Add just enough data for context menu handling and web archives to work.
    NSDictionary* headers = @{ @"Content-Disposition": (NSString *)m_suggestedFilename, @"Content-Type" : @"application/pdf" };
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:m_view->mainResourceURL() statusCode:200 HTTPVersion:(NSString*)kCFHTTPVersion1_1 headerFields:headers]);
    ResourceResponse synthesizedResponse(response.get());

    auto resource = ArchiveResource::create(SharedBuffer::create(m_data.get()), m_view->mainResourceURL(), "application/pdf"_s, String(), String(), synthesizedResponse);
    m_view->frame()->document()->loader()->addArchiveResource(resource.releaseNonNull());
}

void PDFPluginBase::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    m_size = pluginSize;
    m_rootViewToPluginTransform = valueOrDefault(pluginToRootViewTransform.inverse());

#if ENABLE(PDF_HUD)
    updatePDFHUDLocation();
#endif
}

void PDFPluginBase::visibilityDidChange(bool visible)
{
#if ENABLE(PDF_HUD)
    if (!m_frame || !hudEnabled())
        return;
    if (visible)
        m_frame->page()->createPDFHUD(*this, frameForHUDInRootViewCoordinates());
    else
        m_frame->page()->removePDFHUD(*this);
#endif
}

void PDFPluginBase::invalidateRect(const IntRect& rect)
{
    if (!m_view)
        return;

    m_view->invalidateRect(rect);
}

IntPoint PDFPluginBase::convertFromRootViewToPlugin(const IntPoint& point) const
{
    return m_rootViewToPluginTransform.mapPoint(point);
}

IntRect PDFPluginBase::convertFromRootViewToPlugin(const IntRect& rect) const
{
    return m_rootViewToPluginTransform.mapRect(rect);
}

IntPoint PDFPluginBase::convertFromPluginToRootView(const IntPoint& point) const
{
    return m_rootViewToPluginTransform.inverse()->mapPoint(point);
}

IntRect PDFPluginBase::convertFromPluginToRootView(const IntRect& rect) const
{
    return m_rootViewToPluginTransform.inverse()->mapRect(rect);
}

IntRect PDFPluginBase::boundsOnScreen() const
{
    return WebCore::Accessibility::retrieveValueFromMainThread<WebCore::IntRect>([&] () -> WebCore::IntRect {
        FloatRect bounds = FloatRect(FloatPoint(), size());
        FloatRect rectInRootViewCoordinates = valueOrDefault(m_rootViewToPluginTransform.inverse()).mapRect(bounds);
        RefPtr page = this->page();
        if (!page)
            return { };
        return page->chrome().rootViewToScreen(enclosingIntRect(rectInRootViewCoordinates));
    });
}

void PDFPluginBase::updateControlTints(GraphicsContext& graphicsContext)
{
    ASSERT(graphicsContext.invalidatingControlTints());

    if (RefPtr horizontalScrollbar = m_horizontalScrollbar)
        horizontalScrollbar->invalidate();
    if (RefPtr verticalScrollbar = m_verticalScrollbar)
        verticalScrollbar->invalidate();
    invalidateScrollCorner(scrollCornerRect());
}

IntRect PDFPluginBase::scrollCornerRect() const
{
    if (!m_horizontalScrollbar || !m_verticalScrollbar)
        return IntRect();
    if (m_horizontalScrollbar->isOverlayScrollbar()) {
        ASSERT(m_verticalScrollbar->isOverlayScrollbar());
        return IntRect();
    }
    return IntRect(m_view->width() - m_verticalScrollbar->width(), m_view->height() - m_horizontalScrollbar->height(), m_verticalScrollbar->width(), m_horizontalScrollbar->height());
}

ScrollableArea* PDFPluginBase::enclosingScrollableArea() const
{
    // FIXME: Walk up the frame tree and look for a scrollable parent frame or RenderLayer.
    return nullptr;
}

IntRect PDFPluginBase::scrollableAreaBoundingBox(bool*) const
{
    return m_view->frameRect();
}

void PDFPluginBase::setScrollOffset(const ScrollOffset& offset)
{
    m_scrollOffset = IntSize(offset.x(), offset.y());

    didChangeScrollOffset();
}

bool PDFPluginBase::isActive() const
{
    if (RefPtr page = this->page())
        return page->focusController().isActive();

    return false;
}

bool PDFPluginBase::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    if (RefPtr page = this->page())
        return page->settings().scrollingPerformanceTestingEnabled();

    return false;
}

ScrollPosition PDFPluginBase::scrollPosition() const
{
    return IntPoint(m_scrollOffset.width(), m_scrollOffset.height());
}

ScrollPosition PDFPluginBase::minimumScrollPosition() const
{
    return IntPoint();
}

ScrollPosition PDFPluginBase::maximumScrollPosition() const
{
    IntSize scrollbarSpace = scrollbarIntrusion();
    auto pdfDocumentSize = contentsSize();

    IntPoint maximumOffset(pdfDocumentSize.width() - m_size.width() + scrollbarSpace.width(), pdfDocumentSize.height() - m_size.height() + scrollbarSpace.height());
    maximumOffset.clampNegativeToZero();
    return maximumOffset;
}

float PDFPluginBase::deviceScaleFactor() const
{
    if (RefPtr page = this->page())
        return page->deviceScaleFactor();
    return 1;
}

void PDFPluginBase::scrollbarStyleChanged(ScrollbarStyle style, bool forceUpdate)
{
    if (!forceUpdate)
        return;

    if (m_hasBeenDestroyed)
        return;

    // If the PDF was scrolled all the way to bottom right and scrollbars change to overlay style, we don't want to display white rectangles where scrollbars were.
    IntPoint newScrollOffset = IntPoint(m_scrollOffset).shrunkTo(maximumScrollPosition());
    setScrollOffset(newScrollOffset);

    ScrollableArea::scrollbarStyleChanged(style, forceUpdate);
    // As size of the content area changes, scrollbars may need to appear or to disappear.
    updateScrollbars();
}

IntRect PDFPluginBase::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    IntRect rect = scrollbarRect;
    rect.move(scrollbar.location() - m_view->location());

    return m_view->frame()->protectedView()->convertFromRendererToContainingView(m_view->pluginElement().renderer(), rect);
}

IntRect PDFPluginBase::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    IntRect rect = m_view->frame()->protectedView()->convertFromContainingViewToRenderer(m_view->pluginElement().renderer(), parentRect);
    rect.move(m_view->location() - scrollbar.location());

    return rect;
}

IntPoint PDFPluginBase::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    IntPoint point = scrollbarPoint;
    point.move(scrollbar.location() - m_view->location());

    return m_view->frame()->protectedView()->convertFromRendererToContainingView(m_view->pluginElement().renderer(), point);
}

IntPoint PDFPluginBase::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    IntPoint point = m_view->frame()->protectedView()->convertFromContainingViewToRenderer(m_view->pluginElement().renderer(), parentPoint);
    point.move(m_view->location() - scrollbar.location());

    return point;
}

String PDFPluginBase::debugDescription() const
{
    return makeString("PDFPluginBase 0x", hex(reinterpret_cast<uintptr_t>(this), Lowercase));
}

void PDFPluginBase::willDetachRenderer()
{
    if (!m_frame || !m_frame->coreLocalFrame())
        return;
    if (RefPtr frameView = m_frame->coreLocalFrame()->view())
        frameView->removeScrollableArea(this);
}

IntRect PDFPluginBase::viewRelativeVerticalScrollbarRect() const
{
    if (!m_verticalScrollbar)
        return { };

    auto scrollbarRect = IntRect({ }, size());
    scrollbarRect.shiftXEdgeTo(scrollbarRect.maxX() - m_verticalScrollbar->width());

    if (m_horizontalScrollbar)
        scrollbarRect.contract(0, m_horizontalScrollbar->height());

    return scrollbarRect;
}

IntRect PDFPluginBase::viewRelativeHorizontalScrollbarRect() const
{
    if (!m_horizontalScrollbar)
        return { };

    auto scrollbarRect = IntRect({ }, size());
    scrollbarRect.shiftYEdgeTo(scrollbarRect.maxY() - m_horizontalScrollbar->height());

    if (m_verticalScrollbar)
        scrollbarRect.contract(m_verticalScrollbar->width(), 0);

    return scrollbarRect;
}

IntRect PDFPluginBase::viewRelativeScrollCornerRect() const
{
    IntSize scrollbarSpace = scrollbarIntrusion();
    if (scrollbarSpace.isEmpty())
        return { };

    auto cornerRect = IntRect({ }, size());
    cornerRect.shiftXEdgeTo(cornerRect.maxX() - scrollbarSpace.width());
    cornerRect.shiftYEdgeTo(cornerRect.maxY() - scrollbarSpace.height());
    return cornerRect;
}

void PDFPluginBase::updateScrollbars()
{
    if (m_hasBeenDestroyed)
        return;

    bool hadScrollbars = m_horizontalScrollbar || m_verticalScrollbar;
    auto pdfDocumentSize = contentsSize();

    if (m_horizontalScrollbar) {
        if (m_size.width() >= pdfDocumentSize.width())
            destroyScrollbar(ScrollbarOrientation::Horizontal);
    } else if (m_size.width() < pdfDocumentSize.width())
        m_horizontalScrollbar = createScrollbar(ScrollbarOrientation::Horizontal);

    if (m_verticalScrollbar) {
        if (m_size.height() >= pdfDocumentSize.height())
            destroyScrollbar(ScrollbarOrientation::Vertical);
    } else if (m_size.height() < pdfDocumentSize.height())
        m_verticalScrollbar = createScrollbar(ScrollbarOrientation::Vertical);

    if (m_horizontalScrollbar) {
        auto scrollbarRect = viewRelativeHorizontalScrollbarRect();
        scrollbarRect.moveBy(m_view->location());
        m_horizontalScrollbar->setFrameRect(scrollbarRect);

        m_horizontalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), firstPageHeight());
        m_horizontalScrollbar->setProportion(scrollbarRect.width(), pdfDocumentSize.width());
    }

    if (m_verticalScrollbar) {
        auto scrollbarRect = viewRelativeVerticalScrollbarRect();
        scrollbarRect.moveBy(m_view->location());
        m_verticalScrollbar->setFrameRect(scrollbarRect);

        m_verticalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), firstPageHeight());
        m_verticalScrollbar->setProportion(scrollbarRect.height(), pdfDocumentSize.height());
    }

    RefPtr frameView = m_frame ? m_frame->coreLocalFrame()->view() : nullptr;
    if (!frameView)
        return;

    bool hasScrollbars = m_horizontalScrollbar || m_verticalScrollbar;
    if (hadScrollbars != hasScrollbars) {
        if (hasScrollbars)
            frameView->addScrollableArea(this);
        else
            frameView->removeScrollableArea(this);

        frameView->setNeedsLayoutAfterViewConfigurationChange();
    }
}

Ref<Scrollbar> PDFPluginBase::createScrollbar(ScrollbarOrientation orientation)
{
    Ref widget = Scrollbar::createNativeScrollbar(*this, orientation, ScrollbarWidth::Auto);
    didAddScrollbar(widget.ptr(), orientation);

    if (RefPtr page = this->page()) {
        if (page->isMonitoringWheelEvents())
            scrollAnimator().setWheelEventTestMonitor(page->wheelEventTestMonitor());
    }

    if (RefPtr frame = m_view->frame()) {
        if (RefPtr frameView = frame->view())
            frameView->addChild(widget);
    }

    return widget;
}

void PDFPluginBase::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == ScrollbarOrientation::Horizontal ? m_horizontalScrollbar : m_verticalScrollbar;
    if (!scrollbar)
        return;

    willRemoveScrollbar(scrollbar.get(), orientation);
    scrollbar->removeFromParent();
    scrollbar = nullptr;
}

#if ENABLE(PDF_HUD)

void PDFPluginBase::updatePDFHUDLocation()
{
    if (isLocked() || !m_frame || !m_frame->page())
        return;
    m_frame->protectedPage()->updatePDFHUDLocation(*this, frameForHUDInRootViewCoordinates());
}

IntRect PDFPluginBase::frameForHUDInRootViewCoordinates() const
{
    return convertFromPluginToRootView(IntRect(IntPoint(), size()));
}

bool PDFPluginBase::hudEnabled() const
{
    if (RefPtr page = this->page())
        return page->settings().pdfPluginHUDEnabled();
    return false;
}

#endif // ENABLE(PDF_HUD)

void PDFPluginBase::notifyCursorChanged(WebCore::PlatformCursorType cursorType)
{
    if (!m_frame || !m_frame->page())
        return;

    m_frame->protectedPage()->send(Messages::WebPageProxy::SetCursor(WebCore::Cursor::fromType(cursorType)));
}

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
