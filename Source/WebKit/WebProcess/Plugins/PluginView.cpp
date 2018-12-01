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

#include "NPRuntimeUtilities.h"
#include "Plugin.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebEvent.h"
#include "WebLoaderStrategy.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
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
#include <WebCore/HTMLPlugInImageElement.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/HostWindow.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ProxyServer.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <WebCore/ScriptController.h>
#include <WebCore/ScrollView.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/Settings.h>
#include <WebCore/TextEncoding.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(X11)
#include <WebCore/PlatformDisplay.h>
#endif

namespace WebKit {
using namespace JSC;
using namespace WebCore;

// This simulated mouse click delay in HTMLPlugInImageElement.cpp should generally be the same or shorter than this delay.
static const Seconds pluginSnapshotTimerDelay { 1100_ms };

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
    void didReceiveData(NetscapePlugInStreamLoader*, const char*, int) override;
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

    WebProcess::singleton().webLoaderStrategy().schedulePluginStreamLoad(*frame, *this, ResourceRequest {m_request}, [this, protectedThis = makeRef(*this)](RefPtr<NetscapePlugInStreamLoader>&& loader) {
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
    if (!response.isHTTP())
        return String();

    StringBuilder header;
    header.appendLiteral("HTTP ");
    header.appendNumber(response.httpStatusCode());
    header.append(' ');
    header.append(response.httpStatusText());
    header.append('\n');
    for (auto& field : response.httpHeaderFields()) {
        header.append(field.key);
        header.appendLiteral(": ");
        header.append(field.value);
        header.append('\n');
    }

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

void PluginView::Stream::didReceiveData(NetscapePlugInStreamLoader*, const char* bytes, int length)
{
    m_pluginView->m_plugin->streamDidReceiveData(m_streamID, bytes, length);
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

#if ENABLE(NETSCAPE_PLUGIN_API)
    // Protect the plug-in while we're calling into it.
    NPRuntimeObjectMap::PluginProtector pluginProtector(&m_pluginView->m_npRuntimeObjectMap);
#endif
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
    , m_pluginSnapshotTimer(*this, &PluginView::pluginSnapshotTimerFired, pluginSnapshotTimerDelay)
{
    m_webPage->addPluginView(this);
}

PluginView::~PluginView()
{
    if (m_webPage)
        m_webPage->removePluginView(this);

    ASSERT(!m_plugin || !m_plugin->isBeingDestroyed());

    if (m_isWaitingUntilMediaCanStart)
        m_pluginElement->document().removeMediaCanStartListener(this);

    m_pluginElement->document().removeAudioProducer(this);

    destroyPluginAndReset();

    // Null out the plug-in element explicitly so we'll crash earlier if we try to use
    // the plug-in view after it's been destroyed.
    m_pluginElement = nullptr;
}

void PluginView::destroyPluginAndReset()
{
    // Cancel all pending frame loads.
    for (FrameLoadMap::iterator it = m_pendingFrameLoads.begin(), end = m_pendingFrameLoads.end(); it != end; ++it)
        it->key->setLoadListener(0);

    if (m_plugin) {
        m_plugin->destroyPlugin();

        m_pendingURLRequests.clear();
        m_pendingURLRequestsTimer.stop();

#if PLATFORM(COCOA)
        if (m_webPage)
            pluginFocusOrWindowFocusChanged(false);
#endif
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    // Invalidate the object map.
    m_npRuntimeObjectMap.invalidate();
#endif

    cancelAllStreams();
}

void PluginView::recreateAndInitialize(Ref<Plugin>&& plugin)
{
    if (m_plugin) {
        if (m_pluginSnapshotTimer.isActive())
            m_pluginSnapshotTimer.stop();
        destroyPluginAndReset();
    }

    m_plugin = WTFMove(plugin);
    m_isInitialized = false;
    m_isWaitingForSynchronousInitialization = false;
    m_isWaitingUntilMediaCanStart = false;
    m_manualStreamState = ManualStreamState::Initial;
    m_transientPaintingSnapshot = nullptr;

    initializePlugin();
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

void PluginView::manualLoadDidReceiveData(const char* bytes, int length)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_plugin)
        return;

    if (!m_isInitialized) {
        ASSERT(m_manualStreamState == ManualStreamState::HasReceivedResponse);
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
        m_manualStreamData = nullptr;
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

bool PluginView::sendComplexTextInput(uint64_t pluginComplexTextInputIdentifier, const String& textInput)
{
    if (!m_plugin)
        return false;

    if (m_plugin->pluginComplexTextInputIdentifier() != pluginComplexTextInputIdentifier)
        return false;

    m_plugin->sendComplexTextInput(textInput);
    return true;
}
    
id PluginView::accessibilityAssociatedPluginParentForElement(Element* element) const
{
    if (!m_plugin)
        return nil;
    return m_plugin->accessibilityAssociatedPluginParentForElement(element);
}
    
NSObject *PluginView::accessibilityObject() const
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
                m_pluginElement->document().addMediaCanStartListener(this);
                return;
            }
        }
    }

    m_pluginElement->document().addAudioProducer(this);

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    HTMLPlugInImageElement& plugInImageElement = downcast<HTMLPlugInImageElement>(*m_pluginElement);
    m_didPlugInStartOffScreen = !m_webPage->plugInIntersectsSearchRect(plugInImageElement);
#endif
    m_plugin->initialize(this, m_parameters);
    
    // Plug-in initialization continued in didFailToInitializePlugin() or didInitializePlugin().
}

void PluginView::didFailToInitializePlugin()
{
    m_plugin = nullptr;

#if ENABLE(NETSCAPE_PLUGIN_API)
    String frameURLString = frame()->loader().documentLoader()->responseURL().string();
    String pageURLString = m_webPage->corePage()->mainFrame().loader().documentLoader()->responseURL().string();
    m_webPage->send(Messages::WebPageProxy::DidFailToInitializePlugin(m_parameters.mimeType, frameURLString, pageURLString));
#endif // ENABLE(NETSCAPE_PLUGIN_API)
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
    if (m_pluginElement->displayState() < HTMLPlugInElement::Restarting) {
        if (m_plugin->pluginLayer() && frame()) {
            frame()->view()->enterCompositingMode();
            m_pluginElement->invalidateStyleAndLayerComposition();
        }
        if (frame() && !frame()->settings().maximumPlugInSnapshotAttempts()) {
            beginSnapshottingRunningPlugin();
            return;
        }
        m_pluginSnapshotTimer.restart();
    } else {
        if (m_plugin->pluginLayer() && frame()) {
            frame()->view()->enterCompositingMode();
            m_pluginElement->invalidateStyleAndLayerComposition();
        }
        if (m_pluginElement->displayState() == HTMLPlugInElement::RestartingWithPendingMouseClick)
            m_pluginElement->dispatchPendingMouseClick();
    }

    m_plugin->visibilityDidChange(isVisible());
    m_plugin->windowVisibilityChanged(m_webPage->isVisibleOrOccluded());
    m_plugin->windowFocusChanged(m_webPage->windowIsFocused());
#endif

    if (wantsWheelEvents()) {
        if (Frame* frame = m_pluginElement->document().frame()) {
            if (FrameView* frameView = frame->view())
                frameView->setNeedsLayout();
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
    if (!m_isInitialized || !m_plugin || m_pluginProcessHasCrashed)
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

#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* scriptableNPObject = m_plugin->pluginScriptableNPObject();
    if (!scriptableNPObject)
        return 0;

    JSObject* jsObject = m_npRuntimeObjectMap.getOrCreateJSObject(globalObject, scriptableNPObject);
    releaseNPObject(scriptableNPObject);

    return jsObject;
#else
    UNUSED_PARAM(globalObject);
    return 0;
#endif
}

void PluginView::storageBlockingStateChanged()
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return;

    bool storageBlockingPolicy = !frame()->document()->securityOrigin().canAccessPluginStorage(frame()->document()->topOrigin());

    m_plugin->storageBlockingStateChanged(storageBlockingPolicy);
}

void PluginView::privateBrowsingStateChanged(bool privateBrowsingEnabled)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->privateBrowsingStateChanged(privateBrowsingEnabled);
}

bool PluginView::getFormValue(String& formValue)
{
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->getFormValue(formValue);
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

void PluginView::paint(GraphicsContext& context, const IntRect& /*dirtyRect*/, Widget::SecurityOriginPaintPolicy)
{
    if (!m_plugin || !m_isInitialized || m_pluginElement->displayState() < HTMLPlugInElement::Restarting)
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

    unsigned modifiers = 0;
    if (event.shiftKey())
        modifiers |= WebEvent::ShiftKey;
    if (event.ctrlKey())
        modifiers |= WebEvent::ControlKey;
    if (event.altKey())
        modifiers |= WebEvent::AltKey;
    if (event.metaKey())
        modifiers |= WebEvent::MetaKey;

    return std::make_unique<WebMouseEvent>(type, button, event.buttons(), m_plugin->convertToRootView(IntPoint(event.offsetX(), event.offsetY())), event.screenLocation(), 0, 0, 0, clickCount, static_cast<WebEvent::Modifiers>(modifiers), WallTime { }, 0);
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
        if (event.type() != eventNames().mousemoveEvent)
            pluginDidReceiveUserInteraction();
    } else if (eventNames().isWheelEventType(event.type())
        && currentEvent->type() == WebEvent::Wheel && m_plugin->wantsWheelEvents()) {
        didHandleEvent = m_plugin->handleWheelEvent(static_cast<const WebWheelEvent&>(*currentEvent));
        pluginDidReceiveUserInteraction();
    } else if (event.type() == eventNames().mouseoverEvent && currentEvent->type() == WebEvent::MouseMove)
        didHandleEvent = m_plugin->handleMouseEnterEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if (event.type() == eventNames().mouseoutEvent && currentEvent->type() == WebEvent::MouseMove)
        didHandleEvent = m_plugin->handleMouseLeaveEvent(static_cast<const WebMouseEvent&>(*currentEvent));
    else if (event.type() == eventNames().contextmenuEvent && currentEvent->type() == WebEvent::MouseDown) {
        didHandleEvent = m_plugin->handleContextMenuEvent(static_cast<const WebMouseEvent&>(*currentEvent));
        pluginDidReceiveUserInteraction();
    } else if ((event.type() == eventNames().keydownEvent && currentEvent->type() == WebEvent::KeyDown)
               || (event.type() == eventNames().keyupEvent && currentEvent->type() == WebEvent::KeyUp)) {
        didHandleEvent = m_plugin->handleKeyboardEvent(static_cast<const WebKeyboardEvent&>(*currentEvent));
        pluginDidReceiveUserInteraction();
    }

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

bool PluginView::shouldNotAddLayer() const
{
    return m_pluginElement->displayState() < HTMLPlugInElement::Restarting && !m_plugin->supportsSnapshotting();
}

void PluginView::willDetachRenderer()
{
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->willDetachRenderer();
}

RefPtr<SharedBuffer> PluginView::liveResourceData() const
{
    if (!m_isInitialized || !m_plugin)
        return 0;

    return m_plugin->liveResourceData();
}

bool PluginView::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!m_isInitialized || !m_plugin)
        return false;

    return m_plugin->performDictionaryLookupAtLocation(point);
}

String PluginView::getSelectionForWordAtPoint(const WebCore::FloatPoint& point) const
{
    if (!m_isInitialized || !m_plugin)
        return String();
    
    return m_plugin->getSelectionForWordAtPoint(point);
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
        page->focusController().setFocusedElement(m_pluginElement.get(), *frame());
    else
        frame()->document()->setFocusedElement(m_pluginElement.get());
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
    if (WTF::protocolIsJavaScript(request->request().url())) {
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
        FrameLoadRequest frameLoadRequest { *frame, request->request(), ShouldOpenExternalURLsPolicy::ShouldNotAllow };
        frameLoadRequest.setFrameName(request->target());
        frameLoadRequest.setShouldCheckNewWindowPolicy(true);
        frame->loader().load(WTFMove(frameLoadRequest));

        // FIXME: We don't know whether the window was successfully created here so we just assume that it worked.
        // It's better than not telling the plug-in anything.
        m_plugin->frameDidFinishLoading(request->requestID());
        return;
    }

    // Now ask the frame to load the request.
    targetFrame->loader().load(FrameLoadRequest(*targetFrame, request->request(), ShouldOpenExternalURLsPolicy::ShouldNotAllow));

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
    ASSERT(WTF::protocolIsJavaScript(request->request().url()));

    RefPtr<Frame> frame = m_pluginElement->document().frame();
    if (!frame)
        return;
    
    String jsString = decodeURLEscapeSequences(request->request().url().string().substring(sizeof("javascript:") - 1));

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
    auto result = frame->script().executeScript(jsString, request->allowPopups());

    if (!result)
        return;

    // Check if evaluating the JavaScript destroyed the plug-in.
    if (!plugin->controller())
        return;

    // Don't notify the plug-in at all about targeted javascript: requests. This matches Mozilla and WebKit1.
    if (!request->target().isNull())
        return;

    ExecState* scriptState = frame->script().globalObject(pluginWorld())->globalExec();
    String resultString;
    result.getString(scriptState, resultString);
  
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
        for (const auto& element : *m_manualStreamData)
            manualLoadDidReceiveData(element.segment->data(), element.segment->size());
        m_manualStreamData = nullptr;
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

    if (m_pluginElement->displayState() < HTMLPlugInElement::Restarting)
        return;

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
#if ENABLE(NETSCAPE_PLUGIN_API)
    // The plug-in can be null here if it failed to initialize.
    if (!m_isInitialized || !m_plugin)
        return;

    m_plugin->mutedStateChanged(isMuted());
#endif
}

void PluginView::invalidate(const IntRect& dirtyRect)
{
    invalidateRect(dirtyRect);
}

String PluginView::userAgent()
{
    Frame* frame = m_pluginElement->document().frame();
    if (!frame)
        return String();
    
    return frame->loader().client().userAgent(URL());
}

void PluginView::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    FrameLoadRequest frameLoadRequest { m_pluginElement->document(), m_pluginElement->document().securityOrigin(), { }, target, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ShouldOpenExternalURLsPolicy::ShouldNotAllow, InitiatedByMainFrame::Unknown };
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

void PluginView::cancelStreamLoad(uint64_t streamID)
{
    // Keep a reference to the stream. Stream::cancel might remove the stream from the map, and thus
    // releasing its last reference.
    RefPtr<Stream> stream = m_streams.get(streamID);
    if (!stream)
        return;

    // Cancelling the stream here will remove it from the map.
    stream->cancel();
    ASSERT(!m_streams.contains(streamID));
}

void PluginView::continueStreamLoad(uint64_t streamID)
{
    RefPtr<Stream> stream = m_streams.get(streamID);
    if (!stream)
        return;

    stream->continueLoad();
}

void PluginView::cancelManualStreamLoad()
{
    if (!frame())
        return;

    DocumentLoader* documentLoader = frame()->loader().activeDocumentLoader();
    ASSERT(documentLoader);
    
    if (documentLoader->isLoadingMainResource())
        documentLoader->cancelMainResourceLoad(frame()->loader().cancelledError(m_parameters.url));
}

#if ENABLE(NETSCAPE_PLUGIN_API)
NPObject* PluginView::windowScriptNPObject()
{
    if (!frame())
        return nullptr;

    if (!frame()->script().canExecuteScripts(NotAboutToExecuteScript)) {
        // FIXME: Investigate if other browsers allow plug-ins to access JavaScript objects even if JavaScript is disabled.
        return nullptr;
    }

    return m_npRuntimeObjectMap.getOrCreateNPObject(pluginWorld().vm(), frame()->windowProxy().jsWindowProxy(pluginWorld())->window());
}

NPObject* PluginView::pluginElementNPObject()
{
    if (!frame())
        return 0;

    if (!frame()->script().canExecuteScripts(NotAboutToExecuteScript)) {
        // FIXME: Investigate if other browsers allow plug-ins to access JavaScript objects even if JavaScript is disabled.
        return 0;
    }

    JSObject* object = frame()->script().jsObjectForPluginElement(m_pluginElement.get());
    ASSERT(object);

    return m_npRuntimeObjectMap.getOrCreateNPObject(pluginWorld().vm(), object);
}

bool PluginView::evaluate(NPObject* npObject, const String& scriptString, NPVariant* result, bool allowPopups)
{
    // FIXME: Is this check necessary?
    if (!m_pluginElement->document().frame())
        return false;

    // Calling evaluate will run JavaScript that can potentially remove the plug-in element, so we need to
    // protect the plug-in view from destruction.
    NPRuntimeObjectMap::PluginProtector pluginProtector(&m_npRuntimeObjectMap);

    UserGestureIndicator gestureIndicator(allowPopups ? std::optional<ProcessingUserGestureState>(ProcessingUserGesture) : std::nullopt);
    return m_npRuntimeObjectMap.evaluate(npObject, scriptString, result);
}

void PluginView::setPluginIsPlayingAudio(bool pluginIsPlayingAudio)
{
    if (m_pluginIsPlayingAudio == pluginIsPlayingAudio)
        return;

    m_pluginIsPlayingAudio = pluginIsPlayingAudio;
    m_pluginElement->document().updateIsPlayingMedia();
}

bool PluginView::isMuted() const
{
    if (!frame() || !frame()->page())
        return false;

    return frame()->page()->isAudioMuted();
}
#endif

void PluginView::setStatusbarText(const String& statusbarText)
{
    if (!frame())
        return;
    
    Page* page = frame()->page();
    if (!page)
        return;

    page->chrome().setStatusbarText(*frame(), statusbarText);
}

bool PluginView::isAcceleratedCompositingEnabled()
{
    if (!frame())
        return false;
    
    // We know that some plug-ins can support snapshotting without needing
    // accelerated compositing. Since we're trying to snapshot them anyway,
    // put them into normal compositing mode. A side benefit is that this might
    // allow the entire page to stay in that mode.
    if (m_pluginElement->displayState() < HTMLPlugInElement::Restarting && m_parameters.mimeType == "application/x-shockwave-flash")
        return false;

    return frame()->settings().acceleratedCompositingEnabled();
}

void PluginView::pluginProcessCrashed()
{
    m_pluginProcessHasCrashed = true;

    auto* renderer = m_pluginElement->renderer();
    if (!is<RenderEmbeddedObject>(renderer))
        return;

    m_pluginElement->invalidateStyleAndLayerComposition();

    downcast<RenderEmbeddedObject>(*renderer).setPluginUnavailabilityReason(RenderEmbeddedObject::PluginCrashed);

    Widget::invalidate();
}

#if PLATFORM(COCOA)
void PluginView::pluginFocusOrWindowFocusChanged(bool pluginHasFocusAndWindowHasFocus)
{
    if (m_webPage)
        m_webPage->send(Messages::WebPageProxy::PluginFocusOrWindowFocusChanged(m_plugin->pluginComplexTextInputIdentifier(), pluginHasFocusAndWindowHasFocus));
}

void PluginView::setComplexTextInputState(PluginComplexTextInputState pluginComplexTextInputState)
{
    if (m_webPage)
        m_webPage->send(Messages::WebPageProxy::SetPluginComplexTextInputState(m_plugin->pluginComplexTextInputIdentifier(), pluginComplexTextInputState));
}

const MachSendRight& PluginView::compositingRenderServerPort()
{
    return WebProcess::singleton().compositingRenderServerPort();
}

#endif

float PluginView::contentsScaleFactor()
{
    if (Page* page = frame() ? frame()->page() : 0)
        return page->deviceScaleFactor();
        
    return 1;
}
    
String PluginView::proxiesForURL(const String& urlString)
{
    Vector<ProxyServer> proxyServers = proxyServersForURL(URL(URL(), urlString));
    return toString(proxyServers);
}

String PluginView::cookiesForURL(const String& urlString)
{
    return cookies(m_pluginElement->document(), URL(URL(), urlString));
}

void PluginView::setCookiesForURL(const String& urlString, const String& cookieString)
{
    setCookies(m_pluginElement->document(), URL(URL(), urlString), cookieString);
}

bool PluginView::getAuthenticationInfo(const ProtectionSpace& protectionSpace, String& username, String& password)
{
    auto* contentDocument = m_pluginElement->contentDocument();
    if (!contentDocument)
        return false;

    String partitionName = contentDocument->topDocument().domainForCachePartition();
    Credential credential = CredentialStorage::defaultCredentialStorage().get(partitionName, protectionSpace);
    if (credential.isEmpty())
        credential = CredentialStorage::defaultCredentialStorage().getFromPersistentStorage(protectionSpace);

    if (!credential.hasPassword())
        return false;

    username = credential.user();
    password = credential.password();

    return true;
}

bool PluginView::isPrivateBrowsingEnabled()
{
    // If we can't get the real setting, we'll assume that private browsing is enabled.
    if (!frame())
        return true;

    if (!frame()->document()->securityOrigin().canAccessPluginStorage(frame()->document()->topOrigin()))
        return true;

    return frame()->page()->usesEphemeralSession();
}

bool PluginView::asynchronousPluginInitializationEnabled() const
{
    return m_webPage->asynchronousPluginInitializationEnabled();
}

bool PluginView::asynchronousPluginInitializationEnabledForAllPlugins() const
{
    return m_webPage->asynchronousPluginInitializationEnabledForAllPlugins();
}

bool PluginView::artificialPluginInitializationDelayEnabled() const
{
    return m_webPage->artificialPluginInitializationDelayEnabled();
}

void PluginView::protectPluginFromDestruction()
{
    if (m_plugin && !m_plugin->isBeingDestroyed())
        ref();
}

void PluginView::unprotectPluginFromDestruction()
{
    if (!m_plugin || m_plugin->isBeingDestroyed())
        return;

    // A plug-in may ask us to evaluate JavaScript that removes the plug-in from the
    // page, but expect the object to still be alive when the call completes. Flash,
    // for example, may crash if the plug-in is destroyed and we return to code for
    // the destroyed object higher on the stack. To prevent this, if the plug-in has
    // only one remaining reference, call deref() asynchronously.
    if (hasOneRef()) {
        RunLoop::main().dispatch([lastRef = adoptRef(*this)] {
        });
        return;
    }

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

#if PLATFORM(X11)
uint64_t PluginView::createPluginContainer()
{
    uint64_t windowID = 0;
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11)
        m_webPage->sendSync(Messages::WebPageProxy::CreatePluginContainer(), Messages::WebPageProxy::CreatePluginContainer::Reply(windowID));
    return windowID;
}

void PluginView::windowedPluginGeometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, uint64_t windowID)
{
    m_webPage->send(Messages::WebPageProxy::WindowedPluginGeometryDidChange(frameRect, clipRect, windowID));
}

