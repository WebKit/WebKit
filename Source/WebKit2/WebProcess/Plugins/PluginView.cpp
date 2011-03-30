/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "NPRuntimeUtilities.h"
#include "Plugin.h"
#include "ShareableBitmap.h"
#include "WebEvent.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/Chrome.h>
#include <WebCore/CookieJar.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Event.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HostWindow.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/ProxyServer.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/ResourceLoadScheduler.h>
#include <WebCore/ScrollView.h>
#include <WebCore/Settings.h>

using namespace JSC;
using namespace WebCore;

namespace WebKit {

class PluginView::URLRequest : public RefCounted<URLRequest> {
public:
    static PassRefPtr<PluginView::URLRequest> create(uint64_t requestID, const FrameLoadRequest& request, bool allowPopups)
    {
        return adoptRef(new URLRequest(requestID, request, allowPopups));
    }

    uint64_t requestID() const { return m_requestID; }
    const String& target() const { return m_request.frameName(); }
    const ResourceRequest & request() const { return m_request.resourceRequest(); }
    bool allowPopups() const { return m_allowPopups; }

private:
    URLRequest(uint64_t requestID, const FrameLoadRequest& request, bool allowPopups)
        : m_requestID(requestID)
        , m_request(request)
        , m_allowPopups(allowPopups)
    {
    }

    uint64_t m_requestID;
    FrameLoadRequest m_request;
    bool m_allowPopups;
};

class PluginView::Stream : public RefCounted<PluginView::Stream>, NetscapePlugInStreamLoaderClient {
public:
    static PassRefPtr<Stream> create(PluginView* pluginView, uint64_t streamID, const ResourceRequest& request)
    {
        return adoptRef(new Stream(pluginView, streamID, request));
    }
    ~Stream();

    void start();
    void cancel();

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
    virtual void didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse&);
    virtual void didReceiveData(NetscapePlugInStreamLoader*, const char*, int);
    virtual void didFail(NetscapePlugInStreamLoader*, const ResourceError&);
    virtual void didFinishLoading(NetscapePlugInStreamLoader*);

    PluginView* m_pluginView;
    uint64_t m_streamID;
    const ResourceRequest m_request;
    
    // True if the stream was explicitly cancelled by calling cancel().
    // (As opposed to being cancelled by the user hitting the stop button for example.
    bool m_streamWasCancelled;
    
    RefPtr<NetscapePlugInStreamLoader> m_loader;
};

PluginView::Stream::~Stream()
{
    ASSERT(!m_pluginView);
}
    
void PluginView::Stream::start()
{
    ASSERT(!m_loader);

    Frame* frame = m_pluginView->m_pluginElement->document()->frame();
    ASSERT(frame);

    m_loader = resourceLoadScheduler()->schedulePluginStreamLoad(frame, this, m_request);
}

void PluginView::Stream::cancel()
{
    ASSERT(m_loader);

    m_streamWasCancelled = true;
    m_loader->cancel(m_loader->cancelledError());
    m_loader = 0;
}

static String buildHTTPHeaders(const ResourceResponse& response, long long& expectedContentLength)
{
    if (!response.isHTTP())
        return String();

    Vector<UChar> stringBuilder;
    String separator(": ");
    
    String statusLine = String::format("HTTP %d ", response.httpStatusCode());
    stringBuilder.append(statusLine.characters(), statusLine.length());
    stringBuilder.append(response.httpStatusText().characters(), response.httpStatusText().length());
    stringBuilder.append('\n');
    
    HTTPHeaderMap::const_iterator end = response.httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = response.httpHeaderFields().begin(); it != end; ++it) {
        stringBuilder.append(it->first.characters(), it->first.length());
        stringBuilder.append(separator.characters(), separator.length());
        stringBuilder.append(it->second.characters(), it->second.length());
        stringBuilder.append('\n');
    }
    
    String headers = String::adopt(stringBuilder);
    
    // If the content is encoded (most likely compressed), then don't send its length to the plugin,
    // which is only interested in the decoded length, not yet known at the moment.
    // <rdar://problem/4470599> tracks a request for -[NSURLResponse expectedContentLength] to incorporate this logic.
    String contentEncoding = response.httpHeaderField("Content-Encoding");
    if (!contentEncoding.isNull() && contentEncoding != "identity")
        expectedContentLength = -1;

    return headers;
}

