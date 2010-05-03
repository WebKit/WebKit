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

#include "WebPageProxy.h"

#include "DrawingAreaProxy.h"
#include "MessageID.h"
#include "PageClient.h"
#include "WebContext.h"
#include "WebCoreTypeArgumentMarshalling.h"
#include "WebEvent.h"
#include "WebNavigationDataStore.h"
#include "WebPageMessageKinds.h"
#include "WebPageNamespace.h"
#include "WebPageProxyMessageKinds.h"
#include "WebPreferences.h"
#include "WebProcessManager.h"
#include "WebProcessMessageKinds.h"
#include "WebProcessProxy.h"

#include "WKContextPrivate.h"

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;

namespace WebKit {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webPageProxyCounter("WebPageProxy");
#endif

PassRefPtr<WebPageProxy> WebPageProxy::create(WebPageNamespace* pageNamespace, uint64_t pageID)
{
    return adoptRef(new WebPageProxy(pageNamespace, pageID));
}

WebPageProxy::WebPageProxy(WebPageNamespace* pageNamespace, uint64_t pageID)
    : m_pageNamespace(pageNamespace)
    , m_mainFrame(0)
    , m_canGoBack(false)
    , m_canGoForward(false)
    , m_valid(true)
    , m_closed(false)
    , m_pageID(pageID)
{
#ifndef NDEBUG
    webPageProxyCounter.increment();
#endif
}

WebPageProxy::~WebPageProxy()
{
#ifndef NDEBUG
    webPageProxyCounter.decrement();
#endif
}

WebProcessProxy* WebPageProxy::process() const
{
    return m_pageNamespace->process();
}

bool WebPageProxy::isValid()
{
    return m_valid;
}

void WebPageProxy::setPageClient(PageClient* pageClient)
{
    m_pageClient.set(pageClient);
}

void WebPageProxy::initializeLoaderClient(WKPageLoaderClient* loadClient)
{
    m_loaderClient.initialize(loadClient);
}

void WebPageProxy::initializePolicyClient(WKPagePolicyClient* policyClient)
{
    m_policyClient.initialize(policyClient);
}

void WebPageProxy::initializeUIClient(WKPageUIClient* client)
{
    m_uiClient.initialize(client);
}

void WebPageProxy::initializeHistoryClient(WKPageHistoryClient* client)
{
    m_historyClient.initialize(client);
}

void WebPageProxy::revive()
{
    m_valid = true;
    m_pageNamespace->reviveIfNecessary();
    m_pageNamespace->process()->addExistingWebPage(this, m_pageID);

    processDidRevive();
}

void WebPageProxy::initializeWebPage(const IntSize& size, PassOwnPtr<DrawingAreaProxy> drawingArea)
{
    if (!isValid()) {
        puts("initializeWebPage called with a dead WebProcess");
        revive();
        return;
    }

    m_drawingArea = drawingArea;
    process()->connection()->send(WebProcessMessage::Create, m_pageID, CoreIPC::In(size, pageNamespace()->context()->preferences()->store(), *(m_drawingArea.get())));
}

void WebPageProxy::reinitializeWebPage(const WebCore::IntSize& size)
{
    if (!isValid())
        return;

    ASSERT(m_drawingArea);
    process()->connection()->send(WebProcessMessage::Create, m_pageID, CoreIPC::In(size, pageNamespace()->context()->preferences()->store(), *(m_drawingArea.get())));
}

void WebPageProxy::close()
{
    if (!isValid())
        return;

    m_closed = true;

    Vector<RefPtr<WebFrameProxy> > frames;
    copyValuesToVector(m_frameMap, frames);
    for (size_t i = 0, size = frames.size(); i < size; ++i)
        frames[i]->disconnect();
    m_frameMap.clear();
    m_mainFrame = 0;

    m_pageTitle = String();
    m_toolTip = String();

    Vector<RefPtr<ScriptReturnValueCallback> > scriptReturnValueCallbacks;
    copyValuesToVector(m_scriptReturnValueCallbacks, scriptReturnValueCallbacks);
    for (size_t i = 0, size = scriptReturnValueCallbacks.size(); i < size; ++i)
        scriptReturnValueCallbacks[i]->invalidate();
    m_scriptReturnValueCallbacks.clear();

    Vector<RefPtr<RenderTreeExternalRepresentationCallback> > renderTreeExternalRepresentationCallbacks;
    copyValuesToVector(m_renderTreeExternalRepresentationCallbacks, renderTreeExternalRepresentationCallbacks);
    for (size_t i = 0, size = renderTreeExternalRepresentationCallbacks.size(); i < size; ++i)
        renderTreeExternalRepresentationCallbacks[i]->invalidate();
    m_renderTreeExternalRepresentationCallbacks.clear();

    m_canGoForward = false;
    m_canGoBack = false;

    m_loaderClient.initialize(0);
    m_policyClient.initialize(0);
    m_uiClient.initialize(0);

    m_drawingArea.clear();

    process()->connection()->send(WebPageMessage::Close, m_pageID, CoreIPC::In());
    process()->removeWebPage(m_pageID);
}

bool WebPageProxy::tryClose()
{
    if (!isValid())
        return true;

    process()->connection()->send(WebPageMessage::TryClose, m_pageID, CoreIPC::In());
    return false;
}

void WebPageProxy::loadURL(const String& url)
{
    if (!isValid()) {
        puts("loadURL called with a dead WebProcess");
        revive();
    }
    process()->connection()->send(WebPageMessage::LoadURL, m_pageID, CoreIPC::In(url));
}

void WebPageProxy::stopLoading()
{
    if (!isValid())
        return;
    process()->connection()->send(WebPageMessage::StopLoading, m_pageID, CoreIPC::In());
}

void WebPageProxy::reload()
{
    if (!isValid())
        return;
    process()->connection()->send(WebPageMessage::Reload, m_pageID, CoreIPC::In());
}

void WebPageProxy::goForward()
{
    if (!isValid())
        return;
    process()->connection()->send(WebPageMessage::GoForward, m_pageID, CoreIPC::In());
}

void WebPageProxy::goBack()
{
    if (!isValid())
        return;
    process()->connection()->send(WebPageMessage::GoBack, m_pageID, CoreIPC::In());
}

void WebPageProxy::setFocused(bool isFocused)
{
    if (!isValid())
        return;
    process()->connection()->send(WebPageMessage::SetFocused, m_pageID, CoreIPC::In(isFocused));
}

void WebPageProxy::setActive(bool active)
{
    if (!isValid())
        return;
    process()->connection()->send(WebPageMessage::SetActive, m_pageID, CoreIPC::In(active));
}

void WebPageProxy::mouseEvent(const WebMouseEvent& event)
{
    if (!isValid())
        return;

    // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
    if (event.type() != WebEvent::MouseMove)
        process()->responsivenessTimer()->start();
    process()->connection()->send(WebPageMessage::MouseEvent, m_pageID, CoreIPC::In(event));
}

void WebPageProxy::wheelEvent(const WebWheelEvent& event)
{
    if (!isValid())
        return;

    process()->responsivenessTimer()->start();
    process()->connection()->send(WebPageMessage::WheelEvent, m_pageID, CoreIPC::In(event));
}

void WebPageProxy::keyEvent(const WebKeyboardEvent& event)
{
    if (!isValid())
        return;

    process()->responsivenessTimer()->start();
    process()->connection()->send(WebPageMessage::KeyEvent, m_pageID, CoreIPC::In(event));
}

void WebPageProxy::receivedPolicyDecision(WebCore::PolicyAction action, WebFrameProxy* frame, uint64_t listenerID)
{
    if (!isValid())
        return;

    process()->connection()->send(WebPageMessage::DidReceivePolicyDecision, m_pageID, CoreIPC::In(frame->frameID(), listenerID, (uint32_t)action));
}

void WebPageProxy::terminateProcess()
{
    if (!isValid())
        return;

    process()->terminate();
}

void WebPageProxy::runJavaScriptInMainFrame(const String& script, PassRefPtr<ScriptReturnValueCallback> prpCallback)
{
    RefPtr<ScriptReturnValueCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_scriptReturnValueCallbacks.set(callbackID, callback.get());
    process()->connection()->send(WebPageMessage::RunJavaScriptInMainFrame, m_pageID, CoreIPC::In(script, callbackID));
}

void WebPageProxy::getRenderTreeExternalRepresentation(PassRefPtr<RenderTreeExternalRepresentationCallback> prpCallback)
{
    RefPtr<RenderTreeExternalRepresentationCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_renderTreeExternalRepresentationCallbacks.set(callbackID, callback.get());
    process()->connection()->send(WebPageMessage::GetRenderTreeExternalRepresentation, m_pageID, callbackID);
}

void WebPageProxy::preferencesDidChange()
{
    if (!isValid())
        return;

    // FIXME: It probably makes more sense to send individual preference changes.
    process()->connection()->send(WebPageMessage::PreferencesDidChange, m_pageID, CoreIPC::In(pageNamespace()->context()->preferences()->store()));
}

void WebPageProxy::getStatistics(WKContextStatistics* statistics)
{
    statistics->numberOfWKFrames += m_frameMap.size();
}

WebFrameProxy* WebPageProxy::webFrame(uint64_t frameID) const
{
    return m_frameMap.get(frameID).get();
}

void WebPageProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder& arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingAreaProxy>()) {
        m_drawingArea->didReceiveMessage(connection, messageID, arguments);
        return;
    }

    switch (messageID.get<WebPageProxyMessage::Kind>()) {
        case WebPageProxyMessage::DidCreateMainFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didCreateMainFrame(frameID);
            break;
        }
        case WebPageProxyMessage::DidCreateSubFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didCreateSubFrame(frameID);
            break;
        }
        case WebPageProxyMessage::DidStartProvisionalLoadForFrame: {
            uint64_t frameID;
            String url;
            if (!arguments.decode(CoreIPC::Out(frameID, url)))
                return;
            didStartProvisionalLoadForFrame(webFrame(frameID), url);
            break;
        }
        case WebPageProxyMessage::DidReceiveServerRedirectForProvisionalLoadForFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didReceiveServerRedirectForProvisionalLoadForFrame(webFrame(frameID));
            break;
        }
        case WebPageProxyMessage::DidFailProvisionalLoadForFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didFailProvisionalLoadForFrame(webFrame(frameID));
            break;
        }
        case WebPageProxyMessage::DidCommitLoadForFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didCommitLoadForFrame(webFrame(frameID));
            break;
        }
        case WebPageProxyMessage::DidFinishLoadForFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didFinishLoadForFrame(webFrame(frameID));
            break;
        }
        case WebPageProxyMessage::DidFailLoadForFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didFailLoadForFrame(webFrame(frameID));
            break;
        }
        case WebPageProxyMessage::DidReceiveTitleForFrame: {
            uint64_t frameID;
            String title;
            if (!arguments.decode(CoreIPC::Out(frameID, title)))
                return;
            didReceiveTitleForFrame(webFrame(frameID), title);
            break;
        }
        case WebPageProxyMessage::DidFirstLayoutForFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didFirstLayoutForFrame(webFrame(frameID));
            break;
        }
        case WebPageProxyMessage::DidFirstVisuallyNonEmptyLayoutForFrame: {
            uint64_t frameID;
            if (!arguments.decode(frameID))
                return;
            didFirstVisuallyNonEmptyLayoutForFrame(webFrame(frameID));
            break;
        }
        case WebPageProxyMessage::DidStartProgress:
            didStartProgress();
            break;
        case WebPageProxyMessage::DidChangeProgress: {
            double value;
            if (!arguments.decode(value))
                return;
            didChangeProgress(value);
            break;
        }
        case WebPageProxyMessage::DidFinishProgress:
            didFinishProgress();
            break;
        case WebPageProxyMessage::DidReceiveEvent: {
            uint32_t type;
            if (!arguments.decode(type))
                return;
            didReceiveEvent((WebEvent::Type)type);
            break;
        }
        case WebPageProxyMessage::TakeFocus: {
            // FIXME: Use enum here.
            bool direction;
            if (!arguments.decode(direction))
                return;
            takeFocus(direction);
            break;
        }
        case WebPageProxyMessage::DidChangeCanGoBack: {
            bool canGoBack;
            if (!arguments.decode(canGoBack))
                return;
            m_canGoBack = canGoBack;
            break;
        }
        case WebPageProxyMessage::DidChangeCanGoForward: {
            bool canGoForward;
            if (!arguments.decode(canGoForward))
                return;
            m_canGoForward = canGoForward;
            break;
        }
        case WebPageProxyMessage::DecidePolicyForNavigationAction: {
            uint64_t frameID;
            uint32_t navigationType;
            String url;
            uint64_t listenerID;
            if (!arguments.decode(CoreIPC::Out(frameID, navigationType, url, listenerID)))
                return;
            decidePolicyForNavigationAction(webFrame(frameID), navigationType, url, listenerID);
            break;
        }
        case WebPageProxyMessage::DecidePolicyForNewWindowAction: {
            uint64_t frameID;
            uint32_t navigationType;
            String url;
            uint64_t listenerID;
            if (!arguments.decode(CoreIPC::Out(frameID, navigationType, url, listenerID)))
                return;
            decidePolicyForNewWindowAction(webFrame(frameID), navigationType, url, listenerID);
            break;
        }
        case WebPageProxyMessage::DecidePolicyForMIMEType: {
            uint64_t frameID;
            String MIMEType;
            String url;
            uint64_t listenerID;
            if (!arguments.decode(CoreIPC::Out(frameID, MIMEType, url, listenerID)))
                return;
            decidePolicyForMIMEType(webFrame(frameID), MIMEType, url, listenerID);
            break;
        }
        case WebPageProxyMessage::DidRunJavaScriptInMainFrame: {
            String resultString;
            uint64_t callbackID;
            if (!arguments.decode(CoreIPC::Out(resultString, callbackID)))
                return;
            didRunJavaScriptInMainFrame(resultString, callbackID);
            break;
        }
        case WebPageProxyMessage::DidGetRenderTreeExternalRepresentation: {
            String resultString;
            uint64_t callbackID;
            if (!arguments.decode(CoreIPC::Out(resultString, callbackID)))
                return;
            didGetRenderTreeExternalRepresentation(resultString, callbackID);
            break;
        }
        case WebPageProxyMessage::SetToolTip: {
            String toolTip;
            if (!arguments.decode(toolTip))
                return;
            setToolTip(toolTip);
            break;
        }
        case WebPageProxyMessage::ShowPage: {
            showPage();
            break;
        }
        case WebPageProxyMessage::ClosePage: {
            closePage();
            break;
        }
        case WebPageProxyMessage::DidNavigateWithNavigationData: {
            WebNavigationDataStore store;
            uint64_t frameID;
            if (!arguments.decode(CoreIPC::Out(store, frameID)))
                return;
            didNavigateWithNavigationData(webFrame(frameID), store);
            break;
        }
        case WebPageProxyMessage::DidPerformClientRedirect: {
            String sourceURLString;
            String destinationURLString;
            uint64_t frameID;
            if (!arguments.decode(CoreIPC::Out(sourceURLString, destinationURLString, frameID)))
                return;
            didPerformClientRedirect(webFrame(frameID), sourceURLString, destinationURLString);
            break;
        }
        case WebPageProxyMessage::DidPerformServerRedirect: {
            String sourceURLString;
            String destinationURLString;
            uint64_t frameID;
            if (!arguments.decode(CoreIPC::Out(sourceURLString, destinationURLString, frameID)))
                return;
            didPerformServerRedirect(webFrame(frameID), sourceURLString, destinationURLString);
            break;
        }
        case WebPageProxyMessage::DidUpdateHistoryTitle: {
            String title;
            String url;
            uint64_t frameID;
            if (!arguments.decode(CoreIPC::Out(title, url, frameID)))
                return;
            didUpdateHistoryTitle(webFrame(frameID), title, url);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

void WebPageProxy::didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder& arguments, CoreIPC::ArgumentEncoder& reply)
{
    switch (messageID.get<WebPageProxyMessage::Kind>()) {
        case WebPageProxyMessage::CreateNewPage: {
            WebPageProxy* newPage = createNewPage();
            if (newPage) {
                // FIXME: Pass the real size.
                reply.encode(CoreIPC::In(newPage->pageID(), IntSize(100, 100), 
                                         newPage->pageNamespace()->context()->preferences()->store(),
                                         *(newPage->m_drawingArea.get())));
            } else {
                // FIXME: We should encode a drawing area type here instead.
                reply.encode(CoreIPC::In(static_cast<uint64_t>(0), IntSize(), WebPreferencesStore(), 0));
            }
            break;
        }
        case WebPageProxyMessage::RunJavaScriptAlert: {
            uint64_t frameID;
            String alertText;
            if (!arguments.decode(CoreIPC::Out(frameID, alertText)))
                return;
            runJavaScriptAlert(webFrame(frameID), alertText);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

void WebPageProxy::didCreateMainFrame(uint64_t frameID)
{
    ASSERT(!m_mainFrame);
    ASSERT(m_frameMap.isEmpty());

    m_mainFrame = WebFrameProxy::create(this, frameID);
    m_frameMap.set(frameID, m_mainFrame);
}

void WebPageProxy::didCreateSubFrame(uint64_t frameID)
{
    ASSERT(m_mainFrame);
    ASSERT(!m_frameMap.isEmpty());
    ASSERT(!m_frameMap.contains(frameID));

    m_frameMap.set(frameID, WebFrameProxy::create(this, frameID));
}

void WebPageProxy::didStartProgress()
{
    m_loaderClient.didStartProgress(this);
}

void WebPageProxy::didChangeProgress(double value)
{
    m_loaderClient.didChangeProgress(this, value);
}

void WebPageProxy::didFinishProgress()
{
    m_loaderClient.didFinishProgress(this);
}

void WebPageProxy::didStartProvisionalLoadForFrame(WebFrameProxy* frame, const String& url)
{
    frame->didStartProvisionalLoad(url);
    m_loaderClient.didStartProvisionalLoadForFrame(this, frame);
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(WebFrameProxy* frame)
{
    m_loaderClient.didReceiveServerRedirectForProvisionalLoadForFrame(this, frame);
}

void WebPageProxy::didFailProvisionalLoadForFrame(WebFrameProxy* frame)
{
    m_loaderClient.didFailProvisionalLoadWithErrorForFrame(this, frame);
}

void WebPageProxy::didCommitLoadForFrame(WebFrameProxy* frame)
{
    frame->didCommitLoad();
    m_loaderClient.didCommitLoadForFrame(this, frame);
}

void WebPageProxy::didFinishLoadForFrame(WebFrameProxy* frame)
{
    frame->didFinishLoad();
    m_loaderClient.didFinishLoadForFrame(this, frame);
}

void WebPageProxy::didFailLoadForFrame(WebFrameProxy* frame)
{
    m_loaderClient.didFailLoadWithErrorForFrame(this, frame);
}

void WebPageProxy::didReceiveTitleForFrame(WebFrameProxy* frame, const String& title)
{
    frame->didReceiveTitle(title);

    // Cache the title for the main frame in the page.
    if (frame == m_mainFrame)
        m_pageTitle = title;

    m_loaderClient.didReceiveTitleForFrame(this, title.impl(), frame);
}

void WebPageProxy::didFirstLayoutForFrame(WebFrameProxy* frame)
{
    m_loaderClient.didFirstLayoutForFrame(this, frame);
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(WebFrameProxy* frame)
{
    m_loaderClient.didFirstVisuallyNonEmptyLayoutForFrame(this, frame);
}

// PolicyClient

void WebPageProxy::decidePolicyForNavigationAction(WebFrameProxy* frame, uint32_t navigationType, const String& url, uint64_t listenerID)
{
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForNavigationAction(this, navigationType, url, frame, listener.get()))
        listener->use();
}

void WebPageProxy::decidePolicyForNewWindowAction(WebFrameProxy* frame, uint32_t navigationType, const String& url, uint64_t listenerID)
{
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForNewWindowAction(this, navigationType, url, frame, listener.get()))
        listener->use();
}

void WebPageProxy::decidePolicyForMIMEType(WebFrameProxy* frame, const String& MIMEType, const String& url, uint64_t listenerID)
{
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForMIMEType(this, MIMEType, url, frame, listener.get()))
        listener->use();
}

// UIClient
WebPageProxy* WebPageProxy::createNewPage()
{
    return m_uiClient.createNewPage(this);
}
    
void WebPageProxy::showPage()
{
    m_uiClient.showPage(this);
}

void WebPageProxy::closePage()
{
    m_uiClient.close(this);
}

void WebPageProxy::runJavaScriptAlert(WebFrameProxy* frame, const WebCore::String& alertText)
{
    m_uiClient.runJavaScriptAlert(this, alertText.impl(), frame);
}


// HistoryClient

void WebPageProxy::didNavigateWithNavigationData(WebFrameProxy* frame, const WebNavigationDataStore& store) 
{
    m_historyClient.didNavigateWithNavigationData(this, store, frame);
}

void WebPageProxy::didPerformClientRedirect(WebFrameProxy* frame, const String& sourceURLString, const String& destinationURLString)
{
    m_historyClient.didPerformClientRedirect(this, sourceURLString, destinationURLString, frame);
}

void WebPageProxy::didPerformServerRedirect(WebFrameProxy* frame, const String& sourceURLString, const String& destinationURLString)
{
    m_historyClient.didPerformServerRedirect(this, sourceURLString, destinationURLString, frame);
}

void WebPageProxy::didUpdateHistoryTitle(WebFrameProxy* frame, const String& title, const String& url)
{
    m_historyClient.didUpdateHistoryTitle(this, title, url, frame);
}

// Other

void WebPageProxy::takeFocus(bool direction)
{
    m_pageClient->takeFocus(direction);
}

void WebPageProxy::setToolTip(const String& toolTip)
{
    String oldToolTip = m_toolTip;
    m_toolTip = toolTip;
    m_pageClient->toolTipChanged(oldToolTip, m_toolTip);
}

void WebPageProxy::didReceiveEvent(WebEvent::Type type)
{
    switch (type) {
        case WebEvent::MouseMove:
            break;

        case WebEvent::MouseDown:
        case WebEvent::MouseUp:
        case WebEvent::Wheel:
        case WebEvent::KeyDown:
        case WebEvent::KeyUp:
        case WebEvent::RawKeyDown:
        case WebEvent::Char:
            process()->responsivenessTimer()->stop();
            break;
    }
}

void WebPageProxy::didRunJavaScriptInMainFrame(const String& resultString, uint64_t callbackID)
{
    RefPtr<ScriptReturnValueCallback> callback = m_scriptReturnValueCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::didGetRenderTreeExternalRepresentation(const String& resultString, uint64_t callbackID)
{
    RefPtr<RenderTreeExternalRepresentationCallback> callback = m_renderTreeExternalRepresentationCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::processDidBecomeUnresponsive()
{
    m_loaderClient.didBecomeUnresponsive(this);
}

void WebPageProxy::processDidBecomeResponsive()
{
    m_loaderClient.didBecomeResponsive(this);
}

void WebPageProxy::processDidExit()
{
    ASSERT(m_pageClient);

    m_valid = false;

    if (m_mainFrame)
        m_urlAtProcessExit = m_mainFrame->url();

    Vector<RefPtr<WebFrameProxy> > frames;
    copyValuesToVector(m_frameMap, frames);
    for (size_t i = 0, size = frames.size(); i < size; ++i)
        frames[i]->disconnect();
    m_frameMap.clear();
    m_mainFrame = 0;

    m_pageTitle = String();
    m_toolTip = String();

    Vector<RefPtr<ScriptReturnValueCallback> > scriptReturnValueCallbacks;
    copyValuesToVector(m_scriptReturnValueCallbacks, scriptReturnValueCallbacks);
    for (size_t i = 0, size = scriptReturnValueCallbacks.size(); i < size; ++i)
        scriptReturnValueCallbacks[i]->invalidate();
    m_scriptReturnValueCallbacks.clear();

    Vector<RefPtr<RenderTreeExternalRepresentationCallback> > renderTreeExternalRepresentationCallbacks;
    copyValuesToVector(m_renderTreeExternalRepresentationCallbacks, renderTreeExternalRepresentationCallbacks);
    for (size_t i = 0, size = renderTreeExternalRepresentationCallbacks.size(); i < size; ++i)
        renderTreeExternalRepresentationCallbacks[i]->invalidate();
    m_renderTreeExternalRepresentationCallbacks.clear();

    m_canGoForward = false;
    m_canGoBack = false;

    m_pageClient->processDidExit();
}

void WebPageProxy::processDidRevive()
{
    ASSERT(m_pageClient);
    m_pageClient->processDidRevive();
}

} // namespace WebKit
