/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebViewHost.h"

#include "LayoutTestController.h"
#include "TestNavigationController.h"
#include "TestShell.h"
#include "TestWebWorker.h"
#include "public/WebCString.h"
#include "public/WebConsoleMessage.h"
#include "public/WebContextMenuData.h"
#include "public/WebDataSource.h"
#include "public/WebDragData.h"
#include "public/WebElement.h"
#include "public/WebFrame.h"
#include "public/WebGeolocationServiceMock.h"
#include "public/WebHistoryItem.h"
#include "public/WebNode.h"
#include "public/WebRange.h"
#include "public/WebRect.h"
#include "public/WebScreenInfo.h"
#include "public/WebSize.h"
#include "public/WebStorageNamespace.h"
#include "public/WebURLRequest.h"
#include "public/WebURLResponse.h"
#include "public/WebView.h"
#include "public/WebWindowFeatures.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/support/webkit_support.h"
#include <wtf/Assertions.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;
using namespace WebKit;
using namespace skia;
using namespace std;

static const int screenWidth = 1920;
static const int screenHeight = 1080;
static const int screenUnavailableBorder = 8;

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
static const char* linkClickedString = "link clicked";
static const char* formSubmittedString = "form submitted";
static const char* backForwardString = "back/forward";
static const char* reloadString = "reload";
static const char* formResubmittedString = "form resubmitted";
static const char* otherString = "other";
static const char* illegalString = "illegal value";

static int nextPageID = 1;

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
static string urlSuitableForTestResult(const string& url)
{
    if (url.empty() || string::npos == url.find("file://"))
        return url;

    size_t pos = url.rfind('/');
    if (pos == string::npos) {
#if OS(WINDOWS)
        pos = url.rfind('\\');
        if (pos == string::npos)
            pos = 0;
#else
        pos = 0;
#endif
    }
    string filename = url.substr(pos + 1);
    if (filename.empty())
        return "file:"; // A WebKit test has this in its expected output.
    return filename;
}

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
static string descriptionSuitableForTestResult(const string& url)
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

// Adds a file called "DRTFakeFile" to |data_object| (CF_HDROP).  Use to fake
// dragging a file.
static void addDRTFakeFileToDataObject(WebDragData* dragData)
{
    dragData->appendToFileNames(WebString::fromUTF8("DRTFakeFile"));
}

// Get a debugging string from a WebNavigationType.
static const char* webNavigationTypeToString(WebNavigationType type)
{
    switch (type) {
    case WebKit::WebNavigationTypeLinkClicked:
        return linkClickedString;
    case WebKit::WebNavigationTypeFormSubmitted:
        return formSubmittedString;
    case WebKit::WebNavigationTypeBackForward:
        return backForwardString;
    case WebKit::WebNavigationTypeReload:
        return reloadString;
    case WebKit::WebNavigationTypeFormResubmitted:
        return formResubmittedString;
    case WebKit::WebNavigationTypeOther:
        return otherString;
    }
    return illegalString;
}

static string URLDescription(const GURL& url)
{
    if (url.SchemeIs("file"))
        return url.ExtractFileName();
    return url.possibly_invalid_spec();
}

static void printResponseDescription(const WebURLResponse& response)
{
    if (response.isNull()) {
        fputs("(null)", stdout);
        return;
    }
    string url = response.url().spec();
    printf("<NSURLResponse %s, http status code %d>",
           descriptionSuitableForTestResult(url).c_str(),
           response.httpStatusCode());
}

static void printNodeDescription(const WebNode& node, int exception)
{
    if (exception) {
        fputs("ERROR", stdout);
        return;
    }
    if (node.isNull()) {
        fputs("(null)", stdout);
        return;
    }
    fputs(node.nodeName().utf8().data(), stdout);
    const WebNode& parent = node.parentNode();
    if (!parent.isNull()) {
        fputs(" > ", stdout);
        printNodeDescription(parent, 0);
    }
}

static void printRangeDescription(const WebRange& range)
{
    if (range.isNull()) {
        fputs("(null)", stdout);
        return;
    }
    printf("range from %d of ", range.startOffset());
    int exception = 0;
    WebNode startNode = range.startContainer(exception);
    printNodeDescription(startNode, exception);
    printf(" to %d of ", range.endOffset());
    WebNode endNode = range.endContainer(exception);
    printNodeDescription(endNode, exception);
}

static string editingActionDescription(WebEditingAction action)
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

static string textAffinityDescription(WebTextAffinity affinity)
{
    switch (affinity) {
    case WebKit::WebTextAffinityUpstream:
        return "NSSelectionAffinityUpstream";
    case WebKit::WebTextAffinityDownstream:
        return "NSSelectionAffinityDownstream";
    }
    return "(UNKNOWN AFFINITY)";
}

// WebViewClient -------------------------------------------------------------

WebView* WebViewHost::createView(WebFrame*, const WebWindowFeatures&, const WebString&)
{
    if (!layoutTestController()->canOpenWindows())
        return 0;
    return m_shell->createWebView()->webView();
}

WebWidget* WebViewHost::createPopupMenu(WebPopupType)
{
    return 0;
}

WebWidget* WebViewHost::createPopupMenu(const WebPopupMenuInfo&)
{
    return 0;
}

WebStorageNamespace* WebViewHost::createSessionStorageNamespace(unsigned quota)
{
    return WebKit::WebStorageNamespace::createSessionStorageNamespace(quota);
}