void PluginView::Stream::didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse& response)
{
    // Compute the stream related data from the resource response.
    const KURL& responseURL = response.url();
    const String& mimeType = response.mimeType();
    long long expectedContentLength = response.expectedContentLength();
    
    String headers = buildHTTPHeaders(response, expectedContentLength);

    uint32_t streamLength = 0;
    if (expectedContentLength > 0)
        streamLength = expectedContentLength;

    m_pluginView->m_plugin->streamDidReceiveResponse(m_streamID, responseURL, streamLength, response.lastModifiedDate(), mimeType, headers);
}

void PluginView::Stream::didReceiveData(NetscapePlugInStreamLoader*, const char* bytes, int length)
{
    m_pluginView->m_plugin->streamDidReceiveData(m_streamID, bytes, length);
}

void PluginView::Stream::didFail(NetscapePlugInStreamLoader*, const ResourceError& error) 
{
    // Calling streamDidFail could cause us to be deleted, so we hold on to a reference here.
    RefPtr<Stream> protect(this);

    // We only want to call streamDidFail if the stream was not explicitly cancelled by the plug-in.
    if (!m_streamWasCancelled)
        m_pluginView->m_plugin->streamDidFail(m_streamID, error.isCancellation());

    m_pluginView->removeStream(this);
    m_pluginView = 0;
}

void PluginView::Stream::didFinishLoading(NetscapePlugInStreamLoader*)
{
    // Calling streamDidFinishLoading could cause us to be deleted, so we hold on to a reference here.
    RefPtr<Stream> protectStream(this);

    // Protect the plug-in while we're calling into it.
    NPRuntimeObjectMap::PluginProtector pluginProtector(&m_pluginView->m_npRuntimeObjectMap);
    m_pluginView->m_plugin->streamDidFinishLoading(m_streamID);

    m_pluginView->removeStream(this);
    m_pluginView = 0;
}

static inline WebPage* webPage(HTMLPlugInElement* pluginElement)
{
    Frame* frame = pluginElement->document()->frame();
    ASSERT(frame);

    WebPage* webPage = static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame()->page();
    ASSERT(webPage);

    return webPage;
}

PassRefPtr<PluginView> PluginView::create(PassRefPtr<HTMLPlugInElement> pluginElement, PassRefPtr<Plugin> plugin, const Plugin::Parameters& parameters)
{
    return adoptRef(new PluginView(pluginElement, plugin, parameters));
}

PluginView::PluginView(PassRefPtr<HTMLPlugInElement> pluginElement, PassRefPtr<Plugin> plugin, const Plugin::Parameters& parameters)
    : PluginViewBase(0)
    , m_pluginElement(pluginElement)
    , m_plugin(plugin)
    , m_webPage(webPage(m_pluginElement.get()))
    , m_parameters(parameters)
    , m_isInitialized(false)
    , m_isWaitingUntilMediaCanStart(false)
    , m_isBeingDestroyed(false)
    , m_pendingURLRequestsTimer(RunLoop::main(), this, &PluginView::pendingURLRequestsTimerFired)
    , m_npRuntimeObjectMap(this)
    , m_manualStreamState(StreamStateInitial)
{
#if PLATFORM(MAC)
    m_webPage->addPluginView(this);
#endif
}

PluginView::~PluginView()
{
#if PLATFORM(MAC)
    m_webPage->removePluginView(this);
#endif

    ASSERT(!m_isBeingDestroyed);

    if (m_isWaitingUntilMediaCanStart)
        m_pluginElement->document()->removeMediaCanStartListener(this);

    // Cancel all pending frame loads.
    FrameLoadMap::iterator end = m_pendingFrameLoads.end();
    for (FrameLoadMap::iterator it = m_pendingFrameLoads.begin(), end = m_pendingFrameLoads.end(); it != end; ++it)
        it->first->setLoadListener(0);

    if (m_plugin && m_isInitialized) {
        m_isBeingDestroyed = true;
        m_plugin->destroy();
        m_isBeingDestroyed = false;
    }

    // Invalidate the object map.
    m_npRuntimeObjectMap.invalidate();

    cancelAllStreams();

    // Null out the plug-in element explicitly so we'll crash earlier if we try to use
    // the plug-in view after it's been destroyed.
    m_pluginElement = nullptr;
}

