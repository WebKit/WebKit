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

#include "DebuggerAgentImpl.h"
#include "DebuggerAgentManager.h"
#include "ExceptionCode.h"
#include "InjectedScriptHost.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "PageGroup.h"
#include "PageScriptDebugServer.h"
#include "PlatformString.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WebDataSource.h"
#include "WebDevToolsAgentClient.h"
#include "WebFrameImpl.h"
#include "WebRect.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLError.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <wtf/CurrentTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

using namespace WebCore;

namespace WebKit {

namespace {

static const char kFrontendConnectedFeatureName[] = "frontend-connected";
static const char kInspectorStateFeatureName[] = "inspector-state";

class ClientMessageLoopAdapter : public PageScriptDebugServer::ClientMessageLoop {
public:
    static void ensureClientMessageLoopCreated(WebDevToolsAgentClient* client)
    {
        if (s_instance)
            return;
        s_instance = new ClientMessageLoopAdapter(client->createClientMessageLoop());
        PageScriptDebugServer::shared().setClientMessageLoop(s_instance);
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
    static ClientMessageLoopAdapter* s_instance;

};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::s_instance = 0;

} //  namespace

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsAgentClient* client)
    : m_hostId(client->hostIdentifier())
    , m_client(client)
    , m_webViewImpl(webViewImpl)
    , m_attached(false)
{
    DebuggerAgentManager::setExposeV8DebuggerProtocol(
        client->exposeV8DebuggerProtocol());
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl()
{
    DebuggerAgentManager::onWebViewClosed(m_webViewImpl);
    ClientMessageLoopAdapter::inspectedViewClosed(m_webViewImpl);
}

void WebDevToolsAgentImpl::attach()
{
    if (m_attached)
        return;

    if (!m_client->exposeV8DebuggerProtocol())
        ClientMessageLoopAdapter::ensureClientMessageLoopCreated(m_client);

    m_debuggerAgentImpl.set(
        new DebuggerAgentImpl(m_webViewImpl, this, m_client));
    m_attached = true;
}

void WebDevToolsAgentImpl::detach()
{
    // Prevent controller from sending messages to the frontend.
    InspectorController* ic = inspectorController();
    ic->disconnectFrontend();
    ic->hideHighlight();
    ic->close();
    m_debuggerAgentImpl.set(0);
    m_attached = false;
}

void WebDevToolsAgentImpl::frontendLoaded()
{
    inspectorController()->connectFrontend();
}

void WebDevToolsAgentImpl::didNavigate()
{
    ClientMessageLoopAdapter::didNavigate();
    DebuggerAgentManager::onNavigate();
}

void WebDevToolsAgentImpl::didClearWindowObject(WebFrameImpl* webframe)
{
    DebuggerAgentManager::setHostId(webframe, m_hostId);
}

void WebDevToolsAgentImpl::dispatchOnInspectorBackend(const WebString& message)
{
    inspectorController()->dispatchMessageFromFrontend(message);
}

void WebDevToolsAgentImpl::inspectElementAt(const WebPoint& point)
{
    m_webViewImpl->inspectElementAt(point);
}

void WebDevToolsAgentImpl::setRuntimeProperty(const WebString& name, const WebString& value)
{
    if (name == kInspectorStateFeatureName) {
        InspectorController* ic = inspectorController();
        ic->restoreInspectorStateFromCookie(value);
    }
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

void WebDevToolsAgentImpl::highlight(Node* node)
{
    // InspectorController does the actuall tracking of the highlighted node
    // and the drawing of the highlight. Here we just make sure to invalidate
    // the rects of the old and new nodes.
    hideHighlight();
}

void WebDevToolsAgentImpl::hideHighlight()
{
    // FIXME: able to invalidate a smaller rect.
    // FIXME: Is it important to just invalidate the rect of the node region
    // given that this is not on a critical codepath?  In order to do so, we'd
    // have to take scrolling into account.
    const WebSize& size = m_webViewImpl->size();
    WebRect damagedRect(0, 0, size.width, size.height);
    if (m_webViewImpl->client())
        m_webViewImpl->client()->didInvalidateRect(damagedRect);
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
    m_client->runtimePropertyChanged(kInspectorStateFeatureName, state);
}

void WebDevToolsAgentImpl::evaluateInWebInspector(long callId, const WebString& script)
{
    InspectorController* ic = inspectorController();
    ic->evaluateForTestInFrontend(callId, script);
}

void WebDevToolsAgentImpl::setTimelineProfilingEnabled(bool enabled)
{
    InspectorController* ic = inspectorController();
    if (enabled)
        ic->startTimelineProfiler();
    else
        ic->stopTimelineProfiler();
}

void WebDevToolsAgent::executeDebuggerCommand(const WebString& command, int callerId)
{
    DebuggerAgentManager::executeDebuggerCommand(command, callerId);
}

void WebDevToolsAgent::debuggerPauseScript()
{
    DebuggerAgentManager::pauseScript();
}

void WebDevToolsAgent::interruptAndDispatch(MessageDescriptor* d)
{
    class DebuggerTask : public PageScriptDebugServer::Task {
    public:
        DebuggerTask(WebDevToolsAgent::MessageDescriptor* descriptor) : m_descriptor(descriptor) { }
        virtual ~DebuggerTask() { }
        virtual void run()
        {
            if (WebDevToolsAgent* webagent = m_descriptor->agent())
                webagent->dispatchOnInspectorBackend(m_descriptor->message());
        }
    private:
        OwnPtr<WebDevToolsAgent::MessageDescriptor> m_descriptor;
    };
    PageScriptDebugServer::interruptAndRun(new DebuggerTask(d));
}

bool WebDevToolsAgent::shouldInterruptForMessage(const WebString& message)
{
    String commandName;
    if (!InspectorBackendDispatcher::getCommandName(message, &commandName))
        return false;
    return commandName == InspectorBackendDispatcher::Debugger_pauseCmd
        || commandName == InspectorBackendDispatcher::Debugger_setBreakpointCmd
        || commandName == InspectorBackendDispatcher::Debugger_setBreakpointByUrlCmd
        || commandName == InspectorBackendDispatcher::Debugger_removeBreakpointCmd
        || commandName == InspectorBackendDispatcher::Debugger_setBreakpointsActiveCmd
        || commandName == InspectorBackendDispatcher::Profiler_startCmd
        || commandName == InspectorBackendDispatcher::Profiler_stopCmd
        || commandName == InspectorBackendDispatcher::Profiler_getProfileCmd;
}

void WebDevToolsAgent::processPendingMessages()
{
    PageScriptDebugServer::shared().runPendingTasks();
}

void WebDevToolsAgent::setMessageLoopDispatchHandler(MessageLoopDispatchHandler handler)
{
    DebuggerAgentManager::setMessageLoopDispatchHandler(handler);
}

} // namespace WebKit
