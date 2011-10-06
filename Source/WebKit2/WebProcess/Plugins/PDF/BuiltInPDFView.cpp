/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "BuiltInPDFView.h"

#include "PluginView.h"
#include "ShareableBitmap.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/Page.h>
#include <WebCore/PluginData.h>
#include <WebCore/ScrollAnimator.h>
#include <WebCore/ScrollbarTheme.h>

using namespace WebCore;
using namespace std;

namespace WebKit {

const uint64_t pdfDocumentRequestID = 1; // PluginController supports loading multiple streams, but we only need one for PDF.

PassRefPtr<BuiltInPDFView> BuiltInPDFView::create(Page* page)
{
    return adoptRef(new BuiltInPDFView(page));
}

BuiltInPDFView::BuiltInPDFView(Page* page)
    : m_page(page)
{
    m_page->addScrollableArea(this);
}

BuiltInPDFView::~BuiltInPDFView()
{
    if (m_page)
        m_page->removeScrollableArea(this);

    if (m_horizontalScrollbar)
        willRemoveHorizontalScrollbar(m_horizontalScrollbar.get());
    if (m_verticalScrollbar)
        willRemoveVerticalScrollbar(m_verticalScrollbar.get());
}

PluginInfo BuiltInPDFView::pluginInfo()
{
    PluginInfo info;
    info.name = builtInPDFPluginName();

    MimeClassInfo mimeClassInfo;
    mimeClassInfo.type ="application/pdf";
    mimeClassInfo.desc = pdfDocumentTypeDescription();
    mimeClassInfo.extensions.append("pdf");

    info.mimes.append(mimeClassInfo);
    return info;
}

PluginView* BuiltInPDFView::pluginView()
{
    return static_cast<PluginView*>(controller());
}

const PluginView* BuiltInPDFView::pluginView() const
{
    return static_cast<const PluginView*>(controller());
}

void BuiltInPDFView::calculateDocumentSize()
{
    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(m_pdfDocument.get(), 1); // FIXME: Draw all pages of a document.
    if (!pdfPage)
        return;

    CGRect box = CGPDFPageGetBoxRect(pdfPage, kCGPDFCropBox);
    if (CGRectIsEmpty(box))
        box = CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);
    m_pdfDocumentSize = IntSize(box.size);
}

void BuiltInPDFView::updateScrollbars()
{
    if (m_horizontalScrollbar) {
        if (m_frameRect.width() >= m_pdfDocumentSize.width())
            destroyScrollbar(HorizontalScrollbar);
    } else if (m_frameRect.width() < m_pdfDocumentSize.width())
        m_horizontalScrollbar = createScrollbar(HorizontalScrollbar);

    if (m_verticalScrollbar) {
        if (m_frameRect.height() >= m_pdfDocumentSize.height())
            destroyScrollbar(VerticalScrollbar);
    } else if (m_frameRect.height() < m_pdfDocumentSize.height())
        m_verticalScrollbar = createScrollbar(VerticalScrollbar);

    int horizontalScrollbarHeight = (m_horizontalScrollbar && !m_horizontalScrollbar->isOverlayScrollbar()) ? m_horizontalScrollbar->height() : 0;
    int verticalScrollbarWidth = (m_verticalScrollbar && !m_verticalScrollbar->isOverlayScrollbar()) ? m_verticalScrollbar->width() : 0;

    // FIXME: Use document page size for PageDown step.
    int clientHeight = m_pdfDocumentSize.height();
    int pageStep = max(max<int>(clientHeight * Scrollbar::minFractionToStepWhenPaging(), clientHeight - Scrollbar::maxOverlapBetweenPages()), 1);

    if (m_horizontalScrollbar) {
        m_horizontalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_horizontalScrollbar->setProportion(m_frameRect.width() - verticalScrollbarWidth, m_pdfDocumentSize.width());
        IntRect scrollbarRect(pluginView()->x(), pluginView()->y() + m_frameRect.height() - m_horizontalScrollbar->height(), m_frameRect.width(), m_horizontalScrollbar->height());
        if (m_verticalScrollbar)
            scrollbarRect.contract(m_verticalScrollbar->width(), 0);
        m_horizontalScrollbar->setFrameRect(scrollbarRect);
    }
    if (m_verticalScrollbar) {
        m_verticalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_verticalScrollbar->setProportion(m_frameRect.height() - horizontalScrollbarHeight, m_pdfDocumentSize.height());
        IntRect scrollbarRect(IntRect(pluginView()->x() + m_frameRect.width() - m_verticalScrollbar->width(), pluginView()->y(), m_verticalScrollbar->width(), m_frameRect.height()));
        if (m_horizontalScrollbar)
            scrollbarRect.contract(0, m_horizontalScrollbar->height());
        m_verticalScrollbar->setFrameRect(scrollbarRect);
    }
}

