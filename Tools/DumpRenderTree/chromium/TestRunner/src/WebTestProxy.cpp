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

#include "AccessibilityControllerChromium.h"
#include "EventSender.h"
#include "MockWebSpeechInputController.h"
#include "MockWebSpeechRecognizer.h"
#include "SpellCheckClient.h"
#include "TestCommon.h"
#include "TestInterfaces.h"
#include "TestPlugin.h"
#include "TestRunner.h"
#include "WebAccessibilityNotification.h"
#include "WebAccessibilityObject.h"
#include "WebCachedURLRequest.h"
#include "WebConsoleMessage.h"
#include "WebDataSource.h"
#include "WebDeviceOrientationClientMock.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebGeolocationClientMock.h"
#include "WebHistoryItem.h"
#include "WebNode.h"
#include "WebPluginParams.h"
#include "WebPrintParams.h"
#include "WebRange.h"
#include "WebScriptController.h"
#include "WebTestDelegate.h"
#include "WebTestInterfaces.h"
#include "WebTestRunner.h"
#include "WebUserMediaClientMock.h"
#include "WebView.h"
// FIXME: Including platform_canvas.h here is a layering violation.
#include "skia/ext/platform_canvas.h"
#include <cctype>
#include <public/WebCString.h>
#include <public/WebURLError.h>
#include <public/WebURLRequest.h>
#include <public/WebURLResponse.h>

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

void blockRequest(WebURLRequest& request)
{
    request.setURL(WebURL());
}

bool isLocalhost(const string& host)
{
    return host == "127.0.0.1" || host == "localhost";
}

