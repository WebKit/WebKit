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
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebCertificateInfo.h"
#include "WebContext.h"
#include "WebContextUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebData.h"
#include "WebEditCommandProxy.h"
#include "WebEvent.h"
#include "WebFormSubmissionListenerProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageMessageKinds.h"
#include "WebPageNamespace.h"
#include "WebPageProxyMessageKinds.h"
#include "WebPreferences.h"
#include "WebProcessManager.h"
#include "WebProcessMessageKinds.h"
#include "WebProcessProxy.h"
#include "WebURLRequest.h"

#include "WKContextPrivate.h"
#include <stdio.h>

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
    : m_pageClient(0)
    , m_pageNamespace(pageNamespace)
    , m_mainFrame(0)
    , m_estimatedProgress(0.0)
    , m_isInWindow(false)
    , m_backForwardList(WebBackForwardList::create(this))
    , m_textZoomFactor(1)
    , m_pageZoomFactor(1)
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
    m_pageClient = pageClient;
}

void WebPageProxy::setDrawingArea(PassOwnPtr<DrawingAreaProxy> drawingArea)
{
    if (drawingArea == m_drawingArea)
        return;

    m_drawingArea = drawingArea;
}

void WebPageProxy::initializeLoaderClient(const WKPageLoaderClient* loadClient)
{
    m_loaderClient.initialize(loadClient);
}

void WebPageProxy::initializePolicyClient(const WKPagePolicyClient* policyClient)
{
    m_policyClient.initialize(policyClient);
}

void WebPageProxy::initializeFormClient(const WKPageFormClient* formClient)
{
    m_formClient.initialize(formClient);
}

void WebPageProxy::initializeUIClient(const WKPageUIClient* client)
{
    m_uiClient.initialize(client);
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
    process()->send(WebProcessMessage::Create, m_pageID, CoreIPC::In(size, pageNamespace()->context()->preferences()->store(), *(m_drawingArea.get())));
}

void WebPageProxy::reinitializeWebPage(const WebCore::IntSize& size)
{
    if (!isValid())
        return;

    ASSERT(m_drawingArea);
    process()->send(WebProcessMessage::Create, m_pageID, CoreIPC::In(size, pageNamespace()->context()->preferences()->store(), *(m_drawingArea.get())));
}

void WebPageProxy::close()
{
    if (!isValid())
        return;

    m_closed = true;

    process()->disconnectFramesFromPage(this);
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

    Vector<RefPtr<FrameSourceCallback> > frameSourceCallbacks;
    copyValuesToVector(m_frameSourceCallbacks, frameSourceCallbacks);
    m_frameSourceCallbacks.clear();
    for (size_t i = 0, size = frameSourceCallbacks.size(); i < size; ++i)
        frameSourceCallbacks[i]->invalidate();

    Vector<WebEditCommandProxy*> editCommandVector;
    copyToVector(m_editCommandSet, editCommandVector);
    m_editCommandSet.clear();
    for (size_t i = 0, size = editCommandVector.size(); i < size; ++i)
        editCommandVector[i]->invalidate();

    m_estimatedProgress = 0.0;
    
    m_loaderClient.initialize(0);
    m_policyClient.initialize(0);
    m_uiClient.initialize(0);

    m_drawingArea.clear();

    process()->send(WebPageMessage::Close, m_pageID, CoreIPC::In());
    process()->removeWebPage(m_pageID);
}

bool WebPageProxy::tryClose()
{
    if (!isValid())
        return true;

    process()->send(WebPageMessage::TryClose, m_pageID, CoreIPC::In());
    return false;
}

void WebPageProxy::loadURL(const String& url)
{
    if (!isValid()) {
        puts("loadURL called with a dead WebProcess");
        revive();
    }

    process()->send(WebPageMessage::LoadURL, m_pageID, CoreIPC::In(url));
}

void WebPageProxy::loadURLRequest(WebURLRequest* urlRequest)
{
    if (!isValid()) {
        puts("loadURLRequest called with a dead WebProcess");
        revive();
    }

    process()->send(WebPageMessage::LoadURLRequest, m_pageID, CoreIPC::In(urlRequest->resourceRequest()));
}

