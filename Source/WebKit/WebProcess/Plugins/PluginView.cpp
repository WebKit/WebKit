/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "PluginView.h"

#if ENABLE(PDFKIT_PLUGIN)

#include "PDFPlugin.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebKeyboardEvent.h"
#include "WebLoaderStrategy.h"
#include "WebMouseEvent.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebWheelEvent.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/Chrome.h>
#include <WebCore/CookieJar.h>
#include <WebCore/Credential.h>
#include <WebCore/CredentialStorage.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/EventHandler.h>
#include <WebCore/EventNames.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/HostWindow.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PageInlines.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/ScriptController.h>
#include <WebCore/ScrollView.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/Settings.h>
#include <WebCore/UserGestureIndicator.h>
#include <pal/text/TextEncoding.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace JSC;
using namespace WebCore;

class PluginView::Stream : public RefCounted<PluginView::Stream>, NetscapePlugInStreamLoaderClient {
public:
    static Ref<Stream> create(PluginView* pluginView, const ResourceRequest& request)
    {
        return adoptRef(*new Stream(pluginView, request));
    }
    ~Stream();

    void start();
    void cancel();
    void continueLoad();

private:
    Stream(PluginView* pluginView, const ResourceRequest& request)
        : m_pluginView(pluginView)
        , m_request(request)
        , m_streamWasCancelled(false)
    {
    }

    // NetscapePluginStreamLoaderClient
    void willSendRequest(NetscapePlugInStreamLoader*, ResourceRequest&&, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&&) override;
    void didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse&) override;
    void didReceiveData(NetscapePlugInStreamLoader*, const SharedBuffer&) override;
    void didFail(NetscapePlugInStreamLoader*, const ResourceError&) override;
    void didFinishLoading(NetscapePlugInStreamLoader*) override;

    PluginView* m_pluginView;
    ResourceRequest m_request;
    CompletionHandler<void(ResourceRequest&&)> m_loadCallback;

    // True if the stream was explicitly cancelled by calling cancel().
    // (As opposed to being cancelled by the user hitting the stop button for example.
    bool m_streamWasCancelled;
    
    RefPtr<NetscapePlugInStreamLoader> m_loader;
};

PluginView::Stream::~Stream()
{
    if (m_loadCallback)
        m_loadCallback({ });
    ASSERT(!m_pluginView);
}
    
void PluginView::Stream::start()
{
    ASSERT(!m_loader);

    Frame* frame = m_pluginView->frame();
    ASSERT(frame);

    WebProcess::singleton().webLoaderStrategy().schedulePluginStreamLoad(*frame, *this, ResourceRequest {m_request}, [this, protectedThis = Ref { *this }](RefPtr<NetscapePlugInStreamLoader>&& loader) {
        m_loader = WTFMove(loader);
    });
}

void PluginView::Stream::cancel()
{
    ASSERT(m_loader);

    m_streamWasCancelled = true;

    auto loader = std::exchange(m_loader, nullptr);
    loader->cancel(loader->cancelledError());
}

void PluginView::Stream::continueLoad()
{
    ASSERT(m_loadCallback);
    m_loadCallback(ResourceRequest(m_request));
}

void PluginView::Stream::willSendRequest(NetscapePlugInStreamLoader*, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& decisionHandler)
{
    m_loadCallback = WTFMove(decisionHandler);
    m_request = WTFMove(request);
}

void PluginView::Stream::didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse& response)
{
    m_pluginView->m_plugin->streamDidReceiveResponse(response);
}

void PluginView::Stream::didReceiveData(NetscapePlugInStreamLoader*, const SharedBuffer& buffer)
{
    m_pluginView->m_plugin->streamDidReceiveData(buffer);
}

void PluginView::Stream::didFail(NetscapePlugInStreamLoader*, const ResourceError&)
{
    // Calling streamDidFail could cause us to be deleted, so we hold on to a reference here.
    Ref<Stream> protect(*this);

    // We only want to call streamDidFail if the stream was not explicitly cancelled by the plug-in.
    if (!m_streamWasCancelled)
        m_pluginView->m_plugin->streamDidFail();

    ASSERT(m_pluginView->m_stream == this);
    m_pluginView->m_stream = nullptr;
    m_pluginView = nullptr;
}