Frame* PluginView::frame()
{
    return m_pluginElement->document()->frame();
}

void PluginView::manualLoadDidReceiveResponse(const ResourceResponse& response)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == StreamStateInitial);
        m_manualStreamState = StreamStateHasReceivedResponse;
        m_manualStreamResponse = response;
        return;
    }

    // Compute the stream related data from the resource response.
    const KURL& responseURL = response.url();
    const String& mimeType = response.mimeType();
    long long expectedContentLength = response.expectedContentLength();
    
    String headers = buildHTTPHeaders(response, expectedContentLength);
    
    uint32_t streamLength = 0;
    if (expectedContentLength > 0)
        streamLength = expectedContentLength;

    m_plugin->manualStreamDidReceiveResponse(responseURL, streamLength, response.lastModifiedDate(), mimeType, headers);
}

void PluginView::manualLoadDidReceiveData(const char* bytes, int length)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == StreamStateHasReceivedResponse);
        if (!m_manualStreamData)
            m_manualStreamData = SharedBuffer::create();

        m_manualStreamData->append(bytes, length);
        return;
    }

    m_plugin->manualStreamDidReceiveData(bytes, length);
}

void PluginView::manualLoadDidFinishLoading()
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == StreamStateHasReceivedResponse);
        m_manualStreamState = StreamStateFinished;
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
        m_manualStreamState = StreamStateFinished;
        m_manualStreamError = error;
        m_manualStreamData = nullptr;
        return;
    }

    m_plugin->manualStreamDidFail(error.isCancellation());
}

#if PLATFORM(MAC)    
void PluginView::setWindowIsVisible(bool windowIsVisible)
{
    if (!m_plugin)
        return;

    // FIXME: Implement.
}

void PluginView::setWindowIsFocused(bool windowIsFocused)
{
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->windowFocusChanged(windowIsFocused);    
}

void PluginView::windowAndViewFramesChanged(const IntRect& windowFrameInScreenCoordinates, const IntRect& viewFrameInWindowCoordinates)
{
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->windowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates);
}

bool PluginView::sendComplexTextInput(uint64_t pluginComplexTextInputIdentifier, const String& textInput)
{
    if (!m_plugin)
        return false;

    if (m_plugin->pluginComplexTextInputIdentifier() != pluginComplexTextInputIdentifier)
        return false;

    m_plugin->sendComplexTextInput(textInput);
    return true;
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

    if (Frame* frame = m_pluginElement->document()->frame()) {
        if (Page* page = frame->page()) {
            
            // We shouldn't initialize the plug-in right now, add a listener.
            if (!page->canStartMedia()) {
                if (m_isWaitingUntilMediaCanStart)
                    return;
                
                m_isWaitingUntilMediaCanStart = true;
                m_pluginElement->document()->addMediaCanStartListener(this);
                return;
            }
        }
    }
    
    if (!m_plugin->initialize(this, m_parameters)) {
        // We failed to initialize the plug-in.
        m_plugin = 0;

        return;
    }
    
    m_isInitialized = true;

    viewGeometryDidChange();

    redeliverManualStream();

#if PLATFORM(MAC)
    if (m_plugin->pluginLayer()) {
        if (frame()) {
            frame()->view()->enterCompositingMode();
            m_pluginElement->setNeedsStyleRecalc(SyntheticStyleChange);
        }
    }

    windowAndViewFramesChanged(m_webPage->windowFrameInScreenCoordinates(), m_webPage->viewFrameInWindowCoordinates());
    setWindowIsVisible(m_webPage->windowIsVisible());
    setWindowIsFocused(m_webPage->windowIsFocused());
#endif
}

#if PLATFORM(MAC)
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
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return 0;

    NPObject* scriptableNPObject = m_plugin->pluginScriptableNPObject();
    if (!scriptableNPObject)
        return 0;

    JSObject* jsObject = m_npRuntimeObjectMap.getOrCreateJSObject(globalObject, scriptableNPObject);
    releaseNPObject(scriptableNPObject);

    return jsObject;
}

void PluginView::privateBrowsingStateChanged(bool privateBrowsingEnabled)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->privateBrowsingStateChanged(privateBrowsingEnabled);
}

void PluginView::setFrameRect(const WebCore::IntRect& rect)
{
    Widget::setFrameRect(rect);
    viewGeometryDidChange();
}

