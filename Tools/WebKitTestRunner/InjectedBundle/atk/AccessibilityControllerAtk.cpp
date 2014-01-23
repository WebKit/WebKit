/*
 * Copyright (C) 2012 Igalia S.L.
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

#if HAVE(ACCESSIBILITY)

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"

#include <WebKit2/WKBundlePagePrivate.h>
#include <atk/atk.h>
#include <cstdio>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/StringBuilder.h>

namespace WTR {

void AccessibilityController::logAccessibilityEvents()
{
    // Ensure no callbacks are connected before.
    resetToConsistentState();

    // Ensure that accessibility is initialized for the WebView by querying for
    // the root accessible object, which will create the full hierarchy.
    rootElement();

    if (!m_globalNotificationHandler)
        m_globalNotificationHandler = AccessibilityNotificationHandler::create();
    m_globalNotificationHandler->logAccessibilityEvents();

    // Ensure the Atk interface types are registered, otherwise
    // the AtkDocument signal handlers below won't get registered.
    GObject* dummyAxObject = G_OBJECT(g_object_new(ATK_TYPE_OBJECT, nullptr));
    AtkObject* dummyNoOpAxObject = atk_no_op_object_new(dummyAxObject);
    g_object_unref(G_OBJECT(dummyNoOpAxObject));
    g_object_unref(dummyAxObject);
}

void AccessibilityController::resetToConsistentState()
{
    m_globalNotificationHandler = nullptr;
}

static AtkObject* childElementById(AtkObject* parent, const char* id)
{
    if (!ATK_IS_OBJECT(parent))
        return nullptr;

    bool parentFound = false;
    AtkAttributeSet* attributeSet = atk_object_get_attributes(parent);
    for (AtkAttributeSet* attributes = attributeSet; attributes; attributes = attributes->next) {
        AtkAttribute* attribute = static_cast<AtkAttribute*>(attributes->data);
        if (!strcmp(attribute->name, "html-id")) {
            if (!strcmp(attribute->value, id))
                parentFound = true;
            break;
        }
    }
    atk_attribute_set_free(attributeSet);

    if (parentFound)
        return parent;

    int childCount = atk_object_get_n_accessible_children(parent);
    for (int i = 0; i < childCount; i++) {
        AtkObject* result = childElementById(atk_object_ref_accessible_child(parent, i), id);
        if (ATK_IS_OBJECT(result))
            return result;
    }

    return nullptr;
}

PassRefPtr<AccessibilityUIElement> AccessibilityController::accessibleElementById(JSStringRef id)
{
    AtkObject* root = ATK_OBJECT(WKAccessibilityRootObject(InjectedBundle::shared().page()->page()));
    if (!root)
        return nullptr;

    size_t bufferSize = JSStringGetMaximumUTF8CStringSize(id);
    GUniquePtr<gchar> idBuffer(static_cast<gchar*>(g_malloc(bufferSize)));
    JSStringGetUTF8CString(id, idBuffer.get(), bufferSize);

    AtkObject* result = childElementById(root, idBuffer.get());
    if (ATK_IS_OBJECT(result))
        return AccessibilityUIElement::create(result);

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityController::platformName()
{
    JSRetainPtr<JSStringRef> platformName(Adopt, JSStringCreateWithUTF8CString("atk"));
    return platformName;
}

PassRefPtr<AccessibilityUIElement> AccessibilityController::rootElement()
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    void* root = WKAccessibilityRootObject(page);

    return AccessibilityUIElement::create(static_cast<AtkObject*>(root));
}

PassRefPtr<AccessibilityUIElement> AccessibilityController::focusedElement()
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    void* root = WKAccessibilityFocusedObject(page);

    return AccessibilityUIElement::create(static_cast<AtkObject*>(root));
}

bool AccessibilityController::addNotificationListener(JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    // Only one global notification listener.
    if (!m_globalNotificationHandler)
        m_globalNotificationHandler = AccessibilityNotificationHandler::create();
    m_globalNotificationHandler->setNotificationFunctionCallback(functionCallback);

    return true;
}

bool AccessibilityController::removeNotificationListener()
{
    // Programmers should not be trying to remove a listener that's already removed.
    ASSERT(m_globalNotificationHandler);

    m_globalNotificationHandler = nullptr;
    return false;
}

} // namespace WTR

#endif // HAVE(ACCESSIBILITY)
