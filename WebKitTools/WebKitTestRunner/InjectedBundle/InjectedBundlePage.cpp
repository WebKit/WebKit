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

#include "InjectedBundlePage.h"

#include "InjectedBundle.h"
#include "StringFunctions.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit2/WKArray.h>
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundleFramePrivate.h>
#include <WebKit2/WKBundlePagePrivate.h>

using namespace std;

namespace WTR {

static JSValueRef propertyValue(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    if (!object)
        return 0;
    JSRetainPtr<JSStringRef> propertyNameString(Adopt, JSStringCreateWithUTF8CString(propertyName));
    return JSObjectGetProperty(context, object, propertyNameString.get(), 0);
}

static double propertyValueDouble(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    JSValueRef value = propertyValue(context, object, propertyName);
    if (!value)
        return 0;
    return JSValueToNumber(context, value, 0);    
}

static int propertyValueInt(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    return static_cast<int>(propertyValueDouble(context, object, propertyName));    
}

static double numericWindowPropertyValue(WKBundleFrameRef frame, const char* propertyName)
{
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    return propertyValueDouble(context, JSContextGetGlobalObject(context), propertyName);
}

static string dumpPath(JSGlobalContextRef context, JSObjectRef nodeValue)
{
    JSValueRef nodeNameValue = propertyValue(context, nodeValue, "nodeName");
    JSRetainPtr<JSStringRef> jsStringNodeName(Adopt, JSValueToStringCopy(context, nodeNameValue, 0));
    WKRetainPtr<WKStringRef> nodeName = toWK(jsStringNodeName);

    JSValueRef parentNode = propertyValue(context, nodeValue, "parentNode");

    ostringstream out;
    out << nodeName;

    if (parentNode && JSValueIsObject(context, parentNode))
        out << " > " << dumpPath(context, (JSObjectRef)parentNode);

    return out.str();
}

static string dumpPath(WKBundlePageRef page, WKBundleScriptWorldRef world, WKBundleNodeHandleRef node)
{
    if (!node)
        return "(null)";

    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    JSValueRef nodeValue = WKBundleFrameGetJavaScriptWrapperForNodeForWorld(frame, node, world);
    ASSERT(JSValueIsObject(context, nodeValue));
    JSObjectRef nodeObject = (JSObjectRef)nodeValue;

    return dumpPath(context, nodeObject);
}

static string toStr(WKBundlePageRef page, WKBundleScriptWorldRef world, WKBundleRangeHandleRef rangeRef)
{
    if (!rangeRef)
        return "(null)";

    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    JSValueRef rangeValue = WKBundleFrameGetJavaScriptWrapperForRangeForWorld(frame, rangeRef, world);
    ASSERT(JSValueIsObject(context, rangeValue));
    JSObjectRef rangeObject = (JSObjectRef)rangeValue;

    JSValueRef startNodeValue = propertyValue(context, rangeObject, "startContainer");
    ASSERT(JSValueIsObject(context, startNodeValue));
    JSObjectRef startNodeObject = (JSObjectRef)startNodeValue;

    JSValueRef endNodeValue = propertyValue(context, rangeObject, "endContainer");
    ASSERT(JSValueIsObject(context, endNodeValue));
    JSObjectRef endNodeObject = (JSObjectRef)endNodeValue;

    int startOffset = propertyValueInt(context, rangeObject, "startOffset");
    int endOffset = propertyValueInt(context, rangeObject, "endOffset");

    ostringstream out;
    out << "range from " << startOffset << " of " << dumpPath(context, startNodeObject) << " to " << endOffset << " of " << dumpPath(context, endNodeObject);
    return out.str();
}

static ostream& operator<<(ostream& out, WKBundleCSSStyleDeclarationRef style)
{
    // DumpRenderTree calls -[DOMCSSStyleDeclaration description], which just dumps class name and object address.
    // No existing tests actually hit this code path at the time of this writing, because WebCore doesn't call
    // the editing client if the styling operation source is CommandFromDOM or CommandFromDOMWithUserInterface.
    out << "<DOMCSSStyleDeclaration ADDRESS>";
    return out;
}

static ostream& operator<<(ostream& out, WKBundleFrameRef frame)
{
    WKRetainPtr<WKStringRef> name(AdoptWK, WKBundleFrameCopyName(frame));
    if (WKBundleFrameIsMainFrame(frame)) {
        if (!WKStringIsEmpty(name.get()))
            out << "main frame \"" << name << "\"";
        else
            out << "main frame";
    } else {
        if (!WKStringIsEmpty(name.get()))
            out << "frame \"" << name << "\"";
        else
            out << "frame (anonymous)";
    }

    return out;
}

InjectedBundlePage::InjectedBundlePage(WKBundlePageRef page)
    : m_page(page)
    , m_world(AdoptWK, WKBundleScriptWorldCreateWorld())
    , m_isLoading(false)
{
    WKBundlePageLoaderClient loaderClient = {
        0,
        this,
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        didFailProvisionalLoadWithErrorForFrame,
        didCommitLoadForFrame,
        didFinishDocumentLoadForFrame,
        didFinishLoadForFrame,
        didFailLoadWithErrorForFrame,
        didReceiveTitleForFrame,
        0,
        0,
        0,
        didClearWindowForFrame,
        didCancelClientRedirectForFrame,
        willPerformClientRedirectForFrame,
        didChangeLocationWithinPageForFrame,
        didHandleOnloadEventsForFrame,
        didDisplayInsecureContentForFrame,
        didRunInsecureContentForFrame
    };
    WKBundlePageSetLoaderClient(m_page, &loaderClient);

    WKBundlePageUIClient uiClient = {
        0,
        this,
        willAddMessageToConsole,
        willSetStatusbarText,
        willRunJavaScriptAlert,
        willRunJavaScriptConfirm,
        willRunJavaScriptPrompt,
        0 /*mouseDidMoveOverElement*/
    };
    WKBundlePageSetUIClient(m_page, &uiClient);

    WKBundlePageEditorClient editorClient = {
        0,
        this,
        shouldBeginEditing,
        shouldEndEditing,
        shouldInsertNode,
        shouldInsertText,
        shouldDeleteRange,
        shouldChangeSelectedRange,
        shouldApplyStyle,
        didBeginEditing,
        didEndEditing,
        didChange,
        didChangeSelection
    };
    WKBundlePageSetEditorClient(m_page, &editorClient);
}

InjectedBundlePage::~InjectedBundlePage()
{
}

void InjectedBundlePage::stopLoading()
{
    WKBundlePageStopLoading(m_page);
    m_isLoading = false;
}

void InjectedBundlePage::reset()
{
    WKBundlePageClearMainFrameName(m_page);

    WKBundlePageSetPageZoomFactor(m_page, 1);
    WKBundlePageSetTextZoomFactor(m_page, 1);
}

// Loader Client Callbacks

void InjectedBundlePage::didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didStartProvisionalLoadForFrame(frame);
}

