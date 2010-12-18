/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "AccessibilityController.h"

#include "AccessibilityUIElement.h"
#include "DumpRenderTree.h"
#include "FrameLoadDelegate.h"
#include <JavaScriptCore/Assertions.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <WebCore/COMPtr.h>
#include <WebKit/WebKit.h>
#include <oleacc.h>
#include <string>

using namespace std;

AccessibilityController::AccessibilityController()
    : m_focusEventHook(0)
    , m_scrollingStartEventHook(0)
    , m_valueChangeEventHook(0)
    , m_allEventsHook(0)
{
}

AccessibilityController::~AccessibilityController()
{
    setLogFocusEvents(false);
    setLogValueChangeEvents(false);

    if (m_allEventsHook)
        UnhookWinEvent(m_allEventsHook);

    for (HashMap<PlatformUIElement, JSObjectRef>::iterator it = m_notificationListeners.begin(); it != m_notificationListeners.end(); ++it)
        JSValueUnprotect(frame->globalContext(), it->second);
}

AccessibilityUIElement AccessibilityController::elementAtPoint(int x, int y)
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityController::focusedElement()
{
    COMPtr<IAccessible> rootAccessible = rootElement().platformUIElement();

    VARIANT vFocus;
    if (FAILED(rootAccessible->get_accFocus(&vFocus)))
        return 0;

    if (V_VT(&vFocus) == VT_I4) {
        ASSERT(V_I4(&vFocus) == CHILDID_SELF);
        // The root accessible object is the focused object.
        return rootAccessible;
    }

    ASSERT(V_VT(&vFocus) == VT_DISPATCH);
    // We have an IDispatch; query for IAccessible.
    return COMPtr<IAccessible>(Query, V_DISPATCH(&vFocus));
}

AccessibilityUIElement AccessibilityController::rootElement()
{
    COMPtr<IWebView> view;
    if (FAILED(frame->webView(&view)))
        return 0;

    COMPtr<IWebViewPrivate> viewPrivate(Query, view);
    if (!viewPrivate)
        return 0;

    HWND webViewWindow;
    if (FAILED(viewPrivate->viewWindow((OLE_HANDLE*)&webViewWindow)))
        return 0;

    // Get the root accessible object by querying for the accessible object for the
    // WebView's window.
    COMPtr<IAccessible> rootAccessible;
    if (FAILED(AccessibleObjectFromWindow(webViewWindow, static_cast<DWORD>(OBJID_CLIENT), __uuidof(IAccessible), reinterpret_cast<void**>(&rootAccessible))))
        return 0;

    return rootAccessible;
}

static void CALLBACK logEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    // Get the accessible object for this event.
    COMPtr<IAccessible> parentObject;

    VARIANT vChild;
    VariantInit(&vChild);

    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &parentObject, &vChild);
    ASSERT(SUCCEEDED(hr));

    // Get the name of the focused element, and log it to stdout.
    BSTR nameBSTR;
    hr = parentObject->get_accName(vChild, &nameBSTR);
    ASSERT(SUCCEEDED(hr));
    wstring name(nameBSTR, ::SysStringLen(nameBSTR));
    SysFreeString(nameBSTR);

    switch (event) {
        case EVENT_OBJECT_FOCUS:
            printf("Received focus event for object '%S'.\n", name.c_str());
            break;

        case EVENT_OBJECT_VALUECHANGE: {
            BSTR valueBSTR;
            hr = parentObject->get_accValue(vChild, &valueBSTR);
            ASSERT(SUCCEEDED(hr));
            wstring value(valueBSTR, ::SysStringLen(valueBSTR));
            SysFreeString(valueBSTR);

            printf("Received value change event for object '%S', value '%S'.\n", name.c_str(), value.c_str());
            break;
        }

        case EVENT_SYSTEM_SCROLLINGSTART:
            printf("Received scrolling start event for object '%S'.\n", name.c_str());
            break;

        default:
            printf("Received unknown event for object '%S'.\n", name.c_str());
            break;
    }

    VariantClear(&vChild);
}

void AccessibilityController::setLogFocusEvents(bool logFocusEvents)
{
    if (!!m_focusEventHook == logFocusEvents)
        return;

    if (!logFocusEvents) {
        UnhookWinEvent(m_focusEventHook);
        m_focusEventHook = 0;
        return;
    }

    // Ensure that accessibility is initialized for the WebView by querying for
    // the root accessible object.
    rootElement();

    m_focusEventHook = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, GetModuleHandle(0), logEventProc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);

    ASSERT(m_focusEventHook);
}

