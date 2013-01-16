/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "WebTestProxy.h"

#include "WebAccessibilityController.h"
#include "WebAccessibilityNotification.h"
#include "WebAccessibilityObject.h"
#include "WebCachedURLRequest.h"
#include "WebElement.h"
#include "WebEventSender.h"
#include "WebFrame.h"
#include "WebIntent.h"
#include "WebIntentRequest.h"
#include "WebIntentServiceInfo.h"
#include "WebNode.h"
#include "WebRange.h"
#include "WebTestDelegate.h"
#include "WebTestInterfaces.h"
#include "WebTestRunner.h"
#include "WebView.h"
#include "platform/WebCString.h"
#include "platform/WebURLRequest.h"
#include "platform/WebURLResponse.h"
#include <wtf/StringExtras.h>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

namespace {

void printNodeDescription(WebTestDelegate* delegate, const WebNode& node, int exception)
{
    if (exception) {
        delegate->printMessage("ERROR");
        return;
    }
    if (node.isNull()) {
        delegate->printMessage("(null)");
        return;
    }
    delegate->printMessage(node.nodeName().utf8().data());
    const WebNode& parent = node.parentNode();
    if (!parent.isNull()) {
        delegate->printMessage(" > ");
        printNodeDescription(delegate, parent, 0);
    }
}

void printRangeDescription(WebTestDelegate* delegate, const WebRange& range)
{
    if (range.isNull()) {
        delegate->printMessage("(null)");
        return;
    }
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "range from %d of ", range.startOffset());
    delegate->printMessage(buffer);
    int exception = 0;
    WebNode startNode = range.startContainer(exception);
    printNodeDescription(delegate, startNode, exception);
    snprintf(buffer, sizeof(buffer), " to %d of ", range.endOffset());
    delegate->printMessage(buffer);
    WebNode endNode = range.endContainer(exception);
    printNodeDescription(delegate, endNode, exception);
}

string editingActionDescription(WebEditingAction action)
{
    switch (action) {
    case WebKit::WebEditingActionTyped:
        return "WebViewInsertActionTyped";
    case WebKit::WebEditingActionPasted:
        return "WebViewInsertActionPasted";
    case WebKit::WebEditingActionDropped:
        return "WebViewInsertActionDropped";
    }
    return "(UNKNOWN ACTION)";
}

string textAffinityDescription(WebTextAffinity affinity)
{
    switch (affinity) {
    case WebKit::WebTextAffinityUpstream:
        return "NSSelectionAffinityUpstream";
    case WebKit::WebTextAffinityDownstream:
        return "NSSelectionAffinityDownstream";
    }
    return "(UNKNOWN AFFINITY)";
}

void printFrameDescription(WebTestDelegate* delegate, WebFrame* frame)
{
    string name8 = frame->uniqueName().utf8();
    if (frame == frame->view()->mainFrame()) {
        if (!name8.length()) {
            delegate->printMessage("main frame");
            return;
        }
        delegate->printMessage(string("main frame \"") + name8 + "\"");
        return;
    }
    if (!name8.length()) {
        delegate->printMessage("frame (anonymous)");
        return;
    }
    delegate->printMessage(string("frame \"") + name8 + "\"");
}

void printFrameUserGestureStatus(WebTestDelegate* delegate, WebFrame* frame, const char* msg)
{
    bool isUserGesture = frame->isProcessingUserGesture();
    delegate->printMessage(string("Frame with user gesture \"") + (isUserGesture ? "true" : "false") + "\"" + msg);
}

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
string descriptionSuitableForTestResult(const string& url)
{
    if (url.empty() || string::npos == url.find("file://"))
        return url;

    size_t pos = url.rfind('/');
    if (pos == string::npos || !pos)
        return "ERROR:" + url;
    pos = url.rfind('/', pos - 1);
    if (pos == string::npos)
        return "ERROR:" + url;

    return url.substr(pos + 1);
}

void printResponseDescription(WebTestDelegate* delegate, const WebURLResponse& response)
{
    if (response.isNull()) {
        delegate->printMessage("(null)");
        return;
    }
    string url = response.url().spec();
    char data[100];
    snprintf(data, sizeof(data), "%d", response. httpStatusCode());
    delegate->printMessage(string("<NSURLResponse ") + descriptionSuitableForTestResult(url) + ", http status code " + data + ">");
}

string URLDescription(const GURL& url)
{
    if (url.SchemeIs("file"))
        return url.ExtractFileName();
    return url.possibly_invalid_spec();
}

}

