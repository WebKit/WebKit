/*
 * Copyright (C) 2010, 2012, 2015 Apple Inc. All rights reserved.
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

#include "Plugin.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
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

class PluginView::URLRequest : public RefCounted<URLRequest> {
public:
    static Ref<PluginView::URLRequest> create(uint64_t requestID, FrameLoadRequest&& request, bool allowPopups)
    {
        return adoptRef(*new URLRequest(requestID, WTFMove(request), allowPopups));
    }

    uint64_t requestID() const { return m_requestID; }
    const String& target() const { return m_request.frameName(); }
    const ResourceRequest & request() const { return m_request.resourceRequest(); }
    bool allowPopups() const { return m_allowPopups; }

private:
    URLRequest(uint64_t requestID, FrameLoadRequest&& request, bool allowPopups)
        : m_requestID { requestID }
        , m_request { WTFMove(request) }
        , m_allowPopups { allowPopups }
    {
    }

    uint64_t m_requestID;
    FrameLoadRequest m_request;
    bool m_allowPopups;
};

class PluginView::Stream : public RefCounted<PluginView::Stream>, NetscapePlugInStreamLoaderClient {
public:
    static Ref<Stream> create(PluginView* pluginView, uint64_t streamID, const ResourceRequest& request)
    {
        return adoptRef(*new Stream(pluginView, streamID, request));
    }
    ~Stream();

    void start();
    void cancel();
    void continueLoad();

    uint64_t streamID() const { return m_streamID; }

private:
    Stream(PluginView* pluginView, uint64_t streamID, const ResourceRequest& request)
        : m_pluginView(pluginView)
        , m_streamID(streamID)
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
    uint64_t m_streamID;
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
    ASSERT(m_pluginView->m_plugin);
    ASSERT(!m_loader);

    Frame* frame = m_pluginView->m_pluginElement->document().frame();
    ASSERT(frame);

    WebProcess::singleton().webLoaderStrategy().schedulePluginStreamLoad(*frame, *this, ResourceRequest {m_request}, [this, protectedThis = Ref { *this }](RefPtr<NetscapePlugInStreamLoader>&& loader) {
        m_loader = WTFMove(loader);
    });
}

void PluginView::Stream::cancel()
{
    ASSERT(m_loader);

    m_streamWasCancelled = true;
    m_loader->cancel(m_loader->cancelledError());
    m_loader = nullptr;
}

void PluginView::Stream::continueLoad()
{
    ASSERT(m_pluginView->m_plugin);
    ASSERT(m_loadCallback);

    m_loadCallback(ResourceRequest(m_request));
}

static String buildHTTPHeaders(const ResourceResponse& response, long long& expectedContentLength)
{
    if (!response.isInHTTPFamily())
        return String();

    StringBuilder header;
    header.append("HTTP ", response.httpStatusCode(), ' ', response.httpStatusText(), '\n');
    for (auto& field : response.httpHeaderFields())
        header.append(field.key, ": ", field.value, '\n');

    // If the content is encoded (most likely compressed), then don't send its length to the plugin,
    // which is only interested in the decoded length, not yet known at the moment.
    // <rdar://problem/4470599> tracks a request for -[NSURLResponse expectedContentLength] to incorporate this logic.
    String contentEncoding = response.httpHeaderField(HTTPHeaderName::ContentEncoding);
    if (!contentEncoding.isNull() && contentEncoding != "identity")
        expectedContentLength = -1;

    return header.toString();
}

static uint32_t lastModifiedDateMS(const ResourceResponse& response)
{
    auto lastModified = response.lastModified();
    if (!lastModified)
        return 0;

    return lastModified.value().secondsSinceEpoch().millisecondsAs<uint32_t>();
}

void PluginView::Stream::willSendRequest(NetscapePlugInStreamLoader*, ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& decisionHandler)
{
    const URL& requestURL = request.url();
    const URL& redirectResponseURL = redirectResponse.url();

    m_loadCallback = WTFMove(decisionHandler);
    m_request = request;
    m_pluginView->m_plugin->streamWillSendRequest(m_streamID, requestURL, redirectResponseURL, redirectResponse.httpStatusCode());
}

void PluginView::Stream::didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse& response)
{
    // Compute the stream related data from the resource response.
    const URL& responseURL = response.url();
    const String& mimeType = response.mimeType();
    long long expectedContentLength = response.expectedContentLength();
    
    String headers = buildHTTPHeaders(response, expectedContentLength);

    uint32_t streamLength = 0;
    if (expectedContentLength > 0)
        streamLength = expectedContentLength;

    m_pluginView->m_plugin->streamDidReceiveResponse(m_streamID, responseURL, streamLength, lastModifiedDateMS(response), mimeType, headers, response.suggestedFilename());
}

void PluginView::Stream::didReceiveData(NetscapePlugInStreamLoader*, const SharedBuffer& buffer)
{
    m_pluginView->m_plugin->streamDidReceiveData(m_streamID, buffer);
}

void PluginView::Stream::didFail(NetscapePlugInStreamLoader*, const ResourceError& error) 
{
    // Calling streamDidFail could cause us to be deleted, so we hold on to a reference here.
    Ref<Stream> protect(*this);

    // We only want to call streamDidFail if the stream was not explicitly cancelled by the plug-in.
    if (!m_streamWasCancelled)
        m_pluginView->m_plugin->streamDidFail(m_streamID, error.isCancellation());

    m_pluginView->removeStream(this);
    m_pluginView = 0;
}

void PluginView::Stream::didFinishLoading(NetscapePlugInStreamLoader*)
{
    // Calling streamDidFinishLoading could cause us to be deleted, so we hold on to a reference here.
    Ref<Stream> protect(*this);

    m_pluginView->m_plugin->streamDidFinishLoading(m_streamID);

    m_pluginView->removeStream(this);
    m_pluginView = 0;
}

static inline WebPage* webPage(HTMLPlugInElement* pluginElement)
{
    Frame* frame = pluginElement->document().frame();
    ASSERT(frame);

    WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
    if (!webFrame)
        return nullptr;

    return webFrame->page();
}

Ref<PluginView> PluginView::create(HTMLPlugInElement& pluginElement, Ref<Plugin>&& plugin, const Plugin::Parameters& parameters)
{
    return adoptRef(*new PluginView(pluginElement, WTFMove(plugin), parameters));
}

PluginView::PluginView(HTMLPlugInElement& pluginElement, Ref<Plugin>&& plugin, const Plugin::Parameters& parameters)
    : PluginViewBase(0)
    , m_pluginElement(&pluginElement)
    , m_plugin(WTFMove(plugin))
    , m_webPage(webPage(m_pluginElement.get()))
    , m_parameters(parameters)
    , m_pendingURLRequestsTimer(RunLoop::main(), this, &PluginView::pendingURLRequestsTimerFired)
{
    m_webPage->addPluginView(this);
}

PluginView::~PluginView()
{
    if (m_webPage)
        m_webPage->removePluginView(this);

    ASSERT(!m_plugin || !m_plugin->isBeingDestroyed());

    if (m_isWaitingUntilMediaCanStart)
        m_pluginElement->document().removeMediaCanStartListener(*this);

    m_pluginElement->document().removeAudioProducer(*this);

    destroyPluginAndReset();

    // Null out the plug-in element explicitly so we'll crash earlier if we try to use
    // the plug-in view after it's been destroyed.
    m_pluginElement = nullptr;
}

void PluginView::destroyPluginAndReset()
{
    // Cancel all pending frame loads.
    for (FrameLoadMap::iterator it = m_pendingFrameLoads.begin(), end = m_pendingFrameLoads.end(); it != end; ++it)
        it->key->setLoadListener(nullptr);

    if (m_plugin) {
        m_plugin->destroyPlugin();

        m_pendingURLRequests.clear();
        m_pendingURLRequestsTimer.stop();
    }

    cancelAllStreams();
}

void PluginView::setLayerHostingMode(LayerHostingMode layerHostingMode)
{
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    if (!m_plugin)
        return;

    if (m_isInitialized)
        m_plugin->setLayerHostingMode(layerHostingMode);
    else
        m_parameters.layerHostingMode = layerHostingMode;
#else
    UNUSED_PARAM(layerHostingMode);
#endif
}

Frame* PluginView::frame() const
{
    return m_pluginElement->document().frame();
}

void PluginView::manualLoadDidReceiveResponse(const ResourceResponse& response)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::Initial);
        m_manualStreamState = ManualStreamState::HasReceivedResponse;
        m_manualStreamResponse = response;
        return;
    }

    // Compute the stream related data from the resource response.
    const URL& responseURL = response.url();
    const String& mimeType = response.mimeType();
    long long expectedContentLength = response.expectedContentLength();
    
    String headers = buildHTTPHeaders(response, expectedContentLength);
    
    uint32_t streamLength = 0;
    if (expectedContentLength > 0)
        streamLength = expectedContentLength;

    m_plugin->manualStreamDidReceiveResponse(responseURL, streamLength, lastModifiedDateMS(response), mimeType, headers, response.suggestedFilename());
}

void PluginView::manualLoadDidReceiveData(const SharedBuffer& buffer)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamData.append(buffer);
        return;
    }

    m_plugin->manualStreamDidReceiveData(buffer);
}

void PluginView::manualLoadDidFinishLoading()
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
        m_manualStreamState = ManualStreamState::Finished;
        return;
    }

    m_plugin->manualStreamDidFinishLoading();
}

void PluginView::manualLoadDidFail(const ResourceError& error)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        m_manualStreamState = ManualStreamState::Finished;
        m_manualStreamError = error;
        m_manualStreamData.reset();
        return;
    }

    m_plugin->manualStreamDidFail(error.isCancellation());
}

void PluginView::pageScaleFactorDidChange()
{
    viewGeometryDidChange();
}

void PluginView::topContentInsetDidChange()
{
    viewGeometryDidChange();
}

void PluginView::setPageScaleFactor(double scaleFactor, IntPoint)
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

bool PluginView::handlesPageScaleFactor() const
{
    if (!m_plugin || !m_isInitialized)
        return false;

    return m_plugin->handlesPageScaleFactor();
}

bool PluginView::requiresUnifiedScaleFactor() const
{
    if (!m_plugin || !m_isInitialized)
        return false;

    return m_plugin->requiresUnifiedScaleFactor();
}

void PluginView::webPageDestroyed()
{
    m_webPage = 0;
}

void PluginView::activityStateDidChange(OptionSet<WebCore::ActivityState::Flag> changed)
{
    if (!m_plugin || !m_isInitialized)
        return;

    if (changed & ActivityState::IsVisibleOrOccluded)
        m_plugin->windowVisibilityChanged(m_webPage->isVisibleOrOccluded());
    if (changed & ActivityState::WindowIsActive)
        m_plugin->windowFocusChanged(m_webPage->windowIsFocused());
}

#if PLATFORM(COCOA)
void PluginView::setDeviceScaleFactor(float scaleFactor)
{
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->contentsScaleFactorChanged(scaleFactor);
}

void PluginView::windowAndViewFramesChanged(const FloatRect& windowFrameInScreenCoordinates, const FloatRect& viewFrameInWindowCoordinates)
{
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->windowAndViewFramesChanged(enclosingIntRect(windowFrameInScreenCoordinates), enclosingIntRect(viewFrameInWindowCoordinates));
}

id PluginView::accessibilityAssociatedPluginParentForElement(Element* element) const
{
    if (!m_plugin)
        return nil;
    return m_plugin->accessibilityAssociatedPluginParentForElement(element);
}
    
id PluginView::accessibilityObject() const
{
    if (!m_isInitialized || !m_plugin)
        return 0;
    
    return m_plugin->accessibilityObject();
}
#endif

void PluginView::initializePlugin()
{
    if (m_isInitialized)
        return;

    if (!m_plugin) {
        // We've already tried and failed to initialize the plug-in.
        return;
    }

    if (Frame* frame = m_pluginElement->document().frame()) {
        if (Page* page = frame->page()) {
            
            // We shouldn't initialize the plug-in right now, add a listener.
            if (!page->canStartMedia()) {
                if (m_isWaitingUntilMediaCanStart)
                    return;
                
                m_isWaitingUntilMediaCanStart = true;
                m_pluginElement->document().addMediaCanStartListener(*this);
                return;
            }
        }
    }

    m_pluginElement->document().addAudioProducer(*this);

    m_plugin->initialize(*this, m_parameters);
    
    // Plug-in initialization continued in didInitializePlugin().
}

void PluginView::didInitializePlugin()
{
    m_isInitialized = true;

#if PLATFORM(COCOA)
    windowAndViewFramesChanged(m_webPage->windowFrameInScreenCoordinates(), m_webPage->viewFrameInWindowCoordinates());
#endif

    viewVisibilityDidChange();
    viewGeometryDidChange();

    if (m_pluginElement->document().focusedElement() == m_pluginElement)
        m_plugin->setFocus(true);

    redeliverManualStream();

#if PLATFORM(COCOA)
    if (m_plugin->pluginLayer() && frame()) {
        frame()->view()->enterCompositingMode();
        m_pluginElement->invalidateStyleAndLayerComposition();
    }
    m_plugin->visibilityDidChange(isVisible());
    m_plugin->windowVisibilityChanged(m_webPage->isVisibleOrOccluded());
    m_plugin->windowFocusChanged(m_webPage->windowIsFocused());
#endif

    if (wantsWheelEvents()) {
        if (Frame* frame = m_pluginElement->document().frame()) {
            if (FrameView* frameView = frame->view())
                frameView->setNeedsLayoutAfterViewConfigurationChange();
        }
    }

    if (Frame* frame = m_pluginElement->document().frame()) {
        auto* webFrame = WebFrame::fromCoreFrame(*frame);
        if (webFrame->isMainFrame())
            webFrame->page()->send(Messages::WebPageProxy::MainFramePluginHandlesPageScaleGestureDidChange(handlesPageScaleFactor()));
    }
}

#if PLATFORM(COCOA)
PlatformLayer* PluginView::platformLayer() const
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return 0;

    return m_plugin->pluginLayer();
}
#endif

JSObject* PluginView::scriptObject(JSGlobalObject* globalObject)
{
    // If we're already waiting for synchronous initialization of the plugin,
    // calls to scriptObject() are from the plug-in itself and need to return 0;
    if (m_isWaitingForSynchronousInitialization)
        return 0;

    // We might not have started initialization of the plug-in yet, the plug-in might be in the middle
    // of being initializing asynchronously, or initialization might have previously failed.
    if (!m_isInitialized || !m_plugin)
        return 0;

    UNUSED_PARAM(globalObject);
    return 0;
}

void PluginView::storageBlockingStateChanged()
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return;

    bool storageBlockingPolicy = !frame()->document()->securityOrigin().canAccessPluginStorage(frame()->document()->topOrigin());

    m_plugin->storageBlockingStateChanged(storageBlockingPolicy);
}

bool PluginView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->handleScroll(direction, granularity);
}

Scrollbar* PluginView::horizontalScrollbar()
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return 0;

    return m_plugin->horizontalScrollbar();
}

Scrollbar* PluginView::verticalScrollbar()
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return 0;

    return m_plugin->verticalScrollbar();
}

bool PluginView::wantsWheelEvents()
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return 0;
    
    return m_plugin->wantsWheelEvents();
}

void PluginView::setFrameRect(const WebCore::IntRect& rect)
{
    Widget::setFrameRect(rect);
    viewGeometryDidChange();
}

void PluginView::paint(GraphicsContext& context, const IntRect& /*dirtyRect*/, Widget::SecurityOriginPaintPolicy, EventRegionContext*)
{
    if (!m_plugin || !m_isInitialized)
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

    if (m_transientPaintingSnapshot) {
        m_transientPaintingSnapshot->paint(context, contentsScaleFactor(), frameRect().location(), m_transientPaintingSnapshot->bounds());
        return;
    }
    
    GraphicsContextStateSaver stateSaver(context);

    // Translate the coordinate system so that the origin is in the top-left corner of the plug-in.
    context.translate(frameRect().location().x(), frameRect().location().y());

    m_plugin->paint(context, paintRect);
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
    if (!m_isInitialized || !m_plugin)
        return 0;

    return m_plugin->countFindMatches(target, options, maxMatchCount);
}