void WebViewHost::didAddMessageToConsole(const WebConsoleMessage& message, const WebString& sourceName, unsigned sourceLine)
{
    // This matches win DumpRenderTree's UIDelegate.cpp.
    string newMessage;
    if (!message.text.isEmpty()) {
        newMessage = message.text.utf8();
        size_t fileProtocol = newMessage.find("file://");
        if (fileProtocol != string::npos) {
            newMessage = newMessage.substr(0, fileProtocol)
                + urlSuitableForTestResult(newMessage.substr(fileProtocol));
        }
    }
    printf("CONSOLE MESSAGE: line %d: %s\n", sourceLine, newMessage.data());
}

void WebViewHost::didStartLoading()
{
    m_shell->setIsLoading(true);
}

void WebViewHost::didStopLoading()
{
    m_shell->setIsLoading(false);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests.  See EditingDelegate.m in DumpRenderTree.

bool WebViewHost::shouldBeginEditing(const WebRange& range)
{
    if (layoutTestController()->shouldDumpEditingCallbacks()) {
        fputs("EDITING DELEGATE: shouldBeginEditingInDOMRange:", stdout);
        printRangeDescription(range);
        fputs("\n", stdout);
    }
    return layoutTestController()->acceptsEditing();
}

bool WebViewHost::shouldEndEditing(const WebRange& range)
{
    if (layoutTestController()->shouldDumpEditingCallbacks()) {
        fputs("EDITING DELEGATE: shouldEndEditingInDOMRange:", stdout);
        printRangeDescription(range);
        fputs("\n", stdout);
    }
    return layoutTestController()->acceptsEditing();
}

bool WebViewHost::shouldInsertNode(const WebNode& node, const WebRange& range, WebEditingAction action)
{
    if (layoutTestController()->shouldDumpEditingCallbacks()) {
        fputs("EDITING DELEGATE: shouldInsertNode:", stdout);
        printNodeDescription(node, 0);
        fputs(" replacingDOMRange:", stdout);
        printRangeDescription(range);
        printf(" givenAction:%s\n", editingActionDescription(action).c_str());
    }
    return layoutTestController()->acceptsEditing();
}

bool WebViewHost::shouldInsertText(const WebString& text, const WebRange& range, WebEditingAction action)
{
    if (layoutTestController()->shouldDumpEditingCallbacks()) {
        printf("EDITING DELEGATE: shouldInsertText:%s replacingDOMRange:", text.utf8().data());
        printRangeDescription(range);
        printf(" givenAction:%s\n", editingActionDescription(action).c_str());
    }
    return layoutTestController()->acceptsEditing();
}

bool WebViewHost::shouldChangeSelectedRange(
    const WebRange& fromRange, const WebRange& toRange, WebTextAffinity affinity, bool stillSelecting)
{
    if (layoutTestController()->shouldDumpEditingCallbacks()) {
        fputs("EDITING DELEGATE: shouldChangeSelectedDOMRange:", stdout);
        printRangeDescription(fromRange);
        fputs(" toDOMRange:", stdout);
        printRangeDescription(toRange);
        printf(" affinity:%s stillSelecting:%s\n",
               textAffinityDescription(affinity).c_str(),
               (stillSelecting ? "TRUE" : "FALSE"));
    }
    return layoutTestController()->acceptsEditing();
}

bool WebViewHost::shouldDeleteRange(const WebRange& range)
{
    if (layoutTestController()->shouldDumpEditingCallbacks()) {
        fputs("EDITING DELEGATE: shouldDeleteDOMRange:", stdout);
        printRangeDescription(range);
        fputs("\n", stdout);
    }
    return layoutTestController()->acceptsEditing();
}

bool WebViewHost::shouldApplyStyle(const WebString& style, const WebRange& range)
{
    if (layoutTestController()->shouldDumpEditingCallbacks()) {
        printf("EDITING DELEGATE: shouldApplyStyle:%s toElementsInDOMRange:", style.utf8().data());
        printRangeDescription(range);
        fputs("\n", stdout);
    }
    return layoutTestController()->acceptsEditing();
}

bool WebViewHost::isSmartInsertDeleteEnabled()
{
    return m_smartInsertDeleteEnabled;
}

bool WebViewHost::isSelectTrailingWhitespaceEnabled()
{
    return m_selectTrailingWhitespaceEnabled;
}

void WebViewHost::didBeginEditing()
{
    if (!layoutTestController()->shouldDumpEditingCallbacks())
        return;
    fputs("EDITING DELEGATE: webViewDidBeginEditing:WebViewDidBeginEditingNotification\n", stdout);
}

void WebViewHost::didChangeSelection(bool isEmptySelection)
{
    if (layoutTestController()->shouldDumpEditingCallbacks())
        fputs("EDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n", stdout);
    // No need to update clipboard with the selected text in DRT.
}

void WebViewHost::didChangeContents()
{
    if (!layoutTestController()->shouldDumpEditingCallbacks())
        return;
    fputs("EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n", stdout);
}

void WebViewHost::didEndEditing()
{
    if (!layoutTestController()->shouldDumpEditingCallbacks())
        return;
    fputs("EDITING DELEGATE: webViewDidEndEditing:WebViewDidEndEditingNotification\n", stdout);
}

bool WebViewHost::handleCurrentKeyboardEvent()
{
    if (m_editCommandName.empty())
        return false;
    WebFrame* frame = webView()->focusedFrame();
    if (!frame)
        return false;

    return frame->executeCommand(WebString::fromUTF8(m_editCommandName), WebString::fromUTF8(m_editCommandValue));
}

void WebViewHost::spellCheck(const WebString& text, int& misspelledOffset, int& misspelledLength)
{
    // Check the spelling of the given text.
#if OS(MAC_OS_X)
    // FIXME: rebaseline layout-test results of Windows and Linux so we
    // can enable this mock spellchecker on them.
    m_spellcheck.spellCheckWord(text, &misspelledOffset, &misspelledLength);
#endif
}

WebString WebViewHost::autoCorrectWord(const WebString&)
{
    // Returns an empty string as Mac WebKit ('WebKitSupport/WebEditorClient.mm')
    // does. (If this function returns a non-empty string, WebKit replaces the
    // given misspelled string with the result one. This process executes some
    // editor commands and causes layout-test failures.)
    return WebString();
}

void WebViewHost::runModalAlertDialog(WebFrame*, const WebString& message)
{
    printf("ALERT: %s\n", message.utf8().data());
}

bool WebViewHost::runModalConfirmDialog(WebFrame*, const WebString& message)
{
    printf("CONFIRM: %s\n", message.utf8().data());
    return true;
}

bool WebViewHost::runModalPromptDialog(WebFrame* frame, const WebString& message,
                                       const WebString& defaultValue, WebString*)
{
    printf("PROMPT: %s, default text: %s\n", message.utf8().data(), defaultValue.utf8().data());
    return true;
}

bool WebViewHost::runModalBeforeUnloadDialog(WebFrame*, const WebString&)
{
    return true; // Allow window closure.
}

void WebViewHost::showContextMenu(WebFrame*, const WebContextMenuData&)
{
}


void WebViewHost::setStatusText(const WebString& text)
{
    if (!layoutTestController()->shouldDumpStatusCallbacks())
        return;
    // When running tests, write to stdout.
    printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", text.utf8().data());
}

void WebViewHost::startDragging(const WebDragData& data, WebDragOperationsMask mask, const WebImage&, const WebPoint&)
{
    WebDragData mutableDragData = data;
    if (layoutTestController()->shouldAddFileToPasteboard()) {
        // Add a file called DRTFakeFile to the drag&drop clipboard.
        addDRTFakeFileToDataObject(&mutableDragData);
    }

    // When running a test, we need to fake a drag drop operation otherwise
    // Windows waits for real mouse events to know when the drag is over.
    m_shell->eventSender()->doDragDrop(mutableDragData, mask);
}

void WebViewHost::navigateBackForwardSoon(int offset)
{
    navigationController()->goToOffset(offset);
}

int WebViewHost::historyBackListCount()
{
    return navigationController()->lastCommittedEntryIndex();
}

int WebViewHost::historyForwardListCount()
{
    int currentIndex =navigationController()->lastCommittedEntryIndex();
    return navigationController()->entryCount() - currentIndex - 1;
}

void WebViewHost::focusAccessibilityObject(const WebAccessibilityObject& object)
{
    m_shell->accessibilityController()->setFocusedElement(object);
}

void WebViewHost::postAccessibilityNotification(const WebAccessibilityObject& obj, WebAccessibilityNotification notification)
{
    printf("AccessibilityNotification - ");
    
    if (m_shell->accessibilityController()->shouldDumpAccessibilityNotifications()) {
        switch (notification) {
        case WebAccessibilityNotificationActiveDescendantChanged:
            printf("ActiveDescendantChanged");
            break;
        case WebAccessibilityNotificationCheckedStateChanged:
            printf("CheckedStateChanged");
            break;
        case WebAccessibilityNotificationChildrenChanged:
            printf("ChildrenChanged");
            break;
        case WebAccessibilityNotificationFocusedUIElementChanged:
            printf("FocusedUIElementChanged");
            break;
        case WebAccessibilityNotificationLayoutComplete:
            printf("LayoutComplete");
            break;
        case WebAccessibilityNotificationLoadComplete:
            printf("LoadComplete");
            break;
        case WebAccessibilityNotificationSelectedChildrenChanged:
            printf("SelectedChildrenChanged");
            break;
        case WebAccessibilityNotificationSelectedTextChanged:
            printf("SelectedTextChanged");
            break;
        case WebAccessibilityNotificationValueChanged:
            printf("ValueChanged");
            break;
        case WebAccessibilityNotificationScrolledToAnchor:
            printf("ScrolledToAnchor");
            break;
        case WebAccessibilityNotificationLiveRegionChanged:
            printf("LiveRegionChanged");
            break;
        case WebAccessibilityNotificationMenuListValueChanged:
            printf("MenuListValueChanged");
            break;
        case WebAccessibilityNotificationRowCountChanged:
            printf("RowCountChanged");
            break;
        case WebAccessibilityNotificationRowCollapsed:
            printf("RowCollapsed");
            break;
        case WebAccessibilityNotificationRowExpanded:
            printf("RowExpanded");
            break;
        }

        WebKit::WebNode node = obj.node();
        if (!node.isNull() && node.isElementNode()) {
            WebKit::WebElement element = node.to<WebKit::WebElement>();
            if (element.hasAttribute("id"))
                printf(" - id:%s", element.getAttribute("id").utf8().data());
        }

        printf("\n");
    }
}

WebNotificationPresenter* WebViewHost::notificationPresenter()
{
    return m_shell->notificationPresenter();
}

WebKit::WebGeolocationService* WebViewHost::geolocationService()
{
    if (!m_geolocationServiceMock.get())
        m_geolocationServiceMock.set(WebGeolocationServiceMock::createWebGeolocationServiceMock());
    return m_geolocationServiceMock.get();
}

WebSpeechInputController* WebViewHost::speechInputController(WebKit::WebSpeechInputListener* listener)
{
    return m_shell->layoutTestController()->speechInputController(listener);
}

WebKit::WebDeviceOrientationClient* WebViewHost::deviceOrientationClient()
{
    return m_shell->layoutTestController()->deviceOrientationClient();
}

// WebWidgetClient -----------------------------------------------------------

void WebViewHost::didInvalidateRect(const WebRect& rect)
{
    if (m_isPainting)
        LOG_ERROR("unexpected invalidation while painting");
    updatePaintRect(rect);
}

void WebViewHost::didScrollRect(int, int, const WebRect& clipRect)
{
    // This is used for optimizing painting when the renderer is scrolled. We're
    // currently not doing any optimizations so just invalidate the region.
    didInvalidateRect(clipRect);
}

void WebViewHost::didFocus()
{
    m_shell->setFocus(webWidget(), true);
}

void WebViewHost::didBlur()
{
    m_shell->setFocus(webWidget(), false);
}

WebScreenInfo WebViewHost::screenInfo()
{
    // We don't need to set actual values.
    WebScreenInfo info;
    info.depth = 24;
    info.depthPerComponent = 8;
    info.isMonochrome = false;
    info.rect = WebRect(0, 0, screenWidth, screenHeight);
    // Use values different from info.rect for testing.
    info.availableRect = WebRect(screenUnavailableBorder, screenUnavailableBorder,
                                 screenWidth - screenUnavailableBorder * 2,
                                 screenHeight - screenUnavailableBorder * 2);
    return info;
}

void WebViewHost::show(WebNavigationPolicy)
{
    m_hasWindow = true;
    WebSize size = webWidget()->size();
    updatePaintRect(WebRect(0, 0, size.width, size.height));
}

void WebViewHost::closeWidgetSoon()
{
    m_hasWindow = false;
    m_shell->closeWindow(this);
}

void WebViewHost::didChangeCursor(const WebCursorInfo& cursorInfo)
{
    if (!hasWindow())
        return;
    m_currentCursor = cursorInfo;
}

WebRect WebViewHost::windowRect()
{
    return m_windowRect;
}

void WebViewHost::setWindowRect(const WebRect& rect)
{
    m_windowRect = rect;
    const int border2 = TestShell::virtualWindowBorder * 2;
    if (m_windowRect.width <= border2)
        m_windowRect.width = 1 + border2;
    if (m_windowRect.height <= border2)
        m_windowRect.height = 1 + border2;
    int width = m_windowRect.width - border2;
    int height = m_windowRect.height - border2;
    discardBackingStore();
    webWidget()->resize(WebSize(width, height));
    updatePaintRect(WebRect(0, 0, width, height));
}

WebRect WebViewHost::rootWindowRect()
{
    return windowRect();
}

WebRect WebViewHost::windowResizerRect()
{
    // Not necessary.
    return WebRect();
}

void WebViewHost::runModal()
{
    // FIXME: Should we implement this in DRT?
}

// WebFrameClient ------------------------------------------------------------

WebPlugin* WebViewHost::createPlugin(WebFrame* frame, const WebPluginParams& params)
{
    return webkit_support::CreateWebPlugin(frame, params);
}

WebWorker* WebViewHost::createWorker(WebFrame*, WebWorkerClient*)
{
    return new TestWebWorker();
}

WebMediaPlayer* WebViewHost::createMediaPlayer(WebFrame* frame, WebMediaPlayerClient* client)
{
    return webkit_support::CreateMediaPlayer(frame, client);
}

WebApplicationCacheHost* WebViewHost::createApplicationCacheHost(WebFrame* frame, WebApplicationCacheHostClient* client)
{
    return webkit_support::CreateApplicationCacheHost(frame, client);
}

bool WebViewHost::allowPlugins(WebFrame* frame, bool enabledPerSettings)
{
    return enabledPerSettings;
}

bool WebViewHost::allowImages(WebFrame* frame, bool enabledPerSettings)
{
    return enabledPerSettings;
}

void WebViewHost::loadURLExternally(WebFrame*, const WebURLRequest& request, WebNavigationPolicy policy)
{
    ASSERT(policy !=  WebKit::WebNavigationPolicyCurrentTab);
    WebViewHost* another = m_shell->createNewWindow(request.url());
    if (another)
        another->show(policy);
}

WebNavigationPolicy WebViewHost::decidePolicyForNavigation(
    WebFrame*, const WebURLRequest& request,
    WebNavigationType type, const WebNode& originatingNode,
    WebNavigationPolicy defaultPolicy, bool isRedirect)
{
    WebNavigationPolicy result;
    if (!m_policyDelegateEnabled)
        return defaultPolicy;

    printf("Policy delegate: attempt to load %s with navigation type '%s'",
           URLDescription(request.url()).c_str(), webNavigationTypeToString(type));
    if (!originatingNode.isNull()) {
        fputs(" originating from ", stdout);
        printNodeDescription(originatingNode, 0);
    }
    fputs("\n", stdout);
    if (m_policyDelegateIsPermissive)
        result = WebKit::WebNavigationPolicyCurrentTab;
    else
        result = WebKit::WebNavigationPolicyIgnore;

    if (m_policyDelegateShouldNotifyDone)
        layoutTestController()->policyDelegateDone();
    return result;
}

bool WebViewHost::canHandleRequest(WebFrame*, const WebURLRequest& request)
{
    GURL url = request.url();
    // Just reject the scheme used in
    // LayoutTests/http/tests/misc/redirect-to-external-url.html
    return !url.SchemeIs("spaceballs");
}

WebURLError WebViewHost::cannotHandleRequestError(WebFrame*, const WebURLRequest& request)
{
    WebURLError error;
    // A WebKit layout test expects the following values.
    // unableToImplementPolicyWithError() below prints them.
    error.domain = WebString::fromUTF8("WebKitErrorDomain");
    error.reason = 101;
    error.unreachableURL = request.url();
    return error;
}

WebURLError WebViewHost::cancelledError(WebFrame*, const WebURLRequest& request)
{
    return webkit_support::CreateCancelledError(request);
}

void WebViewHost::unableToImplementPolicyWithError(WebFrame* frame, const WebURLError& error)
{
    printf("Policy delegate: unable to implement policy with error domain '%s', "
           "error code %d, in frame '%s'\n",
           error.domain.utf8().data(), error.reason, frame->name().utf8().data());
}

void WebViewHost::willPerformClientRedirect(WebFrame* frame, const WebURL& from, const WebURL& to,
                                            double interval, double fire_time)
{
    if (!m_shell->shouldDumpFrameLoadCallbacks())
        return;
    printFrameDescription(frame);
    printf(" - willPerformClientRedirectToURL: %s \n", to.spec().data());
}

void WebViewHost::didCancelClientRedirect(WebFrame* frame)
{
    if (!m_shell->shouldDumpFrameLoadCallbacks())
        return;
    printFrameDescription(frame);
    fputs(" - didCancelClientRedirectForFrame\n", stdout);
}

void WebViewHost::didCreateDataSource(WebFrame*, WebDataSource* ds)
{
    ds->setExtraData(m_pendingExtraData.leakPtr());
}

void WebViewHost::didStartProvisionalLoad(WebFrame* frame)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didStartProvisionalLoadForFrame\n", stdout);
    }

    if (!m_topLoadingFrame)
        m_topLoadingFrame = frame;

    if (layoutTestController()->stopProvisionalFrameLoads()) {
        printFrameDescription(frame);
        fputs(" - stopping load in didStartProvisionalLoadForFrame callback\n", stdout);
        frame->stopLoading();
    }
    updateAddressBar(frame->view());
}