void BuiltInPDFView::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
    pluginView()->frame()->document()->didAddWheelEventHandler();
    ScrollableArea::didAddHorizontalScrollbar(scrollbar);
}

void BuiltInPDFView::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    ScrollableArea::willRemoveHorizontalScrollbar(scrollbar);
    // FIXME: Maybe need a separate ScrollableArea::didRemoveHorizontalScrollbar callback?
    if (PluginView* pluginView = this->pluginView())
        pluginView->frame()->document()->didRemoveWheelEventHandler();
}

PassRefPtr<Scrollbar> BuiltInPDFView::createScrollbar(ScrollbarOrientation orientation)
{
    // FIXME: Support custom scrollbar styles.
    RefPtr<Scrollbar> widget = Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
    if (orientation == HorizontalScrollbar)
        scrollAnimator()->didAddHorizontalScrollbar(widget.get());
    else 
        scrollAnimator()->didAddVerticalScrollbar(widget.get());
    pluginView()->frame()->view()->addChild(widget.get());
    return widget.release();
}

void BuiltInPDFView::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_horizontalScrollbar : m_verticalScrollbar;
    if (!scrollbar)
        return;

    if (orientation == HorizontalScrollbar)
        scrollAnimator()->willRemoveHorizontalScrollbar(scrollbar.get());
    else
        scrollAnimator()->willRemoveVerticalScrollbar(scrollbar.get());

    scrollbar->removeFromParent();
    scrollbar->disconnectFromScrollableArea();
    scrollbar = 0;
}

void BuiltInPDFView::pdfDocumentDidLoad()
{
    calculateDocumentSize();
    updateScrollbars();

    controller()->invalidate(IntRect(0, 0, m_frameRect.width(), m_frameRect.height()));
}

bool BuiltInPDFView::initialize(const Parameters& parameters)
{
    // Load the src URL if needed.
    if (!parameters.loadManually && !parameters.url.isEmpty())
        controller()->loadURL(pdfDocumentRequestID, "GET", parameters.url.string(), String(), HTTPHeaderMap(), Vector<uint8_t>(), false);

    return true;
}

void BuiltInPDFView::destroy()
{
}