void PluginView::windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID)
{
    m_webPage->send(Messages::WebPageProxy::WindowedPluginVisibilityDidChange(isVisible, windowID));
}
#endif

#if PLATFORM(COCOA)
static bool isAlmostSolidColor(BitmapImage* bitmap)
{
    CGImageRef image = bitmap->nativeImage().get();
    ASSERT(CGImageGetBitsPerComponent(image) == 8);

    CGBitmapInfo imageInfo = CGImageGetBitmapInfo(image);
    if (!(imageInfo & kCGBitmapByteOrder32Little) || (imageInfo & kCGBitmapAlphaInfoMask) != kCGImageAlphaPremultipliedFirst) {
        // FIXME: Consider being able to handle other pixel formats.
        ASSERT_NOT_REACHED();
        return false;
    }

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    size_t bytesPerRow = CGImageGetBytesPerRow(image);

    RetainPtr<CFDataRef> provider = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(image)));
    const UInt8* data = CFDataGetBytePtr(provider.get());

    // Overlay a grid of sampling dots on top of a grayscale version of the image.
    // For the interior points, calculate the difference in luminance among the sample point
    // and its surrounds points, scaled by transparency.
    const unsigned sampleRows = 7;
    const unsigned sampleCols = 7;
    // FIXME: Refine the proper number of samples, and accommodate different aspect ratios.
    if (width < sampleCols || height < sampleRows)
        return false;

    // Ensure that the last row/column land on the image perimeter.
    const float strideWidth = static_cast<float>(width - 1) / (sampleCols - 1);
    const float strideHeight = static_cast<float>(height - 1) / (sampleRows - 1);
    float samples[sampleRows][sampleCols];

    // Find the luminance of the sample points.
    float y = 0;
    const UInt8* row = data;
    for (unsigned i = 0; i < sampleRows; ++i) {
        float x = 0;
        for (unsigned j = 0; j < sampleCols; ++j) {
            const UInt8* p0 = row + (static_cast<int>(x + .5)) * 4;
            // R G B A
            samples[i][j] = (0.2125 * *p0 + 0.7154 * *(p0+1) + 0.0721 * *(p0+2)) * *(p0+3) / 255;
            x += strideWidth;
        }
        y += strideHeight;
        row = data + (static_cast<int>(y + .5)) * bytesPerRow;
    }

    // Determine the image score.
    float accumScore = 0;
    for (unsigned i = 1; i < sampleRows - 1; ++i) {
        for (unsigned j = 1; j < sampleCols - 1; ++j) {
            float diff = samples[i - 1][j] + samples[i + 1][j] + samples[i][j - 1] + samples[i][j + 1] - 4 * samples[i][j];
            accumScore += diff * diff;
        }
    }

    // The score for a given sample can be within the range of 0 and 255^2.
    return accumScore < 2500 * (sampleRows - 2) * (sampleCols - 2);
}
#endif

