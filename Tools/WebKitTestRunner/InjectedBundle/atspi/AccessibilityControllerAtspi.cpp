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
#include "AccessibilityController.h"

#if USE(ATSPI)
#include "AccessibilityNotificationHandler.h"
#include "AccessibilityUIElement.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "StringFunctions.h"
#include <WebCore/AccessibilityObjectAtspi.h>
#include <WebCore/AccessibilityRootAtspi.h>
#include <WebKit/WKBundleFramePrivate.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace WTR {

void AccessibilityController::resetToConsistentState()
{
    if (m_globalNotificationHandler)
        removeNotificationListener();
}

static WebCore::AccessibilityObjectAtspi* findAccessibleObjectById(WebCore::AccessibilityObjectAtspi& axObject, const String& elementID)
{
    axObject.updateBackingStore();
    if (axObject.id() == elementID)
        return &axObject;

    for (const auto& child : axObject.children()) {
        if (auto* element = findAccessibleObjectById(*child, elementID))
            return element;
    }

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityController::accessibleElementById(JSContextRef context, JSStringRef id)
{
    auto* rootObject = static_cast<WebCore::AccessibilityObjectAtspi*>(WKAccessibilityRootObject(WKBundleFrameForJavaScriptContext(context)));
    if (!rootObject)
        return nullptr;

    String elementID = toWTFString(id);
    if (auto* element = findAccessibleObjectById(*rootObject, elementID))
        return AccessibilityUIElement::create(element);
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityController::platformName()
{
    JSRetainPtr<JSStringRef> platformName(Adopt, JSStringCreateWithUTF8CString("atspi"));
    return platformName;
}

void AccessibilityController::injectAccessibilityPreference(JSStringRef domain, JSStringRef key, JSStringRef value)
{
}

Ref<AccessibilityUIElement> AccessibilityController::rootElement(JSContextRef context)
{
    auto* element = static_cast<WebCore::AccessibilityObjectAtspi*>(WKAccessibilityRootObject(WKBundleFrameForJavaScriptContext(context)));
    return AccessibilityUIElement::create(element);
}

RefPtr<AccessibilityUIElement> AccessibilityController::focusedElement(JSContextRef context)
{
    if (!WKAccessibilityRootObject(WKBundleFrameForJavaScriptContext(context)))
        return nullptr;

    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    if (auto* element = static_cast<WebCore::AccessibilityObjectAtspi*>(WKAccessibilityFocusedObject(page)))
        return AccessibilityUIElement::create(element);
    return nullptr;
}

bool AccessibilityController::addNotificationListener(JSContextRef, JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    if (m_globalNotificationHandler)
        return false;

    m_globalNotificationHandler = makeUnique<AccessibilityNotificationHandler>(functionCallback);
    return true;
}

bool AccessibilityController::removeNotificationListener()
{
    ASSERT(m_globalNotificationHandler);
    m_globalNotificationHandler = nullptr;
    return true;
}

void AccessibilityController::overrideClient(JSStringRef)
{
}

} // namespace WTR

#endif // USE(ATSPI)