void WebPageProxy::loadHTMLString(const String& htmlString, const String& baseURL)
{
    if (!isValid())
        return;
    process()->send(WebPageMessage::LoadHTMLString, m_pageID, CoreIPC::In(htmlString, baseURL));
}

void WebPageProxy::stopLoading()
{
    if (!isValid())
        return;
    process()->send(WebPageMessage::StopLoading, m_pageID, CoreIPC::In());
}

void WebPageProxy::reload(bool reloadFromOrigin)
{
    if (!isValid())
        return;
    process()->send(WebPageMessage::Reload, m_pageID, CoreIPC::In(reloadFromOrigin));
}

void WebPageProxy::goForward()
{
    if (!isValid())
        return;

    if (!canGoForward())
        return;

    process()->send(WebPageMessage::GoForward, m_pageID, CoreIPC::In(m_backForwardList->forwardItem()->itemID()));
}

bool WebPageProxy::canGoForward() const
{
    return m_backForwardList->forwardItem();
}

void WebPageProxy::goBack()
{
    if (!isValid())
        return;

    if (!canGoBack())
        return;

    process()->send(WebPageMessage::GoBack, m_pageID, CoreIPC::In(m_backForwardList->backItem()->itemID()));
}

bool WebPageProxy::canGoBack() const
{
    return m_backForwardList->backItem();
}

void WebPageProxy::goToBackForwardItem(WebBackForwardListItem* item)
{
    if (!isValid())
        return;

    process()->send(WebPageMessage::GoToBackForwardItem, m_pageID, CoreIPC::In(item->itemID()));
}

void WebPageProxy::didChangeBackForwardList()
{
    m_loaderClient.didChangeBackForwardList(this);
}

void WebPageProxy::setFocused(bool isFocused)
{
    if (!isValid())
        return;
    process()->send(WebPageMessage::SetFocused, m_pageID, CoreIPC::In(isFocused));
}

void WebPageProxy::setActive(bool active)
{
    if (!isValid())
        return;
    process()->send(WebPageMessage::SetActive, m_pageID, CoreIPC::In(active));
}

void WebPageProxy::selectAll()
{
    if (!isValid())
        return;

    process()->send(WebPageMessage::SelectAll, m_pageID, CoreIPC::In());
}

void WebPageProxy::copy()
{
    if (!isValid())
        return;

    process()->send(WebPageMessage::Copy, m_pageID, CoreIPC::In());
}

void WebPageProxy::cut()
{
    if (!isValid())
        return;

    process()->send(WebPageMessage::Cut, m_pageID, CoreIPC::In());
}

void WebPageProxy::paste()
{
    if (!isValid())
        return;

    process()->send(WebPageMessage::Paste, m_pageID, CoreIPC::In());
}
    
void WebPageProxy::setIsInWindow(bool isInWindow)
{
    if (m_isInWindow == isInWindow)
        return;
    
    m_isInWindow = isInWindow;
    if (!isValid())
        return;
    process()->send(WebPageMessage::SetIsInWindow, m_pageID, CoreIPC::In(isInWindow));
}
    
void WebPageProxy::mouseEvent(const WebMouseEvent& event)
{
    if (!isValid())
        return;

    // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
    if (event.type() != WebEvent::MouseMove)
        process()->responsivenessTimer()->start();
    process()->send(WebPageMessage::MouseEvent, m_pageID, CoreIPC::In(event));
}

void WebPageProxy::wheelEvent(const WebWheelEvent& event)
{
    if (!isValid())
        return;

    process()->responsivenessTimer()->start();
    process()->send(WebPageMessage::WheelEvent, m_pageID, CoreIPC::In(event));
}

void WebPageProxy::keyEvent(const WebKeyboardEvent& event)
{
    if (!isValid())
        return;

    process()->responsivenessTimer()->start();
    process()->send(WebPageMessage::KeyEvent, m_pageID, CoreIPC::In(event));
}

#if ENABLE(TOUCH_EVENTS)
void WebPageProxy::touchEvent(const WebTouchEvent& event)
{
    if (!isValid())
        return;
    process()->send(WebPageMessage::TouchEvent, m_pageID, CoreIPC::In(event)); 
}
#endif