void PluginView::pluginSnapshotTimerFired()
{
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    HTMLPlugInImageElement& plugInImageElement = downcast<HTMLPlugInImageElement>(*m_pluginElement);
    bool isPlugInOnScreen = m_webPage->plugInIntersectsSearchRect(plugInImageElement);
    bool plugInCameOnScreen = isPlugInOnScreen && m_didPlugInStartOffScreen;
    bool snapshotFound = false;
#endif

    if (m_plugin && m_plugin->supportsSnapshotting()) {
        // Snapshot might be 0 if plugin size is 0x0.
        RefPtr<ShareableBitmap> snapshot = m_plugin->snapshot();
        RefPtr<Image> snapshotImage;
        if (snapshot)
            snapshotImage = snapshot->createImage();
        m_pluginElement->updateSnapshot(snapshotImage.get());

        if (snapshotImage) {
#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
            bool snapshotIsAlmostSolidColor = isAlmostSolidColor(downcast<BitmapImage>(snapshotImage.get()));
            snapshotFound = !snapshotIsAlmostSolidColor;
#endif

#if PLATFORM(COCOA)
            unsigned maximumSnapshotRetries = frame() ? frame()->settings().maximumPlugInSnapshotAttempts() : 0;
            if (snapshotIsAlmostSolidColor && m_countSnapshotRetries < maximumSnapshotRetries && !plugInCameOnScreen) {
                ++m_countSnapshotRetries;
                m_pluginSnapshotTimer.restart();
                return;
            }
#endif
        }
    }

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    unsigned candidateArea = 0;
    unsigned maximumSnapshotRetries = frame() ? frame()->settings().maximumPlugInSnapshotAttempts() : 0;
    bool noSnapshotFoundAfterMaxRetries = m_countSnapshotRetries == maximumSnapshotRetries && !isPlugInOnScreen && !snapshotFound;
    if (m_webPage->plugInIsPrimarySize(plugInImageElement, candidateArea)
        && (noSnapshotFoundAfterMaxRetries || plugInCameOnScreen))
        m_pluginElement->setDisplayState(HTMLPlugInElement::Playing);
    else
#endif
        m_pluginElement->setDisplayState(HTMLPlugInElement::DisplayingSnapshot);
}