void PluginView::setBoundsSize(const WebCore::IntSize& size)
{
    Widget::setBoundsSize(size);
    m_boundsSize = size;
    viewGeometryDidChange();
}

void PluginView::paint(GraphicsContext* context, const IntRect& dirtyRect)
{
    if (context->paintingDisabled() || !m_plugin || !m_isInitialized)
        return;

    IntRect dirtyRectInWindowCoordinates = parent()->contentsToWindow(dirtyRect);
    IntRect paintRectInWindowCoordinates = intersection(dirtyRectInWindowCoordinates, clipRectInWindowCoordinates());
    if (paintRectInWindowCoordinates.isEmpty())
        return;

    if (m_snapshot)
        m_snapshot->paint(*context, frameRect().location(), m_snapshot->bounds());
    else {
        // The plugin is given a frame rect which is parent()->contentsToWindow(frameRect()),
        // and un-translates by the its origin when painting. The current CTM reflects
        // this widget's frame is its parent (the document), so we have to offset the CTM by
        // the document's window coordinates.
        IntPoint documentOriginInWindowCoordinates = parent()->contentsToWindow(IntPoint());
        context->save();
        context->translate(-documentOriginInWindowCoordinates.x(), -documentOriginInWindowCoordinates.y());
        m_plugin->paint(context, paintRectInWindowCoordinates);
        context->restore();
    }
}

void PluginView::frameRectsChanged()
{
    Widget::frameRectsChanged();
    viewGeometryDidChange();
}

void PluginView::setParent(ScrollView* scrollView)
{
    Widget::setParent(scrollView);
    
    if (scrollView)
        initializePlugin();
}

void PluginView::handleEvent(Event* event)
{
    if (!m_isInitialized || !m_plugin)
        return;

    const WebEvent* currentEvent = WebPage::currentEvent();
    if (!currentEvent)
        return;

    bool didHandleEvent = false;

    if ((event->type() == eventNames().mousemoveEvent && currentEvent->type() == WebEvent::MouseMove)
        || (event->type() == eventNames().mousedownEvent && currentEvent->type() == WebEvent::MouseDown)
        || (event->type() == eventNames().mouseupEvent && currentEvent->type() == WebEvent::MouseUp)) {
        // We have a mouse event.
        if (currentEvent->type() == WebEvent::MouseDown)
            focusPluginElement();

        didHandleEvent = m_plugin->handleMouseEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    } else if (event->type() == eventNames().mousewheelEvent && currentEvent->type() == WebEvent::Wheel) {
        // We have a wheel event.
        didHandleEvent = m_plugin->handleWheelEvent(static_cast<const WebWheelEvent&>(*currentEvent));
    } else if (event->type() == eventNames().mouseoverEvent && currentEvent->type() == WebEvent::MouseMove) {
        // We have a mouse enter event.
        didHandleEvent = m_plugin->handleMouseEnterEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    } else if (event->type() == eventNames().mouseoutEvent && currentEvent->type() == WebEvent::MouseMove) {
        // We have a mouse leave event.
        didHandleEvent = m_plugin->handleMouseLeaveEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    } else if ((event->type() == eventNames().keydownEvent && currentEvent->type() == WebEvent::KeyDown)
               || (event->type() == eventNames().keyupEvent && currentEvent->type() == WebEvent::KeyUp)) {
        // We have a keyboard event.
        didHandleEvent = m_plugin->handleKeyboardEvent(static_cast<const WebKeyboardEvent&>(*currentEvent));
    }

    if (didHandleEvent)
        event->setDefaultHandled();
}

void PluginView::notifyWidget(WidgetNotification notification)
{
    switch (notification) {
    case WillPaintFlattened:
        if (m_plugin && m_isInitialized)
            m_snapshot = m_plugin->snapshot();
        break;
    case DidPaintFlattened:
        m_snapshot = nullptr;
        break;
    }
}

void PluginView::viewGeometryDidChange()
{
    if (!m_isInitialized || !m_plugin || !parent())
        return;

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());
    frameRectInWindowCoordinates.setSize(m_boundsSize);
    m_plugin->geometryDidChange(frameRectInWindowCoordinates, clipRectInWindowCoordinates());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    ASSERT(parent());

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());
    frameRectInWindowCoordinates.setSize(m_boundsSize);

    // Get the window clip rect for the enclosing layer (in window coordinates).
    RenderLayer* layer = m_pluginElement->renderer()->enclosingLayer();
    FrameView* parentView = m_pluginElement->document()->frame()->view();
    IntRect windowClipRect = parentView->windowClipRectForLayer(layer, true);

    // Intersect the two rects to get the view clip rect in window coordinates.
    return intersection(frameRectInWindowCoordinates, windowClipRect);
}