void PluginView::Stream::didFinishLoading(NetscapePlugInStreamLoader*)
{
    // Calling streamDidFinishLoading could cause us to be deleted, so we hold on to a reference here.
    Ref<Stream> protect(*this);

    m_pluginView->m_plugin->streamDidFinishLoading();

    ASSERT(m_pluginView->m_stream == this);
    m_pluginView->m_stream = nullptr;
    m_pluginView = nullptr;
}

RefPtr<PluginView> PluginView::create(HTMLPlugInElement& element, const URL& mainResourceURL, const String& contentType, bool shouldUseManualLoader)
{
    auto* coreFrame = element.document().frame();
    if (!coreFrame)
        return nullptr;

    auto* frame = WebFrame::fromCoreFrame(*coreFrame);
    if (!frame)
        return nullptr;

    auto* page = frame->page();
    if (!page || !page->shouldUsePDFPlugin(contentType, mainResourceURL.path()))
        return nullptr;

    return adoptRef(*new PluginView(element, mainResourceURL, contentType, shouldUseManualLoader, *page));
}

PluginView::PluginView(HTMLPlugInElement& element, const URL& mainResourceURL, const String&, bool shouldUseManualLoader, WebPage& page)
    : m_pluginElement(element)
    , m_plugin(PDFPlugin::create(element))
    , m_webPage(page)
    , m_mainResourceURL(mainResourceURL)
    , m_shouldUseManualLoader(shouldUseManualLoader)
    , m_pendingResourceRequestTimer(RunLoop::main(), this, &PluginView::pendingResourceRequestTimerFired)
{
    m_webPage->addPluginView(this);
}

PluginView::~PluginView()
{
    if (m_webPage)
        m_webPage->removePluginView(this);
    if (m_stream)
        m_stream->cancel();
    m_plugin->destroy();
}

Frame* PluginView::frame() const
{
    return m_pluginElement->document().frame();
}

void PluginView::manualLoadDidReceiveResponse(const ResourceResponse& response)
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::Initial);
        m_manualStreamState = ManualStreamState::HasReceivedResponse;
        m_manualStreamResponse = response;
        return;
    }

    m_plugin->streamDidReceiveResponse(response);
}

void PluginView::manualLoadDidReceiveData(const SharedBuffer& buffer)
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamData.append(buffer);
        return;
    }

    m_plugin->streamDidReceiveData(buffer);
}

void PluginView::manualLoadDidFinishLoading()
{
    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamState = ManualStreamState::Finished;
        return;
    }

    m_plugin->streamDidFinishLoading();
}

void PluginView::manualLoadDidFail()
{
    if (!m_isInitialized) {
        m_manualStreamState = ManualStreamState::Finished;
        m_manualStreamData.reset();
        return;
    }

    m_plugin->streamDidFail();
}

void PluginView::pageScaleFactorDidChange()
{
    viewGeometryDidChange();
}

void PluginView::topContentInsetDidChange()
{
    viewGeometryDidChange();
}

void PluginView::setPageScaleFactor(double scaleFactor)
{
    m_pageScaleFactor = scaleFactor;
    m_webPage->send(Messages::WebPageProxy::PluginScaleFactorDidChange(scaleFactor));
    m_webPage->send(Messages::WebPageProxy::PluginZoomFactorDidChange(scaleFactor));
    pageScaleFactorDidChange();
}

double PluginView::pageScaleFactor() const
{
    return m_pageScaleFactor;
}

void PluginView::webPageDestroyed()
{
    m_webPage = 0;
}

#if PLATFORM(COCOA)

void PluginView::setDeviceScaleFactor(float scaleFactor)
{
    if (!m_isInitialized)
        return;

    m_plugin->contentsScaleFactorChanged(scaleFactor);
}

id PluginView::accessibilityAssociatedPluginParentForElement(Element* element) const
{
    return m_plugin->accessibilityAssociatedPluginParentForElement(element);
}
    
id PluginView::accessibilityObject() const
{
    if (!m_isInitialized)
        return nil;
    
    return m_plugin->accessibilityObject();
}

#endif