void InjectedBundlePage::didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveServerRedirectForProvisionalLoadForFrame(frame);
}

void InjectedBundlePage::didFailProvisionalLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFailProvisionalLoadWithErrorForFrame(frame);
}

void InjectedBundlePage::didCommitLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didCommitLoadForFrame(frame);
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFinishLoadForFrame(frame);
}

void InjectedBundlePage::didFinishDocumentLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFinishDocumentLoadForFrame(frame);
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFailLoadWithErrorForFrame(frame);
}

void InjectedBundlePage::didReceiveTitleForFrame(WKBundlePageRef page, WKStringRef title, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveTitleForFrame(title, frame);
}

void InjectedBundlePage::didClearWindowForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleScriptWorldRef world, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didClearWindowForFrame(frame, world);
}

void InjectedBundlePage::didCancelClientRedirectForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didCancelClientRedirectForFrame(frame);
}

void InjectedBundlePage::willPerformClientRedirectForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKURLRef url, double delay, double date, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willPerformClientRedirectForFrame(frame, url, delay, date);
}

void InjectedBundlePage::didChangeLocationWithinPageForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didChangeLocationWithinPageForFrame(frame);
}

void InjectedBundlePage::didHandleOnloadEventsForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didHandleOnloadEventsForFrame(frame);
}

void InjectedBundlePage::didDisplayInsecureContentForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didDisplayInsecureContentForFrame(frame);
}

void InjectedBundlePage::didRunInsecureContentForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didRunInsecureContentForFrame(frame);
}


