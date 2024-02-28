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

#import "Logging.h"
#import "MessageSenderInlines.h"
#import "PDFIncrementalLoader.h"
#import "PDFKitSPI.h"
#import "PluginView.h"
#import "WKAccessibilityPDFDocumentObject.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebLoaderStrategy.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardProxyMessages.h"
#import "WebProcess.h"
#import <CoreFoundation/CoreFoundation.h>
#import <PDFKit/PDFKit.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/ArchiveResource.h>
#import <WebCore/Chrome.h>
#import <WebCore/Cursor.h>
#import <WebCore/Document.h>
#import <WebCore/EventNames.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/ScrollAnimator.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/TextStream.h>

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
#if HAVE(INCREMENTAL_PDF_APIS)
    , m_incrementalPDFLoadingEnabled(element.document().settings().incrementalPDFLoadingEnabled())
#endif
{
#if PLATFORM(MAC)
    m_accessibilityDocumentObject = adoptNS([[WKAccessibilityPDFDocumentObject alloc] initWithPDFDocument:m_pdfDocument andElement:&element]);
    [m_accessibilityDocumentObject setPDFPlugin:this];
    if (this->isFullFramePlugin() && m_frame && m_frame->page() && m_frame->isMainFrame())
        [m_accessibilityDocumentObject setParent:dynamic_objc_cast<NSObject>(m_frame->protectedPage()->accessibilityRemoteObject())];
#endif
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
    m_data = nil;

#if HAVE(INCREMENTAL_PDF_APIS)
    if (m_incrementalLoader) {
        m_incrementalLoader->clear();
        m_incrementalLoader = nullptr;
    }
#endif

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

void PDFPluginBase::startLoading()
{
#if HAVE(INCREMENTAL_PDF_APIS)
    if (incrementalPDFLoadingEnabled())
        m_incrementalLoader = PDFIncrementalLoader::create(*this);
#endif
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
    m_pdfDocument = adoptNS([allocPDFDocumentInstance() initWithData:originalData()]);
}

bool PDFPluginBase::isFullFramePlugin() const
{
    // <object> or <embed> plugins will appear to be in their parent frame, so we have to
    // check whether our frame's widget is exactly our PluginView.
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;

    RefPtr document = dynamicDowncast<PluginDocument>(m_frame->coreLocalFrame()->document());
    return document && document->pluginWidget() == m_view;
}

bool PDFPluginBase::handlesPageScaleFactor() const
{
    return m_frame && m_frame->isMainFrame() && isFullFramePlugin();
}

bool PDFPluginBase::isLocked() const
{
    return [m_pdfDocument isLocked];
}

void PDFPluginBase::notifySelectionChanged()
{
    if (!m_frame || !m_frame->page())
        return;
    m_frame->protectedPage()->didChangeSelection(*m_frame->protectedCoreLocalFrame());
}

NSData *PDFPluginBase::originalData() const
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

bool PDFPluginBase::haveStreamedDataForRange(uint64_t offset, size_t count) const
{
    if (!m_data)
        return false;

    return streamedBytes() >= offset + count;
}

bool PDFPluginBase::haveDataForRange(uint64_t offset, size_t count) const
{
    if (!m_data)
        return false;

    uint64_t dataLength = CFDataGetLength(m_data.get());
    return offset + count <= dataLength;
}

size_t PDFPluginBase::copyDataAtPosition(void* buffer, uint64_t sourcePosition, size_t count) const
{
    ASSERT(isMainRunLoop());

    if (!documentFinishedLoading()) {
        // FIXME: if documentFinishedLoading() is false, we may not be able to fulfill this request, but that should only happen if we trigger painting on the main thread.
        LOG_WITH_STREAM(IncrementalPDF, stream << "PDFIncrementalLoader::copyDataAtPosition " << sourcePosition << " on main thread - document has not finished loading");
    }

    if (!haveStreamedDataForRange(sourcePosition, count))
        return 0;

    memcpy(buffer, CFDataGetBytePtr(m_data.get()) + sourcePosition, count);
    return count;
}

const uint8_t* PDFPluginBase::dataPtrForRange(uint64_t sourcePosition, size_t count) const
{
    ASSERT(isMainRunLoop());

    if (!haveDataForRange(sourcePosition, count))
        return nullptr;

    return CFDataGetBytePtr(m_data.get()) + sourcePosition;
}

void PDFPluginBase::insertRangeRequestData(uint64_t offset, const Vector<uint8_t>& requestData)
{
    auto requiredLength = offset + requestData.size();
    ensureDataBufferLength(requiredLength);

    memcpy(CFDataGetMutableBytePtr(m_data.get()) + offset, requestData.data(), requestData.size());
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

    LOG_WITH_STREAM(IncrementalPDF, stream << "PDFPluginBase::streamDidReceiveData() - received " << buffer.size() << " bytes, total streamed bytes " << m_streamedBytes);

#if HAVE(INCREMENTAL_PDF_APIS)
    if (m_incrementalLoader)
        m_incrementalLoader->incrementalPDFStreamDidReceiveData(buffer);
#endif
}

void PDFPluginBase::streamDidFinishLoading()
{
    if (m_hasBeenDestroyed)
        return;

    LOG_WITH_STREAM(IncrementalPDF, stream << "PDFPluginBase::streamDidFinishLoading()");

    addArchiveResource();
    m_documentFinishedLoading = true;

    auto incrementalPDFStreamDidFinishLoading = [&]() {
#if HAVE(INCREMENTAL_PDF_APIS)
        if (!m_incrementalLoader)
            return false;

        m_incrementalLoader->incrementalPDFStreamDidFinishLoading();
        return m_incrementalPDFLoadingEnabled.load();
#else
        return false;
#endif
    };

    if (!incrementalPDFStreamDidFinishLoading()) {
        createPDFDocument();
        installPDFDocument();
    }

    tryRunScriptsInPDFDocument();
}

void PDFPluginBase::streamDidFail()
{
    m_data = nullptr;
#if HAVE(INCREMENTAL_PDF_APIS)
    if (m_incrementalLoader)
        m_incrementalLoader->incrementalPDFStreamDidFail();
#endif
}

#if HAVE(INCREMENTAL_PDF_APIS)

void PDFPluginBase::adoptBackgroundThreadDocument(RetainPtr<PDFDocument>&& backgroundThreadDocument)
{
    if (m_hasBeenDestroyed)
        return;

    ASSERT(!m_pdfDocument);
    ASSERT(isMainRunLoop());

#if !LOG_DISABLED
    incrementalLoaderLog("Adopting PDFDocument from background thread"_s);
#endif

    m_pdfDocument = WTFMove(backgroundThreadDocument);
    // FIXME: Can we throw away the m_incrementalLoader?

    // If the plugin is being destroyed, no point in doing any more PDF work
    if (m_isBeingDestroyed)
        return;

    installPDFDocument();
}

void PDFPluginBase::maybeClearHighLatencyDataProviderFlag()
{
    if (!m_pdfDocument || !m_documentFinishedLoading)
        return;

    if ([m_pdfDocument respondsToSelector:@selector(setHasHighLatencyDataProvider:)])
        [m_pdfDocument setHasHighLatencyDataProvider:NO];
}

void PDFPluginBase::startByteRangeRequest(NetscapePlugInStreamLoaderClient& streamLoaderClient, uint64_t requestIdentifier, uint64_t position, size_t count)
{
    if (!m_incrementalLoader)
        return;

    if (!m_frame)
        return;

    RefPtr coreFrame = m_frame->coreLocalFrame();
    if (!coreFrame)
        return;

    RefPtr documentLoader = coreFrame->loader().documentLoader();
    if (!documentLoader)
        return;

    auto resourceRequest = documentLoader->request();
    resourceRequest.setRequester(ResourceRequestRequester::Unspecified);
    resourceRequest.setURL(m_view->mainResourceURL());
    resourceRequest.setHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, position, "-"_s, position + count - 1));
    resourceRequest.setCachePolicy(ResourceRequestCachePolicy::DoNotUseAnyCache);

    WebProcess::singleton().webLoaderStrategy().schedulePluginStreamLoad(*coreFrame, streamLoaderClient, WTFMove(resourceRequest), [incrementalLoader = Ref { *m_incrementalLoader }, requestIdentifier] (RefPtr<NetscapePlugInStreamLoader>&& streamLoader) {
        incrementalLoader->streamLoaderDidStart(requestIdentifier, WTFMove(streamLoader));
    });
}

