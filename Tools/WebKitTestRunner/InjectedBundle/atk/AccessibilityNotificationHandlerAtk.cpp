/*
 * Copyright (C) 2013 Samsung Electronics Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "AccessibilityNotificationHandlerAtk.h"

#if HAVE(ACCESSIBILITY)

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSWrapper.h"
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <wtf/HashMap.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTR {

namespace {

typedef HashMap<AtkObject*, AccessibilityNotificationHandler*> NotificationHandlersMap;

unsigned stateChangeListenerId = 0;
unsigned focusEventListenerId = 0;
unsigned activeDescendantChangedListenerId = 0;
unsigned childrenChangedListenerId = 0;
unsigned propertyChangedListenerId = 0;
unsigned visibleDataChangedListenerId = 0;
unsigned loadCompleteListenerId = 0;
NotificationHandlersMap notificationHandlers;
AccessibilityNotificationHandler* globalNotificationHandler = nullptr;
bool loggingAccessibilityEvents = false;

void printAccessibilityEvent(AtkObject* accessible, const char* signalName, const char* signalValue)
{
    // Do not handle state-change:defunct signals, as the AtkObject
    // associated to them will not be valid at this point already.
    if (!signalName || !g_strcmp0(signalName, "state-change:defunct"))
        return;

    if (!accessible || !ATK_IS_OBJECT(accessible))
        return;

    const char* objectName = atk_object_get_name(accessible);
    AtkRole objectRole = atk_object_get_role(accessible);

    // Try to always provide a name to be logged for the object.
    if (!objectName || *objectName == '\0')
        objectName = "(No name)";

    GUniquePtr<char> signalNameAndValue(signalValue ? g_strdup_printf("%s = %s", signalName, signalValue) : g_strdup(signalName));
    GUniquePtr<char> accessibilityEventString(g_strdup_printf("Accessibility object emitted \"%s\" / Name: \"%s\" / Role: %d\n", signalNameAndValue.get(), objectName, objectRole));
    InjectedBundle::shared().outputText(String::fromUTF8(accessibilityEventString.get()));
}

gboolean axObjectEventListener(GSignalInvocationHint* signalHint, unsigned numParamValues, const GValue* paramValues, gpointer data)
{
    // At least we should receive the instance emitting the signal.
    if (!numParamValues)
        return true;

    AtkObject* accessible = ATK_OBJECT(g_value_get_object(&paramValues[0]));
    if (!accessible || !ATK_IS_OBJECT(accessible))
        return true;

    GSignalQuery signalQuery;
    GUniquePtr<char> signalName;
    GUniquePtr<char> signalValue;
    const char* notificationName = nullptr;

    g_signal_query(signalHint->signal_id, &signalQuery);

    if (!g_strcmp0(signalQuery.signal_name, "state-change")) {
        signalName.reset(g_strdup_printf("state-change:%s", g_value_get_string(&paramValues[1])));
        signalValue.reset(g_strdup_printf("%d", g_value_get_boolean(&paramValues[2])));
        if (!g_strcmp0(g_value_get_string(&paramValues[1]), "checked"))
            notificationName = "CheckedStateChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "invalid-entry"))
            notificationName = "AXInvalidStatusChanged";
    } else if (!g_strcmp0(signalQuery.signal_name, "focus-event")) {
        signalName.reset(g_strdup("focus-event"));
        signalValue.reset(g_strdup_printf("%d", g_value_get_boolean(&paramValues[1])));
        if (g_value_get_boolean(&paramValues[1]))
            notificationName = "AXFocusedUIElementChanged";
    } else if (!g_strcmp0(signalQuery.signal_name, "children-changed")) {
        const gchar* childrenChangedDetail = g_quark_to_string(signalHint->detail);
        signalName.reset(g_strdup_printf("children-changed:%s", childrenChangedDetail));
        signalValue.reset(g_strdup_printf("%d", g_value_get_uint(&paramValues[1])));
        notificationName = !g_strcmp0(childrenChangedDetail, "add") ? "AXChildrenAdded" : "AXChildrenRemoved";
    } else if (!g_strcmp0(signalQuery.signal_name, "property-change")) {
        signalName.reset(g_strdup_printf("property-change:%s", g_quark_to_string(signalHint->detail)));
        if (!g_strcmp0(g_quark_to_string(signalHint->detail), "accessible-value"))
            notificationName = "AXValueChanged";
    } else if (!g_strcmp0(signalQuery.signal_name, "load-complete"))
        notificationName = "AXLoadComplete";
    else
        signalName.reset(g_strdup(signalQuery.signal_name));

    if (loggingAccessibilityEvents)
        printAccessibilityEvent(accessible, signalName.get(), signalValue.get());

#if PLATFORM(GTK) || PLATFORM(EFL)
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);
#else
    JSContextRef jsContext = nullptr;
#endif
    if (!jsContext)
        return true;

    if (notificationName) {
        JSRetainPtr<JSStringRef> jsNotificationEventName(Adopt, JSStringCreateWithUTF8CString(notificationName));
        JSValueRef notificationNameArgument = JSValueMakeString(jsContext, jsNotificationEventName.get());
        NotificationHandlersMap::iterator elementNotificationHandler = notificationHandlers.find(accessible);
        if (elementNotificationHandler != notificationHandlers.end()) {
            // Listener for one element just gets one argument, the notification name.
            JSObjectCallAsFunction(jsContext, const_cast<JSObjectRef>(elementNotificationHandler->value->notificationFunctionCallback()), 0, 1, &notificationNameArgument, 0);
        }

        if (globalNotificationHandler) {
            // A global listener gets the element and the notification name as arguments.
            JSValueRef arguments[2];
            arguments[0] = toJS(jsContext, WTF::getPtr(WTR::AccessibilityUIElement::create(accessible)));
            arguments[1] = notificationNameArgument;
            JSObjectCallAsFunction(jsContext, const_cast<JSObjectRef>(globalNotificationHandler->notificationFunctionCallback()), 0, 2, arguments, 0);
        }
    }

    return true;
}

} // namespace

AccessibilityNotificationHandler::AccessibilityNotificationHandler()
    : m_platformElement(0)
    , m_notificationFunctionCallback(0)
{
}

AccessibilityNotificationHandler::~AccessibilityNotificationHandler()
{
    removeAccessibilityNotificationHandler();
    disconnectAccessibilityCallbacks();
}

void AccessibilityNotificationHandler::logAccessibilityEvents()
{
    connectAccessibilityCallbacks();
    loggingAccessibilityEvents = true;
}

void AccessibilityNotificationHandler::setNotificationFunctionCallback(JSValueRef notificationFunctionCallback)
{
    if (!notificationFunctionCallback) {
        removeAccessibilityNotificationHandler();
        disconnectAccessibilityCallbacks();
        return;
    }

    m_notificationFunctionCallback = notificationFunctionCallback;

#if PLATFORM(GTK) || PLATFORM(EFL)
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);
#else
    JSContextRef jsContext = nullptr;
#endif
    if (!jsContext)
        return;

    connectAccessibilityCallbacks();

    JSValueProtect(jsContext, m_notificationFunctionCallback);
    // Check if this notification handler is related to a specific element.
    if (m_platformElement) {
        NotificationHandlersMap::iterator currentNotificationHandler = notificationHandlers.find(m_platformElement.get());
        if (currentNotificationHandler != notificationHandlers.end()) {
            ASSERT(currentNotificationHandler->value->platformElement());
            JSValueUnprotect(jsContext, currentNotificationHandler->value->notificationFunctionCallback());
            notificationHandlers.remove(currentNotificationHandler->value->platformElement().get());
        }
        notificationHandlers.add(m_platformElement.get(), this);
    } else {
        if (globalNotificationHandler)
            JSValueUnprotect(jsContext, globalNotificationHandler->notificationFunctionCallback());
        globalNotificationHandler = this;
    }
}

void AccessibilityNotificationHandler::removeAccessibilityNotificationHandler()
{
#if PLATFORM(GTK) || PLATFORM(EFL)
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);
#else
    JSContextRef jsContext = nullptr;
#endif
    if (!jsContext)
        return;

    if (globalNotificationHandler == this) {
        JSValueUnprotect(jsContext, globalNotificationHandler->notificationFunctionCallback());
        globalNotificationHandler = nullptr;
    } else if (m_platformElement.get()) {
        NotificationHandlersMap::iterator removeNotificationHandler = notificationHandlers.find(m_platformElement.get());
        if (removeNotificationHandler != notificationHandlers.end()) {
            JSValueUnprotect(jsContext, removeNotificationHandler->value->notificationFunctionCallback());
            notificationHandlers.remove(removeNotificationHandler);
        }
    }
}

void AccessibilityNotificationHandler::connectAccessibilityCallbacks()
{
    // Ensure no callbacks are connected before.
    if (!disconnectAccessibilityCallbacks())
        return;

    // Add global listeners for AtkObject's signals.
    stateChangeListenerId = atk_add_global_event_listener(axObjectEventListener, "ATK:AtkObject:state-change");
    focusEventListenerId = atk_add_global_event_listener(axObjectEventListener, "ATK:AtkObject:focus-event");
    activeDescendantChangedListenerId = atk_add_global_event_listener(axObjectEventListener, "ATK:AtkObject:active-descendant-changed");
    childrenChangedListenerId = atk_add_global_event_listener(axObjectEventListener, "ATK:AtkObject:children-changed");
    propertyChangedListenerId = atk_add_global_event_listener(axObjectEventListener, "ATK:AtkObject:property-change");
    visibleDataChangedListenerId = atk_add_global_event_listener(axObjectEventListener, "ATK:AtkObject:visible-data-changed");
    loadCompleteListenerId = atk_add_global_event_listener(axObjectEventListener, "ATK:AtkDocument:load-complete");
}

bool AccessibilityNotificationHandler::disconnectAccessibilityCallbacks()
{
    // Only disconnect if there is no notification handler.
    if (!notificationHandlers.isEmpty() || globalNotificationHandler)
        return false;

    // AtkObject signals.
    if (stateChangeListenerId) {
        atk_remove_global_event_listener(stateChangeListenerId);
        stateChangeListenerId = 0;
    }
    if (focusEventListenerId) {
        atk_remove_global_event_listener(focusEventListenerId);
        focusEventListenerId = 0;
    }
    if (activeDescendantChangedListenerId) {
        atk_remove_global_event_listener(activeDescendantChangedListenerId);
        activeDescendantChangedListenerId = 0;
    }
    if (childrenChangedListenerId) {
        atk_remove_global_event_listener(childrenChangedListenerId);
        childrenChangedListenerId = 0;
    }
    if (propertyChangedListenerId) {
        atk_remove_global_event_listener(propertyChangedListenerId);
        propertyChangedListenerId = 0;
    }
    if (visibleDataChangedListenerId) {
        atk_remove_global_event_listener(visibleDataChangedListenerId);
        visibleDataChangedListenerId = 0;
    }
    if (loadCompleteListenerId) {
        atk_remove_global_event_listener(loadCompleteListenerId);
        loadCompleteListenerId = 0;
    }
    return true;
}

} // namespace WTR

#endif // HAVE(ACCESSIBILITY)