WebTestProxyBase::WebTestProxyBase()
    : m_testInterfaces(0)
    , m_delegate(0)
{
}

WebTestProxyBase::~WebTestProxyBase()
{
}

void WebTestProxyBase::setInterfaces(WebTestInterfaces* interfaces)
{
    m_testInterfaces = interfaces;
}

void WebTestProxyBase::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

void WebTestProxyBase::reset()
{
    m_paintRect = WebRect();
    m_resourceIdentifierMap.clear();
}

void WebTestProxyBase::setPaintRect(const WebRect& rect)
{
    m_paintRect = rect;
}

WebRect WebTestProxyBase::paintRect() const
{
    return m_paintRect;
}

void WebTestProxyBase::didInvalidateRect(const WebRect& rect)
{
    // m_paintRect = m_paintRect U rect
    if (rect.isEmpty())
        return;
    if (m_paintRect.isEmpty()) {
        m_paintRect = rect;
        return;
    }
    int left = min(m_paintRect.x, rect.x);
    int top = min(m_paintRect.y, rect.y);
    int right = max(m_paintRect.x + m_paintRect.width, rect.x + rect.width);
    int bottom = max(m_paintRect.y + m_paintRect.height, rect.y + rect.height);
    m_paintRect = WebRect(left, top, right - left, bottom - top);
}

void WebTestProxyBase::didScrollRect(int, int, const WebRect& clipRect)
{
    didInvalidateRect(clipRect);
}

void WebTestProxyBase::scheduleComposite()
{
    m_paintRect = WebRect(0, 0, INT_MAX, INT_MAX);
}

void WebTestProxyBase::scheduleAnimation()
{
    scheduleComposite();
}

void WebTestProxyBase::show(WebNavigationPolicy)
{
    scheduleComposite();
}

void WebTestProxyBase::setWindowRect(const WebRect& rect)
{
    scheduleComposite();
}

void WebTestProxyBase::didAutoResize(const WebSize&)
{
    scheduleComposite();
}

void WebTestProxyBase::postAccessibilityNotification(const WebKit::WebAccessibilityObject& obj, WebKit::WebAccessibilityNotification notification)
{
    if (notification == WebKit::WebAccessibilityNotificationFocusedUIElementChanged)
        m_testInterfaces->accessibilityController()->setFocusedElement(obj);

    const char* notificationName;
    switch (notification) {
    case WebKit::WebAccessibilityNotificationActiveDescendantChanged:
        notificationName = "ActiveDescendantChanged";
        break;
    case WebKit::WebAccessibilityNotificationAutocorrectionOccured:
        notificationName = "AutocorrectionOccured";
        break;
    case WebKit::WebAccessibilityNotificationCheckedStateChanged:
        notificationName = "CheckedStateChanged";
        break;
    case WebKit::WebAccessibilityNotificationChildrenChanged:
        notificationName = "ChildrenChanged";
        break;
    case WebKit::WebAccessibilityNotificationFocusedUIElementChanged:
        notificationName = "FocusedUIElementChanged";
        break;
    case WebKit::WebAccessibilityNotificationLayoutComplete:
        notificationName = "LayoutComplete";
        break;
    case WebKit::WebAccessibilityNotificationLoadComplete:
        notificationName = "LoadComplete";
        break;
    case WebKit::WebAccessibilityNotificationSelectedChildrenChanged:
        notificationName = "SelectedChildrenChanged";
        break;
    case WebKit::WebAccessibilityNotificationSelectedTextChanged:
        notificationName = "SelectedTextChanged";
        break;
    case WebKit::WebAccessibilityNotificationValueChanged:
        notificationName = "ValueChanged";
        break;
    case WebKit::WebAccessibilityNotificationScrolledToAnchor:
        notificationName = "ScrolledToAnchor";
        break;
    case WebKit::WebAccessibilityNotificationLiveRegionChanged:
        notificationName = "LiveRegionChanged";
        break;
    case WebKit::WebAccessibilityNotificationMenuListItemSelected:
        notificationName = "MenuListItemSelected";
        break;
    case WebKit::WebAccessibilityNotificationMenuListValueChanged:
        notificationName = "MenuListValueChanged";
        break;
    case WebKit::WebAccessibilityNotificationRowCountChanged:
        notificationName = "RowCountChanged";
        break;
    case WebKit::WebAccessibilityNotificationRowCollapsed:
        notificationName = "RowCollapsed";
        break;
    case WebKit::WebAccessibilityNotificationRowExpanded:
        notificationName = "RowExpanded";
        break;
    case WebKit::WebAccessibilityNotificationInvalidStatusChanged:
        notificationName = "InvalidStatusChanged";
        break;
    case WebKit::WebAccessibilityNotificationTextChanged:
        notificationName = "TextChanged";
        break;
    case WebKit::WebAccessibilityNotificationAriaAttributeChanged:
        notificationName = "AriaAttributeChanged";
        break;
    default:
        notificationName = "UnknownNotification";
        break;
    }

    m_testInterfaces->accessibilityController()->notificationReceived(obj, notificationName);

    if (m_testInterfaces->accessibilityController()->shouldLogAccessibilityEvents()) {
        string message("AccessibilityNotification - ");
        message += notificationName;

        WebKit::WebNode node = obj.node();
        if (!node.isNull() && node.isElementNode()) {
            WebKit::WebElement element = node.to<WebKit::WebElement>();
            if (element.hasAttribute("id")) {
                message += " - id:";
                message += element.getAttribute("id").utf8().data();
            }
        }

        m_delegate->printMessage(message + "\n");
    }
}