bool hostIsUsedBySomeTestsToGenerateError(const string& host)
{
    return host == "255.255.255.255";
}

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
string urlSuitableForTestResult(const string& url)
{
    if (url.empty() || string::npos == url.find("file://"))
        return url;

    size_t pos = url.rfind('/');
    if (pos == string::npos) {
#ifdef WIN32
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

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* linkClickedString = "link clicked";
const char* formSubmittedString = "form submitted";
const char* backForwardString = "back/forward";
const char* reloadString = "reload";
const char* formResubmittedString = "form resubmitted";
const char* otherString = "other";
const char* illegalString = "illegal value";

// Get a debugging string from a WebNavigationType.
const char* webNavigationTypeToString(WebNavigationType type)
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

string dumpDocumentText(WebFrame* frame)
{
    // We use the document element's text instead of the body text here because
    // not all documents have a body, such as XML documents.
    WebElement documentElement = frame->document().documentElement();
    if (documentElement.isNull())
        return string();
    return documentElement.innerText().utf8();
}

string dumpFramesAsText(WebFrame* frame, bool recursive)
{
    string result;

    // Add header for all but the main frame. Skip empty frames.
    if (frame->parent() && !frame->document().documentElement().isNull()) {
        result.append("\n--------\nFrame: '");
        result.append(frame->uniqueName().utf8().data());
        result.append("'\n--------\n");
    }

    result.append(dumpDocumentText(frame));
    result.append("\n");

    if (recursive) {
        for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
            result.append(dumpFramesAsText(child, recursive));
    }

    return result;
}

string dumpFramesAsPrintedText(WebFrame* frame, bool recursive)
{
    string result;

    // Cannot do printed format for anything other than HTML
    if (!frame->document().isHTMLDocument())
        return string();

    // Add header for all but the main frame. Skip empty frames.
    if (frame->parent() && !frame->document().documentElement().isNull()) {
        result.append("\n--------\nFrame: '");
        result.append(frame->uniqueName().utf8().data());
        result.append("'\n--------\n");
    }

    result.append(frame->renderTreeAsText(WebFrame::RenderAsTextPrinting).utf8());
    result.append("\n");

    if (recursive) {
        for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
            result.append(dumpFramesAsPrintedText(child, recursive));
    }

    return result;
}

string dumpFrameScrollPosition(WebFrame* frame, bool recursive)
{
    string result;
    WebSize offset = frame->scrollOffset();
    if (offset.width > 0 || offset.height > 0) {
        if (frame->parent())
            result = string("frame '") + frame->uniqueName().utf8().data() + "' ";
        char data[100];
        snprintf(data, sizeof(data), "scrolled to %d,%d\n", offset.width, offset.height);
        result += data;
    }

    if (!recursive)
        return result;
    for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
        result += dumpFrameScrollPosition(child, recursive);
    return result;
}

struct ToLower {
    char16 operator()(char16 c) { return tolower(c); }
};

// Returns True if item1 < item2.
bool HistoryItemCompareLess(const WebHistoryItem& item1, const WebHistoryItem& item2)
{
    string16 target1 = item1.target();
    string16 target2 = item2.target();
    std::transform(target1.begin(), target1.end(), target1.begin(), ToLower());
    std::transform(target2.begin(), target2.end(), target2.begin(), ToLower());
    return target1 < target2;
}

string dumpHistoryItem(const WebHistoryItem& item, int indent, bool isCurrent)
{
    string result;

    if (isCurrent) {
        result.append("curr->");
        result.append(indent - 6, ' '); // 6 == "curr->".length()
    } else
        result.append(indent, ' ');

    string url = normalizeLayoutTestURL(item.urlString().utf8());
    result.append(url);
    if (!item.target().isEmpty()) {
        result.append(" (in frame \"");
        result.append(item.target().utf8());
        result.append("\")");
    }
    if (item.isTargetItem())
        result.append("  **nav target**");
    result.append("\n");

    const WebVector<WebHistoryItem>& children = item.children();
    if (!children.isEmpty()) {
        // Must sort to eliminate arbitrary result ordering which defeats
        // reproducible testing.
        // FIXME: WebVector should probably just be a std::vector!!
        std::vector<WebHistoryItem> sortedChildren;
        for (size_t i = 0; i < children.size(); ++i)
            sortedChildren.push_back(children[i]);
        std::sort(sortedChildren.begin(), sortedChildren.end(), HistoryItemCompareLess);
        for (size_t i = 0; i < sortedChildren.size(); ++i)
            result += dumpHistoryItem(sortedChildren[i], indent + 4, false);
    }

    return result;
}

void dumpBackForwardList(const WebVector<WebHistoryItem>& history, size_t currentEntryIndex, string& result)
{
    result.append("\n============== Back Forward List ==============\n");
    for (size_t index = 0; index < history.size(); ++index)
        result.append(dumpHistoryItem(history[index], 8, index == currentEntryIndex));
    result.append("===============================================\n");
}

string dumpAllBackForwardLists(TestInterfaces* interfaces, WebTestDelegate* delegate)
{
    string result;
    const vector<WebTestProxyBase*>& windowList = interfaces->windowList();
    for (unsigned i = 0; i < windowList.size(); ++i) {
        size_t currentEntryIndex = 0;
        WebVector<WebHistoryItem> history;
        delegate->captureHistoryForWindow(windowList.at(i), &history, &currentEntryIndex);
        dumpBackForwardList(history, currentEntryIndex, result);
    }
    return result;
}

}

WebTestProxyBase::WebTestProxyBase()
    : m_testInterfaces(0)
    , m_delegate(0)
    , m_spellcheck(new SpellCheckClient)
{
    reset();
}

WebTestProxyBase::~WebTestProxyBase()
{
    m_testInterfaces->windowClosed(this);
}

void WebTestProxyBase::setInterfaces(WebTestInterfaces* interfaces)
{
    m_testInterfaces = interfaces->testInterfaces();
    m_testInterfaces->windowOpened(this);
}

void WebTestProxyBase::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
    m_spellcheck->setDelegate(delegate);
#if ENABLE_INPUT_SPEECH
    if (m_speechInputController.get())
        m_speechInputController->setDelegate(delegate);
#endif
    if (m_speechRecognizer.get())
        m_speechRecognizer->setDelegate(delegate);
}

void WebTestProxyBase::reset()
{
    m_paintRect = WebRect();
    m_canvas.reset();
    m_isPainting = false;
    m_resourceIdentifierMap.clear();
    m_logConsoleOutput = true;
    if (m_geolocationClient.get())
        m_geolocationClient->resetMock();
#if ENABLE_INPUT_SPEECH
    if (m_speechInputController.get())
        m_speechInputController->clearResults();
#endif
}

WebSpellCheckClient* WebTestProxyBase::spellCheckClient() const
{
    return m_spellcheck.get();
}