void PluginView::focusPluginElement()
{
    ASSERT(frame());
    
    if (Page* page = frame()->page())
        page->focusController()->setFocusedFrame(frame());
    frame()->document()->setFocusedNode(m_pluginElement);
}

void PluginView::pendingURLRequestsTimerFired()
{
    ASSERT(!m_pendingURLRequests.isEmpty());
    
    RefPtr<URLRequest> urlRequest = m_pendingURLRequests.takeFirst();

    // If there are more requests to perform, reschedule the timer.
    if (!m_pendingURLRequests.isEmpty())
        m_pendingURLRequestsTimer.startOneShot(0);
    
    performURLRequest(urlRequest.get());
}
    
void PluginView::performURLRequest(URLRequest* request)
{
    // First, check if this is a javascript: url.
    if (protocolIsJavaScript(request->request().url())) {
        performJavaScriptURLRequest(request);
        return;
    }

    if (!request->target().isNull()) {
        performFrameLoadURLRequest(request);
        return;
    }

    // This request is to load a URL and create a stream.
    RefPtr<Stream> stream = PluginView::Stream::create(this, request->requestID(), request->request());
    addStream(stream.get());
    stream->start();
}

void PluginView::performFrameLoadURLRequest(URLRequest* request)
{
    ASSERT(!request->target().isNull());

    Frame* frame = m_pluginElement->document()->frame();
    if (!frame)
        return;

    if (!m_pluginElement->document()->securityOrigin()->canDisplay(request->request().url())) {
        // We can't load the request, send back a reply to the plug-in.
        m_plugin->frameDidFail(request->requestID(), false);
        return;
    }

    // First, try to find a target frame.
    Frame* targetFrame = frame->loader()->findFrameForNavigation(request->target());
    if (!targetFrame) {
        // We did not find a target frame. Ask our frame to load the page. This may or may not create a popup window.
        frame->loader()->load(request->request(), request->target(), false);

        // FIXME: We don't know whether the window was successfully created here so we just assume that it worked.
        // It's better than not telling the plug-in anything.
        m_plugin->frameDidFinishLoading(request->requestID());
        return;
    }

    // Now ask the frame to load the request.
    targetFrame->loader()->load(request->request(), false);

    WebFrame* targetWebFrame = static_cast<WebFrameLoaderClient*>(targetFrame->loader()->client())->webFrame();
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
    ASSERT(protocolIsJavaScript(request->request().url()));

    RefPtr<Frame> frame = m_pluginElement->document()->frame();
    if (!frame)
        return;
    
    String jsString = decodeURLEscapeSequences(request->request().url().string().substring(sizeof("javascript:") - 1));

    if (!request->target().isNull()) {
        // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
        if (frame->tree()->find(request->target()) != frame) {
            // Let the plug-in know that its frame load failed.
            m_plugin->frameDidFail(request->requestID(), false);
            return;
        }
    }

    // Evaluate the JavaScript code. Note that running JavaScript here could cause the plug-in to be destroyed, so we
    // grab references to the plug-in here.
    RefPtr<Plugin> plugin = m_plugin;

    bool oldAllowPopups = frame->script()->allowPopupsFromPlugin();
    frame->script()->setAllowPopupsFromPlugin(request->allowPopups());
    
    ScriptValue result = frame->script()->executeScript(jsString);

    frame->script()->setAllowPopupsFromPlugin(oldAllowPopups);

    // Check if evaluating the JavaScript destroyed the plug-in.
    if (!plugin->controller())
        return;

    ScriptState* scriptState = frame->script()->globalObject(pluginWorld())->globalExec();
    String resultString;
    result.getString(scriptState, resultString);
  
    if (!request->target().isNull()) {
        // Just send back whether the frame load succeeded or not.
        if (resultString.isNull())
            m_plugin->frameDidFail(request->requestID(), false);
        else
            m_plugin->frameDidFinishLoading(request->requestID());
        return;
    }

    // Send the result back to the plug-in.
    plugin->didEvaluateJavaScript(request->requestID(), decodeURLEscapeSequences(request->request().url()), resultString);
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
    Vector<RefPtr<Stream> > streams;
    copyValuesToVector(m_streams, streams);
    
    for (size_t i = 0; i < streams.size(); ++i)
        streams[i]->cancel();

    // Cancelling a stream removes it from the m_streams map, so if we cancel all streams the map should be empty.
    ASSERT(m_streams.isEmpty());
}