void PDFPluginBase::receivedNonLinearizedPDFSentinel()
{
    m_incrementalPDFLoadingEnabled = false;

    if (m_hasBeenDestroyed)
        return;

    if (!isMainRunLoop()) {
#if !LOG_DISABLED
        incrementalLoaderLog("Disabling incremental PDF loading on background thread"_s);
#endif
        callOnMainRunLoop([this, protectedThis = Ref { *this }] {
            receivedNonLinearizedPDFSentinel();
        });
        return;
    }

    if (m_incrementalLoader)
        m_incrementalLoader->receivedNonLinearizedPDFSentinel();

    if (!m_documentFinishedLoading || m_pdfDocument)
        return;

    createPDFDocument();
    installPDFDocument();
    tryRunScriptsInPDFDocument();
}

#endif // HAVE(INCREMENTAL_PDF_APIS)

void PDFPluginBase::performWebSearch(const String& query)
{
    if (!m_frame || !m_frame->page())
        return;

    if (!query)
        return;

    m_frame->protectedPage()->send(Messages::WebPageProxy::SearchTheWeb(query));
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

void PDFPluginBase::tryRunScriptsInPDFDocument()
{
    if (!m_pdfDocument || !m_documentFinishedLoading || m_didRunScripts)
        return;

    PDFScriptEvaluator::runScripts([m_pdfDocument documentRef], *this);
    m_didRunScripts = true;
}

bool PDFPluginBase::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    auto oldSize = m_size;
    auto oldRootViewToPluginTransform = m_rootViewToPluginTransform;

    m_size = pluginSize;
    m_rootViewToPluginTransform = valueOrDefault(pluginToRootViewTransform.inverse());

    if (m_size == oldSize && m_rootViewToPluginTransform == oldRootViewToPluginTransform)
        return false;

    LOG_WITH_STREAM(PDF, stream << "PDFPluginBase::geometryDidChange - size " << pluginSize << " pluginToRootViewTransform " << pluginToRootViewTransform);

#if ENABLE(PDF_HUD)
    updateHUDLocation();
#endif

    return true;
}