string WebTestProxyBase::captureTree(bool debugRenderTree)
{
    WebScriptController::flushConsoleMessages();

    bool shouldDumpAsText = m_testInterfaces->testRunner()->shouldDumpAsText();
    bool shouldDumpAsPrinted = m_testInterfaces->testRunner()->isPrinting();
    WebFrame* frame = m_testInterfaces->webView()->mainFrame();
    string dataUtf8;
    if (shouldDumpAsText) {
        bool recursive = m_testInterfaces->testRunner()->shouldDumpChildFramesAsText();
        dataUtf8 = shouldDumpAsPrinted ? dumpFramesAsPrintedText(frame, recursive) : dumpFramesAsText(frame, recursive);
    } else {
        bool recursive = m_testInterfaces->testRunner()->shouldDumpChildFrameScrollPositions();
        WebFrame::RenderAsTextControls renderTextBehavior = WebFrame::RenderAsTextNormal;
        if (shouldDumpAsPrinted)
            renderTextBehavior |= WebFrame::RenderAsTextPrinting;
        if (debugRenderTree)
            renderTextBehavior |= WebFrame::RenderAsTextDebug;
        dataUtf8 = frame->renderTreeAsText(renderTextBehavior).utf8();
        dataUtf8 += dumpFrameScrollPosition(frame, recursive);
    }

    if (m_testInterfaces->testRunner()->shouldDumpBackForwardList())
        dataUtf8 += dumpAllBackForwardLists(m_testInterfaces, m_delegate);

    return dataUtf8;
}

SkCanvas* WebTestProxyBase::capturePixels()
{
    m_testInterfaces->webView()->layout();
    if (m_testInterfaces->testRunner()->testRepaint()) {
        WebSize viewSize = m_testInterfaces->webView()->size();
        int width = viewSize.width;
        int height = viewSize.height;
        if (m_testInterfaces->testRunner()->sweepHorizontally()) {
            for (WebRect column(0, 0, 1, height); column.x < width; column.x++)
                paintRect(column);
        } else {
            for (WebRect line(0, 0, width, 1); line.y < height; line.y++)
                paintRect(line);
        }
    } else if (m_testInterfaces->testRunner()->isPrinting())
        paintPagesWithBoundaries();
    else
        paintInvalidatedRegion();

    // See if we need to draw the selection bounds rect. Selection bounds
    // rect is the rect enclosing the (possibly transformed) selection.
    // The rect should be drawn after everything is laid out and painted.
    if (m_testInterfaces->testRunner()->shouldDumpSelectionRect()) {
        // If there is a selection rect - draw a red 1px border enclosing rect
        WebRect wr = m_testInterfaces->webView()->mainFrame()->selectionBoundsRect();
        if (!wr.isEmpty()) {
            // Render a red rectangle bounding selection rect
            SkPaint paint;
            paint.setColor(0xFFFF0000); // Fully opaque red
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setFlags(SkPaint::kAntiAlias_Flag);
            paint.setStrokeWidth(1.0f);
            SkIRect rect; // Bounding rect
            rect.set(wr.x, wr.y, wr.x + wr.width, wr.y + wr.height);
            canvas()->drawIRect(rect, paint);
        }
    }

    return canvas();
}

void WebTestProxyBase::setLogConsoleOutput(bool enabled)
{
    m_logConsoleOutput = enabled;
}

void WebTestProxyBase::paintRect(const WebRect& rect)
{
    WEBKIT_ASSERT(!m_isPainting);
    WEBKIT_ASSERT(canvas());
    m_isPainting = true;
    float deviceScaleFactor = m_testInterfaces->webView()->deviceScaleFactor();
    int scaledX = static_cast<int>(static_cast<float>(rect.x) * deviceScaleFactor);
    int scaledY = static_cast<int>(static_cast<float>(rect.y) * deviceScaleFactor);
    int scaledWidth = static_cast<int>(ceil(static_cast<float>(rect.width) * deviceScaleFactor));
    int scaledHeight = static_cast<int>(ceil(static_cast<float>(rect.height) * deviceScaleFactor));
    WebRect deviceRect(scaledX, scaledY, scaledWidth, scaledHeight);
    m_testInterfaces->webView()->paint(canvas(), deviceRect);
    m_isPainting = false;
}

