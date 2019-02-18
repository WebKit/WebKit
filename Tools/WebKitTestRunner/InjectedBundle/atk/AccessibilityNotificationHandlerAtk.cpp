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
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTR {

namespace {

typedef HashMap<AtkObject*, AccessibilityNotificationHandler*> NotificationHandlersMap;

static WTF::Vector<unsigned>& listenerIds()
{
    static NeverDestroyed<WTF::Vector<unsigned>> ids;
    return ids.get();
}

static NotificationHandlersMap& notificationHandlers()
{
    static NeverDestroyed<NotificationHandlersMap> map;
    return map.get();
}

AccessibilityNotificationHandler* globalNotificationHandler = nullptr;

gboolean axObjectEventListener(GSignalInvocationHint* signalHint, unsigned numParamValues, const GValue* paramValues, gpointer data)
{
    // At least we should receive the instance emitting the signal.
    if (!numParamValues)
        return true;

    AtkObject* accessible = ATK_OBJECT(g_value_get_object(&paramValues[0]));
    if (!accessible || !ATK_IS_OBJECT(accessible))
        return true;

#if PLATFORM(GTK)
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    JSContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);
#else
    JSContextRef jsContext = nullptr;
#endif

    GSignalQuery signalQuery;
    const char* notificationName = nullptr;
    Vector<JSValueRef> extraArgs;

    g_signal_query(signalHint->signal_id, &signalQuery);

    if (!g_strcmp0(signalQuery.signal_name, "state-change")) {
        if (!g_strcmp0(g_value_get_string(&paramValues[1]), "checked"))
            notificationName = "CheckedStateChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "invalid-entry"))
            notificationName = "AXInvalidStatusChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "active"))
            notificationName = "ActiveStateChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "busy"))
            notificationName = "AXElementBusyChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "enabled"))
            notificationName = "AXDisabledStateChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "expanded"))
            notificationName = "AXExpandedChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "pressed"))
            notificationName = "AXPressedStateChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "read-only"))
            notificationName = "AXReadOnlyStatusChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "required"))
            notificationName = "AXRequiredStatusChanged";
        else if (!g_strcmp0(g_value_get_string(&paramValues[1]), "sensitive"))
            notificationName = "AXSensitiveStateChanged";
        else
            return true;
        GUniquePtr<char> signalValue(g_strdup_printf("%d", g_value_get_boolean(&paramValues[2])));
        JSRetainPtr<JSStringRef> jsSignalValue(Adopt, JSStringCreateWithUTF8CString(signalValue.get()));
        extraArgs.append(JSValueMakeString(jsContext, jsSignalValue.get()));
    } else if (!g_strcmp0(signalQuery.signal_name, "focus-event")) {
        if (g_value_get_boolean(&paramValues[1]))
            notificationName = "AXFocusedUIElementChanged";
    } else if (!g_strcmp0(signalQuery.signal_name, "selection-changed")) {
        notificationName = "AXSelectedChildrenChanged";
    } else if (!g_strcmp0(signalQuery.signal_name, "children-changed")) {
        const gchar* childrenChangedDetail = g_quark_to_string(signalHint->detail);
        notificationName = !g_strcmp0(childrenChangedDetail, "add") ? "AXChildrenAdded" : "AXChildrenRemoved";
        gpointer child = g_value_get_pointer(&paramValues[2]);
        if (ATK_IS_OBJECT(child))
            extraArgs.append(toJS(jsContext, WTF::getPtr(WTR::AccessibilityUIElement::create(ATK_OBJECT(child)))));
    } else if (!g_strcmp0(signalQuery.signal_name, "property-change")) {
        if (!g_strcmp0(g_quark_to_string(signalHint->detail), "accessible-value"))
            notificationName = "AXValueChanged";
    } else if (!g_strcmp0(signalQuery.signal_name, "load-complete"))
        notificationName = "AXLoadComplete";
    else if (!g_strcmp0(signalQuery.signal_name, "text-caret-moved")) {
        notificationName = "AXTextCaretMoved";
        GUniquePtr<char> signalValue(g_strdup_printf("%d", g_value_get_int(&paramValues[1])));
        JSRetainPtr<JSStringRef> jsSignalValue(Adopt, JSStringCreateWithUTF8CString(signalValue.get()));
        extraArgs.append(JSValueMakeString(jsContext, jsSignalValue.get()));
    } else if (!g_strcmp0(signalQuery.signal_name, "text-insert") || !g_strcmp0(signalQuery.signal_name, "text-remove"))
        notificationName = "AXTextChanged";

    if (!jsContext)
        return true;

    if (notificationName) {
        JSRetainPtr<JSStringRef> jsNotificationEventName(Adopt, JSStringCreateWithUTF8CString(notificationName));
        JSValueRef notificationNameArgument = JSValueMakeString(jsContext, jsNotificationEventName.get());
        NotificationHandlersMap::iterator elementNotificationHandler = notificationHandlers().find(accessible);
        JSValueRef arguments[5]; // this dimension must be >= 2 + max(extraArgs.size())
        arguments[0] = toJS(jsContext, WTF::getPtr(WTR::AccessibilityUIElement::create(accessible)));
        arguments[1] = notificationNameArgument;
        size_t numOfExtraArgs = extraArgs.size();
        for (size_t i = 0; i < numOfExtraArgs; i++)
            arguments[i + 2] = extraArgs[i];
        if (elementNotificationHandler != notificationHandlers().end()) {
            // Listener for one element. As arguments, it gets the notification name
            // and sometimes extra arguments.
            JSObjectCallAsFunction(jsContext,
                const_cast<JSObjectRef>(elementNotificationHandler->value->notificationFunctionCallback()),
                0, numOfExtraArgs + 1, arguments + 1, 0);
        }

        if (globalNotificationHandler) {
            // A global listener gets additionally the element as the first argument.
            JSObjectCallAsFunction(jsContext,
                const_cast<JSObjectRef>(globalNotificationHandler->notificationFunctionCallback()),
                0, numOfExtraArgs + 2, arguments, 0);
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

void AccessibilityNotificationHandler::setNotificationFunctionCallback(JSValueRef notificationFunctionCallback)
{
    if (!notificationFunctionCallback) {
        removeAccessibilityNotificationHandler();
        disconnectAccessibilityCallbacks();
        return;
    }

    m_notificationFunctionCallback = notificationFunctionCallback;

#if PLATFORM(GTK)
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
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
        NotificationHandlersMap::iterator currentNotificationHandler = notificationHandlers().find(m_platformElement.get());
        if (currentNotificationHandler != notificationHandlers().end()) {
            ASSERT(currentNotificationHandler->value->platformElement());
            JSValueUnprotect(jsContext, currentNotificationHandler->value->notificationFunctionCallback());
            notificationHandlers().remove(currentNotificationHandler->value->platformElement().get());
        }
        notificationHandlers().add(m_platformElement.get(), this);
    } else {
        if (globalNotificationHandler)
            JSValueUnprotect(jsContext, globalNotificationHandler->notificationFunctionCallback());
        globalNotificationHandler = this;
    }
}

void AccessibilityNotificationHandler::removeAccessibilityNotificationHandler()
{
#if PLATFORM(GTK)
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
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
        NotificationHandlersMap::iterator removeNotificationHandler = notificationHandlers().find(m_platformElement.get());
        if (removeNotificationHandler != notificationHandlers().end()) {
            JSValueUnprotect(jsContext, removeNotificationHandler->value->notificationFunctionCallback());
            notificationHandlers().remove(removeNotificationHandler);
        }
    }
}

void AccessibilityNotificationHandler::connectAccessibilityCallbacks()
{
    // Ensure no callbacks are connected before.
    if (!disconnectAccessibilityCallbacks())
        return;

    const char* signalNames[] = {
        "ATK:AtkObject:state-change",
        "ATK:AtkObject:focus-event",
        "ATK:AtkObject:active-descendant-changed",
        "ATK:AtkObject:children-changed",
        "ATK:AtkObject:property-change",
        "ATK:AtkObject:visible-data-changed",
        "ATK:AtkDocument:load-complete",
        "ATK:AtkSelection:selection-changed",
        "ATK:AtkText:text-caret-moved",
        "ATK:AtkText:text-insert",
        "ATK:AtkText:text-remove",
        0
    };

    // Register atk interfaces, otherwise add_global may fail.
    GObject* dummyObject = (GObject*)g_object_new(G_TYPE_OBJECT, NULL, NULL);
    g_object_unref(atk_no_op_object_new(dummyObject));
    g_object_unref(dummyObject);

    // Add global listeners for AtkObject's signals.
    for (const char** signalName = signalNames; *signalName; signalName++) {
        unsigned id = atk_add_global_event_listener(axObjectEventListener, *signalName);
        if (!id) {
            String message = makeString("atk_add_global_event_listener failed for signal ", *signalName, '\n');
            InjectedBundle::singleton().outputText(message);
            continue;
        }

        listenerIds().append(id);
    }
}

bool AccessibilityNotificationHandler::disconnectAccessibilityCallbacks()
{
    // Only disconnect if there is no notification handler.
    if (!notificationHandlers().isEmpty() || globalNotificationHandler)
        return false;

    // AtkObject signals.
    for (size_t i = 0; i < listenerIds().size(); i++) {
        ASSERT(listenerIds()[i]);
        atk_remove_global_event_listener(listenerIds()[i]);
    }
    listenerIds().clear();
    return true;
}

} // namespace WTR

#endif // HAVE(ACCESSIBILITY)
