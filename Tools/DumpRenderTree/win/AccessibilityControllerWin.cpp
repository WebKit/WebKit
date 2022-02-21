/*
 * Copyright (C) 2008, 2009, 2010, 2013 Apple Inc. All Rights Reserved.
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
#include "TestRunner.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <WebCore/AccessibilityObjectWrapperWin.h>
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKit.h>
#include <comutil.h>
#include <oleacc.h>
#include <string>
#include <wtf/Assertions.h>
#include <wtf/text/AtomString.h>

AccessibilityController::AccessibilityController() = default;

AccessibilityController::~AccessibilityController()
{
    setLogFocusEvents(false);
    setLogAccessibilityEvents(false);
    setLogValueChangeEvents(false);

    if (m_notificationsEventHook)
        UnhookWinEvent(m_notificationsEventHook);

    for (auto& listener : m_notificationListeners.values())
        JSValueUnprotect(frame->globalContext(), listener);
}

AccessibilityUIElement AccessibilityController::elementAtPoint(int x, int y)
{
    // FIXME: implement
    return { nullptr };
}

static COMPtr<IAccessibleComparable> comparableObject(const COMPtr<IServiceProvider>& serviceProvider)
{
    COMPtr<IAccessibleComparable> comparable;
    serviceProvider->QueryService(SID_AccessibleComparable, __uuidof(IAccessibleComparable), reinterpret_cast<void**>(&comparable));
    return comparable;
}

static COMPtr<IAccessible> findAccessibleObjectById(AccessibilityUIElement parentObject, BSTR idAttribute)
{
    COMPtr<IAccessible> parentIAccessible = parentObject.platformUIElement();

    COMPtr<IServiceProvider> serviceProvider(Query, parentIAccessible);
    if (!serviceProvider)
        return 0;

    COMPtr<IAccessibleComparable> comparable = comparableObject(serviceProvider);
    if (!comparable)
        return 0;

    _variant_t value;
    _bstr_t elementIdAttributeKey(L"AXDOMIdentifier");
    if (SUCCEEDED(comparable->get_attribute(elementIdAttributeKey, &value.GetVARIANT()))) {
        ASSERT(V_VT(&value) == VT_BSTR);
        if (VARCMP_EQ == ::VarBstrCmp(value.bstrVal, idAttribute, LOCALE_USER_DEFAULT, 0))
            return parentIAccessible;
    }

    long childCount = parentObject.childrenCount();
    if (!childCount)
        return nullptr;

    COMPtr<IAccessible> result;
    for (long i = 0; i < childCount; ++i) {
        AccessibilityUIElement childAtIndex = parentObject.getChildAtIndex(i);

        result = findAccessibleObjectById(childAtIndex, idAttribute);
        if (result)
            return result;
    }

    return nullptr;
}

AccessibilityUIElement AccessibilityController::accessibleElementById(JSStringRef id)
{
    AccessibilityUIElement rootAccessibilityUIElement = rootElement();

    _bstr_t idAttribute(JSStringCopyBSTR(id), false);

    COMPtr<IAccessible> result = findAccessibleObjectById(rootAccessibilityUIElement, idAttribute);
    if (result)
        return AccessibilityUIElement(result);

    return { nullptr };
}

AccessibilityUIElement AccessibilityController::focusedElement()
{
    COMPtr<IAccessible> rootAccessible = rootElement().platformUIElement();

    _variant_t vFocus;
    if (FAILED(rootAccessible->get_accFocus(&vFocus.GetVARIANT())))
        return { nullptr };

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
        return { nullptr };

    COMPtr<IWebViewPrivate2> viewPrivate(Query, view);
    if (!viewPrivate)
        return { nullptr };

    HWND webViewWindow;
    if (FAILED(viewPrivate->viewWindow(&webViewWindow)))
        return { nullptr };

    // Make sure the layout is up to date, so we can find all accessible elements.
    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (framePrivate)
        framePrivate->layout();

    // Get the root accessible object by querying for the accessible object for the
    // WebView's window.
    COMPtr<IAccessible> rootAccessible;
    if (FAILED(AccessibleObjectFromWindow(webViewWindow, static_cast<DWORD>(OBJID_CLIENT), __uuidof(IAccessible), reinterpret_cast<void**>(&rootAccessible))))
        return { nullptr };

    return rootAccessible;
}

static void CALLBACK logEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    // Get the accessible object for this event.
    COMPtr<IAccessible> parentObject;

    _variant_t vChild;

    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &parentObject, &vChild.GetVARIANT());
    ASSERT(SUCCEEDED(hr));

    // Get the name of the focused element, and log it to stdout.
    _bstr_t nameBSTR;
    hr = parentObject->get_accName(vChild, &nameBSTR.GetBSTR());
    ASSERT(SUCCEEDED(hr));
    std::wstring name(nameBSTR, nameBSTR.length());

    switch (event) {
        case EVENT_OBJECT_FOCUS:
            fprintf(testResult, "Received focus event for object '%S'.\n", name.c_str());
            break;

        case EVENT_OBJECT_SELECTION:
            fprintf(testResult, "Received selection event for object '%S'.\n", name.c_str());
            break;

        case EVENT_OBJECT_VALUECHANGE: {
            _bstr_t valueBSTR;
            hr = parentObject->get_accValue(vChild, &valueBSTR.GetBSTR());
            ASSERT(SUCCEEDED(hr));
            std::wstring value(valueBSTR, valueBSTR.length());

            fprintf(testResult, "Received value change event for object '%S', value '%S'.\n", name.c_str(), value.c_str());
            break;
        }

        case EVENT_SYSTEM_SCROLLINGSTART:
            fprintf(testResult, "Received scrolling start event for object '%S'.\n", name.c_str());
            break;

        default:
            fprintf(testResult, "Received unknown event for object '%S'.\n", name.c_str());
            break;
    }
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

void AccessibilityController::platformResetToConsistentState()
{
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

void AccessibilityController::setLogAccessibilityEvents(bool logAccessibilityEvents)
{
    if (!!m_allEventsHook == logAccessibilityEvents)
        return;

    if (!logAccessibilityEvents) {
        UnhookWinEvent(m_allEventsHook);
        m_allEventsHook = 0;
        return;
    }

    // Ensure that accessibility is initialized for the WebView by querying for
    // the root accessible object.
    rootElement();

    m_allEventsHook = SetWinEventHook(EVENT_MIN, EVENT_MAX, GetModuleHandle(0), logEventProc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);

    ASSERT(m_allEventsHook);
}

static std::string stringEvent(DWORD event)
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

    _variant_t vChild;
    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &parentObject, &vChild.GetVARIANT());
    if (FAILED(hr) || !parentObject)
        return;

    COMPtr<IDispatch> childDispatch;
    if (FAILED(parentObject->get_accChild(vChild, &childDispatch)))
        return;

    COMPtr<IAccessible> childAccessible(Query, childDispatch);
    sharedFrameLoadDelegate->accessibilityController()->winNotificationReceived(childAccessible, stringEvent(event));
}

bool AccessibilityController::addNotificationListener(JSObjectRef functionCallback)
{
    return false;
}

void AccessibilityController::removeNotificationListener()
{
}

void AccessibilityController::winNotificationReceived(PlatformUIElement element, const std::string& eventName)
{
    for (auto& slot : m_notificationListeners) {
        COMPtr<IServiceProvider> thisServiceProvider(Query, slot.key);
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

        auto jsNotification = adopt(JSStringCreateWithUTF8CString(eventName.c_str()));
        JSValueRef argument = JSValueMakeString(frame->globalContext(), jsNotification.get());
        JSObjectCallAsFunction(frame->globalContext(), slot.value, 0, 1, &argument, 0);
    }
}

void AccessibilityController::winAddNotificationListener(PlatformUIElement element, JSObjectRef functionCallback)
{
    if (!m_notificationsEventHook)
        m_notificationsEventHook = SetWinEventHook(EVENT_MIN, EVENT_MAX, GetModuleHandle(0), notificationListenerProc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);

    JSValueProtect(frame->globalContext(), functionCallback);
    m_notificationListeners.add(element, functionCallback);
}

void AccessibilityController::enableEnhancedAccessibility(bool)
{
    // FIXME: implement
}

bool AccessibilityController::enhancedAccessibilityEnabled()
{
    // FIXME: implement
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityController::platformName() const
{
    return adopt(JSStringCreateWithUTF8CString("win"));
}