void WebTestProxyBase::paintInvalidatedRegion()
{
    m_testInterfaces->webView()->animate(0.0);
    m_testInterfaces->webView()->layout();
    WebSize widgetSize = m_testInterfaces->webView()->size();
    WebRect clientRect(0, 0, widgetSize.width, widgetSize.height);

    // Paint the canvas if necessary. Allow painting to generate extra rects
    // for the first two calls. This is necessary because some WebCore rendering
    // objects update their layout only when painted.
    // Store the total area painted in total_paint. Then tell the gdk window
    // to update that area after we're done painting it.
    for (int i = 0; i < 3; ++i) {
        // rect = intersect(m_paintRect , clientRect)
        WebRect damageRect = m_paintRect;
        int left = max(damageRect.x, clientRect.x);
        int top = max(damageRect.y, clientRect.y);
        int right = min(damageRect.x + damageRect.width, clientRect.x + clientRect.width);
        int bottom = min(damageRect.y + damageRect.height, clientRect.y + clientRect.height);
        WebRect rect;
        if (left < right && top < bottom)
            rect = WebRect(left, top, right - left, bottom - top);

        m_paintRect = WebRect();
        if (rect.isEmpty())
            continue;
        paintRect(rect);
    }
    WEBKIT_ASSERT(m_paintRect.isEmpty());
}

void WebTestProxyBase::paintPagesWithBoundaries()
{
    WEBKIT_ASSERT(!m_isPainting);
    WEBKIT_ASSERT(canvas());
    m_isPainting = true;

    WebSize pageSizeInPixels = m_testInterfaces->webView()->size();
    WebFrame* webFrame = m_testInterfaces->webView()->mainFrame();

    int pageCount = webFrame->printBegin(pageSizeInPixels);
    int totalHeight = pageCount * (pageSizeInPixels.height + 1) - 1;

    SkCanvas* testCanvas = skia::TryCreateBitmapCanvas(pageSizeInPixels.width, totalHeight, true);
    if (testCanvas) {
        discardBackingStore();
        m_canvas.reset(testCanvas);
    } else {
        webFrame->printEnd();
        return;
    }

    webFrame->printPagesWithBoundaries(canvas(), pageSizeInPixels);
    webFrame->printEnd();

    m_isPainting = false;
}

SkCanvas* WebTestProxyBase::canvas()
{
    if (m_canvas.get())
        return m_canvas.get();
    WebSize widgetSize = m_testInterfaces->webView()->size();
    float deviceScaleFactor = m_testInterfaces->webView()->deviceScaleFactor();
    int scaledWidth = static_cast<int>(ceil(static_cast<float>(widgetSize.width) * deviceScaleFactor));
    int scaledHeight = static_cast<int>(ceil(static_cast<float>(widgetSize.height) * deviceScaleFactor));
    m_canvas.reset(skia::CreateBitmapCanvas(scaledWidth, scaledHeight, true));
    return m_canvas.get();
}

// Paints the entire canvas a semi-transparent black (grayish). This is used
// by the layout tests in fast/repaint. The alpha value matches upstream.
void WebTestProxyBase::displayRepaintMask()
{
    canvas()->drawARGB(167, 0, 0, 0);
}

void WebTestProxyBase::display()
{
    const WebKit::WebSize& size = m_testInterfaces->webView()->size();
    WebRect rect(0, 0, size.width, size.height);
    m_paintRect = rect;
    paintInvalidatedRegion();
    displayRepaintMask();
}

void WebTestProxyBase::displayInvalidatedRegion()
{
    paintInvalidatedRegion();
    displayRepaintMask();
}

void WebTestProxyBase::discardBackingStore()
{
    m_canvas.reset();
}

WebGeolocationClientMock* WebTestProxyBase::geolocationClientMock()
{
    if (!m_geolocationClient.get())
        m_geolocationClient.reset(WebGeolocationClientMock::create());
    return m_geolocationClient.get();
}

WebDeviceOrientationClientMock* WebTestProxyBase::deviceOrientationClientMock()
{
    if (!m_deviceOrientationClient.get())
        m_deviceOrientationClient.reset(WebDeviceOrientationClientMock::create());
    return m_deviceOrientationClient.get();
}

#if ENABLE_INPUT_SPEECH
MockWebSpeechInputController* WebTestProxyBase::speechInputControllerMock()
{
    WEBKIT_ASSERT(m_speechInputController.get());
    return m_speechInputController.get();
}
#endif