void PluginView::initializePlugin()
{
    if (m_isInitialized)
        return;

    m_plugin->setView(*this);

    if (!m_shouldUseManualLoader && !m_mainResourceURL.isEmpty())
        loadMainResource();

    m_isInitialized = true;

    viewVisibilityDidChange();
    viewGeometryDidChange();

    redeliverManualStream();

#if PLATFORM(COCOA)
    if (m_plugin->pluginLayer() && frame()) {
        frame()->view()->enterCompositingMode();
        m_pluginElement->invalidateStyleAndLayerComposition();
    }
    m_plugin->visibilityDidChange(isVisible());
#endif

    if (Frame* frame = this->frame()) {
        if (FrameView* frameView = frame->view())
            frameView->setNeedsLayoutAfterViewConfigurationChange();
        if (frame->isMainFrame() && m_plugin->isFullFramePlugin())
            WebFrame::fromCoreFrame(*frame)->page()->send(Messages::WebPageProxy::MainFramePluginHandlesPageScaleGestureDidChange(true));
    }
}

#if PLATFORM(COCOA)

PlatformLayer* PluginView::platformLayer() const
{
    if (!m_isInitialized)
        return nil;

    return m_plugin->pluginLayer();
}

#endif

bool PluginView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    return m_isInitialized && m_plugin->scroll(direction, granularity);
}

Scrollbar* PluginView::horizontalScrollbar()
{
    if (!m_isInitialized)
        return nullptr;

    return m_plugin->horizontalScrollbar();
}

Scrollbar* PluginView::verticalScrollbar()
{
    if (!m_isInitialized)
        return nullptr;

    return m_plugin->verticalScrollbar();
}

bool PluginView::wantsWheelEvents()
{
    return true;
}

void PluginView::setFrameRect(const WebCore::IntRect& rect)
{
    Widget::setFrameRect(rect);
    viewGeometryDidChange();
}

void PluginView::paint(GraphicsContext& context, const IntRect& /*dirtyRect*/, Widget::SecurityOriginPaintPolicy, EventRegionContext*)
{
    if (!m_isInitialized)
        return;

    if (context.paintingDisabled()) {
        if (context.invalidatingControlTints())
            m_plugin->updateControlTints(context);
        return;
    }

    // FIXME: We should try to intersect the dirty rect with the plug-in's clip rect here.
    IntRect paintRect = IntRect(IntPoint(), frameRect().size());

    if (paintRect.isEmpty())
        return;

    if (m_transientPaintingSnapshot)
        m_transientPaintingSnapshot->paint(context, m_plugin->deviceScaleFactor(), frameRect().location(), m_transientPaintingSnapshot->bounds());
}

void PluginView::frameRectsChanged()
{
    Widget::frameRectsChanged();
    viewGeometryDidChange();
}

void PluginView::clipRectChanged()
{
    viewGeometryDidChange();
}

void PluginView::setParent(ScrollView* scrollView)
{
    Widget::setParent(scrollView);
    
    if (scrollView)
        initializePlugin();
}

unsigned PluginView::countFindMatches(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (!m_isInitialized)
        return 0;

    return m_plugin->countFindMatches(target, options, maxMatchCount);
}

bool PluginView::findString(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->findString(target, options, maxMatchCount);
}

String PluginView::getSelectionString() const
{
    if (!m_isInitialized)
        return String();

    return m_plugin->getSelectionString();
}

void PluginView::handleEvent(Event& event)
{
    if (!m_isInitialized)
        return;

    const WebEvent* currentEvent = WebPage::currentEvent();
    if (!currentEvent)
        return;

    bool didHandleEvent = false;

    if ((event.type() == eventNames().mousemoveEvent && currentEvent->type() == WebEvent::MouseMove)
        || (event.type() == eventNames().mousedownEvent && currentEvent->type() == WebEvent::MouseDown)
        || (event.type() == eventNames().mouseupEvent && currentEvent->type() == WebEvent::MouseUp)) {
        // FIXME: Clicking in a scroll bar should not change focus.
        if (currentEvent->type() == WebEvent::MouseDown) {
            focusPluginElement();
            frame()->eventHandler().setCapturingMouseEventsElement(m_pluginElement.ptr());
        } else if (currentEvent->type() == WebEvent::MouseUp)
            frame()->eventHandler().setCapturingMouseEventsElement(nullptr);

        didHandleEvent = m_plugin->handleMouseEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    } else if (eventNames().isWheelEventType(event.type()) && currentEvent->type() == WebEvent::Wheel)
        didHandleEvent = m_plugin->handleWheelEvent(static_cast<const WebWheelEvent&>(*currentEvent));
    else if (event.type() == eventNames().mouseoverEvent && currentEvent->type() == WebEvent::MouseMove)
        didHandleEvent = m_plugin->handleMouseEnterEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if (event.type() == eventNames().mouseoutEvent && currentEvent->type() == WebEvent::MouseMove)
        didHandleEvent = m_plugin->handleMouseLeaveEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if (event.type() == eventNames().contextmenuEvent && currentEvent->type() == WebEvent::MouseDown)
        didHandleEvent = m_plugin->handleContextMenuEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if ((event.type() == eventNames().keydownEvent && currentEvent->type() == WebEvent::KeyDown)
        || (event.type() == eventNames().keyupEvent && currentEvent->type() == WebEvent::KeyUp))
        didHandleEvent = m_plugin->handleKeyboardEvent(static_cast<const WebKeyboardEvent&>(*currentEvent));

    if (didHandleEvent)
        event.setDefaultHandled();
}
    