void WebViewHost::didReceiveServerRedirectForProvisionalLoad(WebFrame* frame)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didReceiveServerRedirectForProvisionalLoadForFrame\n", stdout);
    }
    updateAddressBar(frame->view());
}

void WebViewHost::didFailProvisionalLoad(WebFrame* frame, const WebURLError& error)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didFailProvisionalLoadWithError\n", stdout);
    }

    locationChangeDone(frame);

    // Don't display an error page if we're running layout tests, because
    // DumpRenderTree doesn't.
}

void WebViewHost::didCommitProvisionalLoad(WebFrame* frame, bool isNewNavigation)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didCommitLoadForFrame\n", stdout);
    }
    updateForCommittedLoad(frame, isNewNavigation);
}

void WebViewHost::didClearWindowObject(WebFrame* frame)
{
    m_shell->bindJSObjectsToWindow(frame);
}

void WebViewHost::didReceiveTitle(WebFrame* frame, const WebString& title)
{
    WebCString title8 = title.utf8();

    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        printf(" - didReceiveTitle: %s\n", title8.data());
    }

    if (layoutTestController()->shouldDumpTitleChanges())
        printf("TITLE CHANGED: %s\n", title8.data());

    setPageTitle(title);
}

void WebViewHost::didFinishDocumentLoad(WebFrame* frame)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didFinishDocumentLoadForFrame\n", stdout);
    } else {
        unsigned pendingUnloadEvents = frame->unloadListenerCount();
        if (pendingUnloadEvents) {
            printFrameDescription(frame);
            printf(" - has %u onunload handler(s)\n", pendingUnloadEvents);
        }
    }
}