void WebPageProxy::receivedPolicyDecision(WebCore::PolicyAction action, WebFrameProxy* frame, uint64_t listenerID)
{
    if (!isValid())
        return;

    process()->send(WebPageMessage::DidReceivePolicyDecision, m_pageID, CoreIPC::In(frame->frameID(), listenerID, (uint32_t)action));
}

void WebPageProxy::setCustomUserAgent(const String& userAgent)
{
    if (!isValid())
        return;

    process()->send(WebPageMessage::SetCustomUserAgent, m_pageID, CoreIPC::In(userAgent));
}

void WebPageProxy::terminateProcess()
{
    if (!isValid())
        return;

    process()->terminate();
}

PassRefPtr<WebData> WebPageProxy::sessionState() const
{
    // FIXME: Return session state data for saving Page state.
    return 0;
}

void WebPageProxy::restoreFromSessionState(WebData*)
{
    // FIXME: Restore the Page from the passed in session state data.
}

void WebPageProxy::setTextZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_textZoomFactor == zoomFactor)
        return;

    m_textZoomFactor = zoomFactor;
    process()->send(WebPageMessage::SetTextZoomFactor, m_pageID, CoreIPC::In(m_textZoomFactor)); 
}

void WebPageProxy::setPageZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_pageZoomFactor == zoomFactor)
        return;

    m_pageZoomFactor = zoomFactor;
    process()->send(WebPageMessage::SetPageZoomFactor, m_pageID, CoreIPC::In(m_pageZoomFactor)); 
}

void WebPageProxy::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    if (!isValid())
        return;

    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;
    process()->send(WebPageMessage::SetPageAndTextZoomFactors, m_pageID, CoreIPC::In(m_pageZoomFactor, m_textZoomFactor)); 
}

void WebPageProxy::runJavaScriptInMainFrame(const String& script, PassRefPtr<ScriptReturnValueCallback> prpCallback)
{
    RefPtr<ScriptReturnValueCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_scriptReturnValueCallbacks.set(callbackID, callback.get());
    process()->send(WebPageMessage::RunJavaScriptInMainFrame, m_pageID, CoreIPC::In(script, callbackID));
}

void WebPageProxy::getRenderTreeExternalRepresentation(PassRefPtr<RenderTreeExternalRepresentationCallback> prpCallback)
{
    RefPtr<RenderTreeExternalRepresentationCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_renderTreeExternalRepresentationCallbacks.set(callbackID, callback.get());
    process()->send(WebPageMessage::GetRenderTreeExternalRepresentation, m_pageID, callbackID);
}

void WebPageProxy::getSourceForFrame(WebFrameProxy* frame, PassRefPtr<FrameSourceCallback> prpCallback)
{
    RefPtr<FrameSourceCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_frameSourceCallbacks.set(callbackID, callback.get());
    process()->send(WebPageMessage::GetSourceForFrame, m_pageID, CoreIPC::In(frame->frameID(), callbackID));
}

void WebPageProxy::preferencesDidChange()
{
    if (!isValid())
        return;

    // FIXME: It probably makes more sense to send individual preference changes.
    // However, WebKitTestRunner depends on getting a preference change notification even if nothing changed in UI process, so that overrides get removed.
    process()->send(WebPageMessage::PreferencesDidChange, m_pageID, CoreIPC::In(pageNamespace()->context()->preferences()->store()));
}

void WebPageProxy::getStatistics(WKContextStatistics* statistics)
{
    statistics->numberOfWKFrames += process()->frameCountInPage(this);
}

void WebPageProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingAreaProxy>()) {
        m_drawingArea->didReceiveMessage(connection, messageID, arguments);
        return;
    }

    switch (messageID.get<WebPageProxyMessage::Kind>()) {
        case WebPageProxyMessage::DidCreateMainFrame: {
            uint64_t frameID;
            if (!arguments->decode(frameID))
                return;
            didCreateMainFrame(frameID);
            break;
        }
        case WebPageProxyMessage::DidCreateSubFrame: {
            uint64_t frameID;
            if (!arguments->decode(frameID))
                return;
            didCreateSubFrame(frameID);
            break;
        }
        case WebPageProxyMessage::DidStartProvisionalLoadForFrame: {
            uint64_t frameID;
            String url;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, url, messageDecoder)))
                return;

            didStartProvisionalLoadForFrame(process()->webFrame(frameID), url, userData.get());
            break;
        }
        case WebPageProxyMessage::DidReceiveServerRedirectForProvisionalLoadForFrame: {
            uint64_t frameID;
            String url;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, url, messageDecoder)))
                return;

            didReceiveServerRedirectForProvisionalLoadForFrame(process()->webFrame(frameID), url, userData.get());
            break;
        }
        case WebPageProxyMessage::DidFailProvisionalLoadForFrame: {
            uint64_t frameID;
            
            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, messageDecoder)))
                return;

            didFailProvisionalLoadForFrame(process()->webFrame(frameID), userData.get());
            break;
        }
        case WebPageProxyMessage::DidCommitLoadForFrame: {
            uint64_t frameID;
            PlatformCertificateInfo certificateInfo;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, certificateInfo, messageDecoder)))
                return;
    
            didCommitLoadForFrame(process()->webFrame(frameID), certificateInfo, userData.get());
            break;
        }
        case WebPageProxyMessage::DidFinishDocumentLoadForFrame: {
            uint64_t frameID;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, messageDecoder)))
                return;

            didFinishDocumentLoadForFrame(process()->webFrame(frameID), userData.get());
            break;
        }
        case WebPageProxyMessage::DidFinishLoadForFrame: {
            uint64_t frameID;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, messageDecoder)))
                return;

            didFinishLoadForFrame(process()->webFrame(frameID), userData.get());
            break;
        }
        case WebPageProxyMessage::DidFailLoadForFrame: {
            uint64_t frameID;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, messageDecoder)))
                return;

            didFailLoadForFrame(process()->webFrame(frameID), userData.get());
            break;
        }
        case WebPageProxyMessage::DidReceiveTitleForFrame: {
            uint64_t frameID;
            String title;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, title, messageDecoder)))
                return;

            didReceiveTitleForFrame(process()->webFrame(frameID), title, userData.get());
            break;
        }
        case WebPageProxyMessage::DidFirstLayoutForFrame: {
            uint64_t frameID;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, messageDecoder)))
                return;

            didFirstLayoutForFrame(process()->webFrame(frameID), userData.get());
            break;
        }
        case WebPageProxyMessage::DidFirstVisuallyNonEmptyLayoutForFrame: {
            uint64_t frameID;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, messageDecoder)))
                return;

            didFirstVisuallyNonEmptyLayoutForFrame(process()->webFrame(frameID), userData.get());
            break;
        }
        case WebPageProxyMessage::DidStartProgress:
            didStartProgress();
            break;
        case WebPageProxyMessage::DidChangeProgress: {
            double value;
            if (!arguments->decode(value))
                return;
            didChangeProgress(value);
            break;
        }
        case WebPageProxyMessage::DidFinishProgress:
            didFinishProgress();
            break;
        case WebPageProxyMessage::DidReceiveEvent: {
            uint32_t type;
            if (!arguments->decode(type))
                return;
            didReceiveEvent((WebEvent::Type)type);
            break;
        }
        case WebPageProxyMessage::TakeFocus: {
            // FIXME: Use enum here.
            bool direction;
            if (!arguments->decode(direction))
                return;
            takeFocus(direction);
            break;
        }
        case WebPageProxyMessage::DecidePolicyForNavigationAction: {
            uint64_t frameID;
            uint32_t navigationType;
            uint32_t modifiers;
            String url;
            uint64_t listenerID;
            if (!arguments->decode(CoreIPC::Out(frameID, navigationType, modifiers, url, listenerID)))
                return;
            decidePolicyForNavigationAction(process()->webFrame(frameID), static_cast<NavigationType>(navigationType), static_cast<WebEvent::Modifiers>(modifiers), url, listenerID);
            break;
        }
        case WebPageProxyMessage::DecidePolicyForNewWindowAction: {
            uint64_t frameID;
            uint32_t navigationType;
            uint32_t modifiers;
            String url;
            uint64_t listenerID;
            if (!arguments->decode(CoreIPC::Out(frameID, navigationType, modifiers, url, listenerID)))
                return;
            decidePolicyForNewWindowAction(process()->webFrame(frameID), static_cast<NavigationType>(navigationType), static_cast<WebEvent::Modifiers>(modifiers), url, listenerID);
            break;
        }
        case WebPageProxyMessage::DecidePolicyForMIMEType: {
            uint64_t frameID;
            String MIMEType;
            String url;
            uint64_t listenerID;
            if (!arguments->decode(CoreIPC::Out(frameID, MIMEType, url, listenerID)))
                return;
            decidePolicyForMIMEType(process()->webFrame(frameID), MIMEType, url, listenerID);
            break;
        }
        case WebPageProxyMessage::WillSubmitForm: {
            uint64_t frameID;
            uint64_t sourceFrameID;
            Vector<std::pair<String, String> > textFieldValues;
            uint64_t listenerID;

            RefPtr<APIObject> userData;
            WebContextUserMessageDecoder messageDecoder(userData, pageNamespace()->context());

            if (!arguments->decode(CoreIPC::Out(frameID, sourceFrameID, textFieldValues, listenerID, messageDecoder)))
                return;

            willSubmitForm(process()->webFrame(frameID), process()->webFrame(sourceFrameID), textFieldValues, userData.get(), listenerID);
            break;
        }
        case WebPageProxyMessage::DidRunJavaScriptInMainFrame: {
            String resultString;
            uint64_t callbackID;
            if (!arguments->decode(CoreIPC::Out(resultString, callbackID)))
                return;
            didRunJavaScriptInMainFrame(resultString, callbackID);
            break;
        }
        case WebPageProxyMessage::DidGetRenderTreeExternalRepresentation: {
            String resultString;
            uint64_t callbackID;
            if (!arguments->decode(CoreIPC::Out(resultString, callbackID)))
                return;
            didGetRenderTreeExternalRepresentation(resultString, callbackID);
            break;
        }
        case WebPageProxyMessage::DidGetSourceForFrame: {
            String resultString;
            uint64_t callbackID;
            if (!arguments->decode(CoreIPC::Out(resultString, callbackID)))
                return;
            didGetSourceForFrame(resultString, callbackID);
            break;
        }
        case WebPageProxyMessage::SetToolTip: {
            String toolTip;
            if (!arguments->decode(toolTip))
                return;
            setToolTip(toolTip);
            break;
        }
        case WebPageProxyMessage::SetCursor: {
#if USE(LAZY_NATIVE_CURSOR)
            Cursor cursor;
            if (!arguments->decode(cursor))
                return;
            setCursor(cursor);
#endif
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
        case WebPageProxyMessage::BackForwardAddItem: {
            uint64_t itemID;
            if (!arguments->decode(CoreIPC::Out(itemID)))
                return;
            addItemToBackForwardList(process()->webBackForwardItem(itemID));
            break;
        }
        case WebPageProxyMessage::BackForwardGoToItem: {
            uint64_t itemID;
            if (!arguments->decode(CoreIPC::Out(itemID)))
                return;
            goToItemInBackForwardList(process()->webBackForwardItem(itemID));
            break;
        }
        case WebPageProxyMessage::ContentsSizeChanged: {
            IntSize size;
            uint64_t frameID;
            if (!arguments->decode(CoreIPC::Out(frameID, size)))
                return;
            contentsSizeChanged(process()->webFrame(frameID), size);
            break;
        }
        case WebPageProxyMessage::SetStatusText: {
            String text;
            if (!arguments->decode(CoreIPC::Out(text)))
                return;
            setStatusText(text);
            break;
        }
        case WebPageProxyMessage::RegisterEditCommandForUndo: {
            uint64_t commandID;
            uint32_t editAction;
            if (!arguments->decode(CoreIPC::Out(commandID, editAction)))
                return;
                
            registerEditCommandForUndo(commandID, static_cast<EditAction>(editAction));
            break;
        }
        case WebPageProxyMessage::ClearAllEditCommands:
            clearAllEditCommands();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

void WebPageProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    if (messageID.is<CoreIPC::MessageClassDrawingAreaProxy>()) {
        m_drawingArea->didReceiveSyncMessage(connection, messageID, arguments, reply);
        return;
    }

    switch (messageID.get<WebPageProxyMessage::Kind>()) {
        case WebPageProxyMessage::CreateNewPage: {
            RefPtr<WebPageProxy> newPage = createNewPage();
            if (newPage) {
                // FIXME: Pass the real size.
                reply->encode(CoreIPC::In(newPage->pageID(), IntSize(100, 100), 
                                          newPage->pageNamespace()->context()->preferences()->store(),
                                          *(newPage->drawingArea())));
            } else {
                reply->encode(CoreIPC::In(static_cast<uint64_t>(0), IntSize(), WebPreferencesStore(), DrawingAreaBase::DrawingAreaInfo()));
            }
            break;
        }
        case WebPageProxyMessage::RunJavaScriptAlert: {
            uint64_t frameID;
            String message;
            if (!arguments->decode(CoreIPC::Out(frameID, message)))
                return;
            runJavaScriptAlert(process()->webFrame(frameID), message);
            break;
        }
        case WebPageProxyMessage::RunJavaScriptConfirm: {
            // FIXME: We should probably encode something in the case that the arguments do not decode correctly.
            uint64_t frameID;
            String message;
            if (!arguments->decode(CoreIPC::Out(frameID, message)))
                return;

            bool result = runJavaScriptConfirm(process()->webFrame(frameID), message);
            reply->encode(CoreIPC::In(result));
            break;
        }
        case WebPageProxyMessage::RunJavaScriptPrompt: {
            // FIXME: We should probably encode something in the case that the arguments do not decode correctly.
            uint64_t frameID;
            String message;
            String defaultValue;
            if (!arguments->decode(CoreIPC::Out(frameID, message, defaultValue)))
                return;

            String result = runJavaScriptPrompt(process()->webFrame(frameID), message, defaultValue);
            reply->encode(CoreIPC::In(result));
            break;
        }

        case WebPageProxyMessage::BackForwardBackItem: {
            WebBackForwardListItem* backItem = m_backForwardList->backItem();
            uint64_t backItemID = backItem ? backItem->itemID() : 0;
            reply->encode(CoreIPC::In(backItemID));
            break;
        }
        case WebPageProxyMessage::BackForwardCurrentItem: {
            WebBackForwardListItem* currentItem = m_backForwardList->currentItem();
            uint64_t currentItemID = currentItem ? currentItem->itemID() : 0;
            reply->encode(CoreIPC::In(currentItemID));
            break;
        }
        case WebPageProxyMessage::BackForwardForwardItem: {
            WebBackForwardListItem* forwardItem = m_backForwardList->forwardItem();
            uint64_t forwardItemID = forwardItem ? forwardItem->itemID() : 0;
            reply->encode(CoreIPC::In(forwardItemID));
            break;
        }
        case WebPageProxyMessage::BackForwardItemAtIndex: {
            int itemIndex;
            if (!arguments->decode(CoreIPC::Out(itemIndex)))
                return;

            WebBackForwardListItem* item = m_backForwardList->itemAtIndex(itemIndex);
            uint64_t itemID = item ? item->itemID() : 0;
            reply->encode(CoreIPC::In(itemID));
            break;
        }
        case WebPageProxyMessage::BackForwardBackListCount: {
            int backListCount = m_backForwardList->backListCount();
            reply->encode(CoreIPC::In(backListCount));
            break;
        }
        case WebPageProxyMessage::BackForwardForwardListCount: {
            int forwardListCount = m_backForwardList->forwardListCount();
            reply->encode(CoreIPC::In(forwardListCount));
            break;
        }
#if USE(ACCELERATED_COMPOSITING)
        case WebPageProxyMessage::DidChangeAcceleratedCompositing: {
            bool compositing;
            if (!arguments->decode(CoreIPC::Out(compositing)))
                return;

            didChangeAcceleratedCompositing(compositing);
            reply->encode(*(drawingArea()));
            break;
        }
#endif // USE(ACCELERATED_COMPOSITING)

        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

void WebPageProxy::didCreateMainFrame(uint64_t frameID)
{
    ASSERT(!m_mainFrame);

    m_mainFrame = WebFrameProxy::create(this, frameID);
    process()->frameCreated(frameID, m_mainFrame.get());
}

void WebPageProxy::didCreateSubFrame(uint64_t frameID)
{
    ASSERT(m_mainFrame);
    process()->frameCreated(frameID, WebFrameProxy::create(this, frameID).get());
}

void WebPageProxy::didStartProgress()
{
    m_estimatedProgress = 0.0;
    
    m_loaderClient.didStartProgress(this);
}

void WebPageProxy::didChangeProgress(double value)
{
    m_estimatedProgress = value;
    
    m_loaderClient.didChangeProgress(this);
}

void WebPageProxy::didFinishProgress()
{
    m_estimatedProgress = 1.0;

    m_loaderClient.didFinishProgress(this);
}

void WebPageProxy::didStartProvisionalLoadForFrame(WebFrameProxy* frame, const String& url, APIObject* userData)
{
    frame->didStartProvisionalLoad(url);
    m_loaderClient.didStartProvisionalLoadForFrame(this, frame, userData);
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(WebFrameProxy* frame, const String& url, APIObject* userData)
{
    frame->didReceiveServerRedirectForProvisionalLoad(url);
    m_loaderClient.didReceiveServerRedirectForProvisionalLoadForFrame(this, frame, userData);
}

void WebPageProxy::didFailProvisionalLoadForFrame(WebFrameProxy* frame, APIObject* userData)
{
    m_loaderClient.didFailProvisionalLoadWithErrorForFrame(this, frame, userData);
}

void WebPageProxy::didCommitLoadForFrame(WebFrameProxy* frame, const PlatformCertificateInfo& certificateInfo, APIObject* userData)
{
    frame->setCertificateInfo(WebCertificateInfo::create(certificateInfo));
    frame->didCommitLoad();
    m_loaderClient.didCommitLoadForFrame(this, frame, userData);
}

void WebPageProxy::didFinishDocumentLoadForFrame(WebFrameProxy* frame, APIObject* userData)
{
    m_loaderClient.didFinishDocumentLoadForFrame(this, frame, userData);
}

void WebPageProxy::didFinishLoadForFrame(WebFrameProxy* frame, APIObject* userData)
{
    frame->didFinishLoad();
    m_loaderClient.didFinishLoadForFrame(this, frame, userData);
}

void WebPageProxy::didFailLoadForFrame(WebFrameProxy* frame, APIObject* userData)
{
    m_loaderClient.didFailLoadWithErrorForFrame(this, frame, userData);
}

void WebPageProxy::didReceiveTitleForFrame(WebFrameProxy* frame, const String& title, APIObject* userData)
{
    frame->didReceiveTitle(title);

    // Cache the title for the main frame in the page.
    if (frame == m_mainFrame)
        m_pageTitle = title;

    m_loaderClient.didReceiveTitleForFrame(this, title, frame, userData);
}

void WebPageProxy::didFirstLayoutForFrame(WebFrameProxy* frame, APIObject* userData)
{
    m_loaderClient.didFirstLayoutForFrame(this, frame, userData);
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(WebFrameProxy* frame, APIObject* userData)
{
    m_loaderClient.didFirstVisuallyNonEmptyLayoutForFrame(this, frame, userData);
}

// PolicyClient

void WebPageProxy::decidePolicyForNavigationAction(WebFrameProxy* frame, NavigationType navigationType, WebEvent::Modifiers modifiers, const String& url, uint64_t listenerID)
{
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForNavigationAction(this, navigationType, modifiers, url, frame, listener.get()))
        listener->use();
}

void WebPageProxy::decidePolicyForNewWindowAction(WebFrameProxy* frame, NavigationType navigationType, WebEvent::Modifiers modifiers, const String& url, uint64_t listenerID)
{
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForNewWindowAction(this, navigationType, modifiers, url, frame, listener.get()))
        listener->use();
}

void WebPageProxy::decidePolicyForMIMEType(WebFrameProxy* frame, const String& MIMEType, const String& url, uint64_t listenerID)
{
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForMIMEType(this, MIMEType, url, frame, listener.get()))
        listener->use();
}

// FormClient

void WebPageProxy::willSubmitForm(WebFrameProxy* frame, WebFrameProxy* sourceFrame, Vector<std::pair<String, String> >& textFieldValues, APIObject* userData, uint64_t listenerID)
{
    RefPtr<WebFormSubmissionListenerProxy> listener = frame->setUpFormSubmissionListenerProxy(listenerID);
    if (!m_formClient.willSubmitForm(this, frame, sourceFrame, textFieldValues, userData, listener.get()))
        listener->continueSubmission();
}

// UIClient

PassRefPtr<WebPageProxy> WebPageProxy::createNewPage()
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

void WebPageProxy::runJavaScriptAlert(WebFrameProxy* frame, const String& message)
{
    m_uiClient.runJavaScriptAlert(this, message, frame);
}

bool WebPageProxy::runJavaScriptConfirm(WebFrameProxy* frame, const String& message)
{
    return m_uiClient.runJavaScriptConfirm(this, message, frame);
}

String WebPageProxy::runJavaScriptPrompt(WebFrameProxy* frame, const String& message, const String& defaultValue)
{
    return m_uiClient.runJavaScriptPrompt(this, message, defaultValue, frame);
}

void WebPageProxy::setStatusText(const String& text)
{
    m_uiClient.setStatusText(this, text);
}

void WebPageProxy::contentsSizeChanged(WebFrameProxy* frame, const WebCore::IntSize& size)
{
    m_uiClient.contentsSizeChanged(this, size, frame);
}

// BackForwardList

void WebPageProxy::addItemToBackForwardList(WebBackForwardListItem* item)
{
    m_backForwardList->addItem(item);
}

void WebPageProxy::goToItemInBackForwardList(WebBackForwardListItem* item)
{
    m_backForwardList->goToItem(item);
}

// Undo management

void WebPageProxy::registerEditCommandForUndo(uint64_t commandID, EditAction editAction)
{
    registerEditCommandForUndo(WebEditCommandProxy::create(commandID, editAction, this));
}

void WebPageProxy::clearAllEditCommands()
{
    m_pageClient->clearAllEditCommands();
}

void WebPageProxy::registerEditCommandForUndo(PassRefPtr<WebEditCommandProxy> commandProxy)
{
    m_pageClient->registerEditCommand(commandProxy, PageClient::Undo);
}

void WebPageProxy::registerEditCommandForRedo(PassRefPtr<WebEditCommandProxy> commandProxy)
{
    m_pageClient->registerEditCommand(commandProxy, PageClient::Redo);
}

void WebPageProxy::addEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.add(command);
}