bool PluginView::handleEditingCommand(const String& commandName, const String&)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->handleEditingCommand(commandName);
}
    
bool PluginView::isEditingCommandEnabled(const String& commandName)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->isEditingCommandEnabled(commandName);
}

bool PluginView::shouldAllowNavigationFromDrags() const
{
    return true;
}

void PluginView::willDetachRenderer()
{
    if (!m_isInitialized)
        return;

    m_plugin->willDetachRenderer();
}

RefPtr<FragmentedSharedBuffer> PluginView::liveResourceData() const
{
    if (!m_isInitialized) {
        if (m_manualStreamState == ManualStreamState::Finished)
            return m_manualStreamData.get();

        return nullptr;
    }

    return m_plugin->liveResourceData();
}

bool PluginView::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!m_isInitialized)
        return false;

    return m_plugin->performDictionaryLookupAtLocation(point);
}

bool PluginView::existingSelectionContainsPoint(const WebCore::FloatPoint& point) const
{
    if (!m_isInitialized)
        return false;
    
    return m_plugin->existingSelectionContainsPoint(point);
}

void PluginView::notifyWidget(WidgetNotification notification)
{
    switch (notification) {
    case WillPaintFlattened:
        if (shouldCreateTransientPaintingSnapshot())
            m_transientPaintingSnapshot = m_plugin->snapshot();
        break;
    case DidPaintFlattened:
        m_transientPaintingSnapshot = nullptr;
        break;
    }
}

void PluginView::show()
{
    bool wasVisible = isVisible();

    setSelfVisible(true);

    if (!wasVisible)
        viewVisibilityDidChange();

    Widget::show();
}

void PluginView::hide()
{
    bool wasVisible = isVisible();

    setSelfVisible(false);

    if (wasVisible)
        viewVisibilityDidChange();

    Widget::hide();
}

void PluginView::setParentVisible(bool isVisible)
{
    if (isParentVisible() == isVisible)
        return;

    Widget::setParentVisible(isVisible);
    viewVisibilityDidChange();
}

bool PluginView::transformsAffectFrameRect()
{
    return false;
}

void PluginView::viewGeometryDidChange()
{
    if (!m_isInitialized || !parent())
        return;

    ASSERT(frame());
    float pageScaleFactor = frame()->page() ? frame()->page()->pageScaleFactor() : 1;

    IntPoint scaledFrameRectLocation(frameRect().location().x() * pageScaleFactor, frameRect().location().y() * pageScaleFactor);
    IntPoint scaledLocationInRootViewCoordinates(parent()->contentsToRootView(scaledFrameRectLocation));

    // FIXME: We still don't get the right coordinates for transformed plugins.
    AffineTransform transform;
    transform.translate(scaledLocationInRootViewCoordinates.x(), scaledLocationInRootViewCoordinates.y());
    transform.scale(pageScaleFactor);

    m_plugin->geometryDidChange(size(), transform);
}

void PluginView::viewVisibilityDidChange()
{
    if (!m_isInitialized || !parent())
        return;

    m_plugin->visibilityDidChange(isVisible());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    Frame* frame = this->frame();

    // Get the window clip rect for the plugin element (in window coordinates).
    IntRect windowClipRect = frame->view()->windowClipRectForFrameOwner(m_pluginElement.ptr(), true);

    // Intersect the two rects to get the view clip rect in window coordinates.
    frameRectInWindowCoordinates.intersect(windowClipRect);

    return frameRectInWindowCoordinates;
}