void WebViewHost::didHandleOnloadEvents(WebFrame* frame)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didHandleOnloadEventsForFrame\n", stdout);
    }
}

void WebViewHost::didFailLoad(WebFrame* frame, const WebURLError& error)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didFailLoadWithError\n", stdout);
    }
    locationChangeDone(frame);
}

void WebViewHost::didFinishLoad(WebFrame* frame)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didFinishLoadForFrame\n", stdout);
    }
    updateAddressBar(frame->view());
    locationChangeDone(frame);
}

void WebViewHost::didNavigateWithinPage(WebFrame* frame, bool isNewNavigation)
{
    frame->dataSource()->setExtraData(m_pendingExtraData.leakPtr());

    updateForCommittedLoad(frame, isNewNavigation);
}

void WebViewHost::didChangeLocationWithinPage(WebFrame* frame)
{
    if (m_shell->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(frame);
        fputs(" - didChangeLocationWithinPageForFrame\n", stdout);
    }
}

void WebViewHost::assignIdentifierToRequest(WebFrame*, unsigned identifier, const WebURLRequest& request)
{
    if (!m_shell->shouldDumpResourceLoadCallbacks())
        return;
    m_resourceIdentifierMap.set(identifier, descriptionSuitableForTestResult(request.url().spec()));
}

