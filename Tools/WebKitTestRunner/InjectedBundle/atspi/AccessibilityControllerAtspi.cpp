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

#if HAVE(ACCESSIBILITY) && USE(ATSPI)
#include "AccessibilityUIElement.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "StringFunctions.h"
#include <WebCore/AccessibilityObjectAtspi.h>
#include <WebCore/AccessibilityRootAtspi.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace WTR {

void AccessibilityController::resetToConsistentState()
{
}

static WebCore::AccessibilityObjectAtspi* findAccessibleObjectById(WebCore::AccessibilityObjectAtspi& axObject, const String& elementID)
{
    axObject.updateBackingStore();
    if (axObject.id() == elementID)
        return &axObject;

    Vector<RefPtr<WebCore::AccessibilityObjectAtspi>> children;
    InjectedBundle::singleton().accessibilityController()->executeOnAXThreadAndWait([axObject = Ref { axObject }, &children] {
        axObject->updateBackingStore();
        children = axObject->children();
    });
    for (const auto& child : children) {
        if (auto* element = findAccessibleObjectById(*child, elementID))
            return element;
    }

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityController::accessibleElementById(JSStringRef id)
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    auto* rootObject = static_cast<WebCore::AccessibilityObjectAtspi*>(WKAccessibilityRootObject(page));
    if (!rootObject)
        return nullptr;

    String elementID = toWTFString(id);
    if (auto* element = findAccessibleObjectById(*rootObject, elementID))
        return AccessibilityUIElement::create(element);
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityController::platformName()
{
    // FIXME: Use atk as platform name for now, because the expected behavior is the same.
    // Once we replace the atk implementation with the atspi one we can use atspi and
    // update the tests helper scripts. https://bugs.webkit.org/show_bug.cgi?id=232227.
    JSRetainPtr<JSStringRef> platformName(Adopt, JSStringCreateWithUTF8CString("atk"));
    return platformName;
}

void AccessibilityController::injectAccessibilityPreference(JSStringRef domain, JSStringRef key, JSStringRef value)
{
}

Ref<AccessibilityUIElement> AccessibilityController::rootElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    auto* element = static_cast<WebCore::AccessibilityObjectAtspi*>(WKAccessibilityRootObject(page));
    return AccessibilityUIElement::create(element);
}

RefPtr<AccessibilityUIElement> AccessibilityController::focusedElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    if (auto* element = static_cast<WebCore::AccessibilityObjectAtspi*>(WKAccessibilityFocusedObject(page)))
        return AccessibilityUIElement::create(element);
    return nullptr;
}

bool AccessibilityController::addNotificationListener(JSValueRef functionCallback)
{
    return true;
}

bool AccessibilityController::removeNotificationListener()
{
    return false;
}

void AccessibilityController::updateIsolatedTreeMode()
{
}

RunLoop& AccessibilityController::axRunLoop()
{
    if (!m_axRunLoop) {
        WKBundlePageRef page = InjectedBundle::singleton().page()->page();
        auto* element = static_cast<WebCore::AccessibilityObjectAtspi*>(WKAccessibilityRootObject(page));
        RELEASE_ASSERT(element);
        m_axRunLoop = &element->root().atspi().runLoop();
    }

    return *m_axRunLoop;
}

void AccessibilityController::executeOnAXThreadAndWait(Function<void()>&& function)
{
    RELEASE_ASSERT(isMainThread());
    std::atomic<bool> done = false;
    axRunLoop().dispatch([this, function = WTFMove(function), &done] {
        function();
        done.store(true);
    });
    while (!done.load())
        g_main_context_iteration(nullptr, FALSE);
}

void AccessibilityController::executeOnAXThread(Function<void()>&& function)
{
    axRunLoop().dispatch([this, function = WTFMove(function)] {
        function();
    });
}

void AccessibilityController::executeOnMainThread(Function<void()>&& function)
{
    if (isMainThread()) {
        function();
        return;
    }

    axRunLoop().dispatch([this, function = WTFMove(function)]() mutable {
        callOnMainThread(WTFMove(function));
    });
}

} // namespace WTR

#endif // HAVE(ACCESSIBILITY) && USE(ATSPI)