void PluginView::focusPluginElement()
{
    ASSERT(frame());
    
    if (Page* page = frame()->page())
        CheckedRef(page->focusController())->setFocusedElement(m_pluginElement.ptr(), *frame());
    else
        RefPtr(frame()->document())->setFocusedElement(m_pluginElement.ptr());
}

void PluginView::pendingResourceRequestTimerFired()
{
    ASSERT(m_pendingResourceRequest);
    auto request = std::exchange(m_pendingResourceRequest, nullptr);

    if (request->url().protocolIsJavaScript())
        return;

    // This protector is needed to make sure the PluginView is not destroyed while it is still needed.
    Ref protectThis { *this };

    ASSERT(!m_stream);
    m_stream = PluginView::Stream::create(this, *request);
    m_stream->start();
}

void PluginView::redeliverManualStream()
{
    if (m_manualStreamState == ManualStreamState::Initial) {
        // Nothing to do.
        return;
    }

    if (m_manualStreamState == ManualStreamState::Failed) {
        manualLoadDidFail();
        return;
    }

    // Deliver the response.
    manualLoadDidReceiveResponse(m_manualStreamResponse);

    // Deliver the data.
    if (m_manualStreamData) {
        m_manualStreamData.take()->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
            manualLoadDidReceiveData(buffer);
        });
    }

    if (m_manualStreamState == ManualStreamState::Finished)
        manualLoadDidFinishLoading();
}

void PluginView::invalidateRect(const IntRect& dirtyRect)
{
    if (!parent() || !m_isInitialized)
        return;

#if PLATFORM(COCOA)
    if (m_plugin->pluginLayer())
        return;
#endif

    auto* renderer = m_pluginElement->renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return;
    auto& object = downcast<RenderEmbeddedObject>(*renderer);

    IntRect contentRect(dirtyRect);
    contentRect.move(object.borderLeft() + object.paddingLeft(), object.borderTop() + object.paddingTop());
    renderer->repaintRectangle(contentRect);
}

void PluginView::loadMainResource()
{
    auto referrer = SecurityPolicy::generateReferrerHeader(frame()->document()->referrerPolicy(), m_mainResourceURL, frame()->loader().outgoingReferrer());
    if (referrer.isEmpty())
        referrer = { };

    ASSERT(!m_pendingResourceRequest);
    m_pendingResourceRequest = WTF::makeUnique<ResourceRequest>(m_mainResourceURL, referrer);
    m_pendingResourceRequestTimer.startOneShot(0_s);
}

bool PluginView::shouldCreateTransientPaintingSnapshot() const
{
    if (!m_isInitialized)
        return false;

    if (FrameView* frameView = frame()->view()) {
        if (frameView->paintBehavior().containsAny({ PaintBehavior::SelectionOnly, PaintBehavior::SelectionAndBackgroundsOnly, PaintBehavior::ForceBlackText })) {
            // This paint behavior is used when drawing the find indicator and there's no need to
            // snapshot plug-ins, because they can never be painted as part of the find indicator.
            return false;
        }
    }

    return true;
}

bool PluginView::isBeingDestroyed() const
{
    return m_plugin->isBeingDestroyed();
}

RetainPtr<PDFDocument> PluginView::pdfDocumentForPrinting() const
{
    return m_plugin->pdfDocumentForPrinting();
}

WebCore::FloatSize PluginView::pdfDocumentSizeForPrinting() const
{
    return m_plugin->pdfDocumentSizeForPrinting();
}

id PluginView::accessibilityHitTest(const WebCore::IntPoint& point) const
{
    return m_plugin->accessibilityHitTest(point);
}

std::tuple<String, PDFSelection *, NSDictionary *> PluginView::lookupTextAtLocation(const WebCore::FloatPoint& point, WebHitTestResultData& data) const
{
    return m_plugin->lookupTextAtLocation(point, data);
}

WebCore::FloatRect PluginView::rectForSelectionInRootView(PDFSelection *selection) const
{
    return m_plugin->rectForSelectionInRootView(selection);
}

CGFloat PluginView::contentScaleFactor() const
{
    return m_plugin->scaleFactor();
}

bool PluginView::isUsingUISideCompositing() const
{
    return m_webPage->isUsingUISideCompositing();
}

} // namespace WebKit

#endif