void WebViewHost::willSendRequest(WebFrame*, unsigned identifier, WebURLRequest& request, const WebURLResponse& redirectResponse)
{
    // Need to use GURL for host() and SchemeIs()
    GURL url = request.url();
    string requestURL = url.possibly_invalid_spec();

    if (layoutTestController()->shouldDumpResourceLoadCallbacks()) {
        GURL mainDocumentURL = request.firstPartyForCookies();
        printResourceDescription(identifier);
        printf(" - willSendRequest <NSURLRequest URL %s, main document URL %s,"
               " http method %s> redirectResponse ",
               descriptionSuitableForTestResult(requestURL).c_str(),
               URLDescription(mainDocumentURL).c_str(),
               request.httpMethod().utf8().data());
        printResponseDescription(redirectResponse);
        fputs("\n", stdout);
    }

    if (!redirectResponse.isNull() && m_blocksRedirects) {
        fputs("Returning null for this redirect\n", stdout);
        // To block the request, we set its URL to an empty one.
        request.setURL(WebURL());
        return;
    }

    if (m_requestReturnNull) {
        // To block the request, we set its URL to an empty one.
        request.setURL(WebURL());
        return;
    }

    string host = url.host();
    // 255.255.255.255 is used in some tests that expect to get back an error.
    if (!host.empty() && (url.SchemeIs("http") || url.SchemeIs("https"))
        && host != "127.0.0.1"
        && host != "255.255.255.255"
        && host != "localhost"
        && !m_shell->allowExternalPages()) {
        printf("Blocked access to external URL %s\n", requestURL.c_str());

        // To block the request, we set its URL to an empty one.
        request.setURL(WebURL());
        return;
    }

    HashSet<String>::const_iterator end = m_clearHeaders.end();
    for (HashSet<String>::const_iterator header = m_clearHeaders.begin(); header != end; ++header)
        request.clearHTTPHeaderField(WebString(header->characters(), header->length()));

    // Set the new substituted URL.
    request.setURL(webkit_support::RewriteLayoutTestsURL(request.url().spec()));
}

