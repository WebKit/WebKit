/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DumpRenderTree.h"
#include "LayoutTestController.h"

#include "EditingDelegate.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"

#include <WebCore/COMPtr.h>
#include <wtf/Platform.h>
#include <JavaScriptCore/Assertions.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit/IWebViewPrivate.h>
#include <string>

using std::string;
using std::wstring;

static JSValueRef dumpAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    dumpAsText = true;   
    return JSValueMakeUndefined(context);
}

static JSValueRef waitUntilDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    waitToDump = true;
    return JSValueMakeUndefined(context);
}

static JSValueRef notifyDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (waitToDump && !topLoadingFrame && !WorkQueue::shared()->count())
        dump();
    waitToDump = false;

    return JSValueMakeUndefined(context);
}

static JSValueRef dumpEditingCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    shouldDumpEditingCallbacks = true;

    return JSValueMakeUndefined(context);
}

static JSValueRef setAcceptsEditingCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    bool acceptsEditing = JSValueToBoolean(context, arguments[0]);

    JSValueRef undefined = JSValueMakeUndefined(context);

    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return undefined;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return undefined;

    COMPtr<IWebEditingDelegate> delegate;
    if (FAILED(viewEditing->editingDelegate(&delegate)))
        return undefined;

    EditingDelegate* editingDelegate = (EditingDelegate*)(IWebEditingDelegate*)delegate.get();
    editingDelegate->setAcceptsEditing(acceptsEditing);

    return undefined;
}

static JSValueRef displayCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ::InvalidateRect(webViewWindow, 0, TRUE);
    ::UpdateWindow(webViewWindow);

    return JSValueMakeUndefined(context);
}

static JSValueRef testRepaintCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    testRepaint = true;

    return JSValueMakeUndefined(context);
}

static JSValueRef repaintSweepHorizontallyCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    repaintSweepHorizontally = true;

    return JSValueMakeUndefined(context);
}

static JSValueRef clearBackForwardListCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return undefined;

    COMPtr<IWebBackForwardList> backForwardList;
    if (FAILED(webView->backForwardList(&backForwardList)))
        return undefined;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(backForwardList->currentItem(&item)))
        return undefined;

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    int capacity;
    if (FAILED(backForwardList->capacity(&capacity)))
        return undefined;

    backForwardList->setCapacity(0);
    backForwardList->setCapacity(capacity);
    backForwardList->addItem(item.get());
    backForwardList->goToItem(item.get());

    return undefined;
}

static JSValueRef setTabKeyCyclesThroughElementsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    bool shouldCycle = false;
    if (argumentCount > 0)
        shouldCycle = JSValueToBoolean(context, arguments[0]);

    JSValueRef undefined = JSValueMakeUndefined(context);

    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return undefined;

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return undefined;

    viewPrivate->setTabKeyCyclesThroughElements(shouldCycle ? TRUE : FALSE);

    return undefined;
}

static JSValueRef dumpTitleChangesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    shouldDumpTitleChanges = true;
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpChildFrameScrollPositionsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    shouldDumpChildFrameScrollPositions = true;
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpChildFramesAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    shouldDumpChildFramesAsText = true;
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpBackForwardListCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    shouldDumpBackForwardList = true;
    return JSValueMakeUndefined(context);
}

static wstring jsValueToWString(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
    JSStringRef jsStr = JSValueToStringCopy(context, value, exception);
    ASSERT(!exception || !*exception);

    char buffer[1024];
    JSStringGetUTF8CString(jsStr, buffer, ARRAYSIZE(buffer));

    JSStringRelease(jsStr);

    wchar_t wbuffer[1024];
    if (!::MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, ARRAYSIZE(wbuffer)))
        return 0;

    return wbuffer;
}

static JSValueRef queueLoadCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    if (argumentCount < 1)
        return undefined;

    wstring url = jsValueToWString(context, arguments[0], exception);
    ASSERT(!exception || !*exception);

    wstring target;
    if (argumentCount >= 2) {
        target = jsValueToWString(context, arguments[1], exception);
        ASSERT(!exception || !*exception);
    }

    COMPtr<IWebDataSource> dataSource;
    if (FAILED(frame->dataSource(&dataSource)))
        return undefined;

    COMPtr<IWebURLResponse> response;
    if (FAILED(dataSource->response(&response)) || !response)
        return undefined;

    BSTR responseURLBSTR;
    if (FAILED(response->URL(&responseURLBSTR)))
        return undefined;
    wstring responseURL(responseURLBSTR, SysStringLen(responseURLBSTR));
    SysFreeString(responseURLBSTR);

    // FIXME: We should do real relative URL resolution here
    int lastSlash = responseURL.rfind('/');
    if (lastSlash != -1)
        responseURL = responseURL.substr(0, lastSlash);

    WorkQueue::shared()->queue(new LoadItem(responseURL + TEXT("/") + url, target));

    return undefined;
}

static JSValueRef queueReloadCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WorkQueue::shared()->queue(new ReloadItem);

    return JSValueMakeUndefined(context);
}

static JSValueRef queueScriptCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    if (argumentCount < 1)
        return undefined;

    wstring script = jsValueToWString(context, arguments[0], exception);
    ASSERT(!exception || !*exception);

    WorkQueue::shared()->queue(new ScriptItem(script));

    return undefined;
}

static JSValueRef queueBackNavigationCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    if (argumentCount < 1)
        return undefined;

    int howFarBack = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!exception || !*exception);

    WorkQueue::shared()->queue(new BackItem(howFarBack));

    return undefined;
}

static JSValueRef queueForwardNavigationCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    if (argumentCount < 1)
        return undefined;

    int howFarForward = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!exception || !*exception);

    WorkQueue::shared()->queue(new ForwardItem(howFarForward));

    return undefined;
}

static JSStaticFunction staticFunctions[] = {
    { "dumpAsText", dumpAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "waitUntilDone", waitUntilDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "notifyDone", notifyDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "dumpEditingCallbacks", dumpEditingCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "setAcceptsEditing", setAcceptsEditingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "display", displayCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "testRepaint", testRepaintCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "repaintSweepHorizontally", repaintSweepHorizontallyCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "clearBackForwardList", clearBackForwardListCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "setTabKeyCyclesThroughElements", setTabKeyCyclesThroughElementsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "dumpTitleChanges", dumpTitleChangesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "dumpChildFrameScrollPositions", dumpChildFrameScrollPositionsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "dumpChildFramesAsText", dumpChildFramesAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "dumpBackForwardList", dumpBackForwardListCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "queueLoad", queueLoadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "queueReload", queueReloadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "queueScript", queueScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "queueBackNavigation", queueBackNavigationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "queueForwardNavigation", queueForwardNavigationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSClassRef getClass(JSContextRef context) {
    static JSClassRef layoutTestControllerClass = 0;

    if (!layoutTestControllerClass) {
        JSClassDefinition classDefinition = { 0 };
        classDefinition.staticFunctions = staticFunctions;

        layoutTestControllerClass = JSClassCreate(&classDefinition);
    }

    return layoutTestControllerClass;
}

JSObjectRef makeLayoutTestController(JSContextRef context)
{
    return JSObjectMake(context, getClass(context), 0);
}