void BuiltInPDFView::paint(GraphicsContext* graphicsContext, const IntRect& dirtyRectInWindowCoordinates)
{
    if (!m_pdfDocument) // FIXME: Draw background and loading progress.
        return;

    // FIXME: This function just draws the fist page of a document at top left corner.
    // We should show the whole document, centering small ones.
    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(m_pdfDocument.get(), 1);
    if (!pdfPage)
        return;

    scrollAnimator()->contentAreaWillPaint();

    CGContextRef context = graphicsContext->platformContext();
    GraphicsContextStateSaver stateSaver(*graphicsContext);
    graphicsContext->clip(dirtyRectInWindowCoordinates);
    graphicsContext->setImageInterpolationQuality(InterpolationHigh);
    graphicsContext->setShouldAntialias(true);
    graphicsContext->setShouldSmoothFonts(true);

    CGRect pageBox = CGPDFPageGetBoxRect(pdfPage, kCGPDFCropBox);
    if (CGRectIsEmpty(pageBox))
        pageBox = CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);

    CGContextClipToRect(context, CGRectMake(m_frameRect.x(), m_frameRect.y(), m_pdfDocumentSize.width() - m_scrollOffset.width(), m_pdfDocumentSize.height() - m_scrollOffset.height()));
    CGContextTranslateCTM(context, m_frameRect.x() - pageBox.origin.x - m_scrollOffset.width(), m_frameRect.y() + pageBox.origin.y + m_pdfDocumentSize.height() - m_scrollOffset.height());

    CGContextScaleCTM(context, 1, -1);
    CGContextDrawPDFPage(context, pdfPage);

    stateSaver.restore();

    stateSaver.save();
    // Undo translation to window coordinates performed by PluginView::paint().
    IntRect dirtyRect = pluginView()->parent()->windowToContents(dirtyRectInWindowCoordinates);
    IntPoint documentOriginInWindowCoordinates = pluginView()->parent()->windowToContents(IntPoint());
    graphicsContext->translate(-documentOriginInWindowCoordinates.x(), -documentOriginInWindowCoordinates.y());
    if (m_horizontalScrollbar)
        m_horizontalScrollbar->paint(graphicsContext, dirtyRect);
    if (m_verticalScrollbar)
        m_verticalScrollbar->paint(graphicsContext, dirtyRect);

    IntRect dirtyCornerRect = intersection(scrollCornerRect(), dirtyRect);
    ScrollbarTheme::nativeTheme()->paintScrollCorner(0, graphicsContext, dirtyCornerRect);
}

void BuiltInPDFView::updateControlTints(GraphicsContext* graphicsContext)
{
    ASSERT(graphicsContext->updatingControlTints());

    if (m_horizontalScrollbar)
        m_horizontalScrollbar->invalidate();
    if (m_verticalScrollbar)
        m_verticalScrollbar->invalidate();
    invalidateScrollCorner(scrollCornerRect());
}

PassRefPtr<ShareableBitmap> BuiltInPDFView::snapshot()
{
    return 0;
}

#if PLATFORM(MAC)
PlatformLayer* BuiltInPDFView::pluginLayer()
{
    return 0;
}
#endif


bool BuiltInPDFView::isTransparent()
{
    // This should never be called from the web process.
    ASSERT_NOT_REACHED();
    return false;
}

void BuiltInPDFView::geometryDidChange(const IntRect& frameRect, const IntRect& clipRect)
{
    if (m_frameRect == frameRect) {
        // Nothing to do.
        return;
    }

    m_frameRect = frameRect;
    updateScrollbars();
}

void BuiltInPDFView::visibilityDidChange()
{
}

void BuiltInPDFView::frameDidFinishLoading(uint64_t)
{
    ASSERT_NOT_REACHED();
}

void BuiltInPDFView::frameDidFail(uint64_t, bool)
{
    ASSERT_NOT_REACHED();
}

void BuiltInPDFView::didEvaluateJavaScript(uint64_t, const WTF::String&)
{
    ASSERT_NOT_REACHED();
}

void BuiltInPDFView::streamDidReceiveResponse(uint64_t streamID, const KURL&, uint32_t, uint32_t, const WTF::String&, const WTF::String&)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);
}
                                           
void BuiltInPDFView::streamDidReceiveData(uint64_t streamID, const char* bytes, int length)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    if (!m_dataBuffer)
        m_dataBuffer.adoptCF(CFDataCreateMutable(0, 0));

    CFDataAppendBytes(m_dataBuffer.get(), reinterpret_cast<const UInt8*>(bytes), length);
}

void BuiltInPDFView::streamDidFinishLoading(uint64_t streamID)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    RetainPtr<CGDataProviderRef> pdfDataProvider(AdoptCF, CGDataProviderCreateWithCFData(m_dataBuffer.get()));
    m_pdfDocument.adoptCF(CGPDFDocumentCreateWithProvider(pdfDataProvider.get()));

    pdfDocumentDidLoad();
}