void WebViewHost::didReceiveResponse(WebFrame*, unsigned identifier, const WebURLResponse& response)
{
    if (m_shell->shouldDumpResourceLoadCallbacks()) {
        printResourceDescription(identifier);
        fputs(" - didReceiveResponse ", stdout);
        printResponseDescription(response);
        fputs("\n", stdout);
    }
    if (m_shell->shouldDumpResourceResponseMIMETypes()) {
        GURL url = response.url();
        WebString mimeType = response.mimeType();
        printf("%s has MIME type %s\n",
            url.ExtractFileName().c_str(),
            // Simulate NSURLResponse's mapping of empty/unknown MIME types to application/octet-stream
            mimeType.isEmpty() ? "application/octet-stream" : mimeType.utf8().data());
    }
}

void WebViewHost::didFinishResourceLoad(WebFrame*, unsigned identifier)
{
    if (m_shell->shouldDumpResourceLoadCallbacks()) {
        printResourceDescription(identifier);
        fputs(" - didFinishLoading\n", stdout);
    }
    m_resourceIdentifierMap.remove(identifier);
}

void WebViewHost::didFailResourceLoad(WebFrame*, unsigned identifier, const WebURLError& error)
{
    if (m_shell->shouldDumpResourceLoadCallbacks()) {
        printResourceDescription(identifier);
        fputs(" - didFailLoadingWithError: ", stdout);
        fputs(webkit_support::MakeURLErrorDescription(error).c_str(), stdout);
        fputs("\n", stdout);
    }
    m_resourceIdentifierMap.remove(identifier);
}

void WebViewHost::didDisplayInsecureContent(WebFrame*)
{
    if (m_shell->shouldDumpFrameLoadCallbacks())
        fputs("didDisplayInsecureContent\n", stdout);
}

void WebViewHost::didRunInsecureContent(WebFrame*, const WebSecurityOrigin& origin)
{
    if (m_shell->shouldDumpFrameLoadCallbacks())
        fputs("didRunInsecureContent\n", stdout);
}

bool WebViewHost::allowScript(WebFrame*, bool enabledPerSettings)
{
    return enabledPerSettings;
}

// Public functions -----------------------------------------------------------

WebViewHost::WebViewHost(TestShell* shell)
    : m_policyDelegateEnabled(false)
    , m_policyDelegateIsPermissive(false)
    , m_policyDelegateShouldNotifyDone(false)
    , m_shell(shell)
    , m_topLoadingFrame(0)
    , m_hasWindow(false)
    , m_pageId(-1)
    , m_lastPageIdUpdated(-1)
    , m_smartInsertDeleteEnabled(true)
#if OS(WINDOWS)
    , m_selectTrailingWhitespaceEnabled(true)