void InjectedBundlePage::didStartProvisionalLoadForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (frame == WKBundlePageGetMainFrame(m_page))
        m_isLoading = true;
}

void InjectedBundlePage::didReceiveServerRedirectForProvisionalLoadForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didFailProvisionalLoadWithErrorForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didCommitLoadForFrame(WKBundleFrameRef frame)
{
}

enum FrameNamePolicy { ShouldNotIncludeFrameName, ShouldIncludeFrameName };

static void dumpFrameScrollPosition(WKBundleFrameRef frame, FrameNamePolicy shouldIncludeFrameName = ShouldNotIncludeFrameName)
{
    double x = numericWindowPropertyValue(frame, "pageXOffset");
    double y = numericWindowPropertyValue(frame, "pageYOffset");
    if (fabs(x) > 0.00000001 || fabs(y) > 0.00000001) {
        if (shouldIncludeFrameName) {
            WKRetainPtr<WKStringRef> name(AdoptWK, WKBundleFrameCopyName(frame));
            InjectedBundle::shared().os() << "frame '" << name << "' ";
        }
        InjectedBundle::shared().os() << "scrolled to " << x << "," << y << "\n";
    }
}

static void dumpDescendantFrameScrollPositions(WKBundleFrameRef frame)
{
    WKRetainPtr<WKArrayRef> childFrames(AdoptWK, WKBundleFrameCopyChildFrames(frame));
    size_t size = WKArrayGetSize(childFrames.get());
    for (size_t i = 0; i < size; ++i) {
        WKBundleFrameRef subframe = static_cast<WKBundleFrameRef>(WKArrayGetItemAtIndex(childFrames.get(), i));
        dumpFrameScrollPosition(subframe, ShouldIncludeFrameName);
        dumpDescendantFrameScrollPositions(subframe);
    }
}

void InjectedBundlePage::dumpAllFrameScrollPositions()
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    dumpFrameScrollPosition(frame);
    dumpDescendantFrameScrollPositions(frame);
}

static void dumpFrameText(WKBundleFrameRef frame)
{
    WKRetainPtr<WKStringRef> text(AdoptWK, WKBundleFrameCopyInnerText(frame));
    InjectedBundle::shared().os() << text << "\n";
}

static void dumpDescendantFramesText(WKBundleFrameRef frame)
{
    WKRetainPtr<WKArrayRef> childFrames(AdoptWK, WKBundleFrameCopyChildFrames(frame));
    size_t size = WKArrayGetSize(childFrames.get());
    for (size_t i = 0; i < size; ++i) {
        WKBundleFrameRef subframe = static_cast<WKBundleFrameRef>(WKArrayGetItemAtIndex(childFrames.get(), i));
        WKRetainPtr<WKStringRef> subframeName(AdoptWK, WKBundleFrameCopyName(subframe));
        InjectedBundle::shared().os() << "\n--------\nFrame: '" << subframeName << "'\n--------\n";
        dumpFrameText(subframe);
        dumpDescendantFramesText(subframe);
    }
}

void InjectedBundlePage::dumpAllFramesText()
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    dumpFrameText(frame);
    dumpDescendantFramesText(frame);
}

void InjectedBundlePage::dump()
{
    ASSERT(InjectedBundle::shared().isTestRunning());

    InjectedBundle::shared().layoutTestController()->invalidateWaitToDumpWatchdogTimer();

    switch (InjectedBundle::shared().layoutTestController()->whatToDump()) {
    case LayoutTestController::RenderTree: {
        WKRetainPtr<WKStringRef> text(AdoptWK, WKBundlePageCopyRenderTreeExternalRepresentation(m_page));
        InjectedBundle::shared().os() << text;
        break;
    }
    case LayoutTestController::MainFrameText:
        dumpFrameText(WKBundlePageGetMainFrame(m_page));
        break;
    case LayoutTestController::AllFramesText:
        dumpAllFramesText();
        break;
    }

    if (InjectedBundle::shared().layoutTestController()->shouldDumpAllFrameScrollPositions())
        dumpAllFrameScrollPositions();
    else if (InjectedBundle::shared().layoutTestController()->shouldDumpMainFrameScrollPosition())
        dumpFrameScrollPosition(WKBundlePageGetMainFrame(m_page));

    InjectedBundle::shared().done();
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!WKBundleFrameIsMainFrame(frame))
        return;

    m_isLoading = false;

    if (this != InjectedBundle::shared().page())
        return;

    if (InjectedBundle::shared().layoutTestController()->waitToDump())
        return;

    dump();
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!WKBundleFrameIsMainFrame(frame))
        return;

    m_isLoading = false;

    if (this != InjectedBundle::shared().page())
        return;

    InjectedBundle::shared().done();
}