MockWebSpeechRecognizer* WebTestProxyBase::speechRecognizerMock()
{
    if (!m_speechRecognizer.get()) {
        m_speechRecognizer.reset(new MockWebSpeechRecognizer());
        m_speechRecognizer->setDelegate(m_delegate);
    }
    return m_speechRecognizer.get();
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
    discardBackingStore();
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

bool WebTestProxyBase::shouldBeginEditing(const WebRange& range)
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldBeginEditingInDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

bool WebTestProxyBase::shouldEndEditing(const WebRange& range)
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldEndEditingInDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

bool WebTestProxyBase::shouldInsertNode(const WebNode& node, const WebRange& range, WebEditingAction action)
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
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
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage(string("EDITING DELEGATE: shouldInsertText:") + text.utf8().data() + " replacingDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage(string(" givenAction:") + editingActionDescription(action) + "\n");
    }
    return true;
}

bool WebTestProxyBase::shouldChangeSelectedRange(
    const WebRange& fromRange, const WebRange& toRange, WebTextAffinity affinity, bool stillSelecting)
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
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
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage("EDITING DELEGATE: shouldDeleteDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

bool WebTestProxyBase::shouldApplyStyle(const WebString& style, const WebRange& range)
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks()) {
        m_delegate->printMessage(string("EDITING DELEGATE: shouldApplyStyle:") + style.utf8().data() + " toElementsInDOMRange:");
        printRangeDescription(m_delegate, range);
        m_delegate->printMessage("\n");
    }
    return true;
}

void WebTestProxyBase::didBeginEditing()
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidBeginEditing:WebViewDidBeginEditingNotification\n");
}

void WebTestProxyBase::didChangeSelection(bool isEmptySelection)
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

void WebTestProxyBase::didChangeContents()
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

void WebTestProxyBase::didEndEditing()
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidEndEditing:WebViewDidEndEditingNotification\n");
}

bool WebTestProxyBase::createView(WebFrame*, const WebURLRequest& request, const WebWindowFeatures&, const WebString&, WebNavigationPolicy)
{
    if (!m_testInterfaces->testRunner()->canOpenWindows())
        return false;
    if (m_testInterfaces->testRunner()->shouldDumpCreateView())
        m_delegate->printMessage(string("createView(") + URLDescription(request.url()) + ")\n");
    return true;
}

WebPlugin* WebTestProxyBase::createPlugin(WebFrame* frame, const WebPluginParams& params)
{
    if (params.mimeType == TestPlugin::mimeType())
        return TestPlugin::create(frame, params, m_delegate);
    return 0;
}

void WebTestProxyBase::setStatusText(const WebString& text)
{
    if (!m_testInterfaces->testRunner()->shouldDumpStatusCallbacks())
        return;
    m_delegate->printMessage(string("UI DELEGATE STATUS CALLBACK: setStatusText:") + text.utf8().data() + "\n");
}

void WebTestProxyBase::didStopLoading()
{
    if (m_testInterfaces->testRunner()->shouldDumpProgressFinishedCallback())
        m_delegate->printMessage("postProgressFinishedNotification\n");
}

bool WebTestProxyBase::isSmartInsertDeleteEnabled()
{
    return m_testInterfaces->testRunner()->isSmartInsertDeleteEnabled();
}

bool WebTestProxyBase::isSelectTrailingWhitespaceEnabled()
{
    return m_testInterfaces->testRunner()->isSelectTrailingWhitespaceEnabled();
}

void WebTestProxyBase::showContextMenu(WebFrame*, const WebContextMenuData& contextMenuData)
{
    m_testInterfaces->eventSender()->setContextMenuData(contextMenuData);
}

WebUserMediaClient* WebTestProxyBase::userMediaClient()
{
#if ENABLE_WEBRTC
    if (!m_userMediaClient.get())
        m_userMediaClient.reset(new WebUserMediaClientMock(m_delegate));
    return m_userMediaClient.get();
#else
    return 0;
#endif // ENABLE_WEBRTC
}

// Simulate a print by going into print mode and then exit straight away.
void WebTestProxyBase::printPage(WebFrame* frame)
{
    WebSize pageSizeInPixels = m_testInterfaces->webView()->size();
    WebPrintParams printParams(pageSizeInPixels);
    frame->printBegin(printParams);
    frame->printEnd();
}