void PluginView::beginSnapshottingRunningPlugin()
{
    m_pluginSnapshotTimer.restart();
}

bool PluginView::shouldAlwaysAutoStart() const
{
    if (!m_plugin)
        return PluginViewBase::shouldAlwaysAutoStart();

    if (MIMETypeRegistry::isJavaAppletMIMEType(m_parameters.mimeType))
        return true;

    return m_plugin->shouldAlwaysAutoStart();
}

void PluginView::pluginDidReceiveUserInteraction()
{
    if (frame() && !frame()->settings().plugInSnapshottingEnabled())
        return;

    if (m_didReceiveUserInteraction)
        return;

    m_didReceiveUserInteraction = true;

    HTMLPlugInImageElement& plugInImageElement = downcast<HTMLPlugInImageElement>(*m_pluginElement);
    String pageOrigin = plugInImageElement.document().page()->mainFrame().document()->baseURL().host().toString();
    String pluginOrigin = plugInImageElement.loadedUrl().host().toString();
    String mimeType = plugInImageElement.serviceType();

    WebProcess::singleton().plugInDidReceiveUserInteraction(pageOrigin, pluginOrigin, mimeType, plugInImageElement.document().page()->sessionID());
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

    if (!m_plugin->canCreateTransientPaintingSnapshot())
        return false;

    return true;
}

} // namespace WebKit