void WebPageProxy::removeEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.remove(command);

    if (!isValid())
        return;
    process()->send(WebPageMessage::DidRemoveEditCommand, m_pageID, command->commandID());
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

void WebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    m_pageClient->setCursor(cursor);
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

void WebPageProxy::didGetSourceForFrame(const String& resultString, uint64_t callbackID)
{
    RefPtr<FrameSourceCallback> callback = m_frameSourceCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(resultString.impl());
}

#if USE(ACCELERATED_COMPOSITING)
void WebPageProxy::didChangeAcceleratedCompositing(bool compositing)
{
    if (compositing)
        didEnterAcceleratedCompositing();
    else
        didLeaveAcceleratedCompositing();
}
#endif

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

    Vector<RefPtr<FrameSourceCallback> > frameSourceCallbacks;
    copyValuesToVector(m_frameSourceCallbacks, frameSourceCallbacks);
    m_frameSourceCallbacks.clear();
    for (size_t i = 0, size = frameSourceCallbacks.size(); i < size; ++i)
        frameSourceCallbacks[i]->invalidate();

    Vector<WebEditCommandProxy*> editCommandVector;
    copyToVector(m_editCommandSet, editCommandVector);
    m_editCommandSet.clear();
    for (size_t i = 0, size = editCommandVector.size(); i < size; ++i)
        editCommandVector[i]->invalidate();
    m_pageClient->clearAllEditCommands();

    m_estimatedProgress = 0.0;

    m_pageClient->processDidExit();
    m_loaderClient.processDidExit(this);
}

void WebPageProxy::processDidRevive()
{
    ASSERT(m_pageClient);
    m_pageClient->processDidRevive();
}

#if USE(ACCELERATED_COMPOSITING)
void WebPageProxy::didEnterAcceleratedCompositing()
{
    m_pageClient->pageDidEnterAcceleratedCompositing();
}

void WebPageProxy::didLeaveAcceleratedCompositing()
{
    m_pageClient->pageDidLeaveAcceleratedCompositing();
}
#endif // USE(ACCELERATED_COMPOSITING)

} // namespace WebKit