void AccessibilityController::setLogValueChangeEvents(bool logValueChangeEvents)
{
    if (!!m_valueChangeEventHook == logValueChangeEvents)
        return;

    if (!logValueChangeEvents) {
        UnhookWinEvent(m_valueChangeEventHook);
        m_valueChangeEventHook = 0;
        return;
    }

    // Ensure that accessibility is initialized for the WebView by querying for
    // the root accessible object.
    rootElement();

    m_valueChangeEventHook = SetWinEventHook(EVENT_OBJECT_VALUECHANGE, EVENT_OBJECT_VALUECHANGE, GetModuleHandle(0), logEventProc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);

    ASSERT(m_valueChangeEventHook);
}

void AccessibilityController::setLogScrollingStartEvents(bool logScrollingStartEvents)
{
    if (!!m_scrollingStartEventHook == logScrollingStartEvents)
        return;

    if (!logScrollingStartEvents) {
        UnhookWinEvent(m_scrollingStartEventHook);
        m_scrollingStartEventHook = 0;
        return;
    }

    // Ensure that accessibility is initialized for the WebView by querying for
    // the root accessible object.
    rootElement();

    m_scrollingStartEventHook = SetWinEventHook(EVENT_SYSTEM_SCROLLINGSTART, EVENT_SYSTEM_SCROLLINGSTART, GetModuleHandle(0), logEventProc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);

    ASSERT(m_scrollingStartEventHook);
}

static string stringEvent(DWORD event)
{
    switch(event) {
        case EVENT_OBJECT_VALUECHANGE:
            return "value change event";
        default:
            return "unknown event";
    }
}

static void CALLBACK notificationListenerProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    // Get the accessible object for this event.
    COMPtr<IAccessible> parentObject;

    VARIANT vChild;
    VariantInit(&vChild);

    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &parentObject, &vChild);
    if (FAILED(hr) || !parentObject)
        return;

    COMPtr<IDispatch> childDispatch;
    if (FAILED(parentObject->get_accChild(vChild, &childDispatch))) {
        VariantClear(&vChild);
        return;
    }

    COMPtr<IAccessible> childAccessible(Query, childDispatch);

    sharedFrameLoadDelegate->accessibilityController()->notificationReceived(childAccessible, stringEvent(event));

    VariantClear(&vChild);
}

static COMPtr<IAccessibleComparable> comparableObject(const COMPtr<IServiceProvider>& serviceProvider)
{
    COMPtr<IAccessibleComparable> comparable;
    serviceProvider->QueryService(SID_AccessibleComparable, __uuidof(IAccessibleComparable), reinterpret_cast<void**>(&comparable));
    return comparable;
}

void AccessibilityController::notificationReceived(PlatformUIElement element, const string& eventName)
{
    for (HashMap<PlatformUIElement, JSObjectRef>::iterator it = m_notificationListeners.begin(); it != m_notificationListeners.end(); ++it) {
        COMPtr<IServiceProvider> thisServiceProvider(Query, it->first);
        if (!thisServiceProvider)
            continue;

        COMPtr<IAccessibleComparable> thisComparable = comparableObject(thisServiceProvider);
        if (!thisComparable)
            continue;

        COMPtr<IServiceProvider> elementServiceProvider(Query, element);
        if (!elementServiceProvider)
            continue;

        COMPtr<IAccessibleComparable> elementComparable = comparableObject(elementServiceProvider);
        if (!elementComparable)
            continue;

        BOOL isSame = FALSE;
        thisComparable->isSameObject(elementComparable.get(), &isSame);
        if (!isSame)
            continue;

        JSRetainPtr<JSStringRef> jsNotification(Adopt, JSStringCreateWithUTF8CString(eventName.c_str()));
        JSValueRef argument = JSValueMakeString(frame->globalContext(), jsNotification.get());
        JSObjectCallAsFunction(frame->globalContext(), it->second, NULL, 1, &argument, NULL);
    }
}

void AccessibilityController::addNotificationListener(PlatformUIElement element, JSObjectRef functionCallback)
{
    if (!m_allEventsHook)
        m_allEventsHook = SetWinEventHook(EVENT_MIN, EVENT_MAX, GetModuleHandle(0), notificationListenerProc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);

    JSValueProtect(frame->globalContext(), functionCallback);
    m_notificationListeners.add(element, functionCallback);
}