void PluginView::redeliverManualStream()
{
    if (m_manualStreamState == StreamStateInitial) {
        // Nothing to do.
        return;
    }

    if (m_manualStreamState == StreamStateFailed) {
        manualLoadDidFail(m_manualStreamError);
        return;
    }

    // Deliver the response.
    manualLoadDidReceiveResponse(m_manualStreamResponse);

    // Deliver the data.
    if (m_manualStreamData) {
        const char* data;
        unsigned position = 0;

        while (unsigned length = m_manualStreamData->getSomeData(data, position)) {
            manualLoadDidReceiveData(data, length);
            position += length;
        }

        m_manualStreamData = nullptr;
    }

    if (m_manualStreamState == StreamStateFinished)
        manualLoadDidFinishLoading();
}

void PluginView::invalidateRect(const IntRect& dirtyRect)
{
    if (!parent() || !m_plugin || !m_isInitialized)
        return;

#if PLATFORM(MAC)
    if (m_plugin->pluginLayer())
        return;
#endif

    IntRect dirtyRectInWindowCoordinates = convertToContainingWindow(dirtyRect);

    parent()->hostWindow()->invalidateContentsAndWindow(intersection(dirtyRectInWindowCoordinates, clipRectInWindowCoordinates()), false);
}

void PluginView::setFocus(bool hasFocus)
{
    Widget::setFocus(hasFocus);

    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->setFocus(hasFocus);
}

void PluginView::mediaCanStart()
{
    ASSERT(m_isWaitingUntilMediaCanStart);
    m_isWaitingUntilMediaCanStart = false;
    
    initializePlugin();
}

void PluginView::invalidate(const IntRect& dirtyRect)
{
    invalidateRect(dirtyRect);
}

String PluginView::userAgent()
{
    Frame* frame = m_pluginElement->document()->frame();
    if (!frame)
        return String();
    
    return frame->loader()->client()->userAgent(KURL());
}

void PluginView::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, 
                         const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    FrameLoadRequest frameLoadRequest(m_pluginElement->document()->securityOrigin());
    frameLoadRequest.resourceRequest().setHTTPMethod(method);
    frameLoadRequest.resourceRequest().setURL(m_pluginElement->document()->completeURL(urlString));
    frameLoadRequest.resourceRequest().addHTTPHeaderFields(headerFields);
    frameLoadRequest.resourceRequest().setHTTPBody(FormData::create(httpBody.data(), httpBody.size()));
    frameLoadRequest.setFrameName(target);

    m_pendingURLRequests.append(URLRequest::create(requestID, frameLoadRequest, allowPopups));
    m_pendingURLRequestsTimer.startOneShot(0);
}

void PluginView::cancelStreamLoad(uint64_t streamID)
{
    // Keep a reference to the stream. Stream::cancel might remove the stream from the map, and thus
    // releasing its last reference.
    RefPtr<Stream> stream = m_streams.get(streamID).get();
    if (!stream)
        return;

    // Cancelling the stream here will remove it from the map.
    stream->cancel();
    ASSERT(!m_streams.contains(streamID));
}

void PluginView::cancelManualStreamLoad()
{
    if (!frame())
        return;

    DocumentLoader* documentLoader = frame()->loader()->activeDocumentLoader();
    ASSERT(documentLoader);
    
    if (documentLoader->isLoadingMainResource())
        documentLoader->cancelMainResourceLoad(frame()->loader()->cancelledError(m_parameters.url));
}

NPObject* PluginView::windowScriptNPObject()
{
    if (!frame())
        return 0;

    // FIXME: Handle JavaScript being disabled.
    ASSERT(frame()->script()->canExecuteScripts(NotAboutToExecuteScript));

    return m_npRuntimeObjectMap.getOrCreateNPObject(*pluginWorld()->globalData(), frame()->script()->windowShell(pluginWorld())->window());
}