#if ENABLE(PDF_HUD)
bool PDFPluginBase::shouldShowHUD() const
{
    if (!hudEnabled())
        return false;

    if (!m_view->isVisible())
        return false;

    if (isLocked())
        return false;

    // FIXME: Don't show HUD if it won't fit.

    return true;
}

void PDFPluginBase::updateHUDVisibility()
{
    if (!m_frame)
        return;

    if (shouldShowHUD())
        m_frame->page()->createPDFHUD(*this, frameForHUDInRootViewCoordinates());
    else
        m_frame->page()->removePDFHUD(*this);
}
#endif

void PDFPluginBase::visibilityDidChange(bool)
{
#if ENABLE(PDF_HUD)
    updateHUDVisibility();
#endif
}

FloatSize PDFPluginBase::pdfDocumentSizeForPrinting() const
{
    return FloatSize { [[m_pdfDocument pageAtIndex:0] boundsForBox:kPDFDisplayBoxCropBox].size };
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

void PDFPluginBase::print()
{
    if (RefPtr page = this->page())
        page->chrome().print(*m_frame->coreLocalFrame());
}

#if PLATFORM(MAC)

void PDFPluginBase::writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types) const
{
    // FIXME: <https://webkit.org/b/269174> PDFPluginBase::writeItemsToPasteboard should be platform-agnostic.
    auto pasteboardTypes = makeVector<String>(types);
    auto pageIdentifier = m_frame && m_frame->coreLocalFrame() ? m_frame->coreLocalFrame()->pageID() : std::nullopt;

    auto& webProcess = WebProcess::singleton();
    webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier), 0);

    ASSERT(items.count >= types.count);
    for (NSUInteger i = 0, count = items.count; i < count; ++i) {
        NSString *type = [types objectAtIndex:i];
        NSData *data = [items objectAtIndex:i];

        // We don't expect the data for any items to be empty, but aren't completely sure.
        // Avoid crashing in the SharedMemory constructor in release builds if we're wrong.
        ASSERT(data.length);
        if (!data.length)
            continue;

        if ([type isEqualToString:legacyStringPasteboardType()] || [type isEqualToString:NSPasteboardTypeString]) {
            auto plainTextString = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
            webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, type, plainTextString.get(), pageIdentifier), 0);
        } else {
            auto buffer = SharedBuffer::create(data);
            webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, type, WTFMove(buffer), pageIdentifier), 0);
        }
    }
}