WebNotificationPresenter* WebTestProxyBase::notificationPresenter()
{
#if ENABLE_NOTIFICATIONS
    return m_testInterfaces->testRunner()->notificationPresenter();
#else
    return 0;
#endif
}

WebGeolocationClient* WebTestProxyBase::geolocationClient()
{
    return geolocationClientMock();
}

WebSpeechInputController* WebTestProxyBase::speechInputController(WebSpeechInputListener* listener)
{
#if ENABLE_INPUT_SPEECH
    if (!m_speechInputController.get()) {
        m_speechInputController.reset(new MockWebSpeechInputController(listener));
        m_speechInputController->setDelegate(m_delegate);
    }
    return m_speechInputController.get();
#else
    WEBKIT_ASSERT(listener);
    return 0;
#endif
}

WebSpeechRecognizer* WebTestProxyBase::speechRecognizer()
{
    return speechRecognizerMock();
}

WebDeviceOrientationClient* WebTestProxyBase::deviceOrientationClient()
{
    return deviceOrientationClientMock();
}

bool WebTestProxyBase::requestPointerLock()
{
    return m_testInterfaces->testRunner()->requestPointerLock();
}

void WebTestProxyBase::requestPointerUnlock()
{
    m_testInterfaces->testRunner()->requestPointerUnlock();
}

bool WebTestProxyBase::isPointerLocked()
{
    return m_testInterfaces->testRunner()->isPointerLocked();
}

void WebTestProxyBase::willPerformClientRedirect(WebFrame* frame, const WebURL&, const WebURL& to, double, double)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(string(" - willPerformClientRedirectToURL: ") + to.spec().data() + " \n");
    }

    if (m_testInterfaces->testRunner()->shouldDumpUserGestureInFrameLoadCallbacks())
        printFrameUserGestureStatus(m_delegate, frame, " - in willPerformClientRedirect\n");
}

void WebTestProxyBase::didCancelClientRedirect(WebFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didCancelClientRedirectForFrame\n");
    }
}

void WebTestProxyBase::didStartProvisionalLoad(WebFrame* frame)
{
    if (!m_testInterfaces->testRunner()->topLoadingFrame())
        m_testInterfaces->testRunner()->setTopLoadingFrame(frame, false);

    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didStartProvisionalLoadForFrame\n");
    }

    if (m_testInterfaces->testRunner()->shouldDumpUserGestureInFrameLoadCallbacks())
        printFrameUserGestureStatus(m_delegate, frame, " - in didStartProvisionalLoadForFrame\n");

    if (m_testInterfaces->testRunner()->stopProvisionalFrameLoads()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - stopping load in didStartProvisionalLoadForFrame callback\n");
        frame->stopLoading();
    }
}

void WebTestProxyBase::didReceiveServerRedirectForProvisionalLoad(WebFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didReceiveServerRedirectForProvisionalLoadForFrame\n");
    }
}

void WebTestProxyBase::didFailProvisionalLoad(WebFrame* frame, const WebURLError&)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFailProvisionalLoadWithError\n");
    }
    locationChangeDone(frame);
}

void WebTestProxyBase::didCommitProvisionalLoad(WebFrame* frame, bool)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didCommitLoadForFrame\n");
    }
}

void WebTestProxyBase::didReceiveTitle(WebFrame* frame, const WebString& title, WebTextDirection direction)
{
    WebCString title8 = title.utf8();

    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(string(" - didReceiveTitle: ") + title8.data() + "\n");
    }

    if (m_testInterfaces->testRunner()->shouldDumpTitleChanges())
        m_delegate->printMessage(string("TITLE CHANGED: '") + title8.data() + "'\n");

    m_testInterfaces->testRunner()->setTitleTextDirection(direction);
}

void WebTestProxyBase::didFinishDocumentLoad(WebFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
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
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didHandleOnloadEventsForFrame\n");
    }
}

void WebTestProxyBase::didFailLoad(WebFrame* frame, const WebURLError&)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFailLoadWithError\n");
    }
    locationChangeDone(frame);
}

void WebTestProxyBase::didFinishLoad(WebFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFinishLoadForFrame\n");
    }
    locationChangeDone(frame);
}