void WebTestProxyBase::startDragging(WebFrame*, const WebDragData& data, WebDragOperationsMask mask, const WebImage&, const WebPoint&)
{
    // When running a test, we need to fake a drag drop operation otherwise
    // Windows waits for real mouse events to know when the drag is over.
    m_testInterfaces->eventSender()->doDragDrop(data, mask);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

// FIXME: Remove the 0 checks for m_testInterfaces->testRunner() once the
// TestInterfaces class owns the TestRunner.

bool WebTestProxyBase::shouldBeginEditing(const WebRange& range)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldBeginEditingInDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

bool WebTestProxyBase::shouldEndEditing(const WebRange& range)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldEndEditingInDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

bool WebTestProxyBase::shouldInsertNode(const WebNode& node, const WebRange& range, WebEditingAction action)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldInsertNode:");
        printNodeDescription(m_delegate, node, 0);
        m_delegate->printMessage(" replacingDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage(string(" givenAction:") + editingActionDescription(action) + "\n");
    }
    return true;
}

bool WebTestProxyBase::shouldInsertText(const WebString& text, const WebRange& range, WebEditingAction action)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage(string("EDITING DELEGATE: shouldInsertText:") + text.utf8().data() + " replacingDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage(string(" givenAction:") + editingActionDescription(action) + "\n");
    }
    return true;
}

bool WebTestProxyBase::shouldChangeSelectedRange(
    const WebRange& fromRange, const WebRange& toRange, WebTextAffinity affinity, bool stillSelecting)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldChangeSelectedDOMRange:");
        printRangeDescription(m_delegate, fromRange);
        m_delegate->printMessage(" toDOMRange:");
        printRangeDescription(m_delegate, toRange);
        m_delegate->printMessage(string(" affinity:") + textAffinityDescription(affinity) + " stillSelecting:" + (stillSelecting ? "TRUE" : "FALSE") + "\n");
    }
    return true;
}

bool WebTestProxyBase::shouldDeleteRange(const WebRange& range)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldDeleteDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

bool WebTestProxyBase::shouldApplyStyle(const WebString& style, const WebRange& range)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage(string("EDITING DELEGATE: shouldApplyStyle:") + style.utf8().data() + " toElementsInDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

void WebTestProxyBase::didBeginEditing()
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidBeginEditing:WebViewDidBeginEditingNotification\n");
}

void WebTestProxyBase::didChangeSelection(bool isEmptySelection)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

void WebTestProxyBase::didChangeContents()
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

void WebTestProxyBase::didEndEditing()
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidEndEditing:WebViewDidEndEditingNotification\n");
}

void WebTestProxyBase::registerIntentService(WebFrame*, const WebIntentServiceInfo& service)
{
    m_delegate->printMessage(string("Registered Web Intent Service: action=") + service.action().utf8().data() + " type=" + service.type().utf8().data() + " title=" + service.title().utf8().data() + " url=" + service.url().spec().data() + " disposition=" + service.disposition().utf8().data() + "\n");
}