void InjectedBundlePage::didReceiveTitleForFrame(WKStringRef title, WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().layoutTestController()->shouldDumpTitleChanges())
        return;

    InjectedBundle::shared().os() << "TITLE CHANGED: " << title << "\n";
}

void InjectedBundlePage::didClearWindowForFrame(WKBundleFrameRef frame, WKBundleScriptWorldRef world)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (WKBundleScriptWorldNormalWorld() != world)
        return;

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    JSObjectRef window = JSContextGetGlobalObject(context);

    JSValueRef exception = 0;
    InjectedBundle::shared().layoutTestController()->makeWindowObject(context, window, &exception);
    InjectedBundle::shared().gcController()->makeWindowObject(context, window, &exception);
    InjectedBundle::shared().eventSendingController()->makeWindowObject(context, window, &exception);
}

void InjectedBundlePage::didCancelClientRedirectForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::willPerformClientRedirectForFrame(WKBundleFrameRef frame, WKURLRef url, double delay, double date)
{
}

void InjectedBundlePage::didChangeLocationWithinPageForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didFinishDocumentLoadForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    unsigned pendingFrameUnloadEvents = WKBundleFrameGetPendingUnloadCount(frame);
    if (pendingFrameUnloadEvents)
        InjectedBundle::shared().os() << frame << " - has " << pendingFrameUnloadEvents << " onunload handler(s)\n";
}

void InjectedBundlePage::didHandleOnloadEventsForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didDisplayInsecureContentForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didRunInsecureContentForFrame(WKBundleFrameRef frame)
{
}

// UI Client Callbacks

void InjectedBundlePage::willAddMessageToConsole(WKBundlePageRef page, WKStringRef message, uint32_t lineNumber, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willAddMessageToConsole(message, lineNumber);
}

void InjectedBundlePage::willSetStatusbarText(WKBundlePageRef page, WKStringRef statusbarText, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willSetStatusbarText(statusbarText);
}

void InjectedBundlePage::willRunJavaScriptAlert(WKBundlePageRef page, WKStringRef message, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptAlert(message, frame);
}

void InjectedBundlePage::willRunJavaScriptConfirm(WKBundlePageRef page, WKStringRef message, WKBundleFrameRef frame, const void *clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptConfirm(message, frame);
}

void InjectedBundlePage::willRunJavaScriptPrompt(WKBundlePageRef page, WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptPrompt(message, defaultValue, frame);
}

void InjectedBundlePage::willAddMessageToConsole(WKStringRef message, uint32_t lineNumber)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    // FIXME: Strip file: urls.
    InjectedBundle::shared().os() << "CONSOLE MESSAGE: line " << lineNumber << ": " << message << "\n";
}

void InjectedBundlePage::willSetStatusbarText(WKStringRef statusbarText)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().layoutTestController()->shouldDumpStatusCallbacks())
        return;

    InjectedBundle::shared().os() << "UI DELEGATE STATUS CALLBACK: setStatusText:" << statusbarText << "\n";
}

void InjectedBundlePage::willRunJavaScriptAlert(WKStringRef message, WKBundleFrameRef)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    InjectedBundle::shared().os() << "ALERT: " << message << "\n";
}

void InjectedBundlePage::willRunJavaScriptConfirm(WKStringRef message, WKBundleFrameRef)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    InjectedBundle::shared().os() << "CONFIRM: " << message << "\n";
}

void InjectedBundlePage::willRunJavaScriptPrompt(WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef)
{
    InjectedBundle::shared().os() << "PROMPT: " << message << ", default text: " << defaultValue <<  "\n";
}

// Editor Client Callbacks

bool InjectedBundlePage::shouldBeginEditing(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldBeginEditing(range);
}

bool InjectedBundlePage::shouldEndEditing(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldEndEditing(range);
}