void WebTestProxyBase::didChangeLocationWithinPage(WebFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didChangeLocationWithinPageForFrame\n");
    }
}

void WebTestProxyBase::didDisplayInsecureContent(WebFrame*)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks())
        m_delegate->printMessage("didDisplayInsecureContent\n");
}

void WebTestProxyBase::didRunInsecureContent(WebFrame*, const WebSecurityOrigin&, const WebURL&)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks())
        m_delegate->printMessage("didRunInsecureContent\n");
}

void WebTestProxyBase::didDetectXSS(WebFrame*, const WebURL&, bool)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks())
        m_delegate->printMessage("didDetectXSS\n");
}

void WebTestProxyBase::assignIdentifierToRequest(WebFrame*, unsigned identifier, const WebKit::WebURLRequest& request)
{
    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        WEBKIT_ASSERT(m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end());
        m_resourceIdentifierMap[identifier] = descriptionSuitableForTestResult(request.url().spec());
    }
}

void WebTestProxyBase::willRequestResource(WebFrame* frame, const WebKit::WebCachedURLRequest& request)
{
    if (m_testInterfaces->testRunner()->shouldDumpResourceRequestCallbacks()) {
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

bool WebTestProxyBase::canHandleRequest(WebFrame*, const WebURLRequest& request)
{
    GURL url = request.url();
    // Just reject the scheme used in
    // LayoutTests/http/tests/misc/redirect-to-external-url.html
    return !url.SchemeIs("spaceballs");
}

WebURLError WebTestProxyBase::cannotHandleRequestError(WebFrame*, const WebURLRequest& request)
{
    WebURLError error;
    // A WebKit layout test expects the following values.
    // unableToImplementPolicyWithError() below prints them.
    error.domain = WebString::fromUTF8("WebKitErrorDomain");
    error.reason = 101;
    error.unreachableURL = request.url();
    return error;
}

void WebTestProxyBase::didCreateDataSource(WebFrame*, WebDataSource* ds)
{
    if (!m_testInterfaces->testRunner()->deferMainResourceDataLoad())
        ds->setDeferMainResourceDataLoad(false);
}

void WebTestProxyBase::willSendRequest(WebFrame*, unsigned identifier, WebKit::WebURLRequest& request, const WebKit::WebURLResponse& redirectResponse)
{
    // Need to use GURL for host() and SchemeIs()
    GURL url = request.url();
    string requestURL = url.possibly_invalid_spec();

    GURL mainDocumentURL = request.firstPartyForCookies();

    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
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

    if (!redirectResponse.isNull() && m_testInterfaces->testRunner()->shouldBlockRedirects()) {
        m_delegate->printMessage("Returning null for this redirect\n");
        blockRequest(request);
        return;
    }

    if (m_testInterfaces->testRunner()->willSendRequestShouldReturnNull()) {
        blockRequest(request);
        return;
    }

    if (m_testInterfaces->testRunner()->httpHeadersToClear()) {
        const set<string> *clearHeaders = m_testInterfaces->testRunner()->httpHeadersToClear();
        for (set<string>::const_iterator header = clearHeaders->begin(); header != clearHeaders->end(); ++header)
            request.clearHTTPHeaderField(WebString::fromUTF8(*header));
    }

    string host = url.host();
    if (!host.empty() && (url.SchemeIs("http") || url.SchemeIs("https"))) {
        if (!isLocalhost(host) && !hostIsUsedBySomeTestsToGenerateError(host)
            && ((!mainDocumentURL.SchemeIs("http") && !mainDocumentURL.SchemeIs("https")) || isLocalhost(mainDocumentURL.host()))
            && !m_delegate->allowExternalPages()) {
            m_delegate->printMessage(string("Blocked access to external URL ") + requestURL + "\n");
            blockRequest(request);
            return;
        }
    }

    // Set the new substituted URL.
    request.setURL(m_delegate->rewriteLayoutTestsURL(request.url().spec()));
}

void WebTestProxyBase::didReceiveResponse(WebFrame*, unsigned identifier, const WebKit::WebURLResponse& response)
{
    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - didReceiveResponse ");
        printResponseDescription(m_delegate, response);
        m_delegate->printMessage("\n");
    }
    if (m_testInterfaces->testRunner()->shouldDumpResourceResponseMIMETypes()) {
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
    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
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
    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
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

void WebTestProxyBase::unableToImplementPolicyWithError(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
{
    char errorBuffer[40];
    snprintf(errorBuffer, sizeof(errorBuffer), "%d", error.reason);
    m_delegate->printMessage(string("Policy delegate: unable to implement policy with error domain '") + error.domain.utf8().data() +
        "', error code " +  errorBuffer +
        ", in frame '" + frame->uniqueName().utf8().data() + "'\n");
}

void WebTestProxyBase::didAddMessageToConsole(const WebConsoleMessage& message, const WebString& sourceName, unsigned sourceLine)
{
    // This matches win DumpRenderTree's UIDelegate.cpp.
    if (!m_logConsoleOutput)
        return;
    m_delegate->printMessage(string("CONSOLE MESSAGE: "));
    if (sourceLine) {
        char buffer[40];
        snprintf(buffer, sizeof(buffer), "line %d: ", sourceLine);
        m_delegate->printMessage(buffer);
    }
    if (!message.text.isEmpty()) {
        string newMessage;
        newMessage = message.text.utf8();
        size_t fileProtocol = newMessage.find("file://");
        if (fileProtocol != string::npos) {
            newMessage = newMessage.substr(0, fileProtocol)
                + urlSuitableForTestResult(newMessage.substr(fileProtocol));
        }
        m_delegate->printMessage(newMessage);
    }
    m_delegate->printMessage(string("\n"));
}

void WebTestProxyBase::runModalAlertDialog(WebFrame*, const WebString& message)
{
    m_delegate->printMessage(string("ALERT: ") + message.utf8().data() + "\n");
}

bool WebTestProxyBase::runModalConfirmDialog(WebFrame*, const WebString& message)
{
    m_delegate->printMessage(string("CONFIRM: ") + message.utf8().data() + "\n");
    return true;
}

bool WebTestProxyBase::runModalPromptDialog(WebFrame* frame, const WebString& message, const WebString& defaultValue, WebString*)
{
    m_delegate->printMessage(string("PROMPT: ") + message.utf8().data() + ", default text: " + defaultValue.utf8().data() + "\n");
    return true;
}

bool WebTestProxyBase::runModalBeforeUnloadDialog(WebFrame*, const WebString& message)
{
    m_delegate->printMessage(string("CONFIRM NAVIGATION: ") + message.utf8().data() + "\n");
    return !m_testInterfaces->testRunner()->shouldStayOnPageAfterHandlingBeforeUnload();
}

void WebTestProxyBase::locationChangeDone(WebFrame* frame)
{
    if (frame != m_testInterfaces->testRunner()->topLoadingFrame())
        return;
    m_testInterfaces->testRunner()->setTopLoadingFrame(frame, true);
}

WebNavigationPolicy WebTestProxyBase::decidePolicyForNavigation(WebFrame*, const WebURLRequest& request, WebNavigationType type, const WebNode& originatingNode, WebNavigationPolicy defaultPolicy, bool isRedirect)
{
    WebNavigationPolicy result;
    if (!m_testInterfaces->testRunner()->policyDelegateEnabled())
        return defaultPolicy;

    m_delegate->printMessage(string("Policy delegate: attempt to load ") + URLDescription(request.url()) + " with navigation type '" + webNavigationTypeToString(type) + "'");
    if (!originatingNode.isNull()) {
        m_delegate->printMessage(" originating from ");
        printNodeDescription(m_delegate, originatingNode, 0);
    }
    m_delegate->printMessage("\n");
    if (m_testInterfaces->testRunner()->policyDelegateIsPermissive())
        result = WebKit::WebNavigationPolicyCurrentTab;
    else
        result = WebKit::WebNavigationPolicyIgnore;

    if (m_testInterfaces->testRunner()->policyDelegateShouldNotifyDone())
        m_testInterfaces->testRunner()->policyDelegateDone();
    return result;
}

bool WebTestProxyBase::willCheckAndDispatchMessageEvent(WebFrame*, WebFrame*, WebSecurityOrigin, WebDOMMessageEvent)
{
    if (m_testInterfaces->testRunner()->shouldInterceptPostMessage()) {
        m_delegate->printMessage("intercepted postMessage\n");
        return true;
    }

    return false;
}

}