void WebTestProxyBase::dispatchIntent(WebFrame* source, const WebIntentRequest& request)
{
    m_delegate->printMessage(string("Received Web Intent: action=") + request.intent().action().utf8().data() + " type=" + request.intent().type().utf8().data() + "\n");
    WebMessagePortChannelArray* ports = request.intent().messagePortChannelsRelease();
    m_delegate->setCurrentWebIntentRequest(request);
    if (ports) {
        char data[100];
        snprintf(data, sizeof(data), "Have %d ports\n", static_cast<int>(ports->size()));
        m_delegate->printMessage(data);
        for (size_t i = 0; i < ports->size(); ++i)
            (*ports)[i]->destroy();
        delete ports;
    }

    if (!request.intent().service().isEmpty())
        m_delegate->printMessage(string("Explicit intent service: ") + request.intent().service().spec().data() + "\n");

    WebVector<WebString> extras = request.intent().extrasNames();
    for (size_t i = 0; i < extras.size(); ++i)
        m_delegate->printMessage(string("Extras[") + extras[i].utf8().data() + "] = " + request.intent().extrasValue(extras[i]).utf8().data() + "\n");

    WebVector<WebURL> suggestions = request.intent().suggestions();
    for (size_t i = 0; i < suggestions.size(); ++i)
        m_delegate->printMessage(string("Have suggestion ") + suggestions[i].spec().data() + "\n");
}

WebView* WebTestProxyBase::createView(WebFrame*, const WebURLRequest& request, const WebWindowFeatures&, const WebString&, WebNavigationPolicy)
{
    if (!m_testInterfaces->testRunner() || !m_testInterfaces->testRunner()->canOpenWindows())
        return 0;
    if (m_testInterfaces->testRunner()->shouldDumpCreateView())
        m_delegate->printMessage(string("createView(") + URLDescription(request.url()) + ")\n");
    return 0;
}

void WebTestProxyBase::setStatusText(const WebString& text)
{
    if (!m_testInterfaces->testRunner() || !m_testInterfaces->testRunner()->shouldDumpStatusCallbacks())
        return;
    m_delegate->printMessage(string("UI DELEGATE STATUS CALLBACK: setStatusText:") + text.utf8().data() + "\n");
}

void WebTestProxyBase::didStopLoading()
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpProgressFinishedCallback())
        m_delegate->printMessage("postProgressFinishedNotification\n");
}

void WebTestProxyBase::willPerformClientRedirect(WebFrame* frame, const WebURL&, const WebURL& to, double, double)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(string(" - willPerformClientRedirectToURL: ") + to.spec().data() + " \n");
    }

    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpUserGestureInFrameLoadCallbacks())
        printFrameUserGestureStatus(m_delegate, frame, " - in willPerformClientRedirect\n");
}

void WebTestProxyBase::didCancelClientRedirect(WebFrame* frame)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didCancelClientRedirectForFrame\n");
    }
}

void WebTestProxyBase::didStartProvisionalLoad(WebFrame* frame)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didStartProvisionalLoadForFrame\n");
    }

    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpUserGestureInFrameLoadCallbacks())
        printFrameUserGestureStatus(m_delegate, frame, " - in didStartProvisionalLoadForFrame\n");

    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->stopProvisionalFrameLoads()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - stopping load in didStartProvisionalLoadForFrame callback\n");
        frame->stopLoading();
    }
}

void WebTestProxyBase::didReceiveServerRedirectForProvisionalLoad(WebFrame* frame)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didReceiveServerRedirectForProvisionalLoadForFrame\n");
    }
}

void WebTestProxyBase::didFailProvisionalLoad(WebFrame* frame, const WebURLError&)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFailProvisionalLoadWithError\n");
    }
}

void WebTestProxyBase::didCommitProvisionalLoad(WebFrame* frame, bool)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didCommitLoadForFrame\n");
    }
}

void WebTestProxyBase::didReceiveTitle(WebFrame* frame, const WebString& title, WebTextDirection direction)
{
    WebCString title8 = title.utf8();

    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(string(" - didReceiveTitle: ") + title8.data() + "\n");
    }

    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpTitleChanges())
        m_delegate->printMessage(string("TITLE CHANGED: '") + title8.data() + "'\n");

    if (m_testInterfaces->testRunner())
        m_testInterfaces->testRunner()->setTitleTextDirection(direction);
}

void WebTestProxyBase::didFinishDocumentLoad(WebFrame* frame)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFinishDocumentLoadForFrame\n");
    } else {
        unsigned pendingUnloadEvents = frame->unloadListenerCount();
        if (pendingUnloadEvents) {
            printFrameDescription(m_delegate, frame);
            char buffer[100];
            snprintf(buffer, sizeof(buffer), " - has %u onunload handler(s)\n", pendingUnloadEvents);
            m_delegate->printMessage(buffer);
        }
    }
}