bool InjectedBundlePage::shouldInsertNode(WKBundlePageRef page, WKBundleNodeHandleRef node, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldInsertNode(node, rangeToReplace, action);
}

bool InjectedBundlePage::shouldInsertText(WKBundlePageRef page, WKStringRef text, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldInsertText(text, rangeToReplace, action);
}

bool InjectedBundlePage::shouldDeleteRange(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldDeleteRange(range);
}

bool InjectedBundlePage::shouldChangeSelectedRange(WKBundlePageRef page, WKBundleRangeHandleRef fromRange, WKBundleRangeHandleRef toRange, WKAffinityType affinity, bool stillSelecting, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldChangeSelectedRange(fromRange, toRange, affinity, stillSelecting);
}

bool InjectedBundlePage::shouldApplyStyle(WKBundlePageRef page, WKBundleCSSStyleDeclarationRef style, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldApplyStyle(style, range);
}

void InjectedBundlePage::didBeginEditing(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didBeginEditing(notificationName);
}

void InjectedBundlePage::didEndEditing(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didEndEditing(notificationName);
}

void InjectedBundlePage::didChange(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didChange(notificationName);
}

void InjectedBundlePage::didChangeSelection(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didChangeSelection(notificationName);
}

bool InjectedBundlePage::shouldBeginEditing(WKBundleRangeHandleRef range)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: shouldBeginEditingInDOMRange:" << toStr(m_page, m_world.get(), range) << "\n";
    return InjectedBundle::shared().layoutTestController()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldEndEditing(WKBundleRangeHandleRef range)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: shouldEndEditingInDOMRange:" << toStr(m_page, m_world.get(), range) << "\n";
    return InjectedBundle::shared().layoutTestController()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertNode(WKBundleNodeHandleRef node, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    static const char* insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: shouldInsertNode:" << dumpPath(m_page, m_world.get(), node) << " replacingDOMRange:" << toStr(m_page, m_world.get(), rangeToReplace) << " givenAction:" << insertactionstring[action] << "\n";
    return InjectedBundle::shared().layoutTestController()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertText(WKStringRef text, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    static const char *insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: shouldInsertText:" << text << " replacingDOMRange:" << toStr(m_page, m_world.get(), rangeToReplace) << " givenAction:" << insertactionstring[action] << "\n";
    return InjectedBundle::shared().layoutTestController()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldDeleteRange(WKBundleRangeHandleRef range)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: shouldDeleteDOMRange:" << toStr(m_page, m_world.get(), range) << "\n";
    return InjectedBundle::shared().layoutTestController()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldChangeSelectedRange(WKBundleRangeHandleRef fromRange, WKBundleRangeHandleRef toRange, WKAffinityType affinity, bool stillSelecting)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    static const char *affinitystring[] = {
        "NSSelectionAffinityUpstream",
        "NSSelectionAffinityDownstream"
    };
    static const char *boolstring[] = {
        "FALSE",
        "TRUE"
    };

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: shouldChangeSelectedDOMRange:" << toStr(m_page, m_world.get(), fromRange) << " toDOMRange:" << toStr(m_page, m_world.get(), toRange) << " affinity:" << affinitystring[affinity] << " stillSelecting:" << boolstring[stillSelecting] << "\n";
    return InjectedBundle::shared().layoutTestController()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldApplyStyle(WKBundleCSSStyleDeclarationRef style, WKBundleRangeHandleRef range)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: shouldApplyStyle:" << style << " toElementsInDOMRange:" << toStr(m_page, m_world.get(), range)  << "\n";
    return InjectedBundle::shared().layoutTestController()->shouldAllowEditing();
}

void InjectedBundlePage::didBeginEditing(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: webViewDidBeginEditing:" << notificationName << "\n";
}

void InjectedBundlePage::didEndEditing(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: webViewDidEndEditing:" << notificationName << "\n";
}

void InjectedBundlePage::didChange(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: webViewDidChange:" << notificationName << "\n";
}

void InjectedBundlePage::didChangeSelection(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().layoutTestController()->shouldDumpEditingCallbacks())
        InjectedBundle::shared().os() << "EDITING DELEGATE: webViewDidChangeSelection:" << notificationName << "\n";
}

} // namespace WTR
