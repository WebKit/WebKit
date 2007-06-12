/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "EventSender.h"

#include "DraggingInfo.h"

#include <WebCore/COMPtr.h>
#include <wtf/Platform.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <JavaScriptCore/Assertions.h>
#include <WebKit/IWebFrame.h>
#include <WebKit/IWebFramePrivate.h>
#include <windows.h>

static bool down;
static bool dragMode = true;
static bool replayingSavedEvents;
static int timeOffset;
static POINT lastMousePosition;

static MSG msgQueue[1024];
static unsigned endOfQueue;
static unsigned startOfQueue;

static bool didDragEnter;
DraggingInfo* draggingInfo = 0;

static JSValueRef getDragModeCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeBoolean(context, dragMode);
}

static bool setDragModeCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    dragMode = JSValueToBoolean(context, value);
    return true;
}

static JSValueRef leapForwardCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount > 0) {
        timeOffset += JSValueToNumber(context, arguments[0], exception);
        ASSERT(!exception || !*exception);
    }

    return JSValueMakeUndefined(context);
}

static DWORD currentEventTime()
{
    return ::GetTickCount() + timeOffset;
}

static MSG makeMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MSG result = {0};
    result.hwnd = hwnd;
    result.message = message;
    result.wParam = wParam;
    result.lParam = lParam;
    result.time = currentEventTime();
    result.pt = lastMousePosition;

    return result;
}

static LRESULT dispatchMessage(const MSG* msg)
{
    ASSERT(msg);

    ::TranslateMessage(msg);
    return ::DispatchMessage(msg);
}

static JSValueRef mouseDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();

    down = true;
    MSG msg = makeMsg(webViewWindow, WM_LBUTTONDOWN, 0, MAKELPARAM(lastMousePosition.x, lastMousePosition.y));
    dispatchMessage(&msg);

    return JSValueMakeUndefined(context);
}

static inline POINTL pointl(const POINT& point)
{
    POINTL result;
    result.x = point.x;
    result.y = point.y;
    return result;
}

static void doMouseUp(MSG msg)
{
    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();

    dispatchMessage(&msg);
    down = false;

    if (draggingInfo) {
        COMPtr<IWebView> webView;
        COMPtr<IDropTarget> webViewDropTarget;
        if (SUCCEEDED(frame->webView(&webView)) && SUCCEEDED(webView->QueryInterface(IID_IDropTarget, (void**)&webViewDropTarget))) {
            POINT screenPoint = msg.pt;
            ::ClientToScreen(webViewWindow, &screenPoint);
            HRESULT hr = draggingInfo->dropSource()->QueryContinueDrag(0, 0);
            DWORD effect = 0;
            webViewDropTarget->DragOver(0, pointl(screenPoint), &effect);
            if (hr == DRAGDROP_S_DROP && effect != DROPEFFECT_NONE) {
                DWORD effect = 0;
                webViewDropTarget->Drop(draggingInfo->dataObject(), 0, pointl(screenPoint), &effect);
            } else
                webViewDropTarget->DragLeave();

            delete draggingInfo;
            draggingInfo = 0;
        }
    }
}

static JSValueRef mouseUpCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    MSG msg = makeMsg(webViewWindow, WM_LBUTTONUP, 0, MAKELPARAM(lastMousePosition.x, lastMousePosition.y));

    if (dragMode && !replayingSavedEvents) {
        msgQueue[endOfQueue++] = msg;
        replaySavedEvents();
    } else
        doMouseUp(msg);

    return JSValueMakeUndefined(context);
}

static void doMouseMove(MSG msg)
{
    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();

    dispatchMessage(&msg);

    if (down && draggingInfo) {
        POINT screenPoint = msg.pt;
        ::ClientToScreen(webViewWindow, &screenPoint);

        IWebView* webView;
        COMPtr<IDropTarget> webViewDropTarget;
        if (SUCCEEDED(frame->webView(&webView)) && SUCCEEDED(webView->QueryInterface(IID_IDropTarget, (void**)&webViewDropTarget))) {
            DWORD effect = 0;
            if (didDragEnter)
                webViewDropTarget->DragOver(MK_LBUTTON, pointl(screenPoint), &effect);
            else {
                webViewDropTarget->DragEnter(draggingInfo->dataObject(), MK_LBUTTON, pointl(screenPoint), &effect);
                didDragEnter = true;
            }
            draggingInfo->dropSource()->GiveFeedback(effect);
        }
    }
}