void WebTestProxyBase::didHandleOnloadEvents(WebFrame* frame)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didHandleOnloadEventsForFrame\n");
    }
}

void WebTestProxyBase::didFailLoad(WebFrame* frame, const WebURLError&)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFailLoadWithError\n");
    }
}

void WebTestProxyBase::didFinishLoad(WebFrame* frame)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFinishLoadForFrame\n");
    }
}

void WebTestProxyBase::didChangeLocationWithinPage(WebFrame* frame)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didChangeLocationWithinPageForFrame\n");
    }
}

void WebTestProxyBase::didDisplayInsecureContent(WebFrame*)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks())
        m_delegate->printMessage("didDisplayInsecureContent\n");
}

void WebTestProxyBase::didRunInsecureContent(WebFrame*, const WebSecurityOrigin&, const WebURL&)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks())
        m_delegate->printMessage("didRunInsecureContent\n");
}

void WebTestProxyBase::didDetectXSS(WebFrame*, const WebURL&, bool)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks())
        m_delegate->printMessage("didDetectXSS\n");
}

void WebTestProxyBase::assignIdentifierToRequest(WebFrame*, unsigned identifier, const WebKit::WebURLRequest& request)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        ASSERT(m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end());
        m_resourceIdentifierMap[identifier] = descriptionSuitableForTestResult(request.url().spec());
    }
}

void WebTestProxyBase::willRequestResource(WebFrame* frame, const WebKit::WebCachedURLRequest& request)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpResourceRequestCallbacks()) {
        printFrameDescription(m_delegate, frame);
        WebElement element = request.initiatorElement();
        if (!element.isNull()) {
            m_delegate->printMessage(" - element with ");
            if (element.hasAttribute("id"))
                m_delegate->printMessage(string("id '") + element.getAttribute("id").utf8().data() + "'");
            else
                m_delegate->printMessage("no id");
        } else
            m_delegate->printMessage(string(" - ") + request.initiatorName().utf8().data());
        m_delegate->printMessage(string(" requested '") + URLDescription(request.urlRequest().url()).c_str() + "'\n");
    }
}

void WebTestProxyBase::willSendRequest(WebFrame*, unsigned identifier, WebKit::WebURLRequest& request, const WebKit::WebURLResponse& redirectResponse)
{
    // Need to use GURL for host() and SchemeIs()
    GURL url = request.url();
    string requestURL = url.possibly_invalid_spec();

    GURL mainDocumentURL = request.firstPartyForCookies();

    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - willSendRequest <NSURLRequest URL ");
        m_delegate->printMessage(descriptionSuitableForTestResult(requestURL).c_str());
        m_delegate->printMessage(", main document URL ");
        m_delegate->printMessage(URLDescription(mainDocumentURL).c_str());
        m_delegate->printMessage(", http method ");
        m_delegate->printMessage(request.httpMethod().utf8().data());
        m_delegate->printMessage("> redirectResponse ");
        printResponseDescription(m_delegate, redirectResponse);
        m_delegate->printMessage("\n");
    }
}

void WebTestProxyBase::didReceiveResponse(WebFrame*, unsigned identifier, const WebKit::WebURLResponse& response)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - didReceiveResponse ");
        printResponseDescription(m_delegate, response);
        m_delegate->printMessage("\n");
    }
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpResourceResponseMIMETypes()) {
        GURL url = response.url();
        WebString mimeType = response.mimeType();
        m_delegate->printMessage(url.ExtractFileName());
        m_delegate->printMessage(" has MIME type ");
        // Simulate NSURLResponse's mapping of empty/unknown MIME types to application/octet-stream
        m_delegate->printMessage(mimeType.isEmpty() ? "application/octet-stream" : mimeType.utf8().data());
        m_delegate->printMessage("\n");
    }
}

void WebTestProxyBase::didFinishResourceLoad(WebFrame*, unsigned identifier)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - didFinishLoading\n");
    }
    m_resourceIdentifierMap.erase(identifier);
}

void WebTestProxyBase::didFailResourceLoad(WebFrame*, unsigned identifier, const WebKit::WebURLError& error)
{
    if (m_testInterfaces->testRunner() && m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - didFailLoadingWithError: ");
        m_delegate->printMessage(m_delegate->makeURLErrorDescription(error));
        m_delegate->printMessage("\n");
    }
    m_resourceIdentifierMap.erase(identifier);
}

}