#else
    , m_selectTrailingWhitespaceEnabled(false)
#endif
    , m_blocksRedirects(false)
    , m_requestReturnNull(false)
    , m_isPainting(false)
    , m_webWidget(0)
{
    m_navigationController.set(new TestNavigationController(this));
}

WebViewHost::~WebViewHost()
{
}

WebView* WebViewHost::webView() const
{
    ASSERT(m_webWidget);
    // DRT does not support popup widgets. So m_webWidget is always a WebView.
    return static_cast<WebView*>(m_webWidget);
}

WebWidget* WebViewHost::webWidget() const
{
    ASSERT(m_webWidget);
    return m_webWidget;
}

void WebViewHost::reset()
{
    // Do a little placement new dance...
    TestShell* shell = m_shell;
    WebWidget* widget = m_webWidget;
    this->~WebViewHost();
    new (this) WebViewHost(shell);
    setWebWidget(widget);
    webView()->mainFrame()->setName(WebString());
}

void WebViewHost::setSelectTrailingWhitespaceEnabled(bool enabled)
{
    m_selectTrailingWhitespaceEnabled = enabled;
    // In upstream WebKit, smart insert/delete is mutually exclusive with select
    // trailing whitespace, however, we allow both because Chromium on Windows
    // allows both.
}

void WebViewHost::setSmartInsertDeleteEnabled(bool enabled)
{
    m_smartInsertDeleteEnabled = enabled;
    // In upstream WebKit, smart insert/delete is mutually exclusive with select
    // trailing whitespace, however, we allow both because Chromium on Windows
    // allows both.
}

void WebViewHost::setCustomPolicyDelegate(bool isCustom, bool isPermissive)
{
    m_policyDelegateEnabled = isCustom;
    m_policyDelegateIsPermissive = isPermissive;
}

void WebViewHost::waitForPolicyDelegate()
{
    m_policyDelegateEnabled = true;
    m_policyDelegateShouldNotifyDone = true;
}

void WebViewHost::setEditCommand(const string& name, const string& value)
{
    m_editCommandName = name;
    m_editCommandValue = value;
}

void WebViewHost::clearEditCommand()
{
    m_editCommandName.clear();
    m_editCommandValue.clear();
}

void WebViewHost::loadURLForFrame(const WebURL& url, const WebString& frameName)
{
    if (!url.isValid())
        return;
    TestShell::resizeWindowForTest(this, url);
    navigationController()->loadEntry(TestNavigationEntry::create(-1, url, WebString(), frameName).get());
}

bool WebViewHost::navigate(const TestNavigationEntry& entry, bool reload)
{
    // Get the right target frame for the entry.
    WebFrame* frame = webView()->mainFrame();
    if (!entry.targetFrame().isEmpty())
        frame = webView()->findFrameByName(entry.targetFrame());

    // TODO(mpcomplete): should we clear the target frame, or should
    // back/forward navigations maintain the target frame?

    // A navigation resulting from loading a javascript URL should not be
    // treated as a browser initiated event.  Instead, we want it to look as if
    // the page initiated any load resulting from JS execution.
    if (!GURL(entry.URL()).SchemeIs("javascript"))
        setPendingExtraData(new TestShellExtraData(entry.pageID()));

    // If we are reloading, then WebKit will use the state of the current page.
    // Otherwise, we give it the state to navigate to.
    if (reload) {
        frame->reload(false);
    } else if (!entry.contentState().isNull()) {
        ASSERT(entry.pageID() != -1);
        frame->loadHistoryItem(entry.contentState());
    } else {
        ASSERT(entry.pageID() == -1);
        frame->loadRequest(WebURLRequest(entry.URL()));
    }

    // In case LoadRequest failed before DidCreateDataSource was called.
    setPendingExtraData(0);

    // Restore focus to the main frame prior to loading new request.
    // This makes sure that we don't have a focused iframe. Otherwise, that
    // iframe would keep focus when the SetFocus called immediately after
    // LoadRequest, thus making some tests fail (see http://b/issue?id=845337
    // for more details).
    webView()->setFocusedFrame(frame);
    m_shell->setFocus(webView(), true);

    return true;
}

// Private functions ----------------------------------------------------------

LayoutTestController* WebViewHost::layoutTestController() const
{
    return m_shell->layoutTestController();
}

void WebViewHost::updateAddressBar(WebView* webView)
{
    WebFrame* mainFrame = webView->mainFrame();
    WebDataSource* dataSource = mainFrame->dataSource();
    if (!dataSource)
        dataSource = mainFrame->provisionalDataSource();
    if (!dataSource)
        return;

    setAddressBarURL(dataSource->request().url());
}

void WebViewHost::locationChangeDone(WebFrame* frame)
{
    if (frame != m_topLoadingFrame)
        return;
    m_topLoadingFrame = 0;
    layoutTestController()->locationChangeDone();
}

void WebViewHost::updateForCommittedLoad(WebFrame* frame, bool isNewNavigation)
{
    // Code duplicated from RenderView::DidCommitLoadForFrame.
    TestShellExtraData* extraData = static_cast<TestShellExtraData*>(frame->dataSource()->extraData());

    if (isNewNavigation) {
        // New navigation.
        updateSessionHistory(frame);
        m_pageId = nextPageID++;
    } else if (extraData && extraData->pendingPageID != -1 && !extraData->requestCommitted) {
        // This is a successful session history navigation!
        updateSessionHistory(frame);
        m_pageId = extraData->pendingPageID;
    }

    // Don't update session history multiple times.
    if (extraData)
        extraData->requestCommitted = true;

    updateURL(frame);
}