static JSValueRef mouseMoveToCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    lastMousePosition.x = (int)JSValueToNumber(context, arguments[0], exception);
    ASSERT(!exception || !*exception);
    lastMousePosition.y = (int)JSValueToNumber(context, arguments[1], exception);
    ASSERT(!exception || !*exception);

    MSG msg = makeMsg(webViewWindow, WM_MOUSEMOVE, down ? MK_LBUTTON : 0, MAKELPARAM(lastMousePosition.x, lastMousePosition.y));

    if (dragMode && down && !replayingSavedEvents) {
        msgQueue[endOfQueue++] = msg;
        return JSValueMakeUndefined(context);
    }

    doMouseMove(msg);

    return JSValueMakeUndefined(context);
}

void replaySavedEvents()
{
    replayingSavedEvents = true;

    MSG emptyMsg = {0};
    while (startOfQueue < endOfQueue) {
        MSG msg = msgQueue[startOfQueue++];
        switch (msg.message) {
            case WM_LBUTTONUP:
                doMouseUp(msg);
                break;
            case WM_MOUSEMOVE:
                doMouseMove(msg);
                break;
            default:
                // Not reached
                break;
        }
    }
    startOfQueue = 0;
    endOfQueue = 0;

    replayingSavedEvents = false;
}

static JSValueRef keyDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    static JSStringRef ctrlKey = JSStringCreateWithUTF8CString("ctrlKey");
    static JSStringRef shiftKey = JSStringCreateWithUTF8CString("shiftKey");
    static JSStringRef altKey = JSStringCreateWithUTF8CString("altKey");
    static JSStringRef metaKey = JSStringCreateWithUTF8CString("metaKey");
    static JSStringRef lengthProperty = JSStringCreateWithUTF8CString("length");

    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();
    
    JSStringRef character = JSValueToStringCopy(context, arguments[0], exception);
    ASSERT(!exception || !*exception);
    int charCode = JSStringGetCharactersPtr(character)[0];
    int virtualKeyCode = toupper(LOBYTE(VkKeyScan(charCode)));
    JSStringRelease(character);

    // Hack to map option-delete to ctrl-delete
    // Remove this when we fix <rdar://problem/5102974> layout tests need a way to decide how to choose the appropriate modifier keys
    bool convertOptionToCtrl = false;
    if (virtualKeyCode == VK_DELETE || virtualKeyCode == VK_BACK)
        convertOptionToCtrl = true;
    
    BYTE keyState[256];
    if (argumentCount > 1) {
        ::GetKeyboardState(keyState);

        BYTE newKeyState[256];
        memcpy(newKeyState, keyState, sizeof(keyState));

        JSObjectRef modifiersArray = JSValueToObject(context, arguments[1], exception);
        if (modifiersArray) {
            int modifiersCount = JSValueToNumber(context, JSObjectGetProperty(context, modifiersArray, lengthProperty, 0), 0);
            for (int i = 0; i < modifiersCount; ++i) {
                JSValueRef value = JSObjectGetPropertyAtIndex(context, modifiersArray, i, 0);
                JSStringRef string = JSValueToStringCopy(context, value, 0);
                if (JSStringIsEqual(string, ctrlKey))
                    newKeyState[VK_CONTROL] = 0x80;
                else if (JSStringIsEqual(string, shiftKey))
                    newKeyState[VK_SHIFT] = 0x80;
                else if (JSStringIsEqual(string, altKey)) {
                    if (convertOptionToCtrl)
                        newKeyState[VK_CONTROL] = 0x80;
                    else
                        newKeyState[VK_MENU] = 0x80;
                } else if (JSStringIsEqual(string, metaKey))
                    newKeyState[VK_MENU] = 0x80;

                JSStringRelease(string);
            }
        }

        ::SetKeyboardState(newKeyState);
    }

    MSG msg = makeMsg(webViewWindow, WM_KEYDOWN, virtualKeyCode, 0);
    dispatchMessage(&msg);
    
    if (argumentCount > 1)
        ::SetKeyboardState(keyState);

    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticFunctions[] = {
    { "mouseDown", mouseDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseUp", mouseUpCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseMoveTo", mouseMoveToCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "leapForward", leapForwardCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "keyDown", keyDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSStaticValue staticValues[] = {
    { "dragMode", getDragModeCallback, setDragModeCallback, kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

static JSClassRef getClass(JSContextRef context) {
    static JSClassRef eventSenderClass = 0;

    if (!eventSenderClass) {
        JSClassDefinition classDefinition = {0};
        classDefinition.staticFunctions = staticFunctions;
        classDefinition.staticValues = staticValues;

        eventSenderClass = JSClassCreate(&classDefinition);
    }

    return eventSenderClass;
}

JSObjectRef makeEventSender(JSContextRef context)
{
    return JSObjectMake(context, getClass(context), 0);
}