void BuiltInPDFView::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    m_dataBuffer.clear();
}

void BuiltInPDFView::manualStreamDidReceiveResponse(const KURL& responseURL, uint32_t streamLength,  uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers)
{
}

void BuiltInPDFView::manualStreamDidReceiveData(const char* bytes, int length)
{
    if (!m_dataBuffer)
        m_dataBuffer.adoptCF(CFDataCreateMutable(0, 0));

    CFDataAppendBytes(m_dataBuffer.get(), reinterpret_cast<const UInt8*>(bytes), length);
}

void BuiltInPDFView::manualStreamDidFinishLoading()
{
    RetainPtr<CGDataProviderRef> pdfDataProvider(AdoptCF, CGDataProviderCreateWithCFData(m_dataBuffer.get()));
    m_pdfDocument.adoptCF(CGPDFDocumentCreateWithProvider(pdfDataProvider.get()));

    pdfDocumentDidLoad();
}

void BuiltInPDFView::manualStreamDidFail(bool)
{
    m_dataBuffer.clear();
}

bool BuiltInPDFView::handleMouseEvent(const WebMouseEvent& event)
{
    switch (event.type()) {
    case WebEvent::MouseMove:
        scrollAnimator()->mouseMovedInContentArea();
        // FIXME: Should also notify scrollbar to show hover effect. Should also send mouseExited to hide it.
        break;
    case WebEvent::MouseDown: {
        // Returning false as will make EventHandler unfocus the plug-in, which is appropriate when clicking scrollbars.
        // Ideally, we wouldn't change focus at all, but PluginView already did that for us.
        // When support for PDF forms is added, we'll need to actually focus the plug-in when clicking in a form.
        break;
    }
    case WebEvent::MouseUp:
        if (m_horizontalScrollbar)
            m_horizontalScrollbar->mouseUp();
        if (m_verticalScrollbar)
            m_verticalScrollbar->mouseUp();
        break;
    default:
        break;
    }
        
    return false;
}

bool BuiltInPDFView::handleWheelEvent(const WebWheelEvent& event)
{
    PlatformWheelEvent platformEvent = platform(event);
    return ScrollableArea::handleWheelEvent(platformEvent);
}

bool BuiltInPDFView::handleMouseEnterEvent(const WebMouseEvent&)
{
    scrollAnimator()->mouseEnteredContentArea();
    return false;
}

bool BuiltInPDFView::handleMouseLeaveEvent(const WebMouseEvent&)
{
    scrollAnimator()->mouseExitedContentArea();
    return false;
}

bool BuiltInPDFView::handleKeyboardEvent(const WebKeyboardEvent&)
{
    return false;
}

void BuiltInPDFView::setFocus(bool hasFocus)
{
}

NPObject* BuiltInPDFView::pluginScriptableNPObject()
{
    return 0;
}

#if PLATFORM(MAC)

void BuiltInPDFView::windowFocusChanged(bool)
{
}

void BuiltInPDFView::windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates)
{
}

void BuiltInPDFView::windowVisibilityChanged(bool)
{
}

uint64_t BuiltInPDFView::pluginComplexTextInputIdentifier() const
{
    return 0;
}

void BuiltInPDFView::sendComplexTextInput(const String&)
{
}

#endif

void BuiltInPDFView::privateBrowsingStateChanged(bool)
{
}

bool BuiltInPDFView::getFormValue(String&)
{
    return false;
}

bool BuiltInPDFView::handleScroll(ScrollDirection direction, ScrollGranularity granularity)
{
    return scroll(direction, granularity);
}

Scrollbar* BuiltInPDFView::horizontalScrollbar()
{
    return m_horizontalScrollbar.get();
}

Scrollbar* BuiltInPDFView::verticalScrollbar()
{
    return m_verticalScrollbar.get();
}