void WebViewHost::updateURL(WebFrame* frame)
{
    WebDataSource* ds = frame->dataSource();
    ASSERT(ds);
    const WebURLRequest& request = ds->request();
    RefPtr<TestNavigationEntry> entry(TestNavigationEntry::create());

    // The referrer will be empty on https->http transitions. It
    // would be nice if we could get the real referrer from somewhere.
    entry->setPageID(m_pageId);
    if (ds->hasUnreachableURL())
        entry->setURL(ds->unreachableURL());
    else
        entry->setURL(request.url());

    const WebHistoryItem& historyItem = frame->currentHistoryItem();
    if (!historyItem.isNull())
        entry->setContentState(historyItem);

    navigationController()->didNavigateToEntry(entry.get());
    updateAddressBar(frame->view());
    m_lastPageIdUpdated = max(m_lastPageIdUpdated, m_pageId);
}

void WebViewHost::updateSessionHistory(WebFrame* frame)
{
    // If we have a valid page ID at this point, then it corresponds to the page
    // we are navigating away from.  Otherwise, this is the first navigation, so
    // there is no past session history to record.
    if (m_pageId == -1)
        return;

    TestNavigationEntry* entry = navigationController()->entryWithPageID(m_pageId);
    if (!entry)
        return;

    const WebHistoryItem& historyItem = webView()->mainFrame()->previousHistoryItem();
    if (historyItem.isNull())
        return;

    entry->setContentState(historyItem);
}

void WebViewHost::printFrameDescription(WebFrame* webframe)
{
    string name8 = webframe->name().utf8();
    if (webframe == webView()->mainFrame()) {
        if (!name8.length()) {
            fputs("main frame", stdout);
            return;
        }
        printf("main frame \"%s\"", name8.c_str());
        return;
    }
    if (!name8.length()) {
        fputs("frame (anonymous)", stdout);
        return;
    }
    printf("frame \"%s\"", name8.c_str());
}

void WebViewHost::printResourceDescription(unsigned identifier)
{
    ResourceMap::iterator it = m_resourceIdentifierMap.find(identifier);
    printf("%s", it != m_resourceIdentifierMap.end() ? it->second.c_str() : "<unknown>");
}

void WebViewHost::setPendingExtraData(TestShellExtraData* extraData)
{
    m_pendingExtraData.set(extraData);
}

void WebViewHost::setPageTitle(const WebString&)
{
    // Nothing to do in layout test.
}

void WebViewHost::setAddressBarURL(const WebURL&)
{
    // Nothing to do in layout test.
}

// Painting functions ---------------------------------------------------------

void WebViewHost::updatePaintRect(const WebRect& rect)
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

void WebViewHost::paintRect(const WebRect& rect)
{
    ASSERT(!m_isPainting);
    ASSERT(canvas());
    m_isPainting = true;
#if PLATFORM(CG)
    webWidget()->paint(canvas()->getTopPlatformDevice().GetBitmapContext(), rect);
#else
    webWidget()->paint(canvas(), rect);
#endif
    m_isPainting = false;
}

void WebViewHost::paintInvalidatedRegion()
{
    webWidget()->layout();
    WebSize widgetSize = webWidget()->size();
    WebRect clientRect(0, 0, widgetSize.width, widgetSize.height);

    // Paint the canvas if necessary.  Allow painting to generate extra rects
    // for the first two calls. This is necessary because some WebCore rendering
    // objects update their layout only when painted.
    // Store the total area painted in total_paint. Then tell the gdk window
    // to update that area after we're done painting it.
    for (int i = 0; i < 3; ++i) {
        // m_paintRect = intersect(m_paintRect , clientRect)
        int left = max(m_paintRect.x, clientRect.x);
        int top = max(m_paintRect.y, clientRect.y);
        int right = min(m_paintRect.x + m_paintRect.width, clientRect.x + clientRect.width);
        int bottom = min(m_paintRect.y + m_paintRect.height, clientRect.y + clientRect.height);
        if (left >= right || top >= bottom)
            m_paintRect = WebRect();
        else
            m_paintRect = WebRect(left, top, right - left, bottom - top);

        if (m_paintRect.isEmpty())
            continue;
        WebRect rect(m_paintRect);
        m_paintRect = WebRect();
        paintRect(rect);
        if (i >= 1)
            LOG_ERROR("painting caused additional invalidations");
    }
    ASSERT(m_paintRect.isEmpty());
}

PlatformCanvas* WebViewHost::canvas()
{
    if (m_canvas)
        return m_canvas.get();
    WebSize widgetSize = webWidget()->size();
    resetScrollRect();
    m_canvas.set(new PlatformCanvas(widgetSize.width, widgetSize.height, true));
    return m_canvas.get();
}

void WebViewHost::resetScrollRect()
{
}

void WebViewHost::discardBackingStore()
{
    m_canvas.clear();
}

// Paints the entire canvas a semi-transparent black (grayish). This is used
// by the layout tests in fast/repaint. The alpha value matches upstream.
void WebViewHost::displayRepaintMask()
{
    canvas()->drawARGB(167, 0, 0, 0);
}
