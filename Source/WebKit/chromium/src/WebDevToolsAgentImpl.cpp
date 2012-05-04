/*
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebDevToolsAgentImpl.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "InjectedScriptHost.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "InspectorInstrumentation.h"
#include "InspectorProtocolVersion.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PageGroup.h"
#include "PageScriptDebugServer.h"
#include "painting/GraphicsContextBuilder.h"
#include "PlatformString.h"
#include "RenderView.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WebDataSource.h"
#include "WebDevToolsAgentClient.h"
#include "WebFrameImpl.h"
#include "platform/WebRect.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebURLError.h"
#include "platform/WebURLRequest.h"
#include "platform/WebURLResponse.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

using namespace WebCore;
using namespace std;

namespace OverlayZOrders {
static const int viewportGutter = 97;

// Use 99 as a big z-order number so that highlight is above other overlays.
static const int highlight = 99;
}

namespace WebKit {

class ClientMessageLoopAdapter : public PageScriptDebugServer::ClientMessageLoop {
public:
    static void ensureClientMessageLoopCreated(WebDevToolsAgentClient* client)
    {
        if (s_instance)
            return;
        OwnPtr<ClientMessageLoopAdapter> instance = adoptPtr(new ClientMessageLoopAdapter(adoptPtr(client->createClientMessageLoop())));
        s_instance = instance.get();
        PageScriptDebugServer::shared().setClientMessageLoop(instance.release());
    }

    static void inspectedViewClosed(WebViewImpl* view)
    {
        if (s_instance)
            s_instance->m_frozenViews.remove(view);
    }

    static void didNavigate()
    {
        // Release render thread if necessary.
        if (s_instance && s_instance->m_running)
            PageScriptDebugServer::shared().continueProgram();
    }

private:
    ClientMessageLoopAdapter(PassOwnPtr<WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop> messageLoop)
        : m_running(false)
        , m_messageLoop(messageLoop) { }


    virtual void run(Page* page)
    {
        if (m_running)
            return;
        m_running = true;

        Vector<WebViewImpl*> views;

        // 1. Disable input events.
        HashSet<Page*>::const_iterator end =  page->group().pages().end();
        for (HashSet<Page*>::const_iterator it =  page->group().pages().begin(); it != end; ++it) {
            WebViewImpl* view = WebViewImpl::fromPage(*it);
            m_frozenViews.add(view);
            views.append(view);
            view->setIgnoreInputEvents(true);
        }

        // 2. Disable active objects
        WebView::willEnterModalLoop();

        // 3. Process messages until quitNow is called.
        m_messageLoop->run();

        // 4. Resume active objects
        WebView::didExitModalLoop();

        // 5. Resume input events.
        for (Vector<WebViewImpl*>::iterator it = views.begin(); it != views.end(); ++it) {
            if (m_frozenViews.contains(*it)) {
                // The view was not closed during the dispatch.
                (*it)->setIgnoreInputEvents(false);
            }
        }

        // 6. All views have been resumed, clear the set.
        m_frozenViews.clear();

        m_running = false;
    }

    virtual void quitNow()
    {
        m_messageLoop->quitNow();
    }

    bool m_running;
    OwnPtr<WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop> m_messageLoop;
    typedef HashSet<WebViewImpl*> FrozenViewsSet;
    FrozenViewsSet m_frozenViews;
    // FIXME: The ownership model for s_instance is somewhat complicated. Can we make this simpler?
    static ClientMessageLoopAdapter* s_instance;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::s_instance = 0;

class DebuggerTask : public PageScriptDebugServer::Task {
public:
    DebuggerTask(PassOwnPtr<WebDevToolsAgent::MessageDescriptor> descriptor)
        : m_descriptor(descriptor)
    {
    }

    virtual ~DebuggerTask() { }
    virtual void run()
    {
        if (WebDevToolsAgent* webagent = m_descriptor->agent())
            webagent->dispatchOnInspectorBackend(m_descriptor->message());
    }

private:
    OwnPtr<WebDevToolsAgent::MessageDescriptor> m_descriptor;
};

class DeviceMetricsSupport : public WebPageOverlay {
public:
    DeviceMetricsSupport(WebViewImpl* webView)
        : m_webView(webView)
        , m_fitWindow(false)
        , m_originalZoomFactor(0)
    {
        m_webView->addPageOverlay(this, OverlayZOrders::viewportGutter);
    }

    ~DeviceMetricsSupport()
    {
        restore();
        m_webView->removePageOverlay(this);
    }

    void setDeviceMetrics(int width, int height, float textZoomFactor, bool fitWindow)
    {
        WebCore::FrameView* view = frameView();
        if (!view)
            return;

        m_emulatedFrameSize = WebSize(width, height);
        m_fitWindow = fitWindow;
        m_originalZoomFactor = 0;
        m_webView->setEmulatedTextZoomFactor(textZoomFactor);
        applySizeOverrideInternal(view, FitWindowAllowed);
        autoZoomPageToFitWidth(view->frame());

        m_webView->sendResizeEventAndRepaint();
    }

    void autoZoomPageToFitWidthOnNavigation(Frame* frame)
    {
        FrameView* frameView = frame->view();
        applySizeOverrideInternal(frameView, FitWindowNotAllowed);
        m_originalZoomFactor = 0;
        applySizeOverrideInternal(frameView, FitWindowAllowed);
        autoZoomPageToFitWidth(frame);
    }

    void autoZoomPageToFitWidth(Frame* frame)
    {
        if (!frame)
            return;

        frame->setTextZoomFactor(m_webView->emulatedTextZoomFactor());
        WebSize scaledFrameSize = scaledEmulatedFrameSize(frame->view());
        ensureOriginalZoomFactor(frame->view());
        double sizeRatio = static_cast<double>(scaledFrameSize.width) / m_emulatedFrameSize.width;
        frame->setPageAndTextZoomFactors(sizeRatio * m_originalZoomFactor, m_webView->emulatedTextZoomFactor());
        Document* doc = frame->document();
        doc->styleResolverChanged(RecalcStyleImmediately);
        doc->updateLayout();
    }

    void webViewResized()
    {
        if (!m_fitWindow)
            return;

        applySizeOverrideIfNecessary();
        autoZoomPageToFitWidth(m_webView->mainFrameImpl()->frame());
    }

    void applySizeOverrideIfNecessary()
    {
        FrameView* view = frameView();
        if (!view)
            return;

        applySizeOverrideInternal(view, FitWindowAllowed);
    }

private:
    enum FitWindowFlag { FitWindowAllowed, FitWindowNotAllowed };

    void ensureOriginalZoomFactor(FrameView* frameView)
    {
        if (m_originalZoomFactor)
            return;

        m_webView->setPageScaleFactor(1, WebPoint());
        m_webView->setZoomLevel(false, 0);
        WebSize scaledEmulatedSize = scaledEmulatedFrameSize(frameView);
        Document* document = frameView->frame()->document();
        double denominator = document->renderView() ? document->renderView()->viewWidth() : frameView->contentsWidth();
        if (!denominator)
            denominator = 1;
        m_originalZoomFactor = static_cast<double>(scaledEmulatedSize.width) / denominator;
    }

    void restore()
    {
        WebCore::FrameView* view = frameView();
        if (!view)
            return;

        m_webView->setZoomLevel(false, 0);
        m_webView->setEmulatedTextZoomFactor(1);
        view->setHorizontalScrollbarLock(false);
        view->setVerticalScrollbarLock(false);
        view->setScrollbarModes(ScrollbarAuto, ScrollbarAuto, false, false);
        view->resize(IntSize(m_webView->size()));
        m_webView->sendResizeEventAndRepaint();
    }

    WebSize scaledEmulatedFrameSize(FrameView* frameView)
    {
        if (!m_fitWindow)
            return m_emulatedFrameSize;

        WebSize scrollbarDimensions = forcedScrollbarDimensions(frameView);

        int overrideWidth = m_emulatedFrameSize.width;
        int overrideHeight = m_emulatedFrameSize.height;

        WebSize webViewSize = m_webView->size();
        int availableViewWidth = max(webViewSize.width - scrollbarDimensions.width, 1);
        int availableViewHeight = max(webViewSize.height - scrollbarDimensions.height, 1);

        double widthRatio = static_cast<double>(overrideWidth) / availableViewWidth;
        double heightRatio = static_cast<double>(overrideHeight) / availableViewHeight;
        double dimensionRatio = max(widthRatio, heightRatio);
        overrideWidth = static_cast<int>(ceil(static_cast<double>(overrideWidth) / dimensionRatio));
        overrideHeight = static_cast<int>(ceil(static_cast<double>(overrideHeight) / dimensionRatio));

        return WebSize(overrideWidth, overrideHeight);
    }

    WebSize forcedScrollbarDimensions(FrameView* frameView)
    {
        frameView->setScrollbarModes(ScrollbarAlwaysOn, ScrollbarAlwaysOn, true, true);

        int verticalScrollbarWidth = 0;
        int horizontalScrollbarHeight = 0;
        if (Scrollbar* verticalBar = frameView->verticalScrollbar())
            verticalScrollbarWidth = !verticalBar->isOverlayScrollbar() ? verticalBar->width() : 0;
        if (Scrollbar* horizontalBar = frameView->horizontalScrollbar())
            horizontalScrollbarHeight = !horizontalBar->isOverlayScrollbar() ? horizontalBar->height() : 0;
        return WebSize(verticalScrollbarWidth, horizontalScrollbarHeight);
    }

    void applySizeOverrideInternal(FrameView* frameView, FitWindowFlag fitWindowFlag)
    {
        WebSize scrollbarDimensions = forcedScrollbarDimensions(frameView);

        WebSize effectiveEmulatedSize = (fitWindowFlag == FitWindowAllowed) ? scaledEmulatedFrameSize(frameView) : m_emulatedFrameSize;
        int overrideWidth = effectiveEmulatedSize.width + scrollbarDimensions.width;
        int overrideHeight = effectiveEmulatedSize.height + scrollbarDimensions.height;

        if (IntSize(overrideWidth, overrideHeight) != frameView->size())
            frameView->resize(overrideWidth, overrideHeight);

        Document* doc = frameView->frame()->document();
        doc->styleResolverChanged(RecalcStyleImmediately);
        doc->updateLayout();
    }

    virtual void paintPageOverlay(WebCanvas* canvas)
    {
        FrameView* frameView = this->frameView();
        if (!frameView)
            return;

        GraphicsContextBuilder builder(canvas);
        GraphicsContext& gc = builder.context();
        gc.clipOut(IntRect(IntPoint(), frameView->size()));
        gc.setFillColor(Color::darkGray, ColorSpaceDeviceRGB);
        gc.drawRect(IntRect(IntPoint(), m_webView->size()));
    }

    WebCore::FrameView* frameView()
    {
        return m_webView->mainFrameImpl() ? m_webView->mainFrameImpl()->frameView() : 0;
    }

    WebViewImpl* m_webView;
    WebSize m_emulatedFrameSize;
    bool m_fitWindow;
    double m_originalZoomFactor;
};


WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsAgentClient* client)
    : m_hostId(client->hostIdentifier())
    , m_client(client)
    , m_webViewImpl(webViewImpl)
    , m_attached(false)
{
    ASSERT(m_hostId > 0);
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl()
{
    ClientMessageLoopAdapter::inspectedViewClosed(m_webViewImpl);
}

void WebDevToolsAgentImpl::attach()
{
    if (m_attached)
        return;

    ClientMessageLoopAdapter::ensureClientMessageLoopCreated(m_client);
    inspectorController()->connectFrontend();
    m_attached = true;
}

void WebDevToolsAgentImpl::reattach(const WebString& savedState)
{
    if (m_attached)
        return;

    ClientMessageLoopAdapter::ensureClientMessageLoopCreated(m_client);
    inspectorController()->restoreInspectorStateFromCookie(savedState);
    m_attached = true;
}

void WebDevToolsAgentImpl::detach()
{
    // Prevent controller from sending messages to the frontend.
    InspectorController* ic = inspectorController();
    ic->disconnectFrontend();
    ic->hideHighlight();
    ic->close();
    m_attached = false;
}

void WebDevToolsAgentImpl::didNavigate()
{
    ClientMessageLoopAdapter::didNavigate();
}

void WebDevToolsAgentImpl::didCreateScriptContext(WebFrameImpl* webframe, int worldId)
{
    // Skip non main world contexts.
    if (worldId)
        return;
    if (WebCore::V8Proxy* proxy = WebCore::V8Proxy::retrieve(webframe->frame()))
        proxy->setContextDebugId(m_hostId);
}

void WebDevToolsAgentImpl::mainFrameViewCreated(WebFrameImpl* webFrame)
{
    if (m_metricsSupport)
        m_metricsSupport->applySizeOverrideIfNecessary();
}

bool WebDevToolsAgentImpl::metricsOverridden()
{
    return !!m_metricsSupport;
}

void WebDevToolsAgentImpl::webViewResized()
{
    if (m_metricsSupport)
        m_metricsSupport->webViewResized();
}

void WebDevToolsAgentImpl::overrideDeviceMetrics(int width, int height, float fontScaleFactor, bool fitWindow)
{
    if (!width && !height) {
        if (m_metricsSupport)
            m_metricsSupport.clear();
        return;
    }

    if (!m_metricsSupport)
        m_metricsSupport = adoptPtr(new DeviceMetricsSupport(m_webViewImpl));
    m_metricsSupport->setDeviceMetrics(width, height, fontScaleFactor, fitWindow);
}

void WebDevToolsAgentImpl::autoZoomPageToFitWidth()
{
    if (m_metricsSupport)
        m_metricsSupport->autoZoomPageToFitWidthOnNavigation(m_webViewImpl->mainFrameImpl()->frame());
}

void WebDevToolsAgentImpl::dispatchOnInspectorBackend(const WebString& message)
{
    inspectorController()->dispatchMessageFromFrontend(message);
}

void WebDevToolsAgentImpl::inspectElementAt(const WebPoint& point)
{
    m_webViewImpl->inspectElementAt(point);
}

InspectorController* WebDevToolsAgentImpl::inspectorController()
{
    if (Page* page = m_webViewImpl->page())
        return page->inspectorController();
    return 0;
}

Frame* WebDevToolsAgentImpl::mainFrame()
{
    if (Page* page = m_webViewImpl->page())
        return page->mainFrame();
    return 0;
}

void WebDevToolsAgentImpl::inspectorDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

void WebDevToolsAgentImpl::openInspectorFrontend(InspectorController*)
{
}

void WebDevToolsAgentImpl::closeInspectorFrontend()
{
}

void WebDevToolsAgentImpl::bringFrontendToFront()
{
}

// WebPageOverlay
void WebDevToolsAgentImpl::paintPageOverlay(WebCanvas* canvas)
{
    InspectorController* ic = inspectorController();
    if (ic)
        ic->drawHighlight(GraphicsContextBuilder(canvas).context());
}

void WebDevToolsAgentImpl::highlight()
{
    m_webViewImpl->addPageOverlay(this, OverlayZOrders::highlight);
}

void WebDevToolsAgentImpl::hideHighlight()
{
    m_webViewImpl->removePageOverlay(this);
}

bool WebDevToolsAgentImpl::sendMessageToFrontend(const String& message)
{
    WebDevToolsAgentImpl* devToolsAgent = static_cast<WebDevToolsAgentImpl*>(m_webViewImpl->devToolsAgent());
    if (!devToolsAgent)
        return false;

    m_client->sendMessageToInspectorFrontend(message);
    return true;
}

void WebDevToolsAgentImpl::updateInspectorStateCookie(const String& state)
{
    m_client->saveAgentRuntimeState(state);
}

void WebDevToolsAgentImpl::clearBrowserCache()
{
    m_client->clearBrowserCache();
}

void WebDevToolsAgentImpl::clearBrowserCookies()
{
    m_client->clearBrowserCookies();
}

void WebDevToolsAgentImpl::setProcessId(long processId)
{
    inspectorController()->setProcessId(processId);
}

void WebDevToolsAgentImpl::evaluateInWebInspector(long callId, const WebString& script)
{
    InspectorController* ic = inspectorController();
    ic->evaluateForTestInFrontend(callId, script);
}

void WebDevToolsAgentImpl::setJavaScriptProfilingEnabled(bool enabled)
{
    InspectorController* ic = inspectorController();
    if (enabled)
        ic->enableProfiler();
    else
        ic->disableProfiler();
}

WebString WebDevToolsAgent::inspectorProtocolVersion()
{
    return WebCore::inspectorProtocolVersion();
}

bool WebDevToolsAgent::supportsInspectorProtocolVersion(const WebString& version)
{
    return WebCore::supportsInspectorProtocolVersion(version);
}

void WebDevToolsAgent::interruptAndDispatch(MessageDescriptor* rawDescriptor)
{
    // rawDescriptor can't be a PassOwnPtr because interruptAndDispatch is a WebKit API function.
    OwnPtr<MessageDescriptor> descriptor = adoptPtr(rawDescriptor);
    OwnPtr<DebuggerTask> task = adoptPtr(new DebuggerTask(descriptor.release()));
    PageScriptDebugServer::interruptAndRun(task.release());
}

bool WebDevToolsAgent::shouldInterruptForMessage(const WebString& message)
{
    String commandName;
    if (!InspectorBackendDispatcher::getCommandName(message, &commandName))
        return false;
    return commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kDebugger_pauseCmd]
        || commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kDebugger_setBreakpointCmd]
        || commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kDebugger_setBreakpointByUrlCmd]
        || commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kDebugger_removeBreakpointCmd]
        || commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kDebugger_setBreakpointsActiveCmd]
        || commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kProfiler_startCmd]
        || commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kProfiler_stopCmd]
        || commandName == InspectorBackendDispatcher::commandNames[InspectorBackendDispatcher::kProfiler_getProfileCmd];
}

void WebDevToolsAgent::processPendingMessages()
{
    PageScriptDebugServer::shared().runPendingTasks();
}

WebString WebDevToolsAgent::disconnectEventAsText()
{
    class ChannelImpl : public InspectorFrontendChannel {
    public:
        virtual bool sendMessageToFrontend(const String& message)
        {
            m_message = message;
            return true;
        }
        String m_message;
    } channel;
#if ENABLE(WORKERS)
    InspectorFrontend::Worker inspector(&channel);
    inspector.disconnectedFromWorker();
#endif
    return channel.m_message;
}

} // namespace WebKit
