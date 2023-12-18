/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "AccessibilityNotificationHandler.h"

#if USE(ATSPI)
#include "InjectedBundlePage.h"
#include "JSWrapper.h"
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/AccessibilityObjectAtspi.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace WTR {

AccessibilityNotificationHandler::AccessibilityNotificationHandler(JSValueRef callback, PlatformUIElement element)
    : m_callback(callback)
    , m_element(element)
{
    WKAccessibilityEnable();
#if ENABLE(DEVELOPER_MODE)
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSValueProtect(jsContext, m_callback);

    WebCore::AccessibilityAtspi::singleton().addNotificationObserver(this, [this](WebCore::AccessibilityObjectAtspi& element, const char* notificationName, WebCore::AccessibilityAtspi::NotificationObserverParameter parameter) {
        if (m_element && m_element.get() != &element)
            return;

        WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
        JSContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);

        JSRetainPtr<JSStringRef> jsNotificationEventName(Adopt, JSStringCreateWithUTF8CString(notificationName));
        JSValueRef jsParameter = WTF::switchOn(parameter,
            [&](String& stringValue) -> JSValueRef {
                JSRetainPtr<JSStringRef> jsStringValue(Adopt, OpaqueJSString::tryCreate(stringValue).get());
                return JSValueMakeString(jsContext, jsStringValue.get());
            },
            [&](bool& boolValue) -> JSValueRef {
                return JSValueMakeBoolean(jsContext, boolValue);
            },
            [&](unsigned& unsignedValue) -> JSValueRef {
                return JSValueMakeNumber(jsContext, unsignedValue);
            },
            [&](Ref<WebCore::AccessibilityObjectAtspi>& elementValue) -> JSValueRef {
                return toJS(jsContext, AccessibilityUIElement::create(elementValue.ptr()).ptr());
            },
            [&](auto&) -> JSValueRef {
                return JSValueMakeUndefined(jsContext);
            });

        if (m_element) {
            // Listener for one element gets the notification name parameter.
            JSValueRef arguments[2];
            arguments[0] = JSValueMakeString(jsContext, jsNotificationEventName.get());
            arguments[1] = jsParameter;
            JSObjectCallAsFunction(jsContext, const_cast<JSObjectRef>(m_callback), 0, 2, arguments, 0);
        } else {
            // A global listener gets the element, notification name and parameter.
            JSValueRef arguments[3];
            arguments[0] = toJS(jsContext, AccessibilityUIElement::create(&element).ptr());
            arguments[1] = JSValueMakeString(jsContext, jsNotificationEventName.get());
            arguments[2] = jsParameter;
            JSObjectCallAsFunction(jsContext, const_cast<JSObjectRef>(m_callback), 0, 3, arguments, 0);
        }
    });
#endif
}

AccessibilityNotificationHandler::~AccessibilityNotificationHandler()
{
#if ENABLE(DEVELOPER_MODE)
    WebCore::AccessibilityAtspi::singleton().removeNotificationObserver(this);

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSValueUnprotect(jsContext, m_callback);
#endif
}

} // namespace WTR

#endif // USE(ATSPI)