IntRect BuiltInPDFView::scrollCornerRect() const
{
    if (!m_horizontalScrollbar || !m_verticalScrollbar)
        return IntRect();
    if (m_horizontalScrollbar->isOverlayScrollbar()) {
        ASSERT(m_verticalScrollbar->isOverlayScrollbar());
        return IntRect();
    }
    return IntRect(pluginView()->width() - m_verticalScrollbar->width(), pluginView()->height() - m_horizontalScrollbar->height(), m_verticalScrollbar->width(), m_horizontalScrollbar->height());
}

ScrollableArea* BuiltInPDFView::enclosingScrollableArea() const
{
    // FIXME: Walk up the frame tree and look for a scrollable parent frame or RenderLayer.
    return 0;
}

void BuiltInPDFView::setScrollOffset(const IntPoint& offset)
{
    m_scrollOffset = IntSize(offset.x(), offset.y());
    // FIXME: It would be better for performance to blit parts that remain visible.
    controller()->invalidate(IntRect(0, 0, m_frameRect.width(), m_frameRect.height()));
}

int BuiltInPDFView::scrollSize(ScrollbarOrientation orientation) const
{
    Scrollbar* scrollbar = ((orientation == HorizontalScrollbar) ? m_horizontalScrollbar : m_verticalScrollbar).get();
    return scrollbar ? (scrollbar->totalSize() - scrollbar->visibleSize()) : 0;
}

bool BuiltInPDFView::isActive() const
{
    return m_page->focusController()->isActive();
}

void BuiltInPDFView::invalidateScrollbarRect(Scrollbar* scrollbar, const LayoutRect& rect)
{
    LayoutRect dirtyRect = rect;
    dirtyRect.moveBy(scrollbar->location());
    dirtyRect.moveBy(-pluginView()->location());
    controller()->invalidate(dirtyRect);
}

void BuiltInPDFView::invalidateScrollCornerRect(const IntRect& rect)
{
    controller()->invalidate(rect);
}

bool BuiltInPDFView::isScrollCornerVisible() const
{
    return false;
}

int BuiltInPDFView::scrollPosition(Scrollbar* scrollbar) const
{
    if (scrollbar->orientation() == HorizontalScrollbar)
        return m_scrollOffset.width();
    if (scrollbar->orientation() == VerticalScrollbar)
        return m_scrollOffset.height();
    ASSERT_NOT_REACHED();
    return 0;
}

IntPoint BuiltInPDFView::scrollPosition() const
{
    return IntPoint(m_scrollOffset.width(), m_scrollOffset.height());
}

IntPoint BuiltInPDFView::minimumScrollPosition() const
{
    return IntPoint(0, 0);
}

IntPoint BuiltInPDFView::maximumScrollPosition() const
{
    int horizontalScrollbarHeight = (m_horizontalScrollbar && !m_horizontalScrollbar->isOverlayScrollbar()) ? m_horizontalScrollbar->height() : 0;
    int verticalScrollbarWidth = (m_verticalScrollbar && !m_verticalScrollbar->isOverlayScrollbar()) ? m_verticalScrollbar->width() : 0;

    IntPoint maximumOffset(m_pdfDocumentSize.width() - m_frameRect.width() + verticalScrollbarWidth, m_pdfDocumentSize.height() - m_frameRect.height() + horizontalScrollbarHeight);
    maximumOffset.clampNegativeToZero();
    return maximumOffset;
}

LayoutUnit BuiltInPDFView::visibleHeight() const
{
    return m_frameRect.height();
}

LayoutUnit BuiltInPDFView::visibleWidth() const
{
    return m_frameRect.width();
}

IntSize BuiltInPDFView::contentsSize() const
{
    return m_pdfDocumentSize;
}

bool BuiltInPDFView::isOnActivePage() const
{
    return pluginView()->frame()->document()->inPageCache();
}

} // namespace WebKit