#endif

#if ENABLE(PDF_HUD)

void PDFPluginBase::updateHUDLocation()
{
    if (!shouldShowHUD())
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

static std::span<const uint8_t> span(NSData *data)
{
    return { static_cast<const uint8_t*>(data.bytes), data.length };
}

void PDFPluginBase::save(CompletionHandler<void(const String&, const URL&, std::span<const uint8_t>)>&& completionHandler)
{
    NSData *data = liveData();
    URL url;
    if (m_frame)
        url = m_frame->url();
    completionHandler(m_suggestedFilename, url, span(data));
}

void PDFPluginBase::openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, std::span<const uint8_t>, const String&)>&& completionHandler)
{
    NSData *data = liveData();
    FrameInfoData frameInfo;
    if (m_frame)
        frameInfo = m_frame->info();
    completionHandler(m_suggestedFilename, WTFMove(frameInfo), span(data), createVersion4UUIDString());
}

#endif // ENABLE(PDF_HUD)

void PDFPluginBase::notifyCursorChanged(WebCore::PlatformCursorType cursorType)
{
    if (!m_frame || !m_frame->page())
        return;

    m_frame->protectedPage()->send(Messages::WebPageProxy::SetCursor(WebCore::Cursor::fromType(cursorType)));
}

bool PDFPluginBase::supportsForms()
{
    // FIXME: We support forms for full-main-frame and <iframe> PDFs, but not <embed> or <object>, because those cases do not have their own Document into which to inject form elements.
    return isFullFramePlugin();
}

bool PDFPluginBase::showContextMenuAtPoint(const IntPoint& point)
{
    auto* frameView = m_frame ? m_frame->coreLocalFrame()->view() : nullptr;
    if (!frameView)
        return false;
    IntPoint contentsPoint = frameView->contentsToRootView(point);
    WebMouseEvent event({ WebEventType::MouseDown, OptionSet<WebEventModifier> { }, WallTime::now() }, WebMouseEventButton::Right, 0, contentsPoint, contentsPoint, 0, 0, 0, 1, WebCore::ForceAtClick);
    return handleContextMenuEvent(event);
}

IntPoint PDFPluginBase::convertFromPDFViewToRootView(const IntPoint& point) const
{
    // FIXME fix the coordinate space
    IntPoint pointInPluginCoordinates(point.x(), size().height() - point.y());
    return valueOrDefault(m_rootViewToPluginTransform.inverse()).mapPoint(pointInPluginCoordinates);
}