bool PluginView::findString(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->findString(target, options, maxMatchCount);
}

String PluginView::getSelectionString() const
{
    if (!m_isInitialized || !m_plugin)
        return String();

    return m_plugin->getSelectionString();
}

std::unique_ptr<WebEvent> PluginView::createWebEvent(MouseEvent& event) const
{
    WebEvent::Type type = WebEvent::NoType;
    unsigned clickCount = 1;
    if (event.type() == eventNames().mousedownEvent)
        type = WebEvent::MouseDown;
    else if (event.type() == eventNames().mouseupEvent)
        type = WebEvent::MouseUp;
    else if (event.type() == eventNames().mouseoverEvent) {
        type = WebEvent::MouseMove;
        clickCount = 0;
    } else if (event.type() == eventNames().clickEvent)
        return nullptr;
    else
        ASSERT_NOT_REACHED();

    WebMouseEvent::Button button = WebMouseEvent::NoButton;
    switch (event.button()) {
    case WebCore::LeftButton:
        button = WebMouseEvent::LeftButton;
        break;
    case WebCore::MiddleButton:
        button = WebMouseEvent::MiddleButton;
        break;
    case WebCore::RightButton:
        button = WebMouseEvent::RightButton;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    OptionSet<WebEvent::Modifier> modifiers;
    if (event.shiftKey())
        modifiers.add(WebEvent::Modifier::ShiftKey);
    if (event.ctrlKey())
        modifiers.add(WebEvent::Modifier::ControlKey);
    if (event.altKey())
        modifiers.add(WebEvent::Modifier::AltKey);
    if (event.metaKey())
        modifiers.add(WebEvent::Modifier::MetaKey);

    return makeUnique<WebMouseEvent>(type, button, event.buttons(), m_plugin->convertToRootView(IntPoint(event.offsetX(), event.offsetY())), event.screenLocation(), 0, 0, 0, clickCount, modifiers, WallTime { }, 0);
}

void PluginView::handleEvent(Event& event)
{
    if (!m_isInitialized || !m_plugin)
        return;

    const WebEvent* currentEvent = WebPage::currentEvent();
    std::unique_ptr<WebEvent> simulatedWebEvent;
    if (is<MouseEvent>(event) && downcast<MouseEvent>(event).isSimulated()) {
        simulatedWebEvent = createWebEvent(downcast<MouseEvent>(event));
        currentEvent = simulatedWebEvent.get();
    }
    if (!currentEvent)
        return;

    bool didHandleEvent = false;

    if ((event.type() == eventNames().mousemoveEvent && currentEvent->type() == WebEvent::MouseMove)
        || (event.type() == eventNames().mousedownEvent && currentEvent->type() == WebEvent::MouseDown)
        || (event.type() == eventNames().mouseupEvent && currentEvent->type() == WebEvent::MouseUp)) {
        // FIXME: Clicking in a scroll bar should not change focus.
        if (currentEvent->type() == WebEvent::MouseDown) {
            focusPluginElement();
            frame()->eventHandler().setCapturingMouseEventsElement(m_pluginElement.get());
        } else if (currentEvent->type() == WebEvent::MouseUp)
            frame()->eventHandler().setCapturingMouseEventsElement(nullptr);

        didHandleEvent = m_plugin->handleMouseEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    } else if (eventNames().isWheelEventType(event.type())
        && currentEvent->type() == WebEvent::Wheel && m_plugin->wantsWheelEvents())
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
    
bool PluginView::handleEditingCommand(const String& commandName, const String& argument)
{
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->handleEditingCommand(commandName, argument);
}
    
bool PluginView::isEditingCommandEnabled(const String& commandName)
{
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->isEditingCommandEnabled(commandName);
}

bool PluginView::shouldAllowScripting()
{
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->shouldAllowScripting();
}

bool PluginView::shouldAllowNavigationFromDrags() const
{
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->shouldAllowNavigationFromDrags();
}

void PluginView::willDetachRenderer()
{
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->willDetachRenderer();
}

RefPtr<FragmentedSharedBuffer> PluginView::liveResourceData() const
{
    if (!m_isInitialized || !m_plugin) {
        if (m_manualStreamState == ManualStreamState::Finished)
            return m_manualStreamData.get();

        return nullptr;
    }

    return m_plugin->liveResourceData();
}

bool PluginView::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->performDictionaryLookupAtLocation(point);
}

bool PluginView::existingSelectionContainsPoint(const WebCore::FloatPoint& point) const
{
    if (!m_isInitialized || !m_plugin)
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
    if (!m_isInitialized || !m_plugin || !parent())
        return;

    ASSERT(frame());
    float pageScaleFactor = frame()->page() ? frame()->page()->pageScaleFactor() : 1;

    IntPoint scaledFrameRectLocation(frameRect().location().x() * pageScaleFactor, frameRect().location().y() * pageScaleFactor);
    IntPoint scaledLocationInRootViewCoordinates(parent()->contentsToRootView(scaledFrameRectLocation));

    // FIXME: We still don't get the right coordinates for transformed plugins.
    AffineTransform transform;
    transform.translate(scaledLocationInRootViewCoordinates.x(), scaledLocationInRootViewCoordinates.y());
    transform.scale(pageScaleFactor);

    // FIXME: The way we calculate this clip rect isn't correct.
    // But it is still important to distinguish between empty and non-empty rects so we can notify the plug-in when it becomes invisible.
    // Making the rect actually correct is covered by https://bugs.webkit.org/show_bug.cgi?id=95362
    IntRect clipRect = boundsRect();
    
    // FIXME: We can only get a semi-reliable answer from clipRectInWindowCoordinates() when the page is not scaled.
    // Fixing that is tracked in <rdar://problem/9026611> - Make the Widget hierarchy play nicely with transforms, for zoomed plug-ins and iframes
    if (pageScaleFactor == 1) {
        clipRect = clipRectInWindowCoordinates();
        if (!clipRect.isEmpty())
            clipRect = boundsRect();
    }
    
    m_plugin->geometryDidChange(size(), clipRect, transform);
}

void PluginView::viewVisibilityDidChange()
{
    if (!m_isInitialized || !m_plugin || !parent())
        return;

    m_plugin->visibilityDidChange(isVisible());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    Frame* frame = this->frame();

    // Get the window clip rect for the plugin element (in window coordinates).
    IntRect windowClipRect = frame->view()->windowClipRectForFrameOwner(m_pluginElement.get(), true);

    // Intersect the two rects to get the view clip rect in window coordinates.
    frameRectInWindowCoordinates.intersect(windowClipRect);

    return frameRectInWindowCoordinates;
}

void PluginView::focusPluginElement()
{
    ASSERT(frame());
    
    if (Page* page = frame()->page())
        CheckedRef(page->focusController())->setFocusedElement(m_pluginElement.get(), *frame());
    else
        RefPtr(frame()->document())->setFocusedElement(m_pluginElement.get());
}

void PluginView::pendingURLRequestsTimerFired()
{
    ASSERT(!m_pendingURLRequests.isEmpty());
    
    RefPtr<URLRequest> urlRequest = m_pendingURLRequests.takeFirst();

    // If there are more requests to perform, reschedule the timer.
    if (!m_pendingURLRequests.isEmpty())
        m_pendingURLRequestsTimer.startOneShot(0_s);
    
    performURLRequest(urlRequest.get());
}
    
void PluginView::performURLRequest(URLRequest* request)
{
    // This protector is needed to make sure the PluginView is not destroyed while it is still needed.
    Ref<PluginView> protect(*this);

    // First, check if this is a javascript: url.
    if (request->request().url().protocolIsJavaScript()) {
        performJavaScriptURLRequest(request);
        return;
    }

    if (!request->target().isNull()) {
        performFrameLoadURLRequest(request);
        return;
    }

    // This request is to load a URL and create a stream.
    auto stream = PluginView::Stream::create(this, request->requestID(), request->request());
    addStream(stream.ptr());
    stream->start();
}

void PluginView::performFrameLoadURLRequest(URLRequest* request)
{
    ASSERT(!request->target().isNull());

    Frame* frame = m_pluginElement->document().frame();
    if (!frame)
        return;

    if (!m_pluginElement->document().securityOrigin().canDisplay(request->request().url())) {
        // We can't load the request, send back a reply to the plug-in.
        m_plugin->frameDidFail(request->requestID(), false);
        return;
    }

    UserGestureIndicator gestureIndicator(request->allowPopups() ? std::optional<ProcessingUserGestureState>(ProcessingUserGesture) : std::nullopt);

    // First, try to find a target frame.
    Frame* targetFrame = frame->loader().findFrameForNavigation(request->target());
    if (!targetFrame) {
        // We did not find a target frame. Ask our frame to load the page. This may or may not create a popup window.
        FrameLoadRequest frameLoadRequest { *frame, request->request() };
        frameLoadRequest.setFrameName(request->target());
        frameLoadRequest.setShouldCheckNewWindowPolicy(true);
        frame->loader().load(WTFMove(frameLoadRequest));

        // FIXME: We don't know whether the window was successfully created here so we just assume that it worked.
        // It's better than not telling the plug-in anything.
        m_plugin->frameDidFinishLoading(request->requestID());
        return;
    }

    // Now ask the frame to load the request.
    targetFrame->loader().load(FrameLoadRequest(*targetFrame, request->request() ));

    auto* targetWebFrame = WebFrame::fromCoreFrame(*targetFrame);
    ASSERT(targetWebFrame);

    if (WebFrame::LoadListener* loadListener = targetWebFrame->loadListener()) {
        // Check if another plug-in view or even this view is waiting for the frame to load.
        // If it is, tell it that the load was cancelled because it will be anyway.
        loadListener->didFailLoad(targetWebFrame, true);
    }
    
    m_pendingFrameLoads.set(targetWebFrame, request);
    targetWebFrame->setLoadListener(this);
}

void PluginView::performJavaScriptURLRequest(URLRequest* request)
{
    ASSERT(request->request().url().protocolIsJavaScript());

    RefPtr<Frame> frame = m_pluginElement->document().frame();
    if (!frame)
        return;
    
    String jsString = PAL::decodeURLEscapeSequences(request->request().url().string().substring(sizeof("javascript:") - 1));

    if (!request->target().isNull()) {
        // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
        if (frame->tree().find(request->target(), *frame) != frame) {
            // Let the plug-in know that its frame load failed.
            m_plugin->frameDidFail(request->requestID(), false);
            return;
        }
    }

    // Evaluate the JavaScript code. Note that running JavaScript here could cause the plug-in to be destroyed, so we
    // grab references to the plug-in here.
    RefPtr<Plugin> plugin = m_plugin;
    auto result = frame->script().executeScriptIgnoringException(jsString, request->allowPopups());

    if (!result)
        return;

    // Check if evaluating the JavaScript destroyed the plug-in.
    if (!plugin->controller())
        return;

    // Don't notify the plug-in at all about targeted javascript: requests. This matches Mozilla and WebKit1.
    if (!request->target().isNull())
        return;

    JSGlobalObject* globalObject = frame->script().globalObject(pluginWorld());
    String resultString;
    result.getString(globalObject, resultString);
  
    // Send the result back to the plug-in.
    plugin->didEvaluateJavaScript(request->requestID(), resultString);
}

void PluginView::addStream(Stream* stream)
{
    ASSERT(!m_streams.contains(stream->streamID()));
    m_streams.set(stream->streamID(), stream);
}
    
void PluginView::removeStream(Stream* stream)
{
    ASSERT(m_streams.get(stream->streamID()) == stream);
    
    m_streams.remove(stream->streamID());
}

void PluginView::cancelAllStreams()
{
    for (auto& stream : copyToVector(m_streams.values()))
        stream->cancel();

    // Cancelling a stream removes it from the m_streams map, so if we cancel all streams the map should be empty.
    ASSERT(m_streams.isEmpty());
}

void PluginView::redeliverManualStream()
{
    if (m_manualStreamState == ManualStreamState::Initial) {
        // Nothing to do.
        return;
    }

    if (m_manualStreamState == ManualStreamState::Failed) {
        manualLoadDidFail(m_manualStreamError);
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
    if (!parent() || !m_plugin || !m_isInitialized)
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

void PluginView::setFocus(bool hasFocus)
{
    Widget::setFocus(hasFocus);

    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->setFocus(hasFocus);
}

void PluginView::mediaCanStart(WebCore::Document&)
{
    ASSERT(m_isWaitingUntilMediaCanStart);
    m_isWaitingUntilMediaCanStart = false;
    
    initializePlugin();
}

void PluginView::pageMutedStateDidChange()
{
}

void PluginView::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    FrameLoadRequest frameLoadRequest { m_pluginElement->document(), m_pluginElement->document().securityOrigin(), { }, target, InitiatedByMainFrame::Unknown };
    frameLoadRequest.resourceRequest().setHTTPMethod(method);
    frameLoadRequest.resourceRequest().setURL(m_pluginElement->document().completeURL(urlString));
    frameLoadRequest.resourceRequest().setHTTPHeaderFields(headerFields);
    if (!httpBody.isEmpty()) {
        frameLoadRequest.resourceRequest().setHTTPBody(FormData::create(httpBody.data(), httpBody.size()));
        if (frameLoadRequest.resourceRequest().httpContentType().isEmpty())
            frameLoadRequest.resourceRequest().setHTTPContentType("application/x-www-form-urlencoded");
    }

    String referrer = SecurityPolicy::generateReferrerHeader(frame()->document()->referrerPolicy(), frameLoadRequest.resourceRequest().url(), frame()->loader().outgoingReferrer());
    if (!referrer.isEmpty())
        frameLoadRequest.resourceRequest().setHTTPReferrer(referrer);

    m_pendingURLRequests.append(URLRequest::create(requestID, WTFMove(frameLoadRequest), allowPopups));
    m_pendingURLRequestsTimer.startOneShot(0_s);
}

float PluginView::contentsScaleFactor()
{
    if (Page* page = frame() ? frame()->page() : 0)
        return page->deviceScaleFactor();
        
    return 1;
}

void PluginView::didFinishLoad(WebFrame* webFrame)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(nullptr);

    m_plugin->frameDidFinishLoading(request->requestID());
}

void PluginView::didFailLoad(WebFrame* webFrame, bool wasCancelled)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(nullptr);
    
    m_plugin->frameDidFail(request->requestID(), wasCancelled);
}

bool PluginView::shouldCreateTransientPaintingSnapshot() const
{
    if (!m_plugin)
        return false;

    if (!m_isInitialized)
        return false;

    if (FrameView* frameView = frame()->view()) {
        if (frameView->paintBehavior().containsAny({ PaintBehavior::SelectionOnly, PaintBehavior::SelectionAndBackgroundsOnly, PaintBehavior::ForceBlackText})) {
            // This paint behavior is used when drawing the find indicator and there's no need to
            // snapshot plug-ins, because they can never be painted as part of the find indicator.
            return false;
        }
    }

    return true;
}

} // namespace WebKit
