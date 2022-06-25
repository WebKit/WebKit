/*
 * Copyright (C) 2007, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Baidu Inc. All rights reserved
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "config.h"
#include "EventSender.h"

#include "DRTDataObject.h"
#include "DRTDropSource.h"
#include "DraggingInfo.h"
#include "DumpRenderTree.h"
#include "WebCoreTestSupport.h"

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebCore/COMPtr.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebKitLegacy/WebFrame.h>
#include <WebKitLegacy/WebKit.h>
#include <windows.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/text/WTFString.h>

#define WM_DRT_SEND_QUEUED_EVENT (WM_APP+1)
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

static bool down;
static bool dragMode = true;
static bool replayingSavedEvents;
static int timeOffset;
static POINT lastMousePosition;

struct DelayedMessage {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    MSG msg;
    unsigned delay;
};

static DelayedMessage msgQueue[1024];
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

static JSValueRef getConstantCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_KEYDOWN"))
        return JSValueMakeNumber(context, WM_KEYDOWN);
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_KEYUP"))
        return JSValueMakeNumber(context, WM_KEYUP);
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_CHAR"))
        return JSValueMakeNumber(context, WM_CHAR);
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_DEADCHAR"))
        return JSValueMakeNumber(context, WM_DEADCHAR);
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_SYSKEYDOWN"))
        return JSValueMakeNumber(context, WM_SYSKEYDOWN);
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_SYSKEYUP"))
        return JSValueMakeNumber(context, WM_SYSKEYUP);
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_SYSCHAR"))
        return JSValueMakeNumber(context, WM_SYSCHAR);
    if (JSStringIsEqualToUTF8CString(propertyName, "WM_SYSDEADCHAR"))
        return JSValueMakeNumber(context, WM_SYSDEADCHAR);
    ASSERT_NOT_REACHED();
    return JSValueMakeUndefined(context);
}

static JSValueRef leapForwardCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount > 0) {
        msgQueue[endOfQueue].delay = JSValueToNumber(context, arguments[0], exception);
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
    MSG result { };
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

static JSValueRef contextClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();

    down = true;
    MSG msg = makeMsg(webViewWindow, WM_RBUTTONDOWN, 0, MAKELPARAM(lastMousePosition.x, lastMousePosition.y));
    dispatchMessage(&msg);
    down = false;
    msg = makeMsg(webViewWindow, WM_RBUTTONUP, 0, MAKELPARAM(lastMousePosition.x, lastMousePosition.y));
    dispatchMessage(&msg);
    
    return JSValueMakeUndefined(context);
}

static WPARAM buildModifierFlags(JSContextRef context, const JSValueRef modifiers)
{
    JSObjectRef modifiersArray = JSValueToObject(context, modifiers, 0);
    if (!modifiersArray)
        return 0;

    WPARAM flags = 0;
    int modifiersCount = JSValueToNumber(context, JSObjectGetProperty(context, modifiersArray, JSStringCreateWithUTF8CString("length"), 0), 0);
    for (int i = 0; i < modifiersCount; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, modifiersArray, i, 0);
        JSStringRef string = JSValueToStringCopy(context, value, 0);
        if (JSStringIsEqualToUTF8CString(string, "ctrlKey")
            || JSStringIsEqualToUTF8CString(string, "addSelectionKey"))
            flags |= MK_CONTROL;
        else if (JSStringIsEqualToUTF8CString(string, "shiftKey")
                 || JSStringIsEqualToUTF8CString(string, "rangeSelectionKey"))
            flags |= MK_SHIFT;
        // No way to specifiy altKey in a MSG.

        JSStringRelease(string);
    }
    return flags;
}

static JSValueRef mouseDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();

    down = true;
    int mouseType = WM_LBUTTONDOWN;
    if (argumentCount >= 1) {
        int mouseNumber = JSValueToNumber(context, arguments[0], exception);
        switch (mouseNumber) {
        case 0:
            mouseType = WM_LBUTTONDOWN;
            break;
        case 1:
            mouseType = WM_MBUTTONDOWN;
            break;
        case 2:
            mouseType = WM_RBUTTONDOWN;
            break;
        case 3:
            // fast/events/mouse-click-events expects the 4th button has event.button = 1, so send an WM_BUTTONDOWN
            mouseType = WM_MBUTTONDOWN;
            break;
        default:
            mouseType = WM_LBUTTONDOWN;
            break;
        }
    }

    WPARAM wparam = 0;
    if (argumentCount >= 2)
        wparam |= buildModifierFlags(context, arguments[1]);
        
    MSG msg = makeMsg(webViewWindow, mouseType, wparam, MAKELPARAM(lastMousePosition.x, lastMousePosition.y));
    if (!msgQueue[endOfQueue].delay)
        dispatchMessage(&msg);
    else {
        // replaySavedEvents has the required logic to make leapForward delays work
        msgQueue[endOfQueue++].msg = msg;
        replaySavedEvents();
    }

    return JSValueMakeUndefined(context);
}

static inline POINTL pointl(const POINT& point)
{
    POINTL result;
    result.x = point.x;
    result.y = point.y;
    return result;
}

static void doMouseUp(MSG msg, HRESULT* oleDragAndDropReturnValue = 0)
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
            DWORD effect = 0;
            ::ClientToScreen(webViewWindow, &screenPoint);
            if (!didDragEnter) {
                webViewDropTarget->DragEnter(draggingInfo->dataObject(), 0, pointl(screenPoint), &effect);
                didDragEnter = true;
            }
            HRESULT hr = draggingInfo->dropSource()->QueryContinueDrag(0, 0);
            if (oleDragAndDropReturnValue)
                *oleDragAndDropReturnValue = hr;
            webViewDropTarget->DragOver(0, pointl(screenPoint), &effect);
            if (hr == DRAGDROP_S_DROP && effect != DROPEFFECT_NONE) {
                DWORD effect = 0;
                webViewDropTarget->Drop(draggingInfo->dataObject(), 0, pointl(screenPoint), &effect);
                draggingInfo->setPerformedDropEffect(effect);
            } else
                webViewDropTarget->DragLeave();

            // Reset didDragEnter so that another drag started within the same frame works properly.
            didDragEnter = false;
        }
    }
}

static JSValueRef mouseUpCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int mouseType = WM_LBUTTONUP;
    if (argumentCount >= 1) {
        int mouseNumber = JSValueToNumber(context, arguments[0], exception);
        switch (mouseNumber) {
        case 0:
            mouseType = WM_LBUTTONUP;
            break;
        case 1:
            mouseType = WM_MBUTTONUP;
            break;
        case 2:
            mouseType = WM_RBUTTONUP;
            break;
        case 3:
            // fast/events/mouse-click-events expects the 4th button has event.button = 1, so send an WM_MBUTTONUP
            mouseType = WM_MBUTTONUP;
            break;
        default:
            mouseType = WM_LBUTTONUP;
            break;
        }
    }

    WPARAM wparam = 0;
    if (argumentCount >= 2)
        wparam |= buildModifierFlags(context, arguments[1]);

    MSG msg = makeMsg(webViewWindow, mouseType, wparam, MAKELPARAM(lastMousePosition.x, lastMousePosition.y));

    if ((dragMode && !replayingSavedEvents) || msgQueue[endOfQueue].delay) {
        msgQueue[endOfQueue++].msg = msg;
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
                webViewDropTarget->DragEnter(draggingInfo->dataObject(), 0, pointl(screenPoint), &effect);
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
        msgQueue[endOfQueue++].msg = msg;
        return JSValueMakeUndefined(context);
    }

    doMouseMove(msg);

    return JSValueMakeUndefined(context);
}

void replaySavedEvents(HRESULT* oleDragAndDropReturnValue)
{
    replayingSavedEvents = true;
  
    MSG msg { };

    while (startOfQueue < endOfQueue && !msgQueue[startOfQueue].delay) {
        msg = msgQueue[startOfQueue++].msg;
        switch (msg.message) {
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                doMouseUp(msg, oleDragAndDropReturnValue);
                break;
            case WM_MOUSEMOVE:
                doMouseMove(msg);
                break;
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                dispatchMessage(&msg);
                break;
            default:
                // Not reached
                break;
        }
    }

    int numQueuedMessages = endOfQueue - startOfQueue;
    if (!numQueuedMessages) {
        startOfQueue = 0;
        endOfQueue = 0;
        replayingSavedEvents = false;
        ASSERT(!down);
        return;
    }

    if (msgQueue[startOfQueue].delay) {
        ::Sleep(msgQueue[startOfQueue].delay);
        msgQueue[startOfQueue].delay = 0;
    }

    ::PostMessage(webViewWindow, WM_DRT_SEND_QUEUED_EVENT, 0, 0);
    while (::GetMessage(&msg, webViewWindow, 0, 0)) {
        // FIXME: Why do we get a WM_MOUSELEAVE? it breaks tests
        if (msg.message == WM_MOUSELEAVE)
            continue;
        if (msg.message != WM_DRT_SEND_QUEUED_EVENT) {
            dispatchMessage(&msg);
            continue;
        }
        msg = msgQueue[startOfQueue++].msg;
        switch (msg.message) {
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                doMouseUp(msg, oleDragAndDropReturnValue);
                break;
            case WM_MOUSEMOVE:
                doMouseMove(msg);
                break;
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                dispatchMessage(&msg);
                break;
            default:
                // Not reached
                break;
        }
        if (startOfQueue >= endOfQueue)
            break;
        ::Sleep(msgQueue[startOfQueue].delay);
        msgQueue[startOfQueue].delay = 0;
        ::PostMessage(webViewWindow, WM_DRT_SEND_QUEUED_EVENT, 0, 0);
    }
    startOfQueue = 0;
    endOfQueue = 0;

    replayingSavedEvents = false;
}

static unsigned makeKeyDataForScanCode(int virtualKeyCode)
{
    unsigned scancode = MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC);
    int keyData = scancode & 0xFF;

    bool forceExtended = false;
    switch (virtualKeyCode) {
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_END:
    case VK_HOME:
    case VK_INSERT:
    case VK_DELETE:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_RCONTROL:
    case VK_RMENU:
        // Some keys need to turn on KF_EXTENDED explicitly
        forceExtended = true;
        break;
    default:
        break;
    }

    scancode = scancode >> 8;
    if (scancode == 0xe0 || scancode == 0xe1 || forceExtended)
        keyData += KF_EXTENDED;
    unsigned repeat = 1;
    return keyData << 16 | repeat;
}

static JSValueRef keyDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    static const JSStringRef lengthProperty = JSStringCreateWithUTF8CString("length");

    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();
    
    JSStringRef character = JSValueToStringCopy(context, arguments[0], exception);
    ASSERT(!*exception);
    int virtualKeyCode = 0;
    int charCode = 0;
    bool needsShiftKeyModifier = false;
    if (JSStringIsEqualToUTF8CString(character, "leftArrow"))
        virtualKeyCode = VK_LEFT;
    else if (JSStringIsEqualToUTF8CString(character, "rightArrow"))
        virtualKeyCode = VK_RIGHT;
    else if (JSStringIsEqualToUTF8CString(character, "upArrow"))
        virtualKeyCode = VK_UP;
    else if (JSStringIsEqualToUTF8CString(character, "downArrow"))
        virtualKeyCode = VK_DOWN;
    else if (JSStringIsEqualToUTF8CString(character, "pageUp"))
        virtualKeyCode = VK_PRIOR;
    else if (JSStringIsEqualToUTF8CString(character, "pageDown"))
        virtualKeyCode = VK_NEXT;
    else if (JSStringIsEqualToUTF8CString(character, "home"))
        virtualKeyCode = VK_HOME;
    else if (JSStringIsEqualToUTF8CString(character, "end"))
        virtualKeyCode = VK_END;
    else if (JSStringIsEqualToUTF8CString(character, "insert"))
        virtualKeyCode = VK_INSERT;
    else if (JSStringIsEqualToUTF8CString(character, "delete"))
        virtualKeyCode = VK_DELETE;
    else if (JSStringIsEqualToUTF8CString(character, "printScreen"))
        virtualKeyCode = VK_SNAPSHOT;
    else if (JSStringIsEqualToUTF8CString(character, "menu"))
        virtualKeyCode = VK_APPS;
    else if (JSStringIsEqualToUTF8CString(character, "leftControl"))
        virtualKeyCode = VK_LCONTROL;
    else if (JSStringIsEqualToUTF8CString(character, "leftShift"))
        virtualKeyCode = VK_LSHIFT;
    else if (JSStringIsEqualToUTF8CString(character, "leftAlt"))
        virtualKeyCode = VK_LMENU;
    else if (JSStringIsEqualToUTF8CString(character, "rightControl"))
        virtualKeyCode = VK_RCONTROL;
    else if (JSStringIsEqualToUTF8CString(character, "rightShift"))
        virtualKeyCode = VK_RSHIFT;
    else if (JSStringIsEqualToUTF8CString(character, "rightAlt"))
        virtualKeyCode = VK_RMENU;
    else {
        size_t characterLength = JSStringGetLength(character);
        static const char shiftedUSCharacters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ~!@#$%^&*()_+{}|:\"<>?";
        const wchar_t* characterPtr = JSStringGetCharactersPtr(character);
        if (characterLength == 1) {
            charCode = characterPtr[0];
            virtualKeyCode = LOBYTE(VkKeyScan(charCode));
            if (strchr(shiftedUSCharacters, charCode))
                needsShiftKeyModifier = true;
        } else if (characterPtr[0] == 'F') {
            if (characterLength == 2 && isASCIIDigit(characterPtr[1]))
                virtualKeyCode = VK_F1 + characterPtr[1] - '1';
            else if (characterLength == 3 && characterPtr[1] == '1' && isASCIIDigit(characterPtr[2]))
                virtualKeyCode = VK_F10 + characterPtr[2] - '0';
        }
    }
    JSStringRelease(character);

    unsigned keyData = makeKeyDataForScanCode(virtualKeyCode);

    switch (virtualKeyCode) {
    case VK_LCONTROL:
    case VK_RCONTROL:
        virtualKeyCode = VK_CONTROL;
        break;
    case VK_LSHIFT:
    case VK_RSHIFT:
        virtualKeyCode = VK_SHIFT;
        break;
    case VK_LMENU:
    case VK_RMENU:
        virtualKeyCode = VK_MENU;
        break;
    default:
        break;
    }

    BYTE keyState[256];
    if (argumentCount > 1 || needsShiftKeyModifier) {
        ::GetKeyboardState(keyState);

        BYTE newKeyState[256];
        memcpy(newKeyState, keyState, sizeof(keyState));

        if (needsShiftKeyModifier)
            newKeyState[VK_SHIFT] = 0x80;

        if (argumentCount > 1) {
            JSObjectRef modifiersArray = JSValueToObject(context, arguments[1], 0);
            if (modifiersArray) {
                int modifiersCount = JSValueToNumber(context, JSObjectGetProperty(context, modifiersArray, lengthProperty, 0), 0);
                for (int i = 0; i < modifiersCount; ++i) {
                    JSValueRef value = JSObjectGetPropertyAtIndex(context, modifiersArray, i, 0);
                    JSStringRef string = JSValueToStringCopy(context, value, 0);
                    if (JSStringIsEqualToUTF8CString(string, "ctrlKey") || JSStringIsEqualToUTF8CString(string, "addSelectionKey"))
                        newKeyState[VK_CONTROL] = 0x80;
                    else if (JSStringIsEqualToUTF8CString(string, "shiftKey") || JSStringIsEqualToUTF8CString(string, "rangeSelectionKey"))
                        newKeyState[VK_SHIFT] = 0x80;
                    else if (JSStringIsEqualToUTF8CString(string, "altKey"))
                        newKeyState[VK_MENU] = 0x80;

                    JSStringRelease(string);
                }
            }
        }

        ::SetKeyboardState(newKeyState);
    }

    MSG msg = makeMsg(webViewWindow, (::GetKeyState(VK_MENU) & 0x8000) ? WM_SYSKEYDOWN : WM_KEYDOWN, virtualKeyCode, keyData);
    if (virtualKeyCode != 255)
        dispatchMessage(&msg);
    else {
        // For characters that do not exist in the active keyboard layout,
        // ::Translate will not work, so we post an WM_CHAR event ourselves.
        ::PostMessage(webViewWindow, WM_CHAR, charCode, 0);
    }

    // Tests expect that all messages are processed by the time keyDown() returns.
    if (::PeekMessage(&msg, webViewWindow, WM_CHAR, WM_CHAR, PM_REMOVE) || ::PeekMessage(&msg, webViewWindow, WM_SYSCHAR, WM_SYSCHAR, PM_REMOVE))
        ::DispatchMessage(&msg);

    MSG msgUp = makeMsg(webViewWindow, (::GetKeyState(VK_MENU) & 0x8000) ? WM_SYSKEYUP : WM_KEYUP, virtualKeyCode, keyData);
    ::DispatchMessage(&msgUp);

    if (argumentCount > 1 || needsShiftKeyModifier)
        ::SetKeyboardState(keyState);

    return JSValueMakeUndefined(context);
}

// eventSender.dispatchMessage(message, wParam, lParam, time = currentEventTime(), x = lastMousePosition.x, y = lastMousePosition.y)
static JSValueRef dispatchMessageCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 3)
        return JSValueMakeUndefined(context);

    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();
    
    MSG msg = {};
    msg.hwnd = webViewWindow;
    msg.message = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);
    msg.wParam = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);
    msg.lParam = static_cast<ULONG_PTR>(JSValueToNumber(context, arguments[2], exception));
    ASSERT(!*exception);
    if (argumentCount >= 4) {
        msg.time = JSValueToNumber(context, arguments[3], exception);
        ASSERT(!*exception);
    }
    if (!msg.time)
        msg.time = currentEventTime();
    if (argumentCount >= 6) {
        msg.pt.x = JSValueToNumber(context, arguments[4], exception);
        ASSERT(!*exception);
        msg.pt.y = JSValueToNumber(context, arguments[5], exception);
        ASSERT(!*exception);
    } else
        msg.pt = lastMousePosition;

    ::DispatchMessage(&msg);

    return JSValueMakeUndefined(context);
}

static JSValueRef textZoomInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return JSValueMakeUndefined(context);

    COMPtr<IWebIBActions> webIBActions(Query, webView);
    if (!webIBActions)
        return JSValueMakeUndefined(context);

    webIBActions->makeTextLarger(0);
    return JSValueMakeUndefined(context);
}

static JSValueRef textZoomOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return JSValueMakeUndefined(context);

    COMPtr<IWebIBActions> webIBActions(Query, webView);
    if (!webIBActions)
        return JSValueMakeUndefined(context);

    webIBActions->makeTextSmaller(0);
    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageInCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return JSValueMakeUndefined(context);

    COMPtr<IWebIBActions> webIBActions(Query, webView);
    if (!webIBActions)
        return JSValueMakeUndefined(context);

    webIBActions->zoomPageIn(0);
    return JSValueMakeUndefined(context);
}

static JSValueRef zoomPageOutCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return JSValueMakeUndefined(context);

    COMPtr<IWebIBActions> webIBActions(Query, webView);
    if (!webIBActions)
        return JSValueMakeUndefined(context);

    webIBActions->zoomPageOut(0);
    return JSValueMakeUndefined(context);
}

static JSValueRef beginDragWithFilesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSObjectRef filesArray = JSValueToObject(context, arguments[0], 0);

    if (!filesArray)
        return JSValueMakeUndefined(context);

    JSStringRef lengthProperty = JSStringCreateWithUTF8CString("length");
    Vector<UChar> files;
    int filesCount = JSValueToNumber(context, JSObjectGetProperty(context, filesArray, lengthProperty, 0), 0);
    for (int i = 0; i < filesCount; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, filesArray, i, 0);
        JSStringRef file = JSValueToStringCopy(context, value, 0);
        files.append(JSStringGetCharactersPtr(file), JSStringGetLength(file));
        files.append(0);
        JSStringRelease(file);
    }

    if (files.isEmpty())
        return JSValueMakeUndefined(context);

    // We should append "0" in the end of |files| so that |DragQueryFileW| retrieved the number of files correctly from Ole Clipboard.
    files.append(0);

    STGMEDIUM hDropMedium { };
    hDropMedium.tymed = TYMED_HGLOBAL;
    SIZE_T dropFilesSize = sizeof(DROPFILES) + (sizeof(WCHAR) * files.size());
    hDropMedium.hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, dropFilesSize);
    if (!hDropMedium.hGlobal)
        return JSValueMakeUndefined(context);

    DROPFILES* dropFiles = reinterpret_cast<DROPFILES*>(GlobalLock(hDropMedium.hGlobal));
    memset(dropFiles, 0, sizeof(DROPFILES));
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;

    UChar* data = reinterpret_cast<UChar*>(reinterpret_cast<BYTE*>(dropFiles) + sizeof(DROPFILES));
    for (size_t i = 0; i < files.size(); ++i)
        data[i] = files[i];
    GlobalUnlock(hDropMedium.hGlobal); 

    STGMEDIUM hFileNameMedium { };
    hFileNameMedium.tymed = TYMED_HGLOBAL;
    SIZE_T hFileNameSize = sizeof(WCHAR) * files.size();
    hFileNameMedium.hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, hFileNameSize);
    if (!hFileNameMedium.hGlobal)
        return JSValueMakeUndefined(context);

    WCHAR* hFileName = static_cast<WCHAR*>(GlobalLock(hFileNameMedium.hGlobal));
    for (size_t i = 0; i < files.size(); i++)
        hFileName[i] = files[i];
    GlobalUnlock(hFileNameMedium.hGlobal);

    if (draggingInfo) {
        delete draggingInfo;
        draggingInfo = 0;
    }

    COMPtr<DRTDataObject> dataObeject;
    COMPtr<IDropSource> source;
    if (FAILED(DRTDataObject::createInstance(&dataObeject)))
        dataObeject = 0;

    if (FAILED(DRTDropSource::createInstance(&source)))
        source = 0;

    if (dataObeject && source) {
        draggingInfo = new DraggingInfo(dataObeject.get(), source.get());
        draggingInfo->setPerformedDropEffect(DROPEFFECT_COPY);
    }

    if (draggingInfo) {
        draggingInfo->dataObject()->SetData(cfHDropFormat(), &hDropMedium, FALSE);
        draggingInfo->dataObject()->SetData(cfFileNameWFormat(), &hFileNameMedium, FALSE);
        draggingInfo->dataObject()->SetData(cfUrlWFormat(), &hFileNameMedium, FALSE);
        OleSetClipboard(draggingInfo->dataObject());
        down = true;
    }

    JSStringRelease(lengthProperty);
    return JSValueMakeUndefined(context);
}

static JSValueRef scalePageByCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return JSValueMakeUndefined(context);

    COMPtr<IWebViewPrivate2> webViewPrivate;
    if (FAILED(webView->QueryInterface(&webViewPrivate)))
        return JSValueMakeUndefined(context);

    POINT origin;
    origin.x = 0;
    origin.y = 0;

    double scale = JSValueToNumber(context, arguments[0], exception);

    if (argumentCount > 1)
        origin.x = JSValueToNumber(context, arguments[1], exception);
    if (argumentCount > 2)
        origin.y = JSValueToNumber(context, arguments[2], exception);

    webViewPrivate->scaleWebView(scale, origin);

    return JSValueMakeUndefined(context);
}

void mouseScrollBy(double x, double y, bool continuous)
{
    RECT rect;
    ::GetWindowRect(webViewWindow, &rect);

    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->layout();

    if (x) {
        UINT scrollChars = 1;
        ::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0);
        x *= WHEEL_DELTA / scrollChars;
        if (continuous)
            x /= WebCore::cScrollbarPixelsPerLine;
        MSG msg = makeMsg(webViewWindow, WM_MOUSEHWHEEL, MAKEWPARAM(0, x), MAKELPARAM(rect.left + lastMousePosition.x, rect.top + lastMousePosition.y));
        dispatchMessage(&msg);
    }

    if (y) {
        UINT scrollLines = 3;
        ::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
        y *= WHEEL_DELTA / scrollLines;
        if (continuous)
            y /= WebCore::cScrollbarPixelsPerLine;
        MSG msg = makeMsg(webViewWindow, WM_MOUSEWHEEL, MAKEWPARAM(0, y), MAKELPARAM(rect.left + lastMousePosition.x, rect.top + lastMousePosition.y));
        dispatchMessage(&msg);
    }
}

static JSValueRef mouseScrollBy(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    double deltaX = JSValueToNumber(context, arguments[0], exception);

    double deltaY = 0;

    if (argumentCount >= 2)
        deltaY = JSValueToNumber(context, arguments[1], exception);

    mouseScrollBy(deltaX, deltaY, false);

    return JSValueMakeUndefined(context);
}

static JSValueRef mouseScrollByWithWheelAndMomentumPhasesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return mouseScrollBy(context, function, thisObject, argumentCount, arguments, exception);
}

static JSValueRef continuousMouseScrollBy(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    double deltaX = JSValueToNumber(context, arguments[0], exception);

    double deltaY = 0;

    if (argumentCount >= 2)
        deltaY = JSValueToNumber(context, arguments[1], exception);

    mouseScrollBy(deltaX, deltaY, true);

    return JSValueMakeUndefined(context);
}

static JSValueRef monitorWheelEvents(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    COMPtr<IWebFrame2> frame2;
    if (FAILED(frame->QueryInterface(&frame2)))
        return JSValueMakeUndefined(context);

    WebCore::Frame* coreFrame = core(static_cast<WebFrame*>(frame2.get()));

    bool resetLatching = true;
    if (argumentCount > 0) {
        auto resetLatchingString = adopt(JSStringCreateWithUTF8CString("resetLatching"));
        auto resetLatchingValue = JSObjectGetProperty(context, JSValueToObject(context, arguments[0], nullptr), resetLatchingString.get(), nullptr);

        if (resetLatchingValue && JSValueIsBoolean(context, resetLatchingValue))
            resetLatching = JSValueToBoolean(context, resetLatchingValue);
    }

    WebCoreTestSupport::monitorWheelEvents(*coreFrame, resetLatching);
    
    return JSValueMakeUndefined(context);
}

static JSValueRef callAfterScrollingCompletes(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);
    
    JSObjectRef jsCallbackFunction = JSValueToObject(context, arguments[0], exception);
    if (!jsCallbackFunction)
        return JSValueMakeUndefined(context);
    
    if (!frame)
        return JSValueMakeUndefined(context);
    
    JSGlobalContextRef globalContext = frame->globalContext();
    
    COMPtr<IWebFrame2> frame2;
    if (FAILED(frame->QueryInterface(&frame2)))
        return JSValueMakeUndefined(context);

    WebCore::Frame* coreFrame = core(static_cast<WebFrame*>(frame2.get()));
    WebCoreTestSupport::setWheelEventMonitorTestCallbackAndStartMonitoring(false, false, *coreFrame, globalContext, jsCallbackFunction);

    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticFunctions[] = {
    { "contextClick", contextClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseDown", mouseDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseUp", mouseUpCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseMoveTo", mouseMoveToCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "leapForward", leapForwardCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "keyDown", keyDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "dispatchMessage", dispatchMessageCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "textZoomIn", textZoomInCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "textZoomOut", textZoomOutCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "zoomPageIn", zoomPageInCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "zoomPageOut", zoomPageOutCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "beginDragWithFiles", beginDragWithFilesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "scalePageBy", scalePageByCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseScrollBy", mouseScrollBy, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "mouseScrollByWithWheelAndMomentumPhases", mouseScrollByWithWheelAndMomentumPhasesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "continuousMouseScrollBy", continuousMouseScrollBy,  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "monitorWheelEvents", monitorWheelEvents, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "callAfterScrollingCompletes", callAfterScrollingCompletes, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSStaticValue staticValues[] = {
    { "dragMode", getDragModeCallback, setDragModeCallback, kJSPropertyAttributeNone },
    { "WM_KEYDOWN", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { "WM_KEYUP", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { "WM_CHAR", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { "WM_DEADCHAR", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { "WM_SYSKEYDOWN", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { "WM_SYSKEYUP", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { "WM_SYSCHAR", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { "WM_SYSDEADCHAR", getConstantCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeNone },
    { 0, 0, 0, 0 }
};

static JSClassRef getClass(JSContextRef context)
{
    static JSClassRef eventSenderClass = 0;

    if (!eventSenderClass) {
        JSClassDefinition classDefinition { };
        classDefinition.staticFunctions = staticFunctions;
        classDefinition.staticValues = staticValues;

        eventSenderClass = JSClassCreate(&classDefinition);
    }

    return eventSenderClass;
}

JSObjectRef makeEventSender(JSContextRef context, bool isTopFrame)
{
    if (isTopFrame) {
        down = false;
        dragMode = true;
        replayingSavedEvents = false;
        timeOffset = 0;
        lastMousePosition.x = 0;
        lastMousePosition.y = 0;

        endOfQueue = 0;
        startOfQueue = 0;

        didDragEnter = false;
        draggingInfo = 0;
    }
    return JSObjectMake(context, getClass(context), 0);
}