FloatRect PDFPluginBase::convertFromPDFViewToScreenForAccessibility(const FloatRect& rect) const
{
    return WebCore::Accessibility::retrieveValueFromMainThread<WebCore::FloatRect>([&] () -> WebCore::FloatRect {
        FloatRect updatedRect = rect;
        updatedRect.setLocation(convertFromPDFViewToRootView(IntPoint(updatedRect.location())));
        RefPtr page = this->page();
        if (!page)
            return { };
        return page->chrome().rootViewToScreen(enclosingIntRect(updatedRect));
    });
}

WebCore::AXObjectCache* PDFPluginBase::axObjectCache() const
{
    ASSERT(isMainRunLoop());
    if (!m_frame || !m_frame->coreLocalFrame() || !m_frame->coreLocalFrame()->document())
        return nullptr;
    return m_frame->coreLocalFrame()->document()->axObjectCache();
}

IntPoint PDFPluginBase::convertFromRootViewToPDFView(const IntPoint& point) const
{
    ASSERT(isMainRunLoop());
    IntPoint pointInPluginCoordinates = m_rootViewToPluginTransform.mapPoint(point);
    return IntPoint(pointInPluginCoordinates.x(), size().height() - pointInPluginCoordinates.y());
}

WebCore::IntPoint PDFPluginBase::lastKnownMousePositionInView() const
{
    if (m_lastMouseEvent)
        return convertFromRootViewToPlugin(m_lastMouseEvent->position());
    return { };
}


void PDFPluginBase::navigateToURL(const URL& url)
{
    if (url.protocolIsJavaScript())
        return;

    RefPtr frame = m_frame ? m_frame->coreLocalFrame() : nullptr;
    if (!frame)
        return;

    RefPtr<Event> coreEvent;
    if (m_lastMouseEvent)
        coreEvent = MouseEvent::create(eventNames().clickEvent, &frame->windowProxy(), platform(*m_lastMouseEvent), 0, 0);

    frame->loader().changeLocation(url, emptyAtom(), coreEvent.get(), ReferrerPolicy::NoReferrer, ShouldOpenExternalURLsPolicy::ShouldAllow);
}

id PDFPluginBase::accessibilityAssociatedPluginParentForElement(Element* element) const
{
    ASSERT(isMainRunLoop());

#if PLATFORM(MAC)
    if (!m_activeAnnotation)
        return nil;

    if (m_activeAnnotation->element() != element)
        return nil;

    return [m_activeAnnotation->annotation() accessibilityNode];
#endif

    return nil;
}

#if !LOG_DISABLED

#if HAVE(INCREMENTAL_PDF_APIS)
static void verboseLog(PDFIncrementalLoader* incrementalLoader, uint64_t streamedBytes, bool documentFinishedLoading)
{
    ASSERT(isMainRunLoop());

    TextStream stream;
    stream << "\n";


    if (incrementalLoader)
        incrementalLoader->logState(stream);

    stream << "The main document loader has finished loading " << streamedBytes << " bytes, and is";
    if (!documentFinishedLoading)
        stream << " not";
    stream << " complete";

    LOG(IncrementalPDFVerbose, "%s", stream.release().utf8().data());
}
#endif

void PDFPluginBase::incrementalLoaderLog(const String& message)
{
#if HAVE(INCREMENTAL_PDF_APIS)
    if (!isMainRunLoop()) {
        callOnMainRunLoop([this, protectedThis = Ref { *this }, message = message.isolatedCopy()] {
            incrementalLoaderLog(message);
        });
        return;
    }

    LOG_WITH_STREAM(IncrementalPDF, stream << message);
    verboseLog(m_incrementalLoader.get(), m_streamedBytes, m_documentFinishedLoading);
    LOG_WITH_STREAM(IncrementalPDFVerbose, stream << message);
#else
    UNUSED_PARAM(message);
#endif
}

#endif // !LOG_DISABLED

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