NPObject* PluginView::pluginElementNPObject()
{
    if (!frame())
        return 0;

    // FIXME: Handle JavaScript being disabled.
    JSObject* object = frame()->script()->jsObjectForPluginElement(m_pluginElement.get());
    ASSERT(object);

    return m_npRuntimeObjectMap.getOrCreateNPObject(*pluginWorld()->globalData(), object);
}

bool PluginView::evaluate(NPObject* npObject, const String& scriptString, NPVariant* result, bool allowPopups)
{
    RefPtr<Frame> frame = m_pluginElement->document()->frame();
    if (!frame)
        return false;

    bool oldAllowPopups = frame->script()->allowPopupsFromPlugin();
    frame->script()->setAllowPopupsFromPlugin(allowPopups);

    // Calling evaluate will run JavaScript that can potentially remove the plug-in element, so we need to
    // protect the plug-in view from destruction.
    NPRuntimeObjectMap::PluginProtector pluginProtector(&m_npRuntimeObjectMap);

    bool returnValue = m_npRuntimeObjectMap.evaluate(npObject, scriptString, result);

    frame->script()->setAllowPopupsFromPlugin(oldAllowPopups);

    return returnValue;
}

void PluginView::setStatusbarText(const String& statusbarText)
{
    if (!frame())
        return;
    
    Page* page = frame()->page();
    if (!page)
        return;

    page->chrome()->setStatusbarText(frame(), statusbarText);
}

bool PluginView::isAcceleratedCompositingEnabled()
{
    if (!frame())
        return false;
    
    Settings* settings = frame()->settings();
    if (!settings)
        return false;

    return settings->acceleratedCompositingEnabled();
}

void PluginView::pluginProcessCrashed()
{
    if (!m_pluginElement->renderer())
        return;

    // FIXME: The renderer could also be a RenderApplet, we should handle that.
    if (!m_pluginElement->renderer()->isEmbeddedObject())
        return;
        
    RenderEmbeddedObject* renderer = toRenderEmbeddedObject(m_pluginElement->renderer());
    renderer->setShowsCrashedPluginIndicator();
    
    invalidateRect(frameRect());
}

#if PLATFORM(WIN)
HWND PluginView::nativeParentWindow()
{
    return m_webPage->nativeWindow();
}
#endif

#if PLATFORM(MAC)
void PluginView::setComplexTextInputEnabled(bool complexTextInputEnabled)
{
    m_webPage->send(Messages::WebPageProxy::SetComplexTextInputEnabled(m_plugin->pluginComplexTextInputIdentifier(), complexTextInputEnabled));
}

mach_port_t PluginView::compositingRenderServerPort()
{
    return WebProcess::shared().compositingRenderServerPort();
}

#endif
    
String PluginView::proxiesForURL(const String& urlString)
{
    const FrameLoader* frameLoader = frame() ? frame()->loader() : 0;
    const NetworkingContext* context = frameLoader ? frameLoader->networkingContext() : 0;
    Vector<ProxyServer> proxyServers = proxyServersForURL(KURL(KURL(), urlString), context);
    return toString(proxyServers);
}

String PluginView::cookiesForURL(const String& urlString)
{
    return cookies(m_pluginElement->document(), KURL(KURL(), urlString));
}

void PluginView::setCookiesForURL(const String& urlString, const String& cookieString)
{
    setCookies(m_pluginElement->document(), KURL(KURL(), urlString), cookieString);
}

bool PluginView::isPrivateBrowsingEnabled()
{
    // If we can't get the real setting, we'll assume that private browsing is enabled.
    if (!frame())
        return true;

    Settings* settings = frame()->settings();
    if (!settings)
        return true;

    return settings->privateBrowsingEnabled();
}

void PluginView::protectPluginFromDestruction()
{
    if (!m_isBeingDestroyed)
        ref();
}

void PluginView::unprotectPluginFromDestruction()
{
    if (!m_isBeingDestroyed)
        deref();
}

void PluginView::didFinishLoad(WebFrame* webFrame)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(0);

    m_plugin->frameDidFinishLoading(request->requestID());
}

void PluginView::didFailLoad(WebFrame* webFrame, bool wasCancelled)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(0);
    
    m_plugin->frameDidFail(request->requestID(), wasCancelled);
}

} // namespace WebKit
